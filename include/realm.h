#pragma once
#include <vector>
#include <queue>
#include <deque>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <cstring>
#include <cstdint>
#include "display.h"
#include "commands/input_intent.h"
#include "core/entity_defs.h"
#include "core/game_state_types.h"
#include "core/game_types.h"
#include "core/validation.h"

// ============================================================
// ENTITY STATS
// ============================================================
extern const TerrainDefinition TERRAIN_DEFS[TERRAIN_COUNT];
const TerrainDefinition& terrainDef(Terrain type);

extern Game g;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

// map generation
struct MapGenerationConfig {
    int biomeChoice = -1;
};
struct MapNoise {
    float samples[32][32]{};
};
struct WorldIndex;
class EventSink;
MapGenerationConfig currentMapGenerationConfig(const Game& game);
void generateMap(Game& game,const MapGenerationConfig& config);
void clearStartArea(Game& game,int cx,int cy,int radius);
void placeGoldCluster(Game& game,int cx,int cy,int count);
MapNoise initMapNoise(Game& game);
float sampleNoise(const MapNoise& noise,float fx,float fy);
void placeCastleRuin(Game& game,int cx,int cy,int size);
void assignBiomesAndPaintBaseTerrain(Game& game,const MapNoise& noise);
void addMountains(Game& game,const MapNoise& noise);
void addWater(Game& game);
void addFish(Game& game);
void addGold(Game& game);
void addStone(Game& game);
void addRoads(Game& game,const MapNoise& noise);
void addPointsOfInterest(Game& game);
void addFoodPatches(Game& game);
void snapshotPreWinterTerrain(Game& game);

// time
float       getBrightness(const Game& game);
Season      getSeason(const Game& game);
float       getSeasonProgress(const Game& game);
const char* getSeasonName(const Game& game);
const char* getTimeName(const Game& game);
bool        isNight(const Game& game);
bool        isDusk(const Game& game);
bool        isDawn(const Game& game);

// core helpers
void    realmSrand(Game& game,unsigned seed);
int     realmRand(Game& game);
int     dist(int x1,int y1,int x2,int y2);
int     mdist(int x1,int y1,int x2,int y2);
bool    inBounds(int x,int y);
const char* stateName(EntityState s);
const char* terrainName(Terrain t);
const char* biomeName(Biome b);
const char* modeName(GameMode m);
const char* cargoResourceName(CargoResource r);
CargoResource resourceForTerrain(Terrain t);
bool    terrainMatchesResource(Terrain t,CargoResource r);
bool    terrainHasDirectGatherResource(Terrain t);
Terrain depletedTerrainForResource(Terrain t);
char    terrainAsciiGlyph(Terrain t);
Cargo   emptyCargo();
int     carcassFoodForAnimal(EntityType type);
const char* groundTypeName(GroundType g);
const char* featureTypeName(FeatureType f);
const char* featureStateName(FeatureState s);
const char* visualDecalName(VisualDecalType d);
VisualTileParts visualPartsForTile(const Tile& tile);
VisualTileParts visualPartsForTerrain(Terrain terrain,Biome biome,int resources,int wear,bool gateOpen=false,bool gateLocked=false);
uint32_t featureTraits(FeatureType feature);
bool    featureConceals(FeatureType feature);
bool    isConcealingTile(int x,int y);
bool    isConcealingTile(const Game& game,int x,int y);
int     movementPenaltyForTile(const Tile& tile);
BuildingVisualState buildingVisualState(const Entity& e);
AnimalCarcassVisualState animalCarcassVisualState(const Entity& e);
TransportVisualState transportVisualState(const Entity& e);
const char* buildingVisualStateName(BuildingVisualState s);
const char* animalCarcassVisualStateName(AnimalCarcassVisualState s);
const char* transportVisualStateName(TransportVisualState s);
bool    isPassable(const Game& game,int x,int y);
bool    isPassableWater(const Game& game,int x,int y);
bool    isDetectedBy(const Game& game,int x,int y,int observerOwner);
bool    isConcealing(const Game& game);
void    addPlayerFood(Game& game,int owner,int amount,Entity* depot);
void    spendPlayerFood(Game& game,int owner,int amount);
Entity* findEntity(Game& game,const WorldIndex& world,int id);
Entity* findDepot(Game& game,const WorldIndex& world,Entity& e);
Entity* entityAt(Game& game,const WorldIndex& world,int x,int y);
Entity* entityAtOwner(Game& game,const WorldIndex& world,int x,int y,int owner);
Entity* corpseAt(Game& game,const WorldIndex& world,int x,int y);
bool    isHarvestableCarcass(const Entity& e);
bool    canPlace(const Game& game,const WorldIndex& world,EntityType type,int x,int y,int owner,int ignoreEntityId=-1);
void    updateSupply(Game& game,int owner);
int     reservedSupply(const Game& game,int owner);
int     spawnEntity(Game& game,EntityType type,int owner,int x,int y,bool built=true);

// projectiles / pathfinding
void spawnProjectile(Game& game,int sx,int sy,int tx,int ty,char gl,int col);
void tickProjectiles(Game& game);
std::vector<std::pair<int,int>> findPath(const Game& game,int sx,int sy,int tx,int ty,int maxSteps=300,bool naval=false);
std::vector<std::pair<int,int>> findPath(const Game& game,const WorldIndex& world,int sx,int sy,int tx,int ty,int maxSteps,bool naval);
std::vector<std::pair<int,int>> findPathFor(const Game& game,Entity& e,int tx,int ty);

// orders
Entity* findNearestEnemy(Game& game,Entity& e,int range);
int unitAtk(const Game& game,const Entity& e);
int unitRange(const Game& game,const Entity& e);
int damageVs(const Game& game,EntityType attacker,EntityType target,int rawDmg,int targetOwner=-1);
void killEntity(Game& game,EventSink& events,Entity& target);
void moveAlongPath(Game& game,const WorldIndex& world,Entity& e);
bool findNearbyResource(Game& game,const WorldIndex& world,EventSink& events,Entity& e);

// garrison
bool canGarrisonIn(EntityType bt);
int  garrisonCap(EntityType bt);
void ejectGarrison(Game& game,Entity& bld);

// tick / game logic
void tickEntity(Game& game,WorldIndex& world,EventSink& events,Entity& e);
void tickProduction(Game& game,WorldIndex& world,EventSink& events,Entity& e);
void tickResearch(Game& game,EventSink& events,Entity& e);
void tickTowers(Game& game,EventSink& events);
void tickGates(Game& game);
void tickFarms(Game& game,EventSink& events);
void tickMarkets(Game& game);
void tickChurches(Game& game,EventSink& events);
void tickAnimals(Game& game,EventSink& events);
void tickSeasons(Game& game,EventSink& events);
void tickThaw(Game& game);
void tickWinter(Game& game,EventSink& events);
void tickPaving(Game& game);
void tickWeather(Game& game,EventSink& events);
void checkWin(Game& game);
void updateFog(Game& game);
bool saveGame(Game& game,const std::string& path);
bool loadGame(Game& game,const std::string& path);
int  dumpMissingTilesetAssets();

// AI
void    tickAI(Game& game,EventSink& events);

// ASCII renderer
void initColors();
void render();
void renderMap(const WorldIndex& world);
void renderUI(const WorldIndex& world);
void getTerrainVisual(Terrain t,int x,int y,char& ch,int& cp);
int ownerColorPair(int owner,bool night);

// input
void handleInput(int ch);

// frontend/shared
void forceUtf8Locale();
int envInt(const char* name,int fallback);
unsigned envUnsigned(const char* name,unsigned fallback);

// game initialization
void initGame(int numAIs);
void initGameWithSeed(int numAIs,unsigned seed,int humanCorner);
void initGameWithSeed(int numAIs,unsigned seed,int humanCorner,const MapGenerationConfig& mapConfig);
void tickSimulationOnce(Game& game,EventSink& events);
void tickSimulationOnce(Game& game,EventSink& events,bool runAI);
