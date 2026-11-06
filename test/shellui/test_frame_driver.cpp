// Project
#include "hud/core/frame_driver.hpp"
#include "test_harness.h"

namespace
{

using orange::overlay::FramePacer;
using orange::overlay::HudTracker;

/* ---- FramePacer ---- */

int test_pacer_waits_for_the_scene_then_works_every_other_frame()
{
    FramePacer p;
    for (int i = 0; i < FramePacer::kInitDelayFrames; ++i)
    {
        TEST_ASSERT_TRUE(!p.tick(true));
    }
    TEST_ASSERT_TRUE(p.tick(true));  /* first working frame */
    TEST_ASSERT_TRUE(!p.tick(true)); /* update interval of 2 */
    TEST_ASSERT_TRUE(p.tick(true));
    TEST_ASSERT_TRUE(!p.tick(true));
    TEST_ASSERT_TRUE(p.tick(true));
    return 0;
}

int test_pacer_does_not_count_before_the_hooks_are_ready()
{
    FramePacer p;
    for (int i = 0; i < 500; ++i)
    {
        TEST_ASSERT_TRUE(!p.tick(false));
    }
    /* The full settle delay still applies once they are. */
    for (int i = 0; i < FramePacer::kInitDelayFrames; ++i)
    {
        TEST_ASSERT_TRUE(!p.tick(true));
    }
    TEST_ASSERT_TRUE(p.tick(true));
    return 0;
}

int test_pacer_pauses_while_the_hooks_are_not_ready()
{
    FramePacer p;
    for (int i = 0; i < 10; ++i)
    {
        p.tick(true);
    }
    for (int i = 0; i < 100; ++i)
    {
        TEST_ASSERT_TRUE(!p.tick(false));
    }
    /* 20 frames of the delay were left. */
    for (int i = 0; i < FramePacer::kInitDelayFrames - 10; ++i)
    {
        TEST_ASSERT_TRUE(!p.tick(true));
    }
    TEST_ASSERT_TRUE(p.tick(true));
    return 0;
}

/* ---- HudTracker ---- */

int test_first_frame_builds_and_pushes_the_texts()
{
    HudTracker t;
    TEST_ASSERT_TRUE(!t.built());
    TEST_ASSERT_TRUE(t.needs_build(5));
    t.built_ok(5);
    TEST_ASSERT_TRUE(t.built());
    TEST_ASSERT_TRUE(t.needs_update(false)); /* just built: paint even if the texts did not change */
    return 0;
}

int test_steady_frames_only_update_when_the_texts_change()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);
    t.needs_update(false);

    TEST_ASSERT_TRUE(!t.needs_build(5));
    TEST_ASSERT_TRUE(!t.needs_update(false));
    TEST_ASSERT_TRUE(!t.needs_build(5));
    TEST_ASSERT_TRUE(t.needs_update(true));
    return 0;
}

int test_a_new_config_generation_rebuilds_and_repaints()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);

    TEST_ASSERT_TRUE(t.needs_build(6));
    t.built_ok(6);
    TEST_ASSERT_TRUE(t.needs_update(false));
    TEST_ASSERT_TRUE(!t.needs_build(6));
    TEST_ASSERT_TRUE(!t.needs_update(false)); /* the repaint was a one-off */
    return 0;
}

int test_a_failed_first_build_is_retried_and_paints_nothing()
{
    HudTracker t;
    TEST_ASSERT_TRUE(t.needs_build(5));
    /* the scene was not ready: no built_ok() */
    TEST_ASSERT_TRUE(!t.built());
    TEST_ASSERT_TRUE(!t.needs_update(true));

    TEST_ASSERT_TRUE(t.needs_build(5)); /* tried again on the next working frame */
    t.built_ok(5);
    TEST_ASSERT_TRUE(t.needs_update(false));
    return 0;
}

int test_a_failed_rebuild_keeps_updating_the_old_bar()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);

    TEST_ASSERT_TRUE(t.needs_build(6)); /* rebuild fails */
    TEST_ASSERT_TRUE(t.built());
    TEST_ASSERT_TRUE(!t.needs_update(false));
    TEST_ASSERT_TRUE(t.needs_update(true));
    TEST_ASSERT_TRUE(t.needs_build(6)); /* and is tried again */
    return 0;
}

int test_the_bar_is_removed_after_a_second_without_frames()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);

    for (int i = 0; i < HudTracker::kStaleReadsBeforeHide - 1; ++i)
    {
        TEST_ASSERT_TRUE(!t.on_stale());
    }
    TEST_ASSERT_TRUE(t.on_stale()); /* the 15th */
    TEST_ASSERT_TRUE(!t.built());
    for (int i = 0; i < 50; ++i)
    {
        TEST_ASSERT_TRUE(!t.on_stale()); /* removed once */
    }
    return 0;
}

int test_a_readable_frame_restarts_the_stale_count()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);

    for (int i = 0; i < HudTracker::kStaleReadsBeforeHide - 1; ++i)
    {
        t.on_stale();
    }
    t.needs_build(5); /* the daemon is back */
    for (int i = 0; i < HudTracker::kStaleReadsBeforeHide - 1; ++i)
    {
        TEST_ASSERT_TRUE(!t.on_stale());
    }
    TEST_ASSERT_TRUE(t.on_stale());
    return 0;
}

int test_nothing_to_remove_when_nothing_was_built()
{
    HudTracker t;
    for (int i = 0; i < 100; ++i)
    {
        TEST_ASSERT_TRUE(!t.on_stale());
    }
    return 0;
}

int test_the_bar_is_rebuilt_after_it_was_removed()
{
    HudTracker t;
    t.needs_build(5);
    t.built_ok(5);
    for (int i = 0; i < HudTracker::kStaleReadsBeforeHide; ++i)
    {
        t.on_stale();
    }
    TEST_ASSERT_TRUE(t.needs_build(5)); /* same generation, but the panel is gone */
    t.built_ok(5);
    TEST_ASSERT_TRUE(t.needs_update(false));
    return 0;
}

} // namespace

extern "C" int test_frame_driver_suite(void)
{
    int failures = 0;
    failures += test_run("frame_driver.pacer", test_pacer_waits_for_the_scene_then_works_every_other_frame);
    failures += test_run("frame_driver.pacer_not_ready", test_pacer_does_not_count_before_the_hooks_are_ready);
    failures += test_run("frame_driver.pacer_pause", test_pacer_pauses_while_the_hooks_are_not_ready);
    failures += test_run("frame_driver.first_frame", test_first_frame_builds_and_pushes_the_texts);
    failures += test_run("frame_driver.steady", test_steady_frames_only_update_when_the_texts_change);
    failures += test_run("frame_driver.new_generation", test_a_new_config_generation_rebuilds_and_repaints);
    failures += test_run("frame_driver.failed_build", test_a_failed_first_build_is_retried_and_paints_nothing);
    failures += test_run("frame_driver.failed_rebuild", test_a_failed_rebuild_keeps_updating_the_old_bar);
    failures += test_run("frame_driver.stale_hide", test_the_bar_is_removed_after_a_second_without_frames);
    failures += test_run("frame_driver.stale_reset", test_a_readable_frame_restarts_the_stale_count);
    failures += test_run("frame_driver.stale_nothing", test_nothing_to_remove_when_nothing_was_built);
    failures += test_run("frame_driver.rebuild_after_hide", test_the_bar_is_rebuilt_after_it_was_removed);
    return failures;
}
