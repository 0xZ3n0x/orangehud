#pragma once
/* The remote injector holds process-wide state (the attach and the elevated
 * authid): every inject_elf() call must hold this lock. */
// Standard library
#include <mutex>

inline std::mutex& inject_mutex()
{
    static std::mutex m;
    return m;
}
