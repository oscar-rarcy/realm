#include "realm.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

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
        if      (c=='q'||c=='Q'||c==27)                 return 0;
        else if (c==KEY_UP   ||c=='k'||c=='K')          selSlot = (selSlot + NUM_SAVE_SLOTS - 1) % NUM_SAVE_SLOTS;
        else if (c==KEY_DOWN ||c=='j'||c=='J')          selSlot = (selSlot + 1) % NUM_SAVE_SLOTS;
        else if (c=='d'||c=='D') {
            SaveSlotInfo info; char path[64]; saveSlotPath(selSlot+1, path, sizeof path);
            if (peekSave(path, info)) remove(path);
        }
        else if (c=='\n'||c=='\r'||c==KEY_ENTER) {
            SaveSlotInfo info;
            if (slotUsed(selSlot, info)) return selSlot + 1;   // only load a used slot
        }
    }
}

static int showSplash(unsigned long long& outSeed, int& outLoadSlot) {
    // Two independent axes: climate (0-4 + Random) and layout (0-4 + Random).
    static const char* climateNames[] = { "Temperate","Desert","Snow","Swamp","Forest","Random" };
    static const char* layoutNames[]  = { "Continental","Highlands","Deep Woods","River","Islands","Random" };
    const int CLIM_RANDOM = 5, LAYOUT_RANDOM = 5;
    static const char* diffNames[]  = { "Easy", "Normal", "Hard" };
    static const char* speedNames[] = { "Slow", "Normal", "Fast" };
    int numAIs = 1;
    int climIdx   = CLIM_RANDOM;    // mixed climate bands
    int layoutIdx = LAYOUT_RANDOM;  // random topology
    int diffIdx = 1;  // Normal
    int speedIdx = GS_NORMAL;  // wall-clock pace; doesn't affect the sim
    unsigned long long pickedSeed = 0; // non-zero once a specific map is chosen in the picker

    static const char* banner[] = {
        u8"██████╗ ███████╗ █████╗ ██╗     ███╗   ███╗",
        u8"██╔══██╗██╔════╝██╔══██╗██║     ████╗ ████║",
        u8"██████╔╝█████╗  ███████║██║     ██╔████╔██║",
        u8"██╔══██╗██╔══╝  ██╔══██║██║     ██║╚██╔╝██║",
        u8"██║  ██║███████╗██║  ██║███████╗██║ ╚═╝ ██║",
        u8"╚═╝  ╚═╝╚══════╝╚═╝  ╚═╝╚══════╝╚═╝     ╚═╝",
    };

    int maxY, maxX;
    while (true) {
        getmaxyx(stdscr, maxY, maxX);
        erase();

        const int W = 78;
        int col = std::max(1, maxX/2 - W/2);
        int row = std::max(0, maxY/2 - 16);

        auto pr = [&](int r, int c, const char* fmt, ...) {
            va_list ap; va_start(ap, fmt);
            char buf[256]; vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
            mvprintw(r, c, "%s", buf);
        };

        // ---- banner ----
        attron(COLOR_PAIR(CP_GOLD) | A_BOLD);
        for (int i = 0; i < 6; i++) mvaddstr(row + i, col + (W-43)/2, banner[i]);
        attroff(COLOR_PAIR(CP_GOLD) | A_BOLD);
        attron(A_TITLE | COLOR_PAIR(CP_UI_ACCENT));
        pr(row + 6, col + (W-22)/2, "~  Medieval Warlord  ~");
        attroff(A_TITLE | COLOR_PAIR(CP_UI_ACCENT));
        row += 8;

        // ---- two columns: war setup | commands ----
        auto box = [&](int r, int c, int w, int h, const char* titleTxt) {
            mvaddstr(r, c, u8"┌─ ");
            attron(A_TITLE); mvaddstr(r, c+3, titleTxt); attroff(A_TITLE);
            int tl = (int)strlen(titleTxt);
            mvaddstr(r, c+3+tl, " ");
            for (int x = c+4+tl; x < c+w-1; x++) mvaddstr(r, x, u8"─");
            mvaddstr(r, c+w-1, u8"┐");
            for (int y = r+1; y < r+h-1; y++) {
                mvaddstr(y, c,     u8"│");
                mvaddstr(y, c+w-1, u8"│");
            }
            mvaddstr(r+h-1, c, u8"└");
            for (int x = c+1; x < c+w-1; x++) mvaddstr(r+h-1, x, u8"─");
            mvaddstr(r+h-1, c+w-1, u8"┘");
        };

        int bw = 38, bh = 10;
        box(row, col,        bw, bh, "THE WAR");
        box(row, col + bw+2, bw, bh, "COMMANDS");

        auto sel = [&](int r, int c, const char* label, const char* value, const char* keys) {
            pr(r, c, "%-11s", label);
            attron(A_BOLD | COLOR_PAIR(CP_UI_HIGH)); pr(r, c+11, "%-10s", value); attroff(A_BOLD | COLOR_PAIR(CP_UI_HIGH));
            attron(COLOR_PAIR(CP_UI_DIM)); pr(r, c+22, "%s", keys); attroff(COLOR_PAIR(CP_UI_DIM));
        };
        char opp[8]; snprintf(opp, sizeof opp, "%d", numAIs);
        sel(row+2, col+2, "Opponents",  opp,                    "1/2/3");
        sel(row+3, col+2, "Difficulty", diffNames[diffIdx],     "E/N/H");
        sel(row+4, col+2, "Climate",    climateNames[climIdx],  "T/D/S/W/F/0");
        sel(row+5, col+2, "Layout",     layoutNames[layoutIdx], "L");
        sel(row+6, col+2, "Speed",      speedNames[speedIdx],   "G");
        sel(row+7, col+2, "Colour",     teamColorName(g.playerColor), "C");
        attron(COLOR_PAIR(CP_MM_PLAYER)|A_BOLD); pr(row+7, col+2+24, "##"); attroff(COLOR_PAIR(CP_MM_PLAYER)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_DIM));
        if (pickedSeed)
            pr(row+8, col+2, "Battlefield chosen — seed locked.   [V] re-pick");
        else
            pr(row+8, col+2, "[L] cycles layout   [V] browse battlefields");
        attroff(COLOR_PAIR(CP_UI_DIM));

        int c2 = col + bw + 4;
        pr(row+2, c2, "Space/Click sel   Enter/RClick act");
        pr(row+3, c2, "B build  T train  A all military");
        pr(row+4, c2, "Z patrol  X hold  1-9/G groups");
        pr(row+5, c2, "U eject  R rally  P pause");
        pr(row+6, c2, "P in-game: save / load");
        pr(row+7, c2, "? in-game help   QQ to menu");
        row += bh + 1;

        // ---- tips ----
        attron(COLOR_PAIR(CP_UI_DIM));
        pr(row++, col+1, "Sow farms on wild wheat. Harvest doubles in autumn; stockpile before the");
        pr(row++, col+1, "freeze. Mud slows siege in spring. Garrison ruined keeps to claim them.");
        attroff(COLOR_PAIR(CP_UI_DIM));
        row++;

        // ---- footer ----
        attron(A_BOLD | COLOR_PAIR(CP_UI_HIGH));
        pr(row, col + (W-64)/2, "[Enter] Begin   [V] Battlefields   [O] Load game   [Q] Quit");
        attroff(A_BOLD | COLOR_PAIR(CP_UI_HIGH));

        refresh();
        int ch = getch();
        if (ch=='q'||ch=='Q') { endwin(); exit(0); }
        if (ch=='\n'||ch==KEY_ENTER||ch=='\r') break;
        // [O] opens the saved-game browser; a chosen slot ends the splash and
        // signals main() to load it instead of generating a fresh match.
        if (ch=='o'||ch=='O') { int sl = showLoadMenu(); if (sl > 0) { outLoadSlot = sl; break; } }
        if (ch=='1') numAIs=1;
        else if (ch=='2') numAIs=2;
        else if (ch=='3') numAIs=3;
        // Changing climate or layout discards any specific previewed seed.
        else if (ch=='0') { climIdx=CLIM_RANDOM; pickedSeed=0; }
        else if (ch=='t'||ch=='T') { climIdx=0; pickedSeed=0; }
        else if (ch=='d'||ch=='D') { climIdx=1; pickedSeed=0; }
        else if (ch=='s'||ch=='S') { climIdx=2; pickedSeed=0; }
        else if (ch=='w'||ch=='W') { climIdx=3; pickedSeed=0; }
        else if (ch=='f'||ch=='F') { climIdx=4; pickedSeed=0; }
        else if (ch=='l'||ch=='L') { layoutIdx=(layoutIdx+1)%(LAYOUT_RANDOM+1); pickedSeed=0; }
        // V opens the visual battlefield picker; committing a thumbnail there
        // locks its exact climate + layout + seed for [Enter].
        else if (ch=='v'||ch=='V') {
            g.biomeChoice  = (climIdx   == CLIM_RANDOM)   ? -1 : climIdx;
            g.layoutChoice = (layoutIdx == LAYOUT_RANDOM) ? -1 : layoutIdx;
            unsigned long long s = 0;
            if (showMapPreview(s)) {
                pickedSeed = s;
                climIdx   = (g.biomeChoice  < 0) ? CLIM_RANDOM   : g.biomeChoice;
                layoutIdx = (g.layoutChoice < 0) ? LAYOUT_RANDOM : g.layoutChoice;
            }
        }
        else if (ch=='e'||ch=='E') diffIdx=0;
        else if (ch=='n'||ch=='N') diffIdx=1;
        else if (ch=='h'||ch=='H') diffIdx=2;
        else if (ch=='g'||ch=='G') speedIdx = (speedIdx + 1) % 3;
        else if (ch=='c'||ch=='C') { g.playerColor = (g.playerColor + 1) % numTeamColors(); applyTeamColors(); }
    }
    g.difficulty = diffIdx;
    gameSpeed = (GameSpeed)speedIdx;
    g.biomeChoice  = (climIdx   == CLIM_RANDOM)   ? -1 : climIdx;     // climate (or mixed)
    g.layoutChoice = (layoutIdx == LAYOUT_RANDOM) ? -1 : layoutIdx;   // layout  (or random)
    outSeed = pickedSeed;   // 0 = let initGame roll a fresh seed
    return numAIs;
}

// One terrain -> minimap glyph/colour, shared by every preview thumbnail. Full
// visibility (no fog), so it reads as a map illustration, not the in-game radar.
static void previewGlyph(Terrain t, char& ch, int& cp) {
    switch (t) {
        case T_WATER: case T_SHALLOWS: case T_REEDS: case T_MARSH:
                                       ch='~'; cp=CP_MM_WATER;  break;
        case T_FISH:                   ch='*'; cp=CP_MM_WATER;  break;
        case T_MOUNTAIN: case T_STONE: ch='^'; cp=CP_MM_MTN;    break;
        case T_HILLS: case T_GRAVEL:   ch='n'; cp=CP_MM_MTN;    break;
        case T_FOREST: case T_PINE: case T_PALM: case T_DEAD_TREE:
                                       ch='*'; cp=CP_MM_FOREST; break;
        case T_GOLD:                   ch='$'; cp=CP_MM_GOLD;   break;
        case T_SAND: case T_DUNES:     ch='.'; cp=CP_MM_SAND;   break;
        case T_DIRT: case T_MUD:       ch=','; cp=CP_MM_SAND;   break;  // cracked flats / wadi beds
        case T_SNOW: case T_ICE:       ch='.'; cp=CP_MM_SNOW;   break;
        case T_CASTLE_WALL: case T_CASTLE_GATE: case T_CASTLE_FLOOR: case T_RUINS:
                                       ch='#'; cp=CP_MM_CASTLE; break;
        case T_BRIDGE:                 ch='='; cp=CP_MM_CASTLE; break;
        case T_MONOLITH:               ch='I'; cp=CP_MM_CASTLE; break;
        case T_WHEAT: case T_BERRY:    ch=':'; cp=CP_GRASS_DRY; break;
        default:                       ch='.'; cp=CP_GRASS;     break;  // grass & friends
    }
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

    static const char* layName[]  = {"Continental","Highlands","Deep Woods","River","Islands"};
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
            char c; int cp; previewGlyph(g.map[yy*MAP_H/TH][xx*MAP_W/TW].terrain, c, cp);
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
            // Headline the map's evocative name on the bottom border (AoE2 style).
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
        g.players[p] = {300, 200, 100, 0, 0, true, 0, 0};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true, 0, 0};
}

// Score and space out player spawns, place each side's Town Hall + peasants,
// and centre the camera on the human. Fills `spawns` for later wildlife/sheep.
static void placeStartingPositions(int numAIs, std::vector<Spawn>& spawns) {
    const int needed = 1 + numAIs;
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
    for (int i = 0; i < (int)spawns.size() && i <= numAIs; i++) {
        int owner = (i == 0) ? 0 : i; // i==0 is human, then AI 1, 2, 3
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

    g.cursorX = spawns[0].thX + 2; g.cursorY = spawns[0].thY + 2;
    g.viewX = std::max(0, spawns[0].thX - 10); g.viewY = std::max(0, spawns[0].thY - 5);
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
            nextTick += Ms(tickPeriodMs());
            if (g.mode != M_PAUSED && g.mode != M_GAME_OVER && g.mode != M_HELP && g.mode != M_SAVELOAD) simTick();
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
    initGame(numAIs, seed);
    for (int i = 0; i < ticks; i++) simTick();
    printf("seed=%llu ticks=%d ais=%d hash=%016llx\n",
           seed, ticks, numAIs, simStateHash());
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

    // --verify-replay: headless playback of a recorded match. Run it twice
    // and compare hashes — a recorded game that replays identically is the
    // end-to-end proof the funnel + sim are deterministic.
    if (argc >= 3 && strcmp(argv[1], "--verify-replay") == 0) {
        unsigned long long seed; int ais, biome, layout, diffc;
        if (!replayLoadFile(argv[2], seed, ais, biome, layout, diffc)) {
            fprintf(stderr, "Can't read replay '%s'.\n", argv[2]);
            return 1;
        }
        g.biomeChoice  = biome;
        g.layoutChoice = layout;
        g.difficulty   = diffc;
        initGame(ais, seed);
        int ticks = (argc >= 4) ? std::max(1, atoi(argv[3])) : 5000;
        for (int i = 0; i < ticks; i++) simTick();
        printf("replay=%s ticks=%d hash=%016llx\n", argv[2], ticks, simStateHash());
        return 0;
    }

    // --replay: load header before touching the screen so a bad file can
    // fail to stderr instead of into a half-initialised terminal.
    bool replay = false;
    unsigned long long repSeed = 0; int repAIs = 1, repBiome = -1, repLayout = -1, repDiff = 1;
    if (argc >= 3 && strcmp(argv[1], "--replay") == 0) {
        if (!replayLoadFile(argv[2], repSeed, repAIs, repBiome, repLayout, repDiff)) {
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
    initColors();

    if (replay) {
        g.biomeChoice  = repBiome;
        g.layoutChoice = repLayout;
        g.difficulty   = repDiff;
        initGame(repAIs, repSeed);
        setStatus("REPLAY — commands come from the recording. Camera/selection are yours; [Q][Q] to quit.");
        runMatch();
        endwin();
        return 0;
    }

    while (true) {
        // The splash is the hub: opponents, difficulty, map, display. Pressing
        // [V] there opens the visual battlefield picker; a committed pick comes
        // back as a locked seed (0 = roll a fresh map of the chosen type).
        unsigned long long pickedSeed = 0;
        int loadSlot = 0;
        int numAIs = showSplash(pickedSeed, loadSlot);

        // Reinitialise colour pairs in case the splash changed anything.
        initColors();

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
