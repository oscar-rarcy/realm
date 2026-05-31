#include "realm.h"
#include "gfx_renderer.h"

#include <emscripten/emscripten.h>
#include <SDL.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

double nextTickMs = 0.0;
bool initialized = false;

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

void notifyReady() {
    EM_ASM({
        globalThis.realmReady = true;
        if (typeof window !== 'undefined') {
            window.dispatchEvent(new CustomEvent('realm-ready'));
        }
    });
}

void frame() {
    if (!initialized) return;

    bool quit = false;
    gfxPollInput(quit);

    double now = emscripten_get_now();
    int safety = 0;
    while (now >= nextTickMs && safety < 4) {
        nextTickMs += TICK_MS;
        if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
            tickSimulationOnce();
        }
        safety++;
    }

    gfxRender();
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
    return g.selectedId;
}

}

int main() {
    forceUtf8Locale();
    displayMode = DM_EMOJI;
    if (urlSettingEquals("display", "ascii") || urlSettingEquals("visual", "ascii")) {
        displayMode = DM_ASCII;
    }
    const bool startedFromEmbed = isEmbedRoute();

    if (!gfxInit()) {
        std::cerr << "realm: web gfxInit failed\n";
        return 1;
    }

    if (urlSettingEquals("projection", "topdown") || urlSettingEquals("view", "topdown")) {
        gfxSetProjection(false);
    } else if (urlSettingEquals("projection", "isometric") || urlSettingEquals("view", "isometric")) {
        gfxSetProjection(true);
    }

    int numAIs = settingInt("REALM_WEB_AIS", "ais", 1);
    numAIs = std::max(1, std::min(3, numAIs));

    if (startedFromEmbed) {
        g.biomeChoice = settingInt("REALM_WEB_BIOME", "biome", B_TEMPERATE);
        if (g.biomeChoice < -1 || g.biomeChoice > B_OCEAN) g.biomeChoice = B_TEMPERATE;

        unsigned seed = (unsigned)settingInt("REALM_WEB_SEED", "seed", 2468);
        int humanCorner = settingInt("REALM_WEB_HUMAN_CORNER", "corner", 1);
        if (humanCorner < 0 || humanCorner > 3) humanCorner = 1;

        initGameWithSeed(numAIs, seed, humanCorner);
    } else {
        g.biomeChoice = settingInt("REALM_WEB_BIOME", "biome", -1);
        if (g.biomeChoice < -1 || g.biomeChoice > B_OCEAN) g.biomeChoice = -1;
        int seed = urlInt("seed", -1);
        int humanCorner = urlInt("corner", -1);
        if (seed >= 0 || (humanCorner >= 0 && humanCorner <= 3)) {
            if (seed < 0) seed = 2468;
            if (humanCorner < 0 || humanCorner > 3) humanCorner = 1;
            initGameWithSeed(numAIs, (unsigned)seed, humanCorner);
        } else {
            initGame(numAIs);
        }
    }
    gfxOnNewGame();
    setStatus("Browser build ready. Select peasants with click/tap and command with right click or keyboard.");

    initialized = true;
    nextTickMs = emscripten_get_now() + TICK_MS;
    notifyReady();
    std::cerr << "realm: web initialized tick=" << g.tick
              << " entities=" << g.entities.size() << "\n";

    emscripten_set_main_loop(frame, 0, 1);
    return 0;
}
