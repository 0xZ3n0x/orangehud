#include "power_state.hpp"

// Standard library
#include <cstdint>
#include <mutex>

// Project
#include "platform/log.h"

extern "C"
{
    int sceKernelOpenEventFlag(intptr_t* event_flag, const char* name);
    int sceKernelPollEventFlag(intptr_t event_flag, uint64_t bits, unsigned int wait_mode, uint64_t* result);
    int sceKernelCloseEventFlag(intptr_t event_flag);
}

namespace
{

constexpr unsigned int kWaitModeOr = 2;
constexpr unsigned int kStateMask = 0xffff;
constexpr unsigned int kStateSuspendOnGoing = 300;
constexpr unsigned int kStateMainOnStandby = 500;
constexpr unsigned int kStateWorking = 1000;

std::mutex g_power_state_mutex;
intptr_t g_state_event_flag = -1;

bool ensure_event_flag_locked()
{
    if (g_state_event_flag >= 0)
    {
        return true;
    }

    const int rc = sceKernelOpenEventFlag(&g_state_event_flag, "SceSystemStateMgrInfo");
    if (rc < 0)
    {
        g_state_event_flag = -1;
        return false;
    }
    LOG_DEBUG("rest: power state event flag opened handle=%lld", static_cast<long long>(g_state_event_flag));
    return true;
}

PowerState read_state_locked()
{
    if (!ensure_event_flag_locked())
    {
        return PowerState::Unknown;
    }

    uint64_t pattern = 0;
    const int rc = sceKernelPollEventFlag(g_state_event_flag, UINT64_MAX, kWaitModeOr, &pattern);
    if (rc < 0)
    {
        LOG_DEBUG("rest: power state event flag poll failed rc=0x%x", rc);
        (void)sceKernelCloseEventFlag(g_state_event_flag);
        g_state_event_flag = -1;
        return PowerState::Unknown;
    }

    switch (static_cast<unsigned int>(pattern & kStateMask))
    {
    case kStateWorking:
        return PowerState::Working;
    case kStateMainOnStandby:
        return PowerState::MainOnStandby;
    case kStateSuspendOnGoing:
        return PowerState::SuspendOnGoing;
    default:
        return PowerState::Unknown;
    }
}

} // namespace

PowerState power_state_get()
{
    std::lock_guard<std::mutex> lock(g_power_state_mutex);
    return read_state_locked();
}

bool power_state_is_sleeping(PowerState state)
{
    return state == PowerState::MainOnStandby || state == PowerState::SuspendOnGoing;
}

bool power_state_is_sleeping()
{
    return power_state_is_sleeping(power_state_get());
}
