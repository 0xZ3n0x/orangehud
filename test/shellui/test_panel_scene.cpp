// Standard library
#include <cmath>
#include <cstring>

// Project
#include "hud/core/panel_scene.hpp"
#include "test_harness.h"

namespace
{

using orange::overlay::arrange_panel;
using orange::overlay::estimate_text_width;
using orange::overlay::kOffscreen;
using orange::overlay::PanelCommand;
using orange::overlay::PanelCommands;
using orange::overlay::PanelConfig;
using orange::overlay::PanelGroups;
using orange::overlay::PanelReadings;
using orange::overlay::PanelScene;
using orange::overlay::PanelState;
using Kind = PanelCommand::Kind;

const PanelGroups kGroups = {{
    {"FPS", "fps_label", "fps_value", "fps_lows"},
    {"CPU", "cpu_label", "cpu_temp", "cpu_usage"},
    {"SoC", "gpu_label", "gpu_temp", "gpu_usage"},
    {"MEM", "ram_label", "ram_value", nullptr},
}};

bool near(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

PanelConfig config()
{
    PanelConfig cfg;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        cfg.show[i] = 1;
        cfg.order[i] = static_cast<uint8_t>(i);
    }
    cfg.font_pt = 18.0f;
    return cfg;
}

PanelReadings fps_only(const char* fps = "60.0")
{
    PanelReadings r;
    r.value0[orange::kOverlayMetricFps] = fps;
    return r;
}

PanelReadings fps_and_cpu()
{
    PanelReadings r = fps_only();
    r.value0[orange::kOverlayMetricCpu] = "50C";
    r.value1[orange::kOverlayMetricCpu] = "12%";
    return r;
}

int index_of(const PanelScene& scene, const char* id)
{
    for (int i = 0; i < scene.count; ++i)
    {
        if (std::strcmp(scene.pieces[i].id, id) == 0)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

/** Apply every command, like the Mono side does when the scene is ready. */
void apply_all(PanelState& state, const PanelCommands& commands)
{
    for (const PanelCommand& c : commands)
    {
        state.applied(c);
    }
}

int count_kind(const PanelCommands& commands, Kind kind)
{
    int n = 0;
    for (const PanelCommand& c : commands)
    {
        n += c.kind == kind ? 1 : 0;
    }
    return n;
}

/* ---- arrange_panel ---- */

int test_nothing_visible_gives_no_scene()
{
    TEST_ASSERT_EQ_INT(0, arrange_panel(kGroups, config(), PanelReadings{}).count);

    PanelConfig cfg = config();
    cfg.show[orange::kOverlayMetricFps] = 0;
    TEST_ASSERT_EQ_INT(0, arrange_panel(kGroups, cfg, fps_only()).count);

    TEST_ASSERT_EQ_INT(0, arrange_panel(kGroups, config(), fps_only("")).count);
    return 0;
}

int test_single_group_is_label_then_value()
{
    const auto scene = arrange_panel(kGroups, config(), fps_only());
    TEST_ASSERT_EQ_INT(2, scene.count);
    TEST_ASSERT_STREQ("fps_label", scene.pieces[0].id);
    TEST_ASSERT_STREQ("FPS", scene.pieces[0].text);
    TEST_ASSERT_STREQ("fps_value", scene.pieces[1].id);
    TEST_ASSERT_STREQ("60.0", scene.pieces[1].text);
    return 0;
}

int test_second_value_only_when_present()
{
    PanelReadings r = fps_only();
    auto scene = arrange_panel(kGroups, config(), r);
    TEST_ASSERT_EQ_INT(-1, index_of(scene, "fps_lows"));

    r.value1[orange::kOverlayMetricFps] = "";
    scene = arrange_panel(kGroups, config(), r);
    TEST_ASSERT_EQ_INT(-1, index_of(scene, "fps_lows"));

    r.value1[orange::kOverlayMetricFps] = "1% 50  0.1% 40";
    scene = arrange_panel(kGroups, config(), r);
    TEST_ASSERT_TRUE(index_of(scene, "fps_lows") >= 0);
    return 0;
}

int test_order_decides_group_sequence()
{
    PanelConfig cfg = config();
    cfg.order[0] = orange::kOverlayMetricCpu;
    cfg.order[1] = orange::kOverlayMetricFps;
    cfg.order[2] = orange::kOverlayMetricGpu;
    cfg.order[3] = orange::kOverlayMetricMemory;
    const auto scene = arrange_panel(kGroups, cfg, fps_and_cpu());
    TEST_ASSERT_TRUE(index_of(scene, "cpu_label") < index_of(scene, "fps_label"));
    return 0;
}

int test_hidden_metric_is_skipped()
{
    PanelConfig cfg = config();
    cfg.show[orange::kOverlayMetricCpu] = 0;
    const auto scene = arrange_panel(kGroups, cfg, fps_and_cpu());
    TEST_ASSERT_EQ_INT(-1, index_of(scene, "cpu_label"));
    TEST_ASSERT_EQ_INT(-1, index_of(scene, "cpu_temp"));
    return 0;
}

int test_out_of_range_order_entries_are_ignored()
{
    PanelConfig cfg = config();
    cfg.order[1] = 200;
    const auto scene = arrange_panel(kGroups, cfg, fps_only());
    TEST_ASSERT_EQ_INT(2, scene.count);
    return 0;
}

int test_font_size_scales_gaps_and_widths()
{
    PanelConfig big = config();
    big.font_pt = 36.0f;
    const auto small_scene = arrange_panel(kGroups, config(), fps_only());
    const auto big_scene = arrange_panel(kGroups, big, fps_only());
    TEST_ASSERT_TRUE(near(2.0f * small_scene.pieces[0].width, big_scene.pieces[0].width));
    const float small_gap = small_scene.pieces[1].x - (small_scene.pieces[0].x + small_scene.pieces[0].width);
    const float big_gap = big_scene.pieces[1].x - (big_scene.pieces[0].x + big_scene.pieces[0].width);
    TEST_ASSERT_TRUE(near(2.0f * small_gap, big_gap));
    return 0;
}

/* ---- PanelState::plan ---- */

int test_first_update_parks_unused_then_places_and_sets_text()
{
    PanelState state;
    const auto scene = arrange_panel(kGroups, config(), fps_only());
    const auto commands = state.plan(kGroups, scene);

    /* Unused widgets: fps_lows, cpu x3, gpu x3, ram x2. */
    TEST_ASSERT_EQ_INT(9, count_kind(commands, Kind::Park));
    TEST_ASSERT_EQ_INT(2, count_kind(commands, Kind::Place));
    TEST_ASSERT_EQ_INT(2, count_kind(commands, Kind::SetText));
    /* Parking comes first. */
    TEST_ASSERT_TRUE(commands.items[0].kind == Kind::Park);
    TEST_ASSERT_TRUE(near(kOffscreen, commands.items[0].x));
    return 0;
}

int test_unchanged_scene_plans_nothing()
{
    PanelState state;
    const auto scene = arrange_panel(kGroups, config(), fps_only());
    apply_all(state, state.plan(kGroups, scene));
    TEST_ASSERT_EQ_INT(0, state.plan(kGroups, scene).count);
    return 0;
}

int test_text_change_without_width_change_only_sets_text()
{
    PanelState state;
    apply_all(state, state.plan(kGroups, arrange_panel(kGroups, config(), fps_only("60.0"))));

    /* Same width in the estimate ("60.0" -> "59.9"), different text. */
    const auto scene = arrange_panel(kGroups, config(), fps_only("59.9"));
    const auto commands = state.plan(kGroups, scene);
    TEST_ASSERT_EQ_INT(1, commands.count);
    TEST_ASSERT_TRUE(commands.items[0].kind == Kind::SetText);
    TEST_ASSERT_STREQ("fps_value", commands.items[0].id);
    TEST_ASSERT_STREQ("59.9", commands.items[0].text);
    return 0;
}

int test_metric_turned_off_is_parked_once_and_comes_back_placed()
{
    PanelState state;
    apply_all(state, state.plan(kGroups, arrange_panel(kGroups, config(), fps_and_cpu())));

    PanelConfig cfg = config();
    cfg.show[orange::kOverlayMetricCpu] = 0;
    auto commands = state.plan(kGroups, arrange_panel(kGroups, cfg, fps_and_cpu()));
    TEST_ASSERT_TRUE(count_kind(commands, Kind::Park) >= 3); /* cpu label + two values */
    apply_all(state, commands);
    TEST_ASSERT_EQ_INT(0, count_kind(state.plan(kGroups, arrange_panel(kGroups, cfg, fps_and_cpu())), Kind::Park));

    /* Back on: the parked widgets are placed again even though their text matches. */
    commands = state.plan(kGroups, arrange_panel(kGroups, config(), fps_and_cpu()));
    bool cpu_label_placed = false;
    for (const PanelCommand& c : commands)
    {
        cpu_label_placed = cpu_label_placed || (c.kind == Kind::Place && std::strcmp(c.id, "cpu_label") == 0);
    }
    TEST_ASSERT_TRUE(cpu_label_placed);
    return 0;
}

int test_unapplied_commands_are_planned_again()
{
    PanelState state;
    const auto scene = arrange_panel(kGroups, config(), fps_only());
    const auto first = state.plan(kGroups, scene);
    /* The scene was not ready: nothing was applied. */
    const auto second = state.plan(kGroups, scene);
    TEST_ASSERT_EQ_INT(first.count, second.count);
    return 0;
}

int test_clear_forgets_what_is_on_screen()
{
    PanelState state;
    const auto scene = arrange_panel(kGroups, config(), fps_only());
    apply_all(state, state.plan(kGroups, scene));
    state.clear();
    TEST_ASSERT_TRUE(state.plan(kGroups, scene).count > 0);
    return 0;
}

/* ---- panel geometry ---- */

constexpr float kPanelFont = 24.0f;

PanelConfig panel_config()
{
    PanelConfig cfg = config();
    cfg.font_pt = kPanelFont;
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        cfg.origin_x[i] = 34.0f;
        cfg.origin_y[i] = 29.0f + 35.0f * static_cast<float>(i);
    }
    return cfg;
}

const orange::overlay::PanelPiece* piece(const PanelScene& scene, const char* id)
{
    const int i = index_of(scene, id);
    return i < 0 ? nullptr : &scene.pieces[static_cast<size_t>(i)];
}

int test_panel_rows_start_at_their_origins()
{
    const auto scene = arrange_panel(kGroups, panel_config(), fps_and_cpu());
    TEST_ASSERT_TRUE(scene.count > 0);
    const auto* fps_label = piece(scene, "fps_label");
    const auto* cpu_label = piece(scene, "cpu_label");
    TEST_ASSERT_TRUE(fps_label != nullptr && cpu_label != nullptr);
    TEST_ASSERT_TRUE(near(34.0f, fps_label->x));
    TEST_ASSERT_TRUE(near(29.0f, fps_label->y));
    TEST_ASSERT_TRUE(near(34.0f, cpu_label->x));
    TEST_ASSERT_TRUE(near(64.0f, cpu_label->y));
    return 0;
}

int test_panel_values_follow_their_label_on_the_same_row()
{
    const auto scene = arrange_panel(kGroups, panel_config(), fps_and_cpu());
    const float scale = kPanelFont / 18.0f;
    const auto* label = piece(scene, "cpu_label");
    const auto* temp = piece(scene, "cpu_temp");
    const auto* usage = piece(scene, "cpu_usage");
    TEST_ASSERT_TRUE(label != nullptr && temp != nullptr && usage != nullptr);
    TEST_ASSERT_TRUE(near(label->y, temp->y) && near(label->y, usage->y));
    TEST_ASSERT_TRUE(near(label->x + label->width + 8.0f * scale, temp->x));
    TEST_ASSERT_TRUE(near(temp->x + temp->width + 10.0f * scale, usage->x));
    return 0;
}

int test_panel_skips_hidden_groups()
{
    PanelConfig cfg = panel_config();
    cfg.show[orange::kOverlayMetricFps] = 0;
    const auto scene = arrange_panel(kGroups, cfg, fps_and_cpu());
    TEST_ASSERT_TRUE(scene.count > 0);
    TEST_ASSERT_EQ_INT(-1, index_of(scene, "fps_label"));
    TEST_ASSERT_TRUE(index_of(scene, "cpu_label") >= 0);
    return 0;
}

int test_panel_row_moving_vertically_is_placed_again_without_new_text()
{
    PanelState state;
    PanelConfig cfg = panel_config();
    apply_all(state, state.plan(kGroups, arrange_panel(kGroups, cfg, fps_and_cpu())));

    cfg.origin_y[orange::kOverlayMetricCpu] += 35.0f;
    const auto commands = state.plan(kGroups, arrange_panel(kGroups, cfg, fps_and_cpu()));
    TEST_ASSERT_EQ_INT(0, count_kind(commands, Kind::SetText));
    TEST_ASSERT_EQ_INT(0, count_kind(commands, Kind::Park));
    /* Label and both values of the cpu row move; the fps row stays. */
    TEST_ASSERT_EQ_INT(3, count_kind(commands, Kind::Place));
    return 0;
}

} // namespace

extern "C" int test_panel_scene_suite(void)
{
    int failures = 0;
    failures += test_run("panel_scene.nothing_visible", test_nothing_visible_gives_no_scene);
    failures += test_run("panel_scene.single_group", test_single_group_is_label_then_value);
    failures += test_run("panel_scene.second_value", test_second_value_only_when_present);
    failures += test_run("panel_scene.order", test_order_decides_group_sequence);
    failures += test_run("panel_scene.hidden_metric", test_hidden_metric_is_skipped);
    failures += test_run("panel_scene.bad_order_entries", test_out_of_range_order_entries_are_ignored);
    failures += test_run("panel_scene.font_scale", test_font_size_scales_gaps_and_widths);
    failures += test_run("panel_state.first_update", test_first_update_parks_unused_then_places_and_sets_text);
    failures += test_run("panel_state.unchanged", test_unchanged_scene_plans_nothing);
    failures += test_run("panel_state.text_only", test_text_change_without_width_change_only_sets_text);
    failures += test_run("panel_state.park_and_return", test_metric_turned_off_is_parked_once_and_comes_back_placed);
    failures += test_run("panel_state.unapplied", test_unapplied_commands_are_planned_again);
    failures += test_run("panel_state.clear", test_clear_forgets_what_is_on_screen);
    failures += test_run("panel.origins", test_panel_rows_start_at_their_origins);
    failures += test_run("panel.row_gaps", test_panel_values_follow_their_label_on_the_same_row);
    failures += test_run("panel.hidden_group", test_panel_skips_hidden_groups);
    failures += test_run("panel.vertical_move", test_panel_row_moving_vertically_is_placed_again_without_new_text);
    return failures;
}
