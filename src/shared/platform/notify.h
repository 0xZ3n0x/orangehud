/* Shared system notification toast.
 *
 * Use notify_user(show_wm, fmt, ...). ShellUI wraps it in its own
 * notify(const char*, ...) (shellui/state.cpp).
 *
 * IMPORTANT (ShellUI injectees):
 *   Do NOT call sceKernelSendNotificationRequest by name from this library.
 *   Injected payloads resolve that symbol as a *function pointer* via dlsym;
 *   a direct CALL would execute the pointer's storage as code (SIGSEGV/SIGILL).
 *   Hosts must register the real sender with notify_set_send().
 */

#pragma once

// Standard library
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** Kernel/userland toast send — matches sceKernelSendNotificationRequest. */
    typedef int32_t (*notify_send_fn)(int32_t device, void* req, size_t size, int32_t blocking);

    /**
     * Install the send implementation for this process.
     * - daemon: pass the real linked sceKernelSendNotificationRequest
     * - shellui: pass a trampoline that calls through the dlsym'd pointer
     * Must be called before any notify_user / notify().
     */
    void notify_set_send(notify_send_fn fn);

    /** Format the toast text (internal; declared for the host tests). */
    void notify_format(char* out, size_t out_sz, int show_watermark, const char* fmt, va_list ap);

    void notify_v(int show_watermark, const char* fmt, va_list ap);
    void notify_user(int show_watermark, const char* fmt, ...);

#ifdef __cplusplus
} // extern "C"
#endif
