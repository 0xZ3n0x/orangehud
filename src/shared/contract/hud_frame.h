/* One seqlock record: the daemon measures everything and publishes ready-made
 * HUD text plus the overlay configuration; the ShellUI module only renders it.
 * Header-only so ShellUI links no sampling or config code.
 */
#pragma once

// Standard library
#include <stdint.h>
#include <string.h>

// SDK
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

// Project
#include "overlay_defs.hpp"
#include "platform/clock.h"
#include "system_tmp.h"

/*
 * ABI of the protocol between the daemon and the HUD module that runs inside
 * SceShellUI, bumped by hand. The module publishes it in its ready marker; a
 * daemon that finds a different value cannot replace that module (hooking the
 * same method twice fails) and asks for a console restart.
 *
 * Bump it when a running module could no longer understand a new daemon, or
 * the other way round: the OrangeHudFrame layout or the meaning of a field,
 * the seqlock, the /system_tmp paths, the ready-marker semantics.
 * test_hud_frame locks the layout, so a layout change fails there first.
 *
 * Do NOT bump it for daemon-only or library changes, or for module changes
 * that keep the protocol: the running module simply keeps its old code until
 * SceShellUI restarts (so a change to the module itself needs a restart to
 * show up).
 */
#define ORANGE_HUD_ABI 1u

#define ORANGE_HUD_MAGIC 0x4F485544u /* 'OHUD' */
#define ORANGE_HUD_FRAME_BYTES 640
#define ORANGE_HUD_FT_N 96                /* per-frame graph history length */
#define ORANGE_HUD_STALE_NS 2000000000ull /* 2 s; the daemon publishes every 100 ms */
#define ORANGE_HUD_TEXT_LEN 48

struct OrangeHudFrame
{
    uint32_t magic;
    uint32_t seq;
    uint64_t unix_ns;
    /** Bumped by the daemon whenever any config field below changes. */
    uint32_t config_gen;

    /* Overlay configuration (what the ShellUI used to read from config.ini). */
    uint8_t enabled;
    uint8_t background;
    uint8_t pos;       /* unused, always 0; kept for the frame layout */
    uint8_t align;     /* unused, always 0; kept for the frame layout */
    uint8_t font_size; /* unused, always 0; kept for the frame layout */
    uint8_t per_core;  /* per-core CPU usage text */
    uint8_t show[orange::kOverlayMetricCount];
    uint8_t order[orange::kOverlayMetricCount];

    /** value[metric][0|1]; an empty first value hides the metric. */
    char value[orange::kOverlayMetricCount][2][ORANGE_HUD_TEXT_LEN];

    /** Per-frame times for the FPS graph: milliseconds x10, newest last, 0 = unknown. */
    uint16_t ft10[ORANGE_HUD_FT_N];
};

static_assert(sizeof(OrangeHudFrame) <= ORANGE_HUD_FRAME_BYTES, "OrangeHudFrame must fit the mapped record");

/** Consistent copy of the record; 0 on success. */
static inline int orange_hud_seqlock_load(const volatile OrangeHudFrame* src, OrangeHudFrame* dst)
{
    for (int i = 0; i < 8; ++i)
    {
        const uint32_t s1 = __atomic_load_n(&src->seq, __ATOMIC_ACQUIRE);
        if (s1 & 1u)
        {
            continue;
        }
        memcpy(dst, (const void*)src, sizeof(*dst));
        const uint32_t s2 = __atomic_load_n(&src->seq, __ATOMIC_ACQUIRE);
        if (s1 == s2 && (s2 & 1u) == 0)
        {
            return 0;
        }
    }
    return -1;
}

/** Publish `frame` into the mapped record (single writer). */
static inline void orange_hud_seqlock_store(volatile OrangeHudFrame* dst, const OrangeHudFrame* frame)
{
    const uint32_t seq = __atomic_load_n(&dst->seq, __ATOMIC_RELAXED);
    __atomic_store_n(&dst->seq, seq + 1u, __ATOMIC_RELEASE);
    OrangeHudFrame tmp = *frame;
    tmp.magic = ORANGE_HUD_MAGIC;
    tmp.seq = seq + 2u;
    memcpy((void*)dst, &tmp, sizeof(tmp));
    __atomic_store_n(&dst->seq, seq + 2u, __ATOMIC_RELEASE);
}

/** Reader side (ShellUI): newest fresh frame, or -1 if none/stale. */
static inline int orange_hud_read(OrangeHudFrame* out)
{
    static OrangeHudFrame* map;
    if (!out)
    {
        return -1;
    }
    if (map == NULL)
    {
        const int fd = open(ORANGE_SYSTEM_TMP_HUD_FRAME, O_RDONLY);
        if (fd < 0)
        {
            return -1;
        }
        /* Mapping past EOF would SIGBUS; the daemon sizes the file right after
         * creating it. */
        struct stat st;
        if (fstat(fd, &st) != 0 || st.st_size < ORANGE_HUD_FRAME_BYTES)
        {
            close(fd);
            return -1;
        }
        void* p = mmap(NULL, ORANGE_HUD_FRAME_BYTES, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);
        if (p == MAP_FAILED)
        {
            return -1;
        }
        map = (OrangeHudFrame*)p;
    }
    OrangeHudFrame local;
    if (orange_hud_seqlock_load(map, &local) != 0 || local.magic != ORANGE_HUD_MAGIC)
    {
        return -1;
    }
    const uint64_t now = realtime_ns();
    if (local.unix_ns == 0 || now < local.unix_ns || now - local.unix_ns > ORANGE_HUD_STALE_NS)
    {
        return -1;
    }
    *out = local;
    return 0;
}
