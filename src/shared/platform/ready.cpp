#include "ready.hpp"

// Standard library
#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// SDK
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

// Project
#include "contract/system_tmp.h"

namespace orange
{
namespace
{

bool valid_name(const char* name)
{
    if (!name || !*name)
    {
        return false;
    }
    for (const char* p = name; *p; ++p)
    {
        if (*p == '/' || *p == '.' || *p == '\\')
        {
            return false;
        }
    }
    return true;
}

bool marker_path(const char* name, char* buf, size_t buflen)
{
    if (!valid_name(name) || !buf)
    {
        return false;
    }
    const int n = std::snprintf(buf, buflen, "%s/%s", ORANGE_SYSTEM_TMP_READY_ROOT, name);
    return n > 0 && static_cast<size_t>(n) < buflen;
}

/** Write via a temp file + rename so a reader never sees a partial marker. */
bool write_text(const char* path, const char* value)
{
    char tmp[300];
    const int n = std::snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(tmp))
    {
        return false;
    }
    const int fd = ::open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0)
    {
        return false;
    }
    const size_t len = std::strlen(value);
    const ssize_t written = ::write(fd, value, len);
    ::close(fd);
    if (written != static_cast<ssize_t>(len) || ::rename(tmp, path) != 0)
    {
        ::unlink(tmp);
        return false;
    }
    return true;
}

bool read_pid_file(const char* path, pid_t* pid_out, unsigned* version_out)
{
    char value[48];
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0)
    {
        return false;
    }
    const ssize_t n = ::read(fd, value, sizeof(value) - 1);
    ::close(fd);
    if (n <= 0 || n >= static_cast<ssize_t>(sizeof(value)))
    {
        return false;
    }
    value[n] = '\0';

    errno = 0;
    char* end = nullptr;
    const long parsed = std::strtol(value, &end, 10);
    if (errno != 0 || end == value || parsed <= 0 || parsed > INT_MAX)
    {
        return false;
    }
    unsigned version = 0;
    if (*end == ' ')
    {
        char* vend = nullptr;
        const unsigned long v = std::strtoul(end + 1, &vend, 10);
        if (vend == end + 1 || *vend != '\0' || v > UINT_MAX)
        {
            return false;
        }
        version = static_cast<unsigned>(v);
    }
    else if (*end != '\0')
    {
        return false;
    }
    *pid_out = static_cast<pid_t>(parsed);
    if (version_out)
    {
        *version_out = version;
    }
    return true;
}

} // namespace

bool ReadyMarker::signal(pid_t pid, unsigned version) const
{
    char path[256];
    char value[48];
    if (pid <= 0 || !marker_path(name_, path, sizeof(path)))
    {
        return false;
    }
    const int n = version == 0 ? std::snprintf(value, sizeof(value), "%ld", static_cast<long>(pid)) : std::snprintf(value, sizeof(value), "%ld %u", static_cast<long>(pid), version);
    if (n <= 0 || static_cast<size_t>(n) >= sizeof(value))
    {
        return false;
    }

    ::mkdir(ORANGE_SYSTEM_TMP_ROOT, 0777);
    ::mkdir(ORANGE_SYSTEM_TMP_READY_ROOT, 0777);
    return write_text(path, value);
}

bool ReadyMarker::read(pid_t* pid_out, unsigned* version_out) const
{
    char path[256];
    if (!pid_out || !marker_path(name_, path, sizeof(path)))
    {
        return false;
    }
    return read_pid_file(path, pid_out, version_out);
}

bool ReadyMarker::matches(pid_t pid) const
{
    pid_t published = -1;
    return pid > 0 && read(&published) && published == pid;
}

bool ReadyMarker::clear() const
{
    char path[256];
    if (!marker_path(name_, path, sizeof(path)))
    {
        return false;
    }
    ::unlink(path);
    return true;
}

bool ReadyMarker::wait(pid_t pid, int timeout_ms, int poll_ms) const
{
    if (!valid_name(name_) || pid <= 0)
    {
        return false;
    }
    if (timeout_ms < 0)
    {
        timeout_ms = 0;
    }
    if (poll_ms < 50)
    {
        poll_ms = 50;
    }

    for (int waited = 0;; waited += poll_ms)
    {
        if (matches(pid))
        {
            return true;
        }
        if (waited >= timeout_ms)
        {
            return false;
        }
        ::usleep(static_cast<useconds_t>(poll_ms) * 1000);
    }
}

} // namespace orange
