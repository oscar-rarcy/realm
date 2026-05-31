#include "realm.h"

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <queue>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <vector>

static std::string startupSummary() {
    std::map<std::string, int> counts;
    for (const auto& e : g.entities) {
        if (!e.alive) continue;
        std::ostringstream k;
        k << e.owner << ':' << (int)e.type << ':' << e.x << ':' << e.y;
        counts[k.str()]++;
    }
    std::ostringstream s;
    s << "seed=" << g.seed << " corner=" << g.humanCorner << " next=" << g.nextId;
    for (const auto& kv : counts) s << '|' << kv.first << '=' << kv.second;
    for (int y = 0; y < MAP_H; y += 7) {
        for (int x = 0; x < MAP_W; x += 11) {
            const Tile& t = g.map[y][x];
            s << "|m" << x << ',' << y << ':' << (int)t.terrain << ':' << t.resources << ':' << (int)t.biome;
        }
    }
    return s.str();
}

static std::string fullStateSummary() {
    std::ostringstream s;
    s << startupSummary() << "|tick=" << g.tick << "|rng=" << g.rngState
      << "|weather=" << g.weather << ':' << g.weatherTimer
      << "|phase=" << g.dayPhase << ':' << g.seasonPhase
      << "|timers=" << g.aiTimer << ':' << g.farmTimer << ':' << g.animalTimer;
    for (int p = 0; p <= MAX_PLAYERS; p++) {
        const Player& pl = g.players[p];
        s << "|p" << p << ':' << pl.gold << ':' << pl.wood << ':' << pl.food
          << ':' << pl.supply << ':' << pl.supplyMax << ':' << pl.alive
          << ':' << pl.research << ':' << pl.aiWaveCd;
    }
    for (const auto& e : g.entities) {
        s << "|e" << e.id << ':' << (int)e.type << ':' << e.owner << ':' << e.x << ':' << e.y
          << ':' << e.hp << ':' << (int)e.state << ':' << e.targetId << ':' << e.targetX
          << ':' << e.targetY << ':' << e.pathIdx << ':' << e.moveCd << ':' << e.atkCd
          << ':' << e.gatherCd << ':' << (int)e.cargo.type << ':' << e.cargo.amount
          << ':' << e.cargo.sourceX << ':' << e.cargo.sourceY << ':' << (int)e.producing
          << ':' << e.trainProgress << ':' << e.trainTime
          << ':' << e.researchProgress << ':' << e.researchTime << ':' << e.underConstruction
          << ':' << e.alive << ':' << e.rallyX << ':' << e.rallyY << ':' << e.resourceX
          << ':' << e.resourceY << ':' << e.storedFood
          << ':' << e.stuckTicks << ':' << e.alertTicks << ':' << e.rallySet
          << ':' << e.researching << ':' << e.attackMove << ':' << e.holdPosition
          << ':' << e.gateOpen << ':' << e.gateLocked << ":q";
        for (int q : e.queue) s << ',' << q;
        s << ":g";
        for (int gid : e.garrison) s << ',' << gid;
    }
    for (const auto& p : g.projectiles) {
        s << "|pr" << p.x << ':' << p.y << ':' << p.tx << ':' << p.ty << ':'
          << p.glyph << ':' << p.color << ':' << p.life << ':' << p.alive;
    }
    return s.str();
}

static void assertIdsCoherent() {
    int maxId = 0;
    std::set<int> seen;
    for (const auto& e : g.entities) {
        assert(e.id > 0);
        assert(e.id < g.nextId);
        assert(seen.insert(e.id).second);
        if (e.id > maxId) maxId = e.id;
    }
    assert(maxId < g.nextId);
}

static int countTypeOwner(EntityType t, int owner) {
    int n = 0;
    for (const auto& e : g.entities)
        if (e.alive && e.type == t && e.owner == owner) n++;
    return n;
}

static int reachableCountFrom(int sx, int sy, bool wantWood, bool wantFood, bool wantGold) {
    bool seen[MAP_H][MAP_W] = {};
    std::queue<std::pair<int,int>> q;
    if (!inBounds(sx, sy) || !isPassable(sx, sy)) return 0;
    seen[sy][sx] = true;
    q.push({sx, sy});
    int found = 0;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        Terrain t = g.map[y][x].terrain;
        if (g.map[y][x].resources > 0) {
            bool wood = t == T_FOREST || t == T_PINE || t == T_PALM || t == T_DEAD_TREE;
            bool food = t == T_BERRY || t == T_FISH;
            bool gold = t == T_GOLD;
            if ((wantWood && wood) || (wantFood && food) || (wantGold && gold)) found++;
        }
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!inBounds(nx, ny) || seen[ny][nx] || !isPassable(nx, ny)) continue;
            seen[ny][nx] = true;
            q.push({nx, ny});
        }
    }
    return found;
}

static int passableAreaFrom(int sx, int sy) {
    bool seen[MAP_H][MAP_W] = {};
    std::queue<std::pair<int,int>> q;
    if (!inBounds(sx, sy) || !isPassable(sx, sy)) return 0;
    seen[sy][sx] = true;
    q.push({sx, sy});
    int total = 0;
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        total++;
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!inBounds(nx, ny) || seen[ny][nx] || !isPassable(nx, ny)) continue;
            seen[ny][nx] = true;
            q.push({nx, ny});
        }
    }
    return total;
}

static void testPlacementBoundsAndStateNames() {
    initGameWithSeed(1, 1001u, 0);
    assert(!canPlace(E_FARM, -1, 0, 0));
    assert(!canPlace(E_FARM, 0, -1, 0));
    assert(!canPlace(E_HOUSE, MAP_W, 4, 0));
    assert(!canPlace(E_HOUSE, 4, MAP_H, 0));
    g.map[12][12].terrain = T_FOREST;
    g.map[12][12].resources = 100;
    assert(isPassable(12, 12));
    assert(!canPlace(E_HOUSE, 12, 12, 0));
    for (int i = S_IDLE; i <= S_GARRISONED; i++)
        assert(std::string(stateName((EntityState)i)) != "Unknown");
    assert(std::string(stateName((EntityState)999)) == "Unknown");
    assert(validateGameState(nullptr));
    int oldCursorX = g.cursorX;
    g.cursorX = -1;
    std::string err;
    assert(!validateGameState(&err));
    assert(!err.empty());
    g.cursorX = oldCursorX;

    Entity* th = nullptr;
    Entity* peasant = nullptr;
    for (auto& e : g.entities) {
        if (e.alive && e.owner == 0 && e.type == E_TOWNHALL) th = &e;
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) peasant = &e;
    }
    assert(th && peasant);
    OccupancyGrid occ{};
    buildOccupancyGrid(occ, true, true);
    assert(isOccupied(occ, th->x, th->y));
    assert(isOccupied(occ, peasant->x, peasant->y));
    peasant->state = S_GARRISONED;
    buildOccupancyGrid(occ, true, true);
    assert(!isOccupied(occ, peasant->x, peasant->y));
    peasant->state = S_IDLE;
}

static void testTraits() {
    assert(isWorker(E_PEASANT));
    assert(canGather(E_PEASANT));
    assert(canBuild(E_PEASANT));
    assert(isMilitary(E_MILITIA));
    assert(isMilitary(E_ARCHER));
    assert(isRanged(E_ARCHER));
    assert(isSiege(E_CATAPULT));
    assert(isNaval(E_WARSHIP));
    assert(isHostileWildlife(E_WOLF));
    assert(isHostileWildlife(E_BOAR));
    assert(!isHostileWildlife(E_SHEEP));
    assert(isDropoff(E_TOWNHALL));
    assert(trainsUnits(E_BARRACKS));
}

static void testCommandBindings() {
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    assert(n >= 10);
    bool sawTrain = false, sawResign = false, sawExit = false, sawSave = false;
    bool sawHelp = false, sawZoom = false, sawProjection = false;
    for (int i = 0; i < n; i++) {
        assert(commands[i].id && *commands[i].id);
        assert(commands[i].keys && *commands[i].keys);
        std::string id = commands[i].id;
        sawTrain = sawTrain || id == "train";
        sawResign = sawResign || id == "resign";
        sawExit = sawExit || id == "exit";
        sawSave = sawSave || id == "save";
        sawHelp = sawHelp || id == "help";
        sawZoom = sawZoom || id == "zoom";
        sawProjection = sawProjection || id == "projection";
    }
    assert(sawTrain && sawResign && sawExit && sawSave);
    assert(sawHelp && sawZoom && sawProjection);
    std::string help = commandHelpLine();
    assert(help.find("T:Train") != std::string::npos);
    assert(help.find("Q:Resign") != std::string::npos);
    assert(help.find("?:Help") != std::string::npos);
}

static void testRecoverableValidation() {
    initGameWithSeed(1, 1501u, 0);
    g.selectedId = g.nextId + 99;
    g.selectedIds.push_back(g.nextId + 100);
    g.controlGroups[0].push_back(g.nextId + 101);
    g.cursorX = -5;
    tickSimulationOnce();
    assert(g.selectedId == -1);
    assert(g.selectedIds.empty());
    assert(g.controlGroups[0].empty());
    assert(inBounds(g.cursorX, g.cursorY));
    assert(validateGameState(nullptr));
}

static void testMatchResetAndDeterminism() {
    initGameWithSeed(2, 2002u, 1);
    std::string first = startupSummary();
    spawnProjectile(5, 5, 10, 10, '-', CP_PROJ_ARROW);
    spawnEntity(E_MILITIA, 0, 20, 20);
    assert(!g.projectiles.empty());
    initGameWithSeed(2, 2002u, 1);
    std::string second = startupSummary();
    assert(first == second);
    assert(g.projectiles.empty());
    assert(countTypeOwner(E_MILITIA, 0) == 0);
    assertIdsCoherent();
}

static void testSupplyAndTownHallCost() {
    initGameWithSeed(1, 3003u, 0);
    Entity* th = nullptr;
    for (auto& e : g.entities)
        if (e.alive && e.owner == 0 && e.type == E_TOWNHALL) { th = &e; break; }
    assert(th);
    int goldBefore = g.players[0].gold;
    orderTrain(*th, E_PEASANT);
    assert(g.players[0].gold == goldBefore - STATS[E_PEASANT].costGold);
    for (int i = 0; i < 30; i++) orderTrain(*th, E_PEASANT);
    assert(g.players[0].supply + (int)th->queue.size() + (th->producing != E_NONE ? 1 : 0) <= g.players[0].supplyMax);

    Entity* peasant = nullptr;
    for (auto& e : g.entities)
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) { peasant = &e; break; }
    assert(peasant);
    g.players[0].gold = STATS[E_TOWNHALL].costGold;
    g.players[0].wood = STATS[E_TOWNHALL].costWood;
    for (int y = 20; y < MAP_H - 5; y++) {
        for (int x = 20; x < MAP_W - 5; x++) {
            if (!canPlace(E_TOWNHALL, x, y, 0)) continue;
            orderBuild(*peasant, E_TOWNHALL, x, y);
            assert(g.players[0].gold == 0);
            assert(g.players[0].wood == 0);
            return;
        }
    }
    assert(false && "no town hall build spot found");
}

static void testBerryGatherAndDepletion() {
    initGameWithSeed(1, 3503u, 0);
    Entity* peasant = nullptr;
    for (auto& e : g.entities)
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) { peasant = &e; break; }
    assert(peasant);
    int bx = peasant->x + 1, by = peasant->y;
    assert(inBounds(bx, by));
    g.map[by][bx].terrain = T_BERRY;
    g.map[by][bx].resources = GATHER_RATE;
    g.map[by][bx].preWinterTerrain = T_BERRY;
    int foodBefore = g.players[0].food;
    int peasantId = peasant->id;
    orderGather(*peasant, bx, by);
    assert(peasant->cargo.type == CR_FOOD);
    assert(peasant->resourceX == bx && peasant->resourceY == by);
    bool depleted = false, delivered = false;
    for (int i = 0; i < 300; i++) {
        tickSimulationOnce();
        if (g.map[by][bx].terrain == T_GRASS && g.map[by][bx].resources == 0) depleted = true;
        if (g.players[0].food > foodBefore) delivered = true;
        if (depleted && delivered) break;
    }
    assert(depleted);
    assert(delivered);
    peasant = findEntity(peasantId);
    assert(peasant && peasant->cargo.amount == 0);
}

static void testMapgenReachabilityAcrossSeeds() {
    for (unsigned seed = 70; seed < 100; seed++) {
        initGameWithSeed(3, seed, (int)(seed % 4));
        int playerStarts = 0;
        std::set<std::pair<int,int>> basePositions;
        for (const auto& e : g.entities) {
            if (!e.alive || e.type != E_TOWNHALL || e.owner >= MAX_PLAYERS) continue;
            basePositions.insert({e.x, e.y});
            int sx = e.x + STATS[e.type].sizeW + 1;
            int sy = e.y + STATS[e.type].sizeH + 1;
            if (!isPassable(sx, sy)) { sx = e.x + STATS[e.type].sizeW + 2; sy = e.y + 1; }
            assert(reachableCountFrom(sx, sy, true, false, false) >= 1);
            assert(reachableCountFrom(sx, sy, false, true, false) >= 1);
            assert(reachableCountFrom(sx, sy, false, false, true) >= 1);
            assert(passableAreaFrom(sx, sy) > (MAP_W * MAP_H) / 3);
            playerStarts++;
        }
        assert(playerStarts == 4);
        assert(basePositions.size() == 4);
        for (const auto& e : g.entities) {
            if (!e.alive || e.owner == OWNER_NATURE) continue;
            for (const auto& b : basePositions) {
                if (e.type == E_TOWNHALL && e.x == b.first && e.y == b.second) continue;
                assert(!(e.x >= b.first && e.x < b.first + STATS[E_TOWNHALL].sizeW
                      && e.y >= b.second && e.y < b.second + STATS[E_TOWNHALL].sizeH));
            }
        }
    }
}

static void testStartSafetyAcrossSeeds() {
    const int starts[4][2] = {
        {5, 5}, {MAP_W - 9, 5}, {5, MAP_H - 9}, {MAP_W - 9, MAP_H - 9}
    };
    for (unsigned seed = 1; seed <= 60; seed++) {
        initGameWithSeed(3, seed, (int)(seed % 4));
        for (const auto& e : g.entities) {
            if (!e.alive) continue;
            for (auto& st : starts) {
                if (isHostileWildlife(e.type))
                    assert(dist(e.x, e.y, st[0] + 1, st[1] + 1) > 14);
            }
            if (e.owner > 0 && e.owner < MAX_PLAYERS) {
                bool nearAStart = false;
                for (auto& st : starts)
                    if (dist(e.x, e.y, st[0] + 1, st[1] + 1) <= 8) nearAStart = true;
                assert(nearAStart);
            }
        }
    }
}

static void testSaveLoadRoundTrip() {
    initGameWithSeed(2, 4004u, 2);
    for (int i = 0; i < 20; i++) tickSimulationOnce();
    std::string before = startupSummary();
    assert(saveGame("build/test-save.realm"));
    initGameWithSeed(1, 9999u, 0);
    assert(loadGame("build/test-save.realm"));
    assert(g.seed == 4004u);
    assert(startupSummary() == before);
    for (int i = 0; i < 20; i++) tickSimulationOnce();

    initGameWithSeed(2, 4444u, 1);
    for (int i = 0; i < 250; i++) tickSimulationOnce();
    assert(saveGame("build/exact-resume.realm"));
    for (int i = 0; i < 100; i++) tickSimulationOnce();
    std::string continuous = fullStateSummary();
    assert(loadGame("build/exact-resume.realm"));
    for (int i = 0; i < 100; i++) tickSimulationOnce();
    assert(fullStateSummary() == continuous);

    FILE* f = std::fopen("build/corrupt-save.realm", "wb");
    assert(f);
    std::fputs("not a realm save\n", f);
    std::fclose(f);
    assert(!loadGame("build/corrupt-save.realm"));
}

static void testLongSimulationAndAIProgression() {
    initGameWithSeed(1, 5005u, 0);
    int initialAiSupplyMax = g.players[1].supplyMax;
    int longTicks = 10000;
    if (const char* env = std::getenv("REALM_TEST_LONG_TICKS")) {
        int requested = std::atoi(env);
        if (requested > 0) longTicks = requested;
    }
    for (int i = 0; i < longTicks; i++) tickSimulationOnce();
    assert(!g.entities.empty());
    assert(g.players[1].supplyMax >= initialAiSupplyMax);
    assert(g.players[1].supplyMax > 10 || countTypeOwner(E_HOUSE, 1) > 0 || countTypeOwner(E_BARRACKS, 1) > 0);
    assertIdsCoherent();
}

int main() {
    testPlacementBoundsAndStateNames();
    testTraits();
    testCommandBindings();
    testRecoverableValidation();
    testMatchResetAndDeterminism();
    testSupplyAndTownHallCost();
    testBerryGatherAndDepletion();
    testStartSafetyAcrossSeeds();
    testMapgenReachabilityAcrossSeeds();
    testSaveLoadRoundTrip();
    testLongSimulationAndAIProgression();
    std::puts("realm_headless_tests: ok");
    return 0;
}
