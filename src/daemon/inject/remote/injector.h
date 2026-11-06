#pragma once
/* Remote ELF injector: loads a module into another process over ptrace and
 * starts its entry point on a new thread there. Used for SceShellUI (the HUD).
 *
 * It keeps process-wide state while it runs (the attach and the elevated
 * authid), so callers must serialize calls (see inject/lock.hpp). */

// Standard library
#include <stdbool.h>
#include <stdint.h>

// SDK
#include <sys/types.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Injects `elf` into `pid`. On success returns true. `entry_out` (may be NULL)
     * is set to the loaded module's entry address as soon as the ELF is mapped,
     * and to 0 before that, so it is valid even if a later step fails. */
    bool inject_elf(pid_t pid, const uint8_t* elf, intptr_t* entry_out);

#ifdef __cplusplus
}
#endif
