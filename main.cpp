#include "realm.h"
#include <chrono>

void initGame() {
    srand((unsigned)time(nullptr));
    g.entities.reserve(512);
    g.projectiles.reserve(256);
    g.nextId = 1; g.tick = 0; g.mode = M_NORMAL;
    g.selectedId = -1; g.selectedIds.clear(); g.groupAssignPending = false;
    g.dragging = false; g.dragStartX = 0; g.dragStartY = 0;
    for (int i = 0; i < 9; i++) g.controlGroups[i].clear();
    g.winner = -1; g.aiTimer = 0; g.farmTimer = 0; g.statusTimer = 0;
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f;
    g.players[0] = {300, 200, 100, 0, 0, true};
    g.players[1] = {300, 200, 100, 0, 0, true};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true};

    generateMap();

    spawnEntity(E_TOWNHALL, 0, 5, 5);
    for (int i = 0; i < 4; i++) spawnEntity(E_PEASANT, 0, 9+i, 9);
    spawnEntity(E_TOWNHALL, 1, MAP_W-9, MAP_H-9);
    for (int i = 0; i < 4; i++) spawnEntity(E_PEASANT, 1, MAP_W-8+i, MAP_H-5);
    updateSupply(0); updateSupply(1);

    g.cursorX = 7; g.cursorY = 7; g.viewX = 0; g.viewY = 0;

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
    // Domestic sheep near each TC
    for (int i = 0, t = 0; i < 5 && t < 200; t++) {
        int ax = 8+(rand()%7)-3, ay = 8+(rand()%7)-3;
        ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
        if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
    }
    for (int i = 0, t = 0; i < 5 && t < 200; t++) {
        int ax = (MAP_W-9)+(rand()%7)-3, ay = (MAP_H-9)+(rand()%7)-3;
        ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
        if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
    }

    updateFog();
}

int main() {
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    initColors();
    initGame();
    setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter]. [A]=select all military.");

    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;
    auto nextTick = Clock::now() + Ms(TICK_MS);

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
        if (Clock::now() >= nextTick) {
            nextTick += Ms(TICK_MS);
            if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                g.tick++;
                g.dayPhase += 1.0f / DAY_LENGTH;
                if (g.dayPhase >= 1.0f) g.dayPhase -= 1.0f;
                g.seasonPhase += 1.0f / SEASON_LENGTH;
                if (g.seasonPhase >= 4.0f) g.seasonPhase -= 4.0f;
                for (int i = 0; i < (int)g.entities.size(); i++) tickEntity(g.entities[i]);
                tickTowers(); tickProjectiles(); tickFarms(); tickMarkets();
                tickChurches(); tickAnimals(); tickAI(); updateFog();
                if (g.tick % 100 == 0) {
                    g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
                        [](const Entity& e){ return !e.alive && e.state==S_DEAD; }), g.entities.end());
                    checkWin();
                }
            }
            render();
        }
    }
    endwin();
    return 0;
}
