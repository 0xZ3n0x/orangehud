/* The pure part of the HUD publisher: turn measured numbers into the texts the
 * ShellUI shows, and the little bits of arithmetic and parsing around the raw
 * readings. No system calls, so the host tests cover them; publisher.cpp
 * does the reading and the publishing.
 */
#pragma once

// Standard library
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace orange::hud
{

inline constexpr int kCpuCores = 8;

/**
 * FPS text: `value0` the rate, `value1` the 1% / 0.1% lows. `live` is whether
 * the sampler has a usable rate. Lows are empty when disabled.
 */
inline void format_fps(char* value0, char* value1, size_t cap, bool live, float fps, bool lows_enabled, float low1, float low01)
{
    if (live)
    {
        std::snprintf(value0, cap, fps >= 100.f ? "%.0f" : "%.1f", fps);
    }
    else
    {
        std::snprintf(value0, cap, "--");
    }

    if (!lows_enabled)
    {
        value1[0] = '\0';
    }
    else if (live && low1 >= 0.f && low01 >= 0.f)
    {
        std::snprintf(value1, cap, "1%% %.0f  0.1%% %.0f", low1, low01);
    }
    else
    {
        std::snprintf(value1, cap, "1%% --  0.1%% --");
    }
}

inline void format_cpu_temp(char* out, size_t cap, int celsius)
{
    std::snprintf(out, cap, "%dC", celsius);
}

/** Average over the cores, or one figure per core; "--%" until every core has a sample pair. */
inline void format_cpu_usage(char* out, size_t cap, bool sampled, bool per_core, const float usage[kCpuCores])
{
    if (!sampled)
    {
        std::snprintf(out, cap, "--%%");
    }
    else if (per_core)
    {
        std::snprintf(out, cap, "%2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%% %2.0f%%", usage[0], usage[1], usage[2], usage[3], usage[4], usage[5], usage[6], usage[7]);
    }
    else
    {
        float avg = 0.f;
        for (int i = 0; i < kCpuCores; ++i)
        {
            avg += usage[i];
        }
        std::snprintf(out, cap, "%.0f%%", avg / kCpuCores);
    }
}

/**
 * Busy share of one core in percent from its idle thread's run-time delta and
 * the wall-clock delta, both measured in the same direction. Clamped to
 * 0..100. `wall_delta_s` must not be zero.
 */
inline float core_usage_percent(float idle_delta_s, float wall_delta_s)
{
    float idle = idle_delta_s / wall_delta_s;
    if (idle > 1.0f)
    {
        idle = 1.0f;
    }
    if (idle < 0.0f)
    {
        idle = 0.0f;
    }
    return (1.0f - idle) * 100.0f;
}

/** "mapped/budget GB"; "--" when no game runs (mapped_mb < 0) or the budget is unknown. */
inline void format_memory(char* out, size_t cap, int mapped_mb, double budget_mb)
{
    if (mapped_mb >= 0 && budget_mb >= 1.0)
    {
        std::snprintf(out, cap, "%.1f/%.1f GB", mapped_mb / 1024.0, budget_mb / 1024.0);
    }
    else
    {
        std::snprintf(out, cap, "--");
    }
}

/** Hottest SoC sensor, or "--C" when no sensor answered (hottest < 0). */
inline void format_soc_temp(char* out, size_t cap, int hottest)
{
    if (hottest >= 0)
    {
        std::snprintf(out, cap, "%dC", hottest);
    }
    else
    {
        std::snprintf(out, cap, "--C");
    }
}

/** SoC power in whole watts from milliwatts; "--W" when the call failed or reported nothing. */
inline void format_soc_power(char* out, size_t cap, int rc, int milliwatts)
{
    if (rc == 0 && milliwatts > 0)
    {
        std::snprintf(out, cap, "%dW", (milliwatts + 500) / 1000);
    }
    else
    {
        std::snprintf(out, cap, "--W");
    }
}

/** Head of FreeBSD's struct kinfo_vmentry; structsize is the record stride. */
struct VmEntry
{
    int structsize;
    int type;
    uint64_t start;
    uint64_t end;
};

inline constexpr int kVmEntryTypeDevice = 4; // KVME_TYPE_DEVICE: direct memory

/** Megabytes of device (direct) memory in a KERN_PROC_VMMAP buffer. */
inline int mapped_device_mb(const char* buf, size_t size)
{
    uint64_t bytes = 0;
    for (size_t off = 0; off + sizeof(VmEntry) <= size;)
    {
        const auto* e = reinterpret_cast<const VmEntry*>(buf + off);
        if (e->structsize <= 0)
        {
            break;
        }
        if (e->type == kVmEntryTypeDevice)
        {
            bytes += e->end - e->start;
        }
        off += static_cast<size_t>(e->structsize);
    }
    return static_cast<int>(bytes / (1024 * 1024));
}

} // namespace orange::hud
