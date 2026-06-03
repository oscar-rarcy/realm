#include "realm.h"
#include "entity_animation.h"
#include "ai/ai.h"
#include "commands/command.h"
#include "commands/input_intent.h"
#include "commands/input_mode_controller.h"
#include "input_keys.h"
#include "view_state.h"
#include "core/build_service.h"
#include "core/game_context.h"
#include "core/game_events.h"
#include "core/order_service.h"
#include "core/production_service.h"
#include "core/research_service.h"
#include "core/market_service.h"
#include "core/world_index.h"
#include "render/render_model.h"
#include "sim/save_migration.h"
#include "sim/save_reader.h"
#include "sim/save_service.h"

#include <cassert>
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <queue>
#include <set>
#include <map>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

void handleMobileHudButton(const std::string& id, const WorldIndex& world);

class TestEventSink : public EventSink {
public:
    void emit(const GameEvent& event) override { events.push_back(event); }

    bool has(GameEventType type) const {
        return std::any_of(events.begin(), events.end(), [type](const GameEvent& event) {
            return event.type == type;
        });
    }

    int count(GameEventType type) const {
        return (int)std::count_if(events.begin(), events.end(), [type](const GameEvent& event) {
            return event.type == type;
        });
    }

    void clear() { events.clear(); }

    std::vector<GameEvent> events;
};

static CommandResult dispatchTestCommand(const Command& command) {
    WorldIndex world = buildWorldIndex(g);
    GameContext context{ g, world, gameEvents() };
    return dispatchCommand(context, command);
}

static Entity* testFindEntity(int id) {
    WorldIndex world = buildWorldIndex(g);
    return findEntity(g, world, id);
}

static Entity* testEntityAt(int x, int y) {
    WorldIndex world = buildWorldIndex(g);
    return entityAt(g, world, x, y);
}

static Entity* testEntityAtOwner(int x, int y, int owner) {
    WorldIndex world = buildWorldIndex(g);
    return entityAtOwner(g, world, x, y, owner);
}

static Entity* testCorpseAt(int x, int y) {
    WorldIndex world = buildWorldIndex(g);
    return corpseAt(g, world, x, y);
}

static bool testCanPlace(EntityType type, int x, int y, int owner) {
    WorldIndex world = buildWorldIndex(g);
    return canPlace(g, world, type, x, y, owner);
}

static void runTestAIGather(PlayerId owner) {
    const AITuning& tuning = defaultAITuning();
    WorldIndex world = buildWorldIndex(g);
    GameContext gameContext{ g, world, gameEvents() };
    AIWorldView view = buildAIWorldView(g, world, owner, tuning);
    AIContext aiContext{ owner, gameContext, view, tuning, {}, {} };
    aiGather(aiContext);
    executeAICommands(aiContext);
}

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
          << ':' << e.gateOpen << ':' << e.gateLocked << ':' << e.patrolMode << ":q";
        for (int q : e.queue) s << ',' << q;
        s << ":g";
        for (int gid : e.garrison) s << ',' << gid;
        s << ":w";
        for (auto waypoint : e.waypoints) s << ',' << waypoint.first << ':' << waypoint.second;
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
    if (!inBounds(sx, sy) || !isPassable(g, sx, sy)) return 0;
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
            if (!inBounds(nx, ny) || seen[ny][nx] || !isPassable(g, nx, ny)) continue;
            seen[ny][nx] = true;
            q.push({nx, ny});
        }
    }
    return found;
}

static int passableAreaFrom(int sx, int sy) {
    bool seen[MAP_H][MAP_W] = {};
    std::queue<std::pair<int,int>> q;
    if (!inBounds(sx, sy) || !isPassable(g, sx, sy)) return 0;
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
            if (!inBounds(nx, ny) || seen[ny][nx] || !isPassable(g, nx, ny)) continue;
            seen[ny][nx] = true;
            q.push({nx, ny});
        }
    }
    return total;
}

static void testLocalGameRng() {
    Game a{};
    Game b{};
    realmSrand(a, 1234u);
    realmSrand(b, 1234u);
    for (int i = 0; i < 8; i++) assert(realmRand(a) == realmRand(b));
    unsigned globalBefore = g.rngState;
    realmSrand(a, 0u);
    assert(a.rngState == 1u);
    assert(g.rngState == globalBefore);
}

static void testPlacementBoundsAndStateNames() {
    initGameWithSeed(1, 1001u, 0);
    assert(!testCanPlace(E_FARM, -1, 0, 0));
    assert(!testCanPlace(E_FARM, 0, -1, 0));
    assert(!testCanPlace(E_HOUSE, MAP_W, 4, 0));
    assert(!testCanPlace(E_HOUSE, 4, MAP_H, 0));
    g.map[12][12].terrain = T_FOREST;
    g.map[12][12].resources = 100;
    assert(isPassable(g, 12, 12));
    assert(!testCanPlace(E_HOUSE, 12, 12, 0));
    Terrain invalidFoundations[] = {T_SHALLOWS, T_MARSH, T_REEDS, T_ICE};
    for (Terrain t : invalidFoundations) {
        g.map[12][12].terrain = t;
        g.map[12][12].resources = 0;
        assert(isPassable(g, 12, 12));
        assert(!testCanPlace(E_HOUSE, 12, 12, 0));
    }
    for (int i = S_IDLE; i <= S_GARRISONED; i++)
        assert(std::string(stateName((EntityState)i)) != "Unknown");
    assert(std::string(stateName((EntityState)999)) == "Unknown");
    assert(validateGameState(g, nullptr));

    Entity* th = nullptr;
    Entity* peasant = nullptr;
    for (auto& e : g.entities) {
        if (e.alive && e.owner == 0 && e.type == E_TOWNHALL) th = &e;
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) peasant = &e;
    }
    assert(th && peasant);
    WorldIndex occupancyWorld = buildWorldIndex(g);
    assert(isOccupied(occupancyWorld, { th->x, th->y }, OccupancyLayer::Any));
    assert(isOccupied(occupancyWorld, { peasant->x, peasant->y }, OccupancyLayer::Any));
    peasant->state = S_GARRISONED;
    occupancyWorld = buildWorldIndex(g);
    assert(!isOccupied(occupancyWorld, { peasant->x, peasant->y }, OccupancyLayer::Any));
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

static void testWeatherSightAndForestMovementPenalties() {
    initGameWithSeed(0, 2751u, 0);
    for (int y = 40; y <= 60; y++) for (int x = 40; x <= 65; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }
    int peasantId = spawnEntity(g, E_PEASANT, 0, 50, 50);
    Entity* peasant = testFindEntity(peasantId);
    assert(peasant);
    g.dayPhase = 0.5f;
    g.seasonPhase = (float)SUMMER;
    g.weather = W_CLEAR;
    updateFog(g);
    assert(g.map[50][57].visible[0]);
    g.weather = W_RAIN;
    updateFog(g);
    assert(g.map[50][56].visible[0]);
    assert(!g.map[50][57].visible[0]);
    g.weather = W_STORM;
    updateFog(g);
    assert(g.map[50][55].visible[0]);
    assert(!g.map[50][56].visible[0]);

    int knightId = spawnEntity(g, E_KNIGHT, 0, 52, 50);
    Entity* knight = testFindEntity(knightId);
    assert(knight);
    g.weather = W_CLEAR;
    g.map[50][53].terrain = T_FOREST;
    WorldIndex world = buildWorldIndex(g);
    knight->path = {{53, 50}};
    knight->pathIdx = 0;
    knight->moveCd = 0;
    moveAlongPath(g, world, *knight);
    assert(knight->x == 53 && knight->moveCd == STATS[E_KNIGHT].speed + 2);
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

static void testRenderModelBuildsViewport() {
    initGameWithSeed(1, 1251u, 0);
    for (int y = 10; y < 15; y++) for (int x = 10; x < 15; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].visible[0] = true;
        g.map[y][x].explored[0] = true;
    }
    int houseId = spawnEntity(g, E_HOUSE, 0, 11, 11);
    int archerId = spawnEntity(g, E_ARCHER, 0, 12, 12);
    int transportId = spawnEntity(g, E_TRANSPORT, 0, 13, 13);
    int hiddenWolfId = spawnEntity(g, E_WOLF, OWNER_NATURE, 14, 14);
    Entity* house = testFindEntity(houseId);
    Entity* archer = testFindEntity(archerId);
    Entity* transport = testFindEntity(transportId);
    Entity* hiddenWolf = testFindEntity(hiddenWolfId);
    assert(house && archer && transport && hiddenWolf);
    house->hp = house->maxHp / 2;
    house->rallySet = 1;
    house->rallyX = 19;
    house->rallyY = 20;
    archer->state = S_ATTACKING;
    archer->targetId = hiddenWolfId;
    archer->targetX = 14;
    archer->targetY = 14;
    archer->facingDx = 1;
    archer->facingDy = 0;
    archer->alertTicks = 7;
    archer->attackMove = 1;
    archer->holdPosition = 1;
    transport->garrison = { archerId };
    hiddenWolf->state = S_DEAD;
    hiddenWolf->deathTicks = 1;
    g.map[14][14].visible[0] = false;
    g.map[14][14].explored[0] = true;
    g.local.selectedId = archerId;
    g.local.selectedIds = { archerId };
    g.mode = M_BUILD_PLACE;
    g.local.buildPending = E_TOWER;
    ui.actionMarkers.push_back({ 13, 13, 9, '!' });
    ui.actionMarkers.push_back({ 30, 30, 9, '?' });

    RenderModel model = buildRenderModel(g, ui.actionMarkers, 0, 10, 10, 5, 5);
    assert(model.viewX == 10 && model.viewY == 10);
    assert(model.viewW == 5 && model.viewH == 5);
    assert(model.mode == M_BUILD_PLACE);
    assert(model.buildPreviewType == E_TOWER);
    assert(model.tiles.size() == 25);
    assert(model.actionMarkers.size() == 1);
    assert(model.actionMarkers[0].x == 13 && model.actionMarkers[0].y == 13);
    assert(model.actionMarkers[0].glyph == '!');
    bool sawHouse = false, sawSelectedArcher = false, sawTransport = false, sawHiddenWolf = false;
    for (const EntityRenderInfo& entity : model.entities) {
        if (entity.id == houseId) {
            sawHouse = true;
            assert(entity.hp == house->hp && entity.maxHp == house->maxHp);
            assert(entity.buildingState == BVS_DAMAGED);
            assert(entity.rallySet && entity.rallyX == 19 && entity.rallyY == 20);
        }
        if (entity.id == archerId) {
            sawSelectedArcher = true;
            assert(entity.selected);
            assert(entity.state == S_ATTACKING);
            assert(entity.targetId == hiddenWolfId && entity.targetX == 14 && entity.targetY == 14);
            assert(entity.facingDx == 1 && entity.facingDy == 0);
            assert(entity.alertTicks == 7);
            assert(entity.attackMove && entity.holdPosition);
        }
        if (entity.id == transportId) {
            sawTransport = true;
            assert(entity.transportState == TVS_LOADED_PARTIAL);
        }
        if (entity.id == hiddenWolfId) {
            sawHiddenWolf = true;
            assert(!entity.visible);
            assert(entity.animalCarcassState != ACVS_ALIVE);
        }
    }
    assert(sawHouse && sawSelectedArcher && sawTransport && sawHiddenWolf);
}

static void testAnimalCarcassHarvesting() {
    initGameWithSeed(0, 2703u, 0);
    int pid = spawnEntity(g, E_PEASANT, 0, 20, 20);
    int did = spawnEntity(g, E_DEER, OWNER_NATURE, 21, 20);
    Entity* peasant = testFindEntity(pid);
    Entity* deer = testFindEntity(did);
    assert(peasant && deer);
    deer->alive = false;
    deer->state = S_DEAD;
    deer->hp = 0;
    deer->deathTicks = 0;
    deer->carcassFoodMax = 120;
    deer->carcassFoodRemaining = 120;
    assert(isHarvestableCarcass(*deer));
    assert(animalCarcassVisualState(*deer) == ACVS_DEAD_UNHARVESTED);

    WorldIndex carcassWorld = buildWorldIndex(g);
    assert(startGather(g, carcassWorld, gameEvents(), peasant->owner, Selection{ peasant->id, { peasant->id } }, { deer->x, deer->y }).ok);
    for (int i = 0; i < GATHER_TICKS; i++) {
        tickEntity(g, carcassWorld, gameEvents(), *peasant);
        carcassWorld = buildWorldIndex(g);
    }
    assert(peasant->cargo.type == CR_FOOD);
    assert(peasant->cargo.amount > 0);
    assert(deer->carcassFoodRemaining < deer->carcassFoodMax);
    deer->carcassFoodRemaining = 60;
    assert(animalCarcassVisualState(*deer) == ACVS_PARTLY_HARVESTED);
    deer->carcassFoodRemaining = 20;
    assert(animalCarcassVisualState(*deer) == ACVS_MOSTLY_HARVESTED);
    deer->carcassFoodRemaining = 0;
    assert(animalCarcassVisualState(*deer) == ACVS_DEPLETED_SKELETON);

    int wid = spawnEntity(g, E_WOLF, OWNER_NATURE, 24, 20);
    Entity* wolf = testFindEntity(wid);
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
    const CommandHelpBinding* commands = gameplayHelpBindings(n);
    assert(n >= 10);
    bool sawTrain = false, sawResign = false, sawSave = false;
    bool sawHelp = false, sawZoom = false;
    for (int i = 0; i < n; i++) {
        assert(commands[i].id && *commands[i].id);
        assert(commands[i].keys && *commands[i].keys);
        std::string id = commands[i].id;
        sawTrain = sawTrain || id == "train";
        sawResign = sawResign || id == "resign";
        sawSave = sawSave || id == "save";
        sawHelp = sawHelp || id == "help";
        sawZoom = sawZoom || id == "zoom";
    }
    assert(sawTrain && sawResign && sawSave);
    assert(sawHelp && sawZoom);
    std::string help = commandHelpLine();
    assert(help.find("T:Train") != std::string::npos);
    assert(help.find("Q:Resign") != std::string::npos);
    assert(help.find("?:Help") != std::string::npos);
    assert(help.find("X:Exit") == std::string::npos);
}

static void testInputIntentMapping() {
    assert(inputIntentFromKey('x').intent == InputIntent::HoldPosition);
    assert(inputIntentFromKey('X').intent == InputIntent::HoldPosition);
    assert(inputIntentFromKey('q').intent == InputIntent::Resign);
    assert(inputIntentFromKey(KEY_F(5)).intent == InputIntent::Save);
    assert(inputIntentFromKey(KEY_F(5)).slot == 1);
    assert(inputIntentFromKey(KEY_F(12)).intent == InputIntent::Load);
    assert(inputIntentFromKey(KEY_F(12)).slot == 4);
    assert(inputIntentFromKey('7').intent == InputIntent::ControlGroup);
    assert(inputIntentFromKey('7').slot == 6);
    assert(inputIntentFromKey(KEY_PPAGE).intent == InputIntent::CursorFastUp);
}

static void testViewStateHelpers() {
    ViewState state{};
    state.viewX = 5;
    state.viewY = 7;
    state.viewW = 20;
    state.viewH = 30;
    ViewportCell cell = viewportCellAt(state, 3, 10, 2);
    assert(cell.inMap && cell.x == 8 && cell.y == 15);
    cell = viewportCellAt(state, 3, 1, 2);
    assert(!cell.inMap);

    state.cursorX = -10;
    state.cursorY = MAP_H + 10;
    clampCursorToMap(state);
    assert(state.cursorX == 0 && state.cursorY == MAP_H - 1);

    const int screenWidth = 100;
    const int panelW = 24;
    const int mmX = screenWidth - panelW + 1;
    const int mmY = 1;
    const int mmW = panelW - 2;
    const int mmH = std::min(state.viewH / 3, 14);
    int mouseX = mmX + 11;
    int mouseY = mmY + 5;
    int expectedX = (mouseX - mmX) * MAP_W / mmW;
    int expectedY = (mouseY - mmY) * MAP_H / mmH;
    assert(handleMinimapClick(state, screenWidth, mouseX, mouseY, true));
    assert(state.cursorX == expectedX && state.cursorY == expectedY);
    assert(state.viewX == std::max(0, std::min(expectedX - state.viewW / 2, MAP_W - state.viewW)));
    assert(state.viewY == std::max(0, std::min(expectedY - state.viewH / 2, MAP_H - state.viewH)));
    assert(!handleMinimapClick(state, screenWidth, 0, 0, true));
}

static void testInputModeController() {
    Game game{};
    setInputMode(game, M_BUILD_SELECT);
    assert(game.mode == M_BUILD_SELECT);
    assert(!isInputBlockedByMode(game.mode));
    setInputMode(game, M_PAUSED);
    assert(isInputBlockedByMode(game.mode));
    cancelInputMode(game);
    assert(game.mode == M_NORMAL);
    startWallBuildMode(game);
    assert(game.mode == M_WALL_DRAG && game.local.buildPending == E_WALL);
    assert(toggleHelpOverlay(game));
    assert(game.local.helpOverlay);
    assert(!toggleHelpOverlay(game));
    assert(!game.local.helpOverlay);
    assert(!controlGroupAssignmentPending(game));

    initGameWithSeed(0, 1451u, 0);
    int peasantId = spawnEntity(g, E_PEASANT, 1, 28, 28);
    g.local.selectedId = peasantId;
    assert(!selectedPeasantCanBuild(g, 0));
    assert(selectedPeasantCanBuild(g, 1));
    std::optional<MapPos> selectedPos = selectedEntityPosition(g, 1);
    assert(selectedPos && selectedPos->x == 28 && selectedPos->y == 28);
    assert(!selectedEntityPosition(g, 0));

    int barracksId = spawnEntity(g, E_BARRACKS, 1, 30, 28);
    g.local.selectedId = barracksId;
    assert(selectedProducerCanTrain(g, 1));
    assert(selectedTrainProducerType(g, 1) && *selectedTrainProducerType(g, 1) == E_BARRACKS);
    assert(trainMenuEligibilityForSelected(g, 1) == InputTrainMenuEligibility::CanTrain);
    assert(utilityModeForSelectedBuilding(g, 1) == InputUtilityMode::Rally);

    int marketId = spawnEntity(g, E_MARKET, 1, 32, 28);
    g.local.selectedId = marketId;
    assert(selectedMarketCanTrade(g, 1));
    assert(trainMenuEligibilityForSelected(g, 1) == InputTrainMenuEligibility::UnsupportedBuilding);
    assert(utilityModeForSelectedBuilding(g, 1) == InputUtilityMode::MarketTrade);

    int blacksmithId = spawnEntity(g, E_BLACKSMITH, 1, 34, 28);
    g.local.selectedId = blacksmithId;
    assert(selectedBlacksmithCanResearch(g, 1));
    assert(utilityModeForSelectedBuilding(g, 1) == InputUtilityMode::Research);

    int trebId = spawnEntity(g, E_TREBUCHET, 1, 36, 28);
    g.local.selectedId = trebId;
    assert(selectedTrebuchetCanToggle(g, 1));
    assert(!selectedTrebuchetCanToggle(g, 0));
}

static void testInputFeedbackHelper() {
    gameEvents().emit({ GameEventType::StatusMessage, 0, -1, { -1, -1 }, "input feedback smoke", 0 });
    flushGameEventsToUi(ui, 0);
    assert(ui.statusMsg == "input feedback smoke");
}

static void testRecoverableValidation() {
    initGameWithSeed(1, 1501u, 0);
    g.local.selectedId = g.nextId + 99;
    g.local.selectedIds.push_back(g.nextId + 100);
    g.controlGroupsByOwner[0][0].push_back(g.nextId + 101);
    int wrongOwnerGroupId = spawnEntity(g, E_MILITIA, 0, 30, 30);
    g.controlGroupsByOwner[1][0].push_back(wrongOwnerGroupId);
    assert(!g.entities.empty());
    Entity& corrupt = g.entities.front();
    const int corruptId = corrupt.id;
    corrupt.targetId = g.nextId + 102;
    corrupt.targetX = -4;
    corrupt.targetY = 1;
    corrupt.path.push_back({ -2, 3 });
    corrupt.pathIdx = (int)corrupt.path.size() + 5;
    corrupt.garrison.push_back(g.nextId + 103);
    g.projectiles.push_back({ std::numeric_limits<float>::quiet_NaN(), 2.0f, 3.0f, 4.0f, '-', CP_PROJ_ARROW, 10, true });
    std::vector<ValidationIssue> issues = validateGameStateIssues(g);
    assert(!issues.empty());
    assert(issues.front().severity == ValidationSeverity::Recoverable);
    bool foundTarget = false;
    bool foundPath = false;
    bool foundGarrison = false;
    bool foundProjectile = false;
    bool foundOwnerGroup = false;
    for (const ValidationIssue& issue : issues) {
        assert(!issue.code.empty());
        if (issue.code == "entity_target_id_outside_valid_range" && issue.entityId == corruptId)
            foundTarget = true;
        if (issue.code == "entity_path_point_out_of_bounds" && issue.entityId == corruptId && issue.tile.x == -2 && issue.tile.y == 3)
            foundPath = true;
        if (issue.code == "garrison_id_outside_valid_range" && issue.entityId == corruptId)
            foundGarrison = true;
        if (issue.code == "projectile_coordinate_is_not_finite")
            foundProjectile = true;
        if (issue.code == "owner_control_group_contains_wrong_owner_entity_id" && issue.entityId == wrongOwnerGroupId)
            foundOwnerGroup = true;
    }
    assert(foundTarget);
    assert(foundPath);
    assert(foundGarrison);
    assert(foundProjectile);
    assert(foundOwnerGroup);

    RecoveryResult recovery = recoverGameState(g, issues);
    assert(recovery.recovered);
    assert(recovery.issuesProcessed == (int)issues.size());
    assert(g.local.selectedId == -1);
    assert(g.local.selectedIds.empty());
    assert(g.controlGroupsByOwner[0][0].empty());
    assert(g.controlGroupsByOwner[1][0].empty());
    assert(corrupt.targetId == -1);
    assert(corrupt.pathIdx >= 0);
    assert(corrupt.garrison.empty());
    assert(!g.projectiles.back().alive);
    assert(validateGameState(g, nullptr));

    assert(validateGameStateIssues(g).empty());

    initGameWithSeed(1, 1502u, 0);
    assert(!g.entities.empty());
    g.entities.front().x = -5;
    issues = validateGameStateIssues(g);
    assert(!issues.empty());
    bool foundHardError = false;
    for (const ValidationIssue& issue : issues)
        if (issue.severity == ValidationSeverity::Error) foundHardError = true;
    assert(foundHardError);
}

static void testMatchResetAndDeterminism() {
    initGameWithSeed(2, 2002u, 1);
    std::string first = startupSummary();
    spawnProjectile(g, 5, 5, 10, 10, '-', CP_PROJ_ARROW);
    spawnEntity(g, E_MILITIA, 0, 20, 20);
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
    WorldIndex trainWorld = buildWorldIndex(g);
    assert(startTrainingService(g, trainWorld, gameEvents(), th->owner, th->id, E_PEASANT).ok);
    assert(g.players[0].gold == goldBefore - STATS[E_PEASANT].costGold);
    for (int i = 0; i < 30; i++) {
        trainWorld = buildWorldIndex(g);
        startTrainingService(g, trainWorld, gameEvents(), th->owner, th->id, E_PEASANT);
    }
    assert(g.players[0].supply + (int)th->queue.size() + (th->producing != E_NONE ? 1 : 0) <= g.players[0].supplyMax);

    Game local = g;
    unsigned globalSeed = g.seed;
    local.seed = 98765u;
    local.players[0].supply = 0;
    local.players[0].supplyMax = 0;
    updateSupply(local, 0);
    assert(local.players[0].supply > 0);
    assert(local.players[0].supplyMax > 0);
    assert(g.seed == globalSeed);
    int localReserved = reservedSupply(local, 0);
    assert(localReserved >= local.players[0].supply);
    WorldIndex localWorld = buildWorldIndex(local);
    Entity* localTh = entityById(local, localWorld, th->id);
    assert(localTh);
    localTh->producing = E_NONE;
    localTh->queue.clear();
    local.players[0].gold = std::max(local.players[0].gold, STATS[E_PEASANT].costGold);
    local.players[0].food = std::max(local.players[0].food, STATS[E_PEASANT].costFood);
    local.players[0].wood = std::max(local.players[0].wood, STATS[E_PEASANT].costWood);
    local.players[0].supplyMax = std::max(local.players[0].supplyMax, local.players[0].supply + STATS[E_PEASANT].supplyUsed);
    CanTrainResult localTrain = canTrain(local, 0, *localTh, E_PEASANT);
    assert(localTrain.ok);
    int globalFoodBefore = g.players[0].food;
    int localFoodBefore = local.players[0].food;
    localWorld = buildWorldIndex(local);
    ServiceResult localStart = startTrainingService(local, localWorld, gameEvents(), 0, localTh->id, E_PEASANT);
    assert(localStart.ok);
    assert(local.players[0].food == localFoodBefore - STATS[E_PEASANT].costFood);
    assert(g.players[0].food == globalFoodBefore);
    assert(g.seed == globalSeed);

    Entity* localBuilder = nullptr;
    for (auto& e : local.entities) {
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) { localBuilder = &e; break; }
    }
    assert(localBuilder);
    int localBuilderId = localBuilder->id;
    local.players[0].gold = std::max(local.players[0].gold, STATS[E_HOUSE].costGold);
    local.players[0].wood = std::max(local.players[0].wood, STATS[E_HOUSE].costWood);
    size_t globalEntityCountBefore = g.entities.size();
    size_t localEntityCountBefore = local.entities.size();
    bool builtLocalHouse = false;
    for (int y = 20; y < MAP_H - 5 && !builtLocalHouse; y++) {
        for (int x = 20; x < MAP_W - 5; x++) {
            WorldIndex buildWorld = buildWorldIndex(local);
            if (!canPlace(local, buildWorld, E_HOUSE, x, y, 0)) continue;
            ServiceResult buildResult = startBuildService(local, buildWorld, gameEvents(), 0, localBuilderId, E_HOUSE, { x, y });
            assert(buildResult.ok);
            builtLocalHouse = true;
            break;
        }
    }
    assert(builtLocalHouse);
    assert(local.entities.size() == localEntityCountBefore + 1);
    assert(g.entities.size() == globalEntityCountBefore);

    local.players[0].wood = std::max(local.players[0].wood, STATS[E_WALL].costWood * 3);
    localEntityCountBefore = local.entities.size();
    globalEntityCountBefore = g.entities.size();
    bool builtLocalWallLine = false;
    for (int y = 20; y < MAP_H - 5 && !builtLocalWallLine; y++) {
        for (int x = 20; x < MAP_W - 7; x++) {
            WorldIndex wallWorld = buildWorldIndex(local);
            if (!canPlace(local, wallWorld, E_WALL, x, y, 0)
                || !canPlace(local, wallWorld, E_WALL, x + 1, y, 0)
                || !canPlace(local, wallWorld, E_WALL, x + 2, y, 0)) {
                continue;
            }
            ServiceResult wallResult = startBuildLineService(local, wallWorld, gameEvents(), 0, localBuilderId, E_WALL, { x, y }, { x + 2, y });
            assert(wallResult.ok);
            builtLocalWallLine = true;
            break;
        }
    }
    assert(builtLocalWallLine);
    assert(local.entities.size() == localEntityCountBefore + 3);
    assert(g.entities.size() == globalEntityCountBefore);

    localTh->producing = E_PEASANT;
    localTh->trainTime = STATS[E_PEASANT].trainTime;
    localTh->trainProgress = localTh->trainTime - 1;
    localTh->queue.clear();
    localTh->rallySet = false;
    localEntityCountBefore = local.entities.size();
    globalEntityCountBefore = g.entities.size();
    WorldIndex productionWorld = buildWorldIndex(local);
    tickProduction(local, productionWorld, gameEvents(), *localTh);
    assert(local.entities.size() == localEntityCountBefore + 1);
    assert(g.entities.size() == globalEntityCountBefore);

    int smithId = spawnEntity(local, E_BLACKSMITH, 0, 55, 55);
    WorldIndex researchWorld = buildWorldIndex(local);
    Entity* localSmith = findEntity(local, researchWorld, smithId);
    assert(localSmith);
    local.players[0].research &= ~R_IRON_WEAPONS;
    int globalResearchBefore = g.players[0].research;
    localSmith->researching = R_IRON_WEAPONS;
    localSmith->researchTime = 1;
    localSmith->researchProgress = 0;
    tickResearch(local, gameEvents(), *localSmith);
    assert(local.players[0].research & R_IRON_WEAPONS);
    assert(g.players[0].research == globalResearchBefore);

    int localDepotId = spawnEntity(local, E_LUMBER_CAMP, 0, 57, 55);
    WorldIndex depotWorld = buildWorldIndex(local);
    Entity* localBuilderForDepot = findEntity(local, depotWorld, localBuilderId);
    assert(localBuilderForDepot);
    localBuilderForDepot->x = 57;
    localBuilderForDepot->y = 54;
    localBuilderForDepot->cargo.type = CR_WOOD;
    localBuilderForDepot->cargo.amount = 10;
    Entity* localDepot = findDepot(local, depotWorld, *localBuilderForDepot);
    assert(localDepot);
    assert(localDepot->id == localDepotId);

    const int gatherX = 57, gatherY = 56;
    Tile globalGatherTileBefore = g.map[gatherY][gatherX];
    g.map[gatherY][gatherX].terrain = T_WATER;
    g.map[gatherY][gatherX].resources = 0;
    local.map[gatherY][gatherX].terrain = T_FOREST;
    local.map[gatherY][gatherX].resources = 20;
    WorldIndex gatherWorld = buildWorldIndex(local);
    Entity* localBuilderForGather = findEntity(local, gatherWorld, localBuilderId);
    assert(localBuilderForGather);
    localBuilderForGather->cargo = emptyCargo();
    ServiceResult gatherResult = startGather(local, gatherWorld, gameEvents(), 0, Selection{ localBuilderId, { localBuilderId } }, { gatherX, gatherY });
    assert(gatherResult.ok);
    assert(localBuilderForGather->state == S_GATHERING);
    assert(localBuilderForGather->targetX == gatherX && localBuilderForGather->targetY == gatherY);
    g.map[gatherY][gatherX] = globalGatherTileBefore;

    int localMilitiaId = spawnEntity(local, E_MILITIA, 0, 64, 55);
    int localEnemyId = spawnEntity(local, E_MILITIA, 1, 65, 55);
    WorldIndex attackWorld = buildWorldIndex(local);
    ServiceResult attackResult = startAttack(local, attackWorld, gameEvents(), 0, Selection{ localMilitiaId, { localMilitiaId } }, localEnemyId);
    assert(attackResult.ok);
    Entity* localMilitia = findEntity(local, attackWorld, localMilitiaId);
    assert(localMilitia);
    assert(localMilitia->state == S_ATTACKING);
    assert(localMilitia->targetId == localEnemyId);
    Entity* localEnemy = findEntity(local, attackWorld, localEnemyId);
    assert(localEnemy);
    localMilitia->x = localEnemy->x;
    localMilitia->y = localEnemy->y - 1;
    localMilitia->path.clear();
    localMilitia->pathIdx = 0;
    localMilitia->atkCd = 0;
    localEnemy->hp = 1;
    local.attackNotifyCd = 0;
    int globalAttackNotifyBefore = g.attackNotifyCd;
    size_t globalProjectileCountBefore = g.projectiles.size();
    tickEntity(local, attackWorld, gameEvents(), *localMilitia);
    assert(!localEnemy->alive);
    assert(local.attackNotifyCd == 200);
    assert(g.attackNotifyCd == globalAttackNotifyBefore);
    assert(g.projectiles.size() == globalProjectileCountBefore);

    int localTowerId = spawnEntity(local, E_TOWER, 0, 58, 55);
    int localArcherId = spawnEntity(local, E_ARCHER, 0, 58, 56);
    WorldIndex garrisonWorld = buildWorldIndex(local);
    Entity* localTower = findEntity(local, garrisonWorld, localTowerId);
    Entity* localArcher = findEntity(local, garrisonWorld, localArcherId);
    assert(localTower && localArcher);
    ServiceResult garrisonResult = startGarrison(local, garrisonWorld, gameEvents(), 0, Selection{ localArcher->id, { localArcher->id } }, localTower->id);
    assert(garrisonResult.ok);
    assert(localArcher->state == S_ENTERING);
    assert(localArcher->targetId == localTower->id);
    localTower->garrison.push_back(localArcher->id);
    int archerXBefore = localArcher->x;
    int archerYBefore = localArcher->y;
    ServiceResult ejectResult = ejectGarrisonService(local, garrisonWorld, gameEvents(), 0, Selection{ localTower->id, { localTower->id } });
    assert(ejectResult.ok);
    assert(localTower->garrison.empty());
    assert(localArcher->alive);
    assert(localArcher->x != archerXBefore || localArcher->y != archerYBefore);

    int localMillId = spawnEntity(local, E_MILL, 0, 61, 55);
    WorldIndex deathWorld = buildWorldIndex(local);
    Entity* localMill = findEntity(local, deathWorld, localMillId);
    assert(localMill);
    local.players[0].food = 50;
    localMill->storedFood = 20;
    int globalFoodBeforeDeath = g.players[0].food;
    killEntity(local, gameEvents(), *localMill);
    assert(!localMill->alive);
    assert(localMill->storedFood == 0);
    assert(local.players[0].food == 30);
    assert(g.players[0].food == globalFoodBeforeDeath);

    Entity* localWinterUnit = nullptr;
    for (auto& e : local.entities) {
        if (e.alive && e.owner == 0 && isUnit(e.type)) { localWinterUnit = &e; break; }
    }
    assert(localWinterUnit);
    local.seasonPhase = (float)WINTER;
    local.tick = 100;
    local.players[0].food = 0;
    localWinterUnit->hp = 10;
    int globalFoodBeforeWinter = g.players[0].food;
    tickWinter(local, gameEvents());
    assert(localWinterUnit->hp == 7);
    assert(local.players[0].food == 0);
    assert(g.players[0].food == globalFoodBeforeWinter);

    Terrain globalTerrainBeforeThaw = g.map[0][0].terrain;
    local.tick = 105;
    local.seasonPhase = 0.99f;
    local.map[0][0].terrain = T_SNOW;
    local.map[0][0].preWinterTerrain = T_GRASS;
    tickThaw(local);
    assert(local.map[0][0].terrain == T_GRASS);
    assert(g.map[0][0].terrain == globalTerrainBeforeThaw);

    Terrain globalTerrainBeforePaving = g.map[64][63].terrain;
    int globalWearBeforePaving = g.map[64][63].wear;
    local.tick = 100;
    local.map[64][63].terrain = T_GRASS;
    local.map[64][63].preWinterTerrain = T_GRASS;
    local.map[64][63].wear = 0;
    spawnEntity(local, E_HOUSE, 0, 64, 64);
    tickPaving(local);
    assert(local.map[64][63].wear > 0);
    assert(g.map[64][63].terrain == globalTerrainBeforePaving);
    assert(g.map[64][63].wear == globalWearBeforePaving);

    unsigned globalSeedBeforeWeather = g.seed;
    int globalWeatherBefore = g.weather;
    unsigned globalRngBeforeWeather = g.rngState;
    unsigned localRngBeforeWeather = 24680u;
    local.rngState = localRngBeforeWeather;
    local.tick = 150;
    local.weather = W_RAIN;
    local.weatherTimer = 100;
    local.seasonPhase = (float)SPRING;
    for (int wy = 0; wy < MAP_H; wy++) {
        for (int wx = 0; wx < MAP_W; wx++) {
            local.map[wy][wx].terrain = T_GRASS;
            local.map[wy][wx].resources = 0;
        }
    }
    tickWeather(local, gameEvents());
    int localMudTiles = 0;
    for (int wy = 0; wy < MAP_H; wy++)
        for (int wx = 0; wx < MAP_W; wx++)
            if (local.map[wy][wx].terrain == T_MUD) localMudTiles++;
    assert(localMudTiles > 0);
    assert(local.rngState != localRngBeforeWeather);
    assert(local.weatherTimer == 99);
    assert(g.seed == globalSeedBeforeWeather);
    assert(g.rngState == globalRngBeforeWeather);
    assert(g.weather == globalWeatherBefore);

    local.weather = W_RAIN;
    local.weatherTimer = 0;
    local.seasonPhase = (float)WINTER;
    tickWeather(local, gameEvents());
    assert(local.weather == W_SNOW);
    assert(local.weatherTimer == 300);

    int localMarketId = spawnEntity(local, E_MARKET, 0, 68, 55);
    WorldIndex marketWorld = buildWorldIndex(local);
    Entity* localMarket = findEntity(local, marketWorld, localMarketId);
    assert(localMarket);
    localMarket->underConstruction = false;
    local.tick = 200;
    int localGoldBeforeMarket = local.players[0].gold;
    int globalGoldBeforeMarket = g.players[0].gold;
    tickMarkets(local);
    assert(local.players[0].gold == localGoldBeforeMarket + 5);
    assert(g.players[0].gold == globalGoldBeforeMarket);

    int localGateId = spawnEntity(local, E_GATE, 0, 70, 55);
    int localGateUnitId = spawnEntity(local, E_PEASANT, 0, 71, 55);
    WorldIndex gateWorld = buildWorldIndex(local);
    Entity* localGate = findEntity(local, gateWorld, localGateId);
    Entity* localGateUnit = findEntity(local, gateWorld, localGateUnitId);
    assert(localGate && localGateUnit);
    localGate->underConstruction = false;
    localGate->gateOpen = false;
    localGate->gateLocked = false;
    int globalEntityCountBeforeGate = (int)g.entities.size();
    tickGates(local);
    assert(localGate->gateOpen);
    assert((int)g.entities.size() == globalEntityCountBeforeGate);

    int localTowerFireId = spawnEntity(local, E_TOWER, 0, 72, 55);
    int localTowerTargetId = spawnEntity(local, E_MILITIA, 1, 73, 55);
    WorldIndex towerFireWorld = buildWorldIndex(local);
    Entity* localTowerFire = findEntity(local, towerFireWorld, localTowerFireId);
    Entity* localTowerTarget = findEntity(local, towerFireWorld, localTowerTargetId);
    assert(localTowerFire && localTowerTarget);
    localTowerFire->underConstruction = false;
    localTowerFire->atkCd = 0;
    int localTargetHpBeforeTower = localTowerTarget->hp;
    size_t localProjectilesBeforeTower = local.projectiles.size();
    size_t globalProjectilesBeforeTower = g.projectiles.size();
    resetWorldIndexBuildCount();
    tickTowers(local, gameEvents());
    assert(worldIndexBuildCount() == 1);
    assert(localTowerTarget->hp < localTargetHpBeforeTower);
    assert(local.projectiles.size() > localProjectilesBeforeTower);
    assert(g.projectiles.size() == globalProjectilesBeforeTower);
    local.tick = 30;
    tickProjectiles(local);
    assert(g.projectiles.size() == globalProjectilesBeforeTower);

    int localChurchId = spawnEntity(local, E_CHURCH, 0, 74, 55);
    int localHurtId = spawnEntity(local, E_MILITIA, 0, 75, 55);
    int localConvertId = spawnEntity(local, E_MILITIA, 1, 76, 55);
    WorldIndex churchWorld = buildWorldIndex(local);
    Entity* localChurch = findEntity(local, churchWorld, localChurchId);
    Entity* localHurt = findEntity(local, churchWorld, localHurtId);
    Entity* localConvert = findEntity(local, churchWorld, localConvertId);
    assert(localChurch && localHurt && localConvert);
    localChurch->underConstruction = false;
    localHurt->hp = localHurt->maxHp - 2;
    localConvert->convertTicks = 200 + localConvert->maxHp * 3 - 1;
    int globalSupplyBeforeChurch = g.players[0].supply;
    local.tick = 225;
    tickChurches(local, gameEvents());
    assert(localHurt->hp == localHurt->maxHp - 1);
    assert(localConvert->owner == 0);
    assert(local.players[0].supply >= globalSupplyBeforeChurch);
    assert(g.players[0].supply == globalSupplyBeforeChurch);

    int localMillForFarmId = spawnEntity(local, E_MILL, 0, 78, 55);
    int localFarmId = spawnEntity(local, E_FARM, 0, 80, 55);
    int localFarmWorkerId = spawnEntity(local, E_PEASANT, 0, 81, 55);
    WorldIndex farmWorld = buildWorldIndex(local);
    Entity* localFarm = findEntity(local, farmWorld, localFarmId);
    Entity* localMillForFarm = findEntity(local, farmWorld, localMillForFarmId);
    Entity* localFarmWorker = findEntity(local, farmWorld, localFarmWorkerId);
    assert(localFarm && localMillForFarm && localFarmWorker);
    localFarm->underConstruction = false;
    localMillForFarm->underConstruction = false;
    localFarm->storedFood = 0;
    local.farmTimer = 39;
    local.seasonPhase = (float)SUMMER;
    int globalEntityCountBeforeFarmTick = (int)g.entities.size();
    tickFarms(local, gameEvents());
    assert(localFarm->storedFood == 7);
    assert((int)g.entities.size() == globalEntityCountBeforeFarmTick);

    int aiFarmId = spawnEntity(local, E_FARM, 1, 84, 55);
    int aiWorkerId = spawnEntity(local, E_PEASANT, 1, 85, 55);
    farmWorld = buildWorldIndex(local);
    Entity* aiFarm = findEntity(local, farmWorld, aiFarmId);
    Entity* aiWorker = findEntity(local, farmWorld, aiWorkerId);
    assert(aiFarm && aiWorker);
    aiFarm->underConstruction = false;
    aiFarm->storedFood = 3;
    aiWorker->state = S_IDLE;
    local.farmTimer = 39;
    local.seasonPhase = (float)SPRING;
    resetWorldIndexBuildCount();
    tickFarms(local, gameEvents());
    assert(worldIndexBuildCount() == 1);
    assert(aiWorker->state == S_BUILDING);
    assert(aiWorker->targetId == aiFarm->id);

    int localSheepId = spawnEntity(local, E_SHEEP, OWNER_NATURE, 90, 55);
    int localShepherdId = spawnEntity(local, E_PEASANT, 0, 89, 55);
    WorldIndex animalWorld = buildWorldIndex(local);
    Entity* localSheep = findEntity(local, animalWorld, localSheepId);
    Entity* localShepherd = findEntity(local, animalWorld, localShepherdId);
    assert(localSheep && localShepherd);
    localSheep->state = S_IDLE;
    local.animalTimer = 0;
    int globalEntityCountBeforeAnimals = (int)g.entities.size();
    resetWorldIndexBuildCount();
    tickAnimals(local, gameEvents());
    assert(worldIndexBuildCount() == 1);
    assert(localSheep->state == S_MOVING);
    assert(!localSheep->path.empty());
    assert((int)g.entities.size() == globalEntityCountBeforeAnimals);

    bool globalVisibleBeforeFog = g.map[10][10].visible[0];
    for (int fy = 0; fy < MAP_H; fy++)
        for (int fx = 0; fx < MAP_W; fx++)
            local.map[fy][fx].visible[0] = local.map[fy][fx].explored[0] = false;
    int localScoutId = spawnEntity(local, E_PEASANT, 0, 10, 10);
    WorldIndex fogWorld = buildWorldIndex(local);
    Entity* localScout = findEntity(local, fogWorld, localScoutId);
    assert(localScout);
    localScout->underConstruction = false;
    updateFog(local);
    assert(local.map[10][10].visible[0]);
    assert(local.map[10][10].explored[0]);
    assert(g.map[10][10].visible[0] == globalVisibleBeforeFog);

    auto localWin = std::make_unique<Game>(g);
    int globalModeBeforeWin = g.mode;
    int globalWinnerBeforeWin = g.winner;
    localWin->mode = M_NORMAL;
    localWin->winner = -2;
    for (int p = 1; p < MAX_PLAYERS; p++) localWin->players[p].alive = false;
    checkWin(*localWin);
    assert(localWin->mode == M_GAME_OVER);
    assert(localWin->winner == 0);
    assert(g.mode == globalModeBeforeWin);
    assert(g.winner == globalWinnerBeforeWin);

    auto localSim = std::make_unique<Game>(g);
    int globalTickBeforeSim = g.tick;
    int globalUiStatusTimerBeforeSim = ui.statusTimer;
    ui.statusTimer = 3;
    ui.actionMarkers.clear();
    ui.actionMarkers.push_back({ 1, 1, 1, '!' });
    size_t globalProjectileCountBeforeSim = g.projectiles.size();
    localSim->tick = 29;
    localSim->projectiles.clear();
    spawnProjectile(*localSim, 1, 1, 5, 1, '-', CP_PROJ_ARROW);
    tickSimulationOnce(*localSim, gameEvents());
    assert(localSim->tick == 30);
    assert(ui.statusTimer == 3);
    assert(ui.actionMarkers.size() == 1);
    tickUiState(ui);
    assert(ui.statusTimer == 2);
    assert(ui.actionMarkers.empty());
    assert(!localSim->projectiles.empty());
    assert(localSim->projectiles.front().x > 1.0f);
    assert(g.tick == globalTickBeforeSim);
    ui.statusTimer = globalUiStatusTimerBeforeSim;
    assert(g.projectiles.size() == globalProjectileCountBeforeSim);

    Terrain globalTerrainBeforeSeason = g.map[2][2].terrain;
    local.map[2][2].terrain = T_GRASS;
    local.map[2][2].preWinterTerrain = T_GRASS;
    local.seasonPhase = (float)WINTER;
    local.prevSeason = AUTUMN;
    tickSeasons(local, gameEvents());
    assert(local.prevSeason == WINTER);
    assert(local.map[2][2].terrain == T_SNOW);
    assert(g.map[2][2].terrain == globalTerrainBeforeSeason);

    Entity* peasant = nullptr;
    for (auto& e : g.entities)
        if (e.alive && e.owner == 0 && e.type == E_PEASANT) { peasant = &e; break; }
    assert(peasant);
    g.players[0].gold = STATS[E_TOWNHALL].costGold;
    g.players[0].wood = STATS[E_TOWNHALL].costWood;
    for (int y = 20; y < MAP_H - 5; y++) {
        for (int x = 20; x < MAP_W - 5; x++) {
            if (!testCanPlace(E_TOWNHALL, x, y, 0)) continue;
                            WorldIndex buildWorld = buildWorldIndex(g);
                            assert(startBuildService(g, buildWorld, gameEvents(), peasant->owner, peasant->id, E_TOWNHALL, { x, y }).ok);
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
    g.local.selectedId = th->id;
    g.local.selectedIds.clear();
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
    th = testFindEntity(g.local.selectedId);
    assert(th);
    assert(th->producing == E_PEASANT);
    assert(g.players[0].gold == goldBefore - STATS[E_PEASANT].costGold);
    handleInput(27);
    assert(g.mode == M_NORMAL);
}

static void testHoldPositionInput() {
    initGameWithSeed(1, 3277u, 0);
    int id = spawnEntity(g, E_MILITIA, 0, 30, 30);
    Entity* m = testFindEntity(id);
    assert(m);
    m->state = S_MOVING;
    m->holdPosition = 0;
    m->targetId = 7;
    g.local.selectedId = id;
    g.local.selectedIds.clear();
    g.mode = M_NORMAL;

    // 'X' must hold position during gameplay, not exit the application.
    handleInput('X');
    m = testFindEntity(id);
    assert(m);
    assert(m->state == S_IDLE);
    assert(m->holdPosition == 1);
    assert(m->targetId == -1);

    // Lowercase 'x' behaves identically.
    m->state = S_MOVING;
    m->holdPosition = 0;
    handleInput('x');
    m = testFindEntity(id);
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
    int pid = spawnEntity(g, E_PEASANT, 0, 39, 40);
    Entity* peasant = testFindEntity(pid);
    assert(peasant);

    int wallCost = STATS[E_WALL].costWood;
    assert(wallCost > 0);
    int segments = 5; // x = 41..45 inclusive (avoid the peasant's own tile at 39/40)
    g.players[0].wood = wallCost * segments;
    g.players[0].gold = 500;
    int woodBefore = g.players[0].wood;

    WorldIndex wallWorld = buildWorldIndex(g);
    assert(startBuildLineService(g, wallWorld, gameEvents(), peasant->owner, peasant->id, E_WALL, { 41, 40 }, { 45, 40 }).ok);
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
    int pid2 = spawnEntity(g, E_PEASANT, 0, 39, 40);
    Entity* peasant2 = testFindEntity(pid2);
    assert(peasant2);
    g.players[0].wood = STATS[E_WALL].costWood; // only enough for 1 of 5
    int woodBefore2 = g.players[0].wood;
    wallWorld = buildWorldIndex(g);
    assert(!startBuildLineService(g, wallWorld, gameEvents(), peasant2->owner, peasant2->id, E_WALL, { 41, 40 }, { 45, 40 }).ok);
    assert(countTypeOwner(E_WALL, 0) == 0);
    assert(g.players[0].wood == woodBefore2);

    // Non-human builder spends that player's resources, owner-aware.
    initGameWithSeed(1, 3379u, 0);
    for (int x = 40; x <= 46; x++) { g.map[40][x].terrain = T_GRASS; g.map[40][x].resources = 0; }
    int aid = spawnEntity(g, E_PEASANT, 1, 39, 40);
    Entity* aiPeasant = testFindEntity(aid);
    assert(aiPeasant);
    g.players[1].wood = STATS[E_WALL].costWood * 3;
    g.players[0].wood = 999;
    int p0WoodBefore = g.players[0].wood;
    wallWorld = buildWorldIndex(g);
    assert(startBuildLineService(g, wallWorld, gameEvents(), aiPeasant->owner, aiPeasant->id, E_WALL, { 41, 40 }, { 43, 40 }).ok);
    assert(countTypeOwner(E_WALL, 1) == 3);
    assert(g.players[1].wood == 0);
    assert(g.players[0].wood == p0WoodBefore); // human resources untouched
    assert(validateGameState(g, nullptr));
}

static void testResearchService() {
    // Shared research service: same canonical costs/durations for human and AI,
    // validation rules enforced centrally.
    initGameWithSeed(1, 4501u, 0);
    int sid = spawnEntity(g, E_BLACKSMITH, 0, 30, 30); // built=true
    Entity* smith = testFindEntity(sid);
    assert(smith && !smith->underConstruction);

    const ResearchDef* iron = researchDef(ResearchId::IronWeapons);
    assert(iron && iron->bit == R_IRON_WEAPONS);

    // Human pays the canonical cost and gets the canonical duration.
    g.players[0].gold = iron->costGold;
    g.players[0].wood = iron->costWood;
    TestEventSink researchSink;
    WorldIndex researchStartWorld = buildWorldIndex(g);
    ServiceResult started = startResearchService(g, researchStartWorld, researchSink, 0, smith->id, ResearchId::IronWeapons);
    assert(started.ok);
    assert(researchSink.has(GameEventType::ResearchStarted));
    assert(g.players[0].gold == 0 && g.players[0].wood == 0);
    assert(smith->researching == R_IRON_WEAPONS);
    assert(smith->researchTime == iron->ticks);

    // Can't start a second research while one is in progress.
    g.players[0].gold = 999; g.players[0].wood = 999;
    WorldIndex busyResearchWorld = buildWorldIndex(g);
    assert(!startResearchService(g, busyResearchWorld, gameEvents(), 0, smith->id, ResearchId::Crossbows).ok);

    // Complete the research; bit gets set and the smith frees up.
    for (int i = 0; i < iron->ticks + 50; i++) {
        tickSimulationOnce(g, gameEvents(), true);
        if (g.players[0].research & R_IRON_WEAPONS) break;
    }
    assert(g.players[0].research & R_IRON_WEAPONS);

    // Already-researched is rejected even with resources.
    g.players[0].gold = 999; g.players[0].wood = 999;
    smith = testFindEntity(sid);
    assert(smith && smith->researching == 0);
    WorldIndex researchWorld = buildWorldIndex(g);
    CanResearchResult again = canResearch(g, researchWorld, 0, *smith, ResearchId::IronWeapons);
    assert(!again.ok);
    ServiceResult researchRejected = startResearchService(g, researchWorld, gameEvents(), 0, smith->id, ResearchId::IronWeapons);
    assert(!researchRejected.ok);
    assert(std::string(researchRejected.reason) == "Already researched.");

    // Counterweight requires an owned, completed Castle.
    const ResearchDef* cw = researchDef(ResearchId::Counterweight);
    assert(cw && cw->requiredOwnedBuilding == E_CASTLE);
    g.players[0].gold = cw->costGold; g.players[0].wood = cw->costWood;
    researchWorld = buildWorldIndex(g);
    assert(!startResearchService(g, researchWorld, gameEvents(), 0, smith->id, ResearchId::Counterweight).ok);
    int castleId = spawnEntity(g, E_CASTLE, 0, 50, 50); // built=true
    (void)castleId;
    g.players[0].gold = cw->costGold; g.players[0].wood = cw->costWood;
    smith = testFindEntity(sid);
    researchWorld = buildWorldIndex(g);
    assert(startResearchService(g, researchWorld, gameEvents(), 0, smith->id, ResearchId::Counterweight).ok);
    assert(smith->researching == R_COUNTERWEIGHT);

    // AI (owner 1) pays resources through the same service.
    initGameWithSeed(1, 4502u, 0);
    int asid = spawnEntity(g, E_BLACKSMITH, 1, 30, 30);
    Entity* aiSmith = testFindEntity(asid);
    assert(aiSmith && !aiSmith->underConstruction);
    g.players[1].gold = iron->costGold; g.players[1].wood = iron->costWood;
    int p0Gold = g.players[0].gold, p0Wood = g.players[0].wood;
    researchWorld = buildWorldIndex(g);
    assert(startResearchService(g, researchWorld, gameEvents(), 1, aiSmith->id, ResearchId::IronWeapons).ok);
    assert(g.players[1].gold == 0 && g.players[1].wood == 0);
    assert(aiSmith->researchTime == iron->ticks); // same duration as human
    assert(g.players[0].gold == p0Gold && g.players[0].wood == p0Wood); // human untouched

    // Not enough resources is rejected.
    initGameWithSeed(1, 4503u, 0);
    int psid = spawnEntity(g, E_BLACKSMITH, 0, 30, 30);
    Entity* poorSmith = testFindEntity(psid);
    assert(poorSmith);
    g.players[0].gold = 0; g.players[0].wood = 0;
    researchWorld = buildWorldIndex(g);
    assert(!startResearchService(g, researchWorld, gameEvents(), 0, poorSmith->id, ResearchId::IronWeapons).ok);
    assert(poorSmith->researching == 0);
    assert(validateGameState(g, nullptr));
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
    int bid = spawnEntity(g, E_BARRACKS, 0, 30, 30);
    Entity* bar = testFindEntity(bid);
    assert(bar && !bar->underConstruction);
    g.players[0].gold = 999; g.players[0].wood = 999;
    g.players[0].food = 100;
    g.players[0].supplyMax = 50;
    int foodBefore = g.players[0].food;
    WorldIndex trainWorld = buildWorldIndex(g);
    assert(startTrainingService(g, trainWorld, gameEvents(), bar->owner, bar->id, E_MILITIA).ok);
    assert(bar->producing == E_MILITIA);
    assert(g.players[0].food == foodBefore - STATS[E_MILITIA].costFood);
    assert(validateGameState(g, nullptr));
}

static void testMarketTradeService() {
    // Shared market service: rates come from one table; validation centralized.
    initGameWithSeed(1, 4701u, 0);
    int mid = spawnEntity(g, E_MARKET, 0, 30, 30);
    Entity* market = testFindEntity(mid);
    assert(market && !market->underConstruction);

    Player& p = g.players[0];
    p.gold = 100; p.wood = 0; p.food = 0;

    // Gold -> wood at the canonical 40:30 rate.
    TestEventSink marketSink;
    WorldIndex marketWorld = buildWorldIndex(g);
    assert(executeTradeService(g, marketWorld, marketSink, 0, market->id, MarketTradeType::GoldForWood).ok);
    assert(marketSink.has(GameEventType::StatusMessage));
    assert(p.gold == 60 && p.wood == 30);

    // Wood -> gold.
    p.wood = 40; p.gold = 0;
    marketWorld = buildWorldIndex(g);
    assert(executeTradeService(g, marketWorld, gameEvents(), 0, market->id, MarketTradeType::WoodForGold).ok);
    assert(p.wood == 0 && p.gold == 30);

    // Gold -> food.
    p.gold = 50; p.food = 0;
    marketWorld = buildWorldIndex(g);
    assert(executeTradeService(g, marketWorld, gameEvents(), 0, market->id, MarketTradeType::GoldForFood).ok);
    assert(p.gold == 0 && p.food == 30);

    // Food -> gold.
    p.food = 40; p.gold = 0;
    marketWorld = buildWorldIndex(g);
    assert(executeTradeService(g, marketWorld, gameEvents(), 0, market->id, MarketTradeType::FoodForGold).ok);
    assert(p.food == 0 && p.gold == 30);

    // Insufficient resources rejected, nothing mutated.
    p.gold = 10; p.wood = 5;
    marketWorld = buildWorldIndex(g);
    ServiceResult tradeRejected = executeTradeService(g, marketWorld, gameEvents(), 0, market->id, MarketTradeType::GoldForWood);
    assert(!tradeRejected.ok);
    assert(std::string(tradeRejected.reason) == "Not enough resources.");
    assert(p.gold == 10 && p.wood == 5);

    // Trade against another player's market is rejected.
    initGameWithSeed(1, 4702u, 0);
    int amid = spawnEntity(g, E_MARKET, 1, 30, 30);
    Entity* aiMarket = testFindEntity(amid);
    assert(aiMarket);
    g.players[0].gold = 100;
    int p0GoldBefore = g.players[0].gold;
    CanTradeResult notMine = canTrade(g, 0, *aiMarket, MarketTradeType::GoldForWood);
    assert(!notMine.ok);
    marketWorld = buildWorldIndex(g);
    assert(!executeTradeService(g, marketWorld, gameEvents(), 0, aiMarket->id, MarketTradeType::GoldForWood).ok);
    assert(g.players[0].gold == p0GoldBefore);

    // Trade against an under-construction market is rejected.
    initGameWithSeed(1, 4703u, 0);
    int umid = spawnEntity(g, E_MARKET, 0, 30, 30, false);
    Entity* uMarket = testFindEntity(umid);
    assert(uMarket && uMarket->underConstruction);
    g.players[0].gold = 100;
    marketWorld = buildWorldIndex(g);
    assert(!executeTradeService(g, marketWorld, gameEvents(), 0, uMarket->id, MarketTradeType::GoldForWood).ok);
    assert(g.players[0].gold == 100);
    assert(validateGameState(g, nullptr));
}

static void testCommandSelectionDriftProtection() {
    initGameWithSeed(1, 4801u, 0);
    for (int y = 28; y <= 34; y++) for (int x = 28; x <= 36; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }
    int aid = spawnEntity(g, E_MILITIA, 0, 30, 30);
    int bid = spawnEntity(g, E_MILITIA, 0, 31, 30);
    Entity* a = testFindEntity(aid);
    Entity* b = testFindEntity(bid);
    assert(a && b);

    Command move;
    move.payload = MoveCommand{ Selection{ aid, { aid } }, { 35, 30 } };

    // Change the global UI selection after the command is created. Dispatch must
    // use the command payload, not the current UI selection.
    g.local.selectedId = bid;
    g.local.selectedIds = { bid };
    assert(dispatchTestCommand(move).status == CommandStatus::Accepted);

    a = testFindEntity(aid);
    b = testFindEntity(bid);
    assert(a && b);
    assert(a->targetX == 35 && a->targetY == 30);
    assert(!(b->targetX == 35 && b->targetY == 30));

    b->state = S_MOVING;
    b->attackMove = 1;
    b->holdPosition = 0;
    a->state = S_MOVING;
    a->attackMove = 1;
    a->holdPosition = 0;
    Command hold;
    hold.payload = HoldPositionCommand{ Selection{ aid, { aid } } };
    assert(dispatchTestCommand(hold).status == CommandStatus::Accepted);

    assert(a->state == S_IDLE && a->holdPosition == 1 && a->attackMove == 0);
    assert(b->state == S_MOVING && b->holdPosition == 0 && b->attackMove == 1);
    assert(validateGameState(g, nullptr));
}

static void testContextResolverProducesTypedCommands() {
    initGameWithSeed(1, 4806u, 0);
    for (int y = 28; y <= 34; y++) for (int x = 28; x <= 38; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
        g.map[y][x].visible[0] = true;
        g.map[y][x].explored[0] = true;
    }
    int workerId = spawnEntity(g, E_PEASANT, 0, 30, 30);
    int enemyId = spawnEntity(g, E_MILITIA, 1, 33, 30);
    int aiWorkerId = spawnEntity(g, E_PEASANT, 1, 31, 31);
    Selection workerSelection{ workerId, { workerId } };
    Selection aiWorkerSelection{ aiWorkerId, { aiWorkerId } };
    auto emptyTile = []() {
        for (int y = 28; y <= 34; y++)
            for (int x = 28; x <= 38; x++)
                if (!testEntityAt(x, y)) return MapPos{ x, y };
        return MapPos{ 35, 30 };
    };

    MapPos moveTarget = emptyTile();
    WorldIndex resolverWorld = buildWorldIndex(g);
    Command move = resolveContextCommand(g, resolverWorld, 0, workerSelection, moveTarget);
    assert(commandHasPayload<MoveCommand>(move));

    Command attack = resolveContextCommand(g, resolverWorld, 0, workerSelection, { 33, 30 });
    assert(commandHasPayload<AttackCommand>(attack));
    const AttackCommand* attackPayload = std::get_if<AttackCommand>(&attack.payload);
    assert(attackPayload && attackPayload->targetId == enemyId);

    g.map[30][30].visible[1] = true;
    Command aiAttack = resolveContextCommand(g, resolverWorld, 1, aiWorkerSelection, { 30, 30 });
    assert(aiAttack.issuer == 1);
    assert(commandHasPayload<AttackCommand>(aiAttack));
    const AttackCommand* aiAttackPayload = std::get_if<AttackCommand>(&aiAttack.payload);
    assert(aiAttackPayload && aiAttackPayload->targetId == workerId);

    WorldIndex world = buildWorldIndex(g);
    GameContext context{ g, world, gameEvents() };
    Command legacyContext;
    legacyContext.issuer = 1;
    legacyContext.payload = ContextCommand{ aiWorkerSelection, { 30, 30 } };
    assert(dispatchCommand(context, legacyContext).status == CommandStatus::Accepted);
    Entity* aiWorker = testFindEntity(aiWorkerId);
    assert(aiWorker && aiWorker->targetId == workerId);

    MapPos gatherTarget = emptyTile();
    g.map[gatherTarget.y][gatherTarget.x].terrain = T_GOLD;
    g.map[gatherTarget.y][gatherTarget.x].resources = 200;
    resolverWorld = buildWorldIndex(g);
    Command gather = resolveContextCommand(g, resolverWorld, 0, workerSelection, gatherTarget);
    assert(commandHasPayload<GatherCommand>(gather));

    MapPos farmTarget = emptyTile();
    g.map[farmTarget.y][farmTarget.x].terrain = T_WHEAT;
    g.map[farmTarget.y][farmTarget.x].resources = 0;
    resolverWorld = buildWorldIndex(g);
    Command farm = resolveContextCommand(g, resolverWorld, 0, workerSelection, farmTarget);
    assert(commandHasPayload<BuildCommand>(farm));
    const BuildCommand* buildPayload = std::get_if<BuildCommand>(&farm.payload);
    assert(buildPayload && buildPayload->entityType == E_FARM);

    int houseId = spawnEntity(g, E_HOUSE, 0, 36, 30);
    Entity* house = testFindEntity(houseId);
    assert(house);
    house->underConstruction = true;
    resolverWorld = buildWorldIndex(g);
    Command help = resolveContextCommand(g, resolverWorld, 0, workerSelection, { 36, 30 });
    assert(commandHasPayload<HelpCommand>(help));
    const HelpCommand* helpPayload = std::get_if<HelpCommand>(&help.payload);
    assert(helpPayload && helpPayload->targetId == houseId);
    Command legacyHelp;
    legacyHelp.payload = ContextCommand{ workerSelection, { 36, 30 } };
    assert(dispatchTestCommand(legacyHelp).status == CommandStatus::Accepted);
    Entity* worker = testFindEntity(workerId);
    assert(worker && worker->state == S_BUILDING && worker->targetId == houseId);
    house->underConstruction = false;
    resolverWorld = buildWorldIndex(g);
    Command garrison = resolveContextCommand(g, resolverWorld, 0, workerSelection, { 36, 30 });
    assert(commandHasPayload<GarrisonCommand>(garrison));
    int farmId = spawnEntity(g, E_FARM, 0, 38, 30);
    Entity* existingFarm = testFindEntity(farmId);
    assert(existingFarm);
    existingFarm->underConstruction = false;
    resolverWorld = buildWorldIndex(g);
    Command tend = resolveContextCommand(g, resolverWorld, 0, workerSelection, { 38, 30 });
    assert(commandHasPayload<HelpCommand>(tend));
    const HelpCommand* tendPayload = std::get_if<HelpCommand>(&tend.payload);
    assert(tendPayload && tendPayload->targetId == farmId);
    assert(validateGameState(g, nullptr));
}

static void testCommandPayloadDispatchCoverage() {
    static_assert(std::variant_size<CommandPayload>::value == 33,
                  "Add this payload to dispatchCommand architecture coverage.");

    auto prepareClearArea = []() {
        for (int y = 20; y <= 50; y++) for (int x = 20; x <= 70; x++) {
            g.map[y][x].terrain = T_GRASS;
            g.map[y][x].resources = 0;
            g.map[y][x].visible[0] = true;
            g.map[y][x].explored[0] = true;
            g.map[y][x].visible[1] = true;
            g.map[y][x].explored[1] = true;
        }
    };

    auto dispatchWithSink = [&](GameContext& context, TestEventSink& sink, Command command,
                                CommandStatus expected) {
        size_t before = sink.events.size();
        CommandResult result = dispatchCommand(context, command);
        assert(result.status == expected);
        if (expected == CommandStatus::Accepted) {
            assert(sink.events.size() > before);
            assert(sink.events.back().type == GameEventType::CommandAccepted);
        } else if (expected == CommandStatus::Rejected) {
            assert(sink.events.size() > before);
            assert(sink.events.back().type == GameEventType::CommandRejected);
        } else {
            assert(sink.events.size() == before);
        }
        context.world = buildWorldIndex(context.game);
        return result;
    };

    initGameWithSeed(1, 4860u, 0);
    prepareClearArea();
    TestEventSink sink;
    WorldIndex world = buildWorldIndex(g);
    GameContext context{ g, world, sink };

    dispatchWithSink(context, sink, Command{ 0, std::monostate{} }, CommandStatus::NoOp);

    int workerId = spawnEntity(g, E_PEASANT, 0, 24, 24);
    int secondWorkerId = spawnEntity(g, E_PEASANT, 0, 25, 24);
    int militiaId = spawnEntity(g, E_MILITIA, 0, 26, 24);
    int enemyId = spawnEntity(g, E_MILITIA, 1, 32, 24);
    int barracksId = spawnEntity(g, E_BARRACKS, 0, 36, 24);
    int smithId = spawnEntity(g, E_BLACKSMITH, 0, 40, 24);
    int marketId = spawnEntity(g, E_MARKET, 0, 44, 24);
    int towerId = spawnEntity(g, E_TOWER, 0, 48, 24);
    int gateId = spawnEntity(g, E_GATE, 0, 52, 24);
    int trebuchetId = spawnEntity(g, E_TREBUCHET, 0, 56, 24);
    context.world = buildWorldIndex(g);

    Entity* barracks = findEntity(g, context.world, barracksId);
    Entity* smith = findEntity(g, context.world, smithId);
    Entity* market = findEntity(g, context.world, marketId);
    Entity* tower = findEntity(g, context.world, towerId);
    Entity* gate = findEntity(g, context.world, gateId);
    Entity* trebuchet = findEntity(g, context.world, trebuchetId);
    assert(barracks && smith && market && tower && gate && trebuchet);
    barracks->underConstruction = false;
    smith->underConstruction = false;
    market->underConstruction = false;
    tower->underConstruction = false;
    gate->underConstruction = false;
    trebuchet->underConstruction = false;
    g.players[0].gold = 5000;
    g.players[0].wood = 5000;
    g.players[0].food = 5000;
    g.players[0].supplyMax = 1000;
    context.world = buildWorldIndex(g);

    dispatchWithSink(context, sink, Command{ 0, SelectCommand{ { 24, 24 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, BoxSelectCommand{ { 23, 23 }, { 26, 25 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, SelectAllOfTypeInViewCommand{ { 24, 24 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, ContextCommand{ Selection{ workerId, { workerId } }, { 28, 24 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, MoveCommand{ Selection{ workerId, { workerId } }, { 29, 24 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, WaypointCommand{ Selection{ workerId, { workerId } }, { 31, 24 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, PatrolCommand{ Selection{ militiaId, { militiaId } }, { 31, 25 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, AttackCommand{ Selection{ workerId, { workerId } }, enemyId } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, AttackMoveCommand{ Selection{ workerId, { workerId } }, { 30, 24 } } }, CommandStatus::Accepted);

    g.map[31][24].terrain = T_GOLD;
    g.map[31][24].resources = 300;
    dispatchWithSink(context, sink, Command{ 0, GatherCommand{ Selection{ workerId, { workerId } }, { 24, 31 } } }, CommandStatus::Accepted);

    dispatchWithSink(context, sink, Command{ 0, BuildCommand{ Selection{ workerId, { workerId } }, E_HOUSE, { 29, 30 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, BuildLineCommand{ Selection{ secondWorkerId, { secondWorkerId } }, E_WALL, { 33, 30 }, { 35, 30 } } }, CommandStatus::Accepted);
    g.players[0].gold = 5000;
    g.players[0].wood = 5000;
    g.players[0].food = 5000;
    g.players[0].supplyMax = 1000;
    dispatchWithSink(context, sink, Command{ 0, TrainCommand{ Selection{ barracksId, { barracksId } }, E_MILITIA } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, ResearchCommand{ Selection{ smithId, { smithId } }, ResearchId::IronWeapons } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, MarketTradeCommand{ Selection{ marketId, { marketId } }, MarketTradeType::GoldForWood } }, CommandStatus::Accepted);

    int helpTargetId = spawnEntity(g, E_HOUSE, 0, 38, 30);
    context.world = buildWorldIndex(g);
    Entity* helpTarget = findEntity(g, context.world, helpTargetId);
    assert(helpTarget);
    helpTarget->underConstruction = true;
    context.world = buildWorldIndex(g);
    dispatchWithSink(context, sink, Command{ 0, HelpCommand{ Selection{ workerId, { workerId } }, helpTargetId } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, GarrisonCommand{ Selection{ secondWorkerId, { secondWorkerId } }, towerId } }, CommandStatus::Accepted);
    tower = findEntity(g, context.world, towerId);
    Entity* secondWorker = findEntity(g, context.world, secondWorkerId);
    assert(tower && secondWorker);
    tower->garrison.push_back(secondWorkerId);
    secondWorker->state = S_GARRISONED;
    dispatchWithSink(context, sink, Command{ 0, EjectGarrisonCommand{ Selection{ towerId, { towerId } } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, SetRallyCommand{ Selection{ barracksId, { barracksId } }, { 36, 32 } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, HoldPositionCommand{ Selection{ workerId, { workerId } } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, StopCommand{ Selection{ workerId, { workerId } } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, AssignControlGroupCommand{ Selection{ workerId, { workerId } }, 0 } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, RecallControlGroupCommand{ 0 } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, TogglePauseCommand{} }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, TogglePauseCommand{} }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, ToggleGateCommand{ Selection{ gateId, { gateId } } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, ToggleTrebuchetPackedCommand{ Selection{ trebuchetId, { trebuchetId } } } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, ToggleDiagnosticsCommand{} }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, RevealMapDebugCommand{} }, CommandStatus::Accepted);

    std::remove("realm-slot6.sav");
    dispatchWithSink(context, sink, Command{ 0, SaveCommand{ 6 } }, CommandStatus::Accepted);
    dispatchWithSink(context, sink, Command{ 0, LoadCommand{ 6 } }, CommandStatus::Accepted);
    std::remove("realm-slot6.sav");
    dispatchWithSink(context, sink, Command{ 0, ResignCommand{} }, CommandStatus::Accepted);

    Command wrongIssuer;
    wrongIssuer.issuer = 1;
    wrongIssuer.payload = MoveCommand{ Selection{ workerId, { workerId } }, { 31, 24 } };
    dispatchWithSink(context, sink, wrongIssuer, CommandStatus::Rejected);
}

static void testCommandDispatcherAppActions() {
    initGameWithSeed(1, 4802u, 0);
    Command command;
    command.payload = TogglePauseCommand{};
    dispatchTestCommand(command);
    assert(g.mode == M_PAUSED);
    dispatchTestCommand(command);
    assert(g.mode == M_NORMAL);

    command = Command{};
    command.payload = ToggleDiagnosticsCommand{};
    bool diagnosticsBefore = g.local.diagnostics;
    dispatchTestCommand(command);
    assert(g.local.diagnostics != diagnosticsBefore);

    g.map[0][0].visible[0] = false;
    g.map[0][0].explored[0] = false;
    command = Command{};
    command.payload = RevealMapDebugCommand{};
    dispatchTestCommand(command);
    assert(g.map[0][0].visible[0] && g.map[0][0].explored[0]);

    int gid = spawnEntity(g, E_GATE, 0, 30, 30);
    Entity* gate = testFindEntity(gid);
    assert(gate && !gate->gateLocked);
    command = Command{};
    command.payload = ToggleGateCommand{ Selection{ gid, { gid } } };
    dispatchTestCommand(command);
    assert(gate->gateLocked && gate->gateOpen);
    dispatchTestCommand(command);
    assert(gate->gateLocked && !gate->gateOpen);
    dispatchTestCommand(command);
    assert(!gate->gateLocked);

    int tid = spawnEntity(g, E_TREBUCHET, 0, 34, 30);
    Entity* treb = testFindEntity(tid);
    assert(treb && treb->packed == 1);
    command = Command{};
    command.payload = ToggleTrebuchetPackedCommand{ Selection{ tid, { tid } } };
    dispatchTestCommand(command);
    assert(treb->packed == 0 && treb->packTicks == 40);

    int a = spawnEntity(g, E_MILITIA, 0, 40, 30);
    int b = spawnEntity(g, E_ARCHER, 0, 41, 30);
    command = Command{};
    command.payload = AssignControlGroupCommand{ Selection{ a, { a, b } }, 2 };
    dispatchTestCommand(command);
    assert(g.controlGroupsByOwner[0][2].size() == 2);
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    command = Command{};
    command.payload = RecallControlGroupCommand{ 2 };
    dispatchTestCommand(command);
    assert(g.local.selectedId == a);
    assert(g.local.selectedIds.size() == 2);

    int c = spawnEntity(g, E_MILITIA, 1, 42, 30);
    int d = spawnEntity(g, E_ARCHER, 1, 43, 30);
    command = Command{};
    command.issuer = 1;
    command.payload = AssignControlGroupCommand{ Selection{ c, { c, d, a } }, 2 };
    CommandResult ownerGroupResult = dispatchTestCommand(command);
    assert(ownerGroupResult.status == CommandStatus::Accepted);
    assert(g.controlGroupsByOwner[0][2].size() == 2);
    assert(g.controlGroupsByOwner[1][2].size() == 2);
    assert(std::find(g.controlGroupsByOwner[1][2].begin(), g.controlGroupsByOwner[1][2].end(), a) == g.controlGroupsByOwner[1][2].end());
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    command = Command{};
    command.issuer = 1;
    command.payload = RecallControlGroupCommand{ 2 };
    dispatchTestCommand(command);
    assert(g.local.selectedId == c);
    assert(g.local.selectedIds.size() == 2);
    command = Command{};
    command.payload = RecallControlGroupCommand{ 2 };
    dispatchTestCommand(command);
    assert(g.local.selectedId == a);
    assert(g.local.selectedIds.size() == 2);

    command = Command{};
    command.payload = SaveCommand{ 7 };
    std::remove("realm-slot7.sav");
    dispatchTestCommand(command);
    unsigned savedSeed = g.seed;
    initGameWithSeed(1, 4803u, 0);
    command = Command{};
    command.payload = LoadCommand{ 7 };
    dispatchTestCommand(command);
    assert(g.seed == savedSeed);
    std::remove("realm-slot7.sav");

    command = Command{};
    command.payload = ResignCommand{};
    g.returnToMenu = false;
    dispatchTestCommand(command);
    assert(g.returnToMenu);
}

static void testCommandIssuerOwnerRules() {
    initGameWithSeed(1, 4804u, 0);
    for (int y = 28; y <= 36; y++) for (int x = 28; x <= 45; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }

    int mid = spawnEntity(g, E_MILITIA, 1, 30, 30);
    Entity* militia = testFindEntity(mid);
    assert(militia);
    Command command;
    command.issuer = 1;
    command.payload = MoveCommand{ Selection{ mid, { mid } }, { 35, 30 } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(militia->targetX == 35 && militia->targetY == 30);

    command = Command{};
    command.issuer = 0;
    command.payload = MoveCommand{ Selection{ mid, { mid } }, { 36, 30 } };
    assert(dispatchTestCommand(command).status == CommandStatus::Rejected);
    assert(militia->targetX == 35 && militia->targetY == 30);

    command = Command{};
    command.issuer = 0;
    command.payload = HoldPositionCommand{ Selection{ mid, { mid } } };
    assert(dispatchTestCommand(command).status == CommandStatus::Rejected);
    assert(militia->holdPosition == 0);

    command = Command{};
    command.issuer = 1;
    command.payload = HoldPositionCommand{ Selection{ mid, { mid } } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(militia->holdPosition == 1);

    command = Command{};
    command.issuer = 1;
    command.payload = StopCommand{ Selection{ mid, { mid } } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(militia->holdPosition == 0 && militia->state == S_IDLE);

    int gateId = spawnEntity(g, E_GATE, 1, 38, 30);
    Entity* gate = testFindEntity(gateId);
    assert(gate);
    command = Command{};
    command.issuer = 1;
    command.payload = ToggleGateCommand{ Selection{ gateId, { gateId } } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(gate->gateLocked && gate->gateOpen);

    int trebId = spawnEntity(g, E_TREBUCHET, 1, 42, 30);
    Entity* treb = testFindEntity(trebId);
    assert(treb && treb->packed == 1);
    command = Command{};
    command.issuer = 1;
    command.payload = ToggleTrebuchetPackedCommand{ Selection{ trebId, { trebId } } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(treb->packed == 0 && treb->packTicks == 40);

    int barracksId = spawnEntity(g, E_BARRACKS, 1, 44, 30);
    Entity* barracks = testFindEntity(barracksId);
    assert(barracks);
    command = Command{};
    command.issuer = 1;
    command.payload = SetRallyCommand{ Selection{ barracksId, { barracksId } }, { 40, 35 } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(barracks->rallySet && barracks->rallyX == 40 && barracks->rallyY == 35);

    command = Command{};
    command.issuer = 1;
    command.payload = SelectCommand{ { 30, 30 } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(g.local.selectedId == mid);

    int archerId = spawnEntity(g, E_ARCHER, 1, 31, 30);
    command = Command{};
    command.issuer = 1;
    command.payload = BoxSelectCommand{ { 29, 29 }, { 32, 31 } };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), mid) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), archerId) != g.local.selectedIds.end());

    command = Command{};
    command.issuer = 1;
    command.payload = AssignControlGroupCommand{ Selection{ mid, { mid, archerId } }, 4 };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    g.local.selectedId = -1;
    g.local.selectedIds.clear();
    command = Command{};
    command.issuer = 1;
    command.payload = RecallControlGroupCommand{ 4 };
    assert(dispatchTestCommand(command).status == CommandStatus::Accepted);
    assert(g.local.selectedId == mid);
    assert(g.local.selectedIds.size() == 2);
    assert(validateGameState(g, nullptr));
}

static void testOrderServiceContracts() {
    initGameWithSeed(1, 4861u, 0);
    for (int y = 20; y <= 45; y++) for (int x = 20; x <= 65; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }

    int workerId = spawnEntity(g, E_PEASANT, 0, 24, 24);
    int helperId = spawnEntity(g, E_PEASANT, 0, 25, 24);
    int militiaId = spawnEntity(g, E_MILITIA, 0, 26, 24);
    int archerId = spawnEntity(g, E_ARCHER, 0, 27, 24);
    int enemyId = spawnEntity(g, E_MILITIA, 1, 32, 24);
    int houseId = spawnEntity(g, E_HOUSE, 0, 34, 24);
    int towerId = spawnEntity(g, E_TOWER, 0, 38, 24);
    int gateId = spawnEntity(g, E_GATE, 0, 42, 24);
    int trebuchetId = spawnEntity(g, E_TREBUCHET, 0, 46, 24);

    WorldIndex world = buildWorldIndex(g);
    Entity* worker = findEntity(g, world, workerId);
    Entity* helper = findEntity(g, world, helperId);
    Entity* militia = findEntity(g, world, militiaId);
    Entity* archer = findEntity(g, world, archerId);
    Entity* house = findEntity(g, world, houseId);
    Entity* tower = findEntity(g, world, towerId);
    Entity* gate = findEntity(g, world, gateId);
    Entity* trebuchet = findEntity(g, world, trebuchetId);
    assert(worker && helper && militia && archer && house && tower && gate && trebuchet);
    house->underConstruction = true;
    tower->underConstruction = false;
    gate->underConstruction = false;
    trebuchet->underConstruction = false;
    TestEventSink sink;

    assert(startMove(g, world, sink, 0, Selection{ workerId, { workerId } }, { 28, 24 }).ok);
    assert(worker->state == S_MOVING && worker->targetX == 28 && worker->targetY == 24);
    assert(sink.has(GameEventType::ActionMarker));

    sink.clear();
    worker->state = S_IDLE;
    worker->path.clear();
    worker->pathIdx = 0;
    assert(appendWaypoint(g, world, sink, 0, Selection{ workerId, { workerId } }, { 29, 24 }).ok);
    assert(worker->waypoints.size() == 1);
    tickEntity(g, world, sink, *worker);
    assert(worker->state == S_MOVING && worker->targetX == 29 && worker->targetY == 24);

    sink.clear();
    assert(startAttackMove(g, world, sink, 0, Selection{ militiaId, { militiaId } }, { 30, 24 }).ok);
    assert(militia->state == S_MOVING && militia->attackMove == 1);
    assert(sink.has(GameEventType::ActionMarker) && sink.has(GameEventType::StatusMessage));

    sink.clear();
    assert(startPatrol(g, world, sink, 0, Selection{ militiaId, { militiaId } }, { 30, 25 }).ok);
    assert(militia->patrolMode);
    assert(!militia->waypoints.empty());
    assert(militia->state == S_MOVING && militia->targetX == 30 && militia->targetY == 25);

    sink.clear();
    assert(startAttack(g, world, sink, 0, Selection{ archerId, { archerId } }, enemyId).ok);
    assert(archer->state == S_ATTACKING && archer->targetId == enemyId);
    assert(sink.has(GameEventType::ActionMarker));

    sink.clear();
    g.map[28][28].terrain = T_GOLD;
    g.map[28][28].resources = 300;
    assert(startGather(g, world, sink, 0, Selection{ workerId, { workerId } }, { 28, 28 }).ok);
    assert(worker->state == S_GATHERING && worker->cargo.type == CR_GOLD);
    assert(sink.has(GameEventType::ActionMarker));

    sink.clear();
    assert(startHelp(g, world, sink, 0, Selection{ helperId, { helperId } }, houseId).ok);
    assert(helper->state == S_BUILDING && helper->targetId == houseId);
    assert(sink.has(GameEventType::StatusMessage));

    sink.clear();
    assert(startGarrison(g, world, sink, 0, Selection{ militiaId, { militiaId } }, towerId).ok);
    assert(militia->state == S_ENTERING && militia->targetId == towerId);

    sink.clear();
    assert(holdPosition(g, world, sink, 0, Selection{ archerId, { archerId } }).ok);
    assert(archer->state == S_IDLE && archer->holdPosition == 1);
    assert(sink.has(GameEventType::StatusMessage));

    sink.clear();
    assert(stopUnits(g, world, sink, 0, Selection{ archerId, { archerId } }).ok);
    assert(archer->state == S_IDLE && archer->holdPosition == 0 && archer->targetId == -1);

    sink.clear();
    assert(toggleGateMode(g, world, sink, 0, Selection{ gateId, { gateId } }).ok);
    assert(gate->gateLocked && gate->gateOpen);
    assert(sink.has(GameEventType::StatusMessage));

    sink.clear();
    assert(toggleTrebuchetPacked(g, world, sink, 0, Selection{ trebuchetId, { trebuchetId } }).ok);
    assert(trebuchet->packed == 0 && trebuchet->packTicks == 40);
    assert(sink.has(GameEventType::StatusMessage));

    int groupA = spawnEntity(g, E_MILITIA, 0, 50, 24);
    int groupB = spawnEntity(g, E_ARCHER, 0, 51, 24);
    world = buildWorldIndex(g);
    Entity* groupUnitA = findEntity(g, world, groupA);
    Entity* groupUnitB = findEntity(g, world, groupB);
    assert(groupUnitA && groupUnitB);
    sink.clear();
    assert(startMove(g, world, sink, 0, Selection{ groupA, { groupA, groupB } }, { 56, 24 }).ok);
    assert(groupUnitA->state == S_MOVING && groupUnitB->state == S_MOVING);
    assert(sink.count(GameEventType::ActionMarker) >= 2);
    assert(sink.has(GameEventType::StatusMessage));
    assert(validateGameState(g, nullptr));
}

static void testSelectionServicesUseIssuer() {
    initGameWithSeed(0, 4850u, 0);
    int humanPeasant = spawnEntity(g, E_PEASANT, 0, 24, 24);
    int firstPeasant = spawnEntity(g, E_PEASANT, 1, 30, 30);
    int secondPeasant = spawnEntity(g, E_PEASANT, 1, 31, 30);
    int militiaId = spawnEntity(g, E_MILITIA, 1, 32, 30);
    int archerId = spawnEntity(g, E_ARCHER, 1, 33, 30);
    int enemyMilitia = spawnEntity(g, E_MILITIA, 0, 34, 30);
    int townHallId = spawnEntity(g, E_TOWNHALL, 1, 36, 30);

    WorldIndex world = buildWorldIndex(g);
    Entity* selected = selectNextIdleWorker(g, world, 1, firstPeasant);
    assert(selected && selected->id == secondPeasant);
    selected = selectNextIdleWorker(g, world, 1, secondPeasant);
    assert(selected && selected->id == firstPeasant);
    assert(g.local.selectedId != humanPeasant);

    selected = selectNextUnit(g, world, 1, secondPeasant);
    assert(selected && selected->id == militiaId);
    selected = selectNextUnit(g, world, 1, archerId);
    assert(selected && selected->id == firstPeasant);

    selected = selectHomeBase(g, world, 1);
    assert(selected && selected->id == townHallId);

    int count = selectAllMilitary(g, world, 1);
    assert(count == 2);
    assert(g.local.selectedId == militiaId);
    assert(g.local.selectedIds.size() == 2);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), militiaId) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), archerId) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), enemyMilitia) == g.local.selectedIds.end());
    assert(selectionContainsMilitary(g, world, 1, Selection{ militiaId, { militiaId, archerId } }));
    assert(!selectionContainsMilitary(g, world, 0, Selection{ militiaId, { militiaId, archerId } }));

    TestEventSink selectSink;
    boxSelect(g, world, selectSink, 1, 30, 30, 33, 30);
    assert(g.local.selectedIds.size() == 2);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), militiaId) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), archerId) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), firstPeasant) == g.local.selectedIds.end());
    selectAtTile(g, world, selectSink, 1, 30, 30, true);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), firstPeasant) != g.local.selectedIds.end());
    selectAtTile(g, world, selectSink, 1, 30, 30, true);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), firstPeasant) == g.local.selectedIds.end());
    boxSelect(g, world, selectSink, 1, 30, 30, 31, 30, true);
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), firstPeasant) != g.local.selectedIds.end());
    assert(std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), secondPeasant) != g.local.selectedIds.end());

    g.local.selectedId = enemyMilitia;
    g.local.selectedIds.clear();
    assert(!beginControlGroupAssignment(g, world, 1));
    assert(!g.local.groupAssignPending);
    g.local.selectedId = militiaId;
    assert(beginControlGroupAssignment(g, world, 1));
    assert(g.local.groupAssignPending);
    assert(g.local.selectedIds.size() == 1 && g.local.selectedIds[0] == militiaId);
    clearSelection(g);
    assert(g.local.selectedId == -1 && g.local.selectedIds.empty() && !g.local.groupAssignPending);
    assert(validateGameState(g, nullptr));
}

static void testAICommandDispatch() {
    initGameWithSeed(1, 4851u, 0);
    for (int y = 28; y <= 40; y++) for (int x = 28; x <= 60; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }

    WorldIndex world = buildWorldIndex(g);
    GameContext gameContext{ g, world, gameEvents() };
    const AITuning& tuning = defaultAITuning();
    AIWorldView view = buildAIWorldView(g, world, 1, tuning);
    AIContext aiContext{ 1, gameContext, view, tuning, {}, {} };
    auto executePlanned = [&]() {
        aiContext.ctx.world = buildWorldIndex(g);
        executeAICommands(aiContext);
    };

    int pid = spawnEntity(g, E_PEASANT, 1, 30, 30);
    Entity* peasant = testFindEntity(pid);
    assert(peasant);
    g.players[1].wood = STATS[E_HOUSE].costWood;
    g.players[1].gold = STATS[E_HOUSE].costGold;
    int humanWoodBefore = g.players[0].wood;
    aiIssueBuild(aiContext, *peasant, E_HOUSE, 34, 34);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<BuildCommand>(aiContext.plannedCommands.back()));
    assert(peasant->targetId == -1);
    executePlanned();
    assert(aiContext.plannedCommands.empty());
    Entity* house = testFindEntity(peasant->targetId);
    assert(house && house->type == E_HOUSE && house->owner == 1 && house->underConstruction);
    assert(g.players[1].wood == 0);
    assert(g.players[0].wood == humanWoodBefore);

    int bid = spawnEntity(g, E_BARRACKS, 1, 42, 30);
    Entity* barracks = testFindEntity(bid);
    assert(barracks && !barracks->underConstruction);
    g.players[1].gold = STATS[E_MILITIA].costGold;
    g.players[1].wood = STATS[E_MILITIA].costWood;
    g.players[1].food = STATS[E_MILITIA].costFood;
    g.players[1].supplyMax = 50;
    aiIssueTrain(aiContext, *barracks, E_MILITIA);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<TrainCommand>(aiContext.plannedCommands.back()));
    assert(barracks->producing == E_NONE);
    executePlanned();
    assert(barracks->producing == E_MILITIA);
    assert(g.players[1].gold == 0 && g.players[1].food == 0);

    int sid = spawnEntity(g, E_BLACKSMITH, 1, 45, 30);
    Entity* smith = testFindEntity(sid);
    assert(smith && !smith->underConstruction);
    const ResearchDef* iron = researchDef(ResearchId::IronWeapons);
    assert(iron);
    g.players[1].gold = iron->costGold;
    g.players[1].wood = iron->costWood;
    aiIssueResearch(aiContext, *smith, ResearchId::IronWeapons);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<ResearchCommand>(aiContext.plannedCommands.back()));
    assert(smith->researching == 0);
    executePlanned();
    assert(smith->researching == R_IRON_WEAPONS);
    assert(g.players[1].gold == 0 && g.players[1].wood == 0);

    int aid = spawnEntity(g, E_ARCHER, 1, 48, 30);
    int enemyId = spawnEntity(g, E_MILITIA, 0, 50, 30);
    Entity* archer = testFindEntity(aid);
    assert(archer);
    aiIssueMove(aiContext, *archer, 49, 32);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<MoveCommand>(aiContext.plannedCommands.back()));
    assert(!(archer->targetX == 49 && archer->targetY == 32));
    executePlanned();
    assert(archer->targetX == 49 && archer->targetY == 32);
    aiIssueAttack(aiContext, *archer, enemyId);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<AttackCommand>(aiContext.plannedCommands.back()));
    assert(!(archer->state == S_ATTACKING && archer->targetId == enemyId));
    executePlanned();
    assert(archer->state == S_ATTACKING && archer->targetId == enemyId);

    int towerId = spawnEntity(g, E_TOWER, 1, 54, 30);
    Entity* tower = testFindEntity(towerId);
    assert(tower);
    aiIssueGarrison(aiContext, *archer, tower->id);
    executePlanned();
    assert(archer->state == S_ENTERING && archer->targetId == tower->id);
    tower->garrison.push_back(archer->id);
    aiIssueEjectGarrison(aiContext, *tower);
    executePlanned();
    assert(tower->garrison.empty());
    int aiTrebId = spawnEntity(g, E_TREBUCHET, 1, 56, 30);
    Entity* aiTreb = testFindEntity(aiTrebId);
    assert(aiTreb && aiTreb->packed == 1);
    aiIssueToggleTrebuchetPacked(aiContext, *aiTreb);
    executePlanned();
    assert(aiTreb->packed == 0 && aiTreb->packTicks == 40 && aiTreb->targetId == -1);

    int farmId = spawnEntity(g, E_FARM, 1, 58, 30);
    int tenderId = spawnEntity(g, E_PEASANT, 1, 57, 30);
    Entity* farm = testFindEntity(farmId);
    Entity* tender = testFindEntity(tenderId);
    assert(farm && tender);
    farm->underConstruction = false;
    aiContext.ctx.world = buildWorldIndex(g);
    aiIssueContext(aiContext, *tender, farm->x, farm->y);
    assert(aiContext.plannedCommands.size() == 1);
    assert(commandHasPayload<ContextCommand>(aiContext.plannedCommands.back()));
    executeAICommands(aiContext);
    assert(tender->state == S_BUILDING && tender->targetId == farm->id);

    Command invalidBuild;
    invalidBuild.issuer = 1;
    invalidBuild.payload = BuildCommand{ Selection{ enemyId, { enemyId } }, E_HOUSE, { 55, 30 } };
    Command invalidTrain;
    invalidTrain.issuer = 1;
    invalidTrain.payload = TrainCommand{ Selection{ enemyId, { enemyId } }, E_MILITIA };
    aiContext.plannedCommands.push_back(invalidBuild);
    aiContext.plannedCommands.push_back(invalidTrain);
    executeAICommands(aiContext);
    assert(aiContext.rejectedCommands.size() == 2);
    assert(aiContext.rejectedBuildCommands == 1);
    assert(aiContext.rejectedTrainCommands == 1);
    assert(!aiContext.rejectedCommands[0].result.reason.empty());
    assert(validateGameState(g, nullptr));
}

static void testAIProductionPlanningContracts() {
    initGameWithSeed(1, 4854u, 0);
    for (int y = 20; y <= 45; y++) for (int x = 20; x <= 65; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }
    int smithId = spawnEntity(g, E_BLACKSMITH, 1, 30, 30);
    int secondSmithId = spawnEntity(g, E_BLACKSMITH, 1, 34, 30);
    Entity* smith = testFindEntity(smithId);
    Entity* secondSmith = testFindEntity(secondSmithId);
    assert(smith && secondSmith && !smith->underConstruction && !secondSmith->underConstruction);
    g.players[1].gold = 5000;
    g.players[1].wood = 5000;
    g.players[1].food = 5000;
    g.players[1].supplyMax = 100;

    WorldIndex world = buildWorldIndex(g);
    TestEventSink sink;
    GameContext gameContext{ g, world, sink };
    AITuning tuning = defaultAITuning();
    AIWorldView view = buildAIWorldView(g, world, 1, tuning);
    AIContext aiContext{ 1, gameContext, view, tuning, {}, {} };
    runAIProduction(aiContext);

    int researchCommands = 0;
    for (const Command& command : aiContext.plannedCommands) {
        if (commandHasPayload<ResearchCommand>(command)) researchCommands++;
    }
    assert(researchCommands == 1);
    assert(smith->researching == 0);
    executeAICommands(aiContext);
    assert(smith->researching != 0);

    AITuning customTuning = tuning;
    customTuning.minPeasantCap = 42;
    customTuning.earlyPeasantLead = 0;
    AIWorldView tunedView = buildAIWorldView(g, world, 1, customTuning);
    assert(tunedView.peasCap >= 42);
}

static void testAIGatherUsesWorldIndex() {
    initGameWithSeed(1, 4852u, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        g.map[y][x].resources = 0;
        g.map[y][x].terrain = T_GRASS;
    }
    g.map[30][35].terrain = T_FOREST;
    g.map[30][35].resources = 80;
    g.map[31][31].terrain = T_GOLD;
    g.map[31][31].resources = 100;

    int pid = spawnEntity(g, E_PEASANT, 1, 30, 30);
    Entity* peasant = testFindEntity(pid);
    assert(peasant);
    runTestAIGather(1);
    assert(peasant->state == S_GATHERING);
    assert(peasant->targetX == 31 && peasant->targetY == 31);

    initGameWithSeed(1, 4853u, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        g.map[y][x].resources = 0;
        g.map[y][x].terrain = T_WATER;
    }
    g.map[20][22].terrain = T_FISH;
    g.map[20][22].resources = 90;
    int bid = spawnEntity(g, E_FISHING_BOAT, 1, 20, 20);
    Entity* boat = testFindEntity(bid);
    assert(boat);
    runTestAIGather(1);
    assert(boat->state == S_GATHERING);
    assert(boat->targetX == 22 && boat->targetY == 20);
    assert(validateGameState(g, nullptr));
}

static void testProductionService() {
    initGameWithSeed(1, 4901u, 0);
    int bid = spawnEntity(g, E_BARRACKS, 0, 30, 30);
    Entity* bar = testFindEntity(bid);
    assert(bar && !bar->underConstruction);
    assert(canProducerTrain(E_BARRACKS, E_MILITIA));
    assert(canProducerTrain(E_BARRACKS, E_RAM));
    assert(!canProducerTrain(E_STABLE, E_ARCHER));

    g.players[0].gold = 999;
    g.players[0].wood = 999;
    g.players[0].food = 999;
    g.players[0].supplyMax = 100;
    int goldBefore = g.players[0].gold;
    int woodBefore = g.players[0].wood;
    int foodBefore = g.players[0].food;
    TestEventSink productionSink;
    WorldIndex productionWorld = buildWorldIndex(g);
    assert(startTrainingService(g, productionWorld, productionSink, 0, bar->id, E_MILITIA).ok);
    assert(productionSink.has(GameEventType::TrainingStarted));
    assert(bar->producing == E_MILITIA);
    assert(bar->trainTime == STATS[E_MILITIA].trainTime);
    assert(g.players[0].gold == goldBefore - STATS[E_MILITIA].costGold);
    assert(g.players[0].wood == woodBefore - STATS[E_MILITIA].costWood);
    assert(g.players[0].food == foodBefore - STATS[E_MILITIA].costFood);

    // Busy producers may queue up to five units, then reject without charging.
    productionSink.clear();
    for (int i = 0; i < 5; i++) assert(startTrainingService(g, productionWorld, productionSink, 0, bar->id, E_ARCHER).ok);
    assert((int)bar->queue.size() == 5);
    assert(productionSink.count(GameEventType::TrainingQueued) == 5);
    goldBefore = g.players[0].gold;
    productionWorld = buildWorldIndex(g);
    assert(!startTrainingService(g, productionWorld, gameEvents(), 0, bar->id, E_ARCHER).ok);
    assert(g.players[0].gold == goldBefore);

    int sid = spawnEntity(g, E_STABLE, 0, 40, 30);
    Entity* stable = testFindEntity(sid);
    assert(stable);
    goldBefore = g.players[0].gold;
    productionWorld = buildWorldIndex(g);
    ServiceResult invalidTraining = startTrainingService(g, productionWorld, gameEvents(), 0, stable->id, E_ARCHER);
    assert(!invalidTraining.ok);
    assert(std::string(invalidTraining.reason) == "Cannot train that unit.");
    assert(g.players[0].gold == goldBefore);

    int bid2 = spawnEntity(g, E_BARRACKS, 0, 45, 30);
    Entity* poorBar = testFindEntity(bid2);
    assert(poorBar);
    g.players[0].gold = 999;
    g.players[0].wood = 999;
    g.players[0].food = 0;
    productionWorld = buildWorldIndex(g);
    ServiceResult trainRejected = startTrainingService(g, productionWorld, gameEvents(), 0, poorBar->id, E_MILITIA);
    assert(!trainRejected.ok);
    assert(trainRejected.reason && std::string(trainRejected.reason).size() > 0);
    assert(poorBar->producing == E_NONE);
    assert(validateGameState(g, nullptr));
}

static void testBuildService() {
    initGameWithSeed(1, 5001u, 0);
    for (int y = 30; y <= 36; y++) for (int x = 30; x <= 40; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }
    int pid = spawnEntity(g, E_PEASANT, 0, 30, 30);
    Entity* peasant = testFindEntity(pid);
    assert(peasant);
    g.players[0].gold = STATS[E_HOUSE].costGold;
    g.players[0].wood = STATS[E_HOUSE].costWood;
    TestEventSink buildSink;
    WorldIndex buildWorld = buildWorldIndex(g);
    assert(!canPlace(g, buildWorld, E_HOUSE, peasant->x, peasant->y, 0));
    assert(canPlace(g, buildWorld, E_HOUSE, peasant->x, peasant->y, 0, peasant->id));
    assert(startBuildService(g, buildWorld, buildSink, 0, peasant->id, E_HOUSE, { peasant->x, peasant->y }).ok);
    assert(buildSink.has(GameEventType::ActionMarker));
    assert(buildSink.has(GameEventType::BuildingPlaced));
    assert(g.players[0].gold == 0 && g.players[0].wood == 0);
    assert(peasant->state == S_BUILDING);
    Entity* house = testFindEntity(peasant->targetId);
    assert(house && house->type == E_HOUSE && house->owner == 0 && house->underConstruction);

    // Insufficient resources reject without spawning.
    initGameWithSeed(1, 5002u, 0);
    for (int y = 30; y <= 36; y++) for (int x = 30; x <= 40; x++) {
        g.map[y][x].terrain = T_GRASS;
        g.map[y][x].resources = 0;
    }
    int pid2 = spawnEntity(g, E_PEASANT, 0, 30, 30);
    Entity* peasant2 = testFindEntity(pid2);
    assert(peasant2);
    g.players[0].gold = 0;
    g.players[0].wood = 0;
    int housesBefore = countTypeOwner(E_HOUSE, 0);
    WorldIndex world = buildWorldIndex(g);
    ServiceResult buildRejected = startBuildService(g, world, gameEvents(), 0, peasant2->id, E_HOUSE, { 34, 34 });
    assert(!buildRejected.ok);
    assert(std::string(buildRejected.reason) == "Not enough resources!");
    assert(countTypeOwner(E_HOUSE, 0) == housesBefore);

    // Non-builders are rejected.
    int mid = spawnEntity(g, E_MILITIA, 0, 31, 30);
    Entity* militia = testFindEntity(mid);
    assert(militia);
    g.players[0].gold = 999;
    g.players[0].wood = 999;
    world = buildWorldIndex(g);
    assert(!startBuildService(g, world, gameEvents(), 0, militia->id, E_HOUSE, { 36, 34 }).ok);

    // Invalid terrain is rejected.
    g.map[34][36].terrain = T_WATER;
    world = buildWorldIndex(g);
    assert(!startBuildService(g, world, gameEvents(), 0, peasant2->id, E_HOUSE, { 36, 34 }).ok);
    assert(validateGameState(g, nullptr));
}

static void testGameEventSink() {
    initGameWithSeed(1, 5101u, 0);
    (void)drainGameEvents();
    resetUiState();
    emitUiStatusEvent(0, "Human status");
    flushGameEventsToUi(ui, 0);
    assert(ui.statusMsg == "Human status");
    emitUiStatusEvent(1, "AI status");
    flushGameEventsToUi(ui, 0);
    assert(ui.statusMsg == "Human status");
    emitUiStatusEvent(1, "AI status");
    flushGameEventsToUi(ui, 1);
    assert(ui.statusMsg == "AI status");

    size_t markersBefore = ui.actionMarkers.size();
    gameEvents().emit({ GameEventType::ActionMarker, 0, -1, { 10, 10 }, "", '!' });
    flushGameEventsToUi(ui, 0);
    assert(ui.actionMarkers.size() == markersBefore + 1);
    assert(ui.actionMarkers.back().x == 10 && ui.actionMarkers.back().y == 10);
    assert(ui.actionMarkers.back().glyph == '!');

    gameEvents().emit({ GameEventType::ActionMarker, 1, -1, { 11, 11 }, "", '?' });
    flushGameEventsToUi(ui, 0);
    assert(ui.actionMarkers.size() == markersBefore + 1);
    gameEvents().emit({ GameEventType::ActionMarker, 1, -1, { 11, 11 }, "", '?' });
    flushGameEventsToUi(ui, 1);
    assert(ui.actionMarkers.size() == markersBefore + 2);
    assert(ui.actionMarkers.back().x == 11 && ui.actionMarkers.back().y == 11);
    assert(ui.actionMarkers.back().glyph == '?');

    struct CaptureSink : EventSink {
        std::vector<GameEvent> events;
        void emit(const GameEvent& event) override { events.push_back(event); }
    } sink;
    int id = spawnEntity(g, E_MILITIA, 0, 22, 22);
    WorldIndex world = buildWorldIndex(g);
    GameContext context{ g, world, sink };
    Command move;
    move.issuer = 0;
    move.payload = MoveCommand{ Selection{ id, { id } }, { 24, 22 } };
    CommandResult acceptedResult = dispatchCommand(context, move);
    assert(acceptedResult.status == CommandStatus::Accepted);
    assert(!sink.events.empty());
    assert(std::any_of(sink.events.begin(), sink.events.end(),
        [](const GameEvent& event){ return event.type == GameEventType::ActionMarker; }));
    assert(!sink.events.empty() && sink.events.back().type == GameEventType::CommandAccepted);

    sink.events.clear();
    Command rejectedMove;
    rejectedMove.issuer = 1;
    rejectedMove.payload = MoveCommand{ Selection{ id, { id } }, { 25, 22 } };
    CommandResult rejectedResult = dispatchCommand(context, rejectedMove);
    assert(rejectedResult.status == CommandStatus::Rejected);
    assert(!sink.events.empty());
    assert(sink.events.back().type == GameEventType::CommandRejected);

    sink.events.clear();
    std::remove("realm-slot8.sav");
    Command save;
    save.issuer = 0;
    save.payload = SaveCommand{ 8 };
    CommandResult saveResult = dispatchCommand(context, save);
    assert(saveResult.status == CommandStatus::Accepted);
    assert(std::any_of(sink.events.begin(), sink.events.end(),
        [](const GameEvent& event){ return event.type == GameEventType::SaveCompleted; }));
    assert(sink.events.back().type == GameEventType::CommandAccepted);

    sink.events.clear();
    Command load;
    load.issuer = 0;
    load.payload = LoadCommand{ 8 };
    CommandResult loadResult = dispatchCommand(context, load);
    assert(loadResult.status == CommandStatus::Accepted);
    assert(std::any_of(sink.events.begin(), sink.events.end(),
        [](const GameEvent& event){ return event.type == GameEventType::LoadCompleted; }));
    assert(sink.events.back().type == GameEventType::CommandAccepted);
    std::remove("realm-slot8.sav");
}

static void testWorldIndexParity() {
    initGameWithSeed(2, 5201u, 1);
    WorldIndex world = buildWorldIndex(g);

    for (const auto& e : g.entities) {
        if (!e.alive) continue;
        assert(entityById(g, world, e.id) == testFindEntity(e.id));
        if (e.owner >= 0 && e.owner <= MAX_PLAYERS) {
            const auto& ownerList = world.entitiesByOwner[e.owner];
            assert(std::find(ownerList.begin(), ownerList.end(), e.id) != ownerList.end());
        }
    }
    assert(entityById(g, world, -12345) == nullptr);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            Entity* legacyTop = testEntityAt(x, y);
            EntityId indexedTop = topEntityAt(g, world, { x, y });
            assert((legacyTop ? legacyTop->id : -1) == indexedTop);
            assert(entityAt(g, world, x, y) == legacyTop);
            Entity* legacyOwner = testEntityAtOwner(x, y, 0);
            EntityId indexedOwner = topEntityAtOwner(g, world, { x, y }, 0);
            assert((legacyOwner ? legacyOwner->id : -1) == indexedOwner);
            assert(entityAtOwner(g, world, x, y, 0) == legacyOwner);
        }
    }

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            bool unitOccupied = false;
            bool buildingOccupied = false;
            for (const Entity& entity : g.entities) {
                if (!entity.alive || entity.state == S_GARRISONED) continue;
                bool building = isBuilding(entity.type);
                if (building && entity.type == E_GATE && entity.gateOpen) continue;
                const EntityStats& stats = STATS[entity.type];
                int w = building ? stats.sizeW : 1;
                int h = building ? stats.sizeH : 1;
                if (x < entity.x || y < entity.y || x >= entity.x + w || y >= entity.y + h) continue;
                if (building) buildingOccupied = true;
                else unitOccupied = true;
            }
            assert(isOccupied(world, { x, y }, OccupancyLayer::Units) == unitOccupied);
            assert(isOccupied(world, { x, y }, OccupancyLayer::Buildings) == buildingOccupied);
            assert(isOccupied(world, { x, y }, OccupancyLayer::Any) == (unitOccupied || buildingOccupied));
        }
    }

    int wood = 0, gold = 0, food = 0, fish = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g.map[y][x].resources <= 0) continue;
        switch (resourceForTerrain(g.map[y][x].terrain)) {
            case CR_WOOD: wood++; break;
            case CR_GOLD: gold++; break;
            case CR_FOOD: food++; break;
            case CR_FISH: fish++; break;
            default: break;
        }
    }
    assert((int)world.resources.wood.size() == wood);
    assert((int)world.resources.gold.size() == gold);
    assert((int)world.resources.food.size() == food);
    assert((int)world.resources.fish.size() == fish);

    for (EntityType type : { E_HOUSE, E_TOWER, E_FARM, E_DOCK }) {
        for (int y = 0; y < MAP_H; y += 5)
            for (int x = 0; x < MAP_W; x += 7)
                assert(canPlace(g, world, type, x, y, 0) == testCanPlace(type, x, y, 0));
    }

    auto local = std::make_unique<Game>(g);
    local->entities.clear();
    const int px = 40, py = 40;
    for (int dy = 0; dy < STATS[E_HOUSE].sizeH; dy++) for (int dx = 0; dx < STATS[E_HOUSE].sizeW; dx++) {
        g.map[py + dy][px + dx].terrain = T_WATER;
        g.map[py + dy][px + dx].resources = 0;
        local->map[py + dy][px + dx].terrain = T_GRASS;
        local->map[py + dy][px + dx].resources = 0;
    }
    assert(!isPassable(g, px, py));
    assert(isPassable(*local, px, py));
    auto localPath = findPath(*local, px, py, px + 1, py, 300, false);
    assert(!localPath.empty());
    assert(localPath.back().first == px + 1 && localPath.back().second == py);
    int localMoverId = spawnEntity(*local, E_PEASANT, 0, px, py);
    WorldIndex localMoveWorld = buildWorldIndex(*local);
    Entity* localMover = findEntity(*local, localMoveWorld, localMoverId);
    assert(localMover);
    assert(startMove(*local, localMoveWorld, gameEvents(), localMover->owner, Selection{ localMover->id, { localMover->id } }, { px + 1, py }).ok);
    assert(localMover->state == S_MOVING);
    assert(!localMover->path.empty());
    assert(localMover->path.back().first == px + 1 && localMover->path.back().second == py);
    localMoveWorld = buildWorldIndex(*local);
    moveAlongPath(*local, localMoveWorld, *localMover);
    assert(localMover->x == px + 1 && localMover->y == py);
    assert(local->map[py][px + 1].wear == 1);
    assert(g.map[py][px + 1].terrain == T_WATER);
    local->entities.clear();
    WorldIndex localWorld = buildWorldIndex(*local);
    assert(canPlace(*local, localWorld, E_HOUSE, px, py, 0));
    spawnEntity(*local, E_HOUSE, 0, px, py);
    localWorld = buildWorldIndex(*local);
    for (int dy = 0; dy < STATS[E_HOUSE].sizeH; dy++) {
        for (int dx = 0; dx < STATS[E_HOUSE].sizeW; dx++) {
            assert(isOccupied(localWorld, { px + dx, py + dy }, OccupancyLayer::Buildings));
        }
    }
    for (int dy = 0; dy < STATS[E_HOUSE].sizeH; dy++) for (int dx = 0; dx < STATS[E_HOUSE].sizeW; dx++) {
        g.map[py + dy][px + dx].terrain = T_GRASS;
        local->map[py + dy][px + dx].terrain = T_WATER;
    }
    localWorld = buildWorldIndex(*local);
    assert(!canPlace(*local, localWorld, E_HOUSE, px, py, 0));

    int corpseId = spawnEntity(g, E_MILITIA, 0, 20, 20);
    Entity* corpse = testFindEntity(corpseId);
    assert(corpse);
    corpse->alive = false;
    corpse->state = S_DEAD;
    corpse->hp = 0;
    WorldIndex corpseWorld = buildWorldIndex(g);
    assert(corpseAt(g, corpseWorld, 20, 20) == testCorpseAt(20, 20));
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
    WorldIndex berryWorld = buildWorldIndex(g);
    assert(startGather(g, berryWorld, gameEvents(), peasant->owner, Selection{ peasant->id, { peasant->id } }, { bx, by }).ok);
    assert(peasant->cargo.type == CR_FOOD);
    assert(peasant->resourceX == bx && peasant->resourceY == by);
    bool depleted = false, delivered = false;
    for (int i = 0; i < 300; i++) {
        tickSimulationOnce(g, gameEvents(), true);
        if (g.map[by][bx].terrain == T_GRASS && g.map[by][bx].resources == 0) depleted = true;
        if (g.players[0].food > foodBefore) delivered = true;
        if (depleted && delivered) break;
    }
    assert(depleted);
    assert(delivered);
    peasant = testFindEntity(peasantId);
    assert(peasant && peasant->cargo.amount == 0);
}

static void testEntityAnimationSpecs() {
    Game animationGame{};
    WorldIndex animationWorld = buildWorldIndex(animationGame);
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
    assert(std::string(entityAnimationActionId(animationGame, animationWorld, e)) == "idle");
    assert(entityActionAnimationSpecFor(animationGame, animationWorld, e)->holdLast);
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
    assert(std::string(entityAnimationActionId(animationGame, animationWorld, e)) == "walk");
    assert(std::string(entityAnimationDirectionBucket(e)) == "front");
    assert(!entityAnimationMirrorHorizontal(e));

    e.state = S_RETURNING;
    e.path.clear();
    e.cargo = {CR_WOOD, 10, 3, 4};
    assert(std::string(entityAnimationActionId(animationGame, animationWorld, e)) == "carry_wood");

    e.alive = false;
    assert(std::string(entityAnimationActionId(animationGame, animationWorld, e)) == "death");

    Entity m{};
    m.type = E_MILITIA;
    m.alive = false;
    m.state = S_DEAD;
    assert(std::string(entityAnimationActionId(animationGame, animationWorld, m)) == "death");
    assert(entityActionAnimationSpecFor(animationGame, animationWorld, m) == militiaDeath);
}

static void testCorpseDecayLifecycle() {
    initGameWithSeed(0, 3903u, 0);
    int id = spawnEntity(g, E_MILITIA, 0, 20, 20);
    Entity* militia = testFindEntity(id);
    assert(militia);
    militia->alive = false;
    militia->state = S_DEAD;
    militia->hp = 0;
    militia->deathTicks = 0;

    assert(testFindEntity(id) == nullptr);
    assert(testEntityAt(20, 20) == nullptr);
    assert(testCorpseAt(20, 20) == militia);

    for (int i = 0; i < DEATH_DECAY_TICKS; i++) tickSimulationOnce(g, gameEvents(), true);
    Entity* decayed = testCorpseAt(20, 20);
    assert(decayed && decayed->deathTicks >= DEATH_DECAY_TICKS);

    for (int i = DEATH_DECAY_TICKS; i < CORPSE_REMOVE_TICKS; i++) tickSimulationOnce(g, gameEvents(), true);
    assert(testCorpseAt(20, 20) == nullptr);
}

static void testMillFoodStockpile() {
    initGameWithSeed(1, 3603u, 0);
    Entity* mill = nullptr;
    for (int y = 20; y < MAP_H - 5 && !mill; y++) {
        for (int x = 20; x < MAP_W - 5 && !mill; x++) {
            if (!testCanPlace(E_MILL, x, y, 0)) continue;
            int id = spawnEntity(g, E_MILL, 0, x, y);
            mill = testFindEntity(id);
        }
    }
    assert(mill);
    int foodBefore = g.players[0].food;
    addPlayerFood(g, 0, 40, mill);
    assert(g.players[0].food == foodBefore + 40);
    assert(mill->storedFood == 40);
    spendPlayerFood(g, 0, 15);
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
    tickSeasons(g, gameEvents());
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
        assert(validateMapInvariants(g, nullptr));
        int playerStarts = 0;
        std::set<std::pair<int,int>> basePositions;
        for (const auto& e : g.entities) {
            if (!e.alive || e.type != E_TOWNHALL || e.owner >= MAX_PLAYERS) continue;
            basePositions.insert({e.x, e.y});
            int sx = e.x + STATS[e.type].sizeW + 1;
            int sy = e.y + STATS[e.type].sizeH + 1;
            if (!isPassable(g, sx, sy)) { sx = e.x + STATS[e.type].sizeW + 2; sy = e.y + 1; }
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

static void testMapGenerationConfigBiomes() {
    for (int biome = B_TEMPERATE; biome <= B_OCEAN; biome++) {
        MapGenerationConfig config;
        config.biomeChoice = biome;
        initGameWithSeed(2, 6000u + (unsigned)biome, biome % 4, config);
        assert(g.biomeChoice == biome);
        assert(validateMapInvariants(g, nullptr));
        int matchingBiomeTiles = 0;
        int waterishTiles = 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            if (g.map[y][x].biome == biome) matchingBiomeTiles++;
            Terrain terrain = g.map[y][x].terrain;
            if (terrainDef(terrain).passableWater) waterishTiles++;
        }
        if (biome == B_OCEAN) assert(waterishTiles > (MAP_W * MAP_H) / 3);
        else assert(matchingBiomeTiles > 0);
    }
    g.biomeChoice = B_TEMPERATE;
}

static void testLocalMapGenerationIsDeterministic() {
    MapGenerationConfig config;
    config.biomeChoice = B_FOREST;
    Game first{};
    Game second{};
    realmSrand(first, 7101u);
    realmSrand(second, 7101u);

    generateMap(first, config);
    generateMap(second, config);

    assert(first.biomeChoice == B_FOREST);
    assert(second.biomeChoice == B_FOREST);
    assert(validateMapInvariants(first, nullptr));
    assert(validateMapInvariants(second, nullptr));
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        assert(first.map[y][x].terrain == second.map[y][x].terrain);
        assert(first.map[y][x].resources == second.map[y][x].resources);
        assert(first.map[y][x].biome == second.map[y][x].biome);
        assert(first.map[y][x].preWinterTerrain == second.map[y][x].preWinterTerrain);
    }
}

static void testStartSafetyAcrossSeeds() {
    for (unsigned seed = 1; seed <= 60; seed++) {
        initGameWithSeed(3, seed, (int)(seed % 4));
        assert(validateMapInvariants(g, nullptr));
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
    for (int i = 0; i < 20; i++) tickSimulationOnce(g, gameEvents(), true);
    std::string before = startupSummary();
    assert(saveGame(g, "build/test-save.realm"));
    {
        std::ifstream in("build/test-save.realm");
        std::string tag;
        int version = 0;
        assert(in >> tag >> version);
        assert(tag == "REALM_SAVE");
        assert(version == REALM_SAVE_VERSION);
    }
    SaveHeaderInfo currentHeader = inspectSaveHeader("build/test-save.realm");
    assert(currentHeader.ok);
    assert(currentHeader.version == REALM_SAVE_VERSION);
    initGameWithSeed(1, 9999u, 0);
    assert(loadGame(g, "build/test-save.realm"));
    assert(g.seed == 4004u);
    assert(startupSummary() == before);
    for (int i = 0; i < 20; i++) tickSimulationOnce(g, gameEvents(), true);

    // Version 8 uses the pre-waypoint payload and should migrate in place.
    {
        std::ifstream in("build/test-save.realm", std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        size_t pos = content.find("REALM_SAVE 10");
        assert(pos != std::string::npos);
        content.replace(pos, std::string("REALM_SAVE 10").size(), "REALM_SAVE 8");
        while ((pos = content.find(" WAYPOINTS ", pos)) != std::string::npos) {
            size_t lineEnd = content.find('\n', pos);
            assert(lineEnd != std::string::npos);
            content.erase(pos, lineEnd - pos);
        }
        std::ofstream out("build/test-save-v8.realm", std::ios::binary);
        out << content;
    }
    initGameWithSeed(1, 9998u, 0);
    assert(loadGame(g, "build/test-save-v8.realm"));
    assert(g.seed == 4004u);

    initGameWithSeed(2, 4444u, 1);
    for (int i = 0; i < 250; i++) tickSimulationOnce(g, gameEvents(), true);
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != 0 || !isUnit(e.type) || isNaval(e.type)) continue;
        e.state = S_IDLE;
        e.path.clear();
        e.pathIdx = 0;
        e.waypoints = {{e.x + 1, e.y}, {e.x, e.y}};
        e.patrolMode = true;
        break;
    }
    assert(saveGame(g, "build/exact-resume.realm"));
    for (int i = 0; i < 100; i++) tickSimulationOnce(g, gameEvents(), true);
    std::string continuous = fullStateSummary();
    assert(loadGame(g, "build/exact-resume.realm"));
    for (int i = 0; i < 100; i++) tickSimulationOnce(g, gameEvents(), true);
    assert(fullStateSummary() == continuous);

    FILE* f = std::fopen("build/corrupt-save.realm", "wb");
    assert(f);
    std::fputs("not a realm save\n", f);
    std::fclose(f);
    std::string beforeFailedLoad = fullStateSummary();
    assert(!loadGame(g, "build/corrupt-save.realm"));
    assert(fullStateSummary() == beforeFailedLoad);

    FILE* old = std::fopen("build/unsupported-save.realm", "wb");
    assert(old);
    std::fputs("REALM_SAVE 7\n", old);
    std::fclose(old);
    SaveHeaderInfo oldHeader = inspectSaveHeader("build/unsupported-save.realm");
    assert(!oldHeader.ok);
    assert(oldHeader.version == 7);
    assert(oldHeader.error.find("Unsupported save version 7") != std::string::npos);
    WorldIndex world = buildWorldIndex(g);
    GameContext context{ g, world, gameEvents() };
    SaveLoadResult unsupported = loadGameService(context, { "build/unsupported-save.realm", 0, 0 });
    assert(!unsupported.ok);
    assert(unsupported.error.find("Unsupported save version 7") != std::string::npos);

    struct CaptureSink : EventSink {
        std::vector<GameEvent> events;
        void emit(const GameEvent& event) override { events.push_back(event); }
    } sink;
    initGameWithSeed(1, 5005u, 0);
    unsigned globalSeedBeforeLocalService = g.seed;
    auto localGame = std::make_unique<Game>(g);
    WorldIndex localWorld = buildWorldIndex(*localGame);
    GameContext localContext{ *localGame, localWorld, sink };
    SaveLoadResult localSave = saveGameService(localContext, { "build/local-context.realm", 0, 0 });
    assert(localSave.ok);
    assert(localGame->seed == 5005u);
    assert(g.seed == globalSeedBeforeLocalService);
    assert(!sink.events.empty() && sink.events.back().type == GameEventType::SaveCompleted);
    localGame->seed = 42u;
    SaveLoadResult localLoad = loadGameService(localContext, { "build/local-context.realm", 0, 0 });
    assert(localLoad.ok);
    assert(localGame->seed == 5005u);
    assert(g.seed == globalSeedBeforeLocalService);
    assert(!sink.events.empty() && sink.events.back().type == GameEventType::LoadCompleted);
    Game recoverableSave = *localGame;
    recoverableSave.local.selectedId = recoverableSave.nextId + 99;
    assert(saveGame(recoverableSave, "build/local-recoverable-save.realm"));
    localGame->seed = 123u;
    localGame->local.selectedId = 12345;
    assert(loadGame(*localGame, "build/local-recoverable-save.realm"));
    assert(localGame->seed == 5005u);
    assert(localGame->local.selectedId == -1);
    assert(g.seed == globalSeedBeforeLocalService);

    Game hardInvalidSave = *localGame;
    assert(!hardInvalidSave.entities.empty());
    hardInvalidSave.entities.front().x = -999;
    assert(saveGame(hardInvalidSave, "build/local-hard-invalid-save.realm"));
    Game localBeforeHardLoad = *localGame;
    assert(!loadGame(*localGame, "build/local-hard-invalid-save.realm"));
    assert(localGame->seed == localBeforeHardLoad.seed);
    assert(localGame->nextId == localBeforeHardLoad.nextId);
    assert(localGame->entities.size() == localBeforeHardLoad.entities.size());
    assert(localGame->local.selectedId == localBeforeHardLoad.local.selectedId);
    assert(g.seed == globalSeedBeforeLocalService);

    localGame->seed = 777u;
    assert(!loadGame(*localGame, "build/corrupt-save.realm"));
    assert(localGame->seed == 777u);
    assert(g.seed == globalSeedBeforeLocalService);
}

static void testLongSimulationAndAIProgression() {
    initGameWithSeed(1, 5005u, 0);
    int initialAiSupplyMax = g.players[1].supplyMax;
    int longTicks = 10000;
    if (const char* env = std::getenv("REALM_TEST_LONG_TICKS")) {
        int requested = std::atoi(env);
        if (requested > 0) longTicks = requested;
    }
    for (int i = 0; i < longTicks; i++) tickSimulationOnce(g, gameEvents(), true);
    assert(!g.entities.empty());
    assert(g.players[1].supplyMax >= initialAiSupplyMax);
    assert(g.players[1].supplyMax > 10 || countTypeOwner(E_HOUSE, 1) > 0 || countTypeOwner(E_BARRACKS, 1) > 0);
    assertIdsCoherent();
}

static void testMobileStopDispatchesCommand() {
    initGameWithSeed(1, 5210u, 0);
    int workerId = spawnEntity(g, E_PEASANT, 0, 24, 24);
    WorldIndex world = buildWorldIndex(g);
    Entity* worker = findEntity(g, world, workerId);
    assert(worker);

    TestEventSink setupSink;
    assert(startMove(g, world, setupSink, 0, Selection{ workerId, { workerId } }, { 30, 24 }).ok);
    assert(worker->state == S_MOVING);
    g.local.selectedId = workerId;
    g.local.selectedIds = { workerId };

    drainGameEvents();
    handleMobileHudButton("stop", world);
    std::vector<GameEvent> events = drainGameEvents();

    assert(worker->state == S_IDLE);
    assert(worker->targetId == -1);
    assert(worker->path.empty());
    assert(worker->pathIdx == 0);
    assert(worker->attackMove == 0);
    assert(std::any_of(events.begin(), events.end(), [](const GameEvent& event) {
        return event.type == GameEventType::UnitOrdered && event.message == "Stopped.";
    }));
    assert(!events.empty() && events.back().type == GameEventType::CommandAccepted);
}

static void testSimulationTickWorldIndexBuildBudget() {
    initGameWithSeed(2, 5211u, 0);
    for (int i = 0; i < 18; i++) {
        spawnEntity(g, E_MILITIA, i % 2, 24 + i, 24 + (i / 6));
    }
    resetWorldIndexBuildCount();
    tickSimulationOnce(g, gameEvents(), false);
    assert(worldIndexBuildCount() <= 6);
}

int main() {
    testEntityAnimationSpecs();
    testCorpseDecayLifecycle();
    testVisualTileBridgeMappings();
    testWeatherSightAndForestMovementPenalties();
    testVisualEntityStateSelectors();
    testRenderModelBuildsViewport();
    testLocalGameRng();
    testAnimalCarcassHarvesting();
    testPlacementBoundsAndStateNames();
    testTraits();
    testCommandBindings();
    testInputIntentMapping();
    testViewStateHelpers();
    testInputModeController();
    testInputFeedbackHelper();
    testRecoverableValidation();
    testMatchResetAndDeterminism();
    testSupplyAndTownHallCost();
    testTownHallTrainInputFlow();
    testHoldPositionInput();
    testWallLineBuild();
    testResearchService();
    testUnitFoodCostTable();
    testMarketTradeService();
    testCommandSelectionDriftProtection();
    testContextResolverProducesTypedCommands();
    testCommandPayloadDispatchCoverage();
    testCommandDispatcherAppActions();
    testCommandIssuerOwnerRules();
    testOrderServiceContracts();
    testSelectionServicesUseIssuer();
    testAICommandDispatch();
    testAIProductionPlanningContracts();
    testAIGatherUsesWorldIndex();
    testProductionService();
    testBuildService();
    testGameEventSink();
    testWorldIndexParity();
    testMobileStopDispatchesCommand();
    testSimulationTickWorldIndexBuildBudget();
    testBerryGatherAndDepletion();
    testMillFoodStockpile();
    testWinterPartialWaterFreeze();
    testMapGenerationConfigBiomes();
    testLocalMapGenerationIsDeterministic();
    testStartSafetyAcrossSeeds();
    testMapgenReachabilityAcrossSeeds();
    testSaveLoadRoundTrip();
    testLongSimulationAndAIProgression();
    std::puts("realm_headless_tests: ok");
    return 0;
}

