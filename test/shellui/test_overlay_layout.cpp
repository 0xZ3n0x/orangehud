// Project
#include "hud/core/layout.hpp"
#include "test_harness.h"

namespace
{

using orange::overlay::compute_panel_layout;
using orange::overlay::kOffscreen;
using orange::overlay::Layout;
using orange::overlay::Metrics;

constexpr float kW = 1920.0f;
constexpr float kH = 1080.0f;

bool near(float a, float b)
{
    return a - b < 0.01f && b - a < 0.01f;
}

Metrics all_metrics()
{
    Metrics m{};
    m.enabled = true;
    m.show_fps = true;
    m.show_cpu = true;
    m.show_gpu = true;
    m.show_ram = true;
    m.per_core_cpu = false;
    m.font_size_pt = orange::overlay::kPanelFontPt;
    return m;
}

float panel_row_h()
{
    return static_cast<float>(orange::overlay::kPanelFontPt) * orange::overlay::kPanelRowFactor;
}

int test_panel_box_sits_in_the_top_left_corner()
{
    const Layout l = compute_panel_layout(kW, kH, all_metrics());
    const float scale = static_cast<float>(orange::overlay::kPanelFontPt) / orange::overlay::kFontH;
    TEST_ASSERT_TRUE(near(orange::overlay::kPanelX, l.x));
    TEST_ASSERT_TRUE(near(orange::overlay::kPanelY, l.y));
    TEST_ASSERT_TRUE(near(orange::overlay::kPanelWidth * scale, l.w));
    TEST_ASSERT_TRUE(near(4.0f * panel_row_h() + 2.0f * orange::overlay::kPanelPadY, l.h));
    TEST_ASSERT_TRUE(near(panel_row_h(), l.row_h));
    return 0;
}

int test_panel_stacks_metrics_in_order()
{
    const Layout l = compute_panel_layout(kW, kH, all_metrics());
    const float x = l.x + orange::overlay::kPanelPadX;
    const float y0 = l.y + orange::overlay::kPanelPadY;
    TEST_ASSERT_TRUE(near(x, l.overlay_fps_x));
    TEST_ASSERT_TRUE(near(x, l.overlay_ram_x));
    TEST_ASSERT_TRUE(near(y0, l.overlay_fps_y));
    TEST_ASSERT_TRUE(near(y0 + 1.0f * panel_row_h(), l.overlay_cpu_y));
    TEST_ASSERT_TRUE(near(y0 + 2.0f * panel_row_h(), l.overlay_gpu_y));
    TEST_ASSERT_TRUE(near(y0 + 3.0f * panel_row_h(), l.overlay_ram_y));
    return 0;
}

int test_panel_hidden_metric_leaves_no_gap()
{
    Metrics m = all_metrics();
    m.show_cpu = false;
    const Layout l = compute_panel_layout(kW, kH, m);
    TEST_ASSERT_TRUE(near(kOffscreen, l.overlay_cpu_x));
    TEST_ASSERT_TRUE(near(kOffscreen, l.overlay_cpu_y));
    TEST_ASSERT_TRUE(near(l.overlay_fps_y + panel_row_h(), l.overlay_gpu_y));
    TEST_ASSERT_TRUE(near(3.0f * panel_row_h() + 2.0f * orange::overlay::kPanelPadY, l.h));
    return 0;
}

int test_panel_per_core_cpu_keeps_the_cpu_row()
{
    Metrics m = all_metrics();
    m.show_cpu = false;
    m.per_core_cpu = true;
    const Layout l = compute_panel_layout(kW, kH, m);
    TEST_ASSERT_TRUE(l.overlay_cpu_y > 0.0f);
    return 0;
}

int test_panel_follows_the_requested_order()
{
    Metrics m = all_metrics();
    m.order = {orange::overlay::kMetricMemory, orange::overlay::kMetricFps, orange::overlay::kMetricGpu, orange::overlay::kMetricCpu};
    const Layout l = compute_panel_layout(kW, kH, m);
    TEST_ASSERT_TRUE(l.overlay_ram_y < l.overlay_fps_y);
    TEST_ASSERT_TRUE(l.overlay_fps_y < l.overlay_gpu_y);
    TEST_ASSERT_TRUE(l.overlay_gpu_y < l.overlay_cpu_y);
    return 0;
}

int test_panel_without_a_screen_or_disabled_parks_everything()
{
    Layout l = compute_panel_layout(0.0f, 0.0f, all_metrics());
    TEST_ASSERT_TRUE(near(kOffscreen, l.overlay_fps_x));
    TEST_ASSERT_TRUE(near(kOffscreen, l.overlay_ram_y));
    TEST_ASSERT_TRUE(near(0.0f, l.w));

    Metrics off = all_metrics();
    off.enabled = false;
    l = compute_panel_layout(kW, kH, off);
    TEST_ASSERT_TRUE(near(kOffscreen, l.overlay_fps_x));
    TEST_ASSERT_TRUE(near(2.0f * orange::overlay::kPanelPadY, l.h));
    return 0;
}

} // namespace

extern "C" int test_overlay_layout_suite(void)
{
    int failures = 0;
    failures += test_run("overlay_layout.box_in_top_left", test_panel_box_sits_in_the_top_left_corner);
    failures += test_run("overlay_layout.stacks_in_order", test_panel_stacks_metrics_in_order);
    failures += test_run("overlay_layout.hidden_metric", test_panel_hidden_metric_leaves_no_gap);
    failures += test_run("overlay_layout.per_core_row", test_panel_per_core_cpu_keeps_the_cpu_row);
    failures += test_run("overlay_layout.requested_order", test_panel_follows_the_requested_order);
    failures += test_run("overlay_layout.no_screen_or_disabled", test_panel_without_a_screen_or_disabled_parks_everything);
    return failures;
}
