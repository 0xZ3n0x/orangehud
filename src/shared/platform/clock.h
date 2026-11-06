/* Monotonic (CLOCK_MONOTONIC) and wall-clock (CLOCK_REALTIME) time in nanoseconds. */
#pragma once

// Standard library
#include <stdint.h>
#include <time.h>

static inline uint64_t monotonic_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/** Wall-clock time: comparable between processes (stamps published for another process to age). */
static inline uint64_t realtime_ns(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
