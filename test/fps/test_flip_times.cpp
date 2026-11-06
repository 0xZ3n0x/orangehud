// Standard library
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// Project
#include "fps/stats/display_flips.hpp"
#include "fps/stats/frame_stats.hpp"
#include "test_harness.h"

namespace
{

using namespace orange::fps;

bool near(float a, float b, float eps = 0.05f)
{
    return std::fabs(a - b) <= eps;
}

constexpr uint64_t kSecond = 1000000000ULL;

/* ---- FlipFrameStats ---- */

constexpr uint64_t kFrameUs = 16667; /* ~60 fps */
constexpr uint64_t kGameStartUs = 1000000;
constexpr uint64_t kMono = 100 * kSecond;

std::vector<uint64_t> flips(uint64_t first_us, int count, uint64_t step_us = kFrameUs)
{
    std::vector<uint64_t> v;
    for (int i = 0; i < count; ++i)
    {
        v.push_back(first_us + static_cast<uint64_t>(i) * step_us);
    }
    return v;
}

int test_flip_stats_measure_the_rate_and_the_one_percent_low()
{
    FlipFrameStats s;
    /* 20 s of 60 fps: 5 s past the warm-up, ~300 frames in the window. */
    const FlipFrameStats::Reading r = s.update(flips(kGameStartUs, 1200), kMono);
    TEST_ASSERT_TRUE(r.frames);
    TEST_ASSERT_TRUE(near(60.f, r.fps, 0.5f));
    TEST_ASSERT_TRUE(near(60.f, r.low1, 0.5f));
    TEST_ASSERT_TRUE(near(-1.f, r.low01)); /* fewer than 1000 frames */
    return 0;
}

int test_flip_stats_zero_point_one_percent_needs_a_thousand_frames()
{
    FlipFrameStats s;
    /* 35 s: ~1200 frames after the 15 s warm-up. */
    const FlipFrameStats::Reading r = s.update(flips(kGameStartUs, 2100), kMono);
    TEST_ASSERT_TRUE(near(60.f, r.low01, 0.5f));
    return 0;
}

int test_flip_stats_warm_up_feeds_the_rate_but_not_the_lows()
{
    FlipFrameStats s;
    const FlipFrameStats::Reading r = s.update(flips(kGameStartUs, 600), kMono); /* 10 s */
    TEST_ASSERT_TRUE(r.frames);
    TEST_ASSERT_TRUE(near(60.f, r.fps, 0.5f));
    TEST_ASSERT_TRUE(near(-1.f, r.low1));
    return 0;
}

int test_flip_stats_report_no_frames_after_a_second_of_silence()
{
    FlipFrameStats s;
    s.update(flips(kGameStartUs, 1200), kMono);

    FlipFrameStats::Reading r = s.update({}, kMono + kSecond / 2);
    TEST_ASSERT_TRUE(r.frames); /* still within the second */
    TEST_ASSERT_TRUE(near(60.f, r.fps, 0.5f));

    r = s.update({}, kMono + kSecond + kSecond / 2);
    TEST_ASSERT_TRUE(!r.frames);
    TEST_ASSERT_TRUE(near(0.f, r.fps));
    TEST_ASSERT_TRUE(near(60.f, r.low1, 0.5f)); /* the lows are kept */
    return 0;
}

int test_flip_stats_refresh_the_lows_once_a_second()
{
    FlipFrameStats s;
    const std::vector<uint64_t> first = flips(kGameStartUs, 1200);
    FlipFrameStats::Reading r = s.update(first, kMono);
    TEST_ASSERT_TRUE(near(60.f, r.low1, 0.5f));

    /* 100 slow frames (100 ms) right after: not visible before a second passes. */
    const std::vector<uint64_t> slow = flips(first.back() + 100000, 100, 100000);
    r = s.update(slow, kMono + kSecond / 2);
    TEST_ASSERT_TRUE(near(60.f, r.low1, 0.5f));
    r = s.update({}, kMono + kSecond);
    TEST_ASSERT_TRUE(r.low1 < 30.f);
    return 0;
}

int test_flip_stats_reset_forgets_the_run()
{
    FlipFrameStats s;
    s.update(flips(kGameStartUs, 1200), kMono);
    s.reset();
    const FlipFrameStats::Reading r = s.update({}, kMono + 10 * kSecond);
    TEST_ASSERT_TRUE(!r.frames);
    TEST_ASSERT_TRUE(near(-1.f, r.low1));
    return 0;
}

/* ---- DisplayFlips ---- */

void fill_answer(unsigned char* raw, uint64_t count, uint64_t flip_us)
{
    std::memset(raw, 0, kDceAnswerBytes);
    std::memcpy(raw + 8, &count, sizeof(count));
    std::memcpy(raw + 24, &flip_us, sizeof(flip_us));
}

int test_display_flips_report_each_counter_step_once()
{
    DisplayFlips d;
    unsigned char raw[kDceAnswerBytes];
    uint64_t us = 0;

    fill_answer(raw, 100, 5000000);
    TEST_ASSERT_TRUE(d.feed(raw, &us)); /* the first read is a flip too */
    TEST_ASSERT_TRUE(us == 5000000);
    TEST_ASSERT_TRUE(!d.feed(raw, &us)); /* same flip read again */

    fill_answer(raw, 101, 5016667);
    TEST_ASSERT_TRUE(d.feed(raw, &us));
    TEST_ASSERT_TRUE(us == 5016667);
    return 0;
}

int test_display_flips_ignore_an_answer_without_a_time()
{
    DisplayFlips d;
    unsigned char raw[kDceAnswerBytes];
    uint64_t us = 0;
    fill_answer(raw, 7, 0);
    TEST_ASSERT_TRUE(!d.feed(raw, &us));
    return 0;
}

int test_display_flips_reset_forgets_the_counter()
{
    DisplayFlips d;
    unsigned char raw[kDceAnswerBytes];
    uint64_t us = 0;
    fill_answer(raw, 100, 5000000);
    d.feed(raw, &us);
    d.reset();
    TEST_ASSERT_TRUE(d.feed(raw, &us));
    return 0;
}

} // namespace

extern "C" int test_flip_times_suite(void)
{
    int failures = 0;
    failures += test_run("flip_times.flip_stats_rate", test_flip_stats_measure_the_rate_and_the_one_percent_low);
    failures += test_run("flip_times.flip_stats_low01", test_flip_stats_zero_point_one_percent_needs_a_thousand_frames);
    failures += test_run("flip_times.flip_stats_warmup", test_flip_stats_warm_up_feeds_the_rate_but_not_the_lows);
    failures += test_run("flip_times.flip_stats_silence", test_flip_stats_report_no_frames_after_a_second_of_silence);
    failures += test_run("flip_times.flip_stats_lows_cadence", test_flip_stats_refresh_the_lows_once_a_second);
    failures += test_run("flip_times.display_flips_steps", test_display_flips_report_each_counter_step_once);
    failures += test_run("flip_times.display_flips_no_time", test_display_flips_ignore_an_answer_without_a_time);
    failures += test_run("flip_times.display_flips_reset", test_display_flips_reset_forgets_the_counter);
    failures += test_run("flip_times.flip_stats_reset", test_flip_stats_reset_forgets_the_run);
    return failures;
}
