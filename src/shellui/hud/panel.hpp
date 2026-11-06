/* The HUD panel: FPS / CPU / SoC / MEM rows and the frametime graph drawn on
 * the game's ShellUI scene. Every group is one entry of a metric table; there
 * is no per-metric create/remove/layout code.
 */
#pragma once

namespace orange::overlay
{
struct PanelReadings;
}

/** Current text of every metric (see orange::overlay::PanelReadings). */
using HudReadings = orange::overlay::PanelReadings;

/**
 * Resolve the scene, size the panel, and (re)create the widgets of every
 * metric enabled in g_hud with placeholder text; removes them all when the
 * HUD is disabled. Returns false while the scene is not
 * ready or widget creation failed (call again later); true when the HUD is
 * disabled or fully built.
 */
bool hud_panel_build();

/** Set metric texts and lay the rows out inside the panel. */
void hud_panel_update(const HudReadings& readings);

/** Remove every HUD widget. The module stays installed; a later hud_panel_build() draws it again. */
void hud_panel_hide();

/** Advance the frametime graph from the live frame; call every frame while the panel is built (it paces itself). */
void hud_graph_update(const HudReadings& readings);
