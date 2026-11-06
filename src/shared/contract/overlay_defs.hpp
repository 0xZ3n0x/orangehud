/* Overlay vocabulary shared by the daemon (config/publisher) and the ShellUI
 * HUD (renderer). Header-only; no I/O.
 */
#pragma once

// Standard library
#include <array>

namespace orange
{

inline constexpr int kOverlayMetricCount = 4;
inline constexpr int kOverlayMetricFps = 0;
inline constexpr int kOverlayMetricCpu = 1;
inline constexpr int kOverlayMetricGpu = 2;
inline constexpr int kOverlayMetricMemory = 3;

inline constexpr int kOverlayFontSmall = 0;
inline constexpr int kOverlayFontMedium = 1;
inline constexpr int kOverlayFontLarge = 2;

inline constexpr int kOverlayFontPtSmall = 14;
inline constexpr int kOverlayFontPtMedium = 18;
inline constexpr int kOverlayFontPtLarge = 24;

inline constexpr std::array<int, kOverlayMetricCount> kOverlayOrderDefault = {kOverlayMetricFps, kOverlayMetricCpu, kOverlayMetricGpu, kOverlayMetricMemory};

inline int overlay_font_pt(int font_size)
{
    if (font_size == kOverlayFontSmall)
    {
        return kOverlayFontPtSmall;
    }
    if (font_size == kOverlayFontLarge)
    {
        return kOverlayFontPtLarge;
    }
    return kOverlayFontPtMedium;
}

inline const char* overlay_font_size_name(int font_size)
{
    if (font_size == kOverlayFontSmall)
    {
        return "small";
    }
    if (font_size == kOverlayFontLarge)
    {
        return "large";
    }
    return "medium";
}

/** Canonical config / log name of a metric (the single source of the names). */
constexpr const char* overlay_metric_name(int metric)
{
    switch (metric)
    {
    case kOverlayMetricCpu:
        return "cpu";
    case kOverlayMetricGpu:
        return "gpu";
    case kOverlayMetricMemory:
        return "memory";
    case kOverlayMetricFps:
    default:
        return "fps";
    }
}

inline void overlay_normalize_order(std::array<int, kOverlayMetricCount>& order)
{
    bool seen[kOverlayMetricCount] = {};
    std::array<int, kOverlayMetricCount> out{};
    int n = 0;
    for (int metric : order)
    {
        if (metric < 0 || metric >= kOverlayMetricCount || seen[metric])
        {
            continue;
        }
        seen[metric] = true;
        out[n++] = metric;
    }
    for (int metric = 0; metric < kOverlayMetricCount; ++metric)
    {
        if (seen[metric])
        {
            continue;
        }
        out[n++] = metric;
    }
    order = out;
}

} // namespace orange
