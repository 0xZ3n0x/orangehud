#pragma once

// Standard library
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ORANGE_X64_ABSOLUTE_JUMP_SIZE 14u
#define ORANGE_X64_ATOMIC_PATCH_SIZE 16u

    struct x64_atomic_patch
    {
        uint8_t expected[ORANGE_X64_ATOMIC_PATCH_SIZE];
        uint8_t desired[ORANGE_X64_ATOMIC_PATCH_SIZE];
    };

    /*
     * Build an aligned 16-byte compare/exchange image for:
     *   jmp qword ptr [rip+0]; dq destination
     * Bytes 14..15 are preserved from the original function.
     */
    bool x64_build_atomic_patch(uintptr_t target_address, const uint8_t current[ORANGE_X64_ATOMIC_PATCH_SIZE], uintptr_t destination, struct x64_atomic_patch* patch);

#ifdef __cplusplus
}
#endif
