// Standard library
#include <cstdio>

extern "C" int test_hde64_suite(void);
extern "C" int test_ready_suite(void);
extern "C" int test_platform_notify_suite(void);
extern "C" int test_settings_suite(void);
extern "C" int test_overlay_text_metrics_suite(void);
extern "C" int test_overlay_layout_suite(void);
extern "C" int test_panel_scene_suite(void);
extern "C" int test_frame_driver_suite(void);
extern "C" int test_inject_policy_suite(void);
extern "C" int test_hud_text_suite(void);
extern "C" int test_hud_config_suite(void);
extern "C" int test_flip_times_suite(void);
extern "C" int test_hud_frame_suite(void);
extern "C" int test_frame_stats_suite(void);
extern "C" int test_trampoline_arena_suite(void);
extern "C" int test_hook_lifecycle_suite(void);
extern "C" int test_hotpatch_suite(void);
extern "C" int test_x64_relocator_suite(void);

int main()
{
    int failures = 0;

    failures += test_hde64_suite();
    failures += test_ready_suite();
    failures += test_platform_notify_suite();
    failures += test_settings_suite();
    failures += test_overlay_text_metrics_suite();
    failures += test_overlay_layout_suite();
    failures += test_panel_scene_suite();
    failures += test_frame_driver_suite();
    failures += test_inject_policy_suite();
    failures += test_hud_text_suite();
    failures += test_hud_config_suite();
    failures += test_flip_times_suite();
    failures += test_hud_frame_suite();
    failures += test_frame_stats_suite();
    failures += test_trampoline_arena_suite();
    failures += test_hook_lifecycle_suite();
    failures += test_hotpatch_suite();
    failures += test_x64_relocator_suite();

    if (failures == 0)
    {
        std::fprintf(stderr, "All host tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d host tests failed.\n", failures);
    return 1;
}
