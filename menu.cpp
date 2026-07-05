#include "realm.h"
// ============================================================
// MENUS & LOBBIES — everything between launching the app and starting a
// match: the splash, skirmish setup, the battlefield picker, saved-game and
// replay browsers, the controls screen, and the multiplayer host/join
// lobbies. Pure presentation + configuration: nothing in this file may
// touch the sim (it runs before initGame or between matches).
// Split out of main.cpp 2026-07-03; main.cpp keeps the game loop, the
// init pipeline and the headless harnesses.
// ============================================================
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <ctime>

// Full splash screen. Sets g.biomeChoice, g.difficulty and game speed.
// Returns numAIs. Banner is block-art; headers use A_TITLE, which the SDL
// build renders in a blackletter face (Luminari) — the terminal gets bold.
static bool showMapPreview(unsigned long long& outSeed);   // visual battlefield picker, below
static int  showLoadMenu();                                // saved-game browser, below

#ifdef __EMSCRIPTEN__
static bool showMultiplayerMenuWeb(SplashResult& r);       // relay room-code lobby, below
#else
static bool showMultiplayerMenu(SplashResult& r);          // lobby flows, below
#endif

// Saved-game browser reachable straight from the splash ([O]) — no function
// keys, no need to start a match first. Returns a 1-based slot to load, or 0
// if the player backs out.
static int showLoadMenu() {
    static const char* seasons[] = {"Spring","Summer","Autumn","Winter"};
    auto slotUsed = [](int s, SaveSlotInfo& info) {
        char path[64]; saveSlotPath(s+1, path, sizeof path); return peekSave(path, info);
    };
    int selSlot = 0;
    for (int s = 0; s < NUM_SAVE_SLOTS; s++) { SaveSlotInfo i; if (slotUsed(s,i)) { selSlot = s; break; } }

    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        const int rowH = 3, hw = 60, hh = 6 + NUM_SAVE_SLOTS * rowH;
        int hx = std::max(1, (maxX - hw)/2), hy = std::max(2, (maxY - hh)/2);
        attron(A_TITLE|COLOR_PAIR(CP_UI_ACCENT));
        mvprintw(hy, hx, "LOAD SAVED GAME");
        attroff(A_TITLE|COLOR_PAIR(CP_UI_ACCENT));
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(hy+1, hx, "Up/Down pick   Enter load   D delete   Esc/Q back");
        attroff(COLOR_PAIR(CP_UI_DIM));
        int rowY0 = hy + 3;
        for (int s = 0; s < NUM_SAVE_SLOTS; s++) {
            int ry = rowY0 + s*rowH;
            bool sel = (s == selSlot);
            SaveSlotInfo info; bool used = slotUsed(s, info);
            attron(sel ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_TEXT));
            mvprintw(ry, hx, "%s Slot %d%s", sel ? ">" : " ", s+1, (s==0 ? "  (quicksave)" : ""));
            if (used) {
                char when[40] = "";
                time_t t = (time_t)info.saveTime; struct tm* lt = localtime(&t);
                if (lt) strftime(when, sizeof when, "%b %d  %H:%M", lt);
                mvprintw(ry+1, hx+4, "Year %d, %-6s   saved %s", info.year, seasons[info.season & 3], when);
            } else {
                mvprintw(ry+1, hx+4, "- empty -");
            }
            attroff(sel ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_TEXT));
        }
        refresh();

        int c = getch();
        if (c == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }   // keep the event queue honest
        if      (c=='q'||c=='Q'||c==27)                 return 0;
        else if (c==KEY_UP   ||c=='k'||c=='K')          selSlot = (selSlot + NUM_SAVE_SLOTS - 1) % NUM_SAVE_SLOTS;
        else if (c==KEY_DOWN ||c=='j'||c=='J')          selSlot = (selSlot + 1) % NUM_SAVE_SLOTS;
        else if (c=='d'||c=='D') {
            // Same double-press grammar as Q-quit and F-loads: destroying a
            // save should never be one accidental keystroke.
            static int armedSlot = -1;
            if (armedSlot == selSlot) {
                SaveSlotInfo info; char path[64]; saveSlotPath(selSlot+1, path, sizeof path);
                if (peekSave(path, info)) remove(path);
                armedSlot = -1;
            } else armedSlot = selSlot;
        }
        else if (c=='\n'||c=='\r'||c==KEY_ENTER) {
            SaveSlotInfo info;
            if (slotUsed(selSlot, info)) return selSlot + 1;   // only load a used slot
        }
    }
}

static void drawFrame(int r, int c, int w, int h, const char* title);   // defined below
static void drawRealmBanner(int maxX, int topRow);

// Replay browser: every match records itself into replays/ — this makes the
// recordings reachable without the command line. Newest first (the
// timestamped filenames sort chronologically). D arms, D confirms.
static std::string showReplayMenu() {
    std::vector<std::string> files;
    auto rescan = [&]() {
        files.clear();
        DIR* d = opendir("replays");
        if (!d) return;
        struct dirent* de;
        while ((de = readdir(d)) != nullptr) {
            std::string n = de->d_name;
            if (n.size() > 4 && n.substr(n.size()-4) == ".rep") files.push_back(n);
        }
        closedir(d);
        std::sort(files.rbegin(), files.rend());
    };
    rescan();
    int sel = 0, top = 0, armedDel = -1;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        drawRealmBanner(maxX, std::max(0, maxY/2 - 14));
        const int VIS = std::min(10, maxY - 16);
        const int bw = 52, bh = 4 + std::max(2, std::min((int)files.size(), VIS));
        int c = std::max(2, maxX/2 - bw/2), r0 = std::max(9, maxY/2 - 4);
        drawFrame(r0, c, bw, bh, "REPLAYS");
        if (files.empty()) {
            attron(COLOR_PAIR(CP_UI_DIM));
            mvprintw(r0+2, c+3, "No recordings yet - every match saves one.");
            attroff(COLOR_PAIR(CP_UI_DIM));
        }
        if (sel < top) top = sel;
        if (sel >= top + VIS) top = sel - VIS + 1;
        for (int i = top; i < (int)files.size() && i < top + VIS; i++) {
            bool f = (i == sel);
            // realm-YYYYMMDD-HHMMSS.rep -> "YYYY-MM-DD HH:MM"
            std::string n = files[i], when = n;
            if (n.size() >= 21 && n.rfind("realm-", 0) == 0)
                when = n.substr(6,4)+"-"+n.substr(10,2)+"-"+n.substr(12,2)+"  "+n.substr(15,2)+":"+n.substr(17,2);
            int a = f ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a);
            mvprintw(r0+2+(i-top), c+3, "%s %s%s", f ? u8"›" : " ", when.c_str(),
                     (armedDel == i) ? "   [D] again to delete!" : "");
            attroff(a);
        }
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(r0 + bh, c, "↑↓/JK pick   Enter watch   D delete (twice)   Esc back");
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') return "";
        else if ((ch==KEY_UP   || ch=='k'|| ch=='K') && !files.empty()) { sel = (sel + (int)files.size() - 1) % (int)files.size(); armedDel = -1; }
        else if ((ch==KEY_DOWN || ch=='j'|| ch=='J') && !files.empty()) { sel = (sel + 1) % (int)files.size(); armedDel = -1; }
        else if ((ch=='d' || ch=='D') && !files.empty()) {
            if (armedDel == sel) {
                remove(("replays/" + files[sel]).c_str());
                armedDel = -1; rescan();
                if (sel >= (int)files.size()) sel = std::max(0, (int)files.size() - 1);
            } else armedDel = sel;
        }
        else if ((ch=='\n' || ch=='\r' || ch==KEY_ENTER) && !files.empty())
            return "replays/" + files[sel];
    }
}

// ---- Splash configuration, remembered across menu visits and matches ----
// UI climate SLOTS map to Biome values (which are no longer contiguous —
// Steppe/Moor were appended after the legacy layout ids). cfgClim stores the
// slot; g.biomeChoice stores the Biome value.
static const int   kClimBiome[]    = { B_TEMPERATE, B_DESERT, B_SNOW, B_SWAMP, B_FOREST, B_STEPPE, B_MOOR };
static const char* kClimateNames[] = { "Temperate","Desert","Snow","Swamp","Forest","Steppe","Moorland","Random" };
static const int   kClimCount = 7;             // slots 0..6; slot 7 = Random/mixed
static int climSlotOf(int biome) {             // reverse lookup; -1/unknown -> Random
    for (int i = 0; i < kClimCount; i++) if (kClimBiome[i] == biome) return i;
    return kClimCount;
}
static const char* kLayoutNames[]  = { "Continental","Highlands","Deep Woods","River","Islands","Plains","Delta","Vale","Canyons","Random" };
static const char* kDiffNames[]    = { "Easy","Normal","Hard" };
static const char* kSpeedNames[]   = { "Slow","Normal","Fast" };
// layouts 0..LAYOUT_COUNT-1; index LAYOUT_COUNT = Random

static int cfgCiv    = -1;                // -1 = random civilisation
static const char* civLabel(int c) { return c < 0 ? "Random" : CIVS[c].name; }
static int cfgNumAIs = 1;
static int mpNumAIs  = 0;                 // multiplayer: extra AI seats
static int cfgDiff   = 1;                 // Normal
static int cfgClim   = kClimCount;        // Random/mixed
static int cfgLayout = LAYOUT_COUNT;      // Random
static int cfgSpeed  = GS_NORMAL;
static unsigned long long cfgSeed = 0;    // non-zero once a specific map is picked
static std::string cfgLastAddr;           // last address joined (typing it once is enough)

// ---- Settings persistence: the splash remembers you between launches ----
// Plain key=value file next to the saves. Only menu preferences — nothing
// sim-critical lives here (match config still travels via lobby/replay).
static void saveMenuConfig() {
    FILE* f = fopen("realm-config.txt", "w");
    if (!f) return;
    fprintf(f, "ais=%d\ndiff=%d\nclim=%d\nlayout=%d\nspeed=%d\nciv=%d\ncolour=%d\nmpais=%d\n",
            cfgNumAIs, cfgDiff, cfgClim, cfgLayout, cfgSpeed, cfgCiv, g.playerColor, mpNumAIs);
    if (!cfgLastAddr.empty()) fprintf(f, "lastaddr=%s\n", cfgLastAddr.c_str());
    fclose(f);
    platformPersistFiles();   // browser build: flush MEMFS down to IndexedDB
}
void loadMenuConfig() {
    FILE* f = fopen("realm-config.txt", "r");
    if (!f) return;
    char line[128];
    while (fgets(line, sizeof line, f)) {
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* k = line;
        char* val = eq + 1;
        val[strcspn(val, "\r\n")] = 0;
        int v = atoi(val);
        if      (!strcmp(k, "ais"))    cfgNumAIs = std::max(1, std::min(3, v));
        else if (!strcmp(k, "diff"))   cfgDiff   = std::max(0, std::min(2, v));
        else if (!strcmp(k, "clim"))   cfgClim   = std::max(0, std::min(kClimCount, v));
        else if (!strcmp(k, "layout")) cfgLayout = std::max(0, std::min((int)LAYOUT_COUNT, v));
        else if (!strcmp(k, "speed"))  cfgSpeed  = std::max(0, std::min(2, v));
        else if (!strcmp(k, "civ"))    cfgCiv    = std::max(-1, std::min(NUM_CIVS - 1, v));
        else if (!strcmp(k, "colour")) g.playerColor = std::max(0, std::min(numTeamColors() - 1, v));
        else if (!strcmp(k, "mpais"))  mpNumAIs  = std::max(0, std::min(2, v));
        else if (!strcmp(k, "lastaddr") && strlen(val) <= 40) cfgLastAddr = val;
    }
    fclose(f);
}

// Titled box frame (reused by the setup screen).
static void drawFrame(int r, int c, int w, int h, const char* title) {
    mvaddstr(r, c, u8"┌─ ");
    attron(A_TITLE); mvaddstr(r, c+3, title); attroff(A_TITLE);
    int tl = (int)strlen(title);
    mvaddstr(r, c+3+tl, " ");
    for (int x = c+4+tl; x < c+w-1; x++) mvaddstr(r, x, u8"─");
    mvaddstr(r, c+w-1, u8"┐");
    for (int y = r+1; y < r+h-1; y++) { mvaddstr(y, c, u8"│"); mvaddstr(y, c+w-1, u8"│"); }
    mvaddstr(r+h-1, c, u8"└");
    for (int x = c+1; x < c+w-1; x++) mvaddstr(r+h-1, x, u8"─");
    mvaddstr(r+h-1, c+w-1, u8"┘");
}

// The REALM block banner + subtitle, centred, with its top at `topRow`.
static void drawRealmBanner(int maxX, int topRow) {
    static const char* banner[] = {
        u8"██████╗ ███████╗ █████╗ ██╗     ███╗   ███╗",
        u8"██╔══██╗██╔════╝██╔══██╗██║     ████╗ ████║",
        u8"██████╔╝█████╗  ███████║██║     ██╔████╔██║",
        u8"██╔══██╗██╔══╝  ██╔══██║██║     ██║╚██╔╝██║",
        u8"██║  ██║███████╗██║  ██║███████╗██║ ╚═╝ ██║",
        u8"╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝",
    };
    const int W = 78;
    int col = std::max(1, maxX/2 - W/2);
    attron(COLOR_PAIR(CP_GOLD) | A_BOLD);
    for (int i = 0; i < 6; i++) mvaddstr(topRow + i, col + (W-43)/2, banner[i]);
    attroff(COLOR_PAIR(CP_GOLD) | A_BOLD);
    attron(A_TITLE | COLOR_PAIR(CP_UI_ACCENT));
    const char* sub = "~  Medieval Warlord  ~";
    mvprintw(topRow + 6, maxX/2 - (int)strlen(sub)/2, "%s", sub);
    attroff(A_TITLE | COLOR_PAIR(CP_UI_ACCENT));
}

// In-game control reference, moved off the main menu onto its own screen.
static void showControlsScreen() {
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        drawRealmBanner(maxX, std::max(0, maxY/2 - 13));
        int c = std::max(2, maxX/2 - 26), r = std::max(9, maxY/2 - 4);
        attron(A_TITLE | COLOR_PAIR(CP_UI_ACCENT)); mvprintw(r-2, c, "CONTROLS"); attroff(A_TITLE | COLOR_PAIR(CP_UI_ACCENT));
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(r+0, c, "Space / Left-click    Select");
        mvprintw(r+1, c, "Enter / Right-click   Move / attack / gather / act");
        mvprintw(r+2, c, "B build   T train     A  select all military");
        mvprintw(r+3, c, "Z patrol  X hold       1-9 / G  control groups");
        mvprintw(r+4, c, "U eject   R rally      P  pause (also save / load)");
        mvprintw(r+5, c, "Tab cycle units        H  jump to town hall");
        mvprintw(r+6, c, "Arrows pan   drag = marquee-select");
        mvprintw(r+7, c, "E  advance era (Town Hall)   R  research");
        mvprintw(r+8, c, "RClick enemy stockyard = raid its piles");
        mvprintw(r+9, c, "C  chat (multiplayer)  ?  in-game help  Q Q  menu");
        mvprintw(r+10,c, "Win: destroy every foe's halls - OR garrison and");
        mvprintw(r+11,c, "hold MOST sacred sites (shrines/mills/keeps) to a bell");
        attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(r+13, c, "Press any key to go back"); attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();
        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch != ERR) return;
    }
}

// Grouped, cursor-navigable skirmish setup. Returns true to begin (config
// committed to g.* + outSeed), false to back out to the main menu.
static bool skirmishSetup(unsigned long long& outSeed) {
    enum { R_OPP, R_DIFF, R_LAYOUT, R_CLIM, R_BROWSE, R_CIV, R_COLOUR, R_SPEED, R_BEGIN, R_COUNT };
    int sel = R_OPP;

    auto adjust = [&](int row, int d) {
        switch (row) {
            case R_OPP:    cfgNumAIs = ((cfgNumAIs - 1 + d + 3) % 3) + 1; break;
            case R_DIFF:   cfgDiff   = (cfgDiff + d + 3) % 3; break;
            case R_LAYOUT: cfgLayout = (cfgLayout + d + (LAYOUT_COUNT+1)) % (LAYOUT_COUNT+1); cfgSeed = 0; break;
            case R_CLIM:   cfgClim   = (cfgClim + d + (kClimCount+1)) % (kClimCount+1); cfgSeed = 0; break;
            case R_CIV:    cfgCiv    = ((cfgCiv + 1) + d + (NUM_CIVS+1)) % (NUM_CIVS+1) - 1; break;
            case R_COLOUR: g.playerColor = (g.playerColor + d + numTeamColors()) % numTeamColors(); applyTeamColors(); break;
            case R_SPEED:  cfgSpeed  = (cfgSpeed + d + 3) % 3; break;
            default: break;
        }
    };
    auto openPicker = [&]() {
        g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : kClimBiome[cfgClim];
        g.layoutChoice = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
        unsigned long long s = 0;
        if (showMapPreview(s)) {
            cfgSeed = s;
            cfgClim   = climSlotOf(g.biomeChoice);
            cfgLayout = (g.layoutChoice < 0) ? LAYOUT_COUNT : g.layoutChoice;
        }
    };

    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 13);
        drawRealmBanner(maxX, top);
        const int bw = 42, bh = 15;
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        drawFrame(r0, c, bw, bh, "NEW SKIRMISH");
        int ix = c + 3, iy = r0 + 1;

        auto header = [&](const char* h) {
            attron(COLOR_PAIR(CP_UI_DIM) | A_BOLD); mvprintw(iy++, ix, "%s", h); attroff(COLOR_PAIR(CP_UI_DIM) | A_BOLD);
        };
        auto field = [&](int rowId, const char* label, const char* value) {
            bool f = (sel == rowId);
            mvaddstr(iy, ix-2, f ? u8"›" : " ");
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a); mvprintw(iy, ix, "%-12s", label); attroff(a);
            int va = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_HIGH);
            attron(va); mvprintw(iy, ix+12, "%s", value); attroff(va);
            iy++;
        };

        char opp[20]; snprintf(opp, sizeof opp, "%d  (vs AI)", cfgNumAIs);
        header("OPPONENTS");
        field(R_OPP,  "Opponents",  opp);
        field(R_DIFF, "Difficulty", kDiffNames[cfgDiff]);
        header("BATTLEFIELD");
        field(R_LAYOUT, "Layout",  kLayoutNames[cfgLayout]);
        field(R_CLIM,   "Climate", kClimateNames[cfgClim]);
        field(R_BROWSE, "Browse maps", cfgSeed ? u8"chosen ▸" : u8"▸");
        header("PLAYER");
        field(R_CIV, "Civilisation", civLabel(cfgCiv));
        int colourRow = iy;
        field(R_COLOUR, "Colour", teamColorName(g.playerColor));
        attron(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD); mvaddstr(colourRow, ix+22, "##"); attroff(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD);
        field(R_SPEED,  "Game speed", kSpeedNames[cfgSpeed]);

        // Begin action — last row inside the frame.
        bool bf = (sel == R_BEGIN);
        mvaddstr(iy+1, ix-2, bf ? u8"›" : " ");
        attron(bf ? (COLOR_PAIR(CP_GOLD) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT));
        mvprintw(iy+1, ix, "Begin battle");
        attroff(bf ? (COLOR_PAIR(CP_GOLD) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT));

        // The chosen civ's character, under the frame.
        if (cfgCiv >= 0) {
            attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(r0 + bh,     c, "+ %s", CIVS[cfgCiv].bonus); attroff(COLOR_PAIR(CP_UI_HIGH));
            attron(COLOR_PAIR(CP_UI_DIM));  mvprintw(r0 + bh + 1, c, "- %s", CIVS[cfgCiv].lack);  attroff(COLOR_PAIR(CP_UI_DIM));
        }
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(r0 + bh + 2, c, "↑↓ select   ←→ change   Enter choose   Esc back");
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q' || ch==8 || ch==127) return false;   // Esc/Q/Backspace
        else if (ch==KEY_UP   || ch=='k') sel = (sel + R_COUNT - 1) % R_COUNT;
        else if (ch==KEY_DOWN || ch=='j') sel = (sel + 1) % R_COUNT;
        else if (ch==KEY_LEFT)  adjust(sel, -1);
        else if (ch==KEY_RIGHT) adjust(sel, +1);
        else if (ch=='v' || ch=='V') openPicker();
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if      (sel == R_BROWSE) openPicker();
            else if (sel == R_BEGIN) {
                g.difficulty   = cfgDiff;
                gameSpeed      = (GameSpeed)cfgSpeed;
                g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : kClimBiome[cfgClim];
                g.layoutChoice = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
                g.civChoice[0] = cfgCiv;
                for (int i = 1; i < MAX_PLAYERS; i++) g.civChoice[i] = -1;
                outSeed = cfgSeed;
                saveMenuConfig();
                return true;
            }
            else adjust(sel, +1);   // Enter on a value row cycles it forward
        }
    }
}

// Top-level main menu (AoE2/BW style): a vertical list of choices. Skirmish
// opens the grouped setup; Multiplayer the lobby flows; Load the slot
// browser; Controls the key reference. Fills `r` with what to play.
void showSplash(SplashResult& r) {
    enum { MI_SKIRMISH, MI_MULTI, MI_LOAD, MI_REPLAYS, MI_CONTROLS, MI_QUIT };
#ifdef __EMSCRIPTEN__
    // Browser build: MULTIPLAYER runs through a WebSocket relay (a tab can't
    // open raw TCP), so the flow differs; QUIT would only freeze the canvas.
    static const char* items[] = { "SKIRMISH", "MULTIPLAYER", "LOAD GAME", "REPLAYS", "CONTROLS" };
    static const int   acts[]  = { MI_SKIRMISH, MI_MULTI, MI_LOAD, MI_REPLAYS, MI_CONTROLS };
    const int N = 5;
#else
    static const char* items[] = { "SKIRMISH", "MULTIPLAYER", "LOAD GAME", "REPLAYS", "CONTROLS", "QUIT" };
    static const int   acts[]  = { MI_SKIRMISH, MI_MULTI, MI_LOAD, MI_REPLAYS, MI_CONTROLS, MI_QUIT };
    const int N = 6;
#endif
    int sel = 0;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 9);
        drawRealmBanner(maxX, top);
        int my0 = top + 9, c = maxX/2 - 7;
        for (int i = 0; i < N; i++) {
            bool f = (i == sel);
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a);
            mvprintw(my0 + i, c, "%s  %s", f ? u8"›" : " ", items[i]);
            attroff(a);
        }
        attron(COLOR_PAIR(CP_UI_DIM));
#ifdef __EMSCRIPTEN__
        const char* hint = "↑↓ select    Enter choose";
#else
        const char* hint = "↑↓ select    Enter choose    Q quit";
#endif
        mvprintw(my0 + N + 2, maxX/2 - (int)strlen(hint)/2, "%s", hint);
        // Build fingerprint: multiplayer needs identical builds, and "which
        // version do you have?" should be answerable from the title screen.
        char ver[48];
        snprintf(ver, sizeof ver, "build %s - protocol v%d", __DATE__, netProtoVersion());
        mvprintw(maxY - 1, maxX - (int)strlen(ver) - 2, "%s", ver);
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
#ifndef __EMSCRIPTEN__
        if (ch=='q' || ch=='Q') { endwin(); exit(0); }
#endif
        if      (ch==KEY_UP   || ch=='k' || ch=='K') sel = (sel + N - 1) % N;
        else if (ch==KEY_DOWN || ch=='j' || ch=='J') sel = (sel + 1) % N;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            switch (acts[sel]) {
            case MI_SKIRMISH: if (skirmishSetup(r.seed)) { r.numAIs = cfgNumAIs; return; } break;
#ifdef __EMSCRIPTEN__
            case MI_MULTI:    if (showMultiplayerMenuWeb(r)) return; break;
#else
            case MI_MULTI:    if (showMultiplayerMenu(r)) return; break;
#endif
            case MI_LOAD:     { int s = showLoadMenu(); if (s > 0) { r.loadSlot = s; return; } break; }
            case MI_REPLAYS:  { std::string rp = showReplayMenu(); if (!rp.empty()) { r.replayPath = rp; return; } break; }
            case MI_CONTROLS: showControlsScreen(); break;
            case MI_QUIT:     endwin(); exit(0);
            }
        }
    }
}

// ============================================================
// MULTIPLAYER LOBBIES — host a game, browse LAN lobbies, or join a
// typed address (works across the internet via port-forward/Tailscale).
// ============================================================

// Bottom-line message helper for the lobby screens.
static void lobbyNote(int row, int col, const char* msg, int cp = CP_UI_DIM) {
    attron(COLOR_PAIR(cp)); mvprintw(row, col, "%s", msg); attroff(COLOR_PAIR(cp));
}

static NetMatchConfig mpCurrentCfg() {
    NetMatchConfig c;
    c.seed       = cfgSeed;   // 0 = rolled at Begin
    c.numAIs     = mpNumAIs;
    c.biome      = (cfgClim   >= kClimCount)   ? -1 : kClimBiome[cfgClim];
    c.layout     = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
    c.difficulty = cfgDiff;
    c.speed      = cfgSpeed;
    c.humanMask  = 3;         // host seat 0, challenger seat 1
    c.civ[0]     = cfgCiv;    // seat 1 is filled in by net.cpp from the client's pick
    return c;
}

#ifdef __EMSCRIPTEN__
// Browser multiplayer pairs by room code through a relay; these hold the
// current room + relay URL, set by the web MP menu before entering a lobby.
static std::string webRoom, webRelay;
#endif

// Host lobby: the skirmish settings plus a live connection panel. Any
// change is pushed to a seated challenger immediately, AoE2-style.
static bool hostLobby(SplashResult& r) {
#ifdef __EMSCRIPTEN__
    if (!netWebHost(webRoom.c_str(), webRelay.c_str())) {
#else
    if (!netHostOpen()) {
#endif
        // Almost always a lingering socket from a lobby closed seconds ago.
        timeout(-1);
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase(); drawRealmBanner(maxX, std::max(0, maxY/2 - 9));
        lobbyNote(maxY/2 + 2, maxX/2 - 24, "Couldn't open the lobby port (another Realm hosting?)", CP_HP_RED);
        lobbyNote(maxY/2 + 4, maxX/2 - 12, "Press any key to go back");
        refresh(); getch();
        return false;
    }
    netHostSetInfo(mpCurrentCfg());

    enum { R_OPP, R_DIFF, R_LAYOUT, R_CLIM, R_BROWSE, R_CIV, R_COLOUR, R_SPEED, R_BEGIN, R_COUNT };
    int sel = R_OPP;
    bool dirty = false;
    auto adjust = [&](int row, int d) {
        switch (row) {
            case R_OPP:    mpNumAIs = (mpNumAIs + d + 3) % 3; break;   // 0..2
            case R_DIFF:   cfgDiff   = (cfgDiff + d + 3) % 3; break;
            case R_LAYOUT: cfgLayout = (cfgLayout + d + (LAYOUT_COUNT+1)) % (LAYOUT_COUNT+1); cfgSeed = 0; break;
            case R_CLIM:   cfgClim   = (cfgClim + d + (kClimCount+1)) % (kClimCount+1); cfgSeed = 0; break;
            case R_CIV:    cfgCiv    = ((cfgCiv + 1) + d + (NUM_CIVS+1)) % (NUM_CIVS+1) - 1; break;
            case R_COLOUR: g.playerColor = (g.playerColor + d + numTeamColors()) % numTeamColors(); applyTeamColors(); break;
            case R_SPEED:  cfgSpeed  = (cfgSpeed + d + 3) % 3; break;
            default: return;
        }
        dirty = true;
    };
    auto openPicker = [&]() {
        g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : kClimBiome[cfgClim];
        g.layoutChoice = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
        unsigned long long sd = 0;
        if (showMapPreview(sd)) {
            cfgSeed = sd;
            cfgClim   = climSlotOf(g.biomeChoice);
            cfgLayout = (g.layoutChoice < 0) ? LAYOUT_COUNT : g.layoutChoice;
            dirty = true;
        }
    };

#ifndef __EMSCRIPTEN__
    std::vector<std::string> addrs = netLocalAddresses();
#endif
    while (true) {
        bool wasSeated = netHostClientPresent();
        netHostPoll();
        if (wasSeated && netConnectionLost()) {
            // Challenger left in the lobby: reopen and keep waiting.
#ifdef __EMSCRIPTEN__
            netWebHost(webRoom.c_str(), webRelay.c_str());
#else
            netHostOpen();
#endif
            netHostSetInfo(mpCurrentCfg());
        }
        if (dirty) { netHostSetInfo(mpCurrentCfg()); dirty = false; }

        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 15);
        drawRealmBanner(maxX, top);
        const int bw = 46, bh = 15;
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        drawFrame(r0, c, bw, bh, "HOST A GAME");
        int ix = c + 3, iy = r0 + 1;

        auto header = [&](const char* h) {
            attron(COLOR_PAIR(CP_UI_DIM) | A_BOLD); mvprintw(iy++, ix, "%s", h); attroff(COLOR_PAIR(CP_UI_DIM) | A_BOLD);
        };
        auto field = [&](int rowId, const char* label, const char* value) {
            bool f = (sel == rowId);
            mvaddstr(iy, ix-2, f ? u8"›" : " ");
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a); mvprintw(iy, ix, "%-12s", label); attroff(a);
            int va = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_HIGH);
            attron(va); mvprintw(iy, ix+12, "%s", value); attroff(va);
            iy++;
        };

        char opp[24]; snprintf(opp, sizeof opp, "%d extra AI%s", mpNumAIs, mpNumAIs==1?"":"s");
        header("MATCH");
        field(R_OPP,  "AI seats",   opp);
        field(R_DIFF, "Difficulty", kDiffNames[cfgDiff]);
        header("BATTLEFIELD");
        field(R_LAYOUT, "Layout",  kLayoutNames[cfgLayout]);
        field(R_CLIM,   "Climate", kClimateNames[cfgClim]);
        field(R_BROWSE, "Browse maps", cfgSeed ? u8"chosen ▸" : u8"▸");
        header("PLAYER");
        field(R_CIV, "Civilisation", civLabel(cfgCiv));
        int colourRow = iy;
        field(R_COLOUR, "Colour", teamColorName(g.playerColor));
        attron(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD); mvaddstr(colourRow, ix+22, "##"); attroff(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD);
        field(R_SPEED,  "Game speed", kSpeedNames[cfgSpeed]);

        bool seated = netHostClientPresent();
        bool bf = (sel == R_BEGIN);
        mvaddstr(iy+1, ix-2, bf ? u8"›" : " ");
        attron(bf ? (COLOR_PAIR(seated ? CP_GOLD : CP_UI_DIM) | A_BOLD)
                  : COLOR_PAIR(seated ? CP_UI_TEXT : CP_UI_DIM));
        mvprintw(iy+1, ix, seated ? "Begin battle" : "Begin battle (waiting for a challenger)");
        attroff(bf ? (COLOR_PAIR(seated ? CP_GOLD : CP_UI_DIM) | A_BOLD)
                   : COLOR_PAIR(seated ? CP_UI_TEXT : CP_UI_DIM));

        // Connection panel under the frame.
        int py = r0 + bh + 1;
        if (seated) {
            attron(COLOR_PAIR(CP_HP_GREEN) | A_BOLD);
            mvprintw(py, c, "%s has joined! Begin when ready.", netHostClientName().c_str());
            attroff(COLOR_PAIR(CP_HP_GREEN) | A_BOLD);
        } else {
            std::string relayWhy;
#ifdef __EMSCRIPTEN__
            relayWhy = netRelayError();
#endif
            if (!relayWhy.empty()) {
                attron(COLOR_PAIR(CP_HP_RED) | A_BOLD);
                mvprintw(py, c, "Relay problem: %s", relayWhy.c_str());
                attroff(COLOR_PAIR(CP_HP_RED) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_UI_HIGH));
                mvprintw(py, c, "Lobby open - waiting for a challenger...");
                attroff(COLOR_PAIR(CP_UI_HIGH));
            }
        }
#ifdef __EMSCRIPTEN__
        attron(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
        mvprintw(py+1, c, "Room code: %s", webRoom.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
        lobbyNote(py+1, c + 12 + (int)webRoom.size() + 3, "— give this to your friend");
        lobbyNote(py+2, c, ("Relay: " + webRelay).c_str());
#else
        std::string addrLine = "LAN: friends pick Join via LAN. Direct: ";
        for (size_t i = 0; i < addrs.size() && i < 2; i++)
            addrLine += (i ? " or " : "") + addrs[i];
        if (addrs.empty()) addrLine += "(no network?)";
        lobbyNote(py+1, c, addrLine.c_str());
        lobbyNote(py+2, c, "Internet play: forward TCP 7521 to this Mac, or share a Tailscale IP.");
#endif
        lobbyNote(py+3, c, "↑↓ select   ←→ change   Enter choose   Esc back");
        refresh();

        timeout(90);
        int ch = getch();
        timeout(-1);
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') { netClose(); return false; }
        else if (ch==KEY_UP   || ch=='k') sel = (sel + R_COUNT - 1) % R_COUNT;
        else if (ch==KEY_DOWN || ch=='j') sel = (sel + 1) % R_COUNT;
        else if (ch==KEY_LEFT)  adjust(sel, -1);
        else if (ch==KEY_RIGHT) adjust(sel, +1);
        else if (ch=='v' || ch=='V') openPicker();
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if      (sel == R_BROWSE) openPicker();
            else if (sel == R_BEGIN && seated) {
                if (cfgSeed == 0)
                    cfgSeed = (unsigned long long)time(nullptr) * 2654435761ull + 1;
                NetMatchConfig fin = mpCurrentCfg();
                netHostSetInfo(fin);          // final settings incl. the real seed
                if (!netHostStart()) { netClose(); return false; }
                saveMenuConfig();
                r.netPlay = true; r.netCfg = fin; r.netSlot = 0;
                cfgSeed = 0;                  // next lobby rolls fresh again
                return true;
            }
            else adjust(sel, +1);
        }
    }
}

// Client lobby: whatever the host settles on, mirrored live, plus a local
// colour pick. Waits for the host's START.
static bool clientLobby(SplashResult& r) {
    NetMatchConfig cfgIn;
    bool haveCfg = false;
    netSendCivPick(cfgCiv);        // declare (or re-declare) our banner
    while (true) {
        int rc = netClientPoll(cfgIn);
        if (rc == 1) haveCfg = true;
        if (rc == 2) {
            r.netPlay = true; r.netCfg = cfgIn; r.netSlot = 1;
            return true;
        }
        if (rc < 0) {
            timeout(-1);
            int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
            erase(); drawRealmBanner(maxX, std::max(0, maxY/2 - 9));
            std::string relayWhy;
#ifdef __EMSCRIPTEN__
            relayWhy = netRelayError();   // the relay's own reason, if any
#endif
            if (netVersionMismatch())
                lobbyNote(maxY/2 + 2, maxX/2 - 27, "Your builds differ - you and the host need the same Realm version.", CP_HP_RED);
            else if (!relayWhy.empty())
                lobbyNote(maxY/2 + 2, maxX/2 - (int)(("Couldn't join: " + relayWhy).size())/2,
                          ("Couldn't join: " + relayWhy).c_str(), CP_HP_RED);
            else
                lobbyNote(maxY/2 + 2, maxX/2 - 22, "Lost the host (they closed the lobby).", CP_HP_RED);
            lobbyNote(maxY/2 + 4, maxX/2 - 12, "Press any key to go back");
            refresh(); getch();
            netClose();
            return false;
        }

        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 13);
        drawRealmBanner(maxX, top);
        const int bw = 44, bh = 12;
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        char title[64];
        snprintf(title, sizeof title, "%s'S GAME", netPeerName().c_str());
        for (char* t = title; *t; t++) *t = toupper((unsigned char)*t);
        drawFrame(r0, c, bw, bh, title);
        int ix = c + 3, iy = r0 + 2;
        attron(COLOR_PAIR(CP_UI_TEXT));
        if (haveCfg) {
            mvprintw(iy++, ix, "AI seats     %d", cfgIn.numAIs);
            mvprintw(iy++, ix, "Difficulty   %s", kDiffNames[std::max(0,std::min(2,cfgIn.difficulty))]);
            mvprintw(iy++, ix, "Layout       %s", kLayoutNames[(cfgIn.layout<0||cfgIn.layout>=LAYOUT_COUNT)?LAYOUT_COUNT:cfgIn.layout]);
            mvprintw(iy++, ix, "Climate      %s", kClimateNames[climSlotOf(cfgIn.biome)]);
            mvprintw(iy++, ix, "Game speed   %s", kSpeedNames[std::max(0,std::min(2,cfgIn.speed))]);
        } else {
            mvprintw(iy++, ix, "Reading the host's settings...");
        }
        attroff(COLOR_PAIR(CP_UI_TEXT));
        iy++;
        attron(COLOR_PAIR(CP_UI_HIGH));
        mvprintw(iy, ix, "Civ  [  %s  ]", civLabel(cfgCiv));
        iy++;
        mvprintw(iy, ix, "Colour  <  %s  >", teamColorName(g.playerColor));
        attroff(COLOR_PAIR(CP_UI_HIGH));
        attron(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD); mvaddstr(iy, ix+24, "##"); attroff(COLOR_PAIR(CP_MM_PLAYER) | A_BOLD);
        if (cfgCiv >= 0) {
            lobbyNote(r0 + bh,     c, (std::string("+ ") + CIVS[cfgCiv].bonus).c_str(), CP_UI_HIGH);
            lobbyNote(r0 + bh + 1, c, (std::string("- ") + CIVS[cfgCiv].lack).c_str());
        }
        lobbyNote(r0 + bh + 2, c, "[ ] civilisation   < > colour   Esc leave   (host starts the match)", CP_UI_HIGH);
        refresh();

        timeout(90);
        int ch = getch();
        timeout(-1);
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') { netSendBye(); netClose(); return false; }
        else if (ch==KEY_LEFT)  { g.playerColor = (g.playerColor + numTeamColors() - 1) % numTeamColors(); applyTeamColors(); }
        else if (ch==KEY_RIGHT) { g.playerColor = (g.playerColor + 1) % numTeamColors(); applyTeamColors(); }
        else if (ch=='[') { cfgCiv = ((cfgCiv + 1) + (NUM_CIVS+1) - 1) % (NUM_CIVS+1) - 1; netSendCivPick(cfgCiv); }
        else if (ch==']') { cfgCiv = ((cfgCiv + 1) + 1) % (NUM_CIVS+1) - 1; netSendCivPick(cfgCiv); }
    }
}

#ifndef __EMSCRIPTEN__
// Type-an-address entry (long-distance play: the host's public IP with TCP
// 7521 forwarded, or their Tailscale address).
static bool joinByAddress(SplashResult& r) {
    std::string addr = cfgLastAddr;   // last game's host is one Enter away
    std::string err;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 11);
        drawRealmBanner(maxX, top);
        const int bw = 52, bh = 6;
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        drawFrame(r0, c, bw, bh, "JOIN BY ADDRESS");
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(r0+2, c+3, "Host address:");
        attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
        mvprintw(r0+2, c+17, "%s_", addr.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
        if (!err.empty()) lobbyNote(r0+4, c+3, err.c_str(), CP_HP_RED);
        lobbyNote(r0 + bh, c, "IP or hostname (Tailscale names work).  Enter connect  Esc back");
        refresh();

        timeout(-1);
        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch == 27) return false;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if (addr.empty()) continue;
            erase(); drawRealmBanner(maxX, top);
            lobbyNote(r0+2, c+3, ("Connecting to " + addr + "...").c_str(), CP_UI_HIGH);
            refresh();
            std::string why;
            if (netJoinConnect(addr.c_str(), NET_TCP_PORT, why)) {
                cfgLastAddr = addr;
                saveMenuConfig();
                if (clientLobby(r)) return true;
                return false;
            }
            err = why;
        }
        else if (
#ifdef KEY_BACKSPACE
                 ch==KEY_BACKSPACE ||   // terminal build; the SDL shim sends ASCII 8
#endif
                 ch==127 || ch==8) { if (!addr.empty()) addr.pop_back(); }
        else if (ch >= 32 && ch < 127 && addr.size() < 40) addr.push_back((char)ch);
    }
}

// LAN browser: broadcast-discovered lobbies, live-refreshed.
static bool joinLanBrowse(SplashResult& r) {
    netDiscoverStart();
    std::vector<NetLobbyInfo> list;
    int sel = 0;
    while (true) {
        netDiscoverPoll(list);
        if (sel >= (int)list.size()) sel = list.empty() ? 0 : (int)list.size() - 1;

        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 12);
        drawRealmBanner(maxX, top);
        const int bw = 56, bh = 4 + std::max(3, (int)list.size() + 1);
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        drawFrame(r0, c, bw, bh, "GAMES ON YOUR NETWORK");
        int iy = r0 + 2;
        if (list.empty()) {
            attron(COLOR_PAIR(CP_UI_DIM));
            mvprintw(iy,   c+3, "Searching...");
            mvprintw(iy+1, c+3, "(the host must be sitting in their lobby)");
            attroff(COLOR_PAIR(CP_UI_DIM));
        }
        for (int i = 0; i < (int)list.size(); i++) {
            bool f = (i == sel);
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a);
            mvprintw(iy + i, c+3, "%s %-14s %-16s %s", f ? u8"›" : " ",
                     list[i].host.c_str(), list[i].addr.c_str(), list[i].map.c_str());
            attroff(a);
        }
        lobbyNote(r0 + bh, c, "Enter join   A type an address instead   Esc back");
        refresh();

        timeout(250);
        int ch = getch();
        timeout(-1);
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') { netDiscoverStop(); return false; }
        else if (ch==KEY_UP   && !list.empty()) sel = (sel + (int)list.size() - 1) % (int)list.size();
        else if (ch==KEY_DOWN && !list.empty()) sel = (sel + 1) % (int)list.size();
        else if (ch=='a' || ch=='A') { netDiscoverStop(); return joinByAddress(r); }
        else if ((ch=='\n' || ch=='\r' || ch==KEY_ENTER) && !list.empty()) {
            NetLobbyInfo pick = list[sel];
            netDiscoverStop();
            std::string why;
            if (netJoinConnect(pick.addr.c_str(), pick.port, why)) {
                if (clientLobby(r)) return true;
                netDiscoverStart();   // back to browsing
            } else {
                netDiscoverStart();
            }
        }
    }
}

// The MULTIPLAYER entry on the splash.
static bool showMultiplayerMenu(SplashResult& r) {
    static const char* items[] = { "HOST A GAME", "JOIN VIA LAN", "JOIN BY ADDRESS", "BACK" };
    const int N = 4;
    int sel = 0;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 9);
        drawRealmBanner(maxX, top);
        int my0 = top + 9, c = maxX/2 - 8;
        for (int i = 0; i < N; i++) {
            bool f = (i == sel);
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a);
            mvprintw(my0 + i, c, "%s  %s", f ? u8"›" : " ", items[i]);
            attroff(a);
        }
        attron(COLOR_PAIR(CP_UI_DIM));
        const char* hint = "Host = your friend joins you (AoE2-style lobby)";
        mvprintw(my0 + N + 2, maxX/2 - (int)strlen(hint)/2, "%s", hint);
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') return false;
        else if (ch==KEY_UP   || ch=='k' || ch=='K') sel = (sel + N - 1) % N;
        else if (ch==KEY_DOWN || ch=='j' || ch=='J') sel = (sel + 1) % N;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if      (sel == 0) { if (hostLobby(r))     return true; }
            else if (sel == 1) { if (joinLanBrowse(r)) return true; }
            else if (sel == 2) { if (joinByAddress(r)) return true; }
            else return false;
        }
    }
}
#else   // __EMSCRIPTEN__

// Two-field entry for browser multiplayer: the relay URL plus a room code
// (auto-generated for the host, typed by the joiner). Fills webRoom/webRelay;
// returns false if the user backs out. Menu-only — no sim state.
static bool webConnectForm(bool asHost) {
    if (webRelay.empty()) webRelay = REALM_RELAY_URL;
    if (asHost) {
        // A short, shareable code. Menu randomness only — never simRand.
        char code[8]; snprintf(code, sizeof code, "%04d", (int)(time(nullptr) % 10000));
        webRoom = code;
    }
    int field = asHost ? 1 : 0;      // 0 = room code, 1 = relay URL
    std::string err;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 11);
        drawRealmBanner(maxX, top);
        const int bw = 58, bh = 8;
        int c = std::max(2, maxX/2 - bw/2);
        int r0 = top + 8;
        drawFrame(r0, c, bw, bh, asHost ? "HOST A GAME" : "JOIN A GAME");
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(r0+2, c+3, "Room code:");
        mvprintw(r0+4, c+3, "Relay:");
        attroff(COLOR_PAIR(CP_UI_TEXT));
        int ca = (field==0) ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_HIGH);
        attron(ca); mvprintw(r0+2, c+14, "%s%s", webRoom.c_str(),  field==0?"_":""); attroff(ca);
        int la = (field==1) ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_HIGH);
        attron(la); mvprintw(r0+4, c+14, "%s%s", webRelay.c_str(), field==1?"_":""); attroff(la);
        lobbyNote(r0+6, c+3, asHost
            ? "Share the room code with your friend, then Enter to open the lobby."
            : "Enter your friend's room code and the same relay, then Enter.");
        if (!err.empty()) lobbyNote(r0+bh, c, err.c_str(), CP_HP_RED);
        else lobbyNote(r0+bh, c, "Tab / ↑↓ switch field   Enter connect   Esc back");
        refresh();

        timeout(-1);
        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch == 27) return false;
        else if (ch=='\t' || ch==KEY_UP || ch==KEY_DOWN) field ^= 1;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if (webRoom.empty())  { err = "Enter a room code.";     continue; }
            if (webRelay.empty()) { err = "Enter a relay address."; continue; }
            return true;
        }
        else if (ch==127 || ch==8
#ifdef KEY_BACKSPACE
                 || ch==KEY_BACKSPACE
#endif
                ) { std::string& s = (field==0) ? webRoom : webRelay; if (!s.empty()) s.pop_back(); }
        else if (ch >= 32 && ch < 127) {
            std::string& s = (field==0) ? webRoom : webRelay;
            if (s.size() < (size_t)(field==0 ? 16 : 80)) s.push_back((char)ch);
        }
    }
}

// The MULTIPLAYER entry on the browser splash — host or join a relay room.
static bool showMultiplayerMenuWeb(SplashResult& r) {
    static const char* items[] = { "HOST A GAME", "JOIN A GAME", "BACK" };
    const int N = 3;
    int sel = 0;
    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        erase();
        int top = std::max(0, maxY/2 - 9);
        drawRealmBanner(maxX, top);
        int my0 = top + 9, c = maxX/2 - 8;
        for (int i = 0; i < N; i++) {
            bool f = (i == sel);
            int a = f ? (COLOR_PAIR(CP_UI_HIGH) | A_BOLD) : COLOR_PAIR(CP_UI_TEXT);
            attron(a);
            mvprintw(my0 + i, c, "%s  %s", f ? u8"›" : " ", items[i]);
            attroff(a);
        }
        attron(COLOR_PAIR(CP_UI_DIM));
        const char* hint = "Play a friend browser-to-browser through a relay (room codes)";
        mvprintw(my0 + N + 2, maxX/2 - (int)strlen(hint)/2, "%s", hint);
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch==27 || ch=='q' || ch=='Q') return false;
        else if (ch==KEY_UP   || ch=='k' || ch=='K') sel = (sel + N - 1) % N;
        else if (ch==KEY_DOWN || ch=='j' || ch=='J') sel = (sel + 1) % N;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if (sel == 0) {                       // HOST — open the room NOW (no in-between
                                                  // form: the code screen used to sit here
                                                  // with no room open yet, so a friend who
                                                  // joined early got "no such room").
                if (webRelay.empty()) webRelay = REALM_RELAY_URL;
                char code[8]; snprintf(code, sizeof code, "%04d", (int)(time(nullptr) % 10000));
                webRoom = code;
                if (hostLobby(r)) return true;    // hostLobby calls netWebHost immediately
            } else if (sel == 1) {                // JOIN — connect, then mirror the host
                if (webConnectForm(false)) {
                    std::string why;
                    if (netWebJoin(webRoom.c_str(), webRelay.c_str(), why) && clientLobby(r))
                        return true;
                }
            } else return false;
        }
    }
}

#endif  // __EMSCRIPTEN__

// One preview cell = a BLOCK of world tiles (a thumbnail cell covers ~7x9 of
// them). The old point-sample read a single tile, so rivers aliased away and
// forests thinned to noise — every map looked like the same sparse meadow.
// Majority-vote the block with feature priority: rare, match-defining
// terrain (gold, keeps, water, woods) must survive the shrink.
static void previewCell(int x0, int y0, int x1, int y1, char& ch, int& cp) {
    int water=0, mtn=0, forest=0, gold=0, hills=0, sand=0, snow=0,
        dirt=0, crop=0, keep=0, lava=0, heath=0, total=0, highland=0;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
        if (!inBounds(x, y)) continue;
        total++;
        if (g.map[y][x].elev > 0) highland++;
        switch (g.map[y][x].terrain) {
            case T_WATER: case T_SHALLOWS: case T_REEDS: case T_MARSH: case T_FISH: water++; break;
            case T_MOUNTAIN: case T_STONE:                       mtn++;    break;
            case T_FOREST: case T_PINE: case T_PALM: case T_DEAD_TREE: forest++; break;
            case T_GOLD:                                         gold++;   break;
            case T_HILLS: case T_GRAVEL:                         hills++;  break;
            case T_SAND: case T_DUNES:                           sand++;   break;
            case T_SNOW: case T_ICE:                             snow++;   break;
            case T_DIRT: case T_MUD:                             dirt++;   break;
            case T_WHEAT: case T_BERRY:                          crop++;   break;
            case T_CASTLE_WALL: case T_CASTLE_GATE: case T_CASTLE_FLOOR:
            case T_RUINS: case T_MONOLITH:                       keep++;   break;
            case T_LAVA: case T_ASH:                             lava++;   break;
            case T_HEATH:                                        heath++;  break;
            default: break;
        }
    }
    if (total == 0) { ch = ' '; cp = CP_FOG; return; }
    // Priority ladder: the rarer and more match-defining, the earlier.
    if (gold > 0)               { ch = '$'; cp = CP_MM_GOLD;   return; }
    if (keep > 0)               { ch = '#'; cp = CP_MM_CASTLE; return; }
    if (lava * 4 >= total)      { ch = '^'; cp = CP_LAVA;      return; }
    if (water * 4 >= total)     { ch = '~'; cp = CP_MM_WATER;  return; }
    if (mtn * 8 >= total)       { ch = '^'; cp = CP_MM_MTN;    return; }
    if (forest * 3 >= total)    { ch = '*'; cp = CP_MM_FOREST; return; }   // deep woods
    if (forest * 7 >= total)    { ch = '\''; cp = CP_MM_FOREST; return; } // scattered trees
    if (hills * 4 >= total)     { ch = 'n'; cp = CP_MM_MTN;    return; }
    if (crop * 5 >= total)      { ch = '"'; cp = CP_GRASS_DRY; return; }
    if (snow * 2 >= total)      { ch = '.'; cp = CP_MM_SNOW;   return; }
    if (heath * 2 >= total)     { ch = ':'; cp = CP_MM_HEATH;  return; }
    if (sand * 2 >= total)      { ch = '.'; cp = CP_MM_SAND;   return; }
    if (dirt * 2 >= total)      { ch = ','; cp = CP_MM_SAND;   return; }
    // Open ground; highland plateaus shade differently so the cliff shapes
    // (the chokepoints that decide fights) read at a glance.
    if (highland * 2 >= total)  { ch = ':'; cp = CP_MM_MTN;    return; }
    ch = '.'; cp = CP_GRASS;
}

// AoE2-style battlefield picker: a grid of real generated minimaps. Browse
// seeds, cycle layout ([ ]) and climate (< >), reroll (R). The exact seed of
// the chosen thumbnail is handed back so the map you preview is the map you
// play; g.layoutChoice and g.biomeChoice are set to the committed combo.
// Returns false if the player backs out to the splash screen.
static bool showMapPreview(unsigned long long& outSeed) {
    int maxY0, maxX0; getmaxyx(stdscr, maxY0, maxX0);
    // A scrollable wall of battlefields. We show a grid window and let the
    // player scroll through a much larger pool (a dozen to a few dozen maps).
    const int COLS = (maxX0 >= 128) ? 4 : (maxX0 >= 92) ? 3 : 2;
    const int VIS_ROWS = (maxY0 >= 30) ? 3 : 2;       // rows visible at once
    const int N = COLS * VIS_ROWS;                    // cells on screen
    const int PAGES = 3;
    const int POOL = N * PAGES;                        // total options to browse
    const int TOTAL_ROWS = (POOL + COLS - 1) / COLS;
    const int TW = std::max(16, std::min(30, (maxX0 - (COLS+1)*2)/COLS));
    const int TH = std::max(7,  std::min(13, (maxY0 - 8)/VIS_ROWS - 2));

    static const char* layName[]  = {"Continental","Highlands","Deep Woods","River","Islands","Plains","Delta","Vale","Canyons"};
    auto climLabel = [](int biome) { return biome < 0 ? "Mixed" : kClimateNames[climSlotOf(biome)]; };
    struct Cand { bool ready=false; unsigned long long seed=0; int lay=0, clim=0; std::string name; std::vector<char> ch; std::vector<int> cp; };
    std::vector<Cand> cand(POOL);
    int layFilter  = g.layoutChoice;                 // -1 = random
    int climFilter = g.biomeChoice;                  // -1 = mixed
    int savedLay = g.layoutChoice, savedClim = g.biomeChoice;   // restored on back-out
    unsigned long long base = (unsigned long long)time(nullptr) * 2654435761ull + 1;
    int sel = 0, top = 0;

    // Distinct-layout / distinct-climate orderings (incl. one Mixed), reshuffled
    // each reroll, so consecutive thumbnails never look alike.
    const int NCLIM = kClimCount + 1;             // every climate + one Mixed
    int layOrder[LAYOUT_COUNT], climOrder[kClimCount + 1];
    auto reshuffle = [&]() {
        for (int i = 0; i < LAYOUT_COUNT; i++) layOrder[i] = i;
        for (int i = LAYOUT_COUNT-1; i > 0; i--) { int j=(int)((base>>(i*3+1))%(i+1)); std::swap(layOrder[i],layOrder[j]); }
        climOrder[0] = -1;
        for (int i = 0; i < kClimCount; i++) climOrder[i+1] = kClimBiome[i];
        for (int i = NCLIM-1; i > 0; i--) { int j=(int)((base>>(i*5+2))%(i+1)); std::swap(climOrder[i],climOrder[j]); }
    };
    // Generate one thumbnail on demand (lazy — only maps you actually scroll to
    // are built, so a 24-map wall stays snappy).
    auto ensure = [&](int i) {
        if (i < 0 || i >= POOL || cand[i].ready) return;
        unsigned long long s = base + (unsigned long long)(i+1)*0x9E3779B97F4A7C15ull;
        if (s == 0) s = 1;
        int lay  = (layFilter  < 0) ? layOrder[i % LAYOUT_COUNT] : layFilter;
        int clim = (climFilter < 0) ? climOrder[i % NCLIM]       : climFilter;
        g.layoutChoice = lay; g.biomeChoice = clim;
        seedSimRng(s); generateMap();
        cand[i].seed = s; cand[i].lay = lay; cand[i].clim = clim;
        cand[i].name = makeMapName(s, lay, clim);
        cand[i].ch.assign(TW*TH, '.'); cand[i].cp.assign(TW*TH, CP_GRASS);
        for (int yy = 0; yy < TH; yy++) for (int xx = 0; xx < TW; xx++) {
            char c; int cp;
            previewCell(xx*MAP_W/TW, yy*MAP_H/TH,
                        (xx+1)*MAP_W/TW, (yy+1)*MAP_H/TH, c, cp);
            cand[i].ch[yy*TW+xx] = c; cand[i].cp[yy*TW+xx] = cp;
        }
        g.entities.clear();   // discard any ruins spawned while generating previews
        cand[i].ready = true;
    };
    auto invalidate = [&]() { for (auto& cd : cand) cd.ready = false; sel = 0; top = 0; reshuffle(); };
    reshuffle();

    while (true) {
        int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
        // Keep the selected cell inside the visible window.
        int selRow = sel / COLS;
        if (selRow < top)             top = selRow;
        if (selRow >= top + VIS_ROWS) top = selRow - VIS_ROWS + 1;
        top = std::max(0, std::min(top, std::max(0, TOTAL_ROWS - VIS_ROWS)));

        erase();
        int gridW = COLS*(TW+2), gridH = VIS_ROWS*(TH+2);
        int ox = std::max(1, maxX/2 - gridW/2), oy = std::max(4, maxY/2 - gridH/2);
        attron(A_TITLE|COLOR_PAIR(CP_UI_ACCENT));
        mvprintw(oy-3, ox, "CHOOSE YOUR BATTLEFIELD");
        attroff(A_TITLE|COLOR_PAIR(CP_UI_ACCENT));
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(oy-2, ox, "Arrows/HJKL move   PgUp/Dn scroll   [ ] layout   < > climate   R reroll   Enter begin   Q back");
        mvprintw(oy-1, ox, "Layout = the land's SHAPE  ~  Climate = its palette");
        attroff(COLOR_PAIR(CP_UI_DIM));

        for (int vr = 0; vr < VIS_ROWS; vr++) for (int c = 0; c < COLS; c++) {
            int i = (top + vr)*COLS + c;
            if (i >= POOL) continue;
            ensure(i);
            int cx = ox + c*(TW+2) + 1, cy = oy + vr*(TH+2) + 1;
            bool isSel = (i == sel);
            int bc = isSel ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_DIM);
            attron(bc);
            mvhline(cy-1, cx, isSel?'=':'-', TW); mvhline(cy+TH, cx, isSel?'=':'-', TW);
            for (int r=0; r<TH; r++) { mvaddch(cy+r, cx-1, '|'); mvaddch(cy+r, cx+TW, '|'); }
            attroff(bc);
            for (int r=0; r<TH; r++) for (int cc=0; cc<TW; cc++) {
                int cp = cand[i].cp[r*TW+cc];
                attron(COLOR_PAIR(cp)); mvaddch(cy+r, cx+cc, cand[i].ch[r*TW+cc]); attroff(COLOR_PAIR(cp));
            }
            // Top border: WHAT it is (layout · climate) — the axis people
            // actually choose on. Bottom border: the evocative name.
            char kind[48];
            snprintf(kind, sizeof kind, " %s ~ %s ", layName[cand[i].lay], climLabel(cand[i].clim));
            kind[std::min((int)strlen(kind), TW)] = '\0';
            attron(isSel ? (COLOR_PAIR(CP_UI_ACCENT)|A_BOLD) : COLOR_PAIR(CP_UI_DIM));
            mvprintw(cy-1, cx+1, "%s", kind);
            attroff(isSel ? (COLOR_PAIR(CP_UI_ACCENT)|A_BOLD) : COLOR_PAIR(CP_UI_DIM));
            char title[64];
            snprintf(title, sizeof title, " %s ", cand[i].name.c_str());
            title[std::min((int)strlen(title), TW)] = '\0';   // keep inside the frame
            attron(isSel ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_TEXT));
            mvprintw(cy+TH, cx+1, "%s", title);
            attroff(isSel ? (COLOR_PAIR(CP_UI_HIGH)|A_BOLD) : COLOR_PAIR(CP_UI_TEXT));
        }

        // Scroll hints: show which rows have more above/below.
        attron(COLOR_PAIR(CP_UI_DIM));
        if (top > 0)                        mvprintw(oy-1,        ox + gridW + 1, "^");
        if (top + VIS_ROWS < TOTAL_ROWS)    mvprintw(oy+gridH-1,  ox + gridW + 1, "v");
        attroff(COLOR_PAIR(CP_UI_DIM));

        // Selected map's full billing: name + layout · climate, and its index.
        ensure(sel);
        attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        mvprintw(oy + gridH + 1, ox, "%s", cand[sel].name.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(oy + gridH + 1, ox + (int)cand[sel].name.size() + 2, "— %s · %s   (%d/%d)",
                 layName[cand[sel].lay], climLabel(cand[sel].clim), sel+1, POOL);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(oy + gridH + 2, ox, "Filter: %s layout, %s climate",
                 layFilter < 0 ? "Random" : layName[layFilter], climLabel(climFilter));
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int c = getch();
        if (c == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (c=='q'||c=='Q')                       { g.layoutChoice = savedLay; g.biomeChoice = savedClim; return false; }
        else if (c=='\n'||c==KEY_ENTER||c=='\r')  { ensure(sel); g.layoutChoice = cand[sel].lay; g.biomeChoice = cand[sel].clim; outSeed = cand[sel].seed; return true; }
        else if (c==KEY_RIGHT||c=='l'||c=='L')    sel = std::min(POOL-1, sel+1);
        else if (c==KEY_LEFT ||c=='h'||c=='H')    sel = std::max(0, sel-1);
        else if (c==KEY_DOWN ||c=='j'||c=='J')    sel = std::min(POOL-1, sel+COLS);
        else if (c==KEY_UP   ||c=='k'||c=='K')    sel = std::max(0, sel-COLS);
        else if (c==KEY_NPAGE)                    sel = std::min(POOL-1, sel + COLS*VIS_ROWS);
        else if (c==KEY_PPAGE)                    sel = std::max(0,      sel - COLS*VIS_ROWS);
        else if (c=='r'||c=='R')                  { base = base*6364136223846793005ull + 1442695040888963407ull; invalidate(); }
        else if (c==']')                          { layFilter  = (layFilter  >= LAYOUT_COUNT-1) ? -1 : layFilter+1;  invalidate(); }
        else if (c=='[')                          { layFilter  = (layFilter  < 0) ? LAYOUT_COUNT-1 : layFilter-1;    invalidate(); }
        else if (c=='.'||c=='>')                  { int sl = climSlotOf(climFilter); sl = (sl + 1) % (kClimCount + 1);
                                                    climFilter = (sl == kClimCount) ? -1 : kClimBiome[sl]; invalidate(); }
        else if (c==','||c=='<')                  { int sl = climSlotOf(climFilter); sl = (sl + kClimCount) % (kClimCount + 1);
                                                    climFilter = (sl == kClimCount) ? -1 : kClimBiome[sl]; invalidate(); }
    }
}

