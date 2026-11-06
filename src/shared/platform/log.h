/* OrangeHUD logging: klog only.
 *
 *   LOG(tag, subtag, fmt, ...)   "[tag][subtag] message\n"
 *   LOGE(tag, subtag, msg)       "[tag][subtag] ERROR: msg: strerror(errno)\n"
 *
 * Everything is compiled out unless LOGGING is defined (CMake option
 * ORANGEHUD_LOGGING, off by default). A disabled LOG() still type-checks its
 * format string and arguments, then is removed as dead code.
 *
 * LOG_ERROR / LOG_WARN / LOG_INFO / LOG_DEBUG are the same thing with tag
 * "OrangeHUD" and the level as subtag.
 */

#pragma once

// Standard library
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int klog_printf(const char* fmt, ...);

#ifdef __cplusplus
}
#endif

#define ORANGE_LOG_LINE_MAX 512

static inline void klog_write(const char* tag, const char* subtag, const char* fmt, ...) __attribute__((format(printf, 3, 4), unused));

static inline void klog_write(const char* tag, const char* subtag, const char* fmt, ...)
{
    char msg[ORANGE_LOG_LINE_MAX];
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    (void)klog_printf("[%s][%s] %s\n", tag, subtag, msg);
}

#ifdef LOGGING
#define LOG(tag, subtag, ...) klog_write((tag), (subtag), __VA_ARGS__)
#define LOGE(tag, subtag, msg) (void)klog_printf("[%s][%s] ERROR: %s: %s\n", (tag), (subtag), (msg), strerror(errno))
#else
#define LOG(tag, subtag, ...)                                                                                                                                                                          \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        if (0)                                                                                                                                                                                         \
            klog_write((tag), (subtag), __VA_ARGS__);                                                                                                                                                  \
    } while (0)
#define LOGE(tag, subtag, msg)                                                                                                                                                                         \
    do                                                                                                                                                                                                 \
    {                                                                                                                                                                                                  \
        if (0)                                                                                                                                                                                         \
            (void)klog_printf("%s%s%s", (tag), (subtag), (msg));                                                                                                                                       \
    } while (0)
#endif

#define ORANGE_LOG_TAG "OrangeHUD"

#define LOG_ERROR(...) LOG(ORANGE_LOG_TAG, "ERROR", __VA_ARGS__)
#define LOG_WARN(...) LOG(ORANGE_LOG_TAG, "WARN", __VA_ARGS__)
#define LOG_INFO(...) LOG(ORANGE_LOG_TAG, "INFO", __VA_ARGS__)
#define LOG_DEBUG(...) LOG(ORANGE_LOG_TAG, "DEBUG", __VA_ARGS__)
