#pragma once

#include "realm.h"
#include "core/world_index.h"

Entity* findEntity(int id);
Entity* findEntity(Game& game, const WorldIndex& world, int id);
Entity* findDepot(Entity& entity);
Entity* findDepot(Game& game, const WorldIndex& world, Entity& entity);
Entity* entityAt(int x, int y);
Entity* entityAt(Game& game, const WorldIndex& world, int x, int y);
Entity* entityAtOwner(int x, int y, int owner);
Entity* entityAtOwner(Game& game, const WorldIndex& world, int x, int y, int owner);
Entity* corpseAt(int x, int y);
Entity* corpseAt(Game& game, const WorldIndex& world, int x, int y);
bool isDetectedBy(int x, int y, int observerOwner);
bool isConcealing();
void buildOccupancyGrid(OccupancyGrid& grid, bool includeUnits, bool includeBuildings, int ignoreEntityId);
void buildOccupancyGrid(const Game& game, OccupancyGrid& grid, bool includeUnits, bool includeBuildings, int ignoreEntityId);
bool isOccupied(const OccupancyGrid& grid, int x, int y);
bool canPlace(const Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner);
