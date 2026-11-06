#include "publish.hpp"

// Project
#include "platform/clock.h"

namespace orange
{
namespace fps
{

void LatestSample::publish(const OrangeFpsSample& sample)
{
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = sample;
}

bool LatestSample::latest(OrangeFpsSample* out) const
{
    if (!out)
    {
        return false;
    }
    OrangeFpsSample s;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        s = latest_;
    }
    const uint64_t now = realtime_ns();
    if (!s.valid || s.unix_ns == 0 || now < s.unix_ns || now - s.unix_ns > ORANGE_FPS_STALE_NS)
    {
        return false;
    }
    *out = s;
    return true;
}

LatestSample& latest_sample()
{
    static LatestSample instance;
    return instance;
}

void publish(const OrangeFpsSample& sample)
{
    latest_sample().publish(sample);
}

bool latest(OrangeFpsSample* out)
{
    return latest_sample().latest(out);
}

} // namespace fps
} // namespace orange
