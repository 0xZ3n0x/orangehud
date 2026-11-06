#include "detour.h"

// Standard library
#include <cstdint>
#include <cstring>

// SDK
#include <machine/param.h>
#include <ps5/kernel.h>
#include <sys/mman.h>
#include <unistd.h>

// Project
#include "detour_c.h"
#include "hotpatch.h"
#include "platform/log.h"
#include "trampoline_arena.hpp"
#include "x64_relocator.h"

namespace
{

constexpr int kProtRwx = PROT_EXEC | PROT_READ | PROT_WRITE;
constexpr size_t kMaxX64InstructionLength = 15;
/*
 * hde64_disasm may inspect a complete 15-byte instruction when the final
 * instruction starts at HOOK_LENGTH - 1. PS5 xotext is execute-only, so every
 * page in that decoder window must be made readable before relocation starts.
 */
constexpr size_t kRelocationReadLength = HOOK_LENGTH + kMaxX64InstructionLength - 1;

struct alignas(16) AtomicPatchBlock
{
    uint64_t low;
    uint64_t high;
};

static_assert(sizeof(AtomicPatchBlock) == ORANGE_X64_ATOMIC_PATCH_SIZE);

uintptr_t page_align_down(uintptr_t addr)
{
    return addr & ~static_cast<uintptr_t>(PAGE_MASK);
}

/*
 * The host's sceKernelMprotect is a function *pointer* filled by dlsym, not a
 * code symbol the linker could bind to, so the host registers it explicitly
 * (detour_set_mprotect).
 */
mprotect_fn g_mprotect = nullptr;

int mprotect_user(void* addr, size_t len)
{
    if (g_mprotect == nullptr)
    {
        return -1;
    }
    return g_mprotect(addr, len, kProtRwx);
}

int mprotect_rwx(void* addr, size_t len)
{
    if (!addr || len == 0)
    {
        return -1;
    }
    /* Userland mprotect first (kstuff HV path), kernel fallback otherwise. */
    if (mprotect_user(addr, len) >= 0)
    {
        return 0;
    }
    return kernel_mprotect(getpid(), reinterpret_cast<uint64_t>(addr), len, kProtRwx);
}

/** Make the page(s) covering [addr, addr+len) RWX. */
int mprotect_range_rwx(uintptr_t addr, size_t len)
{
    const uintptr_t start = page_align_down(addr);
    const uintptr_t end = page_align_down(addr + len - 1) + PAGE_SIZE;
    return mprotect_rwx(reinterpret_cast<void*>(start), static_cast<size_t>(end - start));
}

bool protect_trampoline_arena(void* address, size_t length)
{
    return mprotect_rwx(address, length) == 0;
}

/* Process-lifetime storage; individual trampoline slots are never unmapped. */
orange::TrampolineArena g_trampoline_arena(protect_trampoline_arena);

bool compare_exchange_patch(uintptr_t address, const x64_atomic_patch& patch)
{
    if ((address % alignof(AtomicPatchBlock)) != 0)
    {
        return false;
    }

    AtomicPatchBlock expected{};
    AtomicPatchBlock desired{};
    std::memcpy(&expected, patch.expected, sizeof(expected));
    std::memcpy(&desired, patch.desired, sizeof(desired));

    unsigned char exchanged = 0;
    auto* target = reinterpret_cast<volatile AtomicPatchBlock*>(address);
    __asm__ volatile("lock; cmpxchg16b %1; sete %0" : "=q"(exchanged), "+m"(*target), "+a"(expected.low), "+d"(expected.high) : "b"(desired.low), "c"(desired.high) : "cc", "memory");
    return exchanged != 0;
}

} // namespace

void ReadMemory(uint64_t address, void* buffer, int length)
{
    memcpy(buffer, reinterpret_cast<void*>(address), length);
}

void WriteMemory(uint64_t address, void* buffer, int length)
{
    memcpy(reinterpret_cast<void*>(address), buffer, length);
}

bool PrepareDetour(uint64_t address, void* destination, DetourHandle* handle)
{
    if (!address || !destination || !handle)
    {
        return false;
    }

    *handle = DetourHandle{};
    LOG_DEBUG("Hooking %#02lx => %p", address, destination);

    if ((address % ORANGE_X64_ATOMIC_PATCH_SIZE) != 0)
    {
        LOG_DEBUG("PrepareDetour: target is not 16-byte aligned: %#02lx", address);
        return false;
    }

    const size_t stubLength = ORANGE_X64_TRAMPOLINE_CAPACITY;
    void* executableAddress = g_trampoline_arena.allocate_near(stubLength, address);
    if (!executableAddress)
    {
        LOG_DEBUG("DetourFunction: no unique near trampoline for %#02lx", address);
        return false;
    }
    if (!g_trampoline_arena.owns(executableAddress))
    {
        LOG_DEBUG("DetourFunction: allocator returned an unowned trampoline");
        return false;
    }

    /* PS5 xotext is execute-only. Never decode/copy it before this succeeds. */
    if (mprotect_range_rwx(address, kRelocationReadLength) < 0)
    {
        LOG_ERROR("DetourFunction: failed to mprotect target decoder window");
        return false;
    }

    x64_relocate_result relocation{};
    if (!x64_relocate(reinterpret_cast<const uint8_t*>(address), address, reinterpret_cast<uint8_t*>(executableAddress), reinterpret_cast<uintptr_t>(executableAddress), HOOK_LENGTH, stubLength,
                      &relocation))
    {
        LOG_ERROR("DetourFunction: relocation failed at +%zu: %s", relocation.error_offset, x64_relocate_error_string(relocation.error));
        return false;
    }

    x64_atomic_patch patch{};
    if (!x64_build_atomic_patch(address, reinterpret_cast<const uint8_t*>(address), reinterpret_cast<uintptr_t>(destination), &patch))
    {
        LOG_ERROR("PrepareDetour: failed to build atomic patch");
        return false;
    }

    handle->target = address;
    handle->destination = destination;
    handle->trampoline = executableAddress;
    handle->trampoline_capacity = stubLength;
    handle->stolen_size = relocation.source_size;
    handle->emitted_size = relocation.trampoline_size;
    handle->patch = patch;
    handle->prepared = true;
    return true;
}

bool CommitDetour(DetourHandle* handle)
{
    if (!handle || !handle->prepared || handle->committed)
    {
        return false;
    }

    if (!compare_exchange_patch(handle->target, handle->patch))
    {
        LOG_ERROR("CommitDetour: target changed or atomic patch failed: %#02lx", handle->target);
        return false;
    }

    handle->committed = true;

    LOG_DEBUG("DetourFunction: target=%#02lx hook=%p trampoline=%p stolen=%zu emitted=%zu", handle->target, handle->destination, handle->trampoline, handle->stolen_size, handle->emitted_size);
    return true;
}

void AbortDetour(DetourHandle* handle)
{
    if (!handle)
    {
        return;
    }
    /* Arena slots are process-lifetime allocations; never unmap one slot. */
    *handle = DetourHandle{};
}

bool InstallDetour(uint64_t address, void* destination, void** original_storage)
{
    DetourHandle handle{};
    if (!PrepareDetour(address, destination, &handle))
    {
        return false;
    }

    /* The locked CommitDetour operation publishes this earlier pointer write. */
    if (original_storage)
    {
        std::memcpy(original_storage, &handle.trampoline, sizeof(handle.trampoline));
    }

    if (!CommitDetour(&handle))
    {
        if (original_storage)
        {
            void* null_original = nullptr;
            std::memcpy(original_storage, &null_original, sizeof(null_original));
        }
        AbortDetour(&handle);
        return false;
    }
    return true;
}

void* DetourFunction(uint64_t address, void* destination)
{
    DetourHandle handle{};
    if (!PrepareDetour(address, destination, &handle))
    {
        return nullptr;
    }
    if (!CommitDetour(&handle))
    {
        AbortDetour(&handle);
        return nullptr;
    }
    return handle.trampoline;
}

extern "C" void detour_set_mprotect(mprotect_fn fn)
{
    g_mprotect = fn;
}

extern "C" bool install_detour(uint64_t address, void* destination, void** original_storage)
{
    return InstallDetour(address, destination, original_storage);
}
