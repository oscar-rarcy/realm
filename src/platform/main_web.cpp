#include "realm.h"
#include "view_state.h"
#include "gfx_renderer.h"
#include "env_config.h"
#include "commands/command.h"
#include "commands/command_runner.h"
#include "core/game_events.h"

#include <emscripten/emscripten.h>
#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

double nextTickMs = 0.0;
bool initialized = false;
bool menuReadyLogged = false;
bool asciiOnlySurface = false;
int menuAIs = 1;
int menuBiomeIdx = 7;

enum WebScreen {
    WEB_MENU,
    WEB_MATCH,
    WEB_EXITED
};

WebScreen webScreen = WEB_MENU;

int envIntOnly(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)parsed : fallback;
}

int urlInt(const char* name, int fallback) {
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location) return $1;
        var key = UTF8ToString($0);
        var read = function (source) {
            if (!source) return null;
            var text = String(source);
            if (text.charAt(0) === '#') text = text.slice(1);
            if (text.charAt(0) === '?') text = text.slice(1);
            var params = new URLSearchParams(text);
            return params.has(key) ? params.get(key) : null;
        };
        var raw = read(window.location.search);
        if (raw === null) raw = read(window.location.hash);
        if (raw === null || String(raw).length === 0) return $1;
        var parsed = Number.parseInt(raw, 10);
        return Number.isFinite(parsed) ? parsed : $1;
    }, name, fallback);
}

int settingInt(const char* envName, const char* urlName, int fallback) {
    return urlInt(urlName, envIntOnly(envName, fallback));
}

bool urlSettingEquals(const char* name, const char* expected) {
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location) return 0;
        var key = UTF8ToString($0);
        var expected = UTF8ToString($1).toLowerCase();
        var read = function (source) {
            if (!source) return null;
            var text = String(source);
            if (text.charAt(0) === '#') text = text.slice(1);
            if (text.charAt(0) === '?') text = text.slice(1);
            var params = new URLSearchParams(text);
            return params.has(key) ? params.get(key) : null;
        };
        var raw = read(window.location.search);
        if (raw === null) raw = read(window.location.hash);
        return raw !== null && String(raw).toLowerCase() === expected ? 1 : 0;
    }, name, expected);
}

bool consumeWebResignRequest() {
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.realmPendingResign) return 0;
        window.realmPendingResign = false;
        return 1;
    }) != 0;
}

static bool isEmbedRoute() {
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location || typeof window.location.pathname !== 'string') {
            return 0;
        }
        var path = window.location.pathname.toLowerCase();
        var search = (window.location.search || "").toLowerCase();
        var hash = (window.location.hash || "").toLowerCase();
        if (search.indexOf('embed') !== -1 || hash.indexOf('embed') !== -1) return 1;

        var segments = path.split('/');
        for (var i = 0; i < segments.length; i++) {
            if (segments[i] === 'embed') return 1;
        }
        return 0;
    });
}

static bool isAsciiOnlySurface() {
    return EM_ASM_INT({
        if (typeof window === 'undefined' || !window.location) return 0;
        var host = String(window.location.hostname || "").toLowerCase();
        var path = String(window.location.pathname || "").toLowerCase();
        var search = String(window.location.search || "").toLowerCase();
        var hash = String(window.location.hash || "").toLowerCase();
        if (host.indexOf('ascii.') === 0 || host.indexOf('ascii--') === 0) return 1;
        var segments = path.split('/').filter(Boolean);
        for (var i = 0; i < segments.length; i++) {
            if (segments[i] === 'ascii' || segments[i] === 'realm-ascii') return 1;
        }
        return (search.indexOf('asciionly=1') !== -1 || hash.indexOf('asciionly=1') !== -1) ? 1 : 0;
    });
}

void notifyReady() {
    EM_ASM({
        globalThis.realmReady = true;
        if (typeof window !== 'undefined') {
            window.dispatchEvent(new CustomEvent('realm-ready'));
        }
    });
}

void startMatch(int numAIs, bool deterministic) {
    if (deterministic) {
        g.biomeChoice = settingInt("REALM_WEB_BIOME", "biome", B_TEMPERATE);
        if (g.biomeChoice < -1 || g.biomeChoice > B_OCEAN) g.biomeChoice = B_TEMPERATE;

        unsigned seed = (unsigned)settingInt("REALM_WEB_SEED", "seed", 2468);
        int humanCorner = settingInt("REALM_WEB_HUMAN_CORNER", "corner", 1);
        if (humanCorner < 0 || humanCorner > 3) humanCorner = 1;

        initGameWithSeed(numAIs, seed, humanCorner);
    } else {
        initGame(numAIs);
    }

    if (gfxConsumeLoadGameRequest()) {
        Command command;
        command.payload = LoadCommand{ 0 };
        if (dispatchCommandForLocalGame(g, gameEvents(), command).status == CommandStatus::Accepted) {
            std::cerr << "realm: loaded realm-save.txt from web menu\n";
        } else {
            std::cerr << "realm: web menu load failed; continuing new game\n";
        }
    }

    gfxOnNewGame();
    emitUiStatusEvent(-1, "Browser build ready. Select peasants with click/tap and command with right click or keyboard.");
    webScreen = WEB_MATCH;
    nextTickMs = emscripten_get_now() + TICK_MS;
    std::cerr << "realm: web initialized tick=" << g.tick
              << " entities=" << g.entities.size() << "\n";
}

void showMenu() {
    webScreen = WEB_MENU;
    g.returnToMenu = false;
    menuReadyLogged = false;
    menuAIs = std::max(1, std::min(3, settingInt("REALM_WEB_AIS", "ais", 1)));
    menuBiomeIdx = 7;
    int forcedBiome = settingInt("REALM_WEB_BIOME", "biome", -1);
    if (forcedBiome >= B_TEMPERATE && forcedBiome <= B_OCEAN) menuBiomeIdx = forcedBiome;
}

void frame() {
    if (!initialized) return;

    if (webScreen == WEB_EXITED) return;

    if (webScreen == WEB_MENU) {
        int result = gfxSplashFrame(menuAIs, menuBiomeIdx);
        if (!menuReadyLogged) {
            std::cerr << "realm: main screen ready\n";
            menuReadyLogged = true;
        }
        if (result < 0) {
            webScreen = WEB_EXITED;
            emitUiStatusEvent(-1, "Realm exited.");
            std::cerr << "realm: web exited from main menu\n";
        } else if (result > 0) {
            startMatch(menuAIs, false);
        }
        return;
    }

    bool quit = false;
    gfxPollInput(quit);
    if (consumeWebResignRequest()) {
        showMenu();
        return;
    }
    if (quit) {
        webScreen = WEB_EXITED;
        emitUiStatusEvent(-1, "Realm exited.");
        std::cerr << "realm: web exited from match\n";
        return;
    }
    if (g.returnToMenu) {
        showMenu();
        return;
    }

    double now = emscripten_get_now();
    int safety = 0;
    while (now >= nextTickMs && safety < 4) {
        nextTickMs += TICK_MS;
        if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
            tickSimulationOnce(g, gameEvents(), true);
            tickUiState(ui);
        }
        safety++;
    }

    gfxRender();
    if (g.returnToMenu) showMenu();
}

} // namespace

extern "C" {

EMSCRIPTEN_KEEPALIVE
int realm_web_tick() {
    return g.tick;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_entity_count() {
    return (int)g.entities.size();
}

EMSCRIPTEN_KEEPALIVE
int realm_web_selected_id() {
    return g.local.selectedId;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_selected_count() {
    return g.local.selectedIds.empty() ? (g.local.selectedId >= 0 ? 1 : 0) : (int)g.local.selectedIds.size();
}

EMSCRIPTEN_KEEPALIVE
int realm_web_view_x() {
    return view.viewX;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_view_y() {
    return view.viewY;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_view_w() {
    return view.viewW;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_view_h() {
    return view.viewH;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_cursor_x() {
    return view.cursorX;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_cursor_y() {
    return view.cursorY;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_first_owned_unit_x() {
    for (const Entity& e : g.entities) {
        if (e.alive && e.owner == 0 && isUnit(e.type)) return e.x;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_first_owned_unit_y() {
    for (const Entity& e : g.entities) {
        if (e.alive && e.owner == 0 && isUnit(e.type)) return e.y;
    }
    return -1;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_screen_x_for_tile(int mx, int my) {
    int px = 0, py = 0;
    return gfxScreenCenterForMapTileForTest(mx, my, px, py) ? px : -1;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_screen_y_for_tile(int mx, int my) {
    int px = 0, py = 0;
    return gfxScreenCenterForMapTileForTest(mx, my, px, py) ? py : -1;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_screen() {
    return webScreen == WEB_MATCH ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_ascii_only() {
    return asciiOnlySurface ? 1 : 0;
}

EMSCRIPTEN_KEEPALIVE
int realm_web_display_mode() {
    return displayMode == DM_ASCII ? 0 : 1;
}

}

int main() {
    forceUtf8Locale();
    asciiOnlySurface = isAsciiOnlySurface() || realmVisualModeIsAsciiOnly();
    displayMode = asciiOnlySurface ? DM_ASCII : DM_EMOJI;
    if (!asciiOnlySurface && (urlSettingEquals("display", "ascii") || urlSettingEquals("visual", "ascii"))) {
        displayMode = DM_ASCII;
    } else if (!asciiOnlySurface
               && (urlSettingEquals("display", "tileset") || urlSettingEquals("visual", "tileset")
                   || urlSettingEquals("display", "emoji") || urlSettingEquals("visual", "emoji"))) {
        displayMode = DM_EMOJI;
    }
    const bool startedFromEmbed = isEmbedRoute();

    if (!gfxInit()) {
        std::cerr << "realm: web gfxInit failed\n";
        return 1;
    }

    gfxSetAsciiOnly(asciiOnlySurface);
    gfxSetProjection(true);

    int numAIs = settingInt("REALM_WEB_AIS", "ais", 1);
    numAIs = std::max(1, std::min(3, numAIs));

    if (startedFromEmbed) {
        startMatch(numAIs, true);
    } else {
        showMenu();
    }

    initialized = true;
    notifyReady();

    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}


