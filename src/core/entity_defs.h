#pragma once

#include "core/game_types.h"

struct Entity;

const EntityDefinition& entityDef(EntityType type);
const char* stateName(EntityState state);
BuildingVisualState buildingVisualState(const Entity& entity);
AnimalCarcassVisualState animalCarcassVisualState(const Entity& entity);
TransportVisualState transportVisualState(const Entity& entity);
const char* buildingVisualStateName(BuildingVisualState state);
const char* animalCarcassVisualStateName(AnimalCarcassVisualState state);
const char* transportVisualStateName(TransportVisualState state);
bool isHarvestableCarcass(const Entity& entity);
int carcassFoodForAnimal(EntityType type);
