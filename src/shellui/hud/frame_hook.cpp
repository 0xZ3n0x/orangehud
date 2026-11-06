/* Per-frame hook: the daemon measures everything and publishes an
 * OrangeHudFrame; this side only rebuilds the widgets when the published
 * configuration changes and pushes the texts onto the panel.
 */

// Standard library
#include <cstring>

// Project
#include "boot/hook_lifecycle.hpp"
#include "contract/hud_frame.h"
#include "hud/core/frame_driver.hpp"
#include "hud/core/panel_scene.hpp"
#include "hud/panel.hpp"
#include "shellui.hpp"

namespace
{

HudReadings readings_from(const OrangeHudFrame& f)
{
    HudReadings r;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        r.value0[i] = f.value[i][0][0] ? f.value[i][0] : nullptr;
        r.value1[i] = f.value[i][1][0] ? f.value[i][1] : nullptr;
    }
    return r;
}

} // namespace

void OnRender_Hook(MonoObject* instance)
{
    static orange::overlay::FramePacer pacer;
    static orange::overlay::HudTracker tracker;
    static char last_values[sizeof(OrangeHudFrame::value)] = {};

    if (!pacer.tick(shellui_hooks_are_ready()))
    {
        OnRender_orig(instance);
        return;
    }

    OrangeHudFrame frame;
    if (orange_hud_read(&frame) != 0)
    {
        /* No fresh frame: the daemon exited, was killed or is restarting. The
         * module stays installed (hooks and code are never torn down) but
         * draws nothing until a fresh frame arrives and rebuilds the panel. */
        if (tracker.on_stale())
        {
            hud_panel_hide();
            std::memset(last_values, 0, sizeof(last_values));
        }
    }
    else
    {
        if (tracker.needs_build(frame.config_gen))
        {
            g_hud = frame;
            if (hud_panel_build())
            {
                tracker.built_ok(frame.config_gen);
            }
        }
        if (tracker.needs_update(std::memcmp(last_values, frame.value, sizeof(last_values)) != 0))
        {
            hud_panel_update(readings_from(frame));
            std::memcpy(last_values, frame.value, sizeof(last_values));
        }
        if (tracker.built())
        {
            /* g_hud is the snapshot the panel was built from; the graph needs the live frametimes. */
            std::memcpy(g_hud.ft10, frame.ft10, sizeof(g_hud.ft10));
            hud_graph_update(readings_from(frame));
        }
    }

    OnRender_orig(instance);
}
