#pragma once

// Standard library
#include <cstddef>
#include <cstdint>

// Project
#include "detour_c.h"
#include "hotpatch.h"

#define HOOK_LENGTH 14

/*
 * Two-phase detour handle. Prepare builds the trampoline and immutable patch
 * plan; Commit is the only operation that mutates the target function.
 */
struct DetourHandle
{
    uint64_t target = 0;
    void* destination = nullptr;
    void* trampoline = nullptr;
    size_t trampoline_capacity = 0;
    size_t stolen_size = 0;
    size_t emitted_size = 0;
    x64_atomic_patch patch{};
    bool prepared = false;
    bool committed = false;
};

bool PrepareDetour(uint64_t address, void* destination, DetourHandle* handle);
bool CommitDetour(DetourHandle* handle);
void AbortDetour(DetourHandle* handle);
bool InstallDetour(uint64_t address, void* destination, void** original_storage);

/* Compatibility API for hooks that never call an original function. */
void* DetourFunction(uint64_t address, void* destination);
void WriteMemory(uint64_t address, void* buffer, int length);
