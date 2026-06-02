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

// ============================================================
// CONSTANTS
// ============================================================
const int MAP_W        = 180;
const int MAP_H        = 110;
const int TICK_MS      = 80;
const int FOG_RADIUS   = 7;
const int GATHER_RATE  = 8;
const int GATHER_TICKS = 15;
const int DAY_LENGTH   = 1500;
const int SEASON_LENGTH= 3000;
const int CARRY_MAX    = 20;
const int MAX_PLAYERS  = 4;
const int OWNER_NATURE = MAX_PLAYERS;
const int DEATH_DECAY_TICKS = 375;  // 30 seconds at 80 ms/tick before corpse becomes skeleton/wreck.
const int CORPSE_REMOVE_TICKS = 900; // Keep decayed remains visible for a while before pruning.

// ============================================================
// ENUMS
// ============================================================
enum Terrain {
    T_GRASS, T_TALL_GRASS, T_FLOWERS, T_MEADOW,
    T_FOREST, T_PINE, T_PALM, T_DEAD_TREE,
    T_MOUNTAIN, T_HILLS, T_STONE,
    T_WATER, T_SHALLOWS, T_MARSH, T_REEDS,
    T_GOLD,
    T_SAND, T_DUNES,
    T_SNOW, T_ICE,
    T_DIRT, T_ROAD, T_MUD,
    T_WHEAT, T_BERRY, T_FISH,
    T_RUINS, T_GRAVEL,
    T_LAVA, T_ASH,
    T_CASTLE_WALL, T_CASTLE_FLOOR, T_CASTLE_GATE,
    TERRAIN_COUNT
};

enum EntityType {
    E_NONE = 0,
    E_PEASANT, E_MILITIA, E_ARCHER, E_KNIGHT, E_SPEARMAN, E_CATAPULT, E_TREBUCHET,
    E_FISHING_BOAT, E_WARSHIP, E_TRANSPORT, E_RAM,
    E_TOWNHALL, E_HOUSE, E_BARRACKS, E_STABLE, E_TOWER,
    E_FARM, E_BLACKSMITH, E_CHURCH, E_MARKET, E_WALL, E_GATE, E_CASTLE,
    E_LUMBER_CAMP, E_MINING_CAMP, E_MILL, E_DOCK,
    E_DEER, E_WOLF, E_SHEEP, E_BOAR,
    E_TYPE_COUNT
};

enum EntityState {
    S_IDLE, S_MOVING, S_ATTACKING, S_GATHERING,
    S_BUILDING, S_TRAINING, S_RETURNING, S_DEAD,
    S_ENTERING, S_GARRISONED
};
enum GameMode  { M_NORMAL, M_BUILD_SELECT, M_TRAIN_SELECT, M_WALL_DRAG, M_PAUSED, M_GAME_OVER, M_RALLY_SET, M_RESEARCH_SELECT, M_ATTACK_MOVE, M_MARKET_TRADE };

// Research bits stored in Player.research
enum Research { R_IRON_WEAPONS = 1, R_CROSSBOWS = 2, R_PIKES = 4, R_COUNTERWEIGHT = 8, R_PLATE_HELM = 16 };
enum Biome     { B_TEMPERATE, B_DESERT, B_SNOW, B_SWAMP, B_FOREST, B_VOLCANIC, B_OCEAN };
enum Season    { SPRING = 0, SUMMER, AUTUMN, WINTER };
enum Weather   { W_CLEAR = 0, W_RAIN, W_STORM, W_SNOW };
enum CargoResource { CR_NONE = 0, CR_GOLD, CR_WOOD, CR_FOOD, CR_FISH };

enum GroundType {
    G_GRASS, G_MEADOW, G_DIRT, G_ROAD, G_MUD, G_SAND, G_DUNES,
    G_SNOW, G_TUNDRA, G_ICE, G_WATER, G_SHALLOWS, G_MARSH,
    G_GRAVEL, G_ASH, G_LAVA, G_HILLS, G_ROCKY, G_CASTLE_FLOOR
};

enum FeatureType {
    F_NONE,
    F_FOREST, F_PINE, F_PALM, F_DEAD_TREE,
    F_BERRY_BUSH, F_WHEAT_CROP, F_FISH_SHOAL,
    F_GOLD_DEPOSIT, F_STONE_BOULDERS, F_MOUNTAIN_PEAK,
    F_REEDS, F_RUINS, F_CASTLE_WALL, F_CASTLE_GATE
};

enum FeatureState {
    FS_DEFAULT,
    FS_FULL, FS_MOSTLY_FULL, FS_MOSTLY_EMPTY, FS_DEPLETED,
    FS_OPEN, FS_CLOSED, FS_LOCKED,
    FS_DAMAGED, FS_BROKEN
};

enum FeatureTrait : uint32_t {
    FT_BLOCKS_MOVEMENT       = 1u << 0,
    FT_SLOWS_MOVEMENT        = 1u << 1,
    FT_CONCEALS_UNITS        = 1u << 2,
    FT_REDUCES_LINE_OF_SIGHT = 1u << 3,
    FT_CAN_HIDE_WILDLIFE     = 1u << 4,
    FT_HARVESTABLE           = 1u << 5
};

enum VisualDecalType {
    VD_FLOWERS,
    VD_TALL_GRASS,
    VD_SCUFFS,
    VD_PACKED_PATH,
    VD_COBBLE_PATCH,
    VD_WHEEL_RUTS,
    VD_YARD_CLUTTER,
    VD_CRATES_BARRELS,
    VD_LOG_PILES,
    VD_FARM_TRACKS,
    VD_MUDDY_FOOTPRINTS,
    VD_SNOW_TRAMPLED_PATH
};

enum BuildingVisualState {
    BVS_CONSTRUCTION_0_FOUNDATION,
    BVS_CONSTRUCTION_1_FRAME,
    BVS_CONSTRUCTION_2_NEARLY_COMPLETE,
    BVS_COMPLETE,
    BVS_DAMAGED,
    BVS_GARRISONED,
    BVS_GARRISON_FIRING,
    BVS_TRAINING_PEASANT,
    BVS_TRAINING_INFANTRY,
    BVS_TRAINING_CAVALRY,
    BVS_TRAINING_SHIP,
    BVS_RESEARCHING_IRON_WEAPONS,
    BVS_RESEARCHING_CROSSBOWS,
    BVS_RESEARCHING_PIKES,
    BVS_RESEARCHING_COUNTERWEIGHT,
    BVS_RESEARCHING_PLATE_HELM
};

enum AnimalCarcassVisualState {
    ACVS_ALIVE,
    ACVS_DEAD_UNHARVESTED,
    ACVS_PARTLY_HARVESTED,
    ACVS_MOSTLY_HARVESTED,
    ACVS_DEPLETED_SKELETON
};

enum TransportVisualState {
    TVS_EMPTY,
    TVS_LOADED_PARTIAL,
    TVS_LOADED_FULL,
    TVS_LOAD_UNLOAD,
    TVS_WRECK,
    TVS_DECAYED_WRECK
};

enum EntityTrait : uint32_t {
    TR_WORKER           = 1u << 0,
    TR_GATHERER         = 1u << 1,
    TR_BUILDER          = 1u << 2,
    TR_MILITARY         = 1u << 3,
    TR_INFANTRY         = 1u << 4,
    TR_RANGED           = 1u << 5,
    TR_SIEGE            = 1u << 6,
    TR_NAVAL            = 1u << 7,
    TR_WILD_ANIMAL      = 1u << 8,
    TR_HOSTILE_WILDLIFE = 1u << 9,
    TR_DROPOFF          = 1u << 10,
    TR_TRAINS_UNITS     = 1u << 11,
    TR_DEFENSE          = 1u << 12,
    TR_GARRISON         = 1u << 13
};

// ============================================================
// COLOR PAIR IDS  (shared by renderers and gameplay markers)
// ============================================================
enum {
    CP_GRASS = 1, CP_GRASS_LIGHT, CP_GRASS_DRY, CP_TALL_GRASS,
    CP_FLOWERS, CP_FLOWERS_BLUE, CP_FLOWERS_YELLOW, CP_FLOWERS_RED, CP_MEADOW,
    CP_FOREST, CP_FOREST_DARK, CP_PINE, CP_PALM, CP_DEAD_TREE,
    CP_MOUNTAIN, CP_HILLS, CP_STONE,
    CP_WATER, CP_WATER_SHIMMER, CP_SHALLOWS, CP_MARSH, CP_REEDS,
    CP_GOLD, CP_GOLD_SHIMMER,
    CP_SAND, CP_DUNES, CP_SNOW_GROUND, CP_ICE,
    CP_DIRT, CP_ROAD,
    CP_WHEAT, CP_WHEAT_GOLD, CP_BERRY,
    CP_RUINS, CP_GRAVEL,
    CP_CASTLE_WALL, CP_CASTLE_FLOOR, CP_CASTLE_GATE,
    CP_AUT_TREE_EARLY, CP_AUT_TREE_MID, CP_AUT_TREE_LATE, CP_AUT_TREE_GOLD, CP_AUT_TREE_RED,
    CP_AUT_GRASS, CP_AUT_GRASS_LATE,
    CP_WIN_GROUND, CP_WIN_TREE, CP_WIN_PINE, CP_WIN_ICE,
    CP_NIGHT_GRASS, CP_NIGHT_TREE, CP_NIGHT_WATER,
    CP_NIGHT_GROUND, CP_NIGHT_GOLD, CP_NIGHT_SNOW,
    CP_DAWN_SKY, CP_DUSK_SKY,
    CP_PLAYER, CP_PLAYER_NIGHT, CP_ENEMY, CP_ENEMY_NIGHT,
    CP_SHIP_PLAYER, CP_SHIP_ENEMY,
    CP_SHIP_P0, CP_SHIP_P1, CP_SHIP_P2, CP_SHIP_P3,
    CP_PROJ_ARROW, CP_PROJ_BOULDER, CP_PROJ_TOWER,
    CP_RAIN, CP_SNOW_FALL,
    CP_UI_BAR, CP_UI_TEXT, CP_UI_HIGH, CP_UI_DIM, CP_UI_ACCENT,
    CP_FOG, CP_FOG_EXPLORED, CP_CURSOR,
    CP_HP_GREEN, CP_HP_YELLOW, CP_HP_RED,
    CP_SUN, CP_MOON,
    CP_MM_PLAYER, CP_MM_ENEMY, CP_MM_WATER, CP_MM_FOREST,
    CP_MM_GOLD, CP_MM_SAND, CP_MM_SNOW, CP_MM_MTN, CP_MM_CASTLE,
    CP_SPRING_FLOWER,
    CP_DEER, CP_WOLF, CP_SHEEP, CP_BOAR, CP_MM_ANIMAL,
    CP_LAVA, CP_LAVA_HOT, CP_ASH,
    // Ownership background colours: background = owner, foreground = glyph.
    // Used for all land units and buildings (ships keep CP_SHIP_* wood bg).
    // One set per player slot (0=human, 1-3=AI); separate night variants.
    CP_OWN_P0, CP_OWN_P1, CP_OWN_P2, CP_OWN_P3,
    CP_OWN_P0_NIGHT, CP_OWN_P1_NIGHT, CP_OWN_P2_NIGHT, CP_OWN_P3_NIGHT,
    CP_COUNT
};

// ============================================================
// ENTITY STATS
// ============================================================
struct EntityStats {
    const char* name; char glyph;
    int maxHp, atk, range, speed, atkSpeed, costGold, costWood, costFood, trainTime;
    int sizeW, sizeH, supplyProvided, supplyUsed; bool isBuilding;
    uint32_t traits;
};
extern const EntityStats STATS[E_TYPE_COUNT];

using EntityDefinition = EntityStats;
const EntityDefinition& entityDef(EntityType type);

struct TerrainDefinition {
    Terrain type;
    const char* name;
    CargoResource resource;
    bool passableLand;
    bool passableWater;
    bool buildable;
    bool conceals;
    int movementPenalty;
    GroundType ground;
    FeatureType feature;
};
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

struct OccupancyGrid {
    bool occupied[MAP_H][MAP_W];
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

struct MapPos {
    int x;
    int y;
};

struct VisualTileParts {
    GroundType ground = G_GRASS;
    FeatureType feature = F_NONE;
    FeatureState featureState = FS_DEFAULT;
    int featureResources = 0;
    uint32_t featureTraits = 0;
    std::vector<VisualDecalType> decals;
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
void generateMap();
void clearStartArea(int cx,int cy,int radius);
void placeGoldCluster(int cx,int cy,int count);
float sampleNoise(float fx,float fy);
void placeCastleRuin(int cx,int cy,int size);
void assignBiomesAndPaintBaseTerrain();
void addMountains();
void addWater();
void addFish();
void addGold();
void addStone();
void addRoads();
void addPointsOfInterest();
void addFoodPatches();
void snapshotPreWinterTerrain();

// time
float       getBrightness();
Season      getSeason();
float       getSeasonProgress();
const char* getSeasonName();
const char* getTimeName();
bool        isNight();
bool        isDusk();
bool        isDawn();

// core helpers
void    realmSrand(unsigned seed);
int     realmRand();
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
int     movementPenaltyForTile(const Tile& tile);
BuildingVisualState buildingVisualState(const Entity& e);
AnimalCarcassVisualState animalCarcassVisualState(const Entity& e);
TransportVisualState transportVisualState(const Entity& e);
const char* buildingVisualStateName(BuildingVisualState s);
const char* animalCarcassVisualStateName(AnimalCarcassVisualState s);
const char* transportVisualStateName(TransportVisualState s);
const CommandBinding* gameplayCommands(int& count);
std::string commandHelpLine();
bool    validateGameState(std::string* error=nullptr);
bool    isPassable(int x,int y);
bool    isPassableWater(int x,int y);
bool    isDetectedBy(int x,int y,int observerOwner);
bool    isConcealing();
void    setStatus(const std::string& msg);
void    addActionMarker(int x,int y,char glyph);
void    addPlayerFood(int owner,int amount,Entity* depot);
void    spendPlayerFood(int owner,int amount);
Entity* findEntity(int id);
Entity* findDepot(Entity& e);
Entity* entityAt(int x,int y);
Entity* entityAtOwner(int x,int y,int owner);
Entity* corpseAt(int x,int y);
bool    isHarvestableCarcass(const Entity& e);
void    buildOccupancyGrid(OccupancyGrid& grid,bool includeUnits,bool includeBuildings,int ignoreEntityId=-1);
bool    isOccupied(const OccupancyGrid& grid,int x,int y);
bool    canPlace(EntityType type,int x,int y,int owner);
void    updateSupply(int owner);
int     reservedSupply(int owner);
int     spawnEntity(EntityType type,int owner,int x,int y,bool built=true);

// projectiles / pathfinding
void spawnProjectile(int sx,int sy,int tx,int ty,char gl,int col);
void tickProjectiles();
std::vector<std::pair<int,int>> findPath(int sx,int sy,int tx,int ty,int maxSteps=300,bool naval=false);
std::vector<std::pair<int,int>> findPathFor(Entity& e,int tx,int ty);

// orders
Entity* findNearestEnemy(Entity& e,int range);
int unitAtk(const Entity& e);
int unitRange(const Entity& e);
int damageVs(EntityType attacker,EntityType target,int rawDmg,int targetOwner=-1);
void killEntity(Entity& target);
void orderMove(Entity& e,int tx,int ty);
void orderAttack(Entity& e,int tid);
void orderGather(Entity& e,int tx,int ty);
void orderBuild(Entity& e,EntityType bt,int bx,int by);
void orderBuildLine(Entity& e,EntityType bt,int x0,int y0,int x1,int y1);
void orderTrain(Entity& bld,EntityType ut);
void orderGroupMove(int tx,int ty);
void orderGroupAttack(int tid);
void orderGroupAttackMove(int tx,int ty);
void orderHelp(Entity& e,int buildingId);
void orderGarrison(Entity& e,int buildingId);
void moveAlongPath(Entity& e);
bool findNearbyResource(Entity& e);

// garrison
bool canGarrisonIn(EntityType bt);
int  garrisonCap(EntityType bt);
void ejectGarrison(Entity& bld);

// state management
void resetDetectMapCache();

// tick / game logic
void tickEntity(Entity& e);
void tickProduction(Entity& e);
void tickResearch(Entity& e);
void tickTowers();
void tickGates();
void tickFarms();
void tickMarkets();
void tickChurches();
void tickAnimals();
void tickSeasons();
void tickThaw();
void tickWinter();
void tickPaving();
void tickWeather();
void checkWin();
void updateFog();
void tickActionMarkers();
bool saveGame(const std::string& path);
bool loadGame(const std::string& path);
int  dumpMissingTilesetAssets();

// AI
int     aiCount(int owner,EntityType t);
int     aiCountAll(int owner,EntityType t);
Entity* aiIdle(int owner,EntityType t);
Entity* aiBldg(int owner,EntityType t);
void    aiGather(int owner);
void    aiBuildSpot(int owner,EntityType bt,int& ox,int& oy);
void    tickAI();

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
void tickSimulationOnce();
