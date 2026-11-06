/* The HUD widgets: a MangoHud-style box in the top-left corner, one metric
 * per row, fixed 24pt, MangoHud colors, plus the frametime graph below it.
 * Where the widgets go is decided by orange::overlay::arrange_panel; this
 * file creates them and applies the resulting commands.
 */

#include "panel.hpp"

// Standard library
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>

// Project
#include "contract/hud_frame.h"
#include "hud/core/layout.hpp"
#include "hud/core/panel_scene.hpp"
#include "platform/clock.h"
#include "platform/log.h"
#include "shellui.hpp"

namespace
{

using Layout = orange::overlay::Layout;

struct Rgb
{
    float r, g, b;
};

constexpr Rgb rgb(int r, int g, int b)
{
    return {r / 255.0f, g / 255.0f, b / 255.0f};
}

/** One metric row: [LABEL] value [value]. */
struct HudMetric
{
    const char* title;
    const char* label_id;
    const char* value0_id;
    const char* value1_id; /* nullptr for single-value groups */
    const char* placeholder0;
    const char* placeholder1;
    float Layout::* origin_x;
    float Layout::* origin_y;
};

constexpr std::array<HudMetric, orange::kOverlayMetricCount> kMetrics = {{
    /* kOverlayMetricFps    */
    {"FPS", "id_fps_label", "id_fps_value", "id_fps_lows", "--", "1% --  0.1% --", &Layout::overlay_fps_x, &Layout::overlay_fps_y},
    /* kOverlayMetricCpu    */
    {"CPU", "id_cpu_label", "id_cpu_temp_value", "id_cpu_usage_value", "--C", "--%", &Layout::overlay_cpu_x, &Layout::overlay_cpu_y},
    /* kOverlayMetricGpu    */
    {"SoC", "id_gpu_label", "id_gpu_temp_value", "id_gpu_usage_value", "--C", "--", &Layout::overlay_gpu_x, &Layout::overlay_gpu_y},
    /* kOverlayMetricMemory */
    {"MEM", "id_ram_label", "id_ram_value", nullptr, "--", nullptr, &Layout::overlay_ram_x, &Layout::overlay_ram_y},
}};

/** MangoHud's defaults (data/MangoHud.conf): cpu=2E97CB, gpu=2E9762, ram=C26693,
 * text=FFFFFF, background=020202 at alpha 0.5. */
struct Style
{
    Rgb label_color[orange::kOverlayMetricCount]; /* indexed by metric */
    Rgb background;
    float background_alpha;
    int font_pt;
};

constexpr Style kStyle = {{rgb(255, 255, 255), rgb(0x2E, 0x97, 0xCB), rgb(0x2E, 0x97, 0x62), rgb(0xC2, 0x66, 0x93)}, rgb(0x02, 0x02, 0x02), 0.5f, orange::overlay::kPanelFontPt};

int current_font_pt()
{
    return kStyle.font_pt;
}

constexpr const char* kBgPanelName = "id_orange_overlay_bg";
constexpr const char* kUi2 = "Sce.PlayStation.PUI.UI2";

constexpr int kFontStyle = 1;    /* Bold bit */
constexpr int kFontWeight = 900; /* CSS black */
constexpr int kAlignLeft = 0;
constexpr int kAlignTop = 0;
constexpr float kInitialLabelWidth = 500.0f;

/** Widget ids and titles for the layout core, taken from the metric table. */
constexpr orange::overlay::PanelGroups make_groups()
{
    orange::overlay::PanelGroups groups{};
    for (size_t i = 0; i < kMetrics.size(); ++i)
    {
        groups[i] = {kMetrics[i].title, kMetrics[i].label_id, kMetrics[i].value0_id, kMetrics[i].value1_id};
    }
    return groups;
}
constexpr orange::overlay::PanelGroups kGroups = make_groups();

/**
 * What each widget currently shows. An update only touches Mono for labels
 * whose position, width or text actually changed; cleared whenever the
 * widgets are rebuilt.
 */
orange::overlay::PanelState g_panel_state;

constexpr Rgb kValueColor = {1.0f, 1.0f, 1.0f};

bool any_metric_enabled()
{
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        if (g_hud.show[i])
        {
            return true;
        }
    }
    return false;
}

MonoClass* ui2_class(const char* name)
{
    return mono_class_from_name(pui_img, kUi2, name);
}

MonoObject* find_root()
{
    return Get_Property<MonoObject*>(pui_img, kUi2, "Scene", Game, "RootWidget");
}

MonoObject* find_widget(MonoObject* root, const char* name)
{
    if (!root || !name)
    {
        return nullptr;
    }
    return Invoke<MonoObject*>(pui_img, ui2_class("Widget"), root, "FindWidgetByName", mono_string_new(Root_Domain, name));
}

void remove_widget(MonoObject* root, const char* name)
{
    if (MonoObject* w = find_widget(root, name))
    {
        Invoke<void>(pui_img, ui2_class("Widget"), w, "RemoveFromParent");
    }
}

/** Each label lives in its own "<id>_cell" Panel; remove the cell if present. */
void remove_label(MonoObject* root, const char* label_id)
{
    char cell_name[96];
    std::snprintf(cell_name, sizeof(cell_name), "%s_cell", label_id);
    if (find_widget(root, cell_name))
    {
        remove_widget(root, cell_name);
    }
    else
    {
        remove_widget(root, label_id);
    }
}

template <typename Fn> void for_each_label_id(const HudMetric& m, Fn&& fn)
{
    fn(m.label_id);
    fn(m.value0_id);
    if (m.value1_id)
    {
        fn(m.value1_id);
    }
}

/* ---- Frametime graph: MangoHud frame_timing, drawn with Panels ----
 * 144 columns x 2px, redrawn at 10 Hz. With frame times the columns show
 * the last ORANGE_HUD_FT_N (96) frame times, stretched over the 144 columns
 * (about 1.6 s at 60 fps); without them, the smoothed FPS sampled at 10 Hz (about
 * 14 s). Segments sit on the root with absolute positions (the same pattern as
 * the label cells). */
constexpr const char* kGraphBgName = "id_graph_bg";
constexpr const char* kGraphHeaderId = "id_gheader";
constexpr const char* kGraphMinMaxId = "id_gminmax";
constexpr int kGraphCols = 144;     /* break points of the polyline */
constexpr float kGraphColW = 2.0f;  /* horizontal step between points */
constexpr float kGraphLineH = 2.0f; /* line thickness */
constexpr float kGraphH = 64.0f;
constexpr float kGraphScaleMs = 50.0f; /* MangoHud: fixed 0-50 ms vertical scale */

float g_graph_hist[kGraphCols] = {};
/* What the segments and the min/max header show; reset when they are rebuilt. */
float g_graph_prev_hh[kGraphCols - 1] = {};
float g_graph_prev_top[kGraphCols - 1] = {};
int g_graph_prev_mm_key = -1;
float g_graph_x = 0.0f;
float g_graph_top = 0.0f;
float g_bg_width = 0.0f; /* last applied background width (hugs the content) */

float parse_leading_number(const char* s)
{
    if (!s || !s[0])
    {
        return 0.0f;
    }
    return static_cast<float>(std::atof(s));
}

/** Y position of an ms value: up = slow frame, fixed 0-50 ms scale. */
float point_y_ms(float ms)
{
    if (ms < 0.0f)
    {
        ms = 0.0f;
    }
    if (ms > kGraphScaleMs)
    {
        ms = kGraphScaleMs;
    }
    return g_graph_top + kGraphH - (ms / kGraphScaleMs) * kGraphH;
}

void remove_graph(MonoObject* root)
{
    remove_widget(root, kGraphBgName);
    remove_label(root, kGraphHeaderId);
    remove_label(root, kGraphMinMaxId);
    std::fill(std::begin(g_graph_prev_hh), std::end(g_graph_prev_hh), 0.0f);
    std::fill(std::begin(g_graph_prev_top), std::end(g_graph_prev_top), 0.0f);
    g_graph_prev_mm_key = -1;
    for (int i = 0; i < kGraphCols - 1; ++i)
    {
        char name[32];
        std::snprintf(name, sizeof(name), "id_gline_%d", i);
        remove_widget(root, name);
    }
}

void remove_all(MonoObject* root)
{
    remove_graph(root);
    g_bg_width = 0.0f;
    remove_widget(root, kBgPanelName);
    for (const HudMetric& m : kMetrics)
    {
        for_each_label_id(m, [&](const char* id) { remove_label(root, id); });
    }
}

/** Common label geometry: absolute positioned, one line, left/top aligned. */
void set_label_geometry(MonoClass* label_cls, MonoObject* label, float margin_top, float width)
{
    Set_Property(label_cls, label, "PositionType", 1);
    Set_Property(label_cls, label, "MarginLeft", 0.0f);
    Set_Property(label_cls, label, "MarginTop", margin_top);
    Set_Property(label_cls, label, "Width", width);
    Set_Property(label_cls, label, "HorizontalAlignment", kAlignLeft);
    Set_Property(label_cls, label, "VerticalAlignment", kAlignTop);
    Set_Property(label_cls, label, "FitWidthToText", false);
    Set_Property(label_cls, label, "FitHeightToText", true);
    Set_Property(label_cls, label, "NumberOfLines", 1);
}

MonoObject* create_cell(MonoObject* root, const char* label_id, float x, float y)
{
    MonoClass* panel_cls = ui2_class("Panel");
    if (!panel_cls)
    {
        return nullptr;
    }
    MonoObject* cell = New_Object(panel_cls);
    if (!cell)
    {
        return nullptr;
    }
    mono_runtime_object_init(cell);

    char cell_name[96];
    std::snprintf(cell_name, sizeof(cell_name), "%s_cell", label_id);
    Set_Property(panel_cls, cell, "Name", mono_string_new(Root_Domain, cell_name));
    Set_Property(panel_cls, cell, "X", x);
    Set_Property(panel_cls, cell, "Y", y);
    Set_Property(panel_cls, cell, "Width", kInitialLabelWidth);
    Set_Property(panel_cls, cell, "Height", g_overlay_layout.row_h);
    Set_Property(panel_cls, cell, "BackgroundVisibility", false);
    Widget_Append_Child(root, cell);
    return cell;
}

/** Translucent box behind the panel widgets. */
void create_background(MonoObject* root)
{
    if (!g_hud.background || !any_metric_enabled())
    {
        return;
    }
    if (find_widget(root, kBgPanelName))
    {
        return;
    }

    MonoClass* panel_cls = ui2_class("Panel");
    MonoObject* panel = panel_cls ? New_Object(panel_cls) : nullptr;
    if (!panel)
    {
        LOG_ERROR("overlay: failed to create the background panel");
        return;
    }
    mono_runtime_object_init(panel);

    Set_Property(panel_cls, panel, "Name", mono_string_new(Root_Domain, kBgPanelName));
    Set_Property(panel_cls, panel, "X", g_overlay_layout.x);
    Set_Property(panel_cls, panel, "Y", g_overlay_layout.y);
    Set_Property(panel_cls, panel, "Width", g_overlay_layout.w);
    Set_Property(panel_cls, panel, "Height", g_overlay_layout.h);
    if (MonoObject* bg = CreateUIColor(kStyle.background.r, kStyle.background.g, kStyle.background.b, kStyle.background_alpha))
    {
        Set_Property_Invoke(panel_cls, panel, "BackgroundColor", bg);
    }
    Set_Property(panel_cls, panel, "BackgroundVisibility", true);
    Set_Property(panel_cls, panel, "BackgroundOpacity", 1.0f);
    Set_Property(panel_cls, panel, "BackgroundStyle", 1);
    Widget_Append_Child(root, panel);
}

bool create_label(MonoObject* root, MonoObject* font, const char* id, const char* text, const Rgb& color, float x, float y)
{
    MonoClass* label_cls = ui2_class("Label");
    MonoObject* cell = create_cell(root, id, x, y);
    MonoObject* label = CreateLabel(id, 0.0f, g_overlay_layout.label_margin_top, text, font, kAlignLeft, kAlignTop, color.r, color.g, color.b, 1.0f);
    if (!label || !label_cls || !cell)
    {
        return false;
    }
    set_label_geometry(label_cls, label, g_overlay_layout.label_margin_top, kInitialLabelWidth);
    Widget_Append_Child(cell, label);
    return true;
}

bool create_metric(MonoObject* root, MonoObject* font, const HudMetric& m)
{
    /* Final positions are assigned by the layout pass that follows. */
    const int metric = static_cast<int>(&m - kMetrics.data());
    const float x = g_overlay_layout.*m.origin_x;
    const float y = g_overlay_layout.*m.origin_y;
    bool ok = create_label(root, font, m.label_id, m.title, kStyle.label_color[metric], x, y);
    ok &= create_label(root, font, m.value0_id, m.placeholder0, kValueColor, x, y);
    if (m.value1_id)
    {
        ok &= create_label(root, font, m.value1_id, m.placeholder1, kValueColor, x, y);
    }
    return ok;
}

/* Panel-based staircase polyline: column i draws the vertical link between
 * break points i and i+1. No rotation (the Transform path crashes ShellUI);
 * at 2px steps the eye reads it as one connected line. */
void place(MonoObject* root, MonoClass* label_cls, MonoClass* panel_cls, const char* id, float x, float y, float margin_top, float width);
void set_text(MonoObject* root, MonoClass* label_cls, const char* id, const char* text);

void create_graph(MonoObject* root, MonoObject* font)
{
    MonoClass* panel_cls = ui2_class("Panel");
    MonoClass* label_cls = ui2_class("Label");
    if (!panel_cls || !label_cls)
    {
        return;
    }

    const float bx = g_overlay_layout.x + orange::overlay::kPanelPadX;
    /* Leave room for the header line above (12pt "Frametime" + min/max). */
    const float top = g_overlay_layout.y + g_overlay_layout.h + 30.0f;
    const float head_y = top - 20.0f;
    g_graph_x = bx;
    g_graph_top = top;

    /* MangoHud header: "Frametime" left, min/max right (small font). */
    const char* hid = kGraphHeaderId;
    const char* mmd = kGraphMinMaxId;
    if (MonoObject* small_font = CreateUIFont(12, kFontStyle, kFontWeight))
    {
        if (create_label(root, small_font, hid, "Frametime", kValueColor, bx, head_y))
        {
            place(root, label_cls, panel_cls, hid, bx, head_y, 0.0f, 80.0f);
        }
        const float mm_x = bx + kGraphCols * kGraphColW - 170.0f;
        if (create_label(root, small_font, mmd, "min: --, max: --", kValueColor, mm_x, head_y))
        {
            place(root, label_cls, panel_cls, mmd, mm_x, head_y, 0.0f, 170.0f);
        }
    }

    /* The graph backdrop. */
    MonoObject* bg = New_Object(panel_cls);
    if (!bg)
    {
        return;
    }
    mono_runtime_object_init(bg);
    Set_Property(panel_cls, bg, "Name", mono_string_new(Root_Domain, kGraphBgName));
    Set_Property(panel_cls, bg, "X", bx);
    Set_Property(panel_cls, bg, "Y", top);
    Set_Property(panel_cls, bg, "Width", kGraphCols * kGraphColW);
    Set_Property(panel_cls, bg, "Height", kGraphH);
    if (MonoObject* col = CreateUIColor(0.0f, 0.0f, 0.0f, 0.35f))
    {
        Set_Property_Invoke(panel_cls, bg, "BackgroundColor", col);
    }
    Set_Property(panel_cls, bg, "BackgroundVisibility", true);
    Set_Property(panel_cls, bg, "BackgroundOpacity", 1.0f);
    Set_Property(panel_cls, bg, "BackgroundStyle", 1);
    Widget_Append_Child(root, bg);

    /* The line segments: frametime_color = 00FF00. */
    for (int i = 0; i < kGraphCols - 1; ++i)
    {
        MonoObject* seg = New_Object(panel_cls);
        if (!seg)
        {
            break;
        }
        mono_runtime_object_init(seg);
        char name[32];
        std::snprintf(name, sizeof(name), "id_gline_%d", i);
        Set_Property(panel_cls, seg, "Name", mono_string_new(Root_Domain, name));
        Set_Property(panel_cls, seg, "X", bx + static_cast<float>(i) * kGraphColW);
        Set_Property(panel_cls, seg, "Y", top + kGraphH - kGraphLineH);
        Set_Property(panel_cls, seg, "Width", kGraphColW);
        Set_Property(panel_cls, seg, "Height", kGraphLineH);
        if (MonoObject* col = CreateUIColor(0.0f, 1.0f, 0.0f, 1.0f))
        {
            Set_Property_Invoke(panel_cls, seg, "BackgroundColor", col);
        }
        Set_Property(panel_cls, seg, "BackgroundVisibility", true);
        Set_Property(panel_cls, seg, "BackgroundOpacity", 1.0f);
        Set_Property(panel_cls, seg, "BackgroundStyle", 1);
        Widget_Append_Child(root, seg);
    }

    LOG_INFO("overlay: fps line graph created with %d segments", kGraphCols - 1);
}

/** 10 Hz; draws the daemon's flip-ring frametimes like the MangoHud
 * frame_timing graph: fixed 0-50 ms scale, min/max header, green line. */
void graph_sample_and_update(const HudReadings& readings)
{
    static uint64_t last_ns = 0;
    const uint64_t now = monotonic_ns();
    if (now == 0)
    {
        return;
    }
    if (last_ns != 0 && now - last_ns < 100000000ull)
    {
        return;
    }
    last_ns = now;

    MonoObject* root = find_root();
    if (!root)
    {
        return;
    }
    MonoClass* panel_cls = ui2_class("Panel");
    MonoClass* label_cls = ui2_class("Label");
    if (!panel_cls || !label_cls)
    {
        return;
    }

    /* ms values: flip-ring data first, fps-string history as a fallback. */
    float ms[kGraphCols];
    bool have_ft = false;
    for (int i = 0; i < ORANGE_HUD_FT_N; ++i)
    {
        if (g_hud.ft10[i] != 0)
        {
            have_ft = true;
            break;
        }
    }
    if (have_ft)
    {
        for (int i = 0; i < kGraphCols; ++i)
        {
            ms[i] = static_cast<float>(g_hud.ft10[i * ORANGE_HUD_FT_N / kGraphCols]) * 0.1f;
        }
    }
    else
    {
        for (int i = 0; i < kGraphCols - 1; ++i)
        {
            g_graph_hist[i] = g_graph_hist[i + 1];
        }
        g_graph_hist[kGraphCols - 1] = parse_leading_number(readings.value0[orange::kOverlayMetricFps]);
        for (int i = 0; i < kGraphCols; ++i)
        {
            const float v = g_graph_hist[i];
            ms[i] = v > 1.0f ? 1000.0f / v : 0.0f;
        }
    }

    /* MangoHud: the window min/max go to the header; the scale is fixed. */
    float min_ms = ms[0];
    float max_ms = ms[0];
    for (int i = 1; i < kGraphCols; ++i)
    {
        if (ms[i] < min_ms)
        {
            min_ms = ms[i];
        }
        if (ms[i] > max_ms)
        {
            max_ms = ms[i];
        }
    }
    const int mm_key = static_cast<int>(min_ms * 10.0f + 0.5f) * 4096 + static_cast<int>(max_ms * 10.0f + 0.5f);
    if (mm_key != g_graph_prev_mm_key)
    {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "min: %.1fms, max: %.1fms", min_ms, max_ms);
        set_text(root, label_cls, kGraphMinMaxId, buf);
        g_graph_prev_mm_key = mm_key;
    }

    float py[kGraphCols];
    for (int i = 0; i < kGraphCols; ++i)
    {
        py[i] = point_y_ms(ms[i]);
    }

    for (int i = 0; i < kGraphCols - 1; ++i)
    {
        /* A column covers the vertical span of its two points + line thickness. */
        const float y0 = py[i];
        const float y1 = py[i + 1];
        const float seg_top = y0 < y1 ? y0 : y1;
        const float hh = (y0 > y1 ? y0 : y1) - seg_top + kGraphLineH;
        if (hh == g_graph_prev_hh[i] && seg_top == g_graph_prev_top[i])
        {
            continue; /* unchanged */
        }
        g_graph_prev_hh[i] = hh;
        g_graph_prev_top[i] = seg_top;

        char name[32];
        std::snprintf(name, sizeof(name), "id_gline_%d", i);
        MonoObject* seg = find_widget(root, name);
        if (!seg)
        {
            continue;
        }
        Set_Property(panel_cls, seg, "Y", seg_top);
        Set_Property(panel_cls, seg, "Height", hh);
    }
}

bool resolve_root_dimensions(MonoObject* root, float* screen_w, float* screen_h)
{
    if (std::isfinite(g_overlay_layout.screen_w) && std::isfinite(g_overlay_layout.screen_h) && g_overlay_layout.screen_w > 1.0f && g_overlay_layout.screen_h > 1.0f)
    {
        *screen_w = g_overlay_layout.screen_w;
        *screen_h = g_overlay_layout.screen_h;
        return true;
    }

    MonoClass* widget_cls = ui2_class("Widget");
    if (!widget_cls)
    {
        return false;
    }
    const float width = Get_Property<float>(widget_cls, root, "Width");
    const float height = Get_Property<float>(widget_cls, root, "Height");
    if (!std::isfinite(width) || !std::isfinite(height) || width <= 1.0f || height <= 1.0f)
    {
        return false;
    }

    LOG_DEBUG("overlay: RootWidget logical size %.1f x %.1f", width, height);
    *screen_w = width;
    *screen_h = height;
    return true;
}

/** Move a label (and its cell) and resize it. */
void place(MonoObject* root, MonoClass* label_cls, MonoClass* panel_cls, const char* id, float x, float y, float margin_top, float width)
{
    MonoObject* label = find_widget(root, id);
    if (!label)
    {
        return;
    }

    char cell_name[96];
    std::snprintf(cell_name, sizeof(cell_name), "%s_cell", id);
    if (MonoObject* cell = find_widget(root, cell_name))
    {
        Set_Property(panel_cls, cell, "X", x);
        Set_Property(panel_cls, cell, "Y", y);
        Set_Property(panel_cls, cell, "Width", width);
        Set_Property(panel_cls, cell, "Height", g_overlay_layout.row_h);
    }
    set_label_geometry(label_cls, label, margin_top, width);
}

void set_text(MonoObject* root, MonoClass* label_cls, const char* id, const char* text)
{
    if (MonoObject* label = find_widget(root, id))
    {
        Set_Property(label_cls, label, "Text", mono_string_new(Root_Domain, text));
    }
}

} // namespace

bool hud_panel_build()
{
    MonoObject* root = find_root();
    if (!root)
    {
        return false;
    }

    float screen_w = 0.0f;
    float screen_h = 0.0f;
    if (!resolve_root_dimensions(root, &screen_w, &screen_h))
    {
        return false;
    }
    apply_overlay_layout(screen_w, screen_h);

    g_panel_state.clear();
    remove_all(root);
    if (!g_hud.enabled)
    {
        return true;
    }

    MonoObject* font = CreateUIFont(current_font_pt(), kFontStyle, kFontWeight);
    if (!font)
    {
        return false;
    }

    create_background(root);

    bool ok = true;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        if (g_hud.show[i])
        {
            ok &= create_metric(root, font, kMetrics[i]);
        }
    }
    create_graph(root, font);
    if (!ok)
    {
        remove_all(root);
        return false;
    }

    /* First paint: placeholders packed in the panel. */
    HudReadings placeholders;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        placeholders.value0[i] = kMetrics[i].placeholder0;
        placeholders.value1[i] = kMetrics[i].placeholder1;
    }
    hud_panel_update(placeholders);
    return true;
}

void hud_panel_hide()
{
    MonoObject* root = find_root();
    if (!root)
    {
        return;
    }
    g_panel_state.clear();
    remove_all(root);
}

/**
 * Lay the enabled groups out as one packed run in the corner panel
 * (orange::overlay::arrange_panel) and apply only what differs from what is on
 * screen (PanelState::plan).
 */
void hud_panel_update(const HudReadings& readings)
{
    orange::overlay::PanelConfig cfg;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        cfg.show[i] = g_hud.show[i];
        cfg.order[i] = g_hud.order[i];
    }
    cfg.font_pt = static_cast<float>(current_font_pt());
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        cfg.origin_x[i] = g_overlay_layout.*kMetrics[i].origin_x;
        cfg.origin_y[i] = g_overlay_layout.*kMetrics[i].origin_y;
    }

    const auto scene = orange::overlay::arrange_panel(kGroups, cfg, readings);
    if (scene.count == 0)
    {
        return;
    }

    MonoObject* root = find_root();
    MonoClass* label_cls = ui2_class("Label");
    MonoClass* panel_cls = ui2_class("Panel");
    if (!root || !label_cls || !panel_cls)
    {
        return; /* scene not ready: the same commands are planned again next time */
    }

    /* The background hugs the content: widest row + graph + right pad. */
    {
        float max_right = g_graph_x + kGraphCols * kGraphColW; /* graph included */
        for (const orange::overlay::PanelPiece& p : scene.pieces)
        {
            const float right = p.x + p.width;
            if (right > max_right)
            {
                max_right = right;
            }
        }
        float wanted = max_right - g_overlay_layout.x + orange::overlay::kPanelPadX;
        const float max_w = g_overlay_layout.screen_w - g_overlay_layout.x - 4.0f;
        if (wanted > max_w)
        {
            wanted = max_w;
        }
        if (wanted < 120.0f)
        {
            wanted = 120.0f;
        }
        if (std::fabs(wanted - g_bg_width) > 2.0f)
        {
            if (MonoObject* bg = find_widget(root, kBgPanelName))
            {
                Set_Property(panel_cls, bg, "Width", wanted);
            }
            g_bg_width = wanted;
        }
    }

    const auto commands = g_panel_state.plan(kGroups, scene);
    const float margin_top = g_overlay_layout.label_margin_top;
    for (const orange::overlay::PanelCommand& c : commands)
    {
        switch (c.kind)
        {
        case orange::overlay::PanelCommand::Kind::Park:
            place(root, label_cls, panel_cls, c.id, c.x, c.y, margin_top, 0.0f);
            break;
        case orange::overlay::PanelCommand::Kind::Place:
            place(root, label_cls, panel_cls, c.id, c.x, c.y, margin_top, c.width);
            break;
        case orange::overlay::PanelCommand::Kind::SetText:
            set_text(root, label_cls, c.id, c.text);
            break;
        }
        g_panel_state.applied(c);
    }
}

void hud_graph_update(const HudReadings& readings)
{
    if (g_hud.enabled)
    {
        graph_sample_and_update(readings);
    }
}
