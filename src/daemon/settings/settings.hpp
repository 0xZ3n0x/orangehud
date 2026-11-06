#pragma once

// ---------------------------------------------------------------------------
// Product config schema (single INI file, /data/OrangeHUD/config.ini).
//
// Daemon-only: the ShellUI module never reads this file; it receives the
// overlay part through the HUD frame (orange/hud_frame.h).
//
// Force C++ linkage even when a parent includes us under extern "C"
// (e.g. when a C header includes us inside an extern "C" block).
// ---------------------------------------------------------------------------

#ifdef __cplusplus
extern "C++"
{

// Standard library
#include <array>
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <string>
#include <utility>

// Project
#include "contract/overlay_defs.hpp"

    namespace orange
    {

    // Canonical filesystem paths.
    inline constexpr const char* kConfigPath = "/data/OrangeHUD/config.ini";

    // Semantic config schema. This schema starts at version 1.
    inline constexpr int kSettingsSchemaVersion = 1;
    inline constexpr int kRestModeDelayMinSeconds = 0;
    inline constexpr int kRestModeDelayMaxSeconds = 255;
    inline constexpr int kRestModeDelayDefaultSeconds = 10;

    struct Settings
    {
        // [rest_mode]
        // Extra seconds after Rest Mode trophy SPRX are ready, before re-injecting.
        uint64_t rest_mode_delay_seconds = kRestModeDelayDefaultSeconds;

        // [overlay]
        /** Master visibility switch for the whole HUD panel. */
        bool overlay_enabled = true;
        /** Show the translucent box behind the HUD panel. */
        bool overlay_background = true;
        bool overlay_ram = true;
        bool overlay_cpu = true;
        bool overlay_gpu = true;
        bool overlay_fps = true;
        /** 1% / 0.1% low FPS next to the FPS value. */
        bool overlay_fps_lows = true;
        /** Per-core CPU usage mode on the overlay (id_all_cpu_usage). */
        bool all_cpu_usage = true;
        /** Display order of the metrics; values are kOverlayMetric*. */
        std::array<int, kOverlayMetricCount> overlay_order = kOverlayOrderDefault;

        // Meta
        int schema_version = kSettingsSchemaVersion;
    };

    // Fill `out` with defaults then overlay values from the config file.
    // Returns true if a file was loaded; false means defaults only (file missing).
    bool settings_load(Settings* out);

    // Load an explicit path (host tests and tooling).
    bool settings_load_file(const char* path, Settings* out);

    // Serialize full schema to annotated INI text (no I/O).
    std::string settings_serialize(const Settings& in);

    // Serialize full schema. Writes every path that can be opened for write.
    // Returns true if at least one write succeeded.
    bool settings_save(const Settings& in);

    // Create default full-schema INI if no config exists on any path.
    // Returns true if a file was created (or already existed after load attempt).
    bool settings_ensure_default();

    // Path that was last successfully loaded (empty if defaults only).
    const char* settings_last_loaded_path();

    /** st_mtime of the config file (0 if it does not exist); for reload gating. */
    time_t settings_config_newest_mtime();

    /** True if the file's mtime is strictly greater than `since`. */
    bool settings_config_is_newer_than(time_t since);

    } // namespace orange

} // extern "C++"
#endif /* __cplusplus */
