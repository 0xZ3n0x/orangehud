#pragma once

// Standard library
#include <stdint.h>

// SDK
#include <sys/syscall.h>
#include <sys/types.h>

#define SYS_dl_get_list 0x217
#define SYS_dl_get_info_2 0x2cd

#ifndef MODULE_INFO_NAME_LENGTH
#define MODULE_INFO_NAME_LENGTH 128
#endif
#ifndef MODULE_INFO_SANDBOXED_PATH_LENGTH
#define MODULE_INFO_SANDBOXED_PATH_LENGTH 1024
#endif
#ifndef MODULE_INFO_MAX_SECTIONS
#define MODULE_INFO_MAX_SECTIONS 4
#endif
#ifndef FINGERPRINT_LENGTH
#define FINGERPRINT_LENGTH 20
#endif

/*
 * Kernel `struct proc` as laid out by the PS5 kernel. Only the fields
 * OrangeHUD reads are named; the rest is padding up to the copied size.
 */
struct proc
{
    char _pad0[0x40];
    void* p_ucred;
    char _pad1[0xBC - 0x48];
    int pid;
    char _pad2[0x800 - 0xC0];
};

typedef struct
{
    uint64_t vaddr;
    uint32_t size;
    uint32_t prot;
} module_section_t;

typedef struct
{
    char filename[MODULE_INFO_NAME_LENGTH];
    uint64_t handle;
    uint8_t unknown0[32];     // NOLINT(readability-magic-numbers)
    uint64_t init;            // init
    uint64_t fini;            // fini
    uint64_t eh_frame_hdr;    // eh_frame_hdr
    uint64_t eh_frame_hdr_sz; // eh_frame_hdr_sz
    uint64_t eh_frame;        // eh_frame
    uint64_t eh_frame_sz;     // eh_frame_sz
    module_section_t sections[MODULE_INFO_MAX_SECTIONS];
    uint8_t unknown7[1176]; // NOLINT(readability-magic-numbers)
    uint8_t fingerprint[FINGERPRINT_LENGTH];
    uint32_t unknown8;
    char libname[MODULE_INFO_NAME_LENGTH];
    uint32_t unknown9;
    char sandboxed_path[MODULE_INFO_SANDBOXED_PATH_LENGTH];
    uint64_t sdk_version;
} module_info_t;

#ifdef __cplusplus
extern "C"
{
#endif

    struct proc* get_proc_by_pid(pid_t pid);
    int get_module_handle(pid_t pid, const char* module_name);

/** Dynlib handle of libkernel in every process (for kernel_dynlib_dlsym). */
#define ORANGE_LIBKERNEL_HANDLE 0x2001

#ifdef __cplusplus
}
#endif
