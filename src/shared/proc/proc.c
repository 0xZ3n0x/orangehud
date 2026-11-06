#include "proc.h"

// Standard library
#include <stdlib.h>
#include <string.h>

// SDK
#include <ps5/kernel.h>
#include <unistd.h>

struct proc* get_proc_by_pid(pid_t pid)
{
    uintptr_t next = 0;

    kernel_copyout(KERNEL_ADDRESS_ALLPROC, &next, sizeof(uintptr_t));
    struct proc* proc = (struct proc*)malloc(sizeof(struct proc));
    do
    {
        kernel_copyout(next, proc, sizeof(struct proc));

        if (proc->pid == pid)
        {
            return proc;
        }

        kernel_copyout(next, &next, sizeof(uint64_t));

    } while (next);

    free(proc);
    return NULL;
}

//
// Handle of a loaded module of `pid` by file name (sys_dl_get_list /
// sys_dl_get_info_2). Returns 0 when the module is not loaded.
//
int get_module_handle(pid_t pid, const char* module_name)
{
    size_t num_handles = 0;
    syscall(SYS_dl_get_list, pid, NULL, 0, &num_handles);
    if (num_handles == 0)
    {
        return 0;
    }

    uintptr_t* handles = (uintptr_t*)calloc(num_handles, sizeof(uintptr_t));
    module_info_t* info = (module_info_t*)malloc(sizeof(module_info_t));
    int result = 0;

    if (handles && info)
    {
        syscall(SYS_dl_get_list, pid, handles, num_handles, &num_handles);
        for (size_t i = 0; i < num_handles; ++i)
        {
            bzero(info, sizeof(module_info_t));
            syscall(SYS_dl_get_info_2, pid, 1, handles[i], info);
            if (!strcmp(info->filename, module_name))
            {
                result = (int)info->handle;
                break;
            }
        }
    }

    free(handles);
    free(info);
    return result;
}
