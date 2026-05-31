#include "realm.h"
#include "gfx_renderer.h"

#include <SDL.h>

#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int, char**) {
    std::freopen("realm-run.log", "w", stderr);
    std::cerr << "realm: process started\n";

    forceUtf8Locale();
    displayMode = DM_EMOJI; // graphical renderer defaults to the enhanced view

    if (!gfxInit()) return 1;
    std::cerr << "realm: gfxInit ok\n";

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
