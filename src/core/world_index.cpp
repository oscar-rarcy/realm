#include "world_index.h"

#include <cstring>

static void indexResourceTile(WorldIndex& world, const Tile& tile, int x, int y) {
    if (tile.resources <= 0) return;
    CargoResource type = resourceForTerrain(tile.terrain);
    ResourceTile resource{ { x, y }, type, tile.resources };
    switch (type) {
        case CR_WOOD: world.resources.wood.push_back(resource); break;
        case CR_GOLD: world.resources.gold.push_back(resource); break;
        case CR_FOOD: world.resources.food.push_back(resource); break;
        case CR_FISH: world.resources.fish.push_back(resource); break;
        default: break;
    }
}

static void indexEntityTile(WorldIndex& world, const Entity& entity, bool building, bool blocksOccupancy, int x, int y) {
    if (!inBounds(x, y)) return;
    world.entitiesByTile[tileKey(x, y)].push_back(entity.id);
    if (!blocksOccupancy) return;
    if (building) world.buildingOccupancy.occupied[y][x] = true;
    else world.unitOccupancy.occupied[y][x] = true;
}

WorldIndex buildWorldIndex(const Game& game) {
    WorldIndex world;
    std::memset(world.unitOccupancy.occupied, 0, sizeof(world.unitOccupancy.occupied));
    std::memset(world.buildingOccupancy.occupied, 0, sizeof(world.buildingOccupancy.occupied));

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            indexResourceTile(world, game.map[y][x], x, y);

    for (size_t i = 0; i < game.entities.size(); i++) {
        const Entity& entity = game.entities[i];
        if (!entity.alive) continue;
        world.entityIndexById[entity.id] = i;

        if (entity.owner >= 0 && entity.owner <= MAX_PLAYERS) {
            world.entitiesByOwner[entity.owner].push_back(entity.id);
            if (isBuilding(entity.type)) world.buildingsByOwner[entity.owner].push_back(entity.id);
            else world.unitsByOwner[entity.owner].push_back(entity.id);
        }

        if (entity.state == S_GARRISONED) continue;
        bool building = isBuilding(entity.type);
        bool blocksOccupancy = !(entity.type == E_GATE && entity.gateOpen && building);
        const EntityStats& stats = STATS[entity.type];
        int w = building ? stats.sizeW : 1;
        int h = building ? stats.sizeH : 1;
        for (int dy = 0; dy < h; dy++)
            for (int dx = 0; dx < w; dx++)
                indexEntityTile(world, entity, building, blocksOccupancy, entity.x + dx, entity.y + dy);
    }

    return world;
}

Entity* entityById(Game& game, const WorldIndex& world, EntityId id) {
    auto it = world.entityIndexById.find(id);
    if (it == world.entityIndexById.end() || it->second >= game.entities.size()) return nullptr;
    Entity& entity = game.entities[it->second];
    return entity.alive && entity.id == id ? &entity : nullptr;
}

const Entity* entityById(const Game& game, const WorldIndex& world, EntityId id) {
    auto it = world.entityIndexById.find(id);
    if (it == world.entityIndexById.end() || it->second >= game.entities.size()) return nullptr;
    const Entity& entity = game.entities[it->second];
    return entity.alive && entity.id == id ? &entity : nullptr;
}

std::vector<EntityId> entitiesAt(const WorldIndex& world, MapPos tile) {
    if (!inBounds(tile.x, tile.y)) return {};
    auto it = world.entitiesByTile.find(tileKey(tile.x, tile.y));
    if (it == world.entitiesByTile.end()) return {};
    return it->second;
}

EntityId topEntityAt(const Game& game, const WorldIndex& world, MapPos tile) {
    for (EntityId id : entitiesAt(world, tile)) {
        const Entity* entity = entityById(game, world, id);
        if (entity) return id;
    }
    return -1;
}

EntityId topEntityAtOwner(const Game& game, const WorldIndex& world, MapPos tile, PlayerId owner) {
    for (EntityId id : entitiesAt(world, tile)) {
        const Entity* entity = entityById(game, world, id);
        if (entity && entity->owner == owner) return id;
    }
    return -1;
}

bool isOccupied(const WorldIndex& world, MapPos tile, OccupancyLayer layer) {
    if (!inBounds(tile.x, tile.y)) return false;
    bool unit = world.unitOccupancy.occupied[tile.y][tile.x];
    bool building = world.buildingOccupancy.occupied[tile.y][tile.x];
    switch (layer) {
        case OccupancyLayer::Units: return unit;
        case OccupancyLayer::Buildings: return building;
        case OccupancyLayer::Any: return unit || building;
    }
    return false;
}
