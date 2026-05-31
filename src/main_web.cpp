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

int envIntLocal(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)parsed : fallback;
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
    const bool startedFromEmbed = isEmbedRoute();

    if (!gfxInit()) {
        std::cerr << "realm: web gfxInit failed\n";
        return 1;
    }

    if (startedFromEmbed) {
        int numAIs = envIntLocal("REALM_WEB_AIS", 1);
        numAIs = std::max(1, std::min(3, numAIs));
        g.biomeChoice = envIntLocal("REALM_WEB_BIOME", B_TEMPERATE);
        if (g.biomeChoice < -1 || g.biomeChoice > B_OCEAN) g.biomeChoice = B_TEMPERATE;

        unsigned seed = (unsigned)envIntLocal("REALM_WEB_SEED", 2468);
        int humanCorner = envIntLocal("REALM_WEB_HUMAN_CORNER", 1);
        if (humanCorner < 0 || humanCorner > 3) humanCorner = 1;

        initGameWithSeed(numAIs, seed, humanCorner);
    } else {
        int numAIs = gfxShowSplash();
        if (numAIs < 1 || numAIs > 3) {
            std::cerr << "realm: invalid splash selection " << numAIs << "\n";
            return 1;
        }
        initGame(numAIs);
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
