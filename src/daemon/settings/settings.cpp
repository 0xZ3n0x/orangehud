#include "settings.hpp"

// Standard library
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

// SDK
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace orange
{
namespace
{

const char* g_last_loaded = "";

// ---------------------------------------------------------------------------
// INI reading: "[section]" headers and "key=value" lines, stored as
// "section.key". '#' and ';' start comment lines; the first duplicate wins.
// ---------------------------------------------------------------------------

using IniMap = std::map<std::string, std::string>;

constexpr size_t kMaxIniBytes = 64 * 1024;

std::string trim(const std::string& s)
{
    size_t first = 0;
    while (first < s.size() && std::isspace(static_cast<unsigned char>(s[first])))
    {
        ++first;
    }
    size_t last = s.size();
    while (last > first && std::isspace(static_cast<unsigned char>(s[last - 1])))
    {
        --last;
    }
    return s.substr(first, last - first);
}

bool read_ini(const char* path, IniMap* out)
{
    const int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        return false;
    }
    std::string text;
    char chunk[1024];
    ssize_t n;
    while ((n = read(fd, chunk, sizeof(chunk))) > 0 && text.size() < kMaxIniBytes)
    {
        text.append(chunk, static_cast<size_t>(n));
    }
    close(fd);

    std::string section;
    size_t pos = 0;
    while (pos <= text.size())
    {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos)
        {
            eol = text.size();
        }
        const std::string line = trim(text.substr(pos, eol - pos));
        pos = eol + 1;

        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }
        if (line.front() == '[' && line.back() == ']')
        {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }
        out->emplace(trim(section + "." + line.substr(0, eq)), trim(line.substr(eq + 1)));
    }
    return true;
}

// ---------------------------------------------------------------------------
// Value parsing / naming
// ---------------------------------------------------------------------------

bool path_exists(const char* path)
{
    struct stat st{};
    return path && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool streq_ci(const char* a, const char* b)
{
    if (!a || !b)
    {
        return false;
    }
    while (*a && *b)
    {
        if (std::tolower(static_cast<unsigned char>(*a)) != std::tolower(static_cast<unsigned char>(*b)))
        {
            return false;
        }
        ++a;
        ++b;
    }
    return *a == '\0' && *b == '\0';
}

bool parse_bool(const char* s, bool def)
{
    if (streq_ci(s, "true") || streq_ci(s, "yes") || streq_ci(s, "on") || std::strcmp(s, "1") == 0)
    {
        return true;
    }
    if (streq_ci(s, "false") || streq_ci(s, "no") || streq_ci(s, "off") || std::strcmp(s, "0") == 0)
    {
        return false;
    }
    return def;
}

std::string bool_text(bool value)
{
    return value ? "true" : "false";
}

struct EnumName
{
    int value;
    const char* name;
};

template <size_t N> int parse_enum(const char* s, const EnumName (&names)[N], int def)
{
    for (const EnumName& e : names)
    {
        if (streq_ci(s, e.name))
        {
            return e.value;
        }
    }
    return def;
}

template <size_t N> const char* enum_name(int value, const EnumName (&names)[N])
{
    for (const EnumName& e : names)
    {
        if (e.value == value)
        {
            return e.name;
        }
    }
    return names[0].name;
}

constexpr EnumName kCpuModeNames[] = {{0, "average"}, {1, "per_core"}};
// Names come from overlay_metric_name().
constexpr EnumName kMetricTokens[] = {{kOverlayMetricFps, overlay_metric_name(kOverlayMetricFps)},
                                      {kOverlayMetricCpu, overlay_metric_name(kOverlayMetricCpu)},
                                      {kOverlayMetricGpu, overlay_metric_name(kOverlayMetricGpu)},
                                      {kOverlayMetricMemory, overlay_metric_name(kOverlayMetricMemory)}};

void parse_overlay_order(const char* s, std::array<int, kOverlayMetricCount>* out)
{
    std::array<int, kOverlayMetricCount> parsed{};
    bool seen[kOverlayMetricCount] = {};
    int n = 0;
    const std::string input(s);
    size_t start = 0;
    while (start <= input.size())
    {
        const size_t comma = input.find(',', start);
        const std::string token = trim(input.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        const int metric = parse_enum(token.c_str(), kMetricTokens, -1);
        if (metric >= 0 && !seen[metric] && n < kOverlayMetricCount)
        {
            seen[metric] = true;
            parsed[n++] = metric;
        }
        if (comma == std::string::npos)
        {
            break;
        }
        start = comma + 1;
    }
    overlay_normalize_order(parsed);
    *out = parsed;
}

std::string serialize_overlay_order(const std::array<int, kOverlayMetricCount>& order)
{
    auto normalized = order;
    overlay_normalize_order(normalized);
    std::string out;
    for (int i = 0; i < kOverlayMetricCount; ++i)
    {
        if (i != 0)
        {
            out += ',';
        }
        out += overlay_metric_name(normalized[i]);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Schema: one row per config key. Adding a setting = a Settings field plus a
// row here; parsing, defaults and the annotated file all come from this table.
// ---------------------------------------------------------------------------

struct Key
{
    const char* section;
    const char* name;
    const char* help; // '\n'-separated lines, written as "# " comments
    void (*parse)(const char* text, Settings& s);
    std::string (*format)(const Settings& s);
};

template <bool Settings::* M> void parse_bool_field(const char* t, Settings& s)
{
    s.*M = parse_bool(t, s.*M);
}
template <bool Settings::* M> std::string format_bool_field(const Settings& s)
{
    return bool_text(s.*M);
}

#define BOOL_KEY(section, name, field, help) {section, name, help, parse_bool_field<&Settings::field>, format_bool_field<&Settings::field>}

const Key kKeys[] = {
    {"meta", "schema_version", "Config format version; a file with another value is ignored.", nullptr, [](const Settings&) { return std::to_string(kSettingsSchemaVersion); }},

    {"rest_mode", "resume_reinject_delay_seconds", "Seconds to wait before re-injecting the HUD after Rest Mode (0-255).",
     [](const char* t, Settings& s) {
         const int v = std::atoi(t);
         if (v >= kRestModeDelayMinSeconds && v <= kRestModeDelayMaxSeconds)
         {
             s.rest_mode_delay_seconds = static_cast<uint64_t>(v);
         }
     },
     [](const Settings& s) { return std::to_string(static_cast<unsigned long long>(s.rest_mode_delay_seconds)); }},

    BOOL_KEY("overlay", "enabled", overlay_enabled, "Show the HUD (true, false)."),
    BOOL_KEY("overlay", "background", overlay_background, "Translucent box behind the panel (true, false)."),
    BOOL_KEY("overlay", "show_cpu", overlay_cpu, "CPU temperature and usage (true, false). Ignored while cpu_usage_mode is per_core."),
    {"overlay", "cpu_usage_mode", "CPU usage as one average or per core (average, per_core).",
     [](const char* t, Settings& s) { s.all_cpu_usage = parse_enum(t, kCpuModeNames, s.all_cpu_usage ? 1 : 0) == 1; },
     [](const Settings& s) { return std::string(enum_name(s.all_cpu_usage ? 1 : 0, kCpuModeNames)); }},
    BOOL_KEY("overlay", "show_soc", overlay_gpu, "Hottest SoC sensor and its power (true, false)."),
    BOOL_KEY("overlay", "show_fps", overlay_fps, "In-game frame rate (true, false)."),
    BOOL_KEY("overlay", "show_fps_lows", overlay_fps_lows, "1% and 0.1% low FPS next to the frame rate (true, false)."),
    BOOL_KEY("overlay", "show_memory", overlay_ram, "Memory the game has mapped, against its budget (true, false)."),
    {"overlay", "order", "Row order, comma-separated (fps, cpu, gpu, memory).", [](const char* t, Settings& s) { parse_overlay_order(t, &s.overlay_order); },
     [](const Settings& s) { return serialize_overlay_order(s.overlay_order); }},
};

#undef BOOL_KEY

bool apply_ini(const IniMap& ini, Settings* out)
{
    const auto version = ini.find("meta.schema_version");
    if (version == ini.end() || std::atoi(version->second.c_str()) != kSettingsSchemaVersion)
    {
        return false;
    }
    out->schema_version = kSettingsSchemaVersion;

    for (const Key& key : kKeys)
    {
        if (!key.parse)
        {
            continue;
        }
        const auto it = ini.find(std::string(key.section) + "." + key.name);
        if (it != ini.end())
        {
            key.parse(it->second.c_str(), *out);
        }
    }
    return true;
}

bool write_path(const char* path, const std::string& body)
{
    if (!path)
    {
        return false;
    }
    const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
    if (fd < 0)
    {
        return false;
    }
    const ssize_t n = write(fd, body.data(), body.size());
    close(fd);
    return n == static_cast<ssize_t>(body.size());
}

bool try_load_path(const char* path, Settings* out)
{
    if (!path_exists(path))
    {
        return false;
    }
    IniMap ini;
    if (!read_ini(path, &ini))
    {
        return false;
    }
    Settings parsed = *out;
    if (!apply_ini(ini, &parsed))
    {
        return false;
    }
    *out = parsed;
    g_last_loaded = path;
    return true;
}

} // namespace

std::string settings_serialize(const Settings& in)
{
    std::string b;
    b.reserve(4096);
    b += "# OrangeHUD configuration. Changes apply within a second.\n"
         "# Do not put comments after a value.\n";

    const char* section = nullptr;
    for (const Key& key : kKeys)
    {
        if (!section || std::strcmp(section, key.section) != 0)
        {
            section = key.section;
            b += "\n[";
            b += section;
            b += "]\n";
        }
        if (!key.format)
        {
            continue; // read-only alias
        }
        const char* line = key.help;
        while (*line)
        {
            const char* eol = std::strchr(line, '\n');
            const size_t len = eol ? static_cast<size_t>(eol - line) : std::strlen(line);
            b += "# ";
            b.append(line, len);
            b += '\n';
            line += len + (eol ? 1 : 0);
        }
        b += key.name;
        b += '=';
        b += key.format(in);
        b += '\n';
    }
    return b;
}

bool settings_load_file(const char* path, Settings* out)
{
    if (!out || !path)
    {
        return false;
    }
    *out = Settings{};
    return try_load_path(path, out);
}

bool settings_load(Settings* out)
{
    if (!out)
    {
        return false;
    }
    *out = Settings{}; // defaults
    g_last_loaded = "";
    return try_load_path(kConfigPath, out);
}

bool settings_save(const Settings& in)
{
    return write_path(kConfigPath, settings_serialize(in));
}

bool settings_ensure_default()
{
    if (path_exists(kConfigPath))
    {
        return true;
    }
    return settings_save(Settings{});
}

const char* settings_last_loaded_path()
{
    return g_last_loaded;
}

time_t settings_config_newest_mtime()
{
    struct stat st{};
    if (stat(kConfigPath, &st) != 0 || !S_ISREG(st.st_mode))
    {
        return 0;
    }
    return st.st_mtime;
}

bool settings_config_is_newer_than(time_t since)
{
    const time_t newest = settings_config_newest_mtime();
    return newest != 0 && newest > since;
}

} // namespace orange
