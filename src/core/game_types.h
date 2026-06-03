#pragma once

#include <cstdint>
#include <vector>

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

struct EntityStats {
    const char* name; char glyph;
    int maxHp, atk, range, speed, atkSpeed, costGold, costWood, costFood, trainTime;
    int sizeW, sizeH, supplyProvided, supplyUsed; bool isBuilding;
    uint32_t traits;
};
using EntityDefinition = EntityStats;

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

struct OccupancyGrid {
    bool occupied[MAP_H][MAP_W];
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
