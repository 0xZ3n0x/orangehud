// Standard library
#include <cmath>

// Project
#include "fps/stats/frame_stats.hpp"
#include "test_harness.h"

using orange::fps::FlipRate;
using orange::fps::FrameWindow;

namespace
{

bool near(float a, float b, float eps = 0.01f)
{
    return std::fabs(a - b) <= eps;
}

int frame_stats_constant_rate()
{
    FrameWindow w;
    for (int i = 0; i < 500; ++i)
    {
        w.push(16.6667);
    }
    TEST_ASSERT_TRUE(near(w.low(0.01, 100), 60.f, 0.05f));
    TEST_ASSERT_TRUE(near(w.average_fps(), 60.f, 0.05f));
    return 0;
}

int frame_stats_mangohud_percentile()
{
    /* 100 frames: 99 at 10 ms, one at 50 ms. idx = 0.01 * 100 - 1 = 0 -> the
     * slowest frame, 1000 / 50 = 20 FPS. */
    FrameWindow w;
    for (int i = 0; i < 99; ++i)
    {
        w.push(10.0);
    }
    w.push(50.0);
    TEST_ASSERT_TRUE(near(w.low(0.01, 100), 20.f));
    /* Not enough frames for the 0.1% low. */
    TEST_ASSERT_TRUE(w.low(0.001, 1000) < 0.f);
    return 0;
}

int frame_stats_single_value_not_average()
{
    /* 200 frames: 190 at 10 ms, 9 at 40 ms, 1 at 100 ms. 1% -> idx 1 -> the
     * second slowest frame (40 ms = 25 FPS), not an average of the slowest 1%
     * (70 ms = 14 FPS). */
    FrameWindow w;
    for (int i = 0; i < 190; ++i)
    {
        w.push(10.0);
    }
    for (int i = 0; i < 9; ++i)
    {
        w.push(40.0);
    }
    w.push(100.0);
    TEST_ASSERT_TRUE(near(w.low(0.01, 100), 25.f));
    return 0;
}

int frame_stats_ignores_stalls_and_garbage()
{
    FrameWindow w(100, 500.0);
    TEST_ASSERT_TRUE(!w.push(0.0));
    TEST_ASSERT_TRUE(!w.push(-3.0));
    TEST_ASSERT_TRUE(!w.push(NAN));
    TEST_ASSERT_TRUE(!w.push(501.0));
    TEST_ASSERT_TRUE(w.push(500.0));
    TEST_ASSERT_TRUE(w.size() == 1u);
    return 0;
}

int frame_stats_window_keeps_newest()
{
    FrameWindow w(200);
    for (int i = 0; i < 1000; ++i)
    {
        w.push(100.0); /* old, slow */
    }
    for (int i = 0; i < 200; ++i)
    {
        w.push(10.0); /* newest fill the window */
    }
    TEST_ASSERT_TRUE(w.size() == 200u);
    TEST_ASSERT_TRUE(near(w.low(0.01, 100), 100.f, 0.05f));
    return 0;
}

int frame_stats_flip_rate()
{
    FlipRate r;
    for (int i = 0; i < 31; ++i)
    {
        r.push(1000000000ULL + static_cast<uint64_t>(i) * 33333333ULL);
    }
    float fps = 0.f;
    TEST_ASSERT_TRUE(r.fps(&fps));
    TEST_ASSERT_TRUE(near(fps, 30.f, 0.1f));
    /* Everything older than a second is dropped: no frames -> not valid. */
    r.trim(5000000000ULL);
    TEST_ASSERT_TRUE(!r.fps(&fps));
    return 0;
}

} // namespace

extern "C" int test_frame_stats_suite(void)
{
    int failures = 0;
    failures += test_run("frame_stats.constant_rate", frame_stats_constant_rate);
    failures += test_run("frame_stats.mangohud_percentile", frame_stats_mangohud_percentile);
    failures += test_run("frame_stats.single_value_not_average", frame_stats_single_value_not_average);
    failures += test_run("frame_stats.ignores_stalls_and_garbage", frame_stats_ignores_stalls_and_garbage);
    failures += test_run("frame_stats.window_keeps_newest", frame_stats_window_keeps_newest);
    failures += test_run("frame_stats.flip_rate", frame_stats_flip_rate);
    return failures;
}
