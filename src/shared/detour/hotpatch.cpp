#include "hotpatch.h"

// Standard library
#include <cstring>

bool x64_build_atomic_patch(uintptr_t target_address, const uint8_t current[ORANGE_X64_ATOMIC_PATCH_SIZE], uintptr_t destination, struct x64_atomic_patch* patch)
{
    if (!current || !destination || !patch || (target_address % ORANGE_X64_ATOMIC_PATCH_SIZE) != 0)
    {
        return false;
    }

    std::memcpy(patch->expected, current, ORANGE_X64_ATOMIC_PATCH_SIZE);
    std::memcpy(patch->desired, current, ORANGE_X64_ATOMIC_PATCH_SIZE);

    static constexpr uint8_t prefix[] = {0xff, 0x25, 0, 0, 0, 0};
    std::memcpy(patch->desired, prefix, sizeof(prefix));
    const uint64_t hook_address = destination;
    std::memcpy(patch->desired + sizeof(prefix), &hook_address, sizeof(hook_address));
    return true;
}
