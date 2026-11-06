// Standard library
#include <cerrno>
#include <csignal>
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <thread>

// SDK
#include <fcntl.h>
#include <ps5/klog.h>
#include <sys/signal.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

// Project
#include "console/sce_api.h"
#include "core/fault_frame.h"
#include "core/lifecycle.hpp"
#include "core/settings_store.hpp"
#include "hud/publisher.hpp"
#include "inject/shellui/injector.hpp"
#include "platform/log.h"
#include "platform/notify.h"
#include "platform/ps5_types.h"
#include "platform/ready.hpp"
#include "proc/query.h"
#include "sampling/fps_sampler.hpp"

// sceKernelGetProsperoSystemSwVersion result.
typedef struct
{
    uint64_t pad0;
    char version_str[0x1C];
    uint32_t version;
    uint64_t pad1;
} OrbisKernelSwVersion;

// External C declarations
extern "C"
{
    int sceKernelGetProsperoSystemSwVersion(OrbisKernelSwVersion* sw);
    int sceKernelSendNotificationRequest(int32_t device, OrbisNotificationRequest* req, size_t size, int32_t blocking);
    int sceUserServiceInitialize(const int*);
    extern uint8_t shellui_elf_start[];
}

// Global variables
char _end[1] = {};
static constexpr auto DEFAULT_PRIORITY = 256;

void sig_handler(int signo);

namespace
{

constexpr int kPreviousExitWaitMs = 1500;
constexpr int kHudUnpublishWaitMs = 500;

/** Stop a previous OrangeHUD instance so two daemons never fight over the HUD. */
void stop_previous_instance()
{
    pid_t old_pid = -1;
    if (!orange::kDaemonReady.read(&old_pid) || old_pid <= 1 || old_pid == getpid())
    {
        return;
    }
    char name[32] = {0};
    if (proc_is_alive(old_pid) && sceKernelGetProcessName(old_pid, name) == 0 && strcmp(name, "orange_daemon.elf") == 0)
    {
        LOG_INFO("stopping previous instance pid=%d", static_cast<int>(old_pid));
        /* SIGTERM lets it publish a disabled HUD frame; SIGKILL only if it ignores that. */
        if (kill(old_pid, SIGTERM) != 0 && errno != ESRCH)
        {
            LOG_ERROR("failed to stop pid=%d: %s", static_cast<int>(old_pid), strerror(errno));
        }
        for (int waited_ms = 0; waited_ms < kPreviousExitWaitMs && proc_is_alive(old_pid); waited_ms += 50)
        {
            usleep(50 * 1000);
        }
        if (proc_is_alive(old_pid))
        {
            LOG_WARN("previous instance pid=%d ignored SIGTERM; killing it", static_cast<int>(old_pid));
            (void)kill(old_pid, SIGKILL);
            usleep(200 * 1000);
        }
    }
    orange::kDaemonReady.clear();
}

void stop_handler(int)
{
    request_shutdown();
}

void install_crash_handlers()
{
    struct sigaction action{};
    action.sa_handler = sig_handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    for (int i = 1; i < 12; i++)
    {
        if (i != SIGHUP && i != SIGINT && i != SIGQUIT)
        {
            sigaction(i, &action, nullptr);
        }
    }

    /* Requested exits are not crashes: only raise the flag; main() winds down. */
    struct sigaction stop{};
    stop.sa_handler = stop_handler;
    sigemptyset(&stop.sa_mask);
    for (int sig : {SIGHUP, SIGINT, SIGQUIT, SIGTERM})
    {
        sigaction(sig, &stop, nullptr);
    }
}

void start_worker_threads()
{
    std::thread([] { fps_sampler_thread(nullptr); }).detach();
    std::thread([] { hud_publisher_thread(); }).detach();
}

} // namespace

/* Crash records go to klog and to an append-only file the user can attach. */
static int g_crash_fd = -1;

static void crash_log(const char* fmt, ...)
{
    char line[0x400];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line, sizeof(line) - 1, fmt, ap);
    va_end(ap);
    if (n < 0)
    {
        return;
    }
    if ((size_t)n > sizeof(line) - 2)
    {
        n = (int)sizeof(line) - 2;
    }
    line[n++] = '\n';
    line[n] = '\0';
    klog_printf("[OrangeHUD][CRASH] %s", line);
    if (g_crash_fd >= 0)
    {
        (void)write(g_crash_fd, line, (size_t)n);
    }
}

void sig_handler(int signo)
{
    crash_log("signal %d received; OrangeHUD daemon has crashed", signo);
    print_backtrace(crash_log);
    notify_user(true, "OrangeHUD has crashed ...\n\nPlease attach /data/OrangeHUD/OrangeHUD_crash.log to a GitHub issue.");
    _exit(128 + signo);
}

int main()
{
    /* Raw elfldr uploads default to "payload.elf"; publish our stable name. */
    (void)syscall(SYS_thr_set_name, -1, "orange_daemon.elf");

    mkdir("/data/OrangeHUD", 0777);
    g_crash_fd = open("/data/OrangeHUD/OrangeHUD_crash.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
    /* Real linked kernel export (not a dlsym function-pointer variable). */
    notify_set_send(reinterpret_cast<notify_send_fn>(sceKernelSendNotificationRequest));

    stop_previous_instance();

    sceUserServiceInitialize(&DEFAULT_PRIORITY);
    LOG_DEBUG("daemon entered");

    /* Settings before any user-facing toast. */
    LoadSettings();

    OrbisKernelSwVersion sys_ver;
    sceKernelGetProsperoSystemSwVersion(&sys_ver);

    install_crash_handlers();

    LOG_INFO("=========== starting OrangeHUD version %s ... ===========", ORANGEHUD_VERSION);
    LOG_INFO("console system software 0x%X", static_cast<unsigned>(sys_ver.version >> 16));
    /* proc/query big-app / name lookups used by the FPS sampler. */
    proc_set_sce_hooks([](int pid, char* name) -> int { return sceKernelGetProcessName(pid, name); },
                       [](pid_t pid, void* info) -> int { return sceKernelGetAppInfo(pid, static_cast<app_info_t*>(info)); }, []() -> int { return sceSystemServiceGetAppIdOfRunningBigApp(); });

    start_worker_threads();
    orange::kDaemonReady.signal(getpid());

    static ShellUiInjector injector(shellui_elf_start);
    std::thread([] { injector.watch(); }).detach();
    injector.inject_now();

    notify_user(true, "Welcome to OrangeHUD " ORANGEHUD_VERSION);
    LOG_INFO("OrangeHUD HUD daemon running");

    /* Worker threads are detached; keep the process alive. */
    while (!shutting_down())
    {
        sleep(1);
    }

    /* Let the HUD publisher hand SceShellUI a final disabled frame so the panel
     * disappears now. The injected module and its hooks stay in SceShellUI. */
    for (int waited_ms = 0; waited_ms < kHudUnpublishWaitMs && !hud_unpublished(); waited_ms += 10)
    {
        usleep(10 * 1000);
    }
    orange::kDaemonReady.clear();
    LOG_INFO("OrangeHUD HUD daemon stopped");
    _exit(0);
}
