/* HUD publisher: measures CPU / SoC / memory, folds in the FPS from
 * the samplers and the overlay config, and publishes one OrangeHudFrame that
 * the ShellUI module renders. All sampling lives here so the injected module
 * only draws.
 */

#include "publisher.hpp"

// Standard library
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

// SDK
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>

// Project
#include "console/running_game.hpp"
#include "contract/hud_frame.h"
#include "core/lifecycle.hpp"
#include "core/settings_store.hpp"
#include "fps/publish.hpp"
#include "frame_config.hpp"
#include "platform/log.h"
#include "text.hpp"

extern "C"
{
    struct OrbisKernelTimespec
    {
        int64_t tv_sec;
        int64_t tv_nsec;
    };
    struct Proc_Stats
    {
        int32_t lo_data;                           // 0x00
        uint32_t td_tid;                           // 0x04
        OrbisKernelTimespec user_cpu_usage_time;   // 0x08
        OrbisKernelTimespec system_cpu_usage_time; // 0x18
    }; // 0x28

    int sceKernelGetSocSensorTemperature(int sensorId, int* soctime);
    size_t sceKernelGetDirectMemorySize(void);
    /* Signature is not documented: called with generic pointer arguments, the
     * first output word is the SoC power in mW (live: ~100 W in game, ~6 W idle). */
    int sceKernelGetSocPowerConsumption(void* a, void* b, void* c, void* d);
    int sceKernelGetCpuUsage(struct Proc_Stats* out, int32_t* size);
    int sceKernelGetThreadName(uint32_t id, char* out);
    int sceKernelGetCpuTemperature(int* cputemp);
    int sceKernelClockGettime(int clockId, OrbisKernelTimespec* tp);
}

namespace
{

constexpr int kMaxProcThreads = 3072;
constexpr int kCpuCores = orange::hud::kCpuCores;
constexpr int kClockIdRealtime = 4;
constexpr int kMaxSocSensors = 32; // probed until the first unsupported id

constexpr useconds_t kTickUs = 100 * 1000; // FPS text cadence
constexpr int kSensorEveryTicks = 10;      // CPU/SoC/MEM cadence (1 s)

/** Per-core CPU usage from the idle threads' accumulated run time. */
class CpuMeter
{
  public:
    /** Returns false until every core has a sample pair to diff. */
    bool sample(float usage[kCpuCores])
    {
        if (!discover_idle_threads())
        {
            return false;
        }

        constexpr int kMaxAttempts = 4;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
        {
            Bank& bank = banks_[current_];
            bank.count = kMaxProcThreads;
            if (sceKernelGetCpuUsage(bank.threads, &bank.count))
            {
                continue;
            }
            sceKernelClockGettime(kClockIdRealtime, &bank.time);
            current_ = !current_;
            if (banks_[current_].count <= 0)
            {
                continue;
            }
            return diff(banks_[!current_], banks_[current_], usage);
        }
        return false;
    }

  private:
    struct Bank
    {
        OrbisKernelTimespec time{};
        int32_t count = 0;
        Proc_Stats threads[kMaxProcThreads];
    };

    bool discover_idle_threads()
    {
        if (idle_found_)
        {
            return true;
        }
        int32_t count = kMaxProcThreads;
        if (sceKernelGetCpuUsage(scratch_, &count) || count <= 0)
        {
            return false;
        }

        unsigned int found = 0;
        char name[0x40];
        int core = 0;
        for (int i = 0; i < count; ++i)
        {
            if (!sceKernelGetThreadName(scratch_[i].td_tid, name) && std::sscanf(name, "SceIdleCpu%d", &core) == 1 && core >= 0 && core < kCpuCores)
            {
                idle_tid_[core] = scratch_[i].td_tid;
                found |= 1u << core;
            }
        }
        idle_found_ = (found == (1u << kCpuCores) - 1u);
        return idle_found_;
    }

    static const Proc_Stats* find(const Bank& bank, uint32_t tid)
    {
        for (int i = 0; i < bank.count; ++i)
        {
            if (bank.threads[i].td_tid == tid)
            {
                return &bank.threads[i];
            }
        }
        return nullptr;
    }

    static float usage_time(const Proc_Stats& p)
    {
        return p.system_cpu_usage_time.tv_sec + p.system_cpu_usage_time.tv_nsec / 1000000.0f + p.user_cpu_usage_time.tv_sec + p.user_cpu_usage_time.tv_nsec / 1000000.0f;
    }

    /** `newer` was sampled after `older`. */
    bool diff(const Bank& newer, const Bank& older, float usage[kCpuCores]) const
    {
        const float dt = (older.time.tv_sec + older.time.tv_nsec / 1e9f) - (newer.time.tv_sec + newer.time.tv_nsec / 1e9f);
        if (dt == 0.0f)
        {
            return false;
        }
        for (int i = 0; i < kCpuCores; ++i)
        {
            const Proc_Stats* cur = find(newer, idle_tid_[i]);
            const Proc_Stats* prev = find(older, idle_tid_[i]);
            if (!cur || !prev)
            {
                return false;
            }
            usage[i] = orange::hud::core_usage_percent(usage_time(*prev) - usage_time(*cur), dt);
        }
        return true;
    }

    Bank banks_[2];
    Proc_Stats scratch_[kMaxProcThreads];
    uint32_t idle_tid_[kCpuCores] = {};
    bool idle_found_ = false;
    int current_ = 0;
};

/**
 * Direct (unified, GPU-accessible) memory the running game has mapped. The
 * PS5 has one memory pool, so this is the game's share of it, not a separate
 * VRAM; the matching budget is sceKernelGetDirectMemorySize() while it runs.
 */
class GameMemory
{
  public:
    /** MB mapped by the running game, or -1 when no game runs. */
    int mapped_mb()
    {
        RunningGame game;
        if (!running_game(&game) || game.pid <= 0)
        {
            return -1;
        }

        int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_VMMAP, game.pid};
        size_t size = 0;
        if (sysctl(mib, 4, nullptr, &size, nullptr, 0) != 0)
        {
            return -1;
        }
        size += size / 4 + 65536; // the map can grow between the two calls
        buf_.resize(size);
        if (sysctl(mib, 4, buf_.data(), &size, nullptr, 0) != 0)
        {
            return -1;
        }

        return orange::hud::mapped_device_mb(buf_.data(), size);
    }

  private:
    std::vector<char> buf_;
};

class HudPublisher
{
  public:
    void run()
    {
        if (!open_map())
        {
            return;
        }
        LOG_INFO("hud publisher started (%u us tick)", static_cast<unsigned>(kTickUs));

        for (unsigned tick = 0; !shutting_down(); ++tick)
        {
            if (tick % kSensorEveryTicks == 0)
            {
                (void)LoadSettings(false);
            }
            const orange::Settings cfg = current_settings();

            apply_config(cfg);
            if (frame_.enabled)
            {
                if (tick % kSensorEveryTicks == 0)
                {
                    sample_sensors();
                }
                format_fps();
                format_graph();
            }
            else
            {
                clear_text();
            }

            frame_.unix_ns = realtime_ns();
            orange_hud_seqlock_store(map_, &frame_);
            usleep(kTickUs);
        }
        publish_disabled();
    }

  private:
    using Text = char[ORANGE_HUD_TEXT_LEN];

    /** Last frame before exit: the ShellUI module sees a disabled HUD and removes the panel at once. */
    void publish_disabled()
    {
        frame_.enabled = 0;
        clear_text();
        ++frame_.config_gen;
        frame_.unix_ns = realtime_ns();
        orange_hud_seqlock_store(map_, &frame_);
        set_hud_unpublished();
    }

    bool open_map()
    {
        mkdir("/system_tmp", 0777);
        mkdir(ORANGE_SYSTEM_TMP_ROOT, 0777);
        const int fd = open(ORANGE_SYSTEM_TMP_HUD_FRAME, O_RDWR | O_CREAT, 0644);
        if (fd < 0 || ftruncate(fd, ORANGE_HUD_FRAME_BYTES) != 0)
        {
            LOG_ERROR("hud: cannot create %s", ORANGE_SYSTEM_TMP_HUD_FRAME);
            if (fd >= 0)
            {
                close(fd);
            }
            return false;
        }
        void* p = mmap(nullptr, ORANGE_HUD_FRAME_BYTES, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED)
        {
            LOG_ERROR("hud: mmap failed");
            return false;
        }
        map_ = static_cast<OrangeHudFrame*>(p);
        std::memset(map_, 0, ORANGE_HUD_FRAME_BYTES);
        return true;
    }

    /** Copy the overlay config into the frame; bump config_gen on change. */
    void apply_config(const orange::Settings& s)
    {
        fps_lows_ = s.overlay_fps_lows;
        orange::hud::apply_config(&frame_, orange::hud::config_from_settings(s), &config_set_);
    }

    char* text(int metric, int slot)
    {
        return frame_.value[metric][slot];
    }

    void clear_text()
    {
        std::memset(frame_.value, 0, sizeof(frame_.value));
        std::memset(frame_.ft10, 0, sizeof(frame_.ft10));
    }

    void format_fps()
    {
        constexpr int m = orange::kOverlayMetricFps;
        if (!frame_.show[m])
        {
            text(m, 0)[0] = '\0';
            text(m, 1)[0] = '\0';
            return;
        }
        OrangeFpsSample s{};
        const bool live = orange::fps::latest(&s) && s.fps >= 1.f;
        orange::hud::format_fps(text(m, 0), text(m, 1), ORANGE_HUD_TEXT_LEN, live, s.fps, fps_lows_, s.low1, s.low01);
    }

    /** Graph history is independent of the FPS text being shown. */
    void format_graph()
    {
        OrangeFpsSample s{};
        if (orange::fps::latest(&s))
        {
            std::memcpy(frame_.ft10, s.ft10, sizeof(frame_.ft10));
        }
        else
        {
            std::memset(frame_.ft10, 0, sizeof(frame_.ft10));
        }
    }

    void sample_sensors()
    {
        using namespace orange;
        for (int m = 0; m < kOverlayMetricCount; ++m)
        {
            if (!frame_.show[m])
            {
                text(m, 0)[0] = '\0';
                text(m, 1)[0] = '\0';
            }
        }

        if (frame_.show[kOverlayMetricCpu])
        {
            int cpu_t = 0;
            sceKernelGetCpuTemperature(&cpu_t);
            hud::format_cpu_temp(text(kOverlayMetricCpu, 0), ORANGE_HUD_TEXT_LEN, cpu_t);

            float usage[kCpuCores] = {};
            const bool sampled = cpu_.sample(usage);
            hud::format_cpu_usage(text(kOverlayMetricCpu, 1), ORANGE_HUD_TEXT_LEN, sampled, frame_.per_core, usage);
        }

        if (frame_.show[kOverlayMetricMemory])
        {
            // Mapped direct memory of the running game / the game's direct-memory
            // budget (both read while the game runs; "--" otherwise).
            const int mapped_mb = game_memory_.mapped_mb();
            const double budget_mb = static_cast<double>(sceKernelGetDirectMemorySize()) / (1024.0 * 1024.0);
            hud::format_memory(text(kOverlayMetricMemory, 0), ORANGE_HUD_TEXT_LEN, mapped_mb, budget_mb);
        }

        if (frame_.show[kOverlayMetricGpu])
        {
            // Hottest of the SoC sensors (sensor 0 is a cool spot of the die; the
            // hottest reads 10-15 C higher under load) and the SoC power in W.
            int hottest = -1;
            for (int id = 0; id < kMaxSocSensors; ++id)
            {
                int c = 0;
                if (sceKernelGetSocSensorTemperature(id, &c) != 0)
                {
                    break;
                }
                hottest = std::max(hottest, c);
            }
            hud::format_soc_temp(text(kOverlayMetricGpu, 0), ORANGE_HUD_TEXT_LEN, hottest);

            int out[4][16] = {};
            const int rc = sceKernelGetSocPowerConsumption(out[0], out[1], out[2], out[3]);
            hud::format_soc_power(text(kOverlayMetricGpu, 1), ORANGE_HUD_TEXT_LEN, rc, out[0][0]);
        }
    }

    bool fps_lows_ = true;
    GameMemory game_memory_;
    OrangeHudFrame* map_ = nullptr;
    OrangeHudFrame frame_{};
    bool config_set_ = false;
    CpuMeter cpu_;
};

} // namespace

void hud_publisher_thread()
{
    static HudPublisher publisher;
    publisher.run();
}
