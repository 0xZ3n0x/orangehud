/* The pure half of the HUD: where every widget goes (arrange_panel) and which
 * operations that takes given what is already on screen (PanelState).
 * No Mono, no globals, no heap: the sizes are known up front (four groups of
 * at most three widgets), and this runs every rendered frame inside SceShellUI.
 * The host tests cover it; panel.cpp only applies the resulting commands.
 */
#pragma once

// Standard library
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Project
#include "contract/overlay_defs.hpp"
#include "layout.hpp"
#include "text_metrics.hpp"

namespace orange::overlay
{

/** Every group has at most label + value0 + value1 on screen. */
inline constexpr int kPanelPieceCap = kOverlayMetricCount * 3;
/** Longest text a widget can show (the frame's ORANGE_HUD_TEXT_LEN). */
inline constexpr size_t kPanelTextCap = 48;

/**
 * Current text of every metric. value0 is the first (or only) value, value1
 * the optional second one (CPU/GPU usage). A null value0 hides the group.
 * Indexed by orange::kOverlayMetric*.
 */
struct PanelReadings
{
    const char* value0[kOverlayMetricCount] = {};
    const char* value1[kOverlayMetricCount] = {};
};

/** Title and widget ids of one metric group: [label] value0 [value1]. */
struct PanelGroup
{
    const char* title;
    const char* label_id;
    const char* value0_id;
    const char* value1_id; /* nullptr for single-value groups */
};

using PanelGroups = std::array<PanelGroup, kOverlayMetricCount>;

struct PanelConfig
{
    /* Indexed by metric; `order` is a permutation of the metrics. */
    uint8_t show[kOverlayMetricCount] = {};
    uint8_t order[kOverlayMetricCount] = {};
    float font_pt = kRefFontSize;

    /* Where each metric's row starts. */
    float origin_x[kOverlayMetricCount] = {};
    float origin_y[kOverlayMetricCount] = {};
};

/** One placed widget. `id` is a PanelGroup id; `text` points into the readings given to arrange_panel. */
struct PanelPiece
{
    const char* id;
    const char* text;
    float x;
    float y;
    float width;
};

/** Widgets of the visible groups, left to right. Valid while the readings are. */
struct PanelScene
{
    PanelPiece pieces[kPanelPieceCap] = {};
    int count = 0;
};

namespace detail
{

/** Groups that are enabled and have a first value, in display order. */
inline int shown_groups(const PanelConfig& cfg, const PanelReadings& readings, int* shown)
{
    int n = 0;
    for (const uint8_t metric : cfg.order)
    {
        if (metric >= kOverlayMetricCount)
        {
            continue;
        }
        const char* v0 = readings.value0[metric];
        if (cfg.show[metric] && v0 && v0[0])
        {
            shown[n++] = metric;
        }
    }
    return n;
}

/** One row per group, each starting at its own origin: LABEL value [value]. */
inline void arrange_rows(const PanelGroups& groups, const PanelConfig& cfg, const PanelReadings& readings, const int* shown, int shown_n, PanelScene* scene)
{
    const float scale = cfg.font_pt / kRefFontSize;
    const float kPairGap = 8.0f * scale; /* label -> first value */
    const float kValGap = 10.0f * scale; /* value -> value */

    for (int i = 0; i < shown_n; ++i)
    {
        const int metric = shown[i];
        const PanelGroup& m = groups[metric];
        const char* v0 = readings.value0[metric];
        const char* v1 = readings.value1[metric];
        const float y = cfg.origin_y[metric];

        float x = cfg.origin_x[metric];
        const float label_w = estimate_text_width(m.title, cfg.font_pt);
        scene->pieces[scene->count++] = {m.label_id, m.title, x, y, label_w};
        x += label_w + kPairGap;

        const float v0_w = estimate_text_width(v0, cfg.font_pt);
        scene->pieces[scene->count++] = {m.value0_id, v0, x, y, v0_w};
        if (m.value1_id && v1 && v1[0])
        {
            x += v0_w + kValGap;
            scene->pieces[scene->count++] = {m.value1_id, v1, x, y, estimate_text_width(v1, cfg.font_pt)};
        }
    }
}

} // namespace detail

/**
 * Lay the enabled groups out, one row per group. Widths are estimated from
 * the text so values expand instead of wrapping. count == 0 means there is
 * nothing to lay out: the caller leaves the screen as it is.
 */
inline PanelScene arrange_panel(const PanelGroups& groups, const PanelConfig& cfg, const PanelReadings& readings)
{
    PanelScene scene;
    int shown[kOverlayMetricCount];
    const int shown_n = detail::shown_groups(cfg, readings, shown);
    if (shown_n > 0)
    {
        detail::arrange_rows(groups, cfg, readings, shown, shown_n, &scene);
    }
    return scene;
}

/** One operation on the widget tree. */
struct PanelCommand
{
    enum class Kind
    {
        Park,    /* move the widget out of view */
        Place,   /* move and resize */
        SetText, /* replace the text */
    };
    Kind kind;
    const char* id;
    float x;
    float y;
    float width;
    const char* text;
};

/** At most park + place + set-text for every widget that can exist. */
inline constexpr int kPanelCommandCap = kPanelPieceCap * 3;

/** Fixed-capacity command list plan() fills. */
struct PanelCommands
{
    PanelCommand items[kPanelCommandCap] = {};
    int count = 0;

    bool empty() const
    {
        return count == 0;
    }
    const PanelCommand* begin() const
    {
        return items;
    }
    const PanelCommand* end() const
    {
        return items + count;
    }
    void add(const PanelCommand& c)
    {
        if (count < kPanelCommandCap)
        {
            items[count++] = c;
        }
    }
};

/**
 * What each widget currently shows. plan() lists only the operations whose
 * position, width or text actually differ; the caller reports each command it
 * managed to apply with applied(). A command that was not applied is planned
 * again on the next call.
 */
class PanelState
{
  public:
    /** Forget everything (the widgets were rebuilt or removed). */
    void clear()
    {
        for (Placed& p : placed_)
        {
            p = Placed{};
        }
    }

    /** Operations that bring the screen to `scene`: unused groups are parked first (once). */
    PanelCommands plan(const PanelGroups& groups, const PanelScene& scene) const
    {
        PanelCommands out;

        for (const PanelGroup& g : groups)
        {
            const char* ids[] = {g.label_id, g.value0_id, g.value1_id};
            for (const char* id : ids)
            {
                if (!id || in_scene(scene, id) || state_of(id).parked)
                {
                    continue;
                }
                out.add({PanelCommand::Kind::Park, id, kOffscreen, kOffscreen, 0.0f, nullptr});
            }
        }

        for (int i = 0; i < scene.count; ++i)
        {
            const PanelPiece& p = scene.pieces[i];
            const Placed& st = state_of(p.id);
            if (st.parked || st.x != p.x || st.y != p.y || st.width != p.width)
            {
                out.add({PanelCommand::Kind::Place, p.id, p.x, p.y, p.width, nullptr});
            }
            if (std::strcmp(st.text, p.text) != 0)
            {
                out.add({PanelCommand::Kind::SetText, p.id, 0.0f, 0.0f, 0.0f, p.text});
            }
        }
        return out;
    }

    void applied(const PanelCommand& c)
    {
        Placed& st = slot_of(c.id);
        st.id = c.id;
        switch (c.kind)
        {
        case PanelCommand::Kind::Park:
            st.parked = true;
            break;
        case PanelCommand::Kind::Place:
            st.x = c.x;
            st.y = c.y;
            st.width = c.width;
            st.parked = false;
            break;
        case PanelCommand::Kind::SetText:
            std::snprintf(st.text, sizeof(st.text), "%s", c.text ? c.text : "");
            break;
        }
    }

  private:
    /**
     * What one widget currently shows. `text` is a copy: the readings of one
     * tick point into that tick's frame copy and die with it.
     */
    struct Placed
    {
        const char* id = nullptr;
        float x = NAN;
        float y = NAN;
        float width = NAN;
        bool parked = false;
        char text[kPanelTextCap] = {};
    };

    static bool in_scene(const PanelScene& scene, const char* id)
    {
        for (int i = 0; i < scene.count; ++i)
        {
            if (std::strcmp(scene.pieces[i].id, id) == 0)
            {
                return true;
            }
        }
        return false;
    }

    const Placed& state_of(const char* id) const
    {
        for (const Placed& p : placed_)
        {
            if (p.id && std::strcmp(p.id, id) == 0)
            {
                return p;
            }
        }
        static const Placed kUnseen;
        return kUnseen;
    }

    /* Ids come from the fixed group table, so the table is never full. */
    Placed& slot_of(const char* id)
    {
        Placed* free_slot = nullptr;
        for (Placed& p : placed_)
        {
            if (p.id && std::strcmp(p.id, id) == 0)
            {
                return p;
            }
            if (!p.id && !free_slot)
            {
                free_slot = &p;
            }
        }
        return *free_slot;
    }

    Placed placed_[kPanelPieceCap] = {};
};

} // namespace orange::overlay
