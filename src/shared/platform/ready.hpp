/* Cross-process readiness markers.
 *
 * A service publishes the PID of the process instance it initialized in a
 * small file under /system_tmp/orangehud/ready/<name>. A consumer can then tell
 * a still-running initialized instance from a replacement process that reused
 * the same name, and wait for it with a timeout instead of fixed sleeps.
 */
#pragma once

// SDK
#include <sys/types.h>

namespace orange
{

class ReadyMarker
{
  public:
    /** `name` is a short token without path separators or dots. */
    explicit constexpr ReadyMarker(const char* name) : name_(name)
    {
    }

    /**
     * Publish `pid` as the initialized instance, optionally tagged with a
     * `version` of the publisher's protocol. False for pid <= 0.
     */
    bool signal(pid_t pid, unsigned version = 0) const;
    /** Read the published PID (and version, 0 when none was published). */
    bool read(pid_t* pid_out, unsigned* version_out = nullptr) const;
    /** True when the marker holds exactly `pid`. */
    bool matches(pid_t pid) const;
    /** Remove the marker. */
    bool clear() const;
    /** Poll until the marker holds `pid` or `timeout_ms` elapses. */
    bool wait(pid_t pid, int timeout_ms, int poll_ms = 200) const;

  private:
    const char* name_;
};

/** Initialized OrangeHUD daemon process. */
inline constexpr ReadyMarker kDaemonReady{"daemon"};
/** SceShellUI instance that has the HUD module installed. */
inline constexpr ReadyMarker kHudReady{"hud"};

} // namespace orange
