#include "realm.h"
#include "view_state.h"
#include "gfx_renderer.h"
#include "env_config.h"
#include "entity_animation.h"
#include "user_settings.h"
#include "commands/command.h"
#include "commands/command_runner.h"
#include "core/game_events.h"
#include "platform/fixed_timestep.h"
#include "render/sdl/sdl_hud.h"
#include "render/sdl/sdl_profiler.h"
#include "render/sdl/sdl_viewport.h"

#include <SDL.h>

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

static double steadyNowMs() {
    using Clock = std::chrono::steady_clock;
    using MsDouble = std::chrono::duration<double, std::milli>;
    return std::chrono::duration_cast<MsDouble>(Clock::now().time_since_epoch()).count();
}

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

static bool envFlagLocal(const char* name, bool fallback = false) {
    const char* value = std::getenv(name);
    if (!value || !*value) return fallback;
    std::string text(value);
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) {
        return (char)std::tolower(c);
    });
    if (text == "0" || text == "false" || text == "no" || text == "off") return false;
    return true;
}

static std::string jsonEscapeLocal(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

static std::string rendererNameForProfile() {
    SDL_RendererInfo info{};
    if (s.ren && SDL_GetRendererInfo(s.ren, &info) == 0 && info.name) {
        return info.name;
    }
    return "unknown";
}

static bool tilesetTestMapEnabled() {
    return envFlagLocal("REALM_TILESET_TEST_MAP", false);
}

static void applyTilesetTestMap() {
    const int cx = MAP_W / 2;
    const int cy = MAP_H / 2;
    for (int y = 0; y < MAP_H; ++y) {
        for (int x = 0; x < MAP_W; ++x) {
            Tile& tile = g.map[y][x];
            tile.terrain = T_GRASS;
            tile.resources = 0;
            tile.biome = B_TEMPERATE;
            tile.preWinterTerrain = T_GRASS;
            tile.wear = 0;
            for (int p = 0; p < MAX_PLAYERS; ++p) {
                tile.visible[p] = false;
                tile.explored[p] = false;
            }
        }
    }

    const int visibleRx = envIntLocal("REALM_TILESET_TEST_VISIBLE_RX", 12);
    const int visibleRy = envIntLocal("REALM_TILESET_TEST_VISIBLE_RY", 8);
    for (int y = cy - visibleRy; y <= cy + visibleRy; ++y) {
        for (int x = cx - visibleRx; x <= cx + visibleRx; ++x) {
            if (!inBounds(x, y)) continue;
            g.map[y][x].visible[0] = true;
            g.map[y][x].explored[0] = true;
        }
    }

    auto setFeatureSample = [&](int dx, int dy, Terrain terrain, int resources, Biome biome = B_TEMPERATE) {
        int x = cx + dx;
        int y = cy + dy;
        if (!inBounds(x, y)) return;
        Tile& tile = g.map[y][x];
        tile.terrain = terrain;
        tile.resources = resources;
        tile.biome = biome;
        tile.preWinterTerrain = terrain;
        tile.visible[0] = true;
        tile.explored[0] = true;
    };
    setFeatureSample(-4, 0, T_BERRY, 70);
    setFeatureSample(-2, 0, T_GOLD, 300);
    setFeatureSample(0, 0, T_FOREST, 120);
    setFeatureSample(2, 0, T_PINE, 120);
    setFeatureSample(4, 0, T_MOUNTAIN, 0);

    g.entities.clear();
    g.projectiles.clear();
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    g.local.buildPending = E_NONE;
    g.players[0].alive = true;
    for (int p = 1; p < MAX_PLAYERS; ++p) g.players[p].alive = false;
    g.mode = M_PAUSED;
    view.cursorX = cx;
    view.cursorY = cy;
    view.viewX = std::max(0, cx - 18);
    view.viewY = std::max(0, cy - 14);
    ui.statusTimer = 0;
    std::cerr << "realm: tileset test map enabled grass=visible unknown=unexplored center="
              << cx << "," << cy << "\n";
}

static bool captureUiFrame(const std::string& path) {
    bool ok = gfxSaveScreenshot(path);
    gfxDelay(40);
    std::cerr << "realm: ui screenshot " << (ok ? "ok " : "failed ") << path << "\n";
    return ok;
}

static bool captureUiFrameWithMouse(const std::string& path, int mouseX, int mouseY) {
    int oldX = s.mouseX;
    int oldY = s.mouseY;
    SDL_WarpMouseInWindow(s.win, mouseX, mouseY);
    SDL_PumpEvents();
    bool ok = captureUiFrame(path);
    SDL_WarpMouseInWindow(s.win, oldX, oldY);
    SDL_PumpEvents();
    return ok;
}

static bool firstTilesetHudClickablePointFrom(int startY, int& outX, int& outY) {
    SDL_Rect overlay = tilesetHudOverlayRect();
    for (int y = std::max(overlay.y, startY); y < overlay.y + overlay.h; y += 4) {
        for (int x = overlay.x; x < overlay.x + overlay.w; x += 4) {
            if (!tilesetHudClickableAt(x, y)) continue;
            outX = x;
            outY = y;
            return true;
        }
    }
    return false;
}

static bool setupNightLightView(int zoom) {
    Entity* townHall = firstOwned(E_TOWNHALL, 0);
    if (!townHall) return false;
    g.dayPhase = 0.0f;
    g.weather = W_CLEAR;
    updateFog(g);
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    int centerX = townHall->x + STATS[townHall->type].sizeW / 2;
    int centerY = townHall->y + STATS[townHall->type].sizeH / 2;
    gfxSetProjection(true);
    gfxSetZoomForTest(zoom);
    view.cursorX = centerX;
    view.cursorY = centerY;
    if (std::getenv("REALM_UI_NIGHT_LIGHT_TEST")) {
        const int radius = 9;
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                int mx = centerX + dx;
                int my = centerY + dy;
                if (!inBounds(mx, my) || dx * dx + dy * dy > radius * radius) continue;
                g.map[my][mx].visible[0] = true;
                g.map[my][mx].explored[0] = true;
            }
        }
    }
    gfxOnNewGame();
    ui.statusTimer = 0;
    return true;
}

static int runNightLightTestMode() {
    std::filesystem::path outDir = "build/night-light-screenshots";
    if (const char* env = std::getenv("REALM_UI_NIGHT_LIGHT_TEST_DIR")) {
        if (*env) outDir = env;
    }
    std::filesystem::create_directories(outDir);

    gfxSetWindowSizeForTest(envIntLocal("REALM_UI_TEST_WIDTH", 1074),
                            envIntLocal("REALM_UI_TEST_HEIGHT", 827));
    displayMode = DM_EMOJI;
    gfxSetAsciiOnly(false);
    gfxResetZoomForDisplayMode();
    gfxSetProjection(true);

    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    int ais = envIntLocal("REALM_NIGHT_LIGHT_TEST_AIS", 0);
    initGameWithSeed(ais, (unsigned)envIntLocal("REALM_SEED", 2468),
                     envIntLocal("REALM_HUMAN_CORNER", 1));

    bool ok = setupNightLightView(envIntLocal("REALM_UI_TEST_ZOOM", 26));
    if (ok) ok = captureUiFrame((outDir / "01-night-townhall-light.bmp").string()) && ok;

    std::cerr << "realm: night light test " << (ok ? "complete" : "failed")
              << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
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
    ui.statusTimer = 0;

    bool ok = true;
    ok = captureAsciiComparePair(outDir, "01-overview") && ok;

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.local.selectedId = peasant->id;
        g.local.selectedIds.clear();
        view.cursorX = peasant->x;
        view.cursorY = peasant->y;
        ui.statusTimer = 0;
        ok = captureAsciiComparePair(outDir, "02-selected-peasant") && ok;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.local.selectedId = townHall->id;
        g.local.selectedIds.clear();
        view.cursorX = townHall->x;
        view.cursorY = townHall->y;
        g.local.diagnostics = true;
        ui.statusTimer = 0;
        ok = captureAsciiComparePair(outDir, "03-selected-townhall-diagnostics") && ok;
        g.local.diagnostics = false;
    }

    std::cerr << "realm: ascii compare " << (ok ? "complete" : "failed")
              << " dir=" << outDir.string() << "\n";
    return ok ? 0 : 1;
}

static const char* seasonFileSlug(Season season) {
    switch (season) {
        case SPRING: return "spring";
        case SUMMER: return "summer";
        case AUTUMN: return "autumn";
        case WINTER: return "winter";
    }
    return "unknown";
}

static void prepareSeasonVisualScene(Season season) {
    applyTilesetTestMap();
    g.tick = 600;
    g.dayPhase = 0.25f;
    g.weather = W_CLEAR;
    g.weatherTimer = 999;
    g.seasonPhase = (float)season + 0.75f;
    g.prevSeason = season;
    if (season == WINTER) {
        g.prevSeason = AUTUMN;
        tickSeasons(g, gameEvents());
    }
    updateFog(g);
    view.cursorX = MAP_W / 2;
    view.cursorY = MAP_H / 2;
    view.viewX = std::max(0, view.cursorX - 18);
    view.viewY = std::max(0, view.cursorY - 14);
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    g.local.diagnostics = false;
    g.local.helpOverlay = false;
    ui.statusTimer = 0;
    ui.actionMarkers.clear();
}

static int runSeasonVisualTestMode() {
    std::filesystem::path outDir = "build/season-visuals";
    if (const char* env = std::getenv("REALM_SEASON_VISUAL_TEST_DIR")) {
        if (*env) outDir = env;
    }
    std::filesystem::create_directories(outDir);

    gfxSetWindowSizeForTest(envIntLocal("REALM_UI_TEST_WIDTH", 1074),
                            envIntLocal("REALM_UI_TEST_HEIGHT", 827));
    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    initGameWithSeed(0, (unsigned)envIntLocal("REALM_SEED", 2468),
                     envIntLocal("REALM_HUMAN_CORNER", 1));

    bool ok = true;
    for (Season season : {SPRING, SUMMER, AUTUMN, WINTER}) {
        prepareSeasonVisualScene(season);
        displayMode = DM_ASCII;
        gfxSetAsciiOnly(true);
        gfxSetProjection(false);
        gfxOnNewGame();
        std::string name = std::string("ascii-") + seasonFileSlug(season);
        ok = captureAsciiComparePair(outDir, name) && ok;
    }

    displayMode = DM_EMOJI;
    gfxSetAsciiOnly(false);
    gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 24));
    for (Season season : {SPRING, SUMMER, AUTUMN, WINTER}) {
        prepareSeasonVisualScene(season);
        std::string slug = seasonFileSlug(season);
        gfxSetProjection(false);
        gfxOnNewGame();
        ok = captureUiFrame((outDir / ("tileset-" + slug + "-topdown.bmp")).string()) && ok;
        gfxSetProjection(true);
        gfxOnNewGame();
        ok = captureUiFrame((outDir / ("tileset-" + slug + "-isometric.bmp")).string()) && ok;
    }

    std::cerr << "realm: season visual test " << (ok ? "complete" : "failed")
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

    bool ok = true;
    configurePlayerColorHues(g, 3);
    ok = gfxSaveSplashScreenshot((outDir / "00-splash-color-wheel.bmp").string(), 3, 7) && ok;

    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    initGameWithSeed(1, (unsigned)envIntLocal("REALM_SEED", 2468), envIntLocal("REALM_HUMAN_CORNER", 1));
    if (tilesetTestMapEnabled()) applyTilesetTestMap();
    gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 20));
    gfxOnNewGame();
    ui.statusTimer = 0;

    gfxSetProjection(false);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "01-topdown-overview.bmp").string()) && ok;

    gfxSetProjection(true);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "02-isometric-overview.bmp").string()) && ok;

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.local.selectedId = peasant->id;
        g.local.selectedIds.clear();
        view.cursorX = peasant->x;
        view.cursorY = peasant->y;
        emitUiStatusEvent(-1, "UI test: peasant selected");
        ok = captureUiFrame((outDir / "03-selected-peasant.bmp").string()) && ok;
        {
            SDL_Rect overlay = tilesetHudOverlayRect();
            int hudMx = overlay.x + 24;
            int hudMy = height / 2;
            int mapX = -1, mapY = -1;
            if (tilesetHudConsumesPointer(hudMx, hudMy) && gfxMapTileAtScreenForTest(hudMx, hudMy, mapX, mapY)) {
                std::cerr << "realm: tileset HUD pointer leaked to map tile "
                          << mapX << "," << mapY << " at " << hudMx << "," << hudMy << "\n";
                ok = false;
            }
            int hoverX = 0, hoverY = 0;
            SDL_Rect mini = tilesetHudMiniMapRect();
            if (firstTilesetHudClickablePointFrom(mini.y + mini.h + 1, hoverX, hoverY)) {
                ok = captureUiFrameWithMouse((outDir / "03f-selected-peasant-hover-action.bmp").string(), hoverX, hoverY) && ok;
            } else {
                std::cerr << "realm: tileset HUD action hover target not found\n";
                ok = false;
            }
        }

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
        ui.statusTimer = 0;
        ok = captureUiFrame((outDir / "04-build-menu.bmp").string()) && ok;
        {
            SDL_Rect overlay = tilesetHudOverlayRect();
            int hoverX = overlay.x + overlay.w / 2;
            int hoverY = std::max(232, std::min(274, height / 3)) + 24;
            if (!tilesetHudClickableAt(hoverX, hoverY)
                    && !firstTilesetHudClickablePointFrom(std::max(200, height / 4), hoverX, hoverY)) {
                std::cerr << "realm: tileset HUD build hover target not found\n";
                ok = false;
            } else {
                ok = captureUiFrameWithMouse((outDir / "04a-build-menu-hover-card.bmp").string(), hoverX, hoverY) && ok;
            }
        }
        g.mode = M_NORMAL;
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.local.selectedId = townHall->id;
        g.local.selectedIds.clear();
        view.cursorX = townHall->x;
        view.cursorY = townHall->y;
        g.local.diagnostics = true;
        ok = captureUiFrame((outDir / "05-selected-townhall-diagnostics.bmp").string()) && ok;
        g.local.diagnostics = false;
    }

    g.local.helpOverlay = true;
    ok = captureUiFrame((outDir / "06-help-overlay.bmp").string()) && ok;
    g.local.helpOverlay = false;

    for (int i = 0; i < 60; i++) tickSimulationOnce(g, gameEvents(), true);
    ok = captureUiFrame((outDir / "07-after-60-ticks.bmp").string()) && ok;

    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    view.cursorX = MAP_W / 2;
    view.cursorY = MAP_H / 2;
    ui.statusTimer = 0;
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
    ui.actionMarkers.push_back({MAP_W - 1, 0, 120, 'x'});
    gfxSetZoomForTest(38);
    gfxOnNewGame();
    ok = captureUiFrame((outDir / "10b-top-right-isometric-38px.bmp").string()) && ok;
    ui.actionMarkers.clear();
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

    if (setupNightLightView(26)) {
        ok = captureUiFrame((outDir / "15-night-torch-light.bmp").string()) && ok;
    }

    if (Entity* peasant = firstOwned(E_PEASANT, 0)) {
        g.local.selectedId = peasant->id;
        g.local.selectedIds.clear();
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

    {
        DisplayMode previousMode = displayMode;
        displayMode = DM_ASCII;
        gfxSetWindowSizeForTest(894, 916);
        gfxSetViewportOnly(true);
        gfxSetProjection(true);
        gfxSetZoomForTest(24);
        view.cursorX = MAP_W / 2;
        view.cursorY = MAP_H / 2;
        gfxOnNewGame();

        int topLeftX = -1, topLeftY = -1;
        if (!gfxMapTileAtScreenForTest(2, 2, topLeftX, topLeftY)) {
            std::cerr << "realm: ascii viewport-only top-left hit test failed\n";
            ok = false;
        } else {
            int px = -1, py = -1;
            if (!gfxScreenCenterForMapTileForTest(topLeftX, topLeftY, px, py)
                    || px < 0 || py < 0 || px > 40 || py > 40) {
                std::cerr << "realm: ascii viewport-only screen center mismatch"
                          << " tile=" << topLeftX << ',' << topLeftY
                          << " screen=" << px << ',' << py << "\n";
                ok = false;
            }
        }

        int centerTileX = view.viewX + std::min(13, std::max(0, view.viewW - 1));
        int centerTileY = view.viewY + std::min(13, std::max(0, view.viewH - 1));
        int centerPx = -1, centerPy = -1, roundTripX = -1, roundTripY = -1;
        if (!gfxScreenCenterForMapTileForTest(centerTileX, centerTileY, centerPx, centerPy)
                || !gfxMapTileAtScreenForTest(centerPx, centerPy, roundTripX, roundTripY)
                || roundTripX != centerTileX || roundTripY != centerTileY) {
            std::cerr << "realm: ascii viewport-only round-trip mismatch"
                      << " tile=" << centerTileX << ',' << centerTileY
                      << " screen=" << centerPx << ',' << centerPy
                      << " roundTrip=" << roundTripX << ',' << roundTripY << "\n";
            ok = false;
        }

        gfxSetViewportOnly(false);
        displayMode = previousMode;
        gfxSetProjection(true);
        gfxSetWindowSizeForTest(width, height);
        gfxSetZoomForTest(envIntLocal("REALM_UI_TEST_ZOOM", 20));
        gfxOnNewGame();
    }

    if (Entity* townHall = firstOwned(E_TOWNHALL, 0)) {
        g.local.selectedId = townHall->id;
        g.local.selectedIds.clear();
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

static int runTilesetProfileMode() {
    std::filesystem::path outDir = "build/profiles";
    if (const char* env = std::getenv("REALM_PROFILE_DIR")) {
        if (*env) outDir = env;
    }
    std::filesystem::create_directories(outDir);

    const int width = envIntLocal("REALM_PROFILE_WIDTH", 1280);
    const int height = envIntLocal("REALM_PROFILE_HEIGHT", 800);
    const int frames = std::max(1, envIntLocal("REALM_PROFILE_FRAMES", 600));
    const int warmupFrames = std::max(0, envIntLocal("REALM_PROFILE_WARMUP_FRAMES", 120));
    const int tickEvery = std::max(0, envIntLocal("REALM_PROFILE_TICK_EVERY", 6));
    const int zoom = envIntLocal("REALM_PROFILE_ZOOM", 20);
    const int ais = std::max(0, std::min(3, envIntLocal("REALM_PROFILE_AIS", 3)));
    const unsigned seed = (unsigned)envIntLocal("REALM_PROFILE_SEED", 2468);
    const int humanCorner = envIntLocal("REALM_PROFILE_HUMAN_CORNER", 1);
    const bool presentFrames = envFlagLocal("REALM_PROFILE_PRESENT", true);

    gfxSetWindowSizeForTest(width, height);
    displayMode = DM_EMOJI;
    gfxSetAsciiOnly(false);
    gfxSetProjection(true);
    gfxSetZoomForTest(zoom);

    g.biomeChoice = envIntLocal("REALM_BIOME", B_TEMPERATE);
    initGameWithSeed(ais, seed, humanCorner);
    if (tilesetTestMapEnabled()) applyTilesetTestMap();
    gfxOnNewGame();
    if (envFlagLocal("REALM_PROFILE_VIEWPORT_ONLY", false)) {
        gfxSetViewportOnly(true);
    }
    ui.statusTimer = 0;
    updateFog(g);

    std::cerr << "realm: tileset profile warmup frames=" << warmupFrames
              << " measured_frames=" << frames
              << " tick_every=" << tickEvery
              << " window=" << width << "x" << height
              << " zoom=" << zoom
              << " ais=" << ais
              << " present=" << (presentFrames ? 1 : 0) << "\n";

    realmProfilerSetEnabled(false);
    for (int i = 0; i < warmupFrames; ++i) {
        SDL_PumpEvents();
        if (tickEvery > 0 && (i % tickEvery) == 0) {
            tickSimulationOnce(g, gameEvents(), true);
            tickUiState(ui);
        }
        if (presentFrames) gfxRender();
        else gfxRenderNoPresentForTest();
    }

    realmProfilerReset();
    realmProfilerSetEnabled(true);
    double startMs = steadyNowMs();
    for (int i = 0; i < frames; ++i) {
        SDL_PumpEvents();
        if (tickEvery > 0 && (i % tickEvery) == 0) {
            RealmProfileScope scope("loop.sim_tick");
            tickSimulationOnce(g, gameEvents(), true);
            tickUiState(ui);
        }
        if (presentFrames) gfxRender();
        else gfxRenderNoPresentForTest();
    }
    double elapsedMs = std::max(0.001, steadyNowMs() - startMs);
    realmProfilerSetEnabled(false);

    const double fps = frames * 1000.0 / elapsedMs;
    int actualWidth = 0;
    int actualHeight = 0;
    if (s.win) SDL_GetWindowSize(s.win, &actualWidth, &actualHeight);
    std::filesystem::path jsonPath = outDir / "tileset-profile-sections.json";
    std::filesystem::path csvPath = outDir / "tileset-profile-sections.csv";
    realmProfilerSetEnabled(true);
    realmProfilerWriteReports(jsonPath.string(), csvPath.string());
    realmProfilerSetEnabled(false);

    std::filesystem::path summaryPath = outDir / "tileset-profile-summary.json";
    {
        std::ofstream out(summaryPath, std::ios::binary);
        out << std::fixed << std::setprecision(4);
        out << "{\n";
        out << "  \"schema\": \"realm.tileset_profile_summary.v1\",\n";
        out << "  \"frames\": " << frames << ",\n";
        out << "  \"warmup_frames\": " << warmupFrames << ",\n";
        out << "  \"elapsed_ms\": " << elapsedMs << ",\n";
        out << "  \"fps\": " << fps << ",\n";
        out << "  \"target_fps\": 120.0,\n";
        out << "  \"min_acceptable_fps\": 60.0,\n";
        out << "  \"width\": " << width << ",\n";
        out << "  \"height\": " << height << ",\n";
        out << "  \"actual_width\": " << actualWidth << ",\n";
        out << "  \"actual_height\": " << actualHeight << ",\n";
        out << "  \"zoom\": " << zoom << ",\n";
        out << "  \"ais\": " << ais << ",\n";
        out << "  \"seed\": " << seed << ",\n";
        out << "  \"tick_every\": " << tickEvery << ",\n";
        out << "  \"present\": " << (presentFrames ? "true" : "false") << ",\n";
        out << "  \"mobile_gui\": " << (isMobileGui() ? "true" : "false") << ",\n";
        out << "  \"viewport_only\": " << (s.viewportOnly ? "true" : "false") << ",\n";
        out << "  \"tileset_hud_enabled\": " << (tilesetHudEnabled() ? "true" : "false") << ",\n";
        out << "  \"renderer\": \"" << jsonEscapeLocal(rendererNameForProfile()) << "\"\n";
        out << "}\n";
    }

    std::cerr << "realm: tileset profile complete fps=" << fps
              << " elapsed_ms=" << elapsedMs
              << " summary=" << summaryPath.string()
              << " sections=" << jsonPath.string() << "\n";
    return fps >= 120.0 ? 0 : 2;
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
    UserSettings settings = loadUserSettings();
    applyUserSettingsToGame(g, settings);

    if (!gfxInit()) return 1;
    gfxSetAsciiSquareMapCells(settings.asciiSquareMapCells);
    gfxSetAsciiOnly(asciiOnly);
    gfxResetZoomForDisplayMode();
    gfxSetProjection(true);
    std::cerr << "realm: gfxInit ok\n";

    if (std::getenv("REALM_UI_NIGHT_LIGHT_TEST")) {
        int code = runNightLightTestMode();
        gfxShutdown();
        return code;
    }

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

    if (std::getenv("REALM_SEASON_VISUAL_TEST")) {
        int code = runSeasonVisualTestMode();
        gfxShutdown();
        return code;
    }

    if (std::getenv("REALM_PROFILE_TILESET")) {
        int code = runTilesetProfileMode();
        gfxShutdown();
        return code;
    }

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
            if (dispatchCommandForLocalGame(g, gameEvents(), command).status == CommandStatus::Accepted) {
                std::cerr << "realm: loaded realm-save.txt from GUI menu\n";
            } else {
                std::cerr << "realm: GUI menu load failed; continuing new game\n";
            }
        }
        if (tilesetTestMapEnabled()) applyTilesetTestMap();
        std::cerr << "realm: game initialized\n";
        gfxOnNewGame();
        emitUiStatusEvent(-1, "Dawn breaks over the realm. Select peasants [Space/click] and gather [Enter/R-click].");

        const char* smoke = std::getenv("REALM_SMOKE_TEST");
        if (smoke && std::string(smoke) == "match") {
            for (int i = 0; i < 60; i++) {
                tickSimulationOnce(g, gameEvents(), true);
                tickUiState(ui);
                gfxRender();
                gfxDelay(1);
            }
            std::cerr << "realm: match smoke complete tick=" << g.tick
                      << " entities=" << g.entities.size()
                      << " projectiles=" << g.projectiles.size() << "\n";
            gfxShutdown();
            return 0;
        }

        double nextTickMs = steadyNowMs() + TICK_MS;
        while (!g.returnToMenu) {
            bool quit = false;
            gfxPollInput(quit);
            if (quit) { gfxShutdown(); return 0; }

            bool ticked = false;
            FixedTickPlan tickPlan = planFixedTicks(steadyNowMs(), nextTickMs, TICK_MS);
            nextTickMs = tickPlan.nextTickMs;
            for (int i = 0; i < tickPlan.ticksToRun; ++i) {
                if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                    tickSimulationOnce(g, gameEvents(), true);
                    tickUiState(ui);
                }
                ticked = true;
            }

            (void)ticked;
            gfxRender();
            gfxDelay(displayMode == DM_EMOJI ? 0 : 8);
        }
    }

    gfxShutdown();
    return 0;
}
