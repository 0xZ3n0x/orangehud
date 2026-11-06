/* Process lookup through the kern.proc sysctl (daemon only). Complements the
 * kernel allproc helpers in proc/proc.h.
 */

#pragma once

// Standard library
#include <stdbool.h>

// SDK
#include <sys/types.h>

#ifndef ORANGE_PROC_PROCESS_NAME_LEN
#define ORANGE_PROC_PROCESS_NAME_LEN 128
#endif

/* kinfo_proc.ki_comm usable chars (COMMLEN in kinfo_proc.h). */
#ifndef ORANGE_PROC_KI_COMM_LEN
#define ORANGE_PROC_KI_COMM_LEN 19
#endif

/* kinfo_proc.ki_tdname usable chars (TDNAMLEN in kinfo_proc.h). Shorter
 * than ki_comm, and find_pid matches both, so a caller that probes a
 * long name must try this truncation too or it will silently miss threads. */
#ifndef ORANGE_PROC_KI_TDNAM_LEN
#define ORANGE_PROC_KI_TDNAM_LEN 16
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /** Exact match on kinfo_proc.ki_comm. Returns pid or -1. */
    pid_t find_pid(const char* name);

    /** Substring match on ki_comm. Returns first match or -1. */
    pid_t find_pid_substr(const char* substr);

    /**
     * Extended lookup used by the daemon (FPS sampler, injection).
     * @param needle     if true, substring match process name; else exact.
     * @param for_bigapp if true, match running big-app (requires host SCE helpers
     *                   via proc_set_sce_hooks).
     * Pass name="" + for_bigapp=true for "any process of the running BigApp"
     * (the running game).
     */
    pid_t find_pid_ex(const char* name, bool needle, bool for_bigapp);

    /** True if pid still exists (KERN_PROC_PID). */
    bool proc_is_alive(pid_t pid);

    typedef int (*get_process_name_fn)(int pid, char* name);
    typedef int (*get_app_info_fn)(pid_t pid, void* app_info /* app_info_t */);
    typedef int (*get_bigapp_id_fn)(void);

    void proc_set_sce_hooks(get_process_name_fn get_name, get_app_info_fn get_app_info, get_bigapp_id_fn get_bigapp);

#ifdef __cplusplus
} // extern "C"
#endif
