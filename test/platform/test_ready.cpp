/* Host tests for the PID-bound ready markers (orange::ReadyMarker). */
// Project
#include "platform/ready.hpp"
#include "test_harness.h"

using orange::ReadyMarker;

static int test_process_instance_marker(void)
{
    const ReadyMarker marker("host_pid_marker");
    const pid_t expected = 4242;
    pid_t actual = -1;

    marker.clear();
    TEST_ASSERT_TRUE(!marker.read(&actual));
    TEST_ASSERT_TRUE(marker.signal(expected));
    TEST_ASSERT_TRUE(marker.read(&actual));
    TEST_ASSERT_EQ_INT(expected, actual);
    TEST_ASSERT_TRUE(marker.matches(expected));
    TEST_ASSERT_TRUE(!marker.matches(4243));
    TEST_ASSERT_TRUE(marker.wait(expected, 100, 50));
    TEST_ASSERT_TRUE(!marker.wait(4243, 100, 50));
    TEST_ASSERT_TRUE(marker.clear());
    TEST_ASSERT_TRUE(!marker.matches(expected));
    return 0;
}

static int test_ready_rejects_bad_names_and_pids(void)
{
    pid_t actual = -1;
    TEST_ASSERT_TRUE(!ReadyMarker("../evil").signal(1234));
    TEST_ASSERT_TRUE(!ReadyMarker("a/b").signal(1234));
    TEST_ASSERT_TRUE(!ReadyMarker("dot.name").signal(1234));
    TEST_ASSERT_TRUE(!ReadyMarker("").signal(1234));
    TEST_ASSERT_TRUE(!ReadyMarker(nullptr).signal(1234));
    TEST_ASSERT_TRUE(!ReadyMarker("host_zero_pid").signal(0));
    TEST_ASSERT_TRUE(!ReadyMarker("host_zero_pid").read(nullptr));
    TEST_ASSERT_TRUE(!ReadyMarker("../evil").read(&actual));
    TEST_ASSERT_TRUE(!ReadyMarker("../evil").clear());
    return 0;
}

static int test_ready_wait_times_out(void)
{
    const ReadyMarker marker("host_never_set");
    marker.clear();
    TEST_ASSERT_TRUE(!marker.wait(99, 100, 50));
    return 0;
}

static int test_hud_marker_roundtrip(void)
{
    orange::kHudReady.clear();
    TEST_ASSERT_TRUE(!orange::kHudReady.matches(77));
    TEST_ASSERT_TRUE(orange::kHudReady.signal(77));
    TEST_ASSERT_TRUE(orange::kHudReady.matches(77));
    orange::kHudReady.clear();
    return 0;
}

static int test_marker_version(void)
{
    const ReadyMarker marker("host_version_marker");
    pid_t pid = -1;
    unsigned version = 99;

    marker.clear();
    /* No version published: reads back as 0 (a module of an older build). */
    TEST_ASSERT_TRUE(marker.signal(500));
    TEST_ASSERT_TRUE(marker.read(&pid, &version));
    TEST_ASSERT_EQ_INT(500, pid);
    TEST_ASSERT_EQ_INT(0, static_cast<int>(version));

    TEST_ASSERT_TRUE(marker.signal(501, 7));
    TEST_ASSERT_TRUE(marker.read(&pid, &version));
    TEST_ASSERT_EQ_INT(501, pid);
    TEST_ASSERT_EQ_INT(7, static_cast<int>(version));
    TEST_ASSERT_TRUE(marker.matches(501)); /* PID match ignores the version */
    marker.clear();
    return 0;
}

extern "C" int test_ready_suite(void)
{
    int failures = 0;
    failures += test_run("ready_process_instance", test_process_instance_marker);
    failures += test_run("ready_rejects_bad_input", test_ready_rejects_bad_names_and_pids);
    failures += test_run("ready_wait_timeout", test_ready_wait_times_out);
    failures += test_run("ready_hud_marker", test_hud_marker_roundtrip);
    failures += test_run("ready_marker_version", test_marker_version);
    return failures;
}
