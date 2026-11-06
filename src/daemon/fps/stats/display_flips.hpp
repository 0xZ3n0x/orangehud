/* Display flip times out of the raw /dev/dce flip-status answer (0x60 bytes,
 * twelve qwords), free of any SDK dependency so the host tests can exercise it.
 *
 * Every read returns only the LAST flip: q1 is a counter that grows by one per
 * flip and q3 is that flip's time in microseconds. Polled faster than the
 * display flips, each counter step is one new flip.
 */
#pragma once

// Standard library
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace orange
{
namespace fps
{

constexpr size_t kDceAnswerBytes = 0x60;

class DisplayFlips
{
  public:
    void reset()
    {
        *this = DisplayFlips();
    }

    /** True when `raw` holds a flip not seen before; its time goes to `flip_us`. */
    bool feed(const unsigned char* raw, uint64_t* flip_us)
    {
        uint64_t count = 0;
        uint64_t us = 0;
        std::memcpy(&count, raw + kCountOff, sizeof(count));
        std::memcpy(&us, raw + kTimeOff, sizeof(us));
        if (seeded_ && count == count_)
        {
            return false;
        }
        seeded_ = true;
        count_ = count;
        if (us == 0)
        {
            return false;
        }
        *flip_us = us;
        return true;
    }

  private:
    static constexpr size_t kCountOff = 8;
    static constexpr size_t kTimeOff = 24;
    uint64_t count_ = 0;
    bool seeded_ = false;
};

} // namespace fps
} // namespace orange
