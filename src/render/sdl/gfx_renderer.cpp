#include "render/sdl/sdl_splash.h"
#include "realm.h"
#include "commands/command.h"
#include "commands/command_runner.h"
#include "core/game_events.h"
#include "core/world_index.h"
#include "view_state.h"

bool gfxInit() {
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
    SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n"; return false;
    }
    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init failed: " << TTF_GetError() << "\n"; return false;
    }

    s.win = SDL_CreateWindow("Realm - graphical terminal renderer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, s.winW, s.winH,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!s.win) { std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n"; return false; }
    s.ren = SDL_CreateRenderer(s.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!s.ren) s.ren = SDL_CreateRenderer(s.win, -1, SDL_RENDERER_SOFTWARE);
    if (!s.ren) { std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n"; return false; }
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);

    std::vector<std::string> monoPaths = {
        // Native Windows / MSYS2.
        "C:/Windows/Fonts/consola.ttf", "C:/Windows/Fonts/Consola.ttf",
        "C:/Windows/Fonts/cour.ttf", "C:/Windows/Fonts/Cour.ttf",
        // WSL / WSLg accessing the Windows font directory.
        "/mnt/c/Windows/Fonts/consola.ttf", "/mnt/c/Windows/Fonts/Consola.ttf",
        "/mnt/c/Windows/Fonts/cour.ttf", "/mnt/c/Windows/Fonts/Cour.ttf",
        "/c/Windows/Fonts/consola.ttf", "/c/Windows/Fonts/cour.ttf",
        // Linux/macOS fallbacks.
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf",
        "/System/Library/Fonts/Menlo.ttc", "/Library/Fonts/Menlo.ttc"
    };
    std::vector<std::string> emojiPaths = {
#if defined(REALM_WEB)
        "/assets/fonts/RealmSymbols.ttf",
#endif
        // Native Windows / MSYS2.
        "C:/Windows/Fonts/seguiemj.ttf", "C:/Windows/Fonts/Segoe UI Emoji.ttf",
        "C:/Windows/Fonts/seguisym.ttf",
        // WSL / WSLg accessing the Windows font directory.
        "/mnt/c/Windows/Fonts/seguiemj.ttf", "/mnt/c/Windows/Fonts/seguisym.ttf",
        "/c/Windows/Fonts/seguiemj.ttf", "/c/Windows/Fonts/seguisym.ttf",
        // Linux fallbacks if installed.
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",
        "/usr/share/fonts/opentype/noto/NotoColorEmoji.ttf",
        "/usr/local/share/fonts/NotoColorEmoji.ttf",
        "/usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf",
        "/usr/share/fonts/truetype/ancient-scripts/Symbola_hint.ttf",
        // macOS fallbacks.
        "/System/Library/Fonts/Apple Color Emoji.ttc",
        "/System/Library/Fonts/Apple Symbols.ttf"
    };
    s.mono = openFont(monoPaths, 16, &s.monoPath);
    s.monoSmall = openFont(monoPaths, 13);
    s.emoji = openFont(emojiPaths, 28, &s.emojiPath);
    if (!s.mono) { std::cerr << "Could not find a monospace font.\n"; return false; }
    if (!s.emoji) {
        s.emoji = s.mono;
        s.emojiFontLoaded = false;
        std::cerr << "No tileset symbol font found; using ASCII glyph fallbacks.\n";
    } else {
        s.emojiFontLoaded = true;
    }
    std::cerr << "Text font: " << (s.monoPath.empty() ? "<unknown>" : s.monoPath) << "\n";
    std::cerr << "Tileset symbol font: " << (s.emojiFontLoaded ? s.emojiPath : std::string("<fallback>")) << "\n";
    SDL_StartTextInput();
    return true;
}

void gfxShutdown() {
#if !defined(REALM_WEB)
    tilesetAssetsClear();
#endif
    clearTextCache();
    if (s.mono) TTF_CloseFont(s.mono);
    if (s.monoSmall) TTF_CloseFont(s.monoSmall);
    if (s.emoji && s.emoji != s.mono) TTF_CloseFont(s.emoji);
    if (s.ren) SDL_DestroyRenderer(s.ren);
    if (s.win) SDL_DestroyWindow(s.win);
    TTF_Quit();
    SDL_Quit();
}

int gfxSplashFrame(int& numAIs, int& biomeIdx) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) return -1;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            s.winW = e.window.data1; s.winH = e.window.data2;
        }
        if (!isMobileGui() && e.type == SDL_MOUSEMOTION) {
            s.mouseX = e.motion.x;
            s.mouseY = e.motion.y;
        }
        if (!isMobileGui() && e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            int result = 0;
            if (handleSplashKeyHit(e.button.x, e.button.y, numAIs, biomeIdx, result)) return result;
        }
        if (isMobileGui() && (e.type == SDL_FINGERDOWN || e.type == SDL_MOUSEBUTTONDOWN)) {
            int px = 0, py = 0;
            if (e.type == SDL_FINGERDOWN) {
                px = (int)std::lround(e.tfinger.x * s.winW);
                py = (int)std::lround(e.tfinger.y * s.winH);
                s.suppressNextMouse = true;
            } else {
                if (s.suppressNextMouse) { s.suppressNextMouse = false; continue; }
                if (e.button.button != SDL_BUTTON_LEFT) continue;
                px = e.button.x; py = e.button.y;
            }
            bool done = false;
            if (handleMobileSplashTap(px, py, numAIs, biomeIdx, done) && done) {
                g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
                return 1;
            }
            continue;
        }
        if (e.type != SDL_KEYDOWN) continue;
        SDL_Keycode k = e.key.keysym.sym;
        int ch = 0;
        if (k == SDLK_RETURN || k == SDLK_KP_ENTER) ch = '\n';
        else if (k == SDLK_q) ch = 'q';
        else if (k == SDLK_x) ch = 'x';
        else if (k == SDLK_1) ch = '1';
        else if (k == SDLK_2) ch = '2';
        else if (k == SDLK_3) ch = '3';
        else if (k == SDLK_0) ch = '0';
        else if (k == SDLK_t) ch = 't';
        else if (k == SDLK_d) ch = 'd';
        else if (k == SDLK_s) ch = 's';
        else if (k == SDLK_w) ch = 'w';
        else if (k == SDLK_f) ch = 'f';
        else if (k == SDLK_v) ch = 'v';
        else if (k == SDLK_c) ch = 'c';
        else if (k == SDLK_4) ch = '4';
        else if (k == SDLK_5) ch = '5';
        if (ch) {
            int result = applySplashChoice(ch, numAIs, biomeIdx);
            if (result) return result;
        }
    }
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    drawSplash(numAIs, biomeIdx);
    return 0;
}

void gfxSetAsciiOnly(bool asciiOnly) {
    s.asciiOnly = asciiOnly;
    if (s.asciiOnly) {
        displayMode = DM_ASCII;
        s.isometric = false;
    }
}

int gfxShowSplash() {
    int numAIs = 1;
    int biomeIdx = 7;
    bool loggedReady = false;
    while (true) {
        int result = gfxSplashFrame(numAIs, biomeIdx);
        if (!loggedReady) {
            std::cerr << "realm: main screen ready\n";
            loggedReady = true;
            const char* smoke = std::getenv("REALM_SMOKE_TEST");
            if (smoke) return std::string(smoke) == "match" ? numAIs : -1;
        }
        if (result < 0) { gfxShutdown(); std::exit(0); }
        if (result > 0) return numAIs;
        SDL_Delay(16);
    }
}

bool gfxConsumeLoadGameRequest() {
    bool requested = s.loadGameRequested;
    s.loadGameRequested = false;
    return requested;
}





void gfxOnNewGame() {
    centerViewOnTile(view.cursorX, view.cursorY);
}

Entity* primaryOwnedSelection(const WorldIndex& world) {
    if (!g.selectedIds.empty()) {
        for (int id : g.selectedIds) {
            Entity* e = renderFindEntity(g, world, id);
            if (e && e->alive && e->owner == 0) return e;
        }
        return nullptr;
    }
    Entity* e = renderFindEntity(g, world, g.selectedId);
    return (e && e->alive && e->owner == 0) ? e : nullptr;
}

void mobileSelectIdlePeasant() {
    Entity* pick = nullptr;
    for (auto& e : g.entities) {
        if (e.alive && e.owner == 0 && e.type == E_PEASANT && e.state == S_IDLE) {
            pick = &e;
            break;
        }
    }
    if (!pick) {
        emitUiStatusEvent(-1, "No idle peasants");
        return;
    }
    g.selectedId = pick->id;
    g.selectedIds.clear();
    view.cursorX = pick->x;
    view.cursorY = pick->y;
    centerViewOnTile(view.cursorX, view.cursorY);
    emitUiStatusEvent(-1, "Idle peasant selected");
}

void mobileStopSelection(const WorldIndex& world) {
    int n = 0;
    auto stopOne = [&](Entity* e) {
        if (!e || !e->alive || e->owner != 0 || !isUnit(e->type)) return;
        e->state = S_IDLE;
        e->targetId = -1;
        e->path.clear();
        e->pathIdx = 0;
        e->attackMove = 0;
        n++;
    };
    if (!g.selectedIds.empty()) for (int id : g.selectedIds) stopOne(renderFindEntity(g, world, id));
    else stopOne(renderFindEntity(g, world, g.selectedId));
    if (n) emitUiStatusEvent(-1, "Stopped.");
}

EntityType mobileBuildTypeForId(const std::string& id) {
    if (id == "build:house") return E_HOUSE;
    if (id == "build:farm") return E_FARM;
    if (id == "build:barracks") return E_BARRACKS;
    if (id == "build:stable") return E_STABLE;
    if (id == "build:tower") return E_TOWER;
    if (id == "build:lumber") return E_LUMBER_CAMP;
    if (id == "build:mining") return E_MINING_CAMP;
    if (id == "build:mill") return E_MILL;
    if (id == "build:dock") return E_DOCK;
    if (id == "build:castle") return E_CASTLE;
    return E_NONE;
}

void mobileCancelCommand() {
    s.mobileBuildType = E_NONE;
    s.mobileBuildPage = 0;
    g.mode = M_NORMAL;
    view.dragging = false;
    s.leftDown = false;
    emitUiStatusEvent(-1, "Cancelled.");
}

void handleMobileHudButton(const std::string& id, const WorldIndex& world) {
    Entity* sel = primaryOwnedSelection(world);
    if (id == "cancel") { mobileCancelCommand(); return; }
    if (id == "buildmore") { s.mobileBuildPage = 1; return; }
    if (id == "buildback") { s.mobileBuildPage = 0; return; }
    if (id == "menu") { g.returnToMenu = true; return; }
    if (id == "pause") { handleInput('p'); return; }
    if (id == "fullscreen") { toggleFullscreen(); return; }
    if (id == "idle") { mobileSelectIdlePeasant(); return; }
    if (id == "help") { g.helpOverlay = !g.helpOverlay; emitUiStatusEvent(-1, g.helpOverlay ? "Help open." : "Help closed."); return; }
    if (id == "selectarmy") { handleInput('A'); return; }
    if (id == "stop") { mobileStopSelection(world); return; }
    if (id == "move") { g.mode = M_NORMAL; emitUiStatusEvent(-1, "Tap a destination."); return; }
    if (id == "gather") { g.mode = M_NORMAL; emitUiStatusEvent(-1, "Tap a resource."); return; }
    if (id == "build") {
        if (sel && sel->type == E_PEASANT) {
            g.mode = M_BUILD_SELECT;
            s.mobileBuildPage = 0;
            emitUiStatusEvent(-1, "Choose a building.");
        } else {
            emitUiStatusEvent(-1, "Select a peasant first.");
        }
        return;
    }
    EntityType bt = mobileBuildTypeForId(id);
    if (bt != E_NONE) {
        if (!sel || sel->type != E_PEASANT) {
            emitUiStatusEvent(-1, "Select a peasant first.");
            g.mode = M_NORMAL;
            return;
        }
        s.mobileBuildType = bt;
        g.mode = M_NORMAL;
        emitUiStatusEvent(-1, std::string("Placing ") + STATS[bt].name + ". Tap a valid tile.");
        return;
    }
    if (id == "attack" || id == "attackmove") {
        if (mobileHasSelectedMilitary(world)) {
            g.mode = M_ATTACK_MOVE;
            emitUiStatusEvent(-1, "Tap an attack target or destination.");
        } else {
            emitUiStatusEvent(-1, "Select military units first.");
        }
        return;
    }
    if (id == "rally") {
        if (sel && isBuilding(sel->type)) {
            g.mode = M_RALLY_SET;
            emitUiStatusEvent(-1, "Tap a rally point.");
        }
        return;
    }
    if (id == "train") {
        if (sel && isBuilding(sel->type)) {
            EntityType tt = mobileDefaultTrainType(sel->type);
            if (tt != E_NONE) {
                Command command;
                command.payload = TrainCommand{ currentSelection(g), tt };
                dispatchCommandForLocalGame(g, gameEvents(), command);
            }
        }
        return;
    }
    if (id == "trade") {
        if (sel && sel->type == E_MARKET) {
            g.mode = M_MARKET_TRADE;
            emitUiStatusEvent(-1, "Choose a market trade.");
        }
        return;
    }
    if (id == "research") {
        if (sel && sel->type == E_BLACKSMITH) {
            g.mode = M_RESEARCH_SELECT;
            emitUiStatusEvent(-1, "Choose research.");
        }
        return;
    }
    if (id == "cancelqueue") {
        if (sel && isBuilding(sel->type)) {
            sel->queue.clear();
            if (sel->producing != E_NONE) {
                sel->producing = E_NONE;
                sel->trainProgress = 0;
                sel->trainTime = 0;
                sel->state = S_IDLE;
            }
            emitUiStatusEvent(-1, "Queue cancelled.");
        }
    }
}

bool handleMobileHudHit(int px, int py) {
    if (!isMobileGui()) return false;
    if (!pointInRect(px, py, panelRect())) return false;
    if (s.mobileMinimapTap && pointInRect(px, py, miniMapRect())) {
        moveViewFromMiniMap(px, py, true);
        s.miniMapDown = true;
        return true;
    }
    WorldIndex world = buildWorldIndex(g);
    for (const MobileButton& b : mobileHudButtons(world)) {
        if (pointInRect(px, py, b.r)) {
            handleMobileHudButton(b.id, world);
            return true;
        }
    }
    return true;
}

bool handleKeyHitAt(int px, int py) {
    if (isMobileGui()) return false;
    for (const KeyHit& hit : s.keyHits) {
        if (!pointInRect(px, py, hit.r)) continue;
        bool globalShortcut = g.mode == M_NORMAL || g.mode == M_GAME_OVER || g.mode == M_PAUSED;
        if (globalShortcut && (hit.ch == 'v' || hit.ch == 'V')) {
            Command command;
            command.payload = SaveCommand{ 0 };
            dispatchCommandForLocalGame(g, gameEvents(), command);
        } else if (globalShortcut && (hit.ch == 'd' || hit.ch == 'D')) {
            g.diagnostics = !g.diagnostics;
        } else if (globalShortcut && (hit.ch == 'l' || hit.ch == 'L')) {
            Command command;
            command.payload = LoadCommand{ 0 };
            if (dispatchCommandForLocalGame(g, gameEvents(), command).status == CommandStatus::Accepted) updateViewMetrics(true);
        } else {
            handleInput(hit.ch);
        }
        return true;
    }
    return false;
}

bool screenToMapWithTolerance(int px, int py, int& mx, int& my) {
    if (screenToMap(px, py, mx, my)) return true;
    int radius = std::max(10, std::min(28, s.tile));
    for (int r = 8; r <= radius; r += 8) {
        const int pts[8][2] = {{r,0},{-r,0},{0,r},{0,-r},{r,r},{r,-r},{-r,r},{-r,-r}};
        for (auto& p : pts) {
            if (screenToMap(px + p[0], py + p[1], mx, my)) return true;
        }
    }
    return false;
}

void mobileInspectAt(int mx, int my) {
    view.cursorX = mx;
    view.cursorY = my;
    WorldIndex world = buildWorldIndex(g);
    Entity* e = renderEntityAt(g, world, mx, my);
    if (e && e->alive && g.map[my][mx].visible[0]) {
        std::ostringstream ss;
        ss << STATS[e->type].name << " HP " << e->hp << "/" << e->maxHp << " " << stateName(e->state);
        emitUiStatusEvent(-1, ss.str());
    } else {
        emitUiStatusEvent(-1, cursorTileSummary());
    }
}

void mobileTapMap(int px, int py) {
    int mx = 0, my = 0;
    if (!screenToMapWithTolerance(px, py, mx, my)) return;
    view.cursorX = mx;
    view.cursorY = my;
    if (g.mode == M_PAUSED || g.mode == M_GAME_OVER) return;
    if (s.mobileBuildType != E_NONE) {
        WorldIndex world = buildWorldIndex(g);
        Entity* builder = primaryOwnedSelection(world);
        if (!builder || builder->type != E_PEASANT) {
            emitUiStatusEvent(-1, "Select a peasant first.");
            s.mobileBuildType = E_NONE;
            return;
        }
        if (!renderCanPlace(g, world, s.mobileBuildType, mx, my, 0)) {
            emitUiStatusEvent(-1, "Cannot build there.");
            return;
        }
        Command command;
        command.payload = BuildCommand{ currentSelection(g), s.mobileBuildType, {mx, my} };
        dispatchCommandForLocalGame(g, gameEvents(), command);
        s.mobileBuildType = E_NONE;
        g.mode = M_NORMAL;
        emitUiStatusEvent(-1, "Building placed.");
        return;
    }
    if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE) {
        handleInput('\n');
        return;
    }
    WorldIndex world = buildWorldIndex(g);
    Entity* selection = primaryOwnedSelection(world);
    if (selection && (isUnit(selection->type) || !g.selectedIds.empty())) {
        Command command = resolveContextCommand(g, world, 0, currentSelection(g), {mx, my});
        dispatchCommandForLocalGame(g, gameEvents(), command);
    } else {
        Command command;
        command.payload = SelectCommand{ {mx, my} };
        dispatchCommandForLocalGame(g, gameEvents(), command);
    }
}

void mobilePointerDown(int px, int py) {
    s.touchDown = true;
    s.touchPanning = false;
    s.touchOnMap = false;
    s.touchDownTicks = SDL_GetTicks();
    s.touchStartX = s.touchLastX = px;
    s.touchStartY = s.touchLastY = py;
    if (handleMobileHudHit(px, py)) return;
    if (pointInRect(px, py, mapRect())) {
        s.touchOnMap = true;
        startMiddlePan(px, py);
        s.touchPanning = false;
    }
}

void mobilePointerMotion(int px, int py) {
    if (s.miniMapDown) {
        moveViewFromMiniMap(px, py, true);
        return;
    }
    if (!s.touchDown || !s.touchOnMap) return;
    int distPx = std::abs(px - s.touchStartX) + std::abs(py - s.touchStartY);
    if (distPx > 12) s.touchPanning = true;
    if (s.touchPanning) updateMiddlePan(px, py);
    s.touchLastX = px;
    s.touchLastY = py;
}

void mobilePointerUp(int px, int py) {
    if (s.miniMapDown) {
        moveViewFromMiniMap(px, py, true);
        s.miniMapDown = false;
        s.touchDown = false;
        s.middleDown = false;
        moveCursorToViewCenter();
        return;
    }
    bool wasMap = s.touchOnMap;
    bool wasPan = s.touchPanning;
    Uint32 held = SDL_GetTicks() - s.touchDownTicks;
    s.touchDown = false;
    s.touchOnMap = false;
    s.touchPanning = false;
    if (s.middleDown) {
        if (wasPan) updateMiddlePan(px, py);
        s.middleDown = false;
        if (wasPan) moveCursorToViewCenter();
    }
    if (!wasMap || wasPan) return;
    int mx = 0, my = 0;
    if (!screenToMapWithTolerance(px, py, mx, my)) return;
    if (held >= 550) mobileInspectAt(mx, my);
    else mobileTapMap(px, py);
}

void gfxPollInput(bool& quitRequested) {
    quitRequested = false;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) { quitRequested = true; return; }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            s.winW = e.window.data1; s.winH = e.window.data2; updateViewMetrics(true);
        }
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST && isMobileGui()) {
#if defined(REALM_WEB)
            continue;
#else
            if (g.mode == M_NORMAL) {
                g.mode = M_PAUSED;
                emitUiStatusEvent(-1, "Paused while in background.");
            }
#endif
        }
        if (isMobileGui() && (e.type == SDL_FINGERDOWN || e.type == SDL_FINGERMOTION || e.type == SDL_FINGERUP)) {
            int px = (int)std::lround(e.tfinger.x * s.winW);
            int py = (int)std::lround(e.tfinger.y * s.winH);
            s.suppressNextMouse = true;
            if (e.type == SDL_FINGERDOWN) mobilePointerDown(px, py);
            else if (e.type == SDL_FINGERMOTION) mobilePointerMotion(px, py);
            else mobilePointerUp(px, py);
            continue;
        }
        if (isMobileGui() && (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEMOTION || e.type == SDL_MOUSEBUTTONUP)) {
            if (s.suppressNextMouse) {
                if (e.type == SDL_MOUSEBUTTONUP) s.suppressNextMouse = false;
                continue;
            }
            if (e.type == SDL_MOUSEMOTION) { s.mouseX = e.motion.x; s.mouseY = e.motion.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) { s.mouseX = e.button.x; s.mouseY = e.button.y; }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) { mobilePointerDown(e.button.x, e.button.y); continue; }
            if (e.type == SDL_MOUSEMOTION && (s.touchDown || s.miniMapDown)) { mobilePointerMotion(e.motion.x, e.motion.y); continue; }
            if (e.type == SDL_MOUSEBUTTONUP && e.button.button == SDL_BUTTON_LEFT) { mobilePointerUp(e.button.x, e.button.y); continue; }
        }
        if (e.type == SDL_MOUSEWHEEL) {
            int mx, my; SDL_GetMouseState(&mx, &my);
            if (e.wheel.y > 0) setZoom(s.tile + 3, mx, my);
            if (e.wheel.y < 0) setZoom(s.tile - 3, mx, my);
        }
        if (e.type == SDL_MOUSEMOTION) {
            s.mouseX = e.motion.x;
            s.mouseY = e.motion.y;
            if (s.miniMapDown) {
                moveViewFromMiniMap(e.motion.x, e.motion.y, true);
                continue;
            }
            if (s.middleDown) {
                updateMiddlePan(e.motion.x, e.motion.y);
                continue;
            }
            int mx,my;
            if (screenToMap(e.motion.x, e.motion.y, mx, my)) {
                view.cursorX = mx; view.cursorY = my;
                if (s.leftDown) { s.lastMouseMapX = mx; s.lastMouseMapY = my; }
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN) {
            int mx,my;
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            if (e.button.button == SDL_BUTTON_MIDDLE) {
                startMiddlePan(e.button.x, e.button.y);
            } else if (e.button.button == SDL_BUTTON_LEFT && handleKeyHitAt(e.button.x, e.button.y)) {
                s.leftDown = false;
                s.middleDown = false;
                view.dragging = false;
                continue;
            } else if (e.button.button == SDL_BUTTON_LEFT &&
                       moveViewFromMiniMap(e.button.x, e.button.y)) {
                s.miniMapDown = true;
                s.leftDown = false;
                s.middleDown = false;
                view.dragging = false;
            } else if (screenToMap(e.button.x, e.button.y, mx, my)) {
                view.cursorX = mx; view.cursorY = my;
                if (g.mode == M_TRAIN_SELECT) g.mode = M_NORMAL;
                if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE) {
                    handleInput('\n');
                } else if (e.button.button == SDL_BUTTON_LEFT) {
                    if (e.button.clicks >= 2) {
                        Command command;
                        command.payload = SelectAllOfTypeInViewCommand{ {mx, my} };
                        dispatchCommandForLocalGame(g, gameEvents(), command);
                    }
                    else {
                        s.leftDown = true;
                        s.dragStartX = mx;
                        s.dragStartY = my;
                        s.lastMouseMapX = mx;
                        s.lastMouseMapY = my;
                        view.dragging = true;
                    }
                } else if (e.button.button == SDL_BUTTON_RIGHT) {
                    WorldIndex world = buildWorldIndex(g);
                    Command command = resolveContextCommand(g, world, 0, currentSelection(g), {mx, my});
                    dispatchCommandForLocalGame(g, gameEvents(), command);
                }
            }
        }
        if (e.type == SDL_MOUSEBUTTONUP) {
            int mx,my;
            s.mouseX = e.button.x;
            s.mouseY = e.button.y;
            if (e.button.button == SDL_BUTTON_LEFT && s.miniMapDown) {
                moveViewFromMiniMap(e.button.x, e.button.y, true);
                s.miniMapDown = false;
                s.leftDown = false;
                view.dragging = false;
                continue;
            }
            if (e.button.button == SDL_BUTTON_MIDDLE) {
                if (s.middleDown) updateMiddlePan(e.button.x, e.button.y);
                moveCursorToViewCenter();
                s.middleDown = false;
                continue;
            }
            if (e.button.button == SDL_BUTTON_LEFT) {
                bool hasMap = screenToMap(e.button.x, e.button.y, mx, my);
                if (!hasMap && s.leftDown && inBounds(s.lastMouseMapX, s.lastMouseMapY)) {
                    mx = s.lastMouseMapX;
                    my = s.lastMouseMapY;
                    hasMap = true;
                }
                if (!hasMap) {
                    s.leftDown = false;
                    view.dragging = false;
                    continue;
                }
                view.cursorX = mx; view.cursorY = my;
                if (g.mode == M_TRAIN_SELECT) g.mode = M_NORMAL;
                if (s.leftDown) {
                    bool moved = (std::abs(mx - s.dragStartX) + std::abs(my - s.dragStartY)) > 1;
                    if (moved) {
                        Command command;
                        command.payload = BoxSelectCommand{ {s.dragStartX, s.dragStartY}, {mx, my} };
                        dispatchCommandForLocalGame(g, gameEvents(), command);
                    } else {
                        Command command;
                        command.payload = SelectCommand{ {mx, my} };
                        dispatchCommandForLocalGame(g, gameEvents(), command);
                    }
                }
                s.leftDown = false; view.dragging = false;
            }
        }
        if (e.type == SDL_KEYDOWN) {
            SDL_Keycode k = e.key.keysym.sym;
            if (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT)) {
                toggleFullscreen();
                continue;
            }
            if (k == SDLK_x && g.mode == M_GAME_OVER) {
#if defined(REALM_WEB)
                emitUiStatusEvent(-1, "Close the browser tab to exit.");
                continue;
#else
                quitRequested = true;
                return;
#endif
            }
            if (k >= SDLK_F5 && k <= SDLK_F8) {
                int slot = (int)(k - SDLK_F5) + 1;
                Command command;
                command.payload = SaveCommand{ slot };
                dispatchCommandForLocalGame(g, gameEvents(), command);
                continue;
            }
            if (k >= SDLK_F9 && k <= SDLK_F12) {
                int slot = (int)(k - SDLK_F9) + 1;
                Command command;
                command.payload = LoadCommand{ slot };
                if (dispatchCommandForLocalGame(g, gameEvents(), command).status == CommandStatus::Accepted) updateViewMetrics(true);
                continue;
            }
            if (devCaptureEnabled() && k == SDLK_y) { captureIssueBundle(); continue; }
            if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { setZoom(s.tile+3); continue; }
            if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { setZoom(s.tile-3); continue; }
            int ch = keyToInput(k);
            if (ch) handleInput(ch);
        }
    }
}

void drawFrame(bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    s.keyHits.clear();
    SDL_GetMouseState(&s.mouseX, &s.mouseY);
    if (displayMode == DM_ASCII && isMobileGui()) {
        WorldIndex world = buildWorldIndex(g);
        drawAsciiMobileFrame(world, present);
        return;
    }
    if (displayMode == DM_ASCII && !isMobileGui()) {
        WorldIndex world = buildWorldIndex(g);
        drawAsciiTerminalFrame(world, present);
        return;
    }
    if (displayMode == DM_EMOJI) s.isometric = true;
    WorldIndex world = buildWorldIndex(g);
    setDraw(rgb(3,5,8)); SDL_RenderClear(s.ren);
    if (!isMobileGui()) drawTopBar();
    drawMap(world);
    drawPanel(world);
    if (!isMobileGui()) drawBottom(world);
    drawHelpOverlay();
    if (present) SDL_RenderPresent(s.ren);
}

void gfxRender() {
    flushGameEventsToUi(ui, 0);
    drawFrame(true);
}

void gfxDelay(int ms) {
    SDL_Delay((Uint32)std::max(0, ms));
}

void gfxSetProjection(bool isometric) {
    s.isometric = (displayMode == DM_EMOJI) ? true : isometric;
    updateViewMetrics(true);
}

void gfxSetZoomForTest(int tilePx) {
    setZoom(tilePx);
}

void gfxSetZoomAnchoredForTest(int tilePx, int anchorX, int anchorY) {
    setZoom(tilePx, anchorX, anchorY);
}

bool gfxMapTileAtScreenForTest(int px, int py, int& mx, int& my) {
    return screenToMap(px, py, mx, my);
}

bool gfxScreenCenterForMapTileForTest(int mx, int my, int& px, int& py) {
    return mapTileScreenCenter(mx, my, px, py);
}

void gfxSetWindowSizeForTest(int width, int height) {
    width = std::max(640, width);
    height = std::max(480, height);
    SDL_SetWindowSize(s.win, width, height);
    SDL_SetWindowPosition(s.win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    updateViewMetrics(true);
}

bool gfxSaveScreenshot(const std::string& path) {
    drawFrame(false);
    return saveRendererPixels(path);
}

bool gfxSaveAsciiTerminalReference(const std::string& path) {
    WorldIndex world = buildWorldIndex(g);
    drawAsciiTerminalFrame(world, false);
    return saveRendererPixels(path);
}

bool gfxSaveAsciiTerminalText(const std::string& path) {
    WorldIndex world = buildWorldIndex(g);
    TerminalFrame frame = buildAsciiTerminalFrame(world);
    std::ofstream out(path);
    if (!out) return false;
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) out << frame.at(x, y).ch;
        out << '\n';
    }
    return true;
}

bool gfxSaveSplashScreenshot(const std::string& path, int numAIs, int biomeIdx) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    drawSplash(numAIs, biomeIdx);
    return saveRendererPixels(path);
}
