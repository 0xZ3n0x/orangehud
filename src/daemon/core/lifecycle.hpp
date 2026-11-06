/* Winding the daemon down: the worker threads poll shutting_down() and exit,
 * and the HUD publisher says when its last (disabled) frame is out so main()
 * can leave without the panel lingering on screen.
 */
#pragma once

/**
 * Ask every worker thread to finish. One-way (never cleared) and safe to call
 * from a signal handler.
 */
void request_shutdown();

/** True once request_shutdown() was called. */
bool shutting_down();

/** Called by the HUD publisher once its last (disabled) frame is out. */
void set_hud_unpublished();

bool hud_unpublished();
