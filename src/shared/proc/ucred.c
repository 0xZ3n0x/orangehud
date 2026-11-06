#include "ucred.h"

// Standard library
#include <stdlib.h>

// SDK
#include <ps5/kernel.h>
#include <unistd.h>

// Project
#include "proc/proc.h"

uintptr_t set_ucred_to_ptrace(void)
{
    return set_proc_authid(getpid(), PTRACE_AUTHID);
}

//
// Search process entry on the allproc linked list, acquire ucred, set authid.
// Returns previous authid, or 0 on failure.
//
uintptr_t set_proc_authid(pid_t pid, uintptr_t new_authid)
{
    struct proc* proc = get_proc_by_pid(pid);
    if (!proc)
    {
        return 0;
    }

    uintptr_t authid = 0;
    kernel_copyout((uintptr_t)proc->p_ucred + 0x58, &authid, sizeof(uintptr_t));
    kernel_copyin(&new_authid, (uintptr_t)proc->p_ucred + 0x58, sizeof(uintptr_t));
    free(proc);
    return authid;
}
