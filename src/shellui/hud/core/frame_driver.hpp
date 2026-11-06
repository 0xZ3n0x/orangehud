/* The per-frame bookkeeping of the HUD hook as plain state machines: how many
 * frames to wait, how often to do HUD work, when the panel must be rebuilt,
 * updated or removed. OnRender_Hook reads the frame and calls Mono; the
 * decisions live here so the host tests cover them.
 */
#pragma once

// Standard library
#include <cstdint>

namespace orange::overlay
{

/**
 * Lets the scene settle after hooking, then does HUD work every
 * kUpdateIntervalFrames-th frame. Nothing counts down until the hooks are
 * ready.
 */
class FramePacer
{
  public:
    static constexpr int kInitDelayFrames = 30;
    static constexpr int kUpdateIntervalFrames = 2;

    /** Call once per frame; true when this frame does HUD work. */
    bool tick(bool hooks_ready)
    {
        if (!hooks_ready || init_delay_ > 0 || until_update_ > 0)
        {
            if (hooks_ready)
            {
                if (init_delay_ > 0)
                {
                    --init_delay_;
                }
                else
                {
                    --until_update_;
                }
            }
            return false;
        }
        until_update_ = kUpdateIntervalFrames - 1;
        return true;
    }

  private:
    int init_delay_ = kInitDelayFrames;
    int until_update_ = 0;
};

/**
 * Follows what the daemon publishes: builds the panel when it first has a frame
 * or its configuration changed, pushes the texts when they change, and takes
 * the panel away when the daemon stops publishing.
 *
 * Per frame, in this order:
 *   frame readable:  needs_build() -> [build, then built_ok()] -> needs_update()
 *   frame missing:   on_stale()
 */
class HudTracker
{
  public:
    /** Reads of a stale frame (x kUpdateIntervalFrames frames, ~1 s) before the panel is removed. */
    static constexpr int kStaleReadsBeforeHide = 15;

    bool built() const
    {
        return built_;
    }

    /** A readable frame arrived: does the panel have to be (re)built for `config_gen`? */
    bool needs_build(uint32_t config_gen)
    {
        stale_reads_ = 0;
        just_built_ = false;
        return !built_ || config_gen != built_gen_;
    }

    /** The build that needs_build() asked for worked. */
    void built_ok(uint32_t config_gen)
    {
        built_ = true;
        built_gen_ = config_gen;
        just_built_ = true;
    }

    /** True when the texts must be pushed: the panel was just built, or the values differ from the last push. */
    bool needs_update(bool values_changed)
    {
        return built_ && (just_built_ || values_changed);
    }

    /**
     * No fresh frame (the daemon exited, was killed or is restarting). True
     * once, when the panel should be removed; the module stays installed and
     * rebuilds when a frame arrives again.
     */
    bool on_stale()
    {
        if (built_ && ++stale_reads_ == kStaleReadsBeforeHide)
        {
            built_ = false;
            return true;
        }
        return false;
    }

  private:
    bool built_ = false;
    bool just_built_ = false;
    uint32_t built_gen_ = 0;
    int stale_reads_ = 0;
};

} // namespace orange::overlay
