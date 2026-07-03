#include "realm.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <locale.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

// Millisecond sleep for the headless harness loops (POSIX + Windows).
static void msleep(int ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

static bool isUtf8LocaleName(const char* name) {
    if (!name) return false;
    return std::strstr(name, "UTF-8") || std::strstr(name, "utf8")
        || std::strstr(name, "UTF8")  || std::strstr(name, "65001");
}

static void forceUtf8Locale() {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    const char* loc = setlocale(LC_ALL, "");
    if (isUtf8LocaleName(loc)) return;

    loc = setlocale(LC_ALL, "C.UTF-8");
    if (isUtf8LocaleName(loc)) return;

    loc = setlocale(LC_ALL, "en_US.UTF-8");
    if (isUtf8LocaleName(loc)) return;

    loc = setlocale(LC_ALL, ".UTF-8");
    if (isUtf8LocaleName(loc)) return;

    loc = setlocale(LC_ALL, ".UTF8");
    if (isUtf8LocaleName(loc)) return;
}


// Full splash screen. Sets g.biomeChoice, g.difficulty and game speed.
// Returns numAIs. Banner is block-art; headers use A_TITLE, which the SDL
// build renders in a blackletter face (Luminari) — the terminal gets bold.
static bool showMapPreview(unsigned long long& outSeed);   // visual battlefield picker, below
static int  showLoadMenu();                                // saved-game browser, below

// What the splash resolved to: a skirmish, a saved game, or a connected
// network match (host seat 0 / client seat 1, config agreed in the lobby).
struct SplashResult {
    int numAIs = 1;
    unsigned long long seed = 0;
    int loadSlot = 0;
    bool netPlay = false;
    NetMatchConfig netCfg;
    int netSlot = 0;
    std::string replayPath;   // non-empty: watch this recording
};
static bool showMultiplayerMenu(SplashResult& r);          // lobby flows, below

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
static const char* kClimateNames[] = { "Temperate","Desert","Snow","Swamp","Forest","Random" };
static const char* kLayoutNames[]  = { "Continental","Highlands","Deep Woods","River","Islands","Plains","Random" };
static const char* kDiffNames[]    = { "Easy","Normal","Hard" };
static const char* kSpeedNames[]   = { "Slow","Normal","Fast" };
static const int   kClimCount = 5;             // climates 0..4; index 5 = Random/mixed
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

// ---- Settings persistence: the splash remembers you between launches ----
// Plain key=value file next to the saves. Only menu preferences — nothing
// sim-critical lives here (match config still travels via lobby/replay).
static void saveMenuConfig() {
    FILE* f = fopen("realm-config.txt", "w");
    if (!f) return;
    fprintf(f, "ais=%d\ndiff=%d\nclim=%d\nlayout=%d\nspeed=%d\nciv=%d\ncolour=%d\nmpais=%d\n",
            cfgNumAIs, cfgDiff, cfgClim, cfgLayout, cfgSpeed, cfgCiv, g.playerColor, mpNumAIs);
    fclose(f);
}
static void loadMenuConfig() {
    FILE* f = fopen("realm-config.txt", "r");
    if (!f) return;
    char k[32]; int v;
    while (fscanf(f, "%31[^=]=%d\n", k, &v) == 2) {
        if      (!strcmp(k, "ais"))    cfgNumAIs = std::max(1, std::min(3, v));
        else if (!strcmp(k, "diff"))   cfgDiff   = std::max(0, std::min(2, v));
        else if (!strcmp(k, "clim"))   cfgClim   = std::max(0, std::min(kClimCount, v));
        else if (!strcmp(k, "layout")) cfgLayout = std::max(0, std::min((int)LAYOUT_COUNT, v));
        else if (!strcmp(k, "speed"))  cfgSpeed  = std::max(0, std::min(2, v));
        else if (!strcmp(k, "civ"))    cfgCiv    = std::max(-1, std::min(NUM_CIVS - 1, v));
        else if (!strcmp(k, "colour")) g.playerColor = std::max(0, std::min(numTeamColors() - 1, v));
        else if (!strcmp(k, "mpais"))  mpNumAIs  = std::max(0, std::min(2, v));
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
        attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(r+11, c, "Press any key to go back"); attroff(COLOR_PAIR(CP_UI_DIM));
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
        g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : cfgClim;
        g.layoutChoice = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
        unsigned long long s = 0;
        if (showMapPreview(s)) {
            cfgSeed = s;
            cfgClim   = (g.biomeChoice  < 0) ? kClimCount   : g.biomeChoice;
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
                g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : cfgClim;
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
static void showSplash(SplashResult& r) {
    static const char* items[] = { "SKIRMISH", "MULTIPLAYER", "LOAD GAME", "REPLAYS", "CONTROLS", "QUIT" };
    const int N = 6;
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
        const char* hint = "↑↓ select    Enter choose    Q quit";
        mvprintw(my0 + N + 2, maxX/2 - (int)strlen(hint)/2, "%s", hint);
        // Build fingerprint: multiplayer needs identical builds, and "which
        // version do you have?" should be answerable from the title screen.
        char ver[48];
        snprintf(ver, sizeof ver, "build %s - protocol v2", __DATE__);
        mvprintw(maxY - 1, maxX - (int)strlen(ver) - 2, "%s", ver);
        attroff(COLOR_PAIR(CP_UI_DIM));
        refresh();

        int ch = getch();
        if (ch == KEY_MOUSE) { MEVENT me; getmouse(&me); continue; }
        if (ch=='q' || ch=='Q') { endwin(); exit(0); }
        else if (ch==KEY_UP   || ch=='k' || ch=='K') sel = (sel + N - 1) % N;
        else if (ch==KEY_DOWN || ch=='j' || ch=='J') sel = (sel + 1) % N;
        else if (ch=='\n' || ch=='\r' || ch==KEY_ENTER) {
            if      (sel == 0) { if (skirmishSetup(r.seed)) { r.numAIs = cfgNumAIs; return; } }
            else if (sel == 1) { if (showMultiplayerMenu(r)) return; }
            else if (sel == 2) { int s = showLoadMenu(); if (s > 0) { r.loadSlot = s; return; } }
            else if (sel == 3) { std::string rp = showReplayMenu(); if (!rp.empty()) { r.replayPath = rp; return; } }
            else if (sel == 4) { showControlsScreen(); }
            else               { endwin(); exit(0); }
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
    c.biome      = (cfgClim   >= kClimCount)   ? -1 : cfgClim;
    c.layout     = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
    c.difficulty = cfgDiff;
    c.speed      = cfgSpeed;
    c.humanMask  = 3;         // host seat 0, challenger seat 1
    c.civ[0]     = cfgCiv;    // seat 1 is filled in by net.cpp from the client's pick
    return c;
}

// Host lobby: the skirmish settings plus a live connection panel. Any
// change is pushed to a seated challenger immediately, AoE2-style.
static bool hostLobby(SplashResult& r) {
    if (!netHostOpen()) {
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
        g.biomeChoice  = (cfgClim   >= kClimCount)   ? -1 : cfgClim;
        g.layoutChoice = (cfgLayout >= LAYOUT_COUNT) ? -1 : cfgLayout;
        unsigned long long sd = 0;
        if (showMapPreview(sd)) {
            cfgSeed = sd;
            cfgClim   = (g.biomeChoice  < 0) ? kClimCount   : g.biomeChoice;
            cfgLayout = (g.layoutChoice < 0) ? LAYOUT_COUNT : g.layoutChoice;
            dirty = true;
        }
    };

    std::vector<std::string> addrs = netLocalAddresses();
    while (true) {
        bool wasSeated = netHostClientPresent();
        netHostPoll();
        if (wasSeated && netConnectionLost()) {
            // Challenger left in the lobby: reopen and keep waiting.
            netHostOpen();
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
            attron(COLOR_PAIR(CP_UI_HIGH));
            mvprintw(py, c, "Lobby open - waiting for a challenger...");
            attroff(COLOR_PAIR(CP_UI_HIGH));
        }
        std::string addrLine = "LAN: friends pick Join via LAN. Direct: ";
        for (size_t i = 0; i < addrs.size() && i < 2; i++)
            addrLine += (i ? " or " : "") + addrs[i];
        if (addrs.empty()) addrLine += "(no network?)";
        lobbyNote(py+1, c, addrLine.c_str());
        lobbyNote(py+2, c, "Internet play: forward TCP 7521 to this Mac, or share a Tailscale IP.");
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
            lobbyNote(maxY/2 + 2, maxX/2 - 22, "Lost the host (they closed the lobby, or versions differ).", CP_HP_RED);
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
            mvprintw(iy++, ix, "Climate      %s", kClimateNames[(cfgIn.biome<0||cfgIn.biome>=kClimCount)?kClimCount:cfgIn.biome]);
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

// Type-an-address entry (long-distance play: the host's public IP with TCP
// 7521 forwarded, or their Tailscale address).
static bool joinByAddress(SplashResult& r) {
    std::string addr;
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

// One preview cell = a BLOCK of world tiles (a thumbnail cell covers ~7x9 of
// them). The old point-sample read a single tile, so rivers aliased away and
// forests thinned to noise — every map looked like the same sparse meadow.
// Majority-vote the block with feature priority: rare, match-defining
// terrain (gold, keeps, water, woods) must survive the shrink.
static void previewCell(int x0, int y0, int x1, int y1, char& ch, int& cp) {
    int water=0, mtn=0, forest=0, gold=0, hills=0, sand=0, snow=0,
        dirt=0, crop=0, keep=0, lava=0, total=0, highland=0;
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

    static const char* layName[]  = {"Continental","Highlands","Deep Woods","River","Islands","Plains"};
    static const char* climName[] = {"Temperate","Desert","Snow","Swamp","Forest"};
    struct Cand { bool ready=false; unsigned long long seed=0; int lay=0, clim=0; std::string name; std::vector<char> ch; std::vector<int> cp; };
    std::vector<Cand> cand(POOL);
    int layFilter  = g.layoutChoice;                 // -1 = random
    int climFilter = g.biomeChoice;                  // -1 = mixed
    int savedLay = g.layoutChoice, savedClim = g.biomeChoice;   // restored on back-out
    unsigned long long base = (unsigned long long)time(nullptr) * 2654435761ull + 1;
    int sel = 0, top = 0;

    // Distinct-layout / distinct-climate orderings (incl. one Mixed), reshuffled
    // each reroll, so consecutive thumbnails never look alike.
    int layOrder[LAYOUT_COUNT], climOrder[6];
    auto reshuffle = [&]() {
        for (int i = 0; i < LAYOUT_COUNT; i++) layOrder[i] = i;
        for (int i = LAYOUT_COUNT-1; i > 0; i--) { int j=(int)((base>>(i*3+1))%(i+1)); std::swap(layOrder[i],layOrder[j]); }
        int seed6[6] = { -1, B_TEMPERATE, B_DESERT, B_FOREST, B_SNOW, B_SWAMP };
        for (int i = 0; i < 6; i++) climOrder[i] = seed6[i];
        for (int i = 5; i > 0; i--) { int j=(int)((base>>(i*5+2))%(i+1)); std::swap(climOrder[i],climOrder[j]); }
    };
    // Generate one thumbnail on demand (lazy — only maps you actually scroll to
    // are built, so a 24-map wall stays snappy).
    auto ensure = [&](int i) {
        if (i < 0 || i >= POOL || cand[i].ready) return;
        unsigned long long s = base + (unsigned long long)(i+1)*0x9E3779B97F4A7C15ull;
        if (s == 0) s = 1;
        int lay  = (layFilter  < 0) ? layOrder[i % LAYOUT_COUNT] : layFilter;
        int clim = (climFilter < 0) ? climOrder[i % 6]           : climFilter;
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
            snprintf(kind, sizeof kind, " %s ~ %s ", layName[cand[i].lay],
                     cand[i].clim < 0 ? "Mixed" : climName[cand[i].clim]);
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
                 layName[cand[sel].lay], cand[sel].clim < 0 ? "Mixed lands" : climName[cand[sel].clim],
                 sel+1, POOL);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM));
        mvprintw(oy + gridH + 2, ox, "Filter: %s layout, %s climate",
                 layFilter < 0 ? "Random" : layName[layFilter],
                 climFilter < 0 ? "Mixed" : climName[climFilter]);
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
        else if (c=='.'||c=='>')                  { climFilter = (climFilter >= B_FOREST)      ? -1 : climFilter+1; invalidate(); }
        else if (c==','||c=='<')                  { climFilter = (climFilter < 0) ? B_FOREST   : climFilter-1;      invalidate(); }
    }
}

struct Spawn { int thX, thY; };

// Wipe every piece of per-match state, reseed the sim RNG, resolve the layout
// and battlefield name, and reset the players to their starting treasuries.
static void resetMatchState(unsigned long long seed) {
    g.simSeed = seed;
    seedSimRng(seed);
    // Resolve a random layout to a concrete one (deterministic from the seed,
    // without touching the sim RNG) so the AI and replay header see the real
    // topology. Climate may stay -1 (genuinely mixed bands).
    if (g.layoutChoice < 0 || g.layoutChoice >= LAYOUT_COUNT)
        g.layoutChoice = (int)(seed % LAYOUT_COUNT);
    // Name the battlefield from its final seed/layout/climate (same inputs the
    // picker used, so a previewed map keeps the exact name you chose).
    g.mapName = makeMapName(g.simSeed, g.layoutChoice, g.biomeChoice);
    g.pendingCmds.clear();
    // Critical: wipe every piece of per-match state so a new game can't see
    // entities, projectiles, IDs, or cached fog from the previous match.
    g.entities.clear();
    g.projectiles.clear();
    // Reserve generously: late-game FFA can hit a few hundred live entities plus
    // dead-but-not-yet-purged ones. Reallocating mid-tick would dangle the
    // `Entity& e` reference held by tickEntity while it calls spawnEntity (training,
    // building completion, etc.), corrupting heap state.
    g.entities.reserve(8192);
    g.projectiles.reserve(256);
    g.nextId = 1; g.tick = 0; g.mode = M_NORMAL;
    g.selectedId = -1; g.selectedIds.clear(); g.groupAssignPending = false;
    g.dragging = false; g.dragStartX = 0; g.dragStartY = 0;
    for (int i = 0; i < 9; i++) g.controlGroups[i].clear();
    g.winner = -1; g.aiTimer = 0; g.farmTimer = 0; g.statusTimer = 0;
    g.statusMsg.clear();
    g.weather = W_CLEAR; g.weatherTimer = 0;
    g.buildPending = E_NONE; g.wallDragX = 0; g.wallDragY = 0;
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f; g.prevSeason = -1;
    g.prevTimePhase = 0; g.attackNotifyCd = 0;
    g.returnToMenu = false;
    g.cursorByMouse = false;
    g.winterSeverity = 1;
    // g.difficulty is match config like biomeChoice — set by the splash /
    // replay header / verify harness before initGame; never reset here.
    // Invalidate per-tick detection cache so the new match (which starts at
    // tick=0 again) can't accidentally share a row with last match's tick 0.
    resetDetectMapCache();
    // biomeChoice is set by showSplash before initGame is called; don't reset it here.
    for (int p = 0; p < MAX_PLAYERS; p++)
        g.players[p] = {300, 200, 100, 0, 0, true, 0, 0, ERA_HAMLET, CIV_FREEHOLDERS, 0, 0};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true, 0, 0, ERA_HAMLET, CIV_FREEHOLDERS, 0, 0};
}

// Score and space out player spawns, place each side's Town Hall + peasants,
// and centre the camera on the human. Fills `spawns` for later wildlife/sheep.
static void placeStartingPositions(int numAIs, std::vector<Spawn>& spawns) {
    int humans = 0;
    for (int b = 0; b < MAX_PLAYERS; b++) if ((g.humanMask >> b) & 1) humans++;
    const int needed = std::min(MAX_PLAYERS, std::max(1, humans) + numAIs);
    const int MIN_SPAWN_DIST = std::min(MAP_W, MAP_H) * 2 / 3; // ~73 on 110x180
    const int EDGE = 12;

    auto scoreSpawn = [](int cx, int cy) -> int {
        // Reject if any tile in a 5x5 footprint is impassable / hostile.
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) return -1;
            Terrain t = g.map[y][x].terrain;
            if (t==T_WATER||t==T_MOUNTAIN||t==T_LAVA||t==T_SHALLOWS||t==T_GOLD) return -1;
        }
        int score = 100;
        // Bonus: grass-heavy core (room to build).
        int grass = 0;
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) continue;
            Terrain t = g.map[y][x].terrain;
            if (t==T_GRASS||t==T_MEADOW||t==T_DIRT||t==T_TALL_GRASS) grass++;
        }
        score += grass;
        // Bonus: forest within 10 tiles (wood is critical early).
        bool hasWood = false;
        for (int dy = -10; dy <= 10 && !hasWood; dy++)
            for (int dx = -10; dx <= 10 && !hasWood; dx++) {
                int x = cx+dx, y = cy+dy;
                if (!inBounds(x,y)) continue;
                Terrain t = g.map[y][x].terrain;
                if (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE) hasWood = true;
            }
        if (hasWood) score += 40; else score -= 30;
        return score;
    };

    // Generate ~200 candidates, score each, sort high to low.
    struct Cand { int x, y, score; };
    std::vector<Cand> candidates;
    candidates.reserve(220);
    for (int i = 0; i < 220; i++) {
        int cx = EDGE + simRand() % (MAP_W - 2*EDGE);
        int cy = EDGE + simRand() % (MAP_H - 2*EDGE);
        int s = scoreSpawn(cx, cy);
        if (s > 0) candidates.push_back({cx, cy, s});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Cand& a, const Cand& b){ return a.score > b.score; });

    // Greedily pick `needed` spawns, requiring each new pick to be at least
    // MIN_SPAWN_DIST away from already-picked spawns.
    for (auto& c : candidates) {
        if ((int)spawns.size() >= needed) break;
        bool ok = true;
        for (auto& s : spawns) {
            if (dist(c.x, c.y, s.thX, s.thY) < MIN_SPAWN_DIST) { ok = false; break; }
        }
        if (ok) spawns.push_back({c.x, c.y});
    }
    // Fallback: if we couldn't find enough spaced spawns, relax the distance.
    if ((int)spawns.size() < needed) {
        int relaxed = MIN_SPAWN_DIST / 2;
        for (auto& c : candidates) {
            if ((int)spawns.size() >= needed) break;
            bool dup = false;
            for (auto& s : spawns) if (s.thX==c.x && s.thY==c.y) { dup = true; break; }
            if (dup) continue;
            bool ok = true;
            for (auto& s : spawns) if (dist(c.x, c.y, s.thX, s.thY) < relaxed) { ok = false; break; }
            if (ok) spawns.push_back({c.x, c.y});
        }
    }

    // Randomise which spawn the human gets so AIs don't always get the prime spots.
    if (spawns.size() > 1)
        std::swap(spawns[0], spawns[simRand() % spawns.size()]);

    // Clear ground + place starter gold around each spawn, then drop entities.
    bool spawned[MAX_PLAYERS] = {false};
    for (int i = 0; i < (int)spawns.size() && i < needed; i++) {
        int owner = i; // humans take the low slots, then the AIs
        if (owner >= MAX_PLAYERS) break;
        spawned[owner] = true;
        clearStartArea(spawns[i].thX - 2, spawns[i].thY - 2, 6);
        // Gold deposit a few tiles offset (not directly on the TH).
        placeGoldCluster(spawns[i].thX + 9, spawns[i].thY + 4, 5);
        int thId = spawnEntity(E_TOWNHALL, owner, spawns[i].thX, spawns[i].thY);
        // The starting treasury physically sits in the Town Hall vault.
        if (Entity* th = findEntity(thId)) {
            th->storeGold = 300; th->storeWood = 200; th->storeFood[F_GRAIN] = 100;
        }
        for (int j = 0; j < 4; j++)
            spawnEntity(E_PEASANT, owner, spawns[i].thX + 4 + j, spawns[i].thY + 4);
    }
    // Mark any non-spawned slots dead so checkWin doesn't wait on them.
    for (int p = 1; p < MAX_PLAYERS; p++) if (!spawned[p]) g.players[p].alive = false;
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);

    // Open on the local player's base (slot 1 on the joining machine).
    const Spawn& home = spawns[std::min((size_t)std::max(0, g.localPlayer), spawns.size()-1)];
    g.cursorX = home.thX + 2; g.cursorY = home.thY + 2;
    g.viewX = std::max(0, home.thX - 10); g.viewY = std::max(0, home.thY - 5);
}

// Scatter wildlife (deer/wolves/bears/boars), neutral structures, and a starter
// sheep cluster by each player base.
static void spawnWildlifeAndNeutrals(const std::vector<Spawn>& spawns) {
    // Spawn-safety: hostile/neutral wildlife must keep clear of every player
    // base so peasants don't get gored before they can react.
    auto farFromAnyBase = [](int ax, int ay, int radius) {
        for (auto& e : g.entities) {
            if (!e.alive) continue;
            if (e.type != E_TOWNHALL && e.type != E_CASTLE) continue;
            if (std::abs(ax - e.x) <= radius && std::abs(ay - e.y) <= radius) return false;
        }
        return true;
    };

    // Wild deer in herds of 3-6, each herd anchored to a random open spot.
    {
        int total = 0;
        for (int h = 0; h < 10 && total < 42; h++) {
            int hx = -1, hy = -1;
            for (int t = 0; t < 300 && hx < 0; t++) {
                int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && farFromAnyBase(ax, ay, 14))
                    { hx=ax; hy=ay; }
            }
            if (hx < 0) continue;
            int herdSize = 3 + simRand()%4;
            for (int i = 0, t = 0; i < herdSize && t < 100; t++) {
                int ax = hx+(simRand()%9)-4, ay = hy+(simRand()%9)-4;
                ax = std::max(1, std::min(ax, MAP_W-2));
                ay = std::max(1, std::min(ay, MAP_H-2));
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && !entityAt(ax,ay) && farFromAnyBase(ax, ay, 10))
                    { spawnEntity(E_DEER, OWNER_NATURE, ax, ay); i++; total++; }
            }
        }
    }
    // Wolves den in the forests exclusively — must spawn well clear of bases.
    for (int i = 0, t = 0; i < 7 && t < 600; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_PALM||tr==T_DEAD_TREE) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_WOLF, OWNER_NATURE, ax, ay); i++; }
    }
    // Bears: rare, solitary and serious — a handful haunt the deepest woods,
    // kept well away from any starting base so they're a hazard, not a death.
    for (int i = 0, t = 0; i < 3 && t < 800; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_PALM||tr==T_DEAD_TREE) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 20))
            { spawnEntity(E_BEAR, OWNER_NATURE, ax, ay); i++; }
    }
    // Boars: same buffer as wolves — these are the biggest early-game peasant hazard.
    for (int i = 0, t = 0; i < 18 && t < 800; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        Biome  b  = g.map[ay][ax].biome;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS||tr==T_GRASS)
            && (b==B_TEMPERATE||b==B_FOREST) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_BOAR, OWNER_NATURE, ax, ay); i++; }
    }
    // === NEUTRAL STRUCTURES — the land was lived in before this war ===
    // Hermit shrines: heal whoever rests beside them; a garrisoned monk
    // projects the blessing. Claim by garrisoning.
    for (int i = 0, t = 0; i < 3 && t < 400; t++) {
        int ax = 12 + simRand()%(MAP_W-24), ay = 12 + simRand()%(MAP_H-24);
        if (!canPlace(E_SHRINE, ax, ay, OWNER_NATURE) || !farFromAnyBase(ax, ay, 14)) continue;
        spawnEntity(E_SHRINE, OWNER_NATURE, ax, ay); i++;
    }
    // Old watermills on the waterline: claimed, they act as a half-rate mill
    // and food drop-off — a reason to settle the rivers.
    for (int i = 0, t = 0; i < 3 && t < 600; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        if (!canPlace(E_WATERMILL, ax, ay, OWNER_NATURE) || !farFromAnyBase(ax, ay, 12)) continue;
        bool shore = false;
        for (int dy = -1; dy <= 2 && !shore; dy++) for (int dx = -1; dx <= 2 && !shore; dx++) {
            int nx = ax+dx, ny = ay+dy;
            if (inBounds(nx,ny) && isPassableWater(nx,ny)) shore = true;
        }
        if (!shore) continue;
        spawnEntity(E_WATERMILL, OWNER_NATURE, ax, ay); i++;
    }
    // Trading posts where the old roads run: tolls + market trades when held.
    for (int i = 0, t = 0; i < 2 && t < 600; t++) {
        int ax = 15 + simRand()%(MAP_W-30), ay = 15 + simRand()%(MAP_H-30);
        if (g.map[ay][ax].terrain != T_ROAD) continue;
        // Settle just off the roadside.
        int px = ax + 1, py = ay + 1;
        if (!canPlace(E_TRADING_POST, px, py, OWNER_NATURE) || !farFromAnyBase(px, py, 16)) continue;
        spawnEntity(E_TRADING_POST, OWNER_NATURE, px, py); i++;
    }
    // Abandoned villages: clusters of derelict houses, half-built shells a
    // peasant can repair to claim — found expansions for whoever gets there.
    for (int v = 0, t = 0; v < 4 && t < 500; t++) {
        int ax = 14 + simRand()%(MAP_W-28), ay = 14 + simRand()%(MAP_H-28);
        if (!canPlace(E_HOUSE, ax, ay, OWNER_NATURE) || !farFromAnyBase(ax, ay, 18)) continue;
        int homes = 2 + simRand() % 3;
        for (int h = 0, ht = 0; h < homes && ht < 40; ht++) {
            int hx = ax + (simRand()%9) - 4, hy = ay + (simRand()%9) - 4;
            if (!canPlace(E_HOUSE, hx, hy, OWNER_NATURE)) continue;
            int hid = spawnEntity(E_HOUSE, OWNER_NATURE, hx, hy, false);
            if (Entity* he = findEntity(hid)) he->hp = he->maxHp / 2;  // half the work survives
            h++;
        }
        v++;
    }
    // Wolf dens: the forests have teeth until someone burns them out.
    for (int i = 0, t = 0; i < 5 && t < 600; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if (tr != T_FOREST && tr != T_PINE && tr != T_TALL_GRASS) continue;
        if (entityAt(ax, ay) || !farFromAnyBase(ax, ay, 18)) continue;
        g.map[ay][ax].terrain = T_GRASS; g.map[ay][ax].resources = 0;
        spawnEntity(E_WOLF_DEN, OWNER_NATURE, ax, ay); i++;
    }

    // Domestic sheep near each chosen player spawn (one cluster per spawn).
    for (auto& sp : spawns) {
        int bx = sp.thX + 4, by = sp.thY + 4;
        for (int i = 0, t = 0; i < 4 && t < 200; t++) {
            int ax = bx+(simRand()%7)-3, ay = by+(simRand()%7)-3;
            ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
            if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
        }
    }
}

void initGame(int numAIs, unsigned long long seed) {
    // Seed the deterministic sim RNG. Replays pass the recorded seed; a
    // future multiplayer lobby shares the host's seed with every client.
    if (seed == 0) seed = (unsigned long long)time(nullptr) * 2654435761ull + 1;
    resetMatchState(seed);
    // Civs + AI temperaments. Every seat consumes exactly two simRand rolls
    // regardless of what was chosen, so the RNG stream — and therefore the
    // whole match — is identical across machines and replays.
    for (int pl = 0; pl < MAX_PLAYERS; pl++) {
        int roll  = simRand() % NUM_CIVS;
        int mood  = 1 + simRand() % 3;              // Raider / Builder / Warlord
        g.players[pl].civ = (g.civChoice[pl] >= 0 && g.civChoice[pl] < NUM_CIVS)
                          ? g.civChoice[pl] : roll;
        g.players[pl].aiPersona = mood;             // ignored for human seats
    }
    generateMap();
    std::vector<Spawn> spawns;
    placeStartingPositions(numAIs, spawns);
    spawnWildlifeAndNeutrals(spawns);
    updateFog();
}

// One deterministic sim step. Everything that advances game state lives
// here and ONLY here — the interactive loop, replay playback, and --verify
// all call this same function, so a replay can never tick differently
// from the game that recorded it.
void simTick() {
    // Keep capacity headroom so mid-tick spawnEntity never reallocates
    // under a live Entity& held by tickEntity. Growing here, between
    // ticks, is the only safe point.
    if (g.entities.size() + 256 > g.entities.capacity())
        g.entities.reserve(g.entities.capacity() * 2);
    g.tick++;
    g.dayPhase += 1.0f / DAY_LENGTH;
    if (g.dayPhase >= 1.0f) g.dayPhase -= 1.0f;
    g.seasonPhase += 1.0f / SEASON_LENGTH;
    if (g.seasonPhase >= 4.0f) g.seasonPhase -= 4.0f;
    replayInjectCommands();    // playback: queue this tick's recorded commands
    applyPendingCommands();    // drain the queue (records to replay when live)
    for (int i = 0; i < (int)g.entities.size(); i++) tickEntity(g.entities[i]);
    tickSeasons(); tickThaw(); tickWinter();
    tickWeather(); tickPaving();
    tickTowers(); tickGates(); tickProjectiles(); tickFarms(); tickMarkets();
    tickSpoilage(); tickTaverns(); tickPrisoners();
    tickChurches(); tickAnimals(); tickAI(); updateFog();
    // Prune dead IDs from selection + control groups so UI counts
    // ("Group: N units") stay honest as casualties pile up.
    auto pruneDead = [](std::vector<int>& v) {
        v.erase(std::remove_if(v.begin(), v.end(),
            [](int id){ return findEntity(id) == nullptr; }), v.end());
    };
    pruneDead(g.selectedIds);
    for (int i = 0; i < 9; i++) pruneDead(g.controlGroups[i]);
    if (g.selectedId >= 0 && !findEntity(g.selectedId)) g.selectedId = -1;
    if (g.tick % 100 == 0) {
        g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
            [](const Entity& e){ return !e.alive && e.state==S_DEAD; }), g.entities.end());
        // Corpse markers fade after ~200 ticks (render-only, not sim state).
        g.corpses.erase(std::remove_if(g.corpses.begin(), g.corpses.end(),
            [](const Game::Corpse& c){ return g.tick - c.tick > 200; }), g.corpses.end());
        // Defensive: rebuild supply totals so any kill path that
        // missed updateSupply gets reconciled within ~8 seconds.
        for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);
        checkWin();
    }
    simHashTick();   // REALM_HASH=1: desync-detector log
}

// Interactive match loop (normal play and replay playback).
static void runMatch() {
    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;

    auto nextTick = Clock::now() + Ms(tickPeriodMs());
    int lastCx = g.cursorX, lastCy = g.cursorY;
    bool lastDrag = g.dragging;

    while (!g.returnToMenu) {
        // Block only as long as needed to reach the next game tick
        int wait = (int)std::chrono::duration_cast<Ms>(nextTick - Clock::now()).count();
        timeout(std::max(0, wait));
        int ch = getch();
        handleInput(ch);

        // Drain any events that piled up (mouse moves, key repeats) without
        // running game logic for each one — keeps the cursor smooth
        timeout(0);
        int extra;
        while ((extra = getch()) != ERR) handleInput(extra);

        // Tick and render at fixed rate regardless of input volume
        bool ticked = false;
        if (Clock::now() >= nextTick) {
            bool simAllowed = g.mode != M_PAUSED && g.mode != M_GAME_OVER
                           && g.mode != M_HELP && g.mode != M_SAVELOAD;
            if (!netActive()) {
                nextTick += Ms(tickPeriodMs());
                if (simAllowed) simTick();
            } else {
                // Lockstep: a tick may only run once BOTH sides' command
                // bundles for it have arrived. Otherwise stall (input and
                // rendering stay live) and retry shortly.
                netPump();
                if (simAllowed && !netConnectionLost() && !netDesynced() && netTickReady()) {
                    simTick();
                    netAfterTick();
                    nextTick += Ms(tickPeriodMs());
                    if (nextTick < Clock::now()) nextTick = Clock::now();  // don't spiral after a stall
                } else {
                    nextTick = Clock::now() + Ms(15);
                }
            }
            render();
            ticked = true;
        }
        // Snappy cursor: redraw between ticks when the mouse moved or a drag updated.
        bool cursorMoved = (g.cursorX != lastCx || g.cursorY != lastCy || g.dragging != lastDrag);
        if (!ticked && cursorMoved) render();
        lastCx = g.cursorX; lastCy = g.cursorY; lastDrag = g.dragging;
    }
}

// Headless determinism check: run N ticks from a fixed seed with no human
// commands and print the final state hash. Run it twice; identical hashes
// mean the sim is reproducible — the property lockstep multiplayer needs.
static int runVerify(unsigned long long seed, int ticks, int numAIs, int biome, int layout) {
    g.biomeChoice  = biome;   // climate; -1 = mixed
    g.layoutChoice = layout;  // -1 = random (resolved in initGame)
    g.difficulty   = 1;
    g.humanMask    = 1; g.localPlayer = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = -1;
    initGame(numAIs, seed);
    for (int i = 0; i < ticks; i++) simTick();
    printf("seed=%llu ticks=%d ais=%d hash=%016llx\n",
           seed, ticks, numAIs, simStateHash());
    // Per-seat balance probe: is anyone stuck in an era, starved, or wiped out?
    for (int pl = 0; pl < MAX_PLAYERS; pl++) {
        const Player& P = g.players[pl];
        int peas = 0, mili = 0, blds = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != pl) continue;
            if (e.type == E_PEASANT) peas++;
            else if (isUnit(e.type) && !isNaval(e.type)) mili++;
            else if (isBuilding(e.type)) blds++;
        }
        if (peas + mili + blds == 0 && !P.alive && pl > numAIs) continue;   // never seated
        printf("  P%d %-12s era=%-10s persona=%d %s peas=%d mil=%d blds=%d g/w/f=%d/%d/%d research=%03x raids=%d\n",
               pl + 1, CIVS[P.civ].name, eraName(P.era), P.aiPersona,
               P.alive ? "alive" : "DEAD ", peas, mili, blds, P.gold, P.wood, P.food, P.research,
               g.statRaids[pl]);
    }
    return 0;
}

int main(int argc, char** argv) {
    forceUtf8Locale();

    // --verify runs fully headless: no curses, no renderer, just the sim.
    if (argc >= 2 && strcmp(argv[1], "--verify") == 0) {
        unsigned long long seed = (argc >= 3) ? strtoull(argv[2], nullptr, 10) : 12345;
        int ticks  = (argc >= 4) ? atoi(argv[3]) : 5000;
        int numAIs = (argc >= 5) ? atoi(argv[4]) : 3;
        int biome  = (argc >= 6) ? atoi(argv[5]) : 0;   // climate: Biome 0-4 / -1 mixed
        int layout = (argc >= 7) ? atoi(argv[6]) : 0;   // layout: Layout 0-4 / -1 random
        return runVerify(seed, std::max(1, ticks), std::max(1, std::min(3, numAIs)), biome, layout);
    }

    // --test-raid: headless check of the whole plunder pipeline. Stages a
    // stocked human stockyard in the AI's scouted range plus idle hussars,
    // then requires the AI to rob it and bank the goods within 4000 ticks.
    if (argc >= 2 && strcmp(argv[1], "--test-raid") == 0) {
        g.biomeChoice = 0; g.layoutChoice = 0; g.difficulty = 2;   // Hard: short grace
        g.humanMask = 1; g.localPlayer = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = -1;
        initGame(1, 777);
        Entity* aiTH = nullptr;
        for (auto& e : g.entities)
            if (e.alive && e.owner == 1 && e.type == E_TOWNHALL) { aiTH = &e; break; }
        if (!aiTH) { fprintf(stderr, "no AI town hall\n"); return 1; }
        int yx = -1, yy = -1;
        for (int a = 0; a < 4000 && yx < 0; a++) {
            int x = aiTH->x + (simRand()%81) - 40, y = aiTH->y + (simRand()%81) - 40;
            int d = dist(aiTH->x, aiTH->y, x, y);
            if (d < 26 || d > 40) continue;               // outside base defence, inside a ride
            if (canPlace(E_STOCKYARD, x, y, 0)) { yx = x; yy = y; }
        }
        if (yx < 0) { fprintf(stderr, "no yard site\n"); return 1; }
        int yid = spawnEntity(E_STOCKYARD, 0, yx, yy, true);
        Entity* yard = findEntity(yid);
        yard->storeGold = 300; g.players[0].gold += 300;
        for (int dy = -1; dy <= 3; dy++) for (int dx = -1; dx <= 3; dx++)
            if (inBounds(yx+dx, yy+dy)) g.map[yy+dy][yx+dx].explored[1] = true;
        for (int i = 0; i < 3; i++)
            spawnEntity(E_HUSSAR, 1, aiTH->x + 5 + i, aiTH->y + 5, true);
        int aiGold0 = g.players[1].gold;
        for (int i = 0; i < 4000; i++) simTick();
        yard = findEntity(yid);
        printf("raids=%d yardGold=%d aiGoldGain=%d humanGold=%d\n",
               g.statRaids[1], yard ? yard->storeGold : -1,
               g.players[1].gold - aiGold0, g.players[0].gold);
        return (g.statRaids[1] > 0) ? 0 : 1;
    }

    // --net-host / --net-join: headless lockstep smoke test. Start a host in
    // one process and a joiner in another (same machine or LAN); both run the
    // full handshake + scheduler + scripted cross-wire commands and print the
    // final state hash. Identical hashes = the wire preserves determinism.
    bool netHost = argc >= 2 && strcmp(argv[1], "--net-host") == 0;
    bool netJoinCli = argc >= 3 && strcmp(argv[1], "--net-join") == 0;
    if (netHost || netJoinCli) {
        int ticks = netHost ? ((argc >= 3) ? atoi(argv[2]) : 2000)
                            : ((argc >= 4) ? atoi(argv[3]) : 2000);
        ticks = std::max(NET_CMD_DELAY + 1, ticks);
        int ais = (netHost && argc >= 4) ? std::max(0, std::min(2, atoi(argv[3]))) : 1;
        NetMatchConfig nc;
        int slot;
        if (netHost) {
            nc.seed = 424242; nc.numAIs = ais; nc.biome = 0; nc.layout = 0;
            nc.difficulty = 1; nc.speed = 1; nc.humanMask = 3;
            if (!netHostOpen()) { fprintf(stderr, "netHostOpen failed\n"); return 1; }
            netHostSetInfo(nc);
            fprintf(stderr, "hosting on :%d, waiting for joiner...\n", NET_TCP_PORT);
            while (!netHostClientPresent()) {
                if (!netHostPoll()) { fprintf(stderr, "lobby error\n"); return 1; }
                msleep(10);
            }
            if (!netHostStart()) { fprintf(stderr, "start failed\n"); return 1; }
            slot = 0;
        } else {
            std::string err;
            if (!netJoinConnect(argv[2], NET_TCP_PORT, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
            int rc2;
            while ((rc2 = netClientPoll(nc)) != 2) {
                if (rc2 < 0) { fprintf(stderr, "lost host in lobby\n"); return 1; }
                msleep(5);
            }
            slot = 1;
        }
        g.difficulty = nc.difficulty; g.biomeChoice = nc.biome; g.layoutChoice = nc.layout;
        g.humanMask = nc.humanMask; g.localPlayer = slot;
        for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = nc.civ[i];
        initGame(nc.numAIs, nc.seed);
        int stall = 0;
        while (g.tick < ticks) {
            if (netConnectionLost()) { fprintf(stderr, "connection lost at tick %d\n", g.tick); return 1; }
            if (netDesynced()) { fprintf(stderr, "DESYNC at tick %d\n", netDesyncTick()); return 1; }
            // Scripted human traffic: each side periodically orders one of its
            // own units around. Different phases so both directions carry load.
            if (g.tick % 37 == (slot == 0 ? 0 : 5)) {
                for (auto& e : g.entities) {
                    if (!e.alive || e.owner != slot || !isUnit(e.type)) continue;
                    Command mc;
                    mc.type = CMD_MOVE; mc.player = slot;
                    mc.x = std::max(1, std::min(MAP_W-2, e.x + (int)(g.tick % 13) - 6));
                    mc.y = std::max(1, std::min(MAP_H-2, e.y + (int)(g.tick % 11) - 5));
                    mc.units = {e.id};
                    pushCommand(mc);
                    break;
                }
            }
            if (netTickReady()) { simTick(); netAfterTick(); stall = 0; }
            else { msleep(2); if (++stall > 5000) { fprintf(stderr, "stalled at tick %d\n", g.tick); return 1; } }
        }
        printf("net-%s ticks=%d hash=%016llx\n", netHost ? "host" : "join", g.tick, simStateHash());
        // Linger so the slower side can finish and the final hashes cross.
        for (int i = 0; i < 200 && !netConnectionLost() && !netDesynced(); i++) { netPump(); msleep(5); }
        if (netDesynced()) { fprintf(stderr, "DESYNC at tick %d\n", netDesyncTick()); return 1; }
        netSendBye();
        netClose();
        return 0;
    }

    // --verify-replay: headless playback of a recorded match. Run it twice
    // and compare hashes — a recorded game that replays identically is the
    // end-to-end proof the funnel + sim are deterministic.
    if (argc >= 3 && strcmp(argv[1], "--verify-replay") == 0) {
        unsigned long long seed; int ais, biome, layout, diffc, humask;
        if (!replayLoadFile(argv[2], seed, ais, biome, layout, diffc, humask)) {
            fprintf(stderr, "Can't read replay '%s'.\n", argv[2]);
            return 1;
        }
        g.biomeChoice  = biome;
        g.layoutChoice = layout;
        g.difficulty   = diffc;
        g.humanMask    = humask; g.localPlayer = 0;
        initGame(ais, seed);
        int ticks = (argc >= 4) ? std::max(1, atoi(argv[3])) : 5000;
        for (int i = 0; i < ticks; i++) simTick();
        printf("replay=%s ticks=%d hash=%016llx\n", argv[2], ticks, simStateHash());
        return 0;
    }

    // --replay: load header before touching the screen so a bad file can
    // fail to stderr instead of into a half-initialised terminal.
    bool replay = false;
    unsigned long long repSeed = 0; int repAIs = 1, repBiome = -1, repLayout = -1, repDiff = 1, repMask = 1;
    if (argc >= 3 && strcmp(argv[1], "--replay") == 0) {
        if (!replayLoadFile(argv[2], repSeed, repAIs, repBiome, repLayout, repDiff, repMask)) {
            fprintf(stderr, "Can't read replay '%s' (missing, wrong version, or corrupt).\n", argv[2]);
            return 1;
        }
        replay = true;
    }

#if defined(USE_SDL_SHIM) && !defined(_WIN32)
    // The standalone .app is launched from Finder with cwd "/", where the
    // game's relative save/replay paths can't be written. Run from a writable
    // per-user folder (~/Library/Application Support/Realm) and ensure the
    // replays/ subdir exists. (Terminal launches pass a real cwd; this only
    // moves us when we'd otherwise be stranded at the filesystem root.)
    if (!replay) {
        char cwd[1024];
        bool stranded = (getcwd(cwd, sizeof cwd) && strcmp(cwd, "/") == 0);
        const char* home = getenv("HOME");
        if (stranded && home && *home) {
            std::string lib = std::string(home) + "/Library";
            std::string sup = lib + "/Application Support";
            std::string dir = sup + "/Realm";
            mkdir(lib.c_str(), 0755); mkdir(sup.c_str(), 0755); mkdir(dir.c_str(), 0755);
            if (chdir(dir.c_str()) == 0) mkdir("replays", 0755);
        }
    }
#endif

    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    loadMenuConfig();   // remembered splash preferences (civ, colour, setup)
    initColors();       // after load: team colours honour the remembered pick

    if (replay) {
        g.biomeChoice  = repBiome;
        g.layoutChoice = repLayout;
        g.difficulty   = repDiff;
        g.humanMask    = repMask; g.localPlayer = 0;
        initGame(repAIs, repSeed);
        setStatus("REPLAY — commands come from the recording. Camera/selection are yours; [Q][Q] to quit.");
        runMatch();
        endwin();
        return 0;
    }

    while (true) {
        // The splash is the hub: skirmish setup, multiplayer lobbies, saves.
        SplashResult pick;
        showSplash(pick);

        // Reinitialise colour pairs in case the splash changed anything.
        initColors();

        if (!pick.replayPath.empty()) {
            // Watch a recording chosen in the browser: identical to the
            // --replay CLI path, then back to the splash (playback mode must
            // be cleared or the next live match would drop every order).
            unsigned long long rs; int rAIs, rBiome, rLayout, rDiff, rMask;
            if (replayLoadFile(pick.replayPath.c_str(), rs, rAIs, rBiome, rLayout, rDiff, rMask)) {
                g.biomeChoice = rBiome; g.layoutChoice = rLayout; g.difficulty = rDiff;
                g.humanMask = rMask; g.localPlayer = 0;
                initGame(rAIs, rs);
                setStatus("REPLAY — commands come from the recording. Camera/selection are yours; [Q][Q] to stop.");
                runMatch();
                replayStopPlayback();
            } else {
                // Old-version or corrupt recording; say so instead of nothing.
                setStatus("That replay is from another Realm version — can't play it.");
            }
            continue;
        }

        if (pick.netPlay) {
            // Network match: both machines build the identical world from the
            // lobby-agreed config, then exchange nothing but commands.
            const NetMatchConfig& nc = pick.netCfg;
            g.difficulty   = nc.difficulty;
            gameSpeed      = (GameSpeed)nc.speed;
            g.biomeChoice  = nc.biome;
            g.layoutChoice = nc.layout;
            g.humanMask    = nc.humanMask;
            g.localPlayer  = pick.netSlot;
            for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = nc.civ[i];
            applyTeamColors();
            initGame(nc.numAIs, nc.seed);
            replayStartRecording(nc.numAIs);   // records BOTH players' streams
            setStatus(g.mapName + " — " + netPeerName() +
                      (pick.netSlot == 0 ? " marches against you. " : " awaits your challenge. ") +
                      "The battle is joined!");
            runMatch();
            replayStopRecording();
            netSendBye();
            netClose();
            continue;
        }

        g.humanMask = 1; g.localPlayer = 0;   // solo seat
        int numAIs = pick.numAIs;
        int loadSlot = pick.loadSlot;
        unsigned long long pickedSeed = pick.seed;

        if (loadSlot > 0) {
            // Resume a saved game chosen straight from the splash. Loaded state
            // can't be reproduced from a seed, so it isn't recorded as a replay.
            char path[64]; saveSlotPath(loadSlot, path, sizeof path);
            if (loadGame(path)) {
                setStatus(g.mapName + " — saved game resumed (slot " + std::to_string(loadSlot) + ").");
            } else {
                initGame(numAIs, 0);
                replayStartRecording(numAIs);
                setStatus("Load failed (wrong version or corrupt) — started a fresh match instead.");
            }
        } else {
            initGame(numAIs, pickedSeed);
            replayStartRecording(numAIs);   // every match is recorded; replays/ dir
            setStatus(g.mapName + " — dawn breaks. Select peasants [Space] and gather [Enter]. [A]=select all military.");
        }

        runMatch();
        replayStopRecording();
        // returnToMenu set — loop back to splash.
    }
    endwin();
    return 0;
}
