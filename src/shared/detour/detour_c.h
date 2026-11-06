/* C interface of the detour library (the C++ API lives in detour.h). */
#pragma once

// Standard library
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef int (*mprotect_fn)(void* addr, size_t len, int prot);

    /**
     * The host's userland mprotect (sceKernelMprotect, resolved by dlsym), used
     * to make code pages writable. Call it before the first detour; without it
     * the library falls back to the kernel path only.
     */
    void detour_set_mprotect(mprotect_fn fn);

    /**
     * Detour `address` to `destination`; `*original_storage` receives a callable
     * trampoline to the original function. Same semantics as InstallDetour().
     */
    bool install_detour(uint64_t address, void* destination, void** original_storage);

#ifdef __cplusplus
} // extern "C"
#endif
