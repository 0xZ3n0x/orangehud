#include "running_game.hpp"

// Standard library
#include <mutex>

// Project
#include "platform/clock.h"
#include "proc/query.h"
#include "sce_api.h"

namespace
{

constexpr uint64_t kAliveRecheckNs = 500ULL * 1000000ULL;

std::mutex g_mutex;
RunningGame g_cache;
uint64_t g_checked_ns = 0;

} // namespace

bool running_game(RunningGame* out)
{
    const int app_id = sceSystemServiceGetAppIdOfRunningBigApp();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (app_id < 0)
    {
        g_cache = RunningGame{};
        g_checked_ns = 0;
        return false;
    }

    const uint64_t now = monotonic_ns();
    if (app_id != g_cache.app_id)
    {
        g_cache = RunningGame{};
        g_cache.app_id = app_id;
        g_checked_ns = 0;
    }
    if (g_cache.title_id.empty())
    {
        /* Retried on every call until it succeeds (a failed lookup must not
         * leave the app permanently without a title id). */
        char tid[255] = {};
        if (sceSystemServiceGetAppTitleId(app_id, tid) != 0)
        {
            return false;
        }
        g_cache.title_id = tid;
    }

    /* A game id is known but its process may not be visible yet, or has gone. */
    if (g_cache.pid <= 0 || now - g_checked_ns >= kAliveRecheckNs)
    {
        if (g_cache.pid <= 0 || !proc_is_alive(g_cache.pid))
        {
            g_cache.pid = find_pid_ex("", false, true);
        }
        g_checked_ns = now;
    }

    if (out)
    {
        *out = g_cache;
    }
    return true;
}
