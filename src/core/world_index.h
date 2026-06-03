#pragma once

#include "core/game_types.h"
#include "core/types.h"

#include <array>
#include <unordered_map>
#include <vector>

struct Entity;
struct Game;

inline int tileKey(int x, int y) {
    return y * MAP_W + x;
}

struct ResourceTile {
    MapPos pos{ -1, -1 };
    CargoResource type = CR_NONE;
    int amount = 0;
};

struct ResourceIndex {
    std::vector<ResourceTile> wood;
    std::vector<ResourceTile> gold;
    std::vector<ResourceTile> food;
    std::vector<ResourceTile> fish;
};

enum class OccupancyLayer {
    Units,
    Buildings,
    Any,
};

struct WorldIndex {
    std::unordered_map<EntityId, size_t> entityIndexById;
    std::array<std::vector<EntityId>, MAX_PLAYERS + 1> entitiesByOwner;
    std::array<std::vector<EntityId>, MAX_PLAYERS + 1> unitsByOwner;
    std::array<std::vector<EntityId>, MAX_PLAYERS + 1> buildingsByOwner;
    std::unordered_map<int, std::vector<EntityId>> entitiesByTile;
    WorldOccupancyGrid unitOccupancy{};
    WorldOccupancyGrid buildingOccupancy{};
    ResourceIndex resources;
};

WorldIndex buildWorldIndex(const Game& game);

#ifdef REALM_ENABLE_WORLD_INDEX_STATS
void resetWorldIndexBuildCount();
int worldIndexBuildCount();
#endif

Entity* entityById(Game& game, const WorldIndex& world, EntityId id);
const Entity* entityById(const Game& game, const WorldIndex& world, EntityId id);
Entity* entityByIdAny(Game& game, const WorldIndex& world, EntityId id);
const Entity* entityByIdAny(const Game& game, const WorldIndex& world, EntityId id);

std::vector<EntityId> entitiesAt(const WorldIndex& world, MapPos tile);
EntityId topEntityAt(const Game& game, const WorldIndex& world, MapPos tile);
EntityId topEntityAtOwner(const Game& game, const WorldIndex& world, MapPos tile, PlayerId owner);
bool isOccupied(const WorldIndex& world, MapPos tile, OccupancyLayer layer);
