/* FPS sampler thread (entry point; see fps_sampler.cpp). */
#pragma once

/** Polls the display flip times from /dev/dce and publishes the FPS sample. */
void* fps_sampler_thread(void* args) noexcept;
