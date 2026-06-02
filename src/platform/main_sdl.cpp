#include "realm.h"
#include "view_state.h"
#include "gfx_renderer.h"
#include "env_config.h"
#include "entity_animation.h"
#include "commands/command.h"

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

static bool captureAsciiComparePair(const std::filesystem::path& outDir, const std::string& name) {
    bool ok = true;
    std::filesystem::path terminalShot = outDir / (name + "-terminal-reference.bmp");
    std::filesystem::path terminalText = outDir / (name + "-terminal-reference.txt");
    std::filesystem::path guiShot = outDir / (name + "-gui-ascii.bmp");
    ok = gfxSaveAsciiTerminalReference(terminalShot.string()) && ok;
    ok = gfxSaveAsciiTerminalText(terminalText.string()) && ok;
    ok = gfxSaveScreenshot(guiShot.string()) && ok;
    std::cerr << "realm: ascii compare " << (ok ? "ok " : "failed ") << name << "\n";
    return ok;
}

static int runAsciiCompareMode() {
    std::filesystem::path outDir = "build/ascii-compare";
    if (const char* env = std::getenv("REALM_ASCII_COMPARE_DIR")) {
        if (*env) outDir = env;
    }
    std::filesystem::create_directories(outDir);

    int width = envIntLocal("REALM_ASCII_COMPARE_WIDTH", 1074);
    int height = envIntLocal("REALM_ASCII_COMPARE_HEIGHT", 827);
    gfxSetWindowSizeForTest(width, height);

    displayMode = DM_ASCII;
    gfxSetProjection(false);
    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    initGameWithSeed(1, (unsigned)envIntLocal("REALM_SEED", 2468), envIntLocal("REALM_HUMAN_CORNER", 1));
    gfxOnNewGame();
    g.statusTimer = 0;

    bool ok = true;
    ok = captureAsciiComparePair(outDir, "01-overview") && ok;

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.selectedId = peasant->id;
        g.selectedIds.clear();
        view.cursorX = peasant->x;
        view.cursorY = peasant->y;
        g.statusTimer = 0;
        ok = captureAsciiComparePair(outDir, "02-selected-peasant") && ok;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.selectedId = townHall->id;
        g.selectedIds.clear();
        view.cursorX = townHall->x;
        view.cursorY = townHall->y;
        g.diagnostics = true;
        g.statusTimer = 0;
        ok = captureAsciiComparePair(outDir, "03-selected-townhall-diagnostics") && ok;
        g.diagnostics = false;
    }

    std::cerr << "realm: ascii compare " << (ok ? "complete" : "failed")
              << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
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
        view.cursorX = peasant->x;
        view.cursorY = peasant->y;
        setStatus("UI test: peasant selected");
        ok = captureUiFrame((outDir / "03-selected-peasant.bmp").string()) && ok;

        peasant->state = S_IDLE;
        peasant->targetId = -1;
        peasant->targetX = -1;
        peasant->targetY = -1;
        peasant->path.clear();
        peasant->pathIdx = 0;
        peasant->facingDx = 1;
        peasant->facingDy = 0;
        g.tick = 0;
        ok = captureUiFrame((outDir / "03a-ingame-peasant-idle-down-right-front-frame0.bmp").string()) && ok;
        g.tick = 260;
        ok = captureUiFrame((outDir / "03b-ingame-peasant-idle-down-right-front-frame1-arms-crossed.bmp").string()) && ok;
        peasant->facingDx = 0;
        peasant->facingDy = 1;
        ok = captureUiFrame((outDir / "03c-ingame-peasant-idle-down-left-front-mirrored.bmp").string()) && ok;
        peasant->facingDx = 0;
        peasant->facingDy = -1;
        ok = captureUiFrame((outDir / "03d-ingame-peasant-idle-up-right-back.bmp").string()) && ok;
        peasant->facingDx = -1;
        peasant->facingDy = 0;
        ok = captureUiFrame((outDir / "03e-ingame-peasant-idle-up-left-back-mirrored.bmp").string()) && ok;
        peasant->facingDx = 1;
        peasant->facingDy = 0;
        g.tick = 0;

        g.mode = M_BUILD_SELECT;
        g.statusTimer = 0;
        ok = captureUiFrame((outDir / "04-build-menu.bmp").string()) && ok;
        g.mode = M_NORMAL;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.selectedId = townHall->id;
        g.selectedIds.clear();
        view.cursorX = townHall->x;
        view.cursorY = townHall->y;
        g.diagnostics = true;
        ok = captureUiFrame((outDir / "05-selected-townhall-diagnostics.bmp").string()) && ok;
        g.diagnostics = false;
    }

    g.helpOverlay = true;
    ok = captureUiFrame((outDir / "06-help-overlay.bmp").string()) && ok;
    g.helpOverlay = false;

    for (int i = 0; i < 60; i++) tickSimulationOnce();
    ok = captureUiFrame((outDir / "07-after-60-ticks.bmp").string()) && ok;

    g.selectedId = -1;
    g.selectedIds.clear();
    view.cursorX = MAP_W / 2;
    view.cursorY = MAP_H / 2;
    g.statusTimer = 0;
    gfxSetProjection(false);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "08-center-topdown.bmp").string()) && ok;

    gfxSetProjection(true);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "09-center-isometric.bmp").string()) && ok;

    view.cursorX = MAP_W - 1;
    view.cursorY = 0;
    gfxSetProjection(true);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "10-top-right-isometric.bmp").string()) && ok;

    g.map[0][MAP_W - 1].visible[0] = true;
    g.map[0][MAP_W - 1].explored[0] = true;
    g.actionMarkers.push_back({MAP_W - 1, 0, 120, 'x'});
    gfxSetZoomForTest(38);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "10b-top-right-isometric-38px.bmp").string()) && ok;
    g.actionMarkers.clear();
    gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 20));

    view.cursorX = MAP_W - 1;
    view.cursorY = MAP_H - 1;
    gfxSetProjection(false);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "11-bottom-right-topdown.bmp").string()) && ok;

    gfxSetProjection(true);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "12-bottom-right-isometric.bmp").string()) && ok;

    {
        const int panelW = 286;
        const int miniX = width - panelW + 14;
        const int miniY = 12;
        const int miniW = panelW - 28;
        const int miniH = 110;

        SDL_Event down{};
        down.type = SDL_MOUSEBUTTONDOWN;
        down.button.button = SDL_BUTTON_LEFT;
        down.button.x = miniX + miniW / 4;
        down.button.y = miniY + miniH / 4;
        SDL_PushEvent(&down);

        SDL_Event move{};
        move.type = SDL_MOUSEMOTION;
        move.motion.state = SDL_BUTTON_LMASK;
        move.motion.x = miniX + miniW - 4;
        move.motion.y = miniY + miniH - 4;
        SDL_PushEvent(&move);

        SDL_Event up{};
        up.type = SDL_MOUSEBUTTONUP;
        up.button.button = SDL_BUTTON_LEFT;
        up.button.x = move.motion.x;
        up.button.y = move.motion.y;
        SDL_PushEvent(&up);

        bool quitRequested = false;
        gfxPollInput(quitRequested);
        ok = !quitRequested && ok;
        ok = captureUiFrame((outDir / "13-minimap-pan.bmp").string()) && ok;
    }

    {
        const int panelW = 286;
        const int mapW = width - panelW;
        const int startX = mapW - 36;
        const int startY = height - 88;

        SDL_Event down{};
        down.type = SDL_MOUSEBUTTONDOWN;
        down.button.button = SDL_BUTTON_MIDDLE;
        down.button.x = startX;
        down.button.y = startY;
        SDL_PushEvent(&down);

        for (int i = 0; i < 4; ++i) {
            SDL_Event move{};
            move.type = SDL_MOUSEMOTION;
            move.motion.state = SDL_BUTTON_MMASK;
            move.motion.x = startX - 55 * (i + 1);
            move.motion.y = startY - 28 * (i + 1);
            SDL_PushEvent(&move);
        }

        SDL_Event up{};
        up.type = SDL_MOUSEBUTTONUP;
        up.button.button = SDL_BUTTON_MIDDLE;
        up.button.x = startX - 220;
        up.button.y = startY - 112;
        SDL_PushEvent(&up);

        bool quitRequested = false;
        gfxPollInput(quitRequested);
        ok = !quitRequested && ok;
        ok = captureUiFrame((outDir / "14-middle-pan-edge.bmp").string()) && ok;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.dayPhase = 0.0f;
        g.weather = W_CLEAR;
        updateFog();
        view.cursorX = townHall->x + STATS[townHall->type].sizeW / 2;
        view.cursorY = townHall->y + STATS[townHall->type].sizeH / 2;
        gfxSetZoomForTest(26);
        gfxSetProjection(true);
        gfxOnNewGame();
        ok = captureUiFrame((outDir / "15-night-torch-light.bmp").string()) && ok;
    }

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.selectedId = peasant->id;
        g.selectedIds.clear();
        view.cursorX = peasant->x;
        view.cursorY = peasant->y;
        g.mode = M_NORMAL;
        gfxSetProjection(true);
        gfxSetZoomForTest(24);

        gfxSetWindowSizeForTest(430, 820);
        gfxOnNewGame();
        ok = captureUiFrame((outDir / "16-mobile-portrait-hud.bmp").string()) && ok;

        g.mode = M_BUILD_SELECT;
        ok = captureUiFrame((outDir / "17-mobile-portrait-build-menu.bmp").string()) && ok;

        gfxSetWindowSizeForTest(900, 430);
        g.mode = M_NORMAL;
        gfxOnNewGame();
        ok = captureUiFrame((outDir / "18-mobile-landscape-hud.bmp").string()) && ok;

        DisplayMode previousMode = displayMode;
        displayMode = DM_ASCII;
        gfxSetProjection(false);

        gfxSetWindowSizeForTest(430, 820);
        ok = gfxSaveSplashScreenshot((outDir / "19-mobile-ascii-menu.bmp").string(), 1, 7) && ok;
        gfxOnNewGame();
        ok = captureUiFrame((outDir / "20-mobile-ascii-portrait-hud.bmp").string()) && ok;

        g.mode = M_BUILD_SELECT;
        ok = captureUiFrame((outDir / "21-mobile-ascii-portrait-build-menu.bmp").string()) && ok;

        gfxSetWindowSizeForTest(900, 430);
        g.mode = M_NORMAL;
        gfxOnNewGame();
        ok = captureUiFrame((outDir / "22-mobile-ascii-landscape-hud.bmp").string()) && ok;

        displayMode = previousMode;
        gfxSetProjection(true);
        gfxSetWindowSizeForTest(width, height);
        gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 20));
        gfxOnNewGame();
    }

    auto verifyZoomAnchor = [&](bool iso) {
        gfxSetProjection(iso);
        gfxSetZoomForTest(20);
        view.cursorX = MAP_W / 2;
        view.cursorY = MAP_H / 2;
        gfxOnNewGame();
        int anchorX = (width - 286) / 2;
        int anchorY = 32 + (height - 32 - 48) / 2;
        int beforeX = 0, beforeY = 0, afterX = 0, afterY = 0;
        if (!gfxMapTileAtScreenForTest(anchorX, anchorY, beforeX, beforeY)) {
            std::cerr << "realm: zoom anchor test failed before projection=" << (iso ? "iso" : "top") << "\n";
            return false;
        }
        gfxSetZoomAnchoredForTest(32, anchorX, anchorY);
        if (!gfxMapTileAtScreenForTest(anchorX, anchorY, afterX, afterY)) {
            std::cerr << "realm: zoom anchor test failed after in projection=" << (iso ? "iso" : "top") << "\n";
            return false;
        }
        if (beforeX != afterX || beforeY != afterY) {
            std::cerr << "realm: zoom anchor mismatch in projection=" << (iso ? "iso" : "top")
                      << " before=" << beforeX << ',' << beforeY
                      << " after=" << afterX << ',' << afterY << "\n";
            return false;
        }
        gfxSetZoomAnchoredForTest(18, anchorX, anchorY);
        if (!gfxMapTileAtScreenForTest(anchorX, anchorY, afterX, afterY)) {
            std::cerr << "realm: zoom anchor test failed after out projection=" << (iso ? "iso" : "top") << "\n";
            return false;
        }
        bool same = beforeX == afterX && beforeY == afterY;
        if (!same) {
            std::cerr << "realm: zoom anchor mismatch out projection=" << (iso ? "iso" : "top")
                      << " before=" << beforeX << ',' << beforeY
                      << " after=" << afterX << ',' << afterY << "\n";
        }
        return same;
    };
    ok = verifyZoomAnchor(false) && ok;
    ok = verifyZoomAnchor(true) && ok;

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.selectedId = townHall->id;
        g.selectedIds.clear();
        g.mode = M_NORMAL;
        g.players[0].gold = 500;
        g.players[0].wood = 500;
        g.players[0].food = 500;
        g.players[0].supplyMax = 20;

        SDL_Event train{};
        train.type = SDL_KEYDOWN;
        train.key.keysym.sym = SDLK_t;
        SDL_PushEvent(&train);
        bool quitRequested = false;
        gfxPollInput(quitRequested);
        ok = !quitRequested && g.mode == M_TRAIN_SELECT && ok;

        SDL_Event queuePeasant{};
        queuePeasant.type = SDL_KEYDOWN;
        queuePeasant.key.keysym.sym = SDLK_p;
        SDL_PushEvent(&queuePeasant);
        gfxPollInput(quitRequested);
        townHall = firstOwned(E_TOWNHALL, 0);
        ok = !quitRequested && townHall && townHall->producing == E_PEASANT
             && g.mode == M_TRAIN_SELECT && ok;

        SDL_Event click{};
        click.type = SDL_MOUSEBUTTONDOWN;
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = (width - 286) / 2;
        click.button.y = 32 + (height - 32 - 48) / 2;
        SDL_PushEvent(&click);
        gfxPollInput(quitRequested);
        ok = !quitRequested && g.mode == M_NORMAL && ok;
    }

    if (std::getenv("REALM_UI_CAPTURE_TEST")) {
        SDL_Event event{};
        event.type = SDL_KEYDOWN;
        event.key.keysym.sym = SDLK_y;
        SDL_PushEvent(&event);
        bool quitRequested = false;
        gfxPollInput(quitRequested);
        ok = !quitRequested && ok;
    }

    std::cerr << "realm: ui test " << (ok ? "complete" : "failed") << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--dump-missing-tileset-assets") {
        return dumpMissingTilesetAssets();
    }
    if (argc >= 2 && std::string(argv[1]) == "--dump-animation-spec") {
        const char* entityArg = argc >= 3 ? argv[2] : "peasant";
        EntityType type = entityTypeForAnimationSlug(entityArg);
        if (!writeEntityAnimationSpecJson(std::cout, type)) {
            std::cerr << "unknown entity animation spec: " << entityArg << "\n";
            return 2;
        }
        return 0;
    }
    std::freopen("realm-run.log", "w", stderr);
    std::cerr << "realm: process started\n";

    forceUtf8Locale();
    loadRealmEnvironmentFiles();
    const bool asciiOnly = realmVisualModeIsAsciiOnly();
    displayMode = asciiOnly ? DM_ASCII : DM_EMOJI;

    if (!gfxInit()) return 1;
    gfxSetAsciiOnly(asciiOnly);
    gfxSetProjection(true);
    std::cerr << "realm: gfxInit ok\n";

    if (std::getenv("REALM_UI_TEST")) {
        int code = runUiTestMode();
        gfxShutdown();
        return code;
    }

    if (std::getenv("REALM_ASCII_COMPARE")) {
        int code = runAsciiCompareMode();
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
        if (gfxConsumeLoadGameRequest()) {
            Command command;
            command.payload = LoadCommand{ 0 };
            if (dispatchCommand(g, command).status == CommandStatus::Accepted) {
                std::cerr << "realm: loaded realm-save.txt from GUI menu\n";
            } else {
                std::cerr << "realm: GUI menu load failed; continuing new game\n";
            }
        }
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
