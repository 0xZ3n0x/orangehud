#include "lifecycle.hpp"

// Standard library
#include <atomic>

namespace
{

std::atomic_bool g_shutting_down{false};
std::atomic_bool g_hud_unpublished{false};

} // namespace

void request_shutdown()
{
    g_shutting_down.store(true, std::memory_order_release);
}

bool shutting_down()
{
    return g_shutting_down.load(std::memory_order_acquire);
}

void set_hud_unpublished()
{
    g_hud_unpublished.store(true, std::memory_order_release);
}

bool hud_unpublished()
{
    return g_hud_unpublished.load(std::memory_order_acquire);
}
