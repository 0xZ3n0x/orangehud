#include "notify.h"

// Standard library
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

// Project
#include "log.h"
#include "ps5_types.h"

static notify_send_fn g_send = NULL;

void notify_set_send(notify_send_fn fn)
{
    g_send = fn;
}

void notify_format(char* out, size_t out_sz, int show_watermark, const char* fmt, va_list ap)
{
    char buff[3075];

    vsnprintf(buff, sizeof(buff), fmt, ap);
    if (show_watermark)
    {
        snprintf(out, out_sz, "[OrangeHUD] %s", buff);
    }
    else
    {
        snprintf(out, out_sz, "%s", buff);
    }
}

static void notify_send_request(OrbisNotificationRequest* req)
{
    if (!req)
    {
        return;
    }

    req->type = 0;
    req->unk3 = 0;
    req->target_id = -1;
    req->use_icon_image_uri = 1;
    strncpy(req->uri, "cxml://psnotification/tex_icon_system", sizeof(req->uri) - 1);

    LOG_DEBUG("Notify: %s", req->message);

    if (!g_send)
    {
        /* Never fall back to a direct CALL of sceKernelSendNotificationRequest —
         * that symbol is a data pointer in shellui/fps injectees. */
        LOG_ERROR("Notify: send fn not registered (notify_set_send)");
        return;
    }
    (void)g_send(0, req, sizeof(*req), 0);
}

void notify_v(int show_watermark, const char* fmt, va_list ap)
{
    OrbisNotificationRequest req;
    memset(&req, 0, sizeof(req));
    notify_format(req.message, sizeof(req.message), show_watermark, fmt, ap);
    notify_send_request(&req);
}

void notify_user(int show_watermark, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    notify_v(show_watermark, fmt, args);
    va_end(args);
}
