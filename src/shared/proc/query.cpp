/* sysctl-based process lookup (single implementation for all OrangeHUD bins).
 */

extern "C"
{
#include "query.h"
}

// Standard library
#include <cstdlib>
#include <cstring>

// SDK
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h> /* struct kinfo_proc: same size/offsets as the copy this tree used to vendor */

namespace
{

get_process_name_fn g_get_name = nullptr;
get_app_info_fn g_get_app_info = nullptr;
get_bigapp_id_fn g_get_bigapp = nullptr;

// app_info_t starts with uint32_t app_id (see shellui external_symbols).

/** kern.proc table in a malloc'd buffer (caller frees). Pads the size query
 * because processes can appear between the two sysctl calls. */
void* fetch_proc_table(size_t* size_out)
{
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PROC, 0};
    size_t size = 0;
    if (sysctl(mib, 4, nullptr, &size, nullptr, 0))
    {
        return nullptr;
    }
    size += size / 4 + 4096;
    void* buf = malloc(size);
    if (!buf)
    {
        return nullptr;
    }
    if (sysctl(mib, 4, buf, &size, nullptr, 0))
    {
        free(buf);
        return nullptr;
    }
    *size_out = size;
    return buf;
}

/** Walk the process table; returns the pid of the first entry `match` accepts. */
template <typename Match> pid_t find_in_table(Match match)
{
    size_t buf_size = 0;
    void* buf = fetch_proc_table(&buf_size);
    if (!buf)
    {
        return -1;
    }

    pid_t found = -1;
    for (char* ptr = static_cast<char*>(buf); ptr < static_cast<char*>(buf) + buf_size;)
    {
        auto* ki = reinterpret_cast<struct kinfo_proc*>(ptr);
        if (ki->ki_structsize <= 0)
        {
            break;
        }
        ptr += ki->ki_structsize;
        if (match(*ki))
        {
            found = ki->ki_pid;
            break;
        }
    }

    free(buf);
    return found;
}

bool name_matches(const char* candidate, const char* name, bool substr)
{
    return substr ? strstr(candidate, name) != nullptr : strcmp(candidate, name) == 0;
}

pid_t scan_kinfo(const char* name, bool substr)
{
    if (!name || !*name)
    {
        return -1;
    }
    return find_in_table([&](const kinfo_proc& ki) {
        // Also match the thread name (the daemon historically used ki_tdname).
        return name_matches(ki.ki_comm, name, substr) || (ki.ki_tdname[0] && name_matches(ki.ki_tdname, name, substr));
    });
}

} // namespace

extern "C" void proc_set_sce_hooks(get_process_name_fn get_name, get_app_info_fn get_app_info, get_bigapp_id_fn get_bigapp)
{
    g_get_name = get_name;
    g_get_app_info = get_app_info;
    g_get_bigapp = get_bigapp;
}

extern "C" pid_t find_pid(const char* name)
{
    return scan_kinfo(name, /*substr=*/false);
}

extern "C" pid_t find_pid_substr(const char* substr)
{
    return scan_kinfo(substr, /*substr=*/true);
}

extern "C" bool proc_is_alive(pid_t pid)
{
    if (pid <= 0)
    {
        return false;
    }
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(pid)};
    return sysctl(mib, 4, nullptr, nullptr, nullptr, 0) == 0;
}

extern "C" pid_t find_pid_ex(const char* name, bool needle, bool for_bigapp)
{
    if (!name)
    {
        return -1;
    }

    // Fast path: no big-app filter — name match only.
    if (!for_bigapp)
    {
        return needle ? find_pid_substr(name) : find_pid(name);
    }

    // Big-app path needs SCE hooks installed by the host binary (shellui).
    if (!g_get_bigapp)
    {
        return needle ? find_pid_substr(name) : find_pid(name);
    }

    int bigappid = g_get_bigapp();
    if (bigappid < 0)
    {
        return -1;
    }

    char proc_name[ORANGE_PROC_PROCESS_NAME_LEN] = {0};
    return find_in_table([&](const kinfo_proc& ki) {
        char appinfo_buf[128]{};
        int32_t app_id = 0;
        if (g_get_app_info)
        {
            if (g_get_app_info(ki.ki_pid, appinfo_buf) == 0)
            {
                app_id = *reinterpret_cast<int32_t*>(appinfo_buf);
            }
        }

        if (g_get_name)
        {
            if (g_get_name(ki.ki_pid, proc_name) != 0)
            {
                return false;
            }
        }
        else
        {
            strncpy(proc_name, ki.ki_comm, sizeof(proc_name) - 1);
        }

        if (bigappid != app_id)
        {
            return false;
        }
        return !name[0] || name_matches(proc_name, name, needle);
    });
}
