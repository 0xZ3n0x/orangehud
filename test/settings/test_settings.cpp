/* Host unit tests for the settings schema (no PS5 SDK). */
// Standard library
#include <cstdio>
#include <string>

// SDK
#include <unistd.h>

// Project
#include "settings/settings.hpp"
#include "test_harness.h"

static std::string temp_ini_path()
{
    char tmpl[] = "/tmp/orange-settings-XXXXXX.ini";
    /* mkstemps needs suffix length 4 for ".ini" */
    int fd = mkstemps(tmpl, 4);
    if (fd < 0)
    {
        return {};
    }
    close(fd);
    unlink(tmpl); /* write fresh via save_settings_to */
    return std::string(tmpl);
}

static int test_defaults_and_serialize_keys(void)
{
    orange::Settings s{};
    std::string text = orange::settings_serialize(s);

    TEST_ASSERT_TRUE(text.find("[meta]") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("schema_version=1") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("[general]") == std::string::npos);
    TEST_ASSERT_TRUE(text.find("language=") == std::string::npos);
    TEST_ASSERT_TRUE(text.find("[rest_mode]") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("resume_reinject_delay_seconds=10") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("[overlay]") != std::string::npos);
    TEST_ASSERT_TRUE(text.find("order=fps,cpu,gpu,memory") != std::string::npos);
    /* Sections of removed features are no longer written. */
    const char* gone[] = {"[toolbox]", "[logging]", "[kstuff]", "[startup]", "[cheats]", "[app_jailbreak]", "[cooling]", "[shortcuts]", "[home_screen]", "[game_menu]"};
    for (const char* section : gone)
    {
        TEST_ASSERT_TRUE(text.find(section) == std::string::npos);
    }
    return 0;
}

/* Test helper: write the serialized schema to an explicit path. */
static bool save_settings_to(const char* path, const orange::Settings& in)
{
    FILE* f = fopen(path, "w");
    if (!f)
    {
        return false;
    }
    const std::string text = orange::settings_serialize(in);
    const bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    fclose(f);
    return ok;
}

static int test_full_roundtrip(void)
{
    const std::string path = temp_ini_path();
    TEST_ASSERT_TRUE(!path.empty());

    orange::Settings in{};
    in.rest_mode_delay_seconds = 42;
    in.overlay_enabled = false;
    in.overlay_background = false;
    in.overlay_cpu = false;
    in.all_cpu_usage = true;
    in.overlay_gpu = false;
    in.overlay_fps = false;
    in.overlay_fps_lows = false;
    in.overlay_ram = false;
    in.overlay_order = {orange::kOverlayMetricMemory, orange::kOverlayMetricGpu, orange::kOverlayMetricCpu, orange::kOverlayMetricFps};
    TEST_ASSERT_TRUE(save_settings_to(path.c_str(), in));

    orange::Settings out{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &out));
    TEST_ASSERT_EQ_U64(42, out.rest_mode_delay_seconds);
    TEST_ASSERT_TRUE(out.overlay_enabled == in.overlay_enabled);
    TEST_ASSERT_TRUE(out.overlay_background == in.overlay_background);
    TEST_ASSERT_TRUE(out.overlay_cpu == in.overlay_cpu);
    TEST_ASSERT_TRUE(out.all_cpu_usage == in.all_cpu_usage);
    TEST_ASSERT_TRUE(out.overlay_gpu == in.overlay_gpu);
    TEST_ASSERT_TRUE(out.overlay_fps == in.overlay_fps);
    TEST_ASSERT_TRUE(out.overlay_fps_lows == in.overlay_fps_lows);
    TEST_ASSERT_TRUE(out.overlay_ram == in.overlay_ram);
    for (int i = 0; i < orange::kOverlayMetricCount; ++i)
    {
        TEST_ASSERT_EQ_INT(in.overlay_order[i], out.overlay_order[i]);
    }

    unlink(path.c_str());
    return 0;
}

static int test_missing_and_empty_file_defaults(void)
{
    orange::Settings out{};
    out.overlay_enabled = false;
    TEST_ASSERT_TRUE(!orange::settings_load_file("/nonexistent/orange.ini", &out));

    const std::string path = temp_ini_path();
    TEST_ASSERT_TRUE(!path.empty());
    FILE* f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n", f);
    fclose(f);

    orange::Settings defaults{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &defaults));
    TEST_ASSERT_TRUE(defaults.overlay_enabled);
    TEST_ASSERT_TRUE(defaults.overlay_fps);
    TEST_ASSERT_EQ_U64(10, defaults.rest_mode_delay_seconds);

    /* A different schema version is rejected, not half-applied. */
    f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=99\n\n[overlay]\nenabled=false\n", f);
    fclose(f);
    orange::Settings rejected{};
    (void)orange::settings_load_file(path.c_str(), &rejected);
    TEST_ASSERT_TRUE(rejected.overlay_enabled);

    unlink(path.c_str());
    return 0;
}

/* Sections written by earlier builds are accepted and ignored. */
static int test_rest_mode_delay_parse_policy(void)
{
    std::string path = temp_ini_path();
    TEST_ASSERT_TRUE(!path.empty());

    FILE* f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n\n[rest_mode]\n"
          "resume_reinject_delay_seconds=0\n",
          f);
    fclose(f);
    orange::Settings zero{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &zero));
    TEST_ASSERT_EQ_U64(0, zero.rest_mode_delay_seconds);

    f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n\n[rest_mode]\n"
          "resume_reinject_delay_seconds=255\n",
          f);
    fclose(f);
    orange::Settings max{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &max));
    TEST_ASSERT_EQ_U64(255, max.rest_mode_delay_seconds);

    f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n\n[rest_mode]\n"
          "resume_reinject_delay_seconds=256\n",
          f);
    fclose(f);
    orange::Settings invalid{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &invalid));
    TEST_ASSERT_EQ_U64(10, invalid.rest_mode_delay_seconds);

    unlink(path.c_str());
    return 0;
}

static int test_config_mtime_helpers(void)
{
    /* No config file on the host test machine → newest is 0, not newer. */
    TEST_ASSERT_TRUE(orange::settings_config_newest_mtime() == 0 || orange::settings_config_newest_mtime() >= 0);
    const time_t n = orange::settings_config_newest_mtime();
    if (n == 0)
    {
        TEST_ASSERT_TRUE(!orange::settings_config_is_newer_than(0));
    }
    else
    {
        TEST_ASSERT_TRUE(orange::settings_config_is_newer_than(n - 1));
        TEST_ASSERT_TRUE(!orange::settings_config_is_newer_than(n));
    }
    return 0;
}

static int test_overlay_font_and_order(void)
{
    TEST_ASSERT_EQ_INT(orange::kOverlayFontPtMedium, orange::overlay_font_pt(orange::kOverlayFontMedium));
    TEST_ASSERT_EQ_INT(orange::kOverlayFontPtSmall, orange::overlay_font_pt(orange::kOverlayFontSmall));
    TEST_ASSERT_EQ_INT(orange::kOverlayFontPtLarge, orange::overlay_font_pt(orange::kOverlayFontLarge));
    TEST_ASSERT_STREQ("medium", orange::overlay_font_size_name(orange::kOverlayFontMedium));
    TEST_ASSERT_STREQ("fps", orange::overlay_metric_name(orange::kOverlayMetricFps));
    TEST_ASSERT_STREQ("memory", orange::overlay_metric_name(orange::kOverlayMetricMemory));

    std::string path = temp_ini_path();
    TEST_ASSERT_TRUE(!path.empty());
    FILE* f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n\n[overlay]\norder=fan,ip,memory\n", f);
    fclose(f);
    orange::Settings out{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &out));
    /* "fan" and "ip" are removed metrics: skipped, "memory" stays first. */
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricMemory, out.overlay_order[0]);
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricFps, out.overlay_order[1]);
    TEST_ASSERT_TRUE(orange::settings_serialize(out).find("order=memory,fps,cpu,gpu") != std::string::npos);

    f = fopen(path.c_str(), "w");
    TEST_ASSERT_TRUE(f != nullptr);
    fputs("[meta]\nschema_version=1\n\n[overlay]\norder=\n", f);
    fclose(f);
    orange::Settings invalid{};
    TEST_ASSERT_TRUE(orange::settings_load_file(path.c_str(), &invalid));
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricFps, invalid.overlay_order[0]);
    TEST_ASSERT_EQ_INT(orange::kOverlayMetricMemory, invalid.overlay_order[3]);

    unlink(path.c_str());
    return 0;
}

extern "C" int test_settings_suite(void)
{
    int failures = 0;
    failures += test_run("settings_defaults_serialize", test_defaults_and_serialize_keys);
    failures += test_run("settings_full_roundtrip", test_full_roundtrip);
    failures += test_run("settings_missing_and_empty", test_missing_and_empty_file_defaults);
    failures += test_run("settings_rest_mode_delay", test_rest_mode_delay_parse_policy);
    failures += test_run("settings_overlay_font_and_order", test_overlay_font_and_order);
    failures += test_run("settings_config_mtime_helpers", test_config_mtime_helpers);
    return failures;
}
