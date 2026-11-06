/* Keeps the HUD module injected into SceShellUI.
 *
 * Injection is serialized (one ptrace session at a time) and bound to the
 * ShellUI process instance: the module publishes its PID in a ready marker
 * once its hooks are installed, so a matching marker means "this ShellUI
 * already runs the HUD".
 *
 * Triggers:
 *   inject_now()  cold start
 *   watch()       Rest Mode resume and ShellUI restarts (blocking loop)
 */
#pragma once

// Standard library
#include <atomic>
#include <cstdint>

// SDK
#include <sys/types.h>

// Project
#include "policy.hpp"

class ShellUiInjector
{
  public:
    /** `module_elf` is the embedded shellui.elf image. */
    explicit ShellUiInjector(const uint8_t* module_elf) : elf_(module_elf)
    {
    }

    /**
     * Cold start: claim the live ShellUI and inject on a worker thread with
     * retries and no resume delay. Returns immediately.
     */
    void inject_now();

    /**
     * Watch the power state until shutdown. After Rest Mode the module is
     * re-injected (honoring rest_mode.resume_reinject_delay_seconds); a new
     * SceShellUI PID (crash / restart) is re-injected right away.
     */
    void watch();

  private:
    using Result = orange::inject::Result;

    struct Outcome
    {
        Result result;
        pid_t pid;
        bool ready() const
        {
            return orange::inject::is_ready(result);
        }
    };

    /** One serialized inject attempt on `expected_pid` (0 = the live ShellUI). */
    Outcome inject_once(pid_t expected_pid);
    void report(const Outcome& outcome, bool notify_failure) const;

    /** Wait for userland libs, optional resume delay, then inject once. */
    bool recover_once(pid_t exec_pid, pid_t previous_pid, uint32_t gen, bool apply_resume_delay);
    /** Worker body: up to five attempts with growing delays. */
    void recover(pid_t exec_pid, pid_t previous_pid, uint32_t gen, bool resume_delay);
    /** Start a recovery on its own thread (claims the PID and a generation). */
    void schedule(pid_t exec_pid, bool resume_delay);

    bool wait_for_sprx(pid_t pid, uint32_t gen, const char* name) const;
    bool wait_for_resume_delay(pid_t pid, uint32_t gen) const;
    bool superseded(uint32_t gen) const
    {
        return generation_.load(std::memory_order_acquire) != gen;
    }

    void on_resume();
    void on_new_shellui(pid_t pid);
    void poll_for_restart();

    void remember(pid_t pid)
    {
        if (pid > 1)
        {
            last_shellui_.store(pid, std::memory_order_release);
        }
    }

    const uint8_t* elf_;
    std::atomic<pid_t> last_shellui_{0};
    std::atomic<uint32_t> generation_{0};
};
