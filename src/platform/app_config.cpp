#include "realm.h"
#include "env_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <locale.h>
#include <string>
#include <unordered_map>
#if defined(_WIN32)
#include <windows.h>
#endif

#ifndef REALM_VISUAL_MODE_DEFAULT
#define REALM_VISUAL_MODE_DEFAULT "ascii-only"
#endif

#ifndef REALM_ENABLE_TILESET
#define REALM_ENABLE_TILESET 1
#endif

namespace {

std::string trim(std::string value) {
    auto notSpace = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), notSpace));
    value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(), value.end());
    return value;
}

std::string unquote(std::string value) {
    value = trim(value);
    if (value.size() >= 2) {
        char first = value.front();
        char last = value.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            value = value.substr(1, value.size() - 2);
        }
    }
    return value;
}

void readEnvironmentFile(const char* path, std::unordered_map<std::string, std::string>& values) {
    std::ifstream in(path);
    if (!in) return;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;
        if (line.rfind("export ", 0) == 0) line = trim(line.substr(7));
        std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;

        std::string key = trim(line.substr(0, equals));
        std::string value = line.substr(equals + 1);
        if (key.empty()) continue;
        if (!value.empty() && value.front() != '"' && value.front() != '\'') {
            std::size_t comment = value.find('#');
            if (comment != std::string::npos) value = value.substr(0, comment);
        }
        values[key] = unquote(value);
    }
}

void setEnvironmentIfUnset(const std::string& key, const std::string& value) {
    if (key.empty() || std::getenv(key.c_str())) return;
#if defined(_WIN32)
    _putenv_s(key.c_str(), value.c_str());
#else
    setenv(key.c_str(), value.c_str(), 0);
#endif
}

std::string normalizedVisualMode() {
    std::string mode = realmVisualMode();
    std::transform(mode.begin(), mode.end(), mode.begin(), [](unsigned char ch) {
        if (ch == '_') return '-';
        return (char)std::tolower(ch);
    });
    return mode;
}

} // namespace

static bool isUtf8LocaleName(const char* name) {
    if (!name) return false;
    return std::strstr(name, "UTF-8") || std::strstr(name, "utf8")
        || std::strstr(name, "UTF8")  || std::strstr(name, "65001");
}

void forceUtf8Locale() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    const char* loc = setlocale(LC_ALL, "");
    if (isUtf8LocaleName(loc)) return;
    loc = setlocale(LC_ALL, "C.UTF-8");
    if (isUtf8LocaleName(loc)) return;
    loc = setlocale(LC_ALL, "en_US.UTF-8");
    if (isUtf8LocaleName(loc)) return;
    loc = setlocale(LC_ALL, ".UTF-8");
    if (isUtf8LocaleName(loc)) return;
    setlocale(LC_ALL, ".UTF8");
}

int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)parsed : fallback;
}

unsigned envUnsigned(const char* name, unsigned fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    unsigned long parsed = std::strtoul(v, &end, 10);
    return (end && *end == '\0') ? (unsigned)parsed : fallback;
}

void loadRealmEnvironmentFiles() {
    std::unordered_map<std::string, std::string> values;
    readEnvironmentFile("../.env", values);
    readEnvironmentFile(".env", values);
    readEnvironmentFile("../.env.local", values);
    readEnvironmentFile(".env.local", values);
    for (const auto& entry : values) {
        setEnvironmentIfUnset(entry.first, entry.second);
    }
}

std::string realmVisualMode() {
    const char* value = std::getenv("REALM_VISUAL_MODE");
    if (value && *value) return value;
    return REALM_VISUAL_MODE_DEFAULT;
}

bool realmVisualModeIsAsciiOnly() {
#if !REALM_ENABLE_TILESET
    return true;
#else
    std::string mode = normalizedVisualMode();
    return mode == "ascii-only" || mode == "ascii" || mode == "terminal" || mode == "console";
#endif
}

bool realmVisualModeAllowsTilesetMenu() {
#if !REALM_ENABLE_TILESET
    return false;
#else
    std::string mode = normalizedVisualMode();
    return mode == "tileset-menu" || mode == "tileset" || mode == "selectable" || mode == "gui";
#endif
}
