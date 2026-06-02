#include "realm.h"
#include "env_config.h"
#include "entity_animation.h"
#include <cassert>
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <locale.h>
#include <sstream>
#if defined(_WIN32)
#include <windows.h>
#endif

static bool isUtf8LocaleName(const char* name) {
    if (!name) return false;
    return std::strstr(name, "UTF-8") || std::strstr(name, "utf8")
        || std::strstr(name, "UTF8")  || std::strstr(name, "65001");
}

void forceUtf8Locale() {
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
    setlocale(LC_ALL, ".UTF8");
}


#ifndef USE_SDL_RENDERER
// Full splash screen. Sets g.biomeChoice and displayMode. Returns numAIs.
static int showSplash() {
    static const char* biomeNames[] = {
        "Temperate","Desert","Snow","Swamp","Forest","Volcanic","Ocean","Random"
    };
    int numAIs = 1;
    int biomeIdx = 7; // 7 = random
    const bool asciiOnly = realmVisualModeIsAsciiOnly();

    int maxY, maxX;
    while (true) {
        getmaxyx(stdscr, maxY, maxX);
        erase();

        int col = std::max(2, maxX/2 - 34);
        int row = std::max(0, maxY/2 - 15);

        auto pr = [&](int r, int c, const char* fmt, ...) {
            va_list ap; va_start(ap, fmt);
            char buf[256]; vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap);
            mvprintw(r, c, "%s", buf);
        };

        attron(A_BOLD);
        pr(row,   col+17, "R  E  A  L  M");
        pr(row+1, col+13, "-- Medieval Warlord --");
        attroff(A_BOLD);

        row += 3;
        pr(row++, col, "You are lord of a small settlement in a hostile");
        pr(row++, col, "realm. Gather resources, build an army, and");
        pr(row++, col, "outlast every rival. Survive winter or starve.");

        row++;
        attron(A_BOLD); pr(row++, col, "CONTROLS"); attroff(A_BOLD);
        pr(row++, col, "  Space/click    Select unit or building");
        pr(row++, col, "  Enter/R-click  Command (move/attack/gather)");
        pr(row++, col, "  B              Build menu");
        pr(row++, col, "  T              Train units");
        pr(row++, col, "  A              Select all military");
        pr(row++, col, "  H              Jump to town hall");
        pr(row++, col, "  1-9 / G        Control groups");
        pr(row++, col, "  P              Pause");

        row++;
        attron(A_BOLD); pr(row++, col, "TIPS"); attroff(A_BOLD);
        pr(row++, col, "  Stockpile food before winter (1 food/unit/8s).");
        pr(row++, col, "  Boars fight back. Wolves hunt in winter.");
        pr(row++, col, "  Catapults need 2+ tiles of standoff to fire.");

        row++;
        attron(A_BOLD); pr(row++, col, "OPPONENTS"); attroff(A_BOLD);
        pr(row++, col, "  [1] Duel       [2] Three-way     [3] Four-way");

        row++;
        attron(A_BOLD); pr(row++, col, "BIOME"); attroff(A_BOLD);
        pr(row++, col, "  [0] Random    [T] Temperate  [D] Desert");
        pr(row++, col, "  [S] Snow      [W] Swamp      [F] Forest");
        pr(row++, col, "  [C] Coastal");

        if (!asciiOnly) {
            row++;
            attron(A_BOLD); pr(row++, col, "DISPLAY"); attroff(A_BOLD);
            pr(row++, col, "  [4] ASCII     [5] Emoji");
            pr(row++, col, "  > Display: %s", displayMode == DM_EMOJI ? "Emoji" : "ASCII");
        }

        row++;
        attron(A_BOLD);
        pr(row++, col, "  > Opponents: %d    Biome: %s", numAIs, biomeNames[biomeIdx]);
        attroff(A_BOLD);

        row++;
        pr(row, col, "  [Enter] Start game            [Q/X] Quit");

        refresh();
        int ch = getch();
        if (ch=='q'||ch=='Q'||ch=='x'||ch=='X') { endwin(); exit(0); }
        if (ch=='\n'||ch==KEY_ENTER||ch=='\r') break;
        if (ch=='1') numAIs=1;
        else if (ch=='2') numAIs=2;
        else if (ch=='3') numAIs=3;
        else if (ch=='0') biomeIdx=7;
        else if (ch=='t'||ch=='T') biomeIdx=0;
        else if (ch=='d'||ch=='D') biomeIdx=1;
        else if (ch=='s'||ch=='S') biomeIdx=2;
        else if (ch=='w'||ch=='W') biomeIdx=3;
        else if (ch=='f'||ch=='F') biomeIdx=4;
        else if (ch=='c'||ch=='C') biomeIdx=6;
        else if (ch=='4') displayMode = DM_ASCII;
        else if (ch=='5' && !asciiOnly) displayMode = DM_EMOJI;
    }
    g.biomeChoice = (biomeIdx == 7) ? -1 : biomeIdx;
    return numAIs;
}
#endif

static int envInt(const char* name, int fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    long parsed = std::strtol(v, &end, 10);
    return (end && *end == '\0') ? (int)parsed : fallback;
}

static unsigned envUnsigned(const char* name, unsigned fallback) {
    const char* v = std::getenv(name);
    if (!v || !*v) return fallback;
    char* end = nullptr;
    unsigned long parsed = std::strtoul(v, &end, 10);
    return (end && *end == '\0') ? (unsigned)parsed : fallback;
}

static void placeStartResources(int thX, int thY) {
    for (int i = 0; i < 7; i++) {
        int x = std::max(1, std::min(thX + 6 + i % 3, MAP_W - 2));
        int y = std::max(1, std::min(thY + 1 + i / 3, MAP_H - 2));
        if (!entityAt(x, y)) {
            g.map[y][x].terrain = T_FOREST;
            g.map[y][x].resources = 120;
            g.map[y][x].preWinterTerrain = T_FOREST;
        }
    }
    for (int i = 0; i < 5; i++) {
        int x = std::max(1, std::min(thX + 1 + i, MAP_W - 2));
        int y = std::max(1, std::min(thY + 7, MAP_H - 2));
        if (!entityAt(x, y)) {
            g.map[y][x].terrain = T_BERRY;
            g.map[y][x].resources = 70;
            g.map[y][x].preWinterTerrain = T_BERRY;
        }
    }
}

void initGame(int numAIs) {
    unsigned seedFallback = (unsigned)time(nullptr);
    unsigned seed = envUnsigned("REALM_SEED", seedFallback);
    int humanCorner = envInt("REALM_HUMAN_CORNER", -1);
    int forcedBiome = envInt("REALM_BIOME", g.biomeChoice);
    if (forcedBiome >= -1 && forcedBiome <= B_OCEAN) g.biomeChoice = forcedBiome;
    initGameWithSeed(numAIs, seed, humanCorner);
}

void initGameWithSeed(int numAIs, unsigned seed, int humanCorner) {
    realmSrand(seed);
    // `g.entities` is a deque so spawnEntity() can append during a tick without
    // invalidating the Entity references and pointers held by simulation code.
    int matchNumber = g.matchNumber + 1;
    g.entities.clear();
    g.projectiles.clear();
    g.actionMarkers.clear();
    g.projectiles.reserve(256);
    g.nextId = 1; g.tick = 0; g.mode = M_NORMAL;
    g.selectedId = -1; g.selectedIds.clear(); g.groupAssignPending = false;
    g.dragging = false; g.dragStartX = 0; g.dragStartY = 0;
    for (int i = 0; i < 9; i++) g.controlGroups[i].clear();
    g.winner = -1; g.aiTimer = 0; g.farmTimer = 0; g.animalTimer = 0;
    g.statusMsg.clear(); g.statusTimer = 0;
    g.buildPending = E_NONE; g.wallDragX = 0; g.wallDragY = 0;
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f; g.prevSeason = -1;
    g.prevTimePhase = 0; g.attackNotifyCd = 0;
    g.weather = W_CLEAR; g.weatherTimer = 0;
    g.returnToMenu = false;
    g.seed = seed;
    g.startupAIs = numAIs;
    g.humanCorner = -1;
    g.matchNumber = matchNumber;
    g.diagnostics = std::getenv("REALM_DIAGNOSTICS") != nullptr;
    g.helpOverlay = false;
    // biomeChoice is set by showSplash before initGame is called; don't reset it here.
    for (int p = 0; p < MAX_PLAYERS; p++)
        g.players[p] = {300, 200, 100, 0, 0, true, 0, 0};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true, 0, 0};

    generateMap();

    struct Spawn { int thX, thY; };
    const int needed = std::min(MAX_PLAYERS, 1 + numAIs);
    const int minSpawnDist = std::min(MAP_W, MAP_H) * 2 / 3;
    const int edge = 12;

    auto scoreSpawn = [](int cx, int cy) -> int {
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) return -1;
            Terrain t = g.map[y][x].terrain;
            if (t==T_WATER||t==T_MOUNTAIN||t==T_LAVA||t==T_SHALLOWS||t==T_GOLD) return -1;
        }
        int score = 100;
        int grass = 0;
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) continue;
            Terrain t = g.map[y][x].terrain;
            if (t==T_GRASS||t==T_MEADOW||t==T_DIRT||t==T_TALL_GRASS) grass++;
        }
        score += grass;
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

    struct Cand { int x, y, score; };
    std::vector<Cand> candidates;
    candidates.reserve(260);
    for (int i = 0; i < 260; i++) {
        int cx = edge + realmRand() % (MAP_W - 2*edge);
        int cy = edge + realmRand() % (MAP_H - 2*edge);
        int s = scoreSpawn(cx, cy);
        if (s > 0) candidates.push_back({cx, cy, s});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Cand& a, const Cand& b){ return a.score > b.score; });

    std::vector<Spawn> spawns;
    for (auto& c : candidates) {
        if ((int)spawns.size() >= needed) break;
        bool ok = true;
        for (auto& s : spawns) {
            if (dist(c.x, c.y, s.thX, s.thY) < minSpawnDist) { ok = false; break; }
        }
        if (ok) spawns.push_back({c.x, c.y});
    }
    if ((int)spawns.size() < needed) {
        int relaxed = minSpawnDist / 2;
        for (auto& c : candidates) {
            if ((int)spawns.size() >= needed) break;
            bool duplicate = false;
            for (auto& s : spawns) if (s.thX==c.x && s.thY==c.y) { duplicate = true; break; }
            if (duplicate) continue;
            bool ok = true;
            for (auto& s : spawns) if (dist(c.x, c.y, s.thX, s.thY) < relaxed) { ok = false; break; }
            if (ok) spawns.push_back({c.x, c.y});
        }
    }
    const int cornerAnchors[4][2] = {
        {5, 5}, {MAP_W-9, 5}, {5, MAP_H-9}, {MAP_W-9, MAP_H-9}
    };
    if (spawns.empty()) {
        for (int i = 0; i < needed; i++) spawns.push_back({cornerAnchors[i][0], cornerAnchors[i][1]});
    }
    if (humanCorner < 0 || humanCorner >= 4) {
        humanCorner = realmRand() % 4;
        if (spawns.size() > 1) std::swap(spawns[0], spawns[realmRand() % spawns.size()]);
    } else if (spawns.size() > 1) {
        int best = 0;
        for (int i = 1; i < (int)spawns.size(); i++) {
            if (dist(spawns[i].thX, spawns[i].thY, cornerAnchors[humanCorner][0], cornerAnchors[humanCorner][1])
                < dist(spawns[best].thX, spawns[best].thY, cornerAnchors[humanCorner][0], cornerAnchors[humanCorner][1]))
                best = i;
        }
        std::swap(spawns[0], spawns[best]);
    }
    g.humanCorner = humanCorner;

    bool spawned[MAX_PLAYERS] = {false};
    for (int i = 0; i < (int)spawns.size() && i <= numAIs; i++) {
        int owner = (i == 0) ? 0 : i;
        if (owner >= MAX_PLAYERS) break;
        spawned[owner] = true;
        clearStartArea(spawns[i].thX - 2, spawns[i].thY - 2, 6);
        placeGoldCluster(spawns[i].thX + 9, spawns[i].thY + 4, 5);
        placeStartResources(spawns[i].thX, spawns[i].thY);
        spawnEntity(E_TOWNHALL, owner, spawns[i].thX, spawns[i].thY);
        for (int j = 0; j < 4; j++)
            spawnEntity(E_PEASANT, owner, spawns[i].thX + 4 + j, spawns[i].thY + 4);
    }
    // Mark any non-spawned slots dead so checkWin doesn't wait on them.
    for (int p = 1; p < MAX_PLAYERS; p++) if (!spawned[p]) g.players[p].alive = false;
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);

    g.cursorX = spawns[0].thX + 2; g.cursorY = spawns[0].thY + 2;
    g.viewX = std::max(0, spawns[0].thX - 10); g.viewY = std::max(0, spawns[0].thY - 5);

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
                int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && farFromAnyBase(ax, ay, 14))
                    { hx=ax; hy=ay; }
            }
            if (hx < 0) continue;
            int herdSize = 3 + realmRand()%4;
            for (int i = 0, t = 0; i < herdSize && t < 100; t++) {
                int ax = hx+(realmRand()%9)-4, ay = hy+(realmRand()%9)-4;
                ax = std::max(1, std::min(ax, MAP_W-2));
                ay = std::max(1, std::min(ay, MAP_H-2));
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && !entityAt(ax,ay) && farFromAnyBase(ax, ay, 10))
                    { spawnEntity(E_DEER, OWNER_NATURE, ax, ay); i++; total++; }
            }
        }
    }
    // Wolves in forested areas
    for (int i = 0, t = 0; i < 7 && t < 600; t++) {
        int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_WOLF, OWNER_NATURE, ax, ay); i++; }
    }
    // Boars in temperate woodland and forest biomes
    for (int i = 0, t = 0; i < 18 && t < 800; t++) {
        int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        Biome  b  = g.map[ay][ax].biome;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS||tr==T_GRASS)
            && (b==B_TEMPERATE||b==B_FOREST) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_BOAR, OWNER_NATURE, ax, ay); i++; }
    }
    // Domestic sheep near each player's town hall (one cluster per chosen spawn)
    for (int i = 0; i < (int)spawns.size() && i <= numAIs; i++) {
        int bx = spawns[i].thX + 4, by = spawns[i].thY + 4;
        for (int i = 0, t = 0; i < 4 && t < 200; t++) {
            int ax = bx+(realmRand()%7)-3, ay = by+(realmRand()%7)-3;
            ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
            if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
        }
    }

    updateFog();
    resetDetectMapCache();
    std::cerr << "realm: match start"
              << " match=" << g.matchNumber
              << " seed=" << g.seed
              << " ai=" << g.startupAIs
              << " humanCorner=" << g.humanCorner
              << " biome=" << g.biomeChoice
              << " entities=" << g.entities.size()
              << " projectiles=" << g.projectiles.size() << "\n";
}

#ifndef USE_SDL_RENDERER
int main(int argc, char** argv) {
    forceUtf8Locale();
    if (argc >= 2 && std::strcmp(argv[1], "--dump-missing-tileset-assets") == 0) {
        return dumpMissingTilesetAssets();
    }
    if (argc >= 2 && std::strcmp(argv[1], "--dump-animation-spec") == 0) {
        const char* entityArg = argc >= 3 ? argv[2] : "peasant";
        EntityType type = entityTypeForAnimationSlug(entityArg);
        if (!writeEntityAnimationSpecJson(std::cout, type)) {
            std::cerr << "unknown entity animation spec: " << entityArg << "\n";
            return 2;
        }
        return 0;
    }
    loadRealmEnvironmentFiles();
    displayMode = DM_ASCII;
    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    initColors();

    using Clock = std::chrono::steady_clock;
    using Ms    = std::chrono::milliseconds;

    while (true) {
        int numAIs = showSplash();
        initGame(numAIs);
        setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter]. [A]=select all military.");

        auto nextTick = Clock::now() + Ms(TICK_MS);
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
                nextTick += Ms(TICK_MS);
                if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
                    tickSimulationOnce();
                }
                render();
                ticked = true;
            }
            // Snappy cursor: redraw between ticks when the mouse moved or a drag updated.
            bool cursorMoved = (g.cursorX != lastCx || g.cursorY != lastCy || g.dragging != lastDrag);
            if (!ticked && cursorMoved) render();
            lastCx = g.cursorX; lastCy = g.cursorY; lastDrag = g.dragging;
        }
        // returnToMenu set — loop back to splash.
    }
    endwin();
    return 0;
}
#endif
