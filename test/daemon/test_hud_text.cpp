// Standard library
#include <cmath>
#include <cstring>

// Project
#include "hud/text.hpp"
#include "test_harness.h"

namespace
{

using namespace orange::hud;

constexpr size_t kCap = 48; /* ORANGE_HUD_TEXT_LEN */

bool near(float a, float b)
{
    return std::fabs(a - b) < 0.001f;
}

int test_fps_value_precision()
{
    char v0[kCap], v1[kCap];
    format_fps(v0, v1, kCap, true, 59.94f, true, 50.f, 40.f);
    TEST_ASSERT_STREQ("59.9", v0);
    format_fps(v0, v1, kCap, true, 144.4f, true, 50.f, 40.f);
    TEST_ASSERT_STREQ("144", v0);
    return 0;
}

int test_fps_without_a_live_rate()
{
    char v0[kCap], v1[kCap];
    format_fps(v0, v1, kCap, false, 0.f, true, 50.f, 40.f);
    TEST_ASSERT_STREQ("--", v0);
    TEST_ASSERT_STREQ("1% --  0.1% --", v1);
    return 0;
}

int test_fps_lows()
{
    char v0[kCap], v1[kCap];
    format_fps(v0, v1, kCap, true, 60.f, true, 51.4f, 39.6f);
    TEST_ASSERT_STREQ("1% 51  0.1% 40", v1);

    /* Lows not available yet (negative) while the rate is live. */
    format_fps(v0, v1, kCap, true, 60.f, true, -1.f, 40.f);
    TEST_ASSERT_STREQ("1% --  0.1% --", v1);

    format_fps(v0, v1, kCap, true, 60.f, false, 51.f, 40.f);
    TEST_ASSERT_STREQ("", v1);
    return 0;
}

int test_cpu_temperature()
{
    char out[kCap];
    format_cpu_temp(out, kCap, 72);
    TEST_ASSERT_STREQ("72C", out);
    return 0;
}

int test_cpu_usage_average_and_unsampled()
{
    char out[kCap];
    const float usage[kCpuCores] = {10, 20, 30, 40, 50, 60, 70, 80};
    format_cpu_usage(out, kCap, true, false, usage);
    TEST_ASSERT_STREQ("45%", out);

    format_cpu_usage(out, kCap, false, false, usage);
    TEST_ASSERT_STREQ("--%", out);
    format_cpu_usage(out, kCap, false, true, usage);
    TEST_ASSERT_STREQ("--%", out);
    return 0;
}

int test_cpu_usage_per_core_is_padded()
{
    char out[kCap];
    const float usage[kCpuCores] = {5, 100, 0, 42, 7, 99, 1, 50};
    format_cpu_usage(out, kCap, true, true, usage);
    TEST_ASSERT_STREQ(" 5% 100%  0% 42%  7% 99%  1% 50%", out);
    return 0;
}

int test_core_usage_from_idle_time()
{
    TEST_ASSERT_TRUE(near(25.0f, core_usage_percent(0.75f, 1.0f)));
    /* Both deltas measured backwards, as the sampler does. */
    TEST_ASSERT_TRUE(near(25.0f, core_usage_percent(-0.75f, -1.0f)));
    TEST_ASSERT_TRUE(near(0.0f, core_usage_percent(1.5f, 1.0f)));    /* idle over 100%: clamp */
    TEST_ASSERT_TRUE(near(100.0f, core_usage_percent(-0.2f, 1.0f))); /* negative idle: clamp */
    return 0;
}

int test_memory_text()
{
    char out[kCap];
    format_memory(out, kCap, 8192, 12288.0);
    TEST_ASSERT_STREQ("8.0/12.0 GB", out);
    format_memory(out, kCap, 1536, 12288.0);
    TEST_ASSERT_STREQ("1.5/12.0 GB", out);
    return 0;
}

int test_memory_without_a_game_or_budget()
{
    char out[kCap];
    format_memory(out, kCap, -1, 12288.0);
    TEST_ASSERT_STREQ("--", out);
    format_memory(out, kCap, 8192, 0.0);
    TEST_ASSERT_STREQ("--", out);
    return 0;
}

int test_soc_temperature()
{
    char out[kCap];
    format_soc_temp(out, kCap, 71);
    TEST_ASSERT_STREQ("71C", out);
    format_soc_temp(out, kCap, 0);
    TEST_ASSERT_STREQ("0C", out);
    format_soc_temp(out, kCap, -1);
    TEST_ASSERT_STREQ("--C", out);
    return 0;
}

int test_soc_power_rounds_to_whole_watts()
{
    char out[kCap];
    format_soc_power(out, kCap, 0, 100000);
    TEST_ASSERT_STREQ("100W", out);
    format_soc_power(out, kCap, 0, 6400);
    TEST_ASSERT_STREQ("6W", out);
    format_soc_power(out, kCap, 0, 6500);
    TEST_ASSERT_STREQ("7W", out);
    return 0;
}

int test_soc_power_unavailable()
{
    char out[kCap];
    format_soc_power(out, kCap, -1, 100000);
    TEST_ASSERT_STREQ("--W", out);
    format_soc_power(out, kCap, 0, 0);
    TEST_ASSERT_STREQ("--W", out);
    return 0;
}

/* ---- VM map ---- */

constexpr size_t kStride = 64;

void put_entry(unsigned char* buf, size_t index, int type, uint64_t start, uint64_t end)
{
    VmEntry e{};
    e.structsize = static_cast<int>(kStride);
    e.type = type;
    e.start = start;
    e.end = end;
    std::memcpy(buf + index * kStride, &e, sizeof(e));
}

int test_vm_map_sums_device_memory_only()
{
    alignas(8) unsigned char buf[4 * kStride] = {};
    constexpr uint64_t kMb = 1024 * 1024;
    put_entry(buf, 0, kVmEntryTypeDevice, 0x100000000, 0x100000000 + 2048 * kMb);
    put_entry(buf, 1, 1 /* not device */, 0x200000000, 0x200000000 + 999 * kMb);
    put_entry(buf, 2, kVmEntryTypeDevice, 0x300000000, 0x300000000 + 512 * kMb);
    /* entry 3 stays zero: structsize 0 ends the list */
    TEST_ASSERT_EQ_INT(2560, mapped_device_mb(reinterpret_cast<const char*>(buf), sizeof(buf)));
    return 0;
}

int test_vm_map_empty_or_truncated()
{
    alignas(8) unsigned char buf[2 * kStride] = {};
    TEST_ASSERT_EQ_INT(0, mapped_device_mb(reinterpret_cast<const char*>(buf), 0));
    TEST_ASSERT_EQ_INT(0, mapped_device_mb(reinterpret_cast<const char*>(buf), sizeof(buf)));

    put_entry(buf, 0, kVmEntryTypeDevice, 0, 1024ull * 1024 * 1024);
    /* A buffer too short for the second record is not read past its end. */
    TEST_ASSERT_EQ_INT(1024, mapped_device_mb(reinterpret_cast<const char*>(buf), kStride + sizeof(VmEntry) - 1));
    return 0;
}

} // namespace

extern "C" int test_hud_text_suite(void)
{
    int failures = 0;
    failures += test_run("hud_text.fps_precision", test_fps_value_precision);
    failures += test_run("hud_text.fps_not_live", test_fps_without_a_live_rate);
    failures += test_run("hud_text.fps_lows", test_fps_lows);
    failures += test_run("hud_text.cpu_temp", test_cpu_temperature);
    failures += test_run("hud_text.cpu_usage", test_cpu_usage_average_and_unsampled);
    failures += test_run("hud_text.cpu_per_core", test_cpu_usage_per_core_is_padded);
    failures += test_run("hud_text.core_usage", test_core_usage_from_idle_time);
    failures += test_run("hud_text.memory", test_memory_text);
    failures += test_run("hud_text.memory_unavailable", test_memory_without_a_game_or_budget);
    failures += test_run("hud_text.soc_temp", test_soc_temperature);
    failures += test_run("hud_text.soc_power", test_soc_power_rounds_to_whole_watts);
    failures += test_run("hud_text.soc_power_unavailable", test_soc_power_unavailable);
    failures += test_run("hud_text.vm_map", test_vm_map_sums_device_memory_only);
    failures += test_run("hud_text.vm_map_edges", test_vm_map_empty_or_truncated);
    return failures;
}
