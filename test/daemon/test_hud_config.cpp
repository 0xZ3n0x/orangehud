// Standard library
#include <cstring>

// Project
#include "hud/frame_config.hpp"
#include "test_harness.h"

namespace
{

using namespace orange::hud;

int test_defaults_map_onto_the_frame()
{
    const orange::Settings s{};
    const OrangeHudFrame f = config_from_settings(s);
    TEST_ASSERT_EQ_INT(1, f.enabled);
    TEST_ASSERT_EQ_INT(1, f.background);
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        TEST_ASSERT_EQ_INT(1, f.show[i]);
        TEST_ASSERT_EQ_INT(i, f.order[i]);
    }
    return 0;
}

int test_settings_fields_land_in_their_frame_fields()
{
    orange::Settings s{};
    s.overlay_enabled = false;
    s.overlay_background = false;
    s.overlay_fps = false;
    s.overlay_gpu = false;
    s.overlay_ram = false;
    const OrangeHudFrame f = config_from_settings(s);
    TEST_ASSERT_EQ_INT(0, f.enabled);
    TEST_ASSERT_EQ_INT(0, f.background);
    TEST_ASSERT_EQ_INT(0, f.show[orange::kOverlayMetricFps]);
    TEST_ASSERT_EQ_INT(0, f.show[orange::kOverlayMetricGpu]);
    TEST_ASSERT_EQ_INT(0, f.show[orange::kOverlayMetricMemory]);
    return 0;
}

int test_per_core_usage_forces_the_cpu_metric_on()
{
    orange::Settings s{};
    s.overlay_cpu = false;
    s.all_cpu_usage = false;
    TEST_ASSERT_EQ_INT(0, config_from_settings(s).show[orange::kOverlayMetricCpu]);

    s.all_cpu_usage = true;
    const OrangeHudFrame f = config_from_settings(s);
    TEST_ASSERT_EQ_INT(1, f.show[orange::kOverlayMetricCpu]);
    TEST_ASSERT_EQ_INT(1, f.per_core);
    return 0;
}

int test_the_order_is_normalized_into_a_permutation()
{
    orange::Settings s{};
    s.overlay_order = {orange::kOverlayMetricMemory, orange::kOverlayMetricMemory, 99, orange::kOverlayMetricCpu};
    const OrangeHudFrame f = config_from_settings(s);
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricMemory, f.order[0]);
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricCpu, f.order[1]);
    bool seen[orange::kOverlayMetricCount] = {};
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        TEST_ASSERT_TRUE(f.order[i] < orange::kOverlayMetricCount);
        seen[f.order[i]] = true;
    }
    for (bool b : seen)
    {
        TEST_ASSERT_TRUE(b);
    }
    return 0;
}

int test_the_first_call_always_publishes_a_generation()
{
    OrangeHudFrame frame{};
    bool applied = false;
    const OrangeHudFrame next = config_from_settings(orange::Settings{});
    TEST_ASSERT_TRUE(apply_config(&frame, next, &applied));
    TEST_ASSERT_EQ_INT(1, static_cast<int>(frame.config_gen));
    TEST_ASSERT_TRUE(applied);
    return 0;
}

int test_an_unchanged_config_keeps_the_generation()
{
    OrangeHudFrame frame{};
    bool applied = false;
    const OrangeHudFrame next = config_from_settings(orange::Settings{});
    apply_config(&frame, next, &applied);
    TEST_ASSERT_TRUE(!apply_config(&frame, next, &applied));
    TEST_ASSERT_TRUE(!apply_config(&frame, next, &applied));
    TEST_ASSERT_EQ_INT(1, static_cast<int>(frame.config_gen));
    return 0;
}

int test_a_changed_field_bumps_the_generation_once()
{
    OrangeHudFrame frame{};
    bool applied = false;
    orange::Settings s{};
    apply_config(&frame, config_from_settings(s), &applied);

    s.overlay_background = false;
    TEST_ASSERT_TRUE(apply_config(&frame, config_from_settings(s), &applied));
    TEST_ASSERT_EQ_INT(2, static_cast<int>(frame.config_gen));
    TEST_ASSERT_TRUE(!apply_config(&frame, config_from_settings(s), &applied));

    s.overlay_order = {orange::kOverlayMetricMemory, orange::kOverlayMetricGpu, orange::kOverlayMetricCpu, orange::kOverlayMetricFps};
    TEST_ASSERT_TRUE(apply_config(&frame, config_from_settings(s), &applied));
    TEST_ASSERT_EQ_INT(3, static_cast<int>(frame.config_gen));
    return 0;
}

int test_applying_a_config_leaves_the_texts_and_stamps_alone()
{
    OrangeHudFrame frame{};
    frame.seq = 42;
    frame.unix_ns = 123456789;
    std::strcpy(frame.value[orange::kOverlayMetricFps][0], "59.9");
    bool applied = false;
    apply_config(&frame, config_from_settings(orange::Settings{}), &applied);
    TEST_ASSERT_EQ_INT(42, static_cast<int>(frame.seq));
    TEST_ASSERT_EQ_U64(123456789, frame.unix_ns);
    TEST_ASSERT_STREQ("59.9", frame.value[orange::kOverlayMetricFps][0]);
    return 0;
}

int test_texts_alone_are_not_a_config_change()
{
    OrangeHudFrame a = config_from_settings(orange::Settings{});
    OrangeHudFrame b = a;
    std::strcpy(b.value[orange::kOverlayMetricCpu][1], "12%");
    b.unix_ns = 99;
    b.seq = 7;
    TEST_ASSERT_TRUE(!config_differs(a, b));
    b.pos = 2;
    TEST_ASSERT_TRUE(config_differs(a, b));
    return 0;
}

} // namespace

extern "C" int test_hud_config_suite(void)
{
    int failures = 0;
    failures += test_run("hud_config.defaults", test_defaults_map_onto_the_frame);
    failures += test_run("hud_config.fields", test_settings_fields_land_in_their_frame_fields);
    failures += test_run("hud_config.per_core", test_per_core_usage_forces_the_cpu_metric_on);
    failures += test_run("hud_config.order", test_the_order_is_normalized_into_a_permutation);
    failures += test_run("hud_config.first_gen", test_the_first_call_always_publishes_a_generation);
    failures += test_run("hud_config.unchanged", test_an_unchanged_config_keeps_the_generation);
    failures += test_run("hud_config.changed", test_a_changed_field_bumps_the_generation_once);
    failures += test_run("hud_config.keeps_texts", test_applying_a_config_leaves_the_texts_and_stamps_alone);
    failures += test_run("hud_config.texts_not_config", test_texts_alone_are_not_a_config_change);
    return failures;
}
