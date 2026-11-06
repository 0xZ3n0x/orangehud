/* Scoped restore of a process authid for the ptrace / elevation windows. */
#pragma once

// Project
#include "proc/ucred.h"

namespace orange
{

/** Restores `old_authid` of `pid` on scope exit unless release() was called. */
class AuthIdGuard
{
  public:
    AuthIdGuard(pid_t p, uintptr_t old_authid) : pid(p), old_authid(old_authid)
    {
    }
    ~AuthIdGuard()
    {
        if (active_)
        {
            set_proc_authid(pid, old_authid);
        }
    }
    /** The caller restored the authid itself. */
    void release()
    {
        active_ = false;
    }

    AuthIdGuard(const AuthIdGuard&) = delete;
    AuthIdGuard& operator=(const AuthIdGuard&) = delete;

    pid_t pid;
    uintptr_t old_authid;

  private:
    bool active_ = true;
};

} // namespace orange
