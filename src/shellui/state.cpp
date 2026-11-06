/* ShellUI process state: the last published HUD frame, the panel layout
 * derived from it, and the notify() helper.
 */

// Standard library
#include <cstdarg>

// Project
#include "contract/hud_frame.h"
#include "hud/core/layout.hpp"
#include "platform/log.h"
#include "platform/notify.h"
#include "shellui.hpp"

/* ---- process globals ---- */
OrangeHudFrame g_hud{};
OverlayLayout g_overlay_layout;

/* ---- notify ---- */
void notify(const char* text, ...)
{
    va_list args{};
    va_start(args, text);
    // show_watermark ignored by format; still prefixes [OrangeHUD] like daemons.
    // Send goes through notify_set_send trampoline (dlsym pointer safe).
    notify_v(/*show_watermark=*/0, text, args);
    va_end(args);
    LOG_DEBUG("Notify send returned (notify_user path)");
}

/* ---- layout ---- */
void apply_overlay_layout()
{
    const float screen_w = g_overlay_layout.screen_w;
    const float screen_h = g_overlay_layout.screen_h;
    orange::overlay::Metrics metrics{};
    metrics.enabled = g_hud.enabled;
    metrics.show_fps = g_hud.show[orange::kOverlayMetricFps];
    metrics.show_cpu = g_hud.show[orange::kOverlayMetricCpu];
    metrics.show_gpu = g_hud.show[orange::kOverlayMetricGpu];
    metrics.show_ram = g_hud.show[orange::kOverlayMetricMemory];
    metrics.per_core_cpu = g_hud.per_core;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        metrics.order[i] = g_hud.order[i];
    }
    metrics.font_size_pt = orange::overlay::kPanelFontPt; /* fixed size */
    g_overlay_layout = orange::overlay::compute_panel_layout(screen_w, screen_h, metrics);
}

void apply_overlay_layout(float screen_w, float screen_h)
{
    g_overlay_layout.screen_w = screen_w;
    g_overlay_layout.screen_h = screen_h;
    apply_overlay_layout();
}
