#pragma once

// Standard library
#include <mutex>

// Project
#include "sample.h"

namespace orange
{
namespace fps
{

/** Latest FPS reading shared between the sampler thread and the HUD thread. */
class LatestSample
{
  public:
    /** Called by the samplers with their newest reading. */
    void publish(const OrangeFpsSample& sample);

    /**
     * Newest reading that is valid and fresh (ORANGE_FPS_STALE_NS). Returns
     * false when no usable sample exists.
     */
    bool latest(OrangeFpsSample* out) const;

  private:
    mutable std::mutex mutex_;
    OrangeFpsSample latest_{};
};

/** The process-wide instance; publish() / latest() forward to it. */
LatestSample& latest_sample();

void publish(const OrangeFpsSample& sample);
bool latest(OrangeFpsSample* out);

} // namespace fps
} // namespace orange
