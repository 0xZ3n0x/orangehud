/* Host tests for notify_format (pure string path). */
// Standard library
#include <cstdarg>
#include <cstring>

// Project
#include "platform/notify.h"
#include "test_harness.h"

static void format_msg(char* out, size_t out_sz, int wm, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    notify_format(out, out_sz, wm, fmt, ap);
    va_end(ap);
}

static int test_notify_format_prefix(void)
{
    char out[128];
    format_msg(out, sizeof(out), 1, "hello %s", "world");
    TEST_ASSERT_STREQ("[OrangeHUD] hello world", out);
    return 0;
}

static int test_notify_format_truncates(void)
{
    char out[16];
    format_msg(out, sizeof(out), 1, "0123456789ABCDEFGHIJ");
    /* must be NUL-terminated and start with watermark prefix */
    TEST_ASSERT_TRUE(out[sizeof(out) - 1] == '\0' || strlen(out) < sizeof(out));
    TEST_ASSERT_TRUE(strncmp(out, "[OrangeHUD]", 10) == 0);
    return 0;
}

static int test_notify_format_no_watermark(void)
{
    char out[128];
    format_msg(out, sizeof(out), 0, "hello %s", "world");
    TEST_ASSERT_STREQ("hello world", out);
    return 0;
}

static int test_notify_send_noop(void)
{
    /* hits sceKernelSendNotificationRequest stub — must not crash */
    notify_user(1, "host test notify %d", 7);
    return 0;
}

extern "C" int test_platform_notify_suite(void)
{
    int failures = 0;
    failures += test_run("notify_format_prefix", test_notify_format_prefix);
    failures += test_run("notify_format_truncates", test_notify_format_truncates);
    failures += test_run("notify_format_no_watermark", test_notify_format_no_watermark);
    failures += test_run("notify_send_noop", test_notify_send_noop);
    return failures;
}
