#include "realm.h"
#include "entity_animation.h"
#include "core/research_service.h"
#include "core/market_service.h"

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
          << ':' << e.stuckTicks << ':' << e.alertTicks << ':' << e.deathTicks
          << ':' << e.carcassFoodRemaining << ':' << e.carcassFoodMax << ':' << e.rallySet
          << ':' << e.researching << ':' << e.attackMove << ':' << e.holdPosition
          << ':' << e.facingDx << ':' << e.facingDy
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
    Terrain invalidFoundations[] = {T_SHALLOWS, T_MARSH, T_REEDS, T_ICE};
    for (Terrain t : invalidFoundations) {
        g.map[12][12].terrain = t;
        g.map[12][12].resources = 0;
        assert(isPassable(12, 12));
        assert(!canPlace(E_HOUSE, 12, 12, 0));
    }
    for (int i = S_IDLE; i <= S_GARRISONED; i++)
        assert(std::string(stateName((EntityState)i)) != "Unknown");
    assert(std::string(stateName((EntityState)999)) == "Unknown");
    assert(validateGameState(nullptr));

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
    assert(isMilitary(E_SPEARMAN));
    assert(isMilitary(E_TREBUCHET));
    assert(isRanged(E_ARCHER));
    assert(isRanged(E_TREBUCHET));
    assert(isSiege(E_CATAPULT));
    assert(isSiege(E_TREBUCHET));
    assert(isNaval(E_WARSHIP));
    assert(isHostileWildlife(E_WOLF));
    assert(isHostileWildlife(E_BOAR));
    assert(!isHostileWildlife(E_SHEEP));
    assert(isDropoff(E_TOWNHALL));
    assert(trainsUnits(E_BARRACKS));
}

static void testVisualTileBridgeMappings() {
    Tile tile{};
    tile.biome = B_TEMPERATE;
    tile.resources = 70;

    tile.terrain = T_BERRY;
    VisualTileParts berry = visualPartsForTile(tile);
    assert(berry.ground == G_GRASS);
    assert(berry.feature == F_BERRY_BUSH);
    assert(berry.featureState == FS_FULL);
    assert((berry.featureTraits & FT_HARVESTABLE) != 0);

    tile.terrain = T_FOREST;
    tile.resources = 120;
    VisualTileParts forest = visualPartsForTile(tile);
    assert(forest.ground == G_GRASS);
    assert(forest.feature == F_FOREST);
    assert((forest.featureTraits & FT_CONCEALS_UNITS) != 0);
    assert((forest.featureTraits & FT_BLOCKS_MOVEMENT) == 0);
    assert(movementPenaltyForTile(tile) == 1);

    tile.terrain = T_HILLS;
    VisualTileParts hills = visualPartsForTile(tile);
    assert(hills.ground == G_HILLS);
    assert(hills.feature == F_NONE);

    tile.terrain = T_MOUNTAIN;
    VisualTileParts mountain = visualPartsForTile(tile);
    assert(mountain.ground == G_ROCKY);
    assert(mountain.feature == F_MOUNTAIN_PEAK);
    assert((mountain.featureTraits & FT_BLOCKS_MOVEMENT) != 0);

    tile.terrain = T_STONE;
    assert(visualPartsForTile(tile).feature == F_STONE_BOULDERS);

    tile.terrain = T_FISH;
    tile.biome = B_OCEAN;
    VisualTileParts fish = visualPartsForTile(tile);
    assert(fish.ground == G_WATER);
    assert(fish.feature == F_FISH_SHOAL);

    tile.biome = B_TEMPERATE;
    tile.terrain = T_RUINS;
    VisualTileParts ruins = visualPartsForTile(tile);
    assert(ruins.ground == G_GRAVEL);
    assert(ruins.feature == F_RUINS);

    tile.biome = B_SNOW;
    tile.terrain = T_SNOW;
    assert(visualPartsForTile(tile).ground == G_TUNDRA);

    tile.biome = B_TEMPERATE;
    tile.terrain = T_CASTLE_WALL;
    assert(visualPartsForTile(tile).feature == F_CASTLE_WALL);
    VisualTileParts gate = visualPartsForTerrain(T_CASTLE_GATE, B_TEMPERATE, 0, 0, true, false);
    assert(gate.ground == G_CASTLE_FLOOR);
    assert(gate.feature == F_CASTLE_GATE);
    assert(gate.featureState == FS_OPEN);

    tile.terrain = T_TALL_GRASS;
    VisualTileParts tallGrass = visualPartsForTile(tile);
    assert(tallGrass.feature == F_NONE);
    assert(!tallGrass.decals.empty());

    tile.terrain = T_DIRT;
    tile.wear = 90;
    VisualTileParts worn = visualPartsForTile(tile);
    bool sawCobble = false;
    for (VisualDecalType decal : worn.decals) sawCobble = sawCobble || decal == VD_COBBLE_PATCH;
    assert(sawCobble);
}

static void testVisualEntityStateSelectors() {
    initGameWithSeed(0, 2603u, 0);
    Entity b{};
    b.type = E_TOWNHALL;
    b.hp = 1;
    b.maxHp = 100;
    b.underConstruction = true;
    assert(buildingVisualState(b) == BVS_CONSTRUCTION_0_FOUNDATION);
    b.hp = 50;
    assert(buildingVisualState(b) == BVS_CONSTRUCTION_1_FRAME);
    b.hp = 90;
    assert(buildingVisualState(b) == BVS_CONSTRUCTION_2_NEARLY_COMPLETE);
    b.underConstruction = false;
    b.hp = 40;
    assert(buildingVisualState(b) == BVS_DAMAGED);
    b.hp = 100;
    b.producing = E_PEASANT;
    assert(buildingVisualState(b) == BVS_TRAINING_PEASANT);

    Entity smith{};
    smith.type = E_BLACKSMITH;
    smith.hp = smith.maxHp = 100;
    smith.researching = R_IRON_WEAPONS;
    assert(buildingVisualState(smith) == BVS_RESEARCHING_IRON_WEAPONS);
    smith.researching = R_CROSSBOWS;
    assert(buildingVisualState(smith) == BVS_RESEARCHING_CROSSBOWS);

    Entity transport{};
    transport.type = E_TRANSPORT;
    transport.alive = true;
    assert(transportVisualState(transport) == TVS_EMPTY);
    transport.garrison.push_back(1);
    assert(transportVisualState(transport) == TVS_LOADED_PARTIAL);
    transport.garrison = {1, 2, 3, 4};
    assert(transportVisualState(transport) == TVS_LOADED_FULL);
}

static void testAnimalCarcassHarvesting() {
    initGameWithSeed(0, 2703u, 0);
    int pid = spawnEntity(E_PEASANT, 0, 20, 20);
    int did = spawnEntity(E_DEER, OWNER_NATURE, 21, 20);
    Entity* peasant = findEntity(pid);
    Entity* deer = findEntity(did);
    assert(peasant && deer);
    deer->alive = false;
    deer->state = S_DEAD;
    deer->hp = 0;
    deer->deathTicks = 0;
    deer->carcassFoodMax = 120;
    deer->carcassFoodRemaining = 120;
    assert(isHarvestableCarcass(*deer));
    assert(animalCarcassVisualState(*deer) == ACVS_DEAD_UNHARVESTED);

    orderGather(*peasant, deer->x, deer->y);
    for (int i = 0; i < GATHER_TICKS; i++) tickEntity(*peasant);
    assert(peasant->cargo.type == CR_FOOD);
    assert(peasant->cargo.amount > 0);
    assert(deer->carcassFoodRemaining < deer->carcassFoodMax);
    deer->carcassFoodRemaining = 60;
    assert(animalCarcassVisualState(*deer) == ACVS_PARTLY_HARVESTED);
    deer->carcassFoodRemaining = 20;
    assert(animalCarcassVisualState(*deer) == ACVS_MOSTLY_HARVESTED);
    deer->carcassFoodRemaining = 0;
    assert(animalCarcassVisualState(*deer) == ACVS_DEPLETED_SKELETON);

    int wid = spawnEntity(E_WOLF, OWNER_NATURE, 24, 20);
    Entity* wolf = findEntity(wid);
    assert(wolf);
    wolf->alive = false;
    wolf->state = S_DEAD;
    wolf->hp = 0;
    wolf->carcassFoodMax = 0;
    wolf->carcassFoodRemaining = 0;
    assert(!isHarvestableCarcass(*wolf));
    assert(animalCarcassVisualState(*wolf) == ACVS_DEPLETED_SKELETON);
}

static void testCommandBindings() {
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    assert(n >= 10);
    bool sawTrain = false, sawResign = false, sawExit = false, sawSave = false;
    bool sawHelp = false, sawZoom = false;
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
    }
    assert(sawTrain && sawResign && sawExit && sawSave);
    assert(sawHelp && sawZoom);
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
    tickSimulationOnce();
    assert(g.selectedId == -1);
    assert(g.selectedIds.empty());
    assert(g.controlGroups[0].empty());
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

static void testTownHallTrainInputFlow() {
    initGameWithSeed(1, 3253u, 0);
    Entity* th = nullptr;
    for (auto& e : g.entities)
        if (e.alive && e.owner == 0 && e.type == E_TOWNHALL) { th = &e; break; }
    assert(th);
    g.selectedId = th->id;
    g.selectedIds.clear();
    g.mode = M_NORMAL;
    g.players[0].gold = 500;
    g.players[0].wood = 500;
    g.players[0].food = 500;
    g.players[0].supplyMax = 20;

    handleInput('t');
    assert(g.mode == M_TRAIN_SELECT);
    int goldBefore = g.players[0].gold;
    handleInput('p');
    assert(g.mode == M_TRAIN_SELECT);
    th = findEntity(g.selectedId);
    assert(th);
    assert(th->producing == E_PEASANT);
    assert(g.players[0].gold == goldBefore - STATS[E_PEASANT].costGold);
    handleInput(27);
    assert(g.mode == M_NORMAL);
}

static void testHoldPositionInput() {
    initGameWithSeed(1, 3277u, 0);
    int id = spawnEntity(E_MILITIA, 0, 30, 30);
    Entity* m = findEntity(id);
    assert(m);
    m->state = S_MOVING;
    m->holdPosition = 0;
    m->targetId = 7;
    g.selectedId = id;
    g.selectedIds.clear();
    g.mode = M_NORMAL;

    // 'X' must hold position during gameplay, not exit the application.
    handleInput('X');
    m = findEntity(id);
    assert(m);
    assert(m->state == S_IDLE);
    assert(m->holdPosition == 1);
    assert(m->targetId == -1);

    // Lowercase 'x' behaves identically.
    m->state = S_MOVING;
    m->holdPosition = 0;
    handleInput('x');
    m = findEntity(id);
    assert(m && m->state == S_IDLE && m->holdPosition == 1);
}


static void testWallLineBuild() {
    // Owner-aware, canonical-cost, validated wall-line placement.
    initGameWithSeed(1, 3377u, 0);
    // Carve a clear buildable strip so every segment is placeable.
    for (int x = 40; x <= 46; x++) {
        g.map[40][x].terrain = T_GRASS;
        g.map[40][x].resources = 0;
    }
    int pid = spawnEntity(E_PEASANT, 0, 39, 40);
    Entity* peasant = findEntity(pid);
    assert(peasant);

    int wallCost = STATS[E_WALL].costWood;
    assert(wallCost > 0);
    int segments = 5; // x = 41..45 inclusive (avoid the peasant's own tile at 39/40)
    g.players[0].wood = wallCost * segments;
    g.players[0].gold = 500;
    int woodBefore = g.players[0].wood;

    orderBuildLine(*peasant, E_WALL, 41, 40, 45, 40);
    int wallsBuilt = countTypeOwner(E_WALL, 0);
    assert(wallsBuilt == segments);
    assert(g.players[0].wood == woodBefore - wallCost * segments);
    assert(g.players[0].wood == 0);
    // All walls belong to the builder's owner, not a hardcoded player 0 path.
    for (auto& e : g.entities)
        if (e.alive && e.type == E_WALL) assert(e.owner == peasant->owner);

    // Unaffordable line is rejected entirely (pre-calculated cost).
    initGameWithSeed(1, 3378u, 0);
    for (int x = 40; x <= 46; x++) { g.map[40][x].terrain = T_GRASS; g.map[40][x].resources = 0; }
    int pid2 = spawnEntity(E_PEASANT, 0, 39, 40);
    Entity* peasant2 = findEntity(pid2);
    assert(peasant2);
    g.players[0].wood = STATS[E_WALL].costWood; // only enough for 1 of 5
    int woodBefore2 = g.players[0].wood;
    orderBuildLine(*peasant2, E_WALL, 41, 40, 45, 40);
    assert(countTypeOwner(E_WALL, 0) == 0);
    assert(g.players[0].wood == woodBefore2);

    // Non-human builder spends that player's resources, owner-aware.
    initGameWithSeed(1, 3379u, 0);
    for (int x = 40; x <= 46; x++) { g.map[40][x].terrain = T_GRASS; g.map[40][x].resources = 0; }
    int aid = spawnEntity(E_PEASANT, 1, 39, 40);
    Entity* aiPeasant = findEntity(aid);
    assert(aiPeasant);
    g.players[1].wood = STATS[E_WALL].costWood * 3;
    g.players[0].wood = 999;
    int p0WoodBefore = g.players[0].wood;
    orderBuildLine(*aiPeasant, E_WALL, 41, 40, 43, 40);
    assert(countTypeOwner(E_WALL, 1) == 3);
    assert(g.players[1].wood == 0);
    assert(g.players[0].wood == p0WoodBefore); // human resources untouched
    assert(validateGameState(nullptr));
}

static void testResearchService() {
    // Shared research service: same canonical costs/durations for human and AI,
    // validation rules enforced centrally.
    initGameWithSeed(1, 4501u, 0);
    int sid = spawnEntity(E_BLACKSMITH, 0, 30, 30); // built=true
    Entity* smith = findEntity(sid);
    assert(smith && !smith->underConstruction);

    const ResearchDef* iron = researchDef(ResearchId::IronWeapons);
    assert(iron && iron->bit == R_IRON_WEAPONS);

    // Human pays the canonical cost and gets the canonical duration.
    g.players[0].gold = iron->costGold;
    g.players[0].wood = iron->costWood;
    bool started = startResearch(g, 0, smith->id, ResearchId::IronWeapons);
    assert(started);
    assert(g.players[0].gold == 0 && g.players[0].wood == 0);
    assert(smith->researching == R_IRON_WEAPONS);
    assert(smith->researchTime == iron->ticks);

    // Can't start a second research while one is in progress.
    g.players[0].gold = 999; g.players[0].wood = 999;
    assert(!startResearch(g, 0, smith->id, ResearchId::Crossbows));

    // Complete the research; bit gets set and the smith frees up.
    for (int i = 0; i < iron->ticks + 50; i++) {
        tickSimulationOnce();
        if (g.players[0].research & R_IRON_WEAPONS) break;
    }
    assert(g.players[0].research & R_IRON_WEAPONS);

    // Already-researched is rejected even with resources.
    g.players[0].gold = 999; g.players[0].wood = 999;
    smith = findEntity(sid);
    assert(smith && smith->researching == 0);
    CanResearchResult again = canResearch(g, 0, *smith, ResearchId::IronWeapons);
    assert(!again.ok);
    assert(!startResearch(g, 0, smith->id, ResearchId::IronWeapons));

    // Counterweight requires an owned, completed Castle.
    const ResearchDef* cw = researchDef(ResearchId::Counterweight);
    assert(cw && cw->requiredOwnedBuilding == E_CASTLE);
    g.players[0].gold = cw->costGold; g.players[0].wood = cw->costWood;
    assert(!startResearch(g, 0, smith->id, ResearchId::Counterweight));
    int castleId = spawnEntity(E_CASTLE, 0, 50, 50); // built=true
    (void)castleId;
    g.players[0].gold = cw->costGold; g.players[0].wood = cw->costWood;
    smith = findEntity(sid);
    assert(startResearch(g, 0, smith->id, ResearchId::Counterweight));
    assert(smith->researching == R_COUNTERWEIGHT);

    // AI (owner 1) pays resources through the same service.
    initGameWithSeed(1, 4502u, 0);
    int asid = spawnEntity(E_BLACKSMITH, 1, 30, 30);
    Entity* aiSmith = findEntity(asid);
    assert(aiSmith && !aiSmith->underConstruction);
    g.players[1].gold = iron->costGold; g.players[1].wood = iron->costWood;
    int p0Gold = g.players[0].gold, p0Wood = g.players[0].wood;
    bool aiStarted = startResearch(g, 1, aiSmith->id, ResearchId::IronWeapons);
    assert(aiStarted);
    assert(g.players[1].gold == 0 && g.players[1].wood == 0);
    assert(aiSmith->researchTime == iron->ticks); // same duration as human
    assert(g.players[0].gold == p0Gold && g.players[0].wood == p0Wood); // human untouched

    // Not enough resources is rejected.
    initGameWithSeed(1, 4503u, 0);
    int psid = spawnEntity(E_BLACKSMITH, 0, 30, 30);
    Entity* poorSmith = findEntity(psid);
    assert(poorSmith);
    g.players[0].gold = 0; g.players[0].wood = 0;
    assert(!startResearch(g, 0, poorSmith->id, ResearchId::IronWeapons));
    assert(poorSmith->researching == 0);
    assert(validateGameState(nullptr));
}

static void testUnitFoodCostTable() {
    // Food costs are now sourced from STATS[].costFood; guard the canonical values
    // that previously lived in an orderTrain switch so behavior can't silently drift.
    assert(STATS[E_MILITIA].costFood == 20);
    assert(STATS[E_ARCHER].costFood == 20);
    assert(STATS[E_SPEARMAN].costFood == 20);
    assert(STATS[E_KNIGHT].costFood == 40);
    assert(STATS[E_CATAPULT].costFood == 30);
    assert(STATS[E_TREBUCHET].costFood == 30);
    assert(STATS[E_WARSHIP].costFood == 20);
    assert(STATS[E_TRANSPORT].costFood == 10);
    assert(STATS[E_PEASANT].costFood == 0);
    assert(STATS[E_RAM].costFood == 0);

    // Training deducts food via the table.
    initGameWithSeed(1, 4601u, 0);
    int bid = spawnEntity(E_BARRACKS, 0, 30, 30);
    Entity* bar = findEntity(bid);
    assert(bar && !bar->underConstruction);
    g.players[0].gold = 999; g.players[0].wood = 999;
    g.players[0].food = 100;
    g.players[0].supplyMax = 50;
    int foodBefore = g.players[0].food;
    orderTrain(*bar, E_MILITIA);
    assert(bar->producing == E_MILITIA);
    assert(g.players[0].food == foodBefore - STATS[E_MILITIA].costFood);
    assert(validateGameState(nullptr));
}

static void testMarketTradeService() {
    // Shared market service: rates come from one table; validation centralized.
    initGameWithSeed(1, 4701u, 0);
    int mid = spawnEntity(E_MARKET, 0, 30, 30);
    Entity* market = findEntity(mid);
    assert(market && !market->underConstruction);

    Player& p = g.players[0];
    p.gold = 100; p.wood = 0; p.food = 0;

    // Gold -> wood at the canonical 40:30 rate.
    assert(executeTrade(g, 0, market->id, MarketTradeType::GoldForWood));
    assert(p.gold == 60 && p.wood == 30);

    // Wood -> gold.
    p.wood = 40; p.gold = 0;
    assert(executeTrade(g, 0, market->id, MarketTradeType::WoodForGold));
    assert(p.wood == 0 && p.gold == 30);

    // Gold -> food.
    p.gold = 50; p.food = 0;
    assert(executeTrade(g, 0, market->id, MarketTradeType::GoldForFood));
    assert(p.gold == 0 && p.food == 30);

    // Food -> gold.
    p.food = 40; p.gold = 0;
    assert(executeTrade(g, 0, market->id, MarketTradeType::FoodForGold));
    assert(p.food == 0 && p.gold == 30);

    // Insufficient resources rejected, nothing mutated.
    p.gold = 10; p.wood = 5;
    assert(!executeTrade(g, 0, market->id, MarketTradeType::GoldForWood));
    assert(p.gold == 10 && p.wood == 5);

    // Trade against another player's market is rejected.
    initGameWithSeed(1, 4702u, 0);
    int amid = spawnEntity(E_MARKET, 1, 30, 30);
    Entity* aiMarket = findEntity(amid);
    assert(aiMarket);
    g.players[0].gold = 100;
    int p0GoldBefore = g.players[0].gold;
    CanTradeResult notMine = canTrade(g, 0, *aiMarket, MarketTradeType::GoldForWood);
    assert(!notMine.ok);
    assert(!executeTrade(g, 0, aiMarket->id, MarketTradeType::GoldForWood));
    assert(g.players[0].gold == p0GoldBefore);

    // Trade against an under-construction market is rejected.
    initGameWithSeed(1, 4703u, 0);
    int umid = spawnEntity(E_MARKET, 0, 30, 30, false);
    Entity* uMarket = findEntity(umid);
    assert(uMarket && uMarket->underConstruction);
    g.players[0].gold = 100;
    assert(!executeTrade(g, 0, uMarket->id, MarketTradeType::GoldForWood));
    assert(g.players[0].gold == 100);
    assert(validateGameState(nullptr));
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

static void testEntityAnimationSpecs() {
    assert(entityActionAnimationSpecCount(E_PEASANT) == 16);
    const EntityActionAnimationSpec* mine = findEntityActionAnimationSpec(E_PEASANT, "mine_gold");
    assert(mine);
    assert(std::string(mine->description).find("gold") != std::string::npos);
    assert(mine->targetRelation == ActionTargetRelation::AdjacentTarget);
    assert(mine->rangeTiles == 1);
    assert(mine->frameCount == 2);
    assert(std::string(mine->frames[0].description).find("low") != std::string::npos);
    assert(std::string(mine->frames[1].description).find("raised") != std::string::npos);

    Entity e{};
    e.type = E_PEASANT;
    e.alive = true;
    e.state = S_IDLE;
    e.targetId = -1;
    e.targetX = -1;
    e.targetY = -1;
    assert(std::string(entityAnimationActionId(e)) == "idle");
    assert(entityActionAnimationSpecFor(e)->holdLast);
    assert(entityActionAnimationSpecCount(E_MILITIA) == 1);
    const EntityActionAnimationSpec* militiaDeath = findEntityActionAnimationSpec(E_MILITIA, "death");
    assert(militiaDeath);
    assert(militiaDeath->frameCount == 2);
    assert(std::string(militiaDeath->frames[0].id) == "dead");
    assert(std::string(militiaDeath->frames[1].id) == "decayed");
    assert(std::string(militiaDeath->frames[1].description).find("armour") != std::string::npos);
    assert(std::string(militiaDeath->frames[1].description).find("weapons") != std::string::npos);
    assert(entityTypeForAnimationSlug("militia") == E_MILITIA);
    assert(entityTypeForAnimationSlug("boar") == E_BOAR);
    e.x = 10;
    e.y = 10;
    e.facingDx = 1;
    e.facingDy = 0;
    assert(std::string(entityAnimationDirectionBucket(e)) == "front");
    assert(!entityAnimationMirrorHorizontal(e));
    e.facingDx = 0;
    e.facingDy = 1;
    assert(std::string(entityAnimationDirectionBucket(e)) == "front");
    assert(entityAnimationMirrorHorizontal(e));
    e.facingDx = 0;
    e.facingDy = -1;
    assert(std::string(entityAnimationDirectionBucket(e)) == "back");
    assert(!entityAnimationMirrorHorizontal(e));
    e.facingDx = -1;
    e.facingDy = 0;
    assert(std::string(entityAnimationDirectionBucket(e)) == "back");
    assert(entityAnimationMirrorHorizontal(e));

    e.state = S_MOVING;
    e.path = {{11, 10}};
    e.pathIdx = 0;
    assert(std::string(entityAnimationActionId(e)) == "walk");
    assert(std::string(entityAnimationDirectionBucket(e)) == "front");
    assert(!entityAnimationMirrorHorizontal(e));

    e.state = S_RETURNING;
    e.path.clear();
    e.cargo = {CR_WOOD, 10, 3, 4};
    assert(std::string(entityAnimationActionId(e)) == "carry_wood");

    e.alive = false;
    assert(std::string(entityAnimationActionId(e)) == "death");

    Entity m{};
    m.type = E_MILITIA;
    m.alive = false;
    m.state = S_DEAD;
    assert(std::string(entityAnimationActionId(m)) == "death");
    assert(entityActionAnimationSpecFor(m) == militiaDeath);
}

static void testCorpseDecayLifecycle() {
    initGameWithSeed(0, 3903u, 0);
    int id = spawnEntity(E_MILITIA, 0, 20, 20);
    Entity* militia = findEntity(id);
    assert(militia);
    militia->alive = false;
    militia->state = S_DEAD;
    militia->hp = 0;
    militia->deathTicks = 0;

    assert(findEntity(id) == nullptr);
    assert(entityAt(20, 20) == nullptr);
    assert(corpseAt(20, 20) == militia);

    for (int i = 0; i < DEATH_DECAY_TICKS; i++) tickSimulationOnce();
    Entity* decayed = corpseAt(20, 20);
    assert(decayed && decayed->deathTicks >= DEATH_DECAY_TICKS);

    for (int i = DEATH_DECAY_TICKS; i < CORPSE_REMOVE_TICKS; i++) tickSimulationOnce();
    assert(corpseAt(20, 20) == nullptr);
}

static void testMillFoodStockpile() {
    initGameWithSeed(1, 3603u, 0);
    Entity* mill = nullptr;
    for (int y = 20; y < MAP_H - 5 && !mill; y++) {
        for (int x = 20; x < MAP_W - 5 && !mill; x++) {
            if (!canPlace(E_MILL, x, y, 0)) continue;
            int id = spawnEntity(E_MILL, 0, x, y);
            mill = findEntity(id);
        }
    }
    assert(mill);
    int foodBefore = g.players[0].food;
    addPlayerFood(0, 40, mill);
    assert(g.players[0].food == foodBefore + 40);
    assert(mill->storedFood == 40);
    spendPlayerFood(0, 15);
    assert(g.players[0].food == foodBefore + 25);
    assert(mill->storedFood == 25);
}

static void testWinterPartialWaterFreeze() {
    initGameWithSeed(1, 3653u, 0);
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            g.map[y][x].terrain = T_WATER;
            g.map[y][x].preWinterTerrain = T_WATER;
        }
    }
    g.prevSeason = AUTUMN;
    g.seasonPhase = (float)WINTER;
    tickSeasons();
    int ice = 0, openWater = 0;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (g.map[y][x].terrain == T_ICE) ice++;
            if (g.map[y][x].terrain == T_WATER) openWater++;
        }
    }
    assert(ice > 0);
    assert(openWater > 0);
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
    for (unsigned seed = 1; seed <= 60; seed++) {
        initGameWithSeed(3, seed, (int)(seed % 4));
        std::map<int, std::pair<int,int>> bases;
        for (const auto& e : g.entities) {
            if (e.alive && e.type == E_TOWNHALL && e.owner >= 0 && e.owner < MAX_PLAYERS)
                bases[e.owner] = {e.x, e.y};
        }
        assert(bases.size() == 4);
        for (const auto& e : g.entities) {
            if (!e.alive) continue;
            for (auto& kv : bases) {
                auto& st = kv.second;
                if (e.type == E_DEER)
                    assert(!(std::abs(e.x - st.first) <= 10 && std::abs(e.y - st.second) <= 10));
                if (isHostileWildlife(e.type))
                    assert(!(std::abs(e.x - st.first) <= 16 && std::abs(e.y - st.second) <= 16));
            }
            if (e.owner > 0 && e.owner < MAX_PLAYERS) {
                auto it = bases.find(e.owner);
                assert(it != bases.end());
                assert(dist(e.x, e.y, it->second.first + 1, it->second.second + 1) <= 8);
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
    testEntityAnimationSpecs();
    testCorpseDecayLifecycle();
    testVisualTileBridgeMappings();
    testVisualEntityStateSelectors();
    testAnimalCarcassHarvesting();
    testPlacementBoundsAndStateNames();
    testTraits();
    testCommandBindings();
    testRecoverableValidation();
    testMatchResetAndDeterminism();
    testSupplyAndTownHallCost();
    testTownHallTrainInputFlow();
    testHoldPositionInput();
    testWallLineBuild();
    testResearchService();
    testUnitFoodCostTable();
    testMarketTradeService();
    testBerryGatherAndDepletion();
    testMillFoodStockpile();
    testWinterPartialWaterFreeze();
    testStartSafetyAcrossSeeds();
    testMapgenReachabilityAcrossSeeds();
    testSaveLoadRoundTrip();
    testLongSimulationAndAIProgression();
    std::puts("realm_headless_tests: ok");
    return 0;
}
