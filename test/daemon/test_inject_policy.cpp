// Project
#include "inject/shellui/policy.hpp"
#include "test_harness.h"

namespace
{

using namespace orange::inject;

constexpr unsigned kAbi = 3;

int test_marker_of_the_running_module_with_our_abi_is_current()
{
    TEST_ASSERT_TRUE(marker_state(true, 500, kAbi, 500, kAbi) == MarkerState::Current);
    return 0;
}

int test_marker_with_another_abi_is_stale()
{
    TEST_ASSERT_TRUE(marker_state(true, 500, kAbi - 1, 500, kAbi) == MarkerState::Stale);
    TEST_ASSERT_TRUE(marker_state(true, 500, kAbi + 1, 500, kAbi) == MarkerState::Stale);
    /* A marker written by a build that predates the ABI number (a hash, or 0). */
    TEST_ASSERT_TRUE(marker_state(true, 500, 1410401191u, 500, kAbi) == MarkerState::Stale);
    return 0;
}

int test_marker_of_another_process_or_missing_is_absent()
{
    TEST_ASSERT_TRUE(marker_state(true, 400, kAbi, 500, kAbi) == MarkerState::Absent);
    TEST_ASSERT_TRUE(marker_state(true, 400, kAbi - 1, 500, kAbi) == MarkerState::Absent); /* a replaced ShellUI */
    TEST_ASSERT_TRUE(marker_state(false, 500, kAbi, 500, kAbi) == MarkerState::Absent);
    return 0;
}

int test_ready_results()
{
    TEST_ASSERT_TRUE(is_ready(Result::AlreadyReady));
    TEST_ASSERT_TRUE(is_ready(Result::Injected));
    TEST_ASSERT_TRUE(!is_ready(Result::StaleModule));
    TEST_ASSERT_TRUE(!is_ready(Result::InjectFailed));
    return 0;
}

int test_recovery_ends_on_success_or_stale_module_only()
{
    TEST_ASSERT_TRUE(ends_recovery(Result::AlreadyReady));
    TEST_ASSERT_TRUE(ends_recovery(Result::Injected));
    TEST_ASSERT_TRUE(ends_recovery(Result::StaleModule));
    TEST_ASSERT_TRUE(!ends_recovery(Result::TargetNotFound));
    TEST_ASSERT_TRUE(!ends_recovery(Result::InjectFailed));
    TEST_ASSERT_TRUE(!ends_recovery(Result::ReadyTimeout));
    TEST_ASSERT_TRUE(!ends_recovery(Result::TargetChanged));
    return 0;
}

int test_sleeping_arms_the_resume_step()
{
    const WatchDecision d = watch_decision(false, /*sleeping=*/true, /*working=*/false);
    TEST_ASSERT_TRUE(d.was_sleeping);
    TEST_ASSERT_TRUE(d.step == WatchStep::Idle);
    return 0;
}

int test_working_after_sleep_resumes_once()
{
    WatchDecision d = watch_decision(true, false, /*working=*/true);
    TEST_ASSERT_TRUE(d.step == WatchStep::Resume);
    TEST_ASSERT_TRUE(!d.was_sleeping);

    /* The next tick is a normal one. */
    d = watch_decision(d.was_sleeping, false, true);
    TEST_ASSERT_TRUE(d.step == WatchStep::CheckRestart);
    return 0;
}

int test_working_without_sleep_checks_for_restart()
{
    const WatchDecision d = watch_decision(false, false, true);
    TEST_ASSERT_TRUE(d.step == WatchStep::CheckRestart);
    TEST_ASSERT_TRUE(!d.was_sleeping);
    return 0;
}

int test_unknown_state_changes_nothing()
{
    WatchDecision d = watch_decision(true, false, false);
    TEST_ASSERT_TRUE(d.step == WatchStep::Idle);
    TEST_ASSERT_TRUE(d.was_sleeping); /* still owes a resume */

    d = watch_decision(false, false, false);
    TEST_ASSERT_TRUE(d.step == WatchStep::Idle);
    TEST_ASSERT_TRUE(!d.was_sleeping);
    return 0;
}

int test_resume_without_shellui_waits_for_one()
{
    TEST_ASSERT_TRUE(resume_action(-1, false) == ResumeAction::ScheduleAny);
    TEST_ASSERT_TRUE(resume_action(0, false) == ResumeAction::ScheduleAny);
    TEST_ASSERT_TRUE(resume_action(1, false) == ResumeAction::ScheduleAny);
    return 0;
}

int test_resume_keeps_a_shellui_that_already_runs_the_hud()
{
    TEST_ASSERT_TRUE(resume_action(500, true) == ResumeAction::AlreadyUp);
    return 0;
}

int test_resume_injects_a_shellui_without_the_hud()
{
    TEST_ASSERT_TRUE(resume_action(500, false) == ResumeAction::Schedule);
    return 0;
}

int test_new_shellui_detection()
{
    TEST_ASSERT_TRUE(is_new_shellui(500, 0));
    TEST_ASSERT_TRUE(is_new_shellui(500, 400));
    TEST_ASSERT_TRUE(!is_new_shellui(500, 500));
    TEST_ASSERT_TRUE(!is_new_shellui(-1, 500));
    TEST_ASSERT_TRUE(!is_new_shellui(0, 0));
    TEST_ASSERT_TRUE(!is_new_shellui(1, 0));
    return 0;
}

} // namespace

extern "C" int test_inject_policy_suite(void)
{
    int failures = 0;
    failures += test_run("inject_policy.marker_current", test_marker_of_the_running_module_with_our_abi_is_current);
    failures += test_run("inject_policy.marker_stale", test_marker_with_another_abi_is_stale);
    failures += test_run("inject_policy.marker_absent", test_marker_of_another_process_or_missing_is_absent);
    failures += test_run("inject_policy.ready", test_ready_results);
    failures += test_run("inject_policy.recovery_end", test_recovery_ends_on_success_or_stale_module_only);
    failures += test_run("inject_policy.sleep_arms_resume", test_sleeping_arms_the_resume_step);
    failures += test_run("inject_policy.resume_once", test_working_after_sleep_resumes_once);
    failures += test_run("inject_policy.restart_check", test_working_without_sleep_checks_for_restart);
    failures += test_run("inject_policy.unknown_state", test_unknown_state_changes_nothing);
    failures += test_run("inject_policy.resume_no_shellui", test_resume_without_shellui_waits_for_one);
    failures += test_run("inject_policy.resume_already_up", test_resume_keeps_a_shellui_that_already_runs_the_hud);
    failures += test_run("inject_policy.resume_inject", test_resume_injects_a_shellui_without_the_hud);
    failures += test_run("inject_policy.new_shellui", test_new_shellui_detection);
    return failures;
}
