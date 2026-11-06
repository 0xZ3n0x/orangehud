/* HUD module injection and SceShellUI lifecycle recovery.
 */

#include "injector.hpp"

// Standard library
#include <thread>

// SDK
#include <ps5/kernel.h>
#include <unistd.h>

// Project
#include "console/power_state.hpp"
#include "contract/hud_frame.h"
#include "core/lifecycle.hpp"
#include "core/settings_store.hpp"
#include "inject/lock.hpp"
#include "inject/remote/injector.h"
#include "platform/log.h"
#include "platform/notify.h"
#include "platform/ready.hpp"
#include "policy.hpp"
#include "proc/query.h"
#include "settings/settings.hpp"

namespace
{

/*
 * A just-exec'd SceShellUI is only safe to patch once its userland libs are
 * mapped. Each name is polled for up to 60 x 500 ms.
 */
constexpr const char* kShellUiReadySprx[] = {
    "libSceNpTrophy.sprx",
    "libSceNpTrophy2.sprx",
};
constexpr useconds_t kSprxPollUs = 500 * 1000;
constexpr int kSprxPollMax = 60;

constexpr int kAttempts = 5;
constexpr useconds_t kRetryDelayUs[kAttempts - 1] = {
    1000 * 1000,
    2000 * 1000,
    4000 * 1000,
    8000 * 1000,
};

constexpr int kReadyTimeoutMs = 45 * 1000;
constexpr int kReadyPollMs = 250;
constexpr int kResolveTries = 30;
constexpr useconds_t kResumeSettleUs = 1000 * 1000;
constexpr useconds_t kWatchPollUs = 500 * 1000;

/** sysctl allproc through the proc/query helpers; no PID scan. */
pid_t live_shellui_pid()
{
    return find_pid("SceShellUI");
}

bool sprx_loaded(pid_t pid, const char* name)
{
    uint32_t handle = static_cast<uint32_t>(-1);
    return kernel_dynlib_handle(pid, name, &handle) == 0 && handle != static_cast<uint32_t>(-1);
}

int as_int(pid_t pid)
{
    return static_cast<int>(pid);
}

} // namespace

ShellUiInjector::Outcome ShellUiInjector::inject_once(pid_t expected_pid)
{
    std::lock_guard<std::mutex> lock(inject_mutex());

    const pid_t before = live_shellui_pid();
    /* An event PID is authoritative; reject only an observed replacement. */
    if (expected_pid > 1 && before > 1 && before != expected_pid)
    {
        return {Result::TargetChanged, expected_pid};
    }

    const pid_t pid = expected_pid > 1 ? expected_pid : before;
    if (pid <= 0)
    {
        return {Result::TargetNotFound, pid};
    }
    pid_t published = -1;
    unsigned abi = 0;
    const bool have_marker = orange::kHudReady.read(&published, &abi);
    switch (orange::inject::marker_state(have_marker, published, abi, pid, ORANGE_HUD_ABI))
    {
    case orange::inject::MarkerState::Stale:
        /* A module with another ABI cannot be replaced in a running ShellUI:
         * hooking the same method twice fails, so ask for a restart instead. */
        return {Result::StaleModule, pid};
    case orange::inject::MarkerState::Current:
        return {Result::AlreadyReady, pid};
    case orange::inject::MarkerState::Absent:
        break;
    }

    /* A stale marker belongs to another instance. */
    orange::kHudReady.clear();
    LOG_DEBUG("inject: injecting HUD module into SceShellUI pid=%d", as_int(pid));
    if (!inject_elf(as_int(pid), elf_, nullptr))
    {
        orange::kHudReady.clear();
        return {Result::InjectFailed, pid};
    }
    if (!orange::kHudReady.wait(pid, kReadyTimeoutMs, kReadyPollMs))
    {
        orange::kHudReady.clear();
        return {Result::ReadyTimeout, pid};
    }

    const pid_t after = live_shellui_pid();
    if (after > 1 && after != pid)
    {
        orange::kHudReady.clear();
        return {Result::TargetChanged, pid};
    }
    return {Result::Injected, pid};
}

void ShellUiInjector::report(const Outcome& o, bool notify_failure) const
{
    const int pid = as_int(o.pid);
    switch (o.result)
    {
    case Result::AlreadyReady:
        LOG_DEBUG("inject: HUD already active in SceShellUI pid=%d", pid);
        return;
    case Result::StaleModule:
        LOG_WARN("inject: SceShellUI pid=%d runs an older HUD module", pid);
        notify_user(true, "OrangeHUD was updated. Restart the console to load the new HUD.");
        return;
    case Result::Injected:
        LOG_INFO("inject: HUD online in SceShellUI pid=%d", pid);
        return;
    case Result::TargetNotFound:
        LOG_DEBUG("inject: SceShellUI not found");
        if (notify_failure)
        {
            notify_user(true, "Failed to find SceShellUI");
        }
        return;
    case Result::InjectFailed:
        LOG_DEBUG("inject: injection failed pid=%d", pid);
        if (notify_failure)
        {
            notify_user(true, "Failed to inject the HUD into SceShellUI");
        }
        return;
    case Result::ReadyTimeout:
        LOG_DEBUG("inject: HUD ready timeout pid=%d", pid);
        if (notify_failure)
        {
            notify_user(true, "The HUD did not start in time (ShellUI left running)");
        }
        return;
    case Result::TargetChanged:
        LOG_DEBUG("inject: SceShellUI changed during injection pid=%d", pid);
        if (notify_failure)
        {
            notify_user(true, "SceShellUI restarted while loading the HUD");
        }
        return;
    }
}

bool ShellUiInjector::wait_for_sprx(pid_t pid, uint32_t gen, const char* name) const
{
    for (int i = 0; i < kSprxPollMax; ++i)
    {
        if (superseded(gen))
        {
            return false;
        }
        if (!proc_is_alive(pid))
        {
            LOG_DEBUG("inject: pid=%d died while waiting for %s", as_int(pid), name);
            return false;
        }
        if (sprx_loaded(pid, name))
        {
            LOG_DEBUG("inject: %s loaded pid=%d after %d ms", name, as_int(pid), i * 500);
            return true;
        }
        usleep(kSprxPollUs);
    }
    LOG_DEBUG("inject: %s still missing pid=%d after %d ms", name, as_int(pid), kSprxPollMax * 500);
    return false;
}

bool ShellUiInjector::wait_for_resume_delay(pid_t pid, uint32_t gen) const
{
    uint64_t delay = current_settings().rest_mode_delay_seconds;
    if (delay > static_cast<uint64_t>(orange::kRestModeDelayMaxSeconds))
    {
        delay = orange::kRestModeDelayMaxSeconds;
    }
    if (delay == 0)
    {
        return true;
    }

    LOG_DEBUG("inject: resume delay %llu s pid=%d", static_cast<unsigned long long>(delay), as_int(pid));
    for (uint64_t i = 0; i < delay; ++i)
    {
        if (superseded(gen) || !proc_is_alive(pid))
        {
            return false;
        }
        sleep(1);
    }
    return !superseded(gen) && proc_is_alive(pid);
}

bool ShellUiInjector::recover_once(pid_t exec_pid, pid_t previous_pid, uint32_t gen, bool apply_resume_delay)
{
    LOG_DEBUG("inject: ShellUI pid %d -> %d gen=%u", as_int(previous_pid), as_int(exec_pid), gen);
    for (const char* name : kShellUiReadySprx)
    {
        if (!wait_for_sprx(exec_pid, gen, name))
        {
            return false;
        }
    }
    if (apply_resume_delay && !wait_for_resume_delay(exec_pid, gen))
    {
        return false;
    }
    if (superseded(gen))
    {
        return false;
    }

    if (previous_pid > 1 && previous_pid != exec_pid)
    {
        notify_user(true, "Coming out of Rest Mode — re-activating the HUD...");
    }

    /* Failures are reported once, after the retry loop gives up. */
    const Outcome outcome = inject_once(exec_pid);
    remember(outcome.pid);
    report(outcome, /*notify_failure=*/false);
    /* A stale module cannot be fixed by retrying. */
    return orange::inject::ends_recovery(outcome.result);
}

void ShellUiInjector::recover(pid_t exec_pid, pid_t previous_pid, uint32_t gen, bool resume_delay)
{
    for (int attempt = 0; attempt < kAttempts; ++attempt)
    {
        if (superseded(gen))
        {
            return;
        }

        pid_t target = exec_pid;
        for (int tries = 0; target <= 1 && tries < kResolveTries; ++tries)
        {
            target = live_shellui_pid();
            if (target <= 1)
            {
                usleep(1000 * 1000);
            }
        }

        if (target > 1 && recover_once(target, previous_pid, gen, resume_delay && attempt == 0))
        {
            remember(target);
            return;
        }
        if (attempt + 1 < kAttempts)
        {
            LOG_DEBUG("inject: retry %d/%d target=%d", attempt + 1, kAttempts - 1, as_int(target));
            usleep(kRetryDelayUs[attempt]);
        }
    }
    LOG_WARN("inject: recovery exhausted gen=%u requested_pid=%d", gen, as_int(exec_pid));
    if (!superseded(gen))
    {
        notify_user(true, "Failed to inject the HUD into SceShellUI");
    }
}

void ShellUiInjector::schedule(pid_t exec_pid, bool resume_delay)
{
    if (exec_pid > 1 && power_state_is_sleeping())
    {
        remember(exec_pid);
        LOG_INFO("inject: deferred during standby pid=%d", as_int(exec_pid));
        return;
    }
    const pid_t previous = exec_pid > 1 ? last_shellui_.exchange(exec_pid, std::memory_order_acq_rel) : last_shellui_.load(std::memory_order_acquire);
    const uint32_t gen = generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
    std::thread([this, exec_pid, previous, gen, resume_delay] { recover(exec_pid, previous, gen, resume_delay); }).detach();
}

void ShellUiInjector::inject_now()
{
    /* Claim the PID first so the watcher does not start a second worker. */
    const pid_t pid = live_shellui_pid();
    remember(pid);
    schedule(pid > 1 ? pid : 0, /*resume_delay=*/false);
}

void ShellUiInjector::on_new_shellui(pid_t pid)
{
    /* A new image, even when the kernel reuses the PID. */
    orange::kHudReady.clear();
    if (power_state_is_sleeping())
    {
        remember(pid);
        LOG_INFO("inject: new ShellUI during standby, deferred pid=%d", as_int(pid));
        return;
    }
    schedule(pid, /*resume_delay=*/false);
}

void ShellUiInjector::poll_for_restart()
{
    const pid_t pid = live_shellui_pid();
    if (!orange::inject::is_new_shellui(pid, last_shellui_.load(std::memory_order_acquire)))
    {
        return;
    }
    LOG_INFO("inject: new SceShellUI pid=%d detected", as_int(pid));
    on_new_shellui(pid);
}

void ShellUiInjector::on_resume()
{
    if (power_state_is_sleeping())
    {
        return;
    }
    using orange::inject::ResumeAction;
    const pid_t pid = live_shellui_pid();
    switch (orange::inject::resume_action(pid, pid > 1 && orange::kHudReady.matches(pid)))
    {
    case ResumeAction::ScheduleAny:
        schedule(0, /*resume_delay=*/true);
        return;
    case ResumeAction::AlreadyUp:
        remember(pid);
        return;
    case ResumeAction::Schedule:
        LOG_INFO("inject: resume, scheduling SceShellUI pid=%d", as_int(pid));
        schedule(pid, /*resume_delay=*/true);
        return;
    }
}

void ShellUiInjector::watch()
{
    using orange::inject::WatchStep;
    bool was_sleeping = false;
    while (!shutting_down())
    {
        const PowerState state = power_state_get();
        const auto decision = orange::inject::watch_decision(was_sleeping, power_state_is_sleeping(state), state == PowerState::Working);
        was_sleeping = decision.was_sleeping;
        switch (decision.step)
        {
        case WatchStep::Resume:
            LOG_INFO("inject: WORKING transition, reconciling ShellUI");
            usleep(kResumeSettleUs); /* let the shell settle after wake */
            on_resume();
            break;
        case WatchStep::CheckRestart:
            poll_for_restart();
            break;
        case WatchStep::Idle:
            break;
        }
        usleep(kWatchPollUs);
    }
}
