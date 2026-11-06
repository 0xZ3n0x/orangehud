#pragma once

// Standard library
#include <cstdint>

enum class PowerState : uint8_t
{
    Unknown,
    Working,
    MainOnStandby,
    SuspendOnGoing,
};

/** Read the current SceSystemStateMgr state from its kernel event flag. */
PowerState power_state_get();

/** True for a sampled state while the console is entering/being suspended. */
bool power_state_is_sleeping(PowerState state);

/** True while the console is entering or already in standby suspend. */
bool power_state_is_sleeping();
