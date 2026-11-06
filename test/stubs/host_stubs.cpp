/*
 * Host stubs for util unit tests (no PS5 SDK).
 *
 * Supplies the PS5-runtime-only symbols (klog, notification send)
 * that the production objects under test link against.
 */

// Standard library
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

// Project
#include "platform/notify.h"

/* The runtime symbols are plain C functions in the real system libraries. */
extern "C"
{

    /* PS5 klog sink — silence unless verbose. */
    void klog_printf(const char* fmt, ...)
    {
        va_list args;
        if (std::getenv("ORANGE_TEST_VERBOSE") == nullptr)
        {
            return;
        }
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }

    /* Notification hardware — no-op on host. */
    int32_t sceKernelSendNotificationRequest(int32_t device, void* req, size_t size, int32_t blocking)
    {
        (void)device;
        (void)req;
        (void)size;
        (void)blocking;
        return 0;
    }
}

/* Bind platform notify to the host stub (constructor runs before tests). */
__attribute__((constructor)) static void host_bind_notify_send(void)
{
    notify_set_send(sceKernelSendNotificationRequest);
}
