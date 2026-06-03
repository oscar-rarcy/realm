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
#include "core/game_types.h"
#include "core/validation.h"

// ============================================================
// ENTITY STATS
// ============================================================
extern const EntityStats STATS[E_TYPE_COUNT];

const EntityDefinition& entityDef(EntityType type);

extern const TerrainDefinition TERRAIN_DEFS[TERRAIN_COUNT];
const TerrainDefinition& terrainDef(Terrain type);

inline bool isUnit(EntityType t)     { return (t>=E_PEASANT&&t<=E_RAM)||(t>=E_DEER&&t<=E_BOAR); }
inline bool isBuilding(EntityType t) { return t>=E_TOWNHALL&&t<=E_DOCK; }
inline bool hasTrait(EntityType t, EntityTrait tr) { return (STATS[t].traits & tr) != 0; }
inline bool isRanged(EntityType t)   { return hasTrait(t, TR_RANGED); }
inline bool isNaval(EntityType t)    { return hasTrait(t, TR_NAVAL); }
inline bool isWorker(EntityType t)   { return hasTrait(t, TR_WORKER); }
inline bool canGather(EntityType t)  { return hasTrait(t, TR_GATHERER); }
inline bool canBuild(EntityType t)   { return hasTrait(t, TR_BUILDER); }
inline bool isMilitary(EntityType t) { return hasTrait(t, TR_MILITARY); }
inline bool isInfantry(EntityType t) { return hasTrait(t, TR_INFANTRY); }
inline bool isSiege(EntityType t)    { return hasTrait(t, TR_SIEGE); }
inline bool isWildAnimal(EntityType t) { return hasTrait(t, TR_WILD_ANIMAL); }
inline bool isHostileWildlife(EntityType t) { return hasTrait(t, TR_HOSTILE_WILDLIFE); }
inline bool isDropoff(EntityType t)  { return hasTrait(t, TR_DROPOFF); }
inline bool trainsUnits(EntityType t){ return hasTrait(t, TR_TRAINS_UNITS); }
inline bool canAttack(EntityType t)  { return STATS[t].atk > 0; }

// ============================================================
// DATA STRUCTURES
// ============================================================
struct Projectile { float x,y,tx,ty; char glyph; int color,life; bool alive; };

struct Cargo {
    CargoResource type;
    int amount;
    int sourceX, sourceY;
};

struct ActionMarker {
    int x, y, ticks;
    char glyph;
};

struct CommandBinding {
    const char* id;
    const char* label;
    const char* keys;
    const char* modes;
    const char* help;
};

struct Tile {
    Terrain terrain; int resources;
    bool visible[MAX_PLAYERS], explored[MAX_PLAYERS]; Biome biome;
    Terrain preWinterTerrain; // snapshot taken when winter arrives; restored during spring thaw
    int wear;        // 0-100: traffic + creep. Drives dirt/road transitions and decay.
};

struct Entity {
    int id; EntityType type; int owner, x, y, hp, maxHp;
    EntityState state; int targetId, targetX, targetY;
    std::vector<std::pair<int,int>> path; int pathIdx;
    int moveCd, atkCd, gatherCd;
    Cargo cargo;
    EntityType producing; int trainProgress, trainTime;
    int researchProgress, researchTime;
    bool underConstruction, alive; int rallyX, rallyY;
    int resourceX, resourceY;
    int storedFood;
    int stuckTicks;
    int alertTicks; // > 0 = recently in combat; render flashes '!'
    int deathTicks; // S_DEAD corpse age; drives dead body -> skeleton/wreck visuals.
    int carcassFoodRemaining; // Harvestable food left on dead deer/sheep/boar carcasses.
    int carcassFoodMax;       // Original carcass food amount; wolves intentionally stay zero.
    int rallySet;   // 0 = default, 1 = player-set rally point honoured on training
    int researching; // Research bit currently being researched (Blacksmith only); 0 = none
    int attackMove;  // 1 = engage enemies opportunistically while moving
    int holdPosition;// 1 = ignore auto-aggro; only attack when explicitly ordered
    int facingDx;    // last or intended facing delta, map-space x component
    int facingDy;    // last or intended facing delta, map-space y component
    bool gateOpen;   // E_GATE only: open (passable) vs closed (blocks pathing)
    bool gateLocked; // E_GATE only: manual mode — don't auto-toggle on ally proximity
    int convertTicks; // accumulated exposure to an enemy church
    int retreating;   // >0 while fleeing to safety at low HP
    int packed;       // E_TREBUCHET: 1 = mobile/packed, 0 = deployed/firing
    int packTicks;    // E_TREBUCHET: ticks remaining in pack/unpack transition
    std::vector<int> queue;    // pending EntityTypes to train (FIFO, max 5)
    std::vector<int> garrison; // unit ids currently inside this building
};

struct Player {
    int gold, wood, food, supply, supplyMax;
    bool alive;
    int research;     // bitmask of completed upgrades (R_*)
    int aiWaveCd;     // per-AI rate-limit for wave dispatch
};

struct Game {
    Tile map[MAP_H][MAP_W];
    std::deque<Entity> entities;
    std::vector<Projectile> projectiles;
    int nextId; Player players[MAX_PLAYERS + 1]; int tick;
    GameMode mode;
    int selectedId;
    std::vector<int> selectedIds;
    std::vector<int> controlGroups[9];
    std::vector<int> controlGroupsByOwner[MAX_PLAYERS][9]; // runtime-only slots for non-local issuers
    bool groupAssignPending;
    std::string statusMsg; int statusTimer;
    EntityType buildPending;
    int winner, aiTimer, farmTimer, animalTimer;
    float dayPhase, seasonPhase;
    int prevSeason; // for detecting season transitions
    int prevTimePhase;   // 0=day 1=dusk 2=night 3=dawn; for transition messages
    int attackNotifyCd;  // ticks until next under-attack notification
    int weather;       // current Weather state
    int weatherTimer;  // ticks until next weather change roll
    int biomeChoice;   // -1 = random, else Biome enum value forced on whole map
    bool returnToMenu; // set on game-over to break back to splash screen
    unsigned seed;
    int startupAIs;
    int humanCorner;
    int matchNumber;
    bool diagnostics;
    bool helpOverlay;
    unsigned rngState;
    std::vector<ActionMarker> actionMarkers;
};
extern Game g;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

// map generation
struct MapGenerationConfig {
    int biomeChoice = -1;
};
struct WorldIndex;
MapGenerationConfig currentMapGenerationConfig();
MapGenerationConfig currentMapGenerationConfig(const Game& game);
void generateMap(Game& game,const MapGenerationConfig& config);
void clearStartArea(Game& game,int cx,int cy,int radius);
void placeGoldCluster(Game& game,int cx,int cy,int count);
float sampleNoise(float fx,float fy);
void placeCastleRuin(Game& game,int cx,int cy,int size);
void assignBiomesAndPaintBaseTerrain(Game& game);
void addMountains(Game& game);
void addWater(Game& game);
void addFish(Game& game);
void addGold(Game& game);
void addStone(Game& game);
void addRoads(Game& game);
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
const CommandBinding* gameplayCommands(int& count);
std::string commandHelpLine();
bool    isPassable(const Game& game,int x,int y);
bool    isPassableWater(const Game& game,int x,int y);
bool    isDetectedBy(const Game& game,int x,int y,int observerOwner);
bool    isConcealing(const Game& game);
void    setStatus(const std::string& msg);
void    addActionMarker(int x,int y,char glyph);
void    addPlayerFood(Game& game,int owner,int amount,Entity* depot);
void    spendPlayerFood(Game& game,int owner,int amount);
Entity* findEntity(Game& game,const WorldIndex& world,int id);
Entity* findDepot(Game& game,const WorldIndex& world,Entity& e);
Entity* entityAt(Game& game,const WorldIndex& world,int x,int y);
Entity* entityAtOwner(Game& game,const WorldIndex& world,int x,int y,int owner);
Entity* corpseAt(Game& game,const WorldIndex& world,int x,int y);
bool    isHarvestableCarcass(const Entity& e);
void    buildOccupancyGrid(const Game& game,OccupancyGrid& grid,bool includeUnits,bool includeBuildings,int ignoreEntityId=-1);
bool    isOccupied(const OccupancyGrid& grid,int x,int y);
bool    canPlace(const Game& game,const WorldIndex& world,EntityType type,int x,int y,int owner);
void    updateSupply(Game& game,int owner);
int     reservedSupply(const Game& game,int owner);
int     spawnEntity(Game& game,EntityType type,int owner,int x,int y,bool built=true);

// projectiles / pathfinding
void spawnProjectile(Game& game,int sx,int sy,int tx,int ty,char gl,int col);
void tickProjectiles(Game& game);
std::vector<std::pair<int,int>> findPath(const Game& game,int sx,int sy,int tx,int ty,int maxSteps=300,bool naval=false);
std::vector<std::pair<int,int>> findPathFor(const Game& game,Entity& e,int tx,int ty);

// orders
Entity* findNearestEnemy(Game& game,Entity& e,int range);
int unitAtk(const Game& game,const Entity& e);
int unitRange(const Game& game,const Entity& e);
int damageVs(const Game& game,EntityType attacker,EntityType target,int rawDmg,int targetOwner=-1);
void killEntity(Game& game,Entity& target);
void orderMove(Game& game,Entity& e,int tx,int ty);
void orderAttack(Game& game,const WorldIndex& world,Entity& e,int tid);
void orderGather(Game& game,const WorldIndex& world,Entity& e,int tx,int ty);
void orderGarrison(Game& game,const WorldIndex& world,Entity& e,int buildingId);
struct Selection;
void orderGroupMove(Game& game,const WorldIndex& world,const Selection& selection,int tx,int ty,int owner=0);
void orderGroupAttack(Game& game,const WorldIndex& world,const Selection& selection,int tid,int owner=0);
void orderGroupAttackMove(Game& game,const WorldIndex& world,const Selection& selection,int tx,int ty,int owner=0);
void orderHelp(Game& game,const WorldIndex& world,Entity& e,int buildingId);
void moveAlongPath(Game& game,const WorldIndex& world,Entity& e);
bool findNearbyResource(Game& game,const WorldIndex& world,Entity& e);

// garrison
bool canGarrisonIn(EntityType bt);
int  garrisonCap(EntityType bt);
void ejectGarrison(Game& game,Entity& bld);

// tick / game logic
void tickEntity(Game& game,Entity& e);
void tickProduction(Game& game,Entity& e);
void tickResearch(Game& game,Entity& e);
void tickTowers(Game& game);
void tickGates(Game& game);
void tickFarms(Game& game);
void tickMarkets(Game& game);
void tickChurches(Game& game);
void tickAnimals(Game& game);
void tickSeasons(Game& game);
void tickThaw(Game& game);
void tickWinter(Game& game);
void tickPaving(Game& game);
void tickWeather(Game& game);
void checkWin(Game& game);
void updateFog(Game& game);
void tickActionMarkers(Game& game);
bool saveGame(Game& game,const std::string& path);
bool loadGame(Game& game,const std::string& path);
int  dumpMissingTilesetAssets();

// AI
void    tickAI(Game& game);

// ASCII renderer
void initColors();
void render();
void renderMap();
void renderUI();
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
void tickSimulationOnce(Game& game);
void tickSimulationOnce(Game& game,bool runAI);
