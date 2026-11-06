/* The configuration half of the HUD frame: how the settings map onto it and
 * when a change must bump config_gen (which makes ShellUI rebuild the panel).
 * Pure, so the host tests cover it; publisher.cpp publishes the frame.
 */
#pragma once

// Standard library
#include <cstddef>
#include <cstring>

// Project
#include "contract/hud_frame.h"
#include "settings/settings.hpp"

namespace orange::hud
{

/** A frame whose configuration fields (enabled .. order) come from `s`; everything else is zero. */
inline OrangeHudFrame config_from_settings(const orange::Settings& s)
{
    OrangeHudFrame next{};
    next.enabled = s.overlay_enabled;
    next.background = s.overlay_background;
    next.per_core = s.all_cpu_usage;
    next.show[orange::kOverlayMetricFps] = s.overlay_fps;
    next.show[orange::kOverlayMetricCpu] = s.overlay_cpu || s.all_cpu_usage;
    next.show[orange::kOverlayMetricGpu] = s.overlay_gpu;
    next.show[orange::kOverlayMetricMemory] = s.overlay_ram;
    auto order = s.overlay_order;
    orange::overlay_normalize_order(order);
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        next.order[i] = static_cast<uint8_t>(order[i]);
    }
    return next;
}

/* The configuration fields are the bytes from `enabled` up to the texts. */
inline constexpr size_t kConfigBegin = offsetof(OrangeHudFrame, enabled);
inline constexpr size_t kConfigEnd = offsetof(OrangeHudFrame, value);

inline bool config_differs(const OrangeHudFrame& a, const OrangeHudFrame& b)
{
    return std::memcmp(reinterpret_cast<const char*>(&a) + kConfigBegin, reinterpret_cast<const char*>(&b) + kConfigBegin, kConfigEnd - kConfigBegin) != 0;
}

/** Copy only the configuration fields: the texts, stamp and generation of `to` stay. */
inline void copy_config(OrangeHudFrame* to, const OrangeHudFrame& from)
{
    std::memcpy(reinterpret_cast<char*>(to) + kConfigBegin, reinterpret_cast<const char*>(&from) + kConfigBegin, kConfigEnd - kConfigBegin);
}

/**
 * Bring `frame`'s configuration to `next`. Returns true, after bumping
 * config_gen, when anything changed or `*applied` says none was applied yet
 * (the first call always publishes a generation).
 */
inline bool apply_config(OrangeHudFrame* frame, const OrangeHudFrame& next, bool* applied)
{
    if (*applied && !config_differs(*frame, next))
    {
        return false;
    }
    copy_config(frame, next);
    ++frame->config_gen;
    *applied = true;
    return true;
}

} // namespace orange::hud
