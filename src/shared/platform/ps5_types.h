/* ABI structure of the PS5 notification service, shared by the daemon, the
 * ShellUI module and the platform library. Layout mirrors the system library.
 */
#pragma once

// Standard library
#include <stdint.h>

/** sceKernelSendNotificationRequest payload (total size 0xC30). */
typedef struct
{
    int32_t type;            /* 0x00 */
    int32_t req_id;          /* 0x04 */
    int32_t priority;        /* 0x08 */
    int32_t msg_id;          /* 0x0C */
    int32_t target_id;       /* 0x10 */
    int32_t user_id;         /* 0x14 */
    int32_t unk1;            /* 0x18 */
    int32_t unk2;            /* 0x1C */
    int32_t app_id;          /* 0x20 */
    int32_t error_num;       /* 0x24 */
    int32_t unk3;            /* 0x28 */
    char use_icon_image_uri; /* 0x2C */
    char message[1024];      /* 0x2D */
    char uri[1024];          /* 0x42D */
    char unkstr[1024];       /* 0x82D */
    char _pad_to_c30[3];     /* 0xC2D -> 0xC30 */
} OrbisNotificationRequest;

#ifdef __cplusplus
static_assert(sizeof(OrbisNotificationRequest) == 0xC30, "OrbisNotificationRequest must be 0xC30");
#else
_Static_assert(sizeof(OrbisNotificationRequest) == 0xC30, "OrbisNotificationRequest must be 0xC30");
#endif
