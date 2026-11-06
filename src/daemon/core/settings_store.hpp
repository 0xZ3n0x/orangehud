/* Process-local copy of config.ini, readable from any daemon thread.
 */
#pragma once

// Standard library
#include <ctime>
#include <mutex>

// Project
#include "settings/settings.hpp"

/**
 * Process-local copy of config.ini: reloaded when the file is newer, readable
 * from any thread.
 */
class SettingsStore
{
  public:
    /** Copy of the settings last applied by load(); safe from any thread. */
    orange::Settings current() const;

    /**
     * Refresh from config.ini when it is newer. Set force to bypass the mtime
     * cache. Returns true if the store is usable (including defaults /
     * skip-if-current). A missing config is not an error.
     */
    bool load(bool force = false);

  private:
    mutable std::mutex mutex_;
    orange::Settings settings_;
    time_t last_modified_ = 0; /* config mtime at the last successful update */
    bool ever_loaded_ = false;
};

/** The process-wide settings store. */
SettingsStore& settings_store();

/** Shorthand for settings_store().current(). */
orange::Settings current_settings();

/** Shorthand for settings_store().load(force). */
bool LoadSettings(bool force = false);
