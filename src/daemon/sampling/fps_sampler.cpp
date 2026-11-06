/* FPS sampling from the display flip times the kernel reports through
 * /dev/dce, polled at 240 Hz from outside the game: no injection, no SPRX.
 * The /dev/dce approach follows PHU Games Tools by ArkSama
 * (https://github.com/ArkSama).
 */
#include "fps_sampler.hpp"

// Standard library
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// SDK
#include <unistd.h>

// Project
#include "console/running_game.hpp"
#include "console/sce_api.h"
#include "core/lifecycle.hpp"
#include "core/settings_store.hpp"
#include "fps/publish.hpp"
#include "fps/sources/dce.hpp"
#include "fps/stats/display_flips.hpp"
#include "fps/stats/frame_stats.hpp"
#include "platform/clock.h"
#include "platform/log.h"
#include "settings/settings.hpp"

namespace
{

constexpr unsigned kIdleSleepSec = 1;
constexpr useconds_t kRetryUs = 33333;
constexpr uint64_t kPollNs = 4166666; /* 240 Hz: twice the fastest game rate */
constexpr uint64_t kPublishNs = 33333333;

bool hud_fps_enabled(const orange::Settings& cfg)
{
    return cfg.overlay_enabled && cfg.overlay_fps;
}

class FlipSampler
{
  public:
    void run()
    {
        LOG_INFO("fps sampler started (display flip times, %u Hz)", static_cast<unsigned>(1000000000ULL / kPollNs));
        uint64_t next_ns = 0;
        while (!shutting_down())
        {
            const Pace pace = tick();
            if (pace == Pace::Poll)
            {
                /* A fixed grid: a late read does not push the next ones back. */
                const uint64_t now = monotonic_ns();
                next_ns = next_ns ? next_ns + kPollNs : now + kPollNs;
                if (next_ns > now)
                {
                    usleep(static_cast<useconds_t>((next_ns - now) / 1000));
                }
                else
                {
                    next_ns = now;
                }
                continue;
            }
            next_ns = 0;
            if (pace == Pace::Idle)
            {
                sleep(kIdleSleepSec);
            }
            else
            {
                usleep(kRetryUs);
            }
        }
    }

  private:
    enum class Pace
    {
        Idle,
        Retry,
        Poll
    };

    Pace tick()
    {
        if (!hud_fps_enabled(current_settings()))
        {
            publish_none();
            return Pace::Idle;
        }

        RunningGame game;
        if (!running_game(&game))
        {
            if (had_app_)
            {
                LOG_DEBUG("fps: target cleared (no running big app)");
            }
            had_app_ = false;
            pid_ = -1;
            tid_.clear();
            reset_target();
            publish_none();
            return Pace::Idle;
        }
        had_app_ = true;
        track_target(game);

        if (pid_ <= 0)
        {
            publish_none();
            return Pace::Retry;
        }
        if (!poll_flips())
        {
            publish_none_for_pid();
            return Pace::Retry;
        }
        return Pace::Poll;
    }

    void reset_target()
    {
        flips_.reset();
        flip_stats_.reset();
        new_flip_us_.clear();
        last_publish_ns_ = 0;
        logged_active_ = false;
    }

    /** Follow the running game's process; reset state when it changed. */
    void track_target(const RunningGame& game)
    {
        if (game.pid == pid_ && game.title_id == tid_)
        {
            return;
        }
        reset_target();
        pid_ = game.pid;
        tid_ = game.title_id;

        char name[64] = {};
        const bool named = pid_ > 0 && sceKernelGetProcessName(pid_, name) >= 0;
        LOG_DEBUG("fps: target tid=%s app=%d pid=%d name=%s", tid_.c_str(), game.app_id, static_cast<int>(pid_), named ? name : "?");
    }

    void publish_none()
    {
        OrangeFpsSample s{};
        s.pid = -1;
        s.low1 = s.low01 = -1.0f;
        s.unix_ns = realtime_ns();
        orange::fps::publish(s);
    }

    /** No value yet, but the game is known (HUD shows "--"). */
    void publish_none_for_pid()
    {
        OrangeFpsSample s{};
        s.pid = static_cast<int>(pid_);
        s.low1 = s.low01 = -1.0f;
        s.unix_ns = realtime_ns();
        orange::fps::publish(s);
    }

    /** One /dev/dce read; every 33 ms the flips collected since go out as statistics. False: flip status unreadable. */
    bool poll_flips()
    {
        unsigned char raw[orange::fps::kDceAnswerBytes];
        if (!dce_.sample(raw))
        {
            return false;
        }
        uint64_t flip_us = 0;
        if (flips_.feed(raw, &flip_us))
        {
            new_flip_us_.push_back(flip_us);
        }

        const uint64_t now = monotonic_ns();
        if (now - last_publish_ns_ < kPublishNs)
        {
            return true;
        }
        last_publish_ns_ = now;
        const orange::fps::FlipFrameStats::Reading r = flip_stats_.update(new_flip_us_, now);
        new_flip_us_.clear();

        OrangeFpsSample s{};
        s.pid = static_cast<int>(pid_);
        s.valid = r.frames ? 1 : 0; /* no frames: HUD shows "--" */
        s.source = ORANGE_FPS_SRC_FLIP;
        s.fps = r.frames ? r.fps : 0.f;
        s.low1 = r.low1;
        s.low01 = r.low01;
        s.unix_ns = realtime_ns();
        std::strncpy(s.title_id, tid_.c_str(), sizeof(s.title_id) - 1);
        flip_stats_.copy_graph_ft10(s.ft10, sizeof(s.ft10) / sizeof(s.ft10[0]));
        orange::fps::publish(s);
        if (!logged_active_)
        {
            LOG_INFO("fps: using display flip times tid=%s pid=%d", tid_.c_str(), static_cast<int>(pid_));
            logged_active_ = true;
        }
        return true;
    }

    orange::fps::DceSource dce_;
    orange::fps::DisplayFlips flips_;
    orange::fps::FlipFrameStats flip_stats_;
    std::vector<uint64_t> new_flip_us_;
    uint64_t last_publish_ns_ = 0;
    std::string tid_;
    pid_t pid_ = -1;
    bool logged_active_ = false;
    bool had_app_ = false;
};

} // namespace

void* fps_sampler_thread(void* args) noexcept
{
    (void)args;
    FlipSampler().run();
    return nullptr;
}
