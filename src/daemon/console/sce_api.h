/* Sony system API prototypes used by more than one daemon file. */
#pragma once

// Standard library
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /** sceKernelGetAppInfo result; starts with the app id. */
    typedef struct app_info
    {
        uint32_t app_id;
        uint64_t unknown1;
        uint32_t app_type;
        char title_id[10];
        char unknown2[0x3c];
    } app_info_t;

    int sceSystemServiceGetAppIdOfRunningBigApp(void);
    int sceSystemServiceGetAppTitleId(int app_id, char* title_id);
    int sceKernelGetProcessName(int pid, char* name);
    int sceKernelGetAppInfo(int pid, app_info_t* info);

#ifdef __cplusplus
}
#endif
