/* The running PS4/PS5 title (big app): one place that resolves its app id,
 * title id and process id, shared by the HUD publisher, the FPS samplers and
 * the flip-hook injection. */
#pragma once

// Standard library
#include <string>

// SDK
#include <sys/types.h>

struct RunningGame
{
    int app_id = -1;
    pid_t pid = -1; /* -1 while the process is not found (yet) */
    std::string title_id;
};

/**
 * Fill `out` with the running title. False when no game runs. Thread safe;
 * the process id is cached and re-resolved when the app changes or the
 * process is gone, the title id is cached per app id.
 */
bool running_game(RunningGame* out);
