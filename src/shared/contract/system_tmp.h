/* Shared /system_tmp namespace for OrangeHUD runtime state.
 */
#pragma once

#if defined(ORANGE_HOST_TEST)
#define ORANGE_SYSTEM_TMP_ROOT "/tmp/orangehud"
#else
#define ORANGE_SYSTEM_TMP_ROOT "/system_tmp/orangehud"
#endif

#define ORANGE_SYSTEM_TMP_READY_ROOT ORANGE_SYSTEM_TMP_ROOT "/ready"
#define ORANGE_SYSTEM_TMP_HUD_FRAME ORANGE_SYSTEM_TMP_ROOT "/hud_frame"
