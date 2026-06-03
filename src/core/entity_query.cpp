#include "realm.h"
#include "core/game_events.h"
#include "core/world_index.h"

void addPlayerFood(Game& game, int owner, int amount, Entity* depot) {
    if (owner < 0 || owner >= MAX_PLAYERS || amount <= 0) return;
    game.players[owner].food += amount;
    if (depot && depot->type == E_MILL) depot->storedFood += amount;
}

void spendPlayerFood(Game& game, int owner, int amount) {
    if (owner < 0 || owner >= MAX_PLAYERS || amount <= 0) return;
    Player& p = game.players[owner];
    int spent = std::min(amount, p.food);
    p.food -= spent;
    int remaining = spent;
    for (auto& e : game.entities) {
        if (remaining <= 0) break;
        if (!e.alive || e.owner != owner || e.type != E_MILL || e.underConstruction) continue;
        int take = std::min(e.storedFood, remaining);
        e.storedFood -= take;
        remaining -= take;
    }
}

Entity* findEntity(Game& game, const WorldIndex& world, int id) {
    return entityById(game, world, id);
}

Entity* findDepot(Game& game, const WorldIndex& world, Entity& e) {
    Entity* best = nullptr; int bestD = 99999;
    if (e.owner < 0 || e.owner > MAX_PLAYERS) return nullptr;
    for (EntityId id : world.entitiesByOwner[e.owner]) {
        Entity* o = entityById(game, world, id);
        if (!o) continue;
        if (!o->alive || o->owner != e.owner || o->underConstruction) continue;
        bool isBase = (o->type == E_TOWNHALL || o->type == E_CASTLE) && e.cargo.type != CR_FISH;
        bool isWood = (o->type == E_LUMBER_CAMP && e.cargo.type == CR_WOOD);
        bool isGold = (o->type == E_MINING_CAMP && e.cargo.type == CR_GOLD);
        bool isFish = (o->type == E_DOCK        && e.cargo.type == CR_FISH);
        // Mill accepts food deliveries from farm couriers (and berry pickers).
        bool isFood = (o->type == E_MILL        && e.cargo.type == CR_FOOD);
        if (isBase || isWood || isGold || isFish || isFood) {
            int d = mdist(e.x, e.y, o->x, o->y);
            if (d < bestD) { bestD = d; best = o; }
        }
    }
    return best;
}

Entity* entityAt(Game& game, const WorldIndex& world, int x, int y) {
    EntityId id = topEntityAt(game, world, { x, y });
    return id >= 0 ? entityById(game, world, id) : nullptr;
}

Entity* entityAtOwner(Game& game, const WorldIndex& world, int x, int y, int owner) {
    EntityId id = topEntityAtOwner(game, world, { x, y }, owner);
    return id >= 0 ? entityById(game, world, id) : nullptr;
}

Entity* corpseAt(Game& game, const WorldIndex& world, int x, int y) {
    Entity* found = nullptr;
    for (EntityId id : entitiesAt(world, { x, y })) {
        Entity* entity = entityByIdAny(game, world, id);
        if (!entity || entity->alive || entity->state != S_DEAD || !isUnit(entity->type) || isBuilding(entity->type)) continue;
        found = entity;
    }
    return found;
}

static bool isLandPassableFor(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    return terrainDef(game.map[y][x].terrain).passableLand;
}

static bool isWaterPassableFor(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    return terrainDef(game.map[y][x].terrain).passableWater;
}

bool canPlace(const Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner, int ignoreEntityId) {
    (void)owner;
    // Top-level bounds check protects every map read below, including the
    // farm-only terrain read that previously ran before any inBounds check.
    if (!inBounds(x, y)) return false;
    // Farms can only be sown on open ground, not in winter
    if (type == E_FARM) {
        if ((Season)((int)game.seasonPhase % 4) == WINTER) return false;
        Terrain t = game.map[y][x].terrain;
        if (t!=T_GRASS&&t!=T_MEADOW&&t!=T_TALL_GRASS&&t!=T_FLOWERS&&t!=T_DIRT&&t!=T_WHEAT&&t!=T_SNOW) return false;
    }
    auto& s = STATS[type];
    for (int dy = 0; dy < s.sizeH; dy++) for (int dx = 0; dx < s.sizeW; dx++) {
        int nx = x+dx, ny = y+dy;
        if (!inBounds(nx,ny) || !isLandPassableFor(game, nx, ny)) return false;
        if (!terrainDef(game.map[ny][nx].terrain).buildable) return false;
        if (isOccupied(world, { nx, ny }, OccupancyLayer::Buildings)) return false;
        if (isOccupied(world, { nx, ny }, OccupancyLayer::Units)) {
            bool blocked = false;
            for (EntityId id : entitiesAt(world, { nx, ny })) {
                if (id == ignoreEntityId) continue;
                const Entity* entity = entityById(game, world, id);
                if (entity && !isBuilding(entity->type)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) return false;
        }
    }
    // Docks must sit on the shoreline — at least one neighbouring tile must be water.
    if (type == E_DOCK) {
        bool touchesWater = false;
        for (int dy = -1; dy <= s.sizeH && !touchesWater; dy++)
            for (int dx = -1; dx <= s.sizeW && !touchesWater; dx++) {
                if (dx >= 0 && dx < s.sizeW && dy >= 0 && dy < s.sizeH) continue;
                int nx = x+dx, ny = y+dy;
                if (inBounds(nx,ny) && isWaterPassableFor(game, nx, ny)) touchesWater = true;
            }
        if (!touchesWater) return false;
    }
    return true;
}

bool canPlace(const Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner) {
    return canPlace(game, world, type, x, y, owner, -1);
}
