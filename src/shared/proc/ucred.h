#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

// Standard library
#include <stdint.h>

// SDK
#include <sys/types.h>

/** SceTracer-style authid — only value the kernel accepts for ptrace PT_*. */
#define PTRACE_AUTHID 0x4800000000010003

    /**
     * Set this process ucred authid to PTRACE_AUTHID for ptrace inject/attach.
     * Returns previous authid (0 on failure). Caller restores with set_proc_authid.
     */
    uintptr_t set_ucred_to_ptrace(void);
    uintptr_t set_proc_authid(pid_t pid, uintptr_t new_authid);

#ifdef __cplusplus
}
#endif
