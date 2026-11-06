/* Host tests for the Installing -> Ready hook barrier. */
// Project
#include "boot/hook_lifecycle.hpp"
#include "test_harness.h"

static int test_hook_lifecycle_barrier()
{
    shellui_hooks_publish_failed();
    TEST_ASSERT_TRUE(!shellui_hooks_are_ready());
    shellui_hooks_begin_install();
    TEST_ASSERT_TRUE(!shellui_hooks_are_ready());
    shellui_hooks_publish_ready();
    TEST_ASSERT_TRUE(shellui_hooks_are_ready());
    shellui_hooks_publish_failed();
    TEST_ASSERT_TRUE(!shellui_hooks_are_ready());
    return 0;
}

extern "C" int test_hook_lifecycle_suite(void)
{
    return test_run("hook_lifecycle.barrier", test_hook_lifecycle_barrier);
}
