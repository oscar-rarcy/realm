#include "realm.h"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <locale.h>
#if defined(_WIN32)
#include <windows.h>
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


// Full splash screen. Sets g.biomeChoice and displayMode. Returns numAIs.
static int showSplash() {
    static const char* biomeNames[] = {
        "Temperate","Desert","Snow","Swamp","Forest","Ocean","Random"
    };
    static const char* diffNames[] = { "Easy", "Normal", "Hard" };
    int numAIs = 1;
    int biomeIdx = 6; // 6 = random
    int diffIdx = 1;  // Normal

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

        row++;
        attron(A_BOLD); pr(row++, col, "DISPLAY"); attroff(A_BOLD);
        pr(row++, col, "  [4] ASCII     [5] Emoji");
        pr(row++, col, "  > Display: %s", displayMode == DM_EMOJI ? "Emoji" : "ASCII");

        row++;
        attron(A_BOLD); pr(row++, col, "DIFFICULTY"); attroff(A_BOLD);
        pr(row++, col, "  [E]asy      [N]ormal     [H]ard");

        row++;
        attron(A_BOLD);
        pr(row++, col, "  > Opponents: %d    Biome: %s    Difficulty: %s",
           numAIs, biomeNames[biomeIdx], diffNames[diffIdx]);
        attroff(A_BOLD);

        row++;
        pr(row, col, "  [Enter] Start game            [Q] Quit");

        refresh();
        int ch = getch();
        if (ch=='q'||ch=='Q') { endwin(); exit(0); }
        if (ch=='\n'||ch==KEY_ENTER||ch=='\r') break;
        if (ch=='1') numAIs=1;
        else if (ch=='2') numAIs=2;
        else if (ch=='3') numAIs=3;
        else if (ch=='0') biomeIdx=6;
        else if (ch=='t'||ch=='T') biomeIdx=0;
        else if (ch=='d'||ch=='D') biomeIdx=1;
        else if (ch=='s'||ch=='S') biomeIdx=2;
        else if (ch=='w'||ch=='W') biomeIdx=3;
        else if (ch=='f'||ch=='F') biomeIdx=4;
        else if (ch=='c'||ch=='C') biomeIdx=5;
        else if (ch=='e'||ch=='E') diffIdx=0;
        else if (ch=='n'||ch=='N') diffIdx=1;
        else if (ch=='h'||ch=='H') diffIdx=2;
        else if (ch=='4') displayMode = DM_ASCII;
        else if (ch=='5') displayMode = DM_EMOJI;
    }
    g.difficulty = diffIdx;
    // 6 = Random/mixed; otherwise pass index 0..5 (Ocean is 5 in the local list,
    // but B_OCEAN is enum value 6; remap so the engine sees the right Biome enum).
    if (biomeIdx == 6)      g.biomeChoice = -1;
    else if (biomeIdx == 5) g.biomeChoice = B_OCEAN;  // was index 6 before volcanic removal
    else                    g.biomeChoice = biomeIdx;
    return numAIs;
}

void initGame(int numAIs, unsigned long long seed) {
    // Seed the deterministic sim RNG. Replays pass the recorded seed; a
    // future multiplayer lobby shares the host's seed with every client.
    if (seed == 0) seed = (unsigned long long)time(nullptr) * 2654435761ull + 1;
    g.simSeed = seed;
    seedSimRng(seed);
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

    generateMap();

    // === SPAWN PLACEMENT ===
    // Build a candidate pool of well-spaced positions across the map (not just
    // corners). Each candidate is scored by spawn-friendliness — open ground,
    // not in mountains/water — and the top-scoring positions are chosen such
    // that no two are within MIN_SPAWN_DIST tiles of each other.
    struct Spawn { int thX, thY; };
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
    std::vector<Spawn> spawns;
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
        spawnEntity(E_TOWNHALL, owner, spawns[i].thX, spawns[i].thY);
        for (int j = 0; j < 4; j++)
            spawnEntity(E_PEASANT, owner, spawns[i].thX + 4 + j, spawns[i].thY + 4);
    }
    // Mark any non-spawned slots dead so checkWin doesn't wait on them.
    for (int p = 1; p < MAX_PLAYERS; p++) if (!spawned[p]) g.players[p].alive = false;
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);

    g.cursorX = spawns[0].thX + 2; g.cursorY = spawns[0].thY + 2;
    g.viewX = std::max(0, spawns[0].thX - 10); g.viewY = std::max(0, spawns[0].thY - 5);

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
    // Wolves in forested areas — must spawn well clear of every player base.
    for (int i = 0, t = 0; i < 7 && t < 600; t++) {
        int ax = 10 + simRand()%(MAP_W-20), ay = 10 + simRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_WOLF, OWNER_NATURE, ax, ay); i++; }
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
    // Domestic sheep near each chosen player spawn (one cluster per spawn).
    for (auto& sp : spawns) {
        int bx = sp.thX + 4, by = sp.thY + 4;
        for (int i = 0, t = 0; i < 4 && t < 200; t++) {
            int ax = bx+(simRand()%7)-3, ay = by+(simRand()%7)-3;
            ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
            if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
        }
    }

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
            if (g.mode != M_PAUSED && g.mode != M_GAME_OVER) simTick();
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
static int runVerify(unsigned long long seed, int ticks, int numAIs) {
    g.biomeChoice = 0;   // fixed biome + difficulty: no menu dependence
    g.difficulty  = 1;
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
        return runVerify(seed, std::max(1, ticks), std::max(1, std::min(3, numAIs)));
    }

    // --verify-replay: headless playback of a recorded match. Run it twice
    // and compare hashes — a recorded game that replays identically is the
    // end-to-end proof the funnel + sim are deterministic.
    if (argc >= 3 && strcmp(argv[1], "--verify-replay") == 0) {
        unsigned long long seed; int ais, biome, diffc;
        if (!replayLoadFile(argv[2], seed, ais, biome, diffc)) {
            fprintf(stderr, "Can't read replay '%s'.\n", argv[2]);
            return 1;
        }
        g.biomeChoice = biome;
        g.difficulty  = diffc;
        initGame(ais, seed);
        int ticks = (argc >= 4) ? std::max(1, atoi(argv[3])) : 5000;
        for (int i = 0; i < ticks; i++) simTick();
        printf("replay=%s ticks=%d hash=%016llx\n", argv[2], ticks, simStateHash());
        return 0;
    }

    // --replay: load header before touching the screen so a bad file can
    // fail to stderr instead of into a half-initialised terminal.
    bool replay = false;
    unsigned long long repSeed = 0; int repAIs = 1, repBiome = -1, repDiff = 1;
    if (argc >= 3 && strcmp(argv[1], "--replay") == 0) {
        if (!replayLoadFile(argv[2], repSeed, repAIs, repBiome, repDiff)) {
            fprintf(stderr, "Can't read replay '%s' (missing, wrong version, or corrupt).\n", argv[2]);
            return 1;
        }
        replay = true;
    }

    initscr(); cbreak(); noecho(); keypad(stdscr, TRUE); curs_set(0);
    // REPORT_MOUSE_POSITION gives continuous hover events for live cursor tracking
    mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
    initColors();

    if (replay) {
        g.biomeChoice = repBiome;
        g.difficulty  = repDiff;
        initGame(repAIs, repSeed);
        setStatus("REPLAY — commands come from the recording. Camera/selection are yours; [Q][Q] to quit.");
        runMatch();
        endwin();
        return 0;
    }

    while (true) {
        int numAIs = showSplash();

        // Display mode is selected on the splash screen. Reinitialise colour
        // pairs here so emoji mode can use filled terrain backgrounds while
        // ASCII mode keeps the original mostly-transparent look.
        initColors();

        initGame(numAIs);
        replayStartRecording(numAIs);   // every match is recorded; replays/ dir
        setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter]. [A]=select all military.");

        runMatch();
        replayStopRecording();
        // returnToMenu set — loop back to splash.
    }
    endwin();
    return 0;
}
