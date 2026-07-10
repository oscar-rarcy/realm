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

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Browser build: saves/replays/config live in an IndexedDB-backed
// filesystem (IDBFS). Writes land in MEMFS instantly; syncfs pushes them
// down to IndexedDB so they survive a tab reload. The pull at startup is
// async — Asyncify lets main() just sleep until it lands.
EM_JS(void, realmIdbfsMount, (), {
    FS.mkdir('/realm-data');
    FS.mount(IDBFS, {}, '/realm-data');
    Module.realmFsReady = 0;
    FS.syncfs(true, function(err) { Module.realmFsReady = 1; });
});
EM_JS(int, realmIdbfsReady, (), { return Module.realmFsReady|0; });
EM_JS(void, realmIdbfsPersist, (), { FS.syncfs(false, function(err) {}); });
void platformPersistFiles() { realmIdbfsPersist(); }
#else
// Native builds write straight to disk; nothing to flush.
void platformPersistFiles() {}
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
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f; g.year = 1; g.prevSeason = -1;
    g.prevTimePhase = 0; g.attackNotifyCd = 0;
    g.returnToMenu = false;
    g.cursorByMouse = false;
    g.winterSeverity = 1;
    g.siteHoldOwner = -1; g.siteHoldTicks = 0;
    g.statSamples.clear();
    memset(g.statRaids, 0, sizeof g.statRaids);
    memset(g.statEraTick, 0, sizeof g.statEraTick);
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
    // Spacing scales with the head count: duels get ~73 tiles apart on
    // 180x110, a full 8-seat brawl still guarantees ~31 before the relaxed
    // fallback kicks in.
    const int MIN_SPAWN_DIST = std::min(MAP_W, MAP_H) * 2 / std::max(3, needed - 1);
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
    if (g.seasonPhase >= 4.0f) { g.seasonPhase -= 4.0f; g.year++; }
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
    // Chart sampler (presentation only): a per-player snapshot every 250
    // ticks feeds the post-match statistics screen.
    if (g.tick % 250 == 0) {
        Game::StatSample smp{};
        for (auto& e : g.entities) {
            if (!e.alive || e.owner < 0 || e.owner >= MAX_PLAYERS) continue;
            if (e.type == E_PEASANT || e.type == E_WAGON || e.type == E_FISHING_BOAT) smp.work[e.owner]++;
            else if (isUnit(e.type) && STATS[e.type].atk > 0) smp.army[e.owner]++;
        }
        for (int p = 0; p < MAX_PLAYERS; p++)
            smp.wealth[p] = g.players[p].gold + g.players[p].wood + g.players[p].food;
        g.statSamples.push_back(smp);
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
                           && g.mode != M_HELP && g.mode != M_SAVELOAD && g.mode != M_STATS;
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
    int sitesTotal = 0, sitesHeld[MAX_PLAYERS] = {};
    for (auto& e : g.entities) {
        if (!e.alive || !isClaimable(e.type)) continue;
        sitesTotal++;
        if (e.owner >= 0 && e.owner < MAX_PLAYERS) sitesHeld[e.owner]++;
    }
    printf("seed=%llu ticks=%d ais=%d hash=%016llx winner=%d sites=%d held=%d/%d/%d/%d clock=P%d+%d\n",
           seed, ticks, numAIs, simStateHash(), g.winner, sitesTotal,
           sitesHeld[0], sitesHeld[1], sitesHeld[2], sitesHeld[3],
           g.siteHoldOwner + 1, g.siteHoldTicks);
    // Per-seat balance probe: is anyone stuck in an era, starved, or wiped out?
    for (int pl = 0; pl < MAX_PLAYERS; pl++) {
        const Player& P = g.players[pl];
        int peas = 0, mili = 0, blds = 0, farms = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != pl) continue;
            if (e.type == E_PEASANT) peas++;
            else if (isUnit(e.type) && !isNaval(e.type)) mili++;
            else if (isBuilding(e.type)) { blds++; if (e.type == E_FARM) farms++; }
        }
        if (peas + mili + blds == 0 && !P.alive && pl > numAIs) continue;   // never seated
        printf("  P%d %-12s era=%-10s persona=%d %s peas=%d mil=%d blds=%d farms=%d g/w/f=%d/%d/%d research=%03x raids=%d\n",
               pl + 1, CIVS[P.civ].name, eraName(P.era), P.aiPersona,
               P.alive ? "alive" : "DEAD ", peas, mili, blds, farms, P.gold, P.wood, P.food, P.research,
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
        return runVerify(seed, std::max(1, ticks), std::max(1, std::min(MAX_PLAYERS - 1, numAIs)), biome, layout);
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

    // --test-sow: headless check of the player farm pipeline. Stages a wild
    // wheat meadow beside the human's start, group-right-clicks it through
    // the command funnel (CMD_SOW_FARM with three peasants), and requires a
    // field on the clicked tile, two more tiling the meadow, and grain
    // production within 2000 ticks.
    if (argc >= 2 && strcmp(argv[1], "--test-sow") == 0) {
        g.biomeChoice = 0; g.layoutChoice = 0; g.difficulty = 1;
        g.humanMask = 1; g.localPlayer = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = -1;
        // No AI seat: this gate is about the farm pipeline, and a hostile
        // roll of the sim RNG once razed the test town mid-window.
        initGame(0, 777);
        Entity* th = nullptr; Entity* peas = nullptr;
        std::vector<int> sowers;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != 0) continue;
            if (e.type == E_TOWNHALL) th = &e;
            if (e.type == E_PEASANT) {
                if (!peas) peas = &e;
                if (sowers.size() < 3) sowers.push_back(e.id);
            }
        }
        if (!th || !peas || sowers.size() < 3) { fprintf(stderr, "no start town hall / 3 peasants\n"); return 1; }
        // Paint a 4x4 wheat meadow on open ground near the hall — room for
        // four 2x2 fields, so a three-peasant group sow must yield three.
        int wx = -1, wy = -1;
        for (int a = 0; a < 4000 && wx < 0; a++) {
            int x = th->x + (simRand()%17) - 8, y = th->y + (simRand()%17) - 8;
            if (dist(th->x, th->y, x, y) < 4) continue;   // off the hall's doorstep
            if (canPlace(E_FARM, x, y, 0) && canPlace(E_FARM, x+2, y, 0)
             && canPlace(E_FARM, x, y+2, 0) && canPlace(E_FARM, x+2, y+2, 0)) { wx = x; wy = y; }
        }
        if (wx < 0) { fprintf(stderr, "no sowable ground near start\n"); return 1; }
        for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 4; dx++)
            g.map[wy+dy][wx+dx].terrain = T_WHEAT;
        Command c; c.type = CMD_SOW_FARM; c.player = 0;
        c.x = wx; c.y = wy; c.units = sowers;
        pushCommand(c);
        int maxCarry = 0, farmId = -1, inFieldTicks = 0, tendTiles = 0;
        unsigned seenTiles = 0;   // bitmask of footprint tiles the tender stood on
        int peasId = peas->id;
        // The user-facing bug shape is "built the farm, then stood idle" —
        // so the tender's idle time inside the window is a tracked failure.
        int idleTicks = 0, firstIdle = -1;
        for (int i = 0; i < 2000; i++) {
            simTick();
            Entity* p2 = findEntity(peasId);
            if (p2 && p2->alive && p2->state == S_IDLE) {
                idleTicks++;
                if (firstIdle < 0) firstIdle = i;
            }
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != 0 || e.type != E_FARM) continue;
                if (wx >= e.x && wx < e.x + STATS[E_FARM].sizeW
                 && wy >= e.y && wy < e.y + STATS[E_FARM].sizeH) {
                    farmId = e.id;
                    maxCarry = std::max(maxCarry, e.carrying);
                    // Tenders work from INSIDE the square — and move around it.
                    if (p2 && p2->alive && p2->x >= e.x && p2->x < e.x + STATS[E_FARM].sizeW
                           && p2->y >= e.y && p2->y < e.y + STATS[E_FARM].sizeH) {
                        inFieldTicks++;
                        seenTiles |= 1u << ((p2->y - e.y) * STATS[E_FARM].sizeW + (p2->x - e.x));
                    }
                }
            }
        }
        for (unsigned m = seenTiles; m; m >>= 1) tendTiles += (int)(m & 1);
        int farms = 0;
        for (auto& e : g.entities)
            if (e.alive && e.owner == 0 && e.type == E_FARM) farms++;
        printf("farm=%s farms=%d footprint=%dx%d maxCarry=%d grain=%d inFieldTicks=%d tendTiles=%d idleTicks=%d firstIdle=%d\n",
               farmId >= 0 ? "yes" : "NO", farms, STATS[E_FARM].sizeW, STATS[E_FARM].sizeH,
               maxCarry, g.players[0].food, inFieldTicks, tendTiles, idleTicks, firstIdle);
        return (farmId >= 0 && farms >= 3 && maxCarry > 0 && inFieldTicks > 0 && tendTiles >= 2
                && idleTicks < 100) ? 0 : 1;
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
        // --net-host [ticks] [ais] [clients]: wait for N challengers (4P test).
        int wantJoin = (netHost && argc >= 5)
                     ? std::max(1, std::min(MAX_NET_CLIENTS, atoi(argv[4]))) : 1;
        NetMatchConfig nc;
        int slot;
        if (netHost) {
            nc.seed = 424242; nc.numAIs = ais; nc.biome = 0; nc.layout = 0;
            nc.difficulty = 1; nc.speed = 1;
            if (!netHostOpen()) { fprintf(stderr, "netHostOpen failed\n"); return 1; }
            netHostSetInfo(nc);
            fprintf(stderr, "hosting on :%d, waiting for %d joiner(s)...\n", NET_TCP_PORT, wantJoin);
            while (netSeatedCount() < wantJoin) {
                if (!netHostPoll()) { fprintf(stderr, "lobby error\n"); return 1; }
                msleep(10);
            }
            if (!netHostStart()) { fprintf(stderr, "start failed\n"); return 1; }
            nc = netFinalConfig();    // the real humanMask / clamped AI count
            slot = 0;
        } else {
            std::string err;
            if (!netJoinConnect(argv[2], NET_TCP_PORT, err)) { fprintf(stderr, "%s\n", err.c_str()); return 1; }
            int rc2;
            while ((rc2 = netClientPoll(nc)) != 2) {
                if (rc2 < 0) { fprintf(stderr, "lost host in lobby\n"); return 1; }
                msleep(5);
            }
            slot = netMySeat();       // assigned by the host's roster
        }
        g.difficulty = nc.difficulty; g.biomeChoice = nc.biome; g.layoutChoice = nc.layout;
        g.humanMask = nc.humanMask; g.localPlayer = slot;
        for (int i = 0; i < MAX_PLAYERS; i++) g.civChoice[i] = nc.civ[i];
        initGame(nc.numAIs, nc.seed);
        int stall = 0;
        // REALM_NET_PAUSE_TEST=1: host stalls 12s at tick 1000 exactly like a
        // player pausing (pumping the wire, not ticking). The link must ride
        // it out on keepalives — this is the regression test for the >10s
        // pause reading as a dead peer.
        bool pauseTest = netHost && getenv("REALM_NET_PAUSE_TEST");
        while (g.tick < ticks) {
            if (netConnectionLost()) { fprintf(stderr, "connection lost at tick %d\n", g.tick); return 1; }
            if (netDesynced()) { fprintf(stderr, "DESYNC at tick %d\n", netDesyncTick()); return 1; }
            if (pauseTest && g.tick == 1000) {
                pauseTest = false;
                fprintf(stderr, "pause-test: host stalling 12s at tick 1000...\n");
                for (int i = 0; i < 120 && !netConnectionLost(); i++) { netPump(); msleep(100); }
                if (netConnectionLost()) { fprintf(stderr, "pause-test: link died during the stall\n"); return 1; }
                fprintf(stderr, "pause-test: link survived, resuming\n");
            }
            // Scripted human traffic: each seat periodically orders one of its
            // own units around. Different phases so every leg carries load.
            if (g.tick % 37 == (slot * 5) % 37) {
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
            else { msleep(2); if (++stall > 10000) { fprintf(stderr, "stalled at tick %d\n", g.tick); return 1; } }
        }
        printf("net-%s seat=%d ticks=%d hash=%016llx\n",
               netHost ? "host" : "join", slot, g.tick, simStateHash());
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

#ifdef __EMSCRIPTEN__
    // Work out of the persistent IndexedDB mount from the first frame on.
    realmIdbfsMount();
    while (!realmIdbfsReady()) emscripten_sleep(20);
    if (chdir("/realm-data") == 0) mkdir("replays", 0755);
#endif

#if defined(USE_SDL_SHIM) && !defined(_WIN32) && !defined(__EMSCRIPTEN__)
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
