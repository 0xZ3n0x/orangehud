#pragma once

// Standard library
#include <array>
#include <cmath>

namespace orange::overlay
{

inline constexpr float kOffscreen = -4096.0f;
inline constexpr float kFontH = 18.0f; /* reference size the width tables were measured at */
inline constexpr float kTextTopInset = 5.0f;

/* Corner panel: a compact box in the top-left corner, one metric per row. */
inline constexpr int kPanelFontPt = 24;
inline constexpr float kPanelX = 24.0f;
inline constexpr float kPanelY = 24.0f;
inline constexpr float kPanelPadX = 10.0f;
inline constexpr float kPanelPadY = 5.0f;
inline constexpr float kPanelWidth = 360.0f;    /* at kFontH, scaled with the font */
inline constexpr float kPanelRowFactor = 1.45f; /* row height = font size * factor */
inline constexpr int kMetricCount = 4;
inline constexpr int kMetricFps = 0;
inline constexpr int kMetricCpu = 1;
inline constexpr int kMetricGpu = 2;
inline constexpr int kMetricMemory = 3;

struct Layout
{
    float screen_w = 0.0f;
    float screen_h = 0.0f;
    float x = 0.0f; /* panel top-left */
    float y = 0.0f;
    float w = 0.0f; /* initial panel width; the renderer widens it to fit the content */
    float h = 0.0f;
    float row_h = 0.0f; /* height of one metric row */
    float label_margin_top = kTextTopInset;
    float overlay_cpu_x = 0.0f;
    float overlay_cpu_y = 0.0f;
    float overlay_gpu_x = 0.0f;
    float overlay_gpu_y = 0.0f;
    float overlay_ram_x = 0.0f;
    float overlay_ram_y = 0.0f;
    float overlay_fps_x = 0.0f;
    float overlay_fps_y = 0.0f;
};

struct Metrics
{
    bool enabled = true;
    bool show_fps = true;
    bool show_cpu = true;
    bool show_gpu = true;
    bool show_ram = true;
    bool per_core_cpu = false;
    int font_size_pt = static_cast<int>(kFontH);
    /* Must match orange::kOverlayMetric* / kOverlayOrderDefault. */
    std::array<int, kMetricCount> order{kMetricFps, kMetricCpu, kMetricGpu, kMetricMemory};
};

inline void normalize_metric_order(std::array<int, kMetricCount>& order)
{
    bool seen[kMetricCount] = {};
    std::array<int, kMetricCount> out{};
    int n = 0;
    for (int metric : order)
    {
        if (metric < 0 || metric >= kMetricCount || seen[metric])
        {
            continue;
        }
        seen[metric] = true;
        out[n++] = metric;
    }
    for (int metric = 0; metric < kMetricCount; ++metric)
    {
        if (seen[metric])
        {
            continue;
        }
        out[n++] = metric;
    }
    order = out;
}

inline bool valid_screen_dimension(float value)
{
    return std::isfinite(value) && value > 1.0f;
}

/**
 * Corner panel: the visible metrics stacked in rows inside a box at the
 * top-left corner. Every metric gets its own origin (x, y); hidden metrics
 * are parked off screen. Edge and alignment do not apply.
 */
inline Layout compute_panel_layout(float screen_w, float screen_h, const Metrics& metrics)
{
    Layout out{};
    const int font_pt = metrics.font_size_pt > 0 ? metrics.font_size_pt : kPanelFontPt;
    const float scale = static_cast<float>(font_pt) / kFontH;
    out.row_h = static_cast<float>(font_pt) * kPanelRowFactor;
    out.label_margin_top = kTextTopInset;
    out.overlay_cpu_x = out.overlay_cpu_y = kOffscreen;
    out.overlay_gpu_x = out.overlay_gpu_y = kOffscreen;
    out.overlay_ram_x = out.overlay_ram_y = kOffscreen;
    out.overlay_fps_x = out.overlay_fps_y = kOffscreen;

    if (!valid_screen_dimension(screen_w) || !valid_screen_dimension(screen_h))
    {
        return out;
    }

    out.screen_w = screen_w;
    out.screen_h = screen_h;

    const bool show_cpu = metrics.enabled && (metrics.show_cpu || metrics.per_core_cpu);
    const bool show_gpu = metrics.enabled && metrics.show_gpu;
    const bool show_ram = metrics.enabled && metrics.show_ram;
    const bool show_fps = metrics.enabled && metrics.show_fps;
    const int visible = (show_fps ? 1 : 0) + (show_cpu ? 1 : 0) + (show_gpu ? 1 : 0) + (show_ram ? 1 : 0);

    out.x = kPanelX;
    out.y = kPanelY;
    out.w = kPanelWidth * scale;
    out.h = static_cast<float>(visible) * out.row_h + 2.0f * kPanelPadY;

    struct Slot
    {
        bool on;
        float* x;
        float* y;
    };
    Slot by_metric[kMetricCount] = {
        {show_fps, &out.overlay_fps_x, &out.overlay_fps_y},
        {show_cpu, &out.overlay_cpu_x, &out.overlay_cpu_y},
        {show_gpu, &out.overlay_gpu_x, &out.overlay_gpu_y},
        {show_ram, &out.overlay_ram_x, &out.overlay_ram_y},
    };

    std::array<int, kMetricCount> order = metrics.order;
    normalize_metric_order(order);

    int row = 0;
    for (int metric : order)
    {
        const Slot& slot = by_metric[metric];
        if (!slot.on)
        {
            continue;
        }
        *slot.x = out.x + kPanelPadX;
        *slot.y = out.y + kPanelPadY + static_cast<float>(row) * out.row_h;
        ++row;
    }
    return out;
}

} // namespace orange::overlay
