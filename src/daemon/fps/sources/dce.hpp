/* /dev/dce flip status: the display's last flip (counter and time).
 * Follows PHU Games Tools by ArkSama (https://github.com/ArkSama).
 */
#pragma once

// Standard library
#include <cstdint>

namespace orange
{
namespace fps
{

enum class DceSampleStatus : uint8_t
{
    NotAttempted,
    Ok,
    InvalidArgument,
    Unavailable,
    OpenFailed,
    IoctlFailed,
};

class DceSource
{
  public:
    DceSource() = default;
    DceSource(const DceSource&) = delete;
    DceSource& operator=(const DceSource&) = delete;
    ~DceSource();

    /** Open /dev/dce. False if the node is missing or MAC-denied. */
    bool open();
    void close();
    /** Session-level fail: do not retry open every tick. */
    bool unavailable() const
    {
        return unavailable_;
    }
    const char* request_abi_name() const;

    /** Read the flip status: `raw` (kDceAnswerBytes) gets the whole ioctl answer. False on ioctl error. */
    bool sample(unsigned char* raw, DceSampleStatus* status = nullptr);

  private:
    int fd_ = -1;
    uint8_t request_abi_ = 0;
    bool unavailable_ = false;
    bool logged_fail_ = false;
    bool logged_abi_ = false;
};

} // namespace fps
} // namespace orange
