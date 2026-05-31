#include "realm.h"
#include "gfx_renderer.h"

#include <SDL.h>

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

static Entity* firstOwned(EntityType type, int owner) {
    for (auto& e : g.entities)
        if (e.alive && e.owner == owner && e.type == type) return &e;
    return nullptr;
}

static int envIntLocal(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)parsed : fallback;
}

static bool captureUiFrame(const std::string& path) {
    bool ok = gfxSaveScreenshot(path);
    gfxDelay(40);
    std::cerr << "realm: ui screenshot " << (ok ? "ok " : "failed ") << path << "\n";
    return ok;
}

static int runUiTestMode() {
    std::filesystem::path outDir = "build/ui-screenshots";
    if (const char* env = std::getenv("REALM_UI_TEST_DIR")) {
        if (*env) outDir = env;
    }
    std::filesystem::create_directories(outDir);

    int width = envIntLocal("REALM_UI_TEST_WIDTH", 1074);
    int height = envIntLocal("REALM_UI_TEST_HEIGHT", 827);
    gfxSetWindowSizeForTest(width, height);

    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    initGameWithSeed(1, (unsigned)envIntLocal("REALM_SEED", 2468), envIntLocal("REALM_HUMAN_CORNER", 1));
    gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 20));
    gfxOnNewGame();
    g.statusTimer = 0;

    bool ok = true;
    gfxSetProjection(false);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "01-topdown-overview.bmp").string()) && ok;

    gfxSetProjection(true);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "02-isometric-overview.bmp").string()) && ok;

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.selectedId = peasant->id;
        g.selectedIds.clear();
        g.cursorX = peasant->x;
        g.cursorY = peasant->y;
        setStatus("UI test: peasant selected");
        ok = captureUiFrame((outDir / "03-selected-peasant.bmp").string()) && ok;

        g.mode = M_BUILD_SELECT;
        g.statusTimer = 0;
        ok = captureUiFrame((outDir / "04-build-menu.bmp").string()) && ok;
        g.mode = M_NORMAL;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.selectedId = townHall->id;
        g.selectedIds.clear();
        g.cursorX = townHall->x;
        g.cursorY = townHall->y;
        g.diagnostics = true;
        ok = captureUiFrame((outDir / "05-selected-townhall-diagnostics.bmp").string()) && ok;
        g.diagnostics = false;
    }

    g.helpOverlay = true;
    ok = captureUiFrame((outDir / "06-help-overlay.bmp").string()) && ok;
    g.helpOverlay = false;

    for (int i = 0; i < 60; i++) tickSimulationOnce();
    ok = captureUiFrame((outDir / "07-after-60-ticks.bmp").string()) && ok;

    std::cerr << "realm: ui test " << (ok ? "complete" : "failed") << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
}

int main(int, char**) {
    std::freopen("realm-run.log", "w", stderr);
    std::cerr << "realm: process started\n";

    forceUtf8Locale();
    displayMode = DM_EMOJI; // graphical renderer defaults to the enhanced view

    if (!gfxInit()) return 1;
    std::cerr << "realm: gfxInit ok\n";

    if (std::getenv("REALM_UI_TEST")) {
        int code = runUiTestMode();
        gfxShutdown();
        return code;
    }

    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;

    while (true) {
        std::cerr << "realm: entering main screen\n";
        int numAIs = gfxShowSplash();
        if (numAIs < 0) {
            std::cerr << "realm: smoke test complete\n";
            gfxShutdown();
            return 0;
        }
        std::cerr << "realm: starting game with " << numAIs << " AI opponent(s)\n";
        initGame(numAIs);
        std::cerr << "realm: game initialized\n";
        gfxOnNewGame();
        setStatus("Dawn breaks over the realm. Select peasants [Space/click] and gather [Enter/R-click].");

        const char* smoke = std::getenv("REALM_SMOKE_TEST");
        if (smoke && std::string(smoke) == "match") {
            for (int i = 0; i < 60; i++) {
                tickSimulationOnce();
                gfxRender();
                gfxDelay(1);
            }
            std::cerr << "realm: match smoke complete tick=" << g.tick
                      << " entities=" << g.entities.size()
                      << " projectiles=" << g.projectiles.size() << "\n";
            gfxShutdown();
            return 0;
        }

        auto nextTick = Clock::now() + Ms(TICK_MS);
        while (!g.returnToMenu) {
            bool quit = false;
            gfxPollInput(quit);
            if (quit) { gfxShutdown(); return 0; }

            bool ticked = false;
            if (Clock::now() >= nextTick) {
                nextTick += Ms(TICK_MS);
                if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                    tickSimulationOnce();
                }
                ticked = true;
            }

            (void)ticked;
            gfxRender();
            gfxDelay(8);
        }
    }

    gfxShutdown();
    return 0;
}
