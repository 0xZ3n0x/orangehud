/* HUD publisher thread (entry point; see publisher.cpp). */
#pragma once

/** HUD publisher: samples CPU/SoC/memory + config, publishes the frame. */
void hud_publisher_thread();
