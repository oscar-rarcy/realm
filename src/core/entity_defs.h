#pragma once

#include "core/game_types.h"

struct Entity;

extern const EntityStats STATS[E_TYPE_COUNT];
const EntityDefinition& entityDef(EntityType type);
inline bool isUnit(EntityType type) {
    return (type >= E_PEASANT && type <= E_RAM) || (type >= E_DEER && type <= E_BOAR);
}
inline bool isBridge(EntityType type) {
    return type == E_WOODEN_BRIDGE || type == E_STONE_BRIDGE;
}
inline bool isBuilding(EntityType type) {
    return (type >= E_TOWNHALL && type <= E_DOCK) || isBridge(type);
}
inline bool hasTrait(EntityType type, EntityTrait trait) {
    return (STATS[type].traits & trait) != 0;
}
inline bool isRanged(EntityType type) { return hasTrait(type, TR_RANGED); }
inline bool isNaval(EntityType type) { return hasTrait(type, TR_NAVAL); }
inline bool isWorker(EntityType type) { return hasTrait(type, TR_WORKER); }
inline bool canGather(EntityType type) { return hasTrait(type, TR_GATHERER); }
inline bool canBuild(EntityType type) { return hasTrait(type, TR_BUILDER); }
inline bool isMilitary(EntityType type) { return hasTrait(type, TR_MILITARY); }
inline bool isInfantry(EntityType type) { return hasTrait(type, TR_INFANTRY); }
inline bool isSiege(EntityType type) { return hasTrait(type, TR_SIEGE); }
inline bool isWildAnimal(EntityType type) { return hasTrait(type, TR_WILD_ANIMAL); }
inline bool isHostileWildlife(EntityType type) { return hasTrait(type, TR_HOSTILE_WILDLIFE); }
inline bool isDropoff(EntityType type) { return hasTrait(type, TR_DROPOFF); }
inline bool trainsUnits(EntityType type) { return hasTrait(type, TR_TRAINS_UNITS); }
inline bool canAttack(EntityType type) { return STATS[type].atk > 0; }
const char* stateName(EntityState state);
BuildingVisualState buildingVisualState(const Entity& entity);
AnimalCarcassVisualState animalCarcassVisualState(const Entity& entity);
TransportVisualState transportVisualState(const Entity& entity);
const char* buildingVisualStateName(BuildingVisualState state);
const char* animalCarcassVisualStateName(AnimalCarcassVisualState state);
const char* transportVisualStateName(TransportVisualState state);
bool isHarvestableCarcass(const Entity& entity);
int carcassFoodForAnimal(EntityType type);
