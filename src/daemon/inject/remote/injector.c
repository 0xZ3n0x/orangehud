#include "injector.h"

// Standard library
#include <errno.h>
#include <stdlib.h>

// SDK
#include <ps5/kernel.h>
#include <ps5/nid.h>
#include <pthread.h>
#include <sys/mman.h>
#include <unistd.h>

// Third party
#include "elfldr.h"
#include "pt.h"

// Project
#include "platform/log.h"
#include "proc/proc.h"
#include "proc/ucred.h"

/* What the stager reads in the target. Its layout is part of the machine code
 * below: pthread_create_ptr, elf_main and payload_args are read by offset. */
typedef struct
{
    void* reserved; /* unused slot, keeps the offsets below */
    int (*elf_main)(void* payload_args);
    void* payload_args;
    int (*pthread_create_ptr)(pthread_t*, const pthread_addr_t*, void* (*)(void*), void*);
} stager_args;

/* The stager is compiled code copied into the target: its bytes depend on the
 * flags of this target (see src/CMakeLists.txt) and on the section order
 * below. It starts the module on a thread, then stops on int3; pt_call2 waits
 * for that stop before it restores the target's registers. */
int __attribute__((section(".stager_shellcode$1"))) stager(stager_args* args)
{
    pthread_t thread;
    args->pthread_create_ptr(&thread, 0, (void* (*)(void*))args->elf_main, args->payload_args);

    __asm__("int3");

    return 0;
}

/* Only marks where the stager ends. */
int __attribute__((section(".stager_shellcode$2"))) stager_end(void)
{
    return 0;
}

static uint64_t stager_size(void)
{
    return (uint64_t)((const char*)&stager_end - (const char*)&stager);
}

/* Authid is elevated once for the whole window and restored on every exit:
 * PTRACE_AUTHID, not DEBUG_AUTHID (which makes attach / waitpid fail). */
static bool read_self_authid(uint64_t* out)
{
    *out = kernel_get_ucred_authid(getpid());
    return *out != 0;
}

static void write_self_authid(uint64_t authid)
{
    if (kernel_set_ucred_authid(getpid(), authid) != 0)
    {
        LOG_ERROR("inject: authid restore failed authid=0x%lx", (unsigned long)authid);
    }
}

/* mmap in the target; the remote result is NULL or MAP_FAILED on failure. */
static uint64_t remote_map(pid_t pid, uint64_t size)
{
    uint64_t addr = pt_mmap(pid, 0, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    return (addr == 0 || addr == (uint64_t)-1) ? 0 : addr;
}

/* Everything after a successful attach. The caller detaches. */
static bool inject_attached(pid_t pid, const uint8_t* elf, intptr_t* entry_out)
{
    char nid[12] = {0};
    nid_encode("pthread_create", nid);
    intptr_t remote_pthread_create = pt_resolve(pid, nid);
    if (remote_pthread_create == 0)
    {
        LOG_ERROR("inject: pthread_create not found in pid=%d", pid);
        return false;
    }

    /* The target is stopped from here on: remote mmap / mprotect via pt_syscall. */
    intptr_t entry = elfldr_load(pid, (uint8_t*)elf);
    if (entry <= 0)
    {
        LOG_ERROR("inject: failed to load ELF into pid=%d", pid);
        return false;
    }
    if (entry_out)
    {
        *entry_out = entry;
    }

    intptr_t payload_args = elfldr_payload_args(pid);
    if (payload_args <= 0)
    {
        LOG_ERROR("inject: failed to allocate payload args in pid=%d", pid);
        return false;
    }

    stager_args args = {
        .elf_main = (int (*)(void*))entry,
        .payload_args = (void*)payload_args,
        .pthread_create_ptr = (int (*)(pthread_t*, const pthread_addr_t*, void* (*)(void*), void*))remote_pthread_create,
    };

    const uint64_t code_size = stager_size();
    if (code_size == 0 || code_size > 4096)
    {
        LOG_ERROR("inject: bad stager size %lu", (unsigned long)code_size);
        return false;
    }

    uint64_t bootstrap = remote_map(pid, code_size);
    if (bootstrap == 0)
    {
        LOG_ERROR("inject: bootstrap mmap failed pid=%d", pid);
        return false;
    }
    if (kernel_mprotect(pid, bootstrap, code_size, PROT_EXEC | PROT_WRITE | PROT_READ) != 0)
    {
        LOG_ERROR("inject: bootstrap mprotect failed pid=%d addr=%#lx", pid, (unsigned long)bootstrap);
        return false;
    }
    if (pt_copyin(pid, stager, bootstrap, code_size) != 0)
    {
        LOG_ERROR("inject: bootstrap copyin failed pid=%d addr=%#lx", pid, (unsigned long)bootstrap);
        return false;
    }

    uint64_t args_addr = remote_map(pid, sizeof(args));
    if (args_addr == 0)
    {
        LOG_ERROR("inject: args mmap failed pid=%d", pid);
        return false;
    }
    if (pt_copyin(pid, &args, args_addr, sizeof(args)) != 0)
    {
        LOG_ERROR("inject: args copyin failed pid=%d", pid);
        return false;
    }

    if (pt_call2(pid, bootstrap, args_addr) == -1)
    {
        LOG_ERROR("inject: pt_call2 failed pid=%d errno=%d", pid, errno);
        return false;
    }
    return true;
}

bool inject_elf(pid_t pid, const uint8_t* elf, intptr_t* entry_out)
{
    if (entry_out)
    {
        *entry_out = 0;
    }
    if (pid < 0 || !elf)
    {
        LOG_ERROR("inject: invalid arguments pid=%d elf=%p", pid, (const void*)elf);
        return false;
    }

    /* Same existence check as before: a pid with no proc is not attached to. */
    struct proc* target = get_proc_by_pid(pid);
    if (!target)
    {
        LOG_ERROR("inject: no process with pid=%d", pid);
        return false;
    }
    free(target);

    uint64_t original_authid = 0;
    const bool authid_saved = read_self_authid(&original_authid);
    if (!authid_saved)
    {
        LOG_ERROR("inject: authid backup failed (continuing)");
    }
    set_ucred_to_ptrace();

    bool ok = false;
    if (pt_attach(pid) < 0)
    {
        LOG_ERROR("inject: attach failed pid=%d errno=%d", pid, errno);
    }
    else
    {
        ok = inject_attached(pid, elf, entry_out);
        if (pt_detach(pid, 0) != 0)
        {
            LOG_ERROR("inject: detach failed pid=%d errno=%d", pid, errno);
        }
    }

    if (authid_saved)
    {
        write_self_authid(original_authid);
    }
    return ok;
}
