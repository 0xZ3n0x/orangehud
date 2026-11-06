/* Decisions of the ShellUI injector as pure functions of what it observed
 * (power state, PIDs, ready marker, injection outcome). ShellUiInjector gathers
 * the inputs and carries out the result; nothing here touches the system, so
 * the host tests cover the rules.
 */
#pragma once

// SDK
#include <sys/types.h>

namespace orange::inject
{

/** How one injection attempt ended. */
enum class Result
{
    AlreadyReady,
    Injected,
    TargetNotFound,
    InjectFailed,
    ReadyTimeout,
    TargetChanged,
    StaleModule, // running module speaks another protocol version
};

/** The HUD module runs in the target. */
constexpr bool is_ready(Result r)
{
    return r == Result::AlreadyReady || r == Result::Injected;
}

/** Recovery stops here: done, or a stale module that retrying cannot fix. */
constexpr bool ends_recovery(Result r)
{
    return is_ready(r) || r == Result::StaleModule;
}

/** What the HUD ready marker says about a ShellUI process. */
enum class MarkerState
{
    Absent,  /* no marker, or it names another process: the HUD is not in this ShellUI */
    Current, /* the module runs and speaks our ABI */
    Stale,   /* the module runs but speaks another ABI: it cannot be replaced */
};

constexpr MarkerState marker_state(bool published, pid_t published_pid, unsigned published_abi, pid_t pid, unsigned expected_abi)
{
    if (!published || published_pid != pid)
    {
        return MarkerState::Absent;
    }
    return published_abi == expected_abi ? MarkerState::Current : MarkerState::Stale;
}

enum class WatchStep
{
    Idle,
    Resume,       /* back from standby: reconcile the ShellUI */
    CheckRestart, /* normal running: look for a replaced ShellUI */
};

struct WatchDecision
{
    bool was_sleeping;
    WatchStep step;
};

/**
 * One tick of the power-state watch. `sleeping` and `working` come from the
 * sampled state; an unknown state changes nothing.
 */
constexpr WatchDecision watch_decision(bool was_sleeping, bool sleeping, bool working)
{
    if (sleeping)
    {
        return {true, WatchStep::Idle};
    }
    if (!working)
    {
        return {was_sleeping, WatchStep::Idle};
    }
    return {false, was_sleeping ? WatchStep::Resume : WatchStep::CheckRestart};
}

enum class ResumeAction
{
    ScheduleAny, /* no ShellUI yet: inject as soon as one shows up */
    AlreadyUp,   /* the live ShellUI already runs the HUD */
    Schedule,    /* inject into the live ShellUI */
};

/** After standby: `ready` is whether the HUD marker names `live_pid`. */
constexpr ResumeAction resume_action(pid_t live_pid, bool ready)
{
    if (live_pid <= 1)
    {
        return ResumeAction::ScheduleAny;
    }
    return ready ? ResumeAction::AlreadyUp : ResumeAction::Schedule;
}

/** A live ShellUI the injector has not handled yet (pid <= 1 means none). */
constexpr bool is_new_shellui(pid_t live_pid, pid_t last_pid)
{
    return live_pid > 1 && live_pid != last_pid;
}

} // namespace orange::inject
