#include "realm.h"
#include "core/game_events.h"
#include "core/world_index.h"

void setStatus(const std::string& msg) { emitStatusEvent(-1, msg); }

void addActionMarker(int x, int y, char glyph) {
    emitActionMarkerEvent(-1, { x, y }, glyph);
}

void addPlayerFood(int owner, int amount, Entity* depot) {
    addPlayerFood(g, owner, amount, depot);
}

void addPlayerFood(Game& game, int owner, int amount, Entity* depot) {
    if (owner < 0 || owner >= MAX_PLAYERS || amount <= 0) return;
    game.players[owner].food += amount;
    if (depot && depot->type == E_MILL) depot->storedFood += amount;
}

void spendPlayerFood(int owner, int amount) {
    spendPlayerFood(g, owner, amount);
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

Entity* findEntity(int id) {
    for (auto& e : g.entities) if (e.id == id && e.alive) return &e;
    return nullptr;
}

Entity* findEntity(Game& game, const WorldIndex& world, int id) {
    return entityById(game, world, id);
}

Entity* findDepot(Entity& e) {
    WorldIndex world = buildWorldIndex(g);
    return findDepot(g, world, e);
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

Entity* entityAt(int x, int y) {
    for (auto& e : g.entities) {
        if (!e.alive || e.state == S_GARRISONED) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

Entity* entityAt(Game& game, const WorldIndex& world, int x, int y) {
    EntityId id = topEntityAt(game, world, { x, y });
    return id >= 0 ? entityById(game, world, id) : nullptr;
}

Entity* entityAtOwner(int x, int y, int owner) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner || e.state == S_GARRISONED) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

Entity* entityAtOwner(Game& game, const WorldIndex& world, int x, int y, int owner) {
    EntityId id = topEntityAtOwner(game, world, { x, y }, owner);
    return id >= 0 ? entityById(game, world, id) : nullptr;
}

Entity* corpseAt(int x, int y) {
    Entity* found = nullptr;
    for (auto& e : g.entities) {
        if (e.alive || e.state != S_DEAD || !isUnit(e.type) || isBuilding(e.type)) continue;
        if (e.x == x && e.y == y) found = &e;
    }
    return found;
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

void buildOccupancyGrid(OccupancyGrid& grid, bool includeUnits, bool includeBuildings, int ignoreEntityId) {
    buildOccupancyGrid(g, grid, includeUnits, includeBuildings, ignoreEntityId);
}

void buildOccupancyGrid(const Game& game, OccupancyGrid& grid, bool includeUnits, bool includeBuildings, int ignoreEntityId) {
    memset(grid.occupied, 0, sizeof(grid.occupied));
    for (const auto& e : game.entities) {
        if (!e.alive || e.state == S_GARRISONED || e.id == ignoreEntityId) continue;
        bool building = isBuilding(e.type);
        if ((building && !includeBuildings) || (!building && !includeUnits)) continue;
        if (e.type == E_GATE && e.gateOpen && includeBuildings) continue;
        const EntityStats& s = STATS[e.type];
        int w = building ? s.sizeW : 1;
        int h = building ? s.sizeH : 1;
        for (int dy = 0; dy < h; dy++) for (int dx = 0; dx < w; dx++) {
            int x = e.x + dx, y = e.y + dy;
            if (inBounds(x, y)) grid.occupied[y][x] = true;
        }
    }
}

bool isOccupied(const OccupancyGrid& grid, int x, int y) {
    return inBounds(x, y) && grid.occupied[y][x];
}

static bool isLandPassableFor(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    Terrain t = game.map[y][x].terrain;
    return t != T_MOUNTAIN && t != T_WATER && t != T_STONE && t != T_CASTLE_WALL
        && t != T_FISH && t != T_LAVA;
}

static bool isWaterPassableFor(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    Terrain t = game.map[y][x].terrain;
    return t == T_WATER || t == T_SHALLOWS || t == T_REEDS || t == T_FISH;
}

bool canPlace(const Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner) {
    (void)owner;
    // Top-level bounds check protects every g.map read below, including the
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
        Terrain ter = game.map[ny][nx].terrain;
        if (ter == T_GOLD) return false;
        // Forests are resource terrain — chop the trees before you can build here.
        if (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) return false;
        if (ter==T_SHALLOWS||ter==T_MARSH||ter==T_REEDS||ter==T_ICE) return false;
        if (isOccupied(world, { nx, ny }, OccupancyLayer::Any)) return false;
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

bool canPlace(EntityType type, int x, int y, int owner) {
    WorldIndex world = buildWorldIndex(g);
    return canPlace(g, world, type, x, y, owner);
}
