#include "realm.h"
#include <chrono>

// Pre-match menu: pick the number of AI opponents (1, 2, or 3). Q exits.
static int pickAICount() {
    int maxY, maxX;
    while (true) {
        getmaxyx(stdscr, maxY, maxX);
        erase();
        int row = maxY/2 - 6;
        auto center = [&](const char* s) {
            int len = (int)strlen(s);
            mvprintw(row++, std::max(0, maxX/2 - len/2), "%s", s);
        };
        attron(A_BOLD); center("REALM"); attroff(A_BOLD);
        center("=====");
        row++;
        center("Choose number of opponents:");
        row++;
        center("[1]  1 AI   - duel");
        center("[2]  2 AIs  - 3-corner FFA");
        center("[3]  3 AIs  - full 4-corner FFA");
        row++;
        center("Press 1, 2, or 3 to begin.   Q to quit.");
        refresh();
        int ch = getch();
        if (ch == 'q' || ch == 'Q') { endwin(); exit(0); }
        if (ch == '1') return 1;
        if (ch == '2') return 2;
        if (ch == '3') return 3;
    }
}

void initGame(int numAIs) {
    srand((unsigned)time(nullptr));
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
    g.buildPending = E_NONE; g.wallDragX = 0; g.wallDragY = 0;
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f; g.prevSeason = -1;
    for (int p = 0; p < MAX_PLAYERS; p++)
        g.players[p] = {300, 200, 100, 0, 0, true, 0, 0};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true, 0, 0};

    generateMap();

    // Four corner spawn points: thX,thY, peasant row anchor pX,pY, pDir (+1 or -1 along X)
    struct Spawn { int thX,thY, pX,pY, pDir; };
    const Spawn corners[4] = {
        {5,        5,        9,         9,         1},   // top-left
        {MAP_W-9,  5,        MAP_W-14,  9,         1},   // top-right
        {5,        MAP_H-9,  9,         MAP_H-5,   1},   // bottom-left
        {MAP_W-9,  MAP_H-9,  MAP_W-14,  MAP_H-5,   1},   // bottom-right
    };
    // Free-for-all: one human at a random corner, up to numAIs at the others.
    // Corners beyond what the player chose are left empty.
    int humanCorner = rand() % 4;
    int aiCounter = 0;
    bool spawned[MAX_PLAYERS] = {false};
    for (int c = 0; c < 4; c++) {
        int owner;
        if (c == humanCorner) owner = 0;
        else {
            if (aiCounter >= numAIs) continue;          // skip empty corner
            owner = 1 + aiCounter++;
            if (owner >= MAX_PLAYERS) continue;         // safety net
        }
        spawned[owner] = true;
        auto& s = corners[c];
        spawnEntity(E_TOWNHALL, owner, s.thX, s.thY);
        for (int i = 0; i < 4; i++) spawnEntity(E_PEASANT, owner, s.pX + i*s.pDir, s.pY);
    }
    // Mark any non-spawned slots dead so checkWin doesn't wait on them.
    for (int p = 1; p < MAX_PLAYERS; p++) if (!spawned[p]) g.players[p].alive = false;
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);

    auto& s0 = corners[humanCorner];
    g.cursorX = s0.thX + 2; g.cursorY = s0.thY + 2;
    g.viewX = std::max(0, s0.thX - 10); g.viewY = std::max(0, s0.thY - 5);

    // Wild deer in open terrain
    for (int i = 0, t = 0; i < 25 && t < 600; t++) {
        int ax = 10 + rand()%(MAP_W-20), ay = 10 + rand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST) && !entityAt(ax,ay))
            { spawnEntity(E_DEER, OWNER_NATURE, ax, ay); i++; }
    }
    // Wolves in forested areas
    for (int i = 0, t = 0; i < 5 && t < 600; t++) {
        int ax = 10 + rand()%(MAP_W-20), ay = 10 + rand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS) && !entityAt(ax,ay))
            { spawnEntity(E_WOLF, OWNER_NATURE, ax, ay); i++; }
    }
    // Domestic sheep near each player's town hall (one cluster per occupied corner)
    for (int c = 0; c < 4; c++) {
        int bx = corners[c].thX + 4, by = corners[c].thY + 4;
        for (int i = 0, t = 0; i < 4 && t < 200; t++) {
            int ax = bx+(rand()%7)-3, ay = by+(rand()%7)-3;
            ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
            if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
        }
    }

    updateFog();
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    initColors();
    int numAIs = pickAICount();
    initGame(numAIs);
    setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter]. [A]=select all military.");

    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;
    auto nextTick = Clock::now() + Ms(TICK_MS);

    int lastCx = g.cursorX, lastCy = g.cursorY;
    bool lastDrag = g.dragging;
    while (true) {
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
            nextTick += Ms(TICK_MS);
            if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                g.tick++;
                g.dayPhase += 1.0f / DAY_LENGTH;
                if (g.dayPhase >= 1.0f) g.dayPhase -= 1.0f;
                g.seasonPhase += 1.0f / SEASON_LENGTH;
                if (g.seasonPhase >= 4.0f) g.seasonPhase -= 4.0f;
                for (int i = 0; i < (int)g.entities.size(); i++) tickEntity(g.entities[i]);
                tickSeasons(); tickThaw(); tickWinter();
                tickWeather(); tickPaving();
                tickTowers(); tickGates(); tickProjectiles(); tickFarms(); tickMarkets();
                tickChurches(); tickAnimals(); tickAI(); updateFog();
                if (g.tick % 100 == 0) {
                    g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
                        [](const Entity& e){ return !e.alive && e.state==S_DEAD; }), g.entities.end());
                    checkWin();
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
    endwin();
    return 0;
}
