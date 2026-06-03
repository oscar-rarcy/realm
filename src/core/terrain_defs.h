#pragma once

#include "core/game_types.h"

#include <cstdint>

struct Game;
struct Tile;

const TerrainDefinition& terrainDef(Terrain type);
const char* terrainName(Terrain terrain);
const char* biomeName(Biome biome);
CargoResource resourceForTerrain(Terrain terrain);
bool terrainMatchesResource(Terrain terrain, CargoResource resource);
const char* groundTypeName(GroundType ground);
const char* featureTypeName(FeatureType feature);
const char* featureStateName(FeatureState state);
const char* visualDecalName(VisualDecalType decal);
VisualTileParts visualPartsForTile(const Tile& tile);
VisualTileParts visualPartsForTerrain(Terrain terrain, Biome biome, int resources, int wear,
                                      bool gateOpen, bool gateLocked);
uint32_t featureTraits(FeatureType feature);
bool featureConceals(FeatureType feature);
bool isConcealingTile(int x, int y);
int movementPenaltyForTile(const Tile& tile);
bool isPassable(const Game& game, int x, int y);
bool isPassableWater(const Game& game, int x, int y);
