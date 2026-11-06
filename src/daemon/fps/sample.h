/* One FPS reading as produced by the daemon samplers. It stays inside the
 * daemon; the HUD publisher folds it into the OrangeHudFrame ShellUI renders.
 */
#pragma once

// Standard library
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ORANGE_FPS_STALE_NS 2000000000ull /* 2 s */

    enum OrangeFpsSource
    {
        ORANGE_FPS_SRC_NONE = 0,
        ORANGE_FPS_SRC_FLIP = 1u << 0 /* display flip times polled from /dev/dce */
    };

    struct OrangeFpsSample
    {
        int32_t pid;
        uint8_t valid;
        uint8_t source; /* OrangeFpsSource bits */
        float fps;
        float low1;  /* average of the slowest 1% of the last minute, -1 if unknown */
        float low01; /* same for the slowest 0.1% */
        uint64_t unix_ns;
        char title_id[16];
        /** Per-frame times ms*10 for the HUD graph (newest last, 0 = unknown). */
        uint16_t ft10[96];
    };

#ifdef __cplusplus
}
#endif
