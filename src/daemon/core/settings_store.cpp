/* Process-local settings store + dual-path mtime reload gate.
 */

#include "settings_store.hpp"

// Project
#include "platform/log.h"
#include "platform/notify.h"
#include "settings/settings.hpp"

orange::Settings SettingsStore::current() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return settings_;
}

bool SettingsStore::load(bool force)
{
    const time_t newest = orange::settings_config_newest_mtime();

    // Skip disk I/O when the file is not newer than the last applied snapshot.
    if (!force && ever_loaded_ && !orange::settings_config_is_newer_than(last_modified_))
    {
        return true;
    }

    if (newest == 0)
    {
        LOG_ERROR("[Daemon] Config file not found. Creating default schema...");
        if (orange::settings_ensure_default())
        {
            notify_user(true, "OrangeHUD config created! @ /data/OrangeHUD/config.ini");
        }
    }

    LOG_INFO("[Daemon] Loading Settings from shared schema...");
    orange::Settings s{};
    const bool from_file = orange::settings_load(&s);
    if (!from_file && newest != 0)
    {
        notify_user(true, "Failed to read the settings file");
        /* Remember this mtime so the same unreadable file is neither re-read
         * nor reported on every tick; the previous settings stay in effect and
         * the next edit of the file triggers a new attempt. */
        last_modified_ = newest;
        ever_loaded_ = true;
        return false;
    }

    if (from_file)
    {
        LOG_INFO("[Daemon] Reading Settings from %s", orange::settings_last_loaded_path());
    }
    else
    {
        LOG_INFO("[Daemon] Using default settings (no config file)");
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        settings_ = s;
    }

    last_modified_ = orange::settings_config_newest_mtime();
    ever_loaded_ = true;
    return true;
}

SettingsStore& settings_store()
{
    static SettingsStore instance;
    return instance;
}

orange::Settings current_settings()
{
    return settings_store().current();
}

bool LoadSettings(bool force)
{
    return settings_store().load(force);
}
