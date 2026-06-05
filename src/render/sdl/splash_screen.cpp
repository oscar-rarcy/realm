#include "render/sdl/sdl_splash.h"
#include "realm.h"
#include "user_settings.h"
#include "core/game_events.h"

bool saveRendererPixels(const std::string& path) {
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, s.winW, s.winH, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::cerr << "realm: screenshot surface failed: " << SDL_GetError() << "\n";
        return false;
    }
    bool ok = SDL_RenderReadPixels(s.ren, nullptr, SDL_PIXELFORMAT_ARGB8888,
        surface->pixels, surface->pitch) == 0;
    if (!ok) {
        std::cerr << "realm: screenshot read failed: " << SDL_GetError() << "\n";
    } else if (SDL_SaveBMP(surface, path.c_str()) != 0) {
        std::cerr << "realm: screenshot save failed: " << SDL_GetError() << "\n";
        ok = false;
    }
    SDL_FreeSurface(surface);
    SDL_RenderPresent(s.ren);
    return ok;
}

void drawHelpOverlay() {
    if (!g.local.helpOverlay) return;
    SDL_Rect r{std::max(20, s.winW / 2 - 360), std::max(20, s.winH / 2 - 260), 720, 520};
    if (r.x + r.w > s.winW - 20) r.w = std::max(320, s.winW - 40), r.x = 20;
    if (r.y + r.h > s.winH - 20) r.h = std::max(320, s.winH - 40), r.y = 20;
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(6,8,11,235)); SDL_RenderFillRect(s.ren, &r);
    setDraw(rgb(160,170,185,220)); SDL_RenderDrawRect(s.ren, &r);

    int x = r.x + 18, y = r.y + 16;
    drawText(x, y, "Help", rgb(255,235,145)); y += 28;
    int n = 0;
    const CommandHelpBinding* commands = gameplayHelpBindings(n);
    for (int i = 0; i < n && y < r.y + r.h - 96; i++) {
        std::ostringstream line;
        line << commands[i].keys << "  " << commands[i].label << " - " << commands[i].help;
        drawText(x, y, trimPanelLine(line.str(), 78), rgb(220,225,220));
        y += 20;
    }
    y += 10;
    drawText(x, y, "Food: berries, hunting, farms/mills, wheat work, and fishing all feed your stockpile.", rgb(185,195,200)); y += 20;
    drawText(x, y, "Winter drains food from living units; starvation damages units when stores run out.", rgb(185,195,200)); y += 20;
    drawText(x, y, "Legend: owner colours mark player/enemies; animals are neutral; ! marks recent combat.", rgb(185,195,200)); y += 20;
    drawText(x, r.y + r.h - 28, "Press ? to close", rgb(255,230,120));
}

void clearTextCache() {
    for (auto& kv : s.textCache) SDL_DestroyTexture(kv.second);
    s.textCache.clear();
    for (auto& kv : s.sizedMonoFonts) {
        if (kv.second) TTF_CloseFont(kv.second);
    }
    s.sizedMonoFonts.clear();
}

bool pointInRect(int x, int y, SDL_Rect r) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static SDL_Point splashColorWheelCenter() {
    int col = s.winW / 2 - 360;
    return SDL_Point{col + 610, std::max(40, s.winH / 2 - 255) + 218};
}

static int splashColorWheelRadius() {
    return std::max(44, std::min(60, s.winW / 24));
}

static int hueFromWheelPoint(int px, int py, SDL_Point c) {
    constexpr double pi = 3.14159265358979323846;
    double angle = std::atan2((double)(py - c.y), (double)(px - c.x));
    int hue = (int)std::lround(angle * 180.0 / pi);
    return normalizePlayerColorHue(hue);
}

static bool saveCurrentUserSettings() {
    UserSettings settings = userSettingsFromGame(g);
    settings.asciiSquareMapCells = s.asciiSquareMapCells;
    return saveUserSettings(settings);
}

static void toggleAsciiMapCellAspect() {
    s.asciiSquareMapCells = !s.asciiSquareMapCells;
    saveCurrentUserSettings();
}

static bool handleSplashColorWheelClick(int px, int py, int numAIs) {
    SDL_Point c = splashColorWheelCenter();
    int radius = splashColorWheelRadius();
    int dx = px - c.x;
    int dy = py - c.y;
    int d2 = dx * dx + dy * dy;
    if (d2 > (radius + 12) * (radius + 12) || d2 < (radius - 24) * (radius - 24)) {
        return false;
    }
    setHumanPlayerColorHue(g, hueFromWheelPoint(px, py, c));
    configurePlayerColorHues(g, numAIs);
    saveCurrentUserSettings();
    return true;
}

static void drawSplashColorWheel(int numAIs) {
    configurePlayerColorHues(g, numAIs);
    SDL_Point c = splashColorWheelCenter();
    int radius = splashColorWheelRadius();
    int inner = std::max(12, radius - 18);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    constexpr double pi = 3.14159265358979323846;
    for (int hue = 0; hue < 360; ++hue) {
        double a = hue * pi / 180.0;
        Color col = colorFromHue(hue);
        setDraw(col);
        for (int r = inner; r <= radius; ++r) {
            int x = c.x + (int)std::lround(std::cos(a) * r);
            int y = c.y + (int)std::lround(std::sin(a) * r);
            SDL_RenderDrawPoint(s.ren, x, y);
            SDL_RenderDrawPoint(s.ren, x + 1, y);
        }
    }

    setDraw(rgb(12, 18, 25, 230));
    for (int r = 0; r < inner - 2; ++r) {
        for (int x = -r; x <= r; ++x) {
            int y = (int)std::lround(std::sqrt((double)(r * r - x * x)));
            SDL_RenderDrawPoint(s.ren, c.x + x, c.y + y);
            SDL_RenderDrawPoint(s.ren, c.x + x, c.y - y);
        }
    }
    int humanHue = normalizePlayerColorHue(g.playerColorHue[0]);
    double selected = humanHue * pi / 180.0;
    int sx = c.x + (int)std::lround(std::cos(selected) * radius);
    int sy = c.y + (int)std::lround(std::sin(selected) * radius);
    SDL_Rect marker{sx - 5, sy - 5, 10, 10};
    setDraw(rgb(5, 8, 12, 235)); SDL_RenderFillRect(s.ren, &marker);
    setDraw(rgb(255, 245, 190, 255)); SDL_RenderDrawRect(s.ren, &marker);

    drawTextFit(c.x - radius, c.y - radius - 30, "PLAYER COLOUR", rgb(255,230,135), radius * 2 + 90);
    int activePlayers = std::max(2, std::min(MAX_PLAYERS, 1 + numAIs));
    int swatchY = c.y + radius + 18;
    int swatchX = c.x - radius;
    for (int owner = 0; owner < activePlayers; ++owner) {
        SDL_Rect swatch{swatchX + owner * 34, swatchY, 24, 16};
        setDraw(colorFromHue(g.playerColorHue[owner])); SDL_RenderFillRect(s.ren, &swatch);
        setDraw(owner == 0 ? rgb(255,245,190) : rgb(150,165,175)); SDL_RenderDrawRect(s.ren, &swatch);
    }
    drawTextFit(c.x - radius, swatchY + 22, "You + CPUs", rgb(185,195,200), radius * 2 + 34);
}

std::vector<MobileButton> mobileSplashButtons() {
    std::vector<MobileButton> buttons;
    int pad = mobileSafePad();
    int bw = std::min(360, std::max(220, s.winW - pad * 2));
    int x = (s.winW - bw) / 2;
    int y = mobilePortrait() ? std::max(120, s.winH / 4) : std::max(70, s.winH / 5);
    if (s.mobileSplashSettings) {
        addGridButtons(buttons, x, y, bw,
            {{"orient", s.mobileOrientation == 0 ? "Orientation Auto" : s.mobileOrientation == 1 ? "Portrait Lock" : "Landscape Lock"},
             {"uiscale", "UI Scale"},
             {"zoom", "Zoom Level"},
             {"mapaspect", s.asciiSquareMapCells ? "Map Square" : "Map Terminal"},
             {"minimap", s.mobileMinimapTap ? "Minimap On" : "Minimap Off"},
             {"confirm", s.mobileConfirmCommands ? "Confirm On" : "Confirm Off"},
             {"edge", s.mobileEdgeScroll ? "Edge Scroll On" : "Edge Scroll Off"},
             {"settings", "Back"}}, 1);
        return buttons;
    }
    if (s.mobileSplashHelp) {
        addGridButtons(buttons, x, y + 230, bw, {{"helpback", "Back"}}, 1);
        return buttons;
    }
    std::vector<std::pair<std::string, std::string>> mainItems{
        {"new", "New Game"}, {"load", "Load Game"}
    };
    if (!s.asciiOnly) {
        mainItems.push_back({"visual", displayMode == DM_ASCII ? "Visual ASCII" : "Visual Tileset"});
    }
#if defined(REALM_WEB)
    mainItems.push_back({"fullscreen", "Full Screen"});
    mainItems.push_back({"settings", "Settings"});
    mainItems.push_back({"help", "Help"});
#else
    mainItems.push_back({"settings", "Settings"});
    mainItems.push_back({"help", "Help"});
    mainItems.push_back({"quit", "Quit"});
#endif
    addGridButtons(buttons, x, y, bw, mainItems, 1);
    return buttons;
}

void drawAsciiMobileSplash(int numAIs, int biomeIdx) {
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    int pad = mobileSafePad();
    int x = pad;
    int y = pad + 8;
    int w = std::max(1, s.winW - pad * 2);
    setDraw(termDim());
    SDL_Rect border{pad, pad, w, std::max(1, s.winH - pad * 2)};
    SDL_RenderDrawRect(s.ren, &border);
    drawTextFit(x + 10, y, "R E A L M", termHigh(), std::max(1, w - 20), s.mono); y += 28;
    drawTextFit(x + 10, y, "-- Medieval Warlord --", termAccent(), std::max(1, w - 20),
                s.monoSmall ? s.monoSmall : s.mono);
    y += 32;

    if (s.mobileSplashHelp) {
        drawTextFit(x + 10, y, "TOUCH CONTROLS", termHigh(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 26;
        drawTextFit(x + 10, y, "Tap selects or commands. Drag the map to pan.", termFg(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 22;
        drawTextFit(x + 10, y, "Tap terminal buttons for build, attack, rally, pause, and idle.", termFg(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono); y += 22;
        drawTextFit(x + 10, y, "Long press inspects. Double tap selects nearby units of the same type.", termDim(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono);
    } else if (s.mobileSplashSettings) {
        drawTextFit(x + 10, y, "SETTINGS", termHigh(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono); y += 26;
        drawTextFit(x + 10, y, "Tap an option to cycle it.", termDim(), std::max(1, w - 20),
                    s.monoSmall ? s.monoSmall : s.mono);
    } else {
        static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
        std::ostringstream ss;
        ss << "Opponents:" << numAIs << "  Biome:" << biomeNames[biomeIdx] << "  Display:ASCII";
        drawTextFit(x + 10, y, ss.str(), termFg(), std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono); y += 24;
        drawTextFit(x + 10, y, "Tap [New Game] to start. Tap [Visual ASCII] for tileset.", termDim(),
                    std::max(1, w - 20), s.monoSmall ? s.monoSmall : s.mono);
    }

    for (const MobileButton& b : mobileSplashButtons()) {
        drawConsoleButton(b, b.id == "visual", b.id == "quit");
    }
    SDL_RenderPresent(s.ren);
}

void drawMobileSplash(int numAIs, int biomeIdx) {
    if (displayMode == DM_ASCII) {
        drawAsciiMobileSplash(numAIs, biomeIdx);
        return;
    }
    setDraw(rgb(7,10,15)); SDL_RenderClear(s.ren);
    int pad = mobileSafePad();
    drawTextFit(pad, pad + 8, "R E A L M", rgb(255,235,145), s.winW - pad * 2);
    drawTextFit(pad, pad + 34, "Medieval Warlord", rgb(180,205,230), s.winW - pad * 2);
    if (s.mobileSplashHelp) {
        int y = pad + 82;
        drawTextFit(pad, y, "Touch Controls", rgb(255,230,135), s.winW - pad * 2); y += 28;
        drawTextFit(pad, y, "Tap selects or commands. Drag the map to pan.", rgb(220,225,220), s.winW - pad * 2); y += 24;
        drawTextFit(pad, y, "Use command buttons for build, attack, rally, pause, and idle peasants.", rgb(220,225,220), s.winW - pad * 2); y += 24;
        drawTextFit(pad, y, "Long press inspects. Double tap selects nearby units of the same type.", rgb(220,225,220), s.winW - pad * 2);
    } else if (!s.mobileSplashSettings) {
        static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
        std::ostringstream ss;
        ss << "Opponents " << numAIs << "   Biome " << biomeNames[biomeIdx];
        drawTextFit(pad, pad + 64, ss.str(), rgb(220,225,220), s.winW - pad * 2);
    }
    for (const MobileButton& b : mobileSplashButtons()) drawButton(b);
    SDL_RenderPresent(s.ren);
}

bool handleMobileSplashTap(int px, int py, int& numAIs, int& biomeIdx, bool& done) {
    for (const MobileButton& b : mobileSplashButtons()) {
        if (!pointInRect(px, py, b.r)) continue;
        if (b.id == "new") { done = true; return true; }
        if (b.id == "load") { s.loadGameRequested = true; done = true; return true; }
        if (b.id == "fullscreen") { toggleFullscreen(); return true; }
        if (b.id == "visual" && !s.asciiOnly) {
            if (displayMode == DM_ASCII) {
                displayMode = DM_EMOJI;
                s.isometric = true;
            } else {
                displayMode = DM_ASCII;
                s.isometric = false;
            }
            resetZoomForDisplayMode();
            return true;
        }
        if (b.id == "quit") { gfxShutdown(); std::exit(0); }
        if (b.id == "settings") { s.mobileSplashSettings = !s.mobileSplashSettings; s.mobileSplashHelp = false; return true; }
        if (b.id == "help") { s.mobileSplashHelp = true; s.mobileSplashSettings = false; return true; }
        if (b.id == "helpback") { s.mobileSplashHelp = false; return true; }
        if (b.id == "orient") { s.mobileOrientation = (s.mobileOrientation + 1) % 3; return true; }
        if (b.id == "uiscale") { s.mobileUiScale = s.mobileUiScale >= 1.25f ? 0.9f : s.mobileUiScale + 0.1f; return true; }
        if (b.id == "zoom") {
            if (s.tile >= zoomMaxTilePx()) setZoom(zoomDefaultTilePx());
            else zoomBySteps(1);
            return true;
        }
        if (b.id == "mapaspect") { toggleAsciiMapCellAspect(); return true; }
        if (b.id == "minimap") { s.mobileMinimapTap = !s.mobileMinimapTap; return true; }
        if (b.id == "confirm") { s.mobileConfirmCommands = !s.mobileConfirmCommands; return true; }
        if (b.id == "edge") { s.mobileEdgeScroll = !s.mobileEdgeScroll; return true; }
    }
    (void)numAIs; (void)biomeIdx;
    return false;
}

int applySplashChoice(int ch, int& numAIs, int& biomeIdx) {
    if (ch == '\n' || ch == '\r') {
        g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
        return 1;
    }
    if (ch == 'q' || ch == 'Q' || ch == 'x' || ch == 'X') {
#if defined(REALM_WEB)
        emitUiStatusEvent(-1, "Close the browser tab to exit.");
        return 0;
#else
        return -1;
#endif
    }
    if (ch == '1') numAIs = 1;
    else if (ch == '2') numAIs = 2;
    else if (ch == '3') numAIs = 3;
    else if (ch == '0') biomeIdx = 7;
    else if (ch == 't' || ch == 'T') biomeIdx = 0;
    else if (ch == 'd' || ch == 'D') biomeIdx = 1;
    else if (ch == 's' || ch == 'S') biomeIdx = 2;
    else if (ch == 'w' || ch == 'W') biomeIdx = 3;
    else if (ch == 'f' || ch == 'F') biomeIdx = 4;
    else if (ch == 'c' || ch == 'C') biomeIdx = 6;
    else if (ch == '4') {
        displayMode = DM_ASCII;
        resetZoomForDisplayMode();
    }
    else if (ch == '5' && !s.asciiOnly) {
        displayMode = DM_EMOJI;
        s.isometric = true;
        resetZoomForDisplayMode();
    }
    else if (ch == '6') {
        toggleAsciiMapCellAspect();
    }
    return 0;
}

bool handleSplashKeyHit(int px, int py, int& numAIs, int& biomeIdx, int& result) {
    if (handleSplashColorWheelClick(px, py, numAIs)) {
        result = 0;
        return true;
    }
    for (const KeyHit& hit : s.keyHits) {
        if (!pointInRect(px, py, hit.r)) continue;
        result = applySplashChoice(hit.ch, numAIs, biomeIdx);
        return true;
    }
    return false;
}

void drawSplash(int numAIs, int biomeIdx) {
    s.keyHits.clear();
    SDL_GetMouseState(&s.mouseX, &s.mouseY);
    if (isMobileGui()) {
        drawMobileSplash(numAIs, biomeIdx);
        return;
    }
    setDraw(rgb(7,10,15)); SDL_RenderClear(s.ren);
    int col = s.winW/2 - 360;
    int y = std::max(40, s.winH/2 - 255);
    auto line = [&](const std::string& t, Color c = rgb(220,225,220), int dy = 22) {
        drawText(col, y, t, c); y += dy;
    };
    drawText(col+210, y, "R  E  A  L  M", rgb(255,235,145)); y += 28;
    drawText(col+175, y, "-- Medieval Warlord --", rgb(180,205,230)); y += 40;
    line("You are lord of a small settlement in a hostile realm.");
    line("Gather resources, build an army, and outlast every rival.");
    y += 10;
    line("CONTROLS", rgb(255,230,135));
    line("  Space/click    Select unit or building");
    line("  Enter/R-click  Command (move/attack/gather)");
    line("  Mouse wheel    Zoom in/out");
    line("  Middle-drag    Pan the map");
    line("  B/T/A/H/P      Build / Train / Military / Town hall / Pause");
    y += 10;
    line("OPPONENTS", rgb(255,230,135));
    drawKeyTokensInText(col, y, "  [1] Duel       [2] Three-way     [3] Four-way",
                        {{"[1]", '1'}, {"[2]", '2'}, {"[3]", '3'}},
                        rgb(220,225,220), 720); y += 22;
    y += 4;
    line("BIOME", rgb(255,230,135));
    drawKeyTokensInText(col, y, "  [0] Random    [T] Temperate  [D] Desert",
                        {{"[0]", '0'}, {"[T]", 't'}, {"[D]", 'd'}},
                        rgb(220,225,220), 720); y += 22;
    drawKeyTokensInText(col, y, "  [S] Snow      [W] Swamp      [F] Forest",
                        {{"[S]", 's'}, {"[W]", 'w'}, {"[F]", 'f'}},
                        rgb(220,225,220), 720); y += 22;
    drawKeyTokensInText(col, y, "  [C] Coastal",
                        {{"[C]", 'c'}},
                        rgb(220,225,220), 720); y += 22;
    if (!s.asciiOnly) {
        y += 4;
        line("DISPLAY", rgb(255,230,135));
        std::string displayLine = std::string("  [4] ASCII     [5] Tileset     > ") + (displayMode==DM_EMOJI ? "Tileset" : "ASCII");
        drawKeyTokensInText(col, y, displayLine, {{"[4]", '4'}, {"[5]", '5'}},
                            rgb(220,225,220), 720); y += 22;
    }
    y += 4;
    line("ASCII MAP", rgb(255,230,135));
    std::string mapLine = std::string("  [6] Map cells: ") + (s.asciiSquareMapCells ? "Square" : "Terminal");
    drawKeyTokensInText(col, y, mapLine, {{"[6]", '6'}}, rgb(220,225,220), 720); y += 22;
    drawSplashColorWheel(numAIs);
    y += 10;
    static const char* biomeNames[] = {"Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"};
    std::ostringstream ss; ss << "  > Opponents: " << numAIs << "    Biome: " << biomeNames[biomeIdx];
    line(ss.str(), rgb(255,245,180));
    y += 4;
    drawKeyTokensInText(col, y, "  [Enter] Start game            [Q/X] Quit",
                        {{"[Enter]", '\n'}, {"Q", 'q'}, {"X", 'x'}},
                        rgb(210,230,245), 720);
    SDL_RenderPresent(s.ren);
}
