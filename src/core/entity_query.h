#pragma once

#include "core/world_index.h"

Entity* findEntity(Game& game, const WorldIndex& world, int id);
Entity* findDepot(Game& game, const WorldIndex& world, Entity& entity);
Entity* entityAt(Game& game, const WorldIndex& world, int x, int y);
Entity* entityAtOwner(Game& game, const WorldIndex& world, int x, int y, int owner);
Entity* corpseAt(Game& game, const WorldIndex& world, int x, int y);
bool isDetectedBy(const Game& game, int x, int y, int observerOwner);
bool isConcealing(const Game& game);
void buildOccupancyGrid(const Game& game, OccupancyGrid& grid, bool includeUnits, bool includeBuildings, int ignoreEntityId);
bool isOccupied(const OccupancyGrid& grid, int x, int y);
bool canPlace(const Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner);
