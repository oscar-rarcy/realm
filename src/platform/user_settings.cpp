#include "user_settings.h"

#include "realm.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

#if defined(REALM_WEB)
#include <emscripten/emscripten.h>
#else
#include <filesystem>
#include <system_error>
#endif

namespace {

constexpr const char* SETTINGS_MAGIC = "REALM_SETTINGS";
constexpr int SETTINGS_VERSION = 1;

bool readHueValue(std::istream& in, int& hue) {
    int raw = 0;
    if (!(in >> raw)) return false;
    hue = normalizePlayerColorHue(raw);
    return true;
}

bool parseBoolText(const std::string& text, bool fallback) {
    std::string value = text;
    for (char& ch : value) ch = (char)std::tolower((unsigned char)ch);
    if (value == "1" || value == "true" || value == "yes" || value == "on" || value == "square") return true;
    if (value == "0" || value == "false" || value == "no" || value == "off" || value == "terminal") return false;
    return fallback;
}

bool readBoolValue(std::istream& in, bool fallback) {
    std::string raw;
    if (!(in >> raw)) return fallback;
    return parseBoolText(raw, fallback);
}

#if !defined(REALM_WEB)
std::filesystem::path settingsPath() {
    if (const char* overridePath = std::getenv("REALM_SETTINGS_PATH")) {
        if (*overridePath) return std::filesystem::path(overridePath);
    }

#if defined(_WIN32)
    const char* root = std::getenv("APPDATA");
    if (!root || !*root) root = std::getenv("LOCALAPPDATA");
    if (!root || !*root) root = std::getenv("USERPROFILE");
    if (root && *root) return std::filesystem::path(root) / "Realm" / "settings.txt";
#else
    if (const char* xdg = std::getenv("XDG_CONFIG_HOME")) {
        if (*xdg) return std::filesystem::path(xdg) / "realm" / "settings.txt";
    }
    if (const char* home = std::getenv("HOME")) {
        if (*home) return std::filesystem::path(home) / ".config" / "realm" / "settings.txt";
    }
#endif

    return std::filesystem::path(".realm-settings.txt");
}
#endif

} // namespace

UserSettings defaultUserSettings() {
    return UserSettings{};
}

UserSettings loadUserSettings() {
    UserSettings settings = defaultUserSettings();

#if defined(REALM_WEB)
    int storedHue = EM_ASM_INT({
        if (typeof window === 'undefined') return -1;
        try {
            var raw = window.localStorage && window.localStorage.getItem('realm.settings.v1');
            if (!raw) return -1;
            var parsed = JSON.parse(raw);
            var hue = Number(parsed && parsed.playerColorHue);
            var square = parsed && parsed.asciiSquareMapCells ? 1 : 0;
            return Number.isFinite(hue) ? (Math.round(hue) * 2 + square) : -1;
        } catch (error) {
            return -1;
        }
    });
    if (storedHue >= 0) {
        settings.asciiSquareMapCells = (storedHue & 1) != 0;
        settings.playerColorHue = normalizePlayerColorHue(storedHue / 2);
    }
#else
    std::ifstream in(settingsPath());
    if (!in) return settings;

    std::string tag;
    while (in >> tag) {
        if (tag == SETTINGS_MAGIC) {
            int version = 0;
            in >> version;
            (void)version;
            continue;
        }
        if (tag == "player_color_hue" || tag == "playerColorHue") {
            int hue = settings.playerColorHue;
            if (readHueValue(in, hue)) settings.playerColorHue = hue;
            continue;
        }
        if (tag == "ascii_square_map_cells" || tag == "asciiSquareMapCells") {
            settings.asciiSquareMapCells = readBoolValue(in, settings.asciiSquareMapCells);
            continue;
        }
        std::string ignored;
        std::getline(in, ignored);
    }
#endif

    return settings;
}

bool saveUserSettings(const UserSettings& settings) {
#if defined(REALM_WEB)
    EM_ASM({
        if (typeof window === 'undefined') return;
        try {
            var hue = $0;
            var square = $1 ? true : false;
            var settings = { version: 1, playerColorHue: hue, asciiSquareMapCells: square };
            if (window.localStorage) {
                window.localStorage.setItem('realm.settings.v1', JSON.stringify(settings));
            }
        } catch (error) {}
    }, normalizePlayerColorHue(settings.playerColorHue), settings.asciiSquareMapCells ? 1 : 0);
    return true;
#else
    std::filesystem::path finalPath = settingsPath();
    std::error_code ec;
    if (finalPath.has_parent_path()) {
        std::filesystem::create_directories(finalPath.parent_path(), ec);
        if (ec) return false;
    }

    std::filesystem::path tmpPath = finalPath;
    tmpPath.replace_extension(tmpPath.extension().string() + ".tmp");

    std::ofstream out(tmpPath, std::ios::trunc);
    if (!out) return false;
    out << SETTINGS_MAGIC << ' ' << SETTINGS_VERSION << "\n";
    out << "player_color_hue " << normalizePlayerColorHue(settings.playerColorHue) << "\n";
    out << "ascii_square_map_cells " << (settings.asciiSquareMapCells ? 1 : 0) << "\n";
    out.close();
    if (!out) return false;

    ec = {};
    if (std::filesystem::exists(finalPath, ec)) {
        std::filesystem::remove(finalPath, ec);
        if (ec) return false;
    }
    ec = {};
    std::filesystem::rename(tmpPath, finalPath, ec);
    return !ec;
#endif
}

void applyUserSettingsToGame(Game& game, const UserSettings& settings) {
    setHumanPlayerColorHue(game, settings.playerColorHue);
    configurePlayerColorHues(game, game.startupAIs);
}

UserSettings userSettingsFromGame(const Game& game) {
    UserSettings settings = loadUserSettings();
    settings.playerColorHue = normalizePlayerColorHue(game.playerColorHue[0]);
    return settings;
}

bool saveUserSettingsFromGame(const Game& game) {
    return saveUserSettings(userSettingsFromGame(game));
}
