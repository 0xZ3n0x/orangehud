/* Frame statistics from the display flip timestamps, free of any SDK
 * dependency so the host tests can exercise them.
 *
 *  - FrameWindow: frame times of the last N frames; 1% / 0.1% low with
 *    MangoHud's definition (frame times sorted descending, idx = p * N - 1,
 *    FPS = 1000 / ft[idx]).
 *  - FlipRate: frame rate over a trailing time span from flip timestamps.
 *  - FlipFrameStats: both of them fed from the flip timestamps, with the
 *    warm-up cut and the once-a-second refresh of the lows.
 */
#pragma once

// Standard library
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace orange
{
namespace fps
{

class FrameWindow
{
  public:
    /** `max_frame_ms`: longer frames (loading, pauses) are not recorded. */
    explicit FrameWindow(size_t capacity = 10000, double max_frame_ms = 500.0) : capacity_(capacity), max_frame_ms_(max_frame_ms)
    {
        frames_.reserve(capacity + kSlack);
    }

    /** Record one frame time in milliseconds. Returns false when ignored. */
    bool push(double ms)
    {
        if (!(ms > 0.0) || !std::isfinite(ms) || ms > max_frame_ms_)
        {
            return false;
        }
        frames_.push_back(static_cast<float>(ms));
        if (frames_.size() > capacity_ + kSlack)
        {
            frames_.erase(frames_.begin(), frames_.end() - static_cast<std::ptrdiff_t>(capacity_));
        }
        return true;
    }

    /** Frames currently inside the window (at most the capacity). */
    size_t size() const
    {
        return std::min(frames_.size(), capacity_);
    }

    void clear()
    {
        frames_.clear();
    }

    /**
     * FPS of the p-th slowest frame (p = 0.01 for the 1% low); -1 while fewer
     * than `min_frames` frames are available.
     */
    float low(double p, size_t min_frames) const
    {
        const size_t n = size();
        if (n == 0 || n < min_frames)
        {
            return -1.f;
        }
        std::vector<float> desc(frames_.end() - static_cast<std::ptrdiff_t>(n), frames_.end());
        std::sort(desc.begin(), desc.end(), std::greater<float>());
        long idx = static_cast<long>(p * static_cast<double>(n)) - 1;
        if (idx < 0)
        {
            idx = 0;
        }
        if (static_cast<size_t>(idx) >= n)
        {
            idx = static_cast<long>(n) - 1;
        }
        return static_cast<float>(1000.0 / static_cast<double>(desc[static_cast<size_t>(idx)]));
    }

    /** 1000 / mean frame time over the window, 0 when empty. */
    float average_fps() const
    {
        const size_t n = size();
        if (n == 0)
        {
            return 0.f;
        }
        double sum = 0.0;
        for (size_t i = frames_.size() - n; i < frames_.size(); ++i)
        {
            sum += frames_[i];
        }
        return sum > 0.0 ? static_cast<float>(1000.0 * static_cast<double>(n) / sum) : 0.f;
    }

  private:
    static constexpr size_t kSlack = 2048;
    size_t capacity_;
    double max_frame_ms_;
    std::vector<float> frames_;
};

class FlipRate
{
  public:
    explicit FlipRate(uint64_t span_ns = 1000000000ULL) : span_ns_(span_ns)
    {
    }

    void push(uint64_t t_ns)
    {
        stamps_.push_back(t_ns);
    }

    /** Drop timestamps older than the span. */
    void trim(uint64_t now_ns)
    {
        size_t drop = 0;
        while (drop < stamps_.size() && now_ns - stamps_[drop] > span_ns_)
        {
            ++drop;
        }
        if (drop)
        {
            stamps_.erase(stamps_.begin(), stamps_.begin() + static_cast<std::ptrdiff_t>(drop));
        }
    }

    /** Frame rate over the kept timestamps; false when fewer than two flips. */
    bool fps(float* out) const
    {
        if (stamps_.size() < 2 || stamps_.back() <= stamps_.front())
        {
            return false;
        }
        const double span = static_cast<double>(stamps_.back() - stamps_.front());
        *out = static_cast<float>(static_cast<double>(stamps_.size() - 1) * 1e9 / span);
        return true;
    }

  private:
    uint64_t span_ns_;
    std::vector<uint64_t> stamps_;
};

/**
 * FPS and 1% / 0.1% lows from the display flip timestamps (kernel clock,
 * microseconds). The first kWarmupUs of a run (loading) feed the FPS but
 * not the lows; the lows are re-sorted at most once a second.
 */
class FlipFrameStats
{
  public:
    static constexpr uint64_t kWarmupUs = 15ULL * 1000000ULL;
    static constexpr uint64_t kSecondNs = 1000000000ULL;

    struct Reading
    {
        bool frames = false; /* a rate was measured within the last second */
        float fps = 0.f;
        float low1 = -1.f;
        float low01 = -1.f;
    };

    void reset()
    {
        *this = FlipFrameStats();
    }

    /** Copies the per-frame graph ring out as oldest..newest (ms*10, 0-padded). */
    void copy_graph_ft10(uint16_t* out, size_t n) const
    {
        for (size_t i = 0; i < n; ++i)
        {
            out[i] = 0;
        }
        const size_t count = graph_count_ < kGraphN ? graph_count_ : kGraphN;
        const size_t start = graph_count_ < kGraphN ? 0 : graph_pos_;
        for (size_t i = 0; i < count; ++i)
        {
            out[n - count + i] = graph_ft10_[(start + i) % kGraphN];
        }
    }

    /** `new_us`: flip times since the last call; `mono_ns`: the daemon's monotonic clock. */
    Reading update(const std::vector<uint64_t>& new_us, uint64_t mono_ns)
    {
        for (const uint64_t t : new_us)
        {
            if (first_us_ == 0)
            {
                first_us_ = t;
            }
            if (prev_us_ != 0 && t > prev_us_ && t - first_us_ >= kWarmupUs)
            {
                const double ms = static_cast<double>(t - prev_us_) / 1000.0;
                window_.push(ms);
                /* Same cut as FrameWindow (500 ms): loading spikes stay off the graph. */
                if (ms <= 500.0)
                {
                    uint16_t v = static_cast<uint16_t>(ms * 10.0 + 0.5);
                    if (v == 0)
                    {
                        v = 1;
                    }
                    graph_ft10_[graph_pos_] = v;
                    graph_pos_ = (graph_pos_ + 1) % kGraphN;
                    if (graph_count_ < kGraphN)
                    {
                        ++graph_count_;
                    }
                }
            }
            prev_us_ = t;
            rate_.push(t * 1000ULL); /* ns, flip clock */
        }
        if (!new_us.empty())
        {
            last_advance_ns_ = mono_ns;
        }
        if (prev_us_ != 0)
        {
            rate_.trim(prev_us_ * 1000ULL);
        }

        Reading r;
        r.frames = last_advance_ns_ != 0 && mono_ns - last_advance_ns_ < kSecondNs && rate_.fps(&r.fps);

        if (mono_ns - lows_ns_ >= kSecondNs)
        { /* sorting once a second */
            lows_ns_ = mono_ns;
            low1_ = window_.low(0.01, 100);
            low01_ = window_.low(0.001, 1000);
        }
        r.low1 = low1_;
        r.low01 = low01_;
        return r;
    }

  private:
    static constexpr size_t kGraphN = 96; /* ORANGE_HUD_FT_N; kept in sync by a contract test */
    uint16_t graph_ft10_[kGraphN] = {};
    size_t graph_pos_ = 0;
    size_t graph_count_ = 0;

    FrameWindow window_;
    FlipRate rate_;
    uint64_t prev_us_ = 0;
    uint64_t first_us_ = 0;
    uint64_t last_advance_ns_ = 0;
    uint64_t lows_ns_ = 0;
    float low1_ = -1.f;
    float low01_ = -1.f;
};

} // namespace fps
} // namespace orange
