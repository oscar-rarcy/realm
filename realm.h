#pragma once
#include <ncurses.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <cstring>
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
    T_CASTLE_WALL, T_CASTLE_FLOOR, T_CASTLE_GATE
};

enum EntityType {
    E_NONE = 0,
    E_PEASANT, E_MILITIA, E_ARCHER, E_KNIGHT, E_SPEARMAN, E_CATAPULT, E_TREBUCHET,
    E_FISHING_BOAT, E_WARSHIP, E_TRANSPORT, E_RAM,
    E_TOWNHALL, E_HOUSE, E_BARRACKS, E_STABLE, E_TOWER,
    E_FARM, E_BLACKSMITH, E_CHURCH, E_MARKET, E_WALL, E_GATE, E_CASTLE,
    E_LUMBER_CAMP, E_MINING_CAMP, E_MILL, E_DOCK,
    E_DEER, E_WOLF, E_SHEEP, E_BOAR
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

// ============================================================
// COLOR PAIR IDS  (used in both entity.cpp and render.cpp)
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
    // Per-player ship hulls: brown deck, owner-coloured glyph.
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
    int maxHp, atk, range, speed, atkSpeed, costGold, costWood, trainTime;
    int sizeW, sizeH, supplyProvided, supplyUsed; bool isBuilding;
};
extern const EntityStats STATS[];

inline bool isUnit(EntityType t)     { return (t>=E_PEASANT&&t<=E_RAM)||(t>=E_DEER&&t<=E_BOAR); }
inline bool isBuilding(EntityType t) { return t>=E_TOWNHALL&&t<=E_DOCK; }
inline bool isRanged(EntityType t)   { return t==E_ARCHER||t==E_CATAPULT||t==E_TREBUCHET||t==E_WARSHIP; }
inline bool isNaval(EntityType t)    { return t==E_FISHING_BOAT||t==E_WARSHIP||t==E_TRANSPORT; }

// ============================================================
// DATA STRUCTURES
// ============================================================
struct Projectile { float x,y,tx,ty; char glyph; int color,life; bool alive; };

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
    int moveCd, atkCd, gatherCd, gatherType;
    EntityType producing; int prodProgress, prodTime;
    bool underConstruction, alive; int rallyX, rallyY;
    int carrying;
    int stuckTicks;
    int alertTicks; // > 0 = recently in combat; render flashes '!'
    int rallySet;   // 0 = default, 1 = player-set rally point honoured on training
    int researching; // Research bit currently being researched (Blacksmith only); 0 = none
    int attackMove;  // 1 = engage enemies opportunistically while moving
    int holdPosition;// 1 = ignore auto-aggro; only attack when explicitly ordered
    bool gateOpen;   // E_GATE only: open (passable) vs closed (blocks pathing)
    bool gateLocked; // E_GATE only: manual mode — don't auto-toggle on ally proximity
    int convertTicks; // accumulated exposure to an enemy church; convert when threshold met
    int retreating;   // >0 while fleeing to safety at low HP; suppresses auto-aggro
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
    std::vector<Entity> entities;
    std::vector<Projectile> projectiles;
    int nextId; Player players[MAX_PLAYERS + 1]; int tick;
    GameMode mode; int cursorX, cursorY, viewX, viewY, viewW, viewH;
    int selectedId;
    std::vector<int> selectedIds;
    std::vector<int> controlGroups[9];
    bool groupAssignPending;
    bool dragging; int dragStartX, dragStartY;
    std::string statusMsg; int statusTimer;
    EntityType buildPending; int wallDragX, wallDragY;
    int winner, aiTimer, farmTimer;
    float dayPhase, seasonPhase;
    int prevSeason;       // for detecting season transitions
    int prevTimePhase;    // 0=day 1=dusk 2=night 3=dawn; for transition messages
    int attackNotifyCd;  // ticks until next "Under attack" message is allowed
    int weather;          // current Weather state
    int weatherTimer;     // ticks until next weather change roll
    int biomeChoice;      // -1 = random, else Biome enum value forced on whole map
    bool returnToMenu;    // set on game-over to break back to splash screen
};
extern Game g;

// ============================================================
// FUNCTION PROTOTYPES
// ============================================================

// mapgen.cpp
void generateMap();
void clearStartArea(int cx, int cy, int radius);
void placeGoldCluster(int cx, int cy, int count);

// entity.cpp — time
float       getBrightness();
Season      getSeason();
float       getSeasonProgress();
const char* getSeasonName();
const char* getTimeName();
bool        isNight();
bool        isDusk();
bool        isDawn();

// entity.cpp — helpers
int     dist(int x1,int y1,int x2,int y2);
int     mdist(int x1,int y1,int x2,int y2);
bool    inBounds(int x,int y);
bool    isPassable(int x,int y);
bool    isPassableWater(int x,int y);
bool    isDetectedBy(int x,int y,int observerOwner);
bool    isConcealing();
void    setStatus(const std::string& msg);
Entity* findEntity(int id);
Entity* findDepot(Entity& e);
Entity* entityAt(int x,int y);
Entity* entityAtOwner(int x,int y,int owner);
bool    canPlace(EntityType type,int x,int y,int owner);
void    updateSupply(int owner);
int     spawnEntity(EntityType type,int owner,int x,int y,bool built=true);

// entity.cpp — projectiles / pathfinding
void spawnProjectile(int sx,int sy,int tx,int ty,char gl,int col);
void tickProjectiles();
std::vector<std::pair<int,int>> findPath(int sx,int sy,int tx,int ty,int maxSteps=300,bool naval=false);

// entity.cpp — orders
Entity* findNearestEnemy(Entity& e,int range);
void orderMove(Entity& e,int tx,int ty);
void orderAttack(Entity& e,int tid);
void orderGather(Entity& e,int tx,int ty);
void orderBuild(Entity& e,EntityType bt,int bx,int by);
void orderTrain(Entity& bld,EntityType ut);
void orderGroupMove(int tx,int ty);
void orderGroupAttack(int tid);
void orderGroupAttackMove(int tx,int ty);
void orderHelp(Entity& e,int buildingId);
void orderGarrison(Entity& e,int buildingId);
void moveAlongPath(Entity& e);

// entity.cpp — garrison
bool canGarrisonIn(EntityType bt);
int  garrisonCap(EntityType bt);
void ejectGarrison(Entity& bld);

// entity.cpp — state management
void resetDetectMapCache();

// entity.cpp — tick / game logic
void tickEntity(Entity& e);
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

// entity.cpp — AI
int     aiCount(int owner,EntityType t);
int     aiCountAll(int owner,EntityType t);
Entity* aiIdle(int owner,EntityType t);
Entity* aiBldg(int owner,EntityType t);
void    aiGather(int owner);
void    aiBuildSpot(int owner,EntityType bt,int& ox,int& oy);
void    tickAI();

// render.cpp
void initColors();
void render();

// input.cpp
void handleInput(int ch);

// save.cpp
bool saveGame(const char* path);
bool loadGame(const char* path);

// main.cpp
void initGame(int numAIs);
