#include "realm.h"

const TerrainDefinition TERRAIN_DEFS[TERRAIN_COUNT] = {
    {T_GRASS, "grass", CR_NONE, true, false, true, false, 0, G_GRASS, F_NONE},
    {T_TALL_GRASS, "tall grass", CR_NONE, true, false, true, true, 0, G_GRASS, F_NONE},
    {T_FLOWERS, "flowers", CR_NONE, true, false, true, false, 0, G_GRASS, F_NONE},
    {T_MEADOW, "meadow", CR_NONE, true, false, true, false, 0, G_MEADOW, F_NONE},
    {T_FOREST, "forest", CR_WOOD, true, false, false, true, 1, G_GRASS, F_FOREST},
    {T_PINE, "pine forest", CR_WOOD, true, false, false, true, 1, G_GRASS, F_PINE},
    {T_PALM, "palm grove", CR_WOOD, true, false, false, true, 1, G_SAND, F_PALM},
    {T_DEAD_TREE, "dead trees", CR_WOOD, true, false, false, true, 1, G_ASH, F_DEAD_TREE},
    {T_MOUNTAIN, "mountain", CR_NONE, false, false, false, false, 0, G_ROCKY, F_MOUNTAIN_PEAK},
    {T_HILLS, "hills", CR_NONE, true, false, false, false, 0, G_HILLS, F_NONE},
    {T_STONE, "stone", CR_NONE, false, false, false, false, 0, G_ROCKY, F_STONE_BOULDERS},
    {T_WATER, "water", CR_NONE, false, true, false, false, 0, G_WATER, F_NONE},
    {T_SHALLOWS, "shallows", CR_NONE, true, true, false, false, 0, G_SHALLOWS, F_NONE},
    {T_MARSH, "marsh", CR_NONE, true, false, false, false, 0, G_MARSH, F_NONE},
    {T_REEDS, "reeds", CR_NONE, true, false, false, true, 1, G_MARSH, F_REEDS},
    {T_GOLD, "gold", CR_GOLD, true, false, false, false, 0, G_GRAVEL, F_GOLD_DEPOSIT},
    {T_SAND, "sand", CR_NONE, true, false, true, false, 0, G_SAND, F_NONE},
    {T_DUNES, "dunes", CR_NONE, true, false, true, false, 0, G_DUNES, F_NONE},
    {T_SNOW, "snow", CR_NONE, true, false, true, false, 0, G_TUNDRA, F_NONE},
    {T_ICE, "ice", CR_NONE, true, false, false, false, 0, G_ICE, F_NONE},
    {T_DIRT, "dirt", CR_NONE, true, false, true, false, 0, G_DIRT, F_NONE},
    {T_ROAD, "road", CR_NONE, true, false, true, false, 0, G_ROAD, F_NONE},
    {T_MUD, "mud", CR_NONE, true, false, false, false, 0, G_MUD, F_NONE},
    {T_WHEAT, "wheat", CR_FOOD, true, false, true, false, 0, G_MEADOW, F_WHEAT_CROP},
    {T_BERRY, "berry", CR_FOOD, true, false, false, false, 0, G_GRASS, F_BERRY_BUSH},
    {T_FISH, "fish", CR_FISH, false, true, false, false, 0, G_WATER, F_FISH_SHOAL},
    {T_RUINS, "ruins", CR_NONE, true, false, false, false, 0, G_GRAVEL, F_RUINS},
    {T_GRAVEL, "gravel", CR_NONE, true, false, true, false, 0, G_GRAVEL, F_NONE},
    {T_LAVA, "lava", CR_NONE, false, false, false, false, 0, G_LAVA, F_NONE},
    {T_ASH, "ash", CR_NONE, true, false, true, false, 0, G_ASH, F_NONE},
    {T_CASTLE_WALL, "castle wall", CR_NONE, false, false, false, false, 0, G_CASTLE_FLOOR, F_CASTLE_WALL},
    {T_CASTLE_FLOOR, "castle floor", CR_NONE, true, false, true, false, 0, G_CASTLE_FLOOR, F_NONE},
    {T_CASTLE_GATE, "castle gate", CR_NONE, true, false, false, false, 0, G_CASTLE_FLOOR, F_CASTLE_GATE},
};

static_assert(sizeof(TERRAIN_DEFS) / sizeof(TERRAIN_DEFS[0]) == TERRAIN_COUNT, "TERRAIN_DEFS must match Terrain");

const TerrainDefinition& terrainDef(Terrain type) {
    if (type < T_GRASS || type >= TERRAIN_COUNT) return TERRAIN_DEFS[T_GRASS];
    return TERRAIN_DEFS[type];
}

static GroundType defaultGroundForBiome(Biome biome) {
    switch (biome) {
        case B_DESERT: return G_SAND;
        case B_SNOW: return G_TUNDRA;
        case B_SWAMP: return G_MARSH;
        case B_VOLCANIC: return G_ASH;
        case B_OCEAN: return G_SHALLOWS;
        case B_FOREST:
        case B_TEMPERATE:
        default: return G_GRASS;
    }
}

static int defaultFeatureResourceMax(Terrain t) {
    switch (t) {
        case T_FOREST:
        case T_PINE:
        case T_PALM:
        case T_DEAD_TREE: return 120;
        case T_BERRY: return 70;
        case T_WHEAT: return 80;
        case T_FISH: return 90;
        case T_GOLD: return 300;
        default: return 0;
    }
}

static FeatureState featureStateFromResources(Terrain terrain, int resources) {
    int maxResources = defaultFeatureResourceMax(terrain);
    if (maxResources <= 0) return FS_DEFAULT;
    if (resources <= 0) return FS_DEPLETED;
    int pct = (resources * 100) / maxResources;
    if (pct >= 76) return FS_FULL;
    if (pct >= 36) return FS_MOSTLY_FULL;
    return FS_MOSTLY_EMPTY;
}

const char* groundTypeName(GroundType ground) {
    switch (ground) {
        case G_GRASS: return "grass";
        case G_MEADOW: return "meadow";
        case G_DIRT: return "dirt";
        case G_ROAD: return "road";
        case G_MUD: return "mud";
        case G_SAND: return "sand";
        case G_DUNES: return "dunes";
        case G_SNOW: return "snow";
        case G_TUNDRA: return "tundra";
        case G_ICE: return "ice";
        case G_WATER: return "water";
        case G_SHALLOWS: return "shallows";
        case G_MARSH: return "marsh";
        case G_GRAVEL: return "gravel";
        case G_ASH: return "ash";
        case G_LAVA: return "lava";
        case G_HILLS: return "hills";
        case G_ROCKY: return "rocky";
        case G_CASTLE_FLOOR: return "castle_floor";
    }
    return "unknown";
}

const char* featureTypeName(FeatureType feature) {
    switch (feature) {
        case F_NONE: return "none";
        case F_FOREST: return "forest";
        case F_PINE: return "pine";
        case F_PALM: return "palm";
        case F_DEAD_TREE: return "dead_tree";
        case F_BERRY_BUSH: return "berry_bush";
        case F_WHEAT_CROP: return "wheat_crop";
        case F_FISH_SHOAL: return "fish_shoal";
        case F_GOLD_DEPOSIT: return "gold_deposit";
        case F_STONE_BOULDERS: return "stone_boulders";
        case F_MOUNTAIN_PEAK: return "mountain_peak";
        case F_REEDS: return "reeds";
        case F_RUINS: return "ruins";
        case F_CASTLE_WALL: return "castle_wall";
        case F_CASTLE_GATE: return "castle_gate";
    }
    return "unknown";
}

const char* featureStateName(FeatureState state) {
    switch (state) {
        case FS_DEFAULT: return "default";
        case FS_FULL: return "full";
        case FS_MOSTLY_FULL: return "mostly_full";
        case FS_MOSTLY_EMPTY: return "mostly_empty";
        case FS_DEPLETED: return "depleted";
        case FS_OPEN: return "open";
        case FS_CLOSED: return "closed";
        case FS_LOCKED: return "locked";
        case FS_DAMAGED: return "damaged";
        case FS_BROKEN: return "broken";
    }
    return "unknown";
}

const char* visualDecalName(VisualDecalType decal) {
    switch (decal) {
        case VD_FLOWERS: return "flowers";
        case VD_TALL_GRASS: return "tall_grass";
        case VD_SCUFFS: return "scuffs";
        case VD_PACKED_PATH: return "packed_path";
        case VD_COBBLE_PATCH: return "cobble_patch";
        case VD_WHEEL_RUTS: return "wheel_ruts";
        case VD_YARD_CLUTTER: return "yard_clutter";
        case VD_CRATES_BARRELS: return "crates_barrels";
        case VD_LOG_PILES: return "log_piles";
        case VD_FARM_TRACKS: return "farm_tracks";
        case VD_MUDDY_FOOTPRINTS: return "muddy_footprints";
        case VD_SNOW_TRAMPLED_PATH: return "snow_trampled_path";
    }
    return "unknown";
}

uint32_t featureTraits(FeatureType feature) {
    switch (feature) {
        case F_FOREST:
        case F_PINE:
            return FT_SLOWS_MOVEMENT | FT_CONCEALS_UNITS | FT_REDUCES_LINE_OF_SIGHT
                | FT_CAN_HIDE_WILDLIFE | FT_HARVESTABLE;
        case F_REEDS:
            return FT_SLOWS_MOVEMENT | FT_CONCEALS_UNITS | FT_REDUCES_LINE_OF_SIGHT
                | FT_CAN_HIDE_WILDLIFE;
        case F_PALM:
        case F_DEAD_TREE:
        case F_BERRY_BUSH:
        case F_WHEAT_CROP:
        case F_FISH_SHOAL:
        case F_GOLD_DEPOSIT:
            return FT_HARVESTABLE;
        case F_STONE_BOULDERS:
        case F_MOUNTAIN_PEAK:
        case F_CASTLE_WALL:
            return FT_BLOCKS_MOVEMENT;
        case F_CASTLE_GATE:
            return FT_BLOCKS_MOVEMENT;
        case F_RUINS:
        case F_NONE:
        default:
            return 0;
    }
}

bool featureConceals(FeatureType feature) {
    return (featureTraits(feature) & FT_CONCEALS_UNITS) != 0;
}

VisualTileParts visualPartsForTerrain(Terrain terrain, Biome biome, int resources, int wear,
                                       bool gateOpen, bool gateLocked) {
    VisualTileParts parts;
    parts.ground = defaultGroundForBiome(biome);
    parts.featureResources = resources;

    switch (terrain) {
        case T_GRASS: parts.ground = G_GRASS; break;
        case T_TALL_GRASS:
            parts.ground = G_GRASS;
            parts.decals.push_back(VD_TALL_GRASS);
            break;
        case T_FLOWERS:
            parts.ground = G_GRASS;
            parts.decals.push_back(VD_FLOWERS);
            break;
        case T_MEADOW: parts.ground = G_MEADOW; break;
        case T_FOREST: parts.feature = F_FOREST; break;
        case T_PINE:
            parts.ground = biome == B_SNOW ? G_TUNDRA : defaultGroundForBiome(biome);
            parts.feature = F_PINE;
            break;
        case T_PALM:
            parts.ground = G_SAND;
            parts.feature = F_PALM;
            break;
        case T_DEAD_TREE:
            parts.ground = biome == B_VOLCANIC ? G_ASH : defaultGroundForBiome(biome);
            parts.feature = F_DEAD_TREE;
            break;
        case T_MOUNTAIN:
            parts.ground = G_ROCKY;
            parts.feature = F_MOUNTAIN_PEAK;
            break;
        case T_HILLS: parts.ground = G_HILLS; break;
        case T_STONE:
            parts.ground = G_ROCKY;
            parts.feature = F_STONE_BOULDERS;
            break;
        case T_WATER: parts.ground = G_WATER; break;
        case T_SHALLOWS: parts.ground = G_SHALLOWS; break;
        case T_MARSH: parts.ground = G_MARSH; break;
        case T_REEDS:
            parts.ground = (biome == B_OCEAN) ? G_SHALLOWS : G_MARSH;
            parts.feature = F_REEDS;
            break;
        case T_GOLD:
            parts.ground = (biome == B_DESERT) ? G_DIRT : G_ROCKY;
            parts.feature = F_GOLD_DEPOSIT;
            break;
        case T_SAND: parts.ground = G_SAND; break;
        case T_DUNES: parts.ground = G_DUNES; break;
        case T_SNOW: parts.ground = biome == B_SNOW ? G_TUNDRA : G_SNOW; break;
        case T_ICE: parts.ground = G_ICE; break;
        case T_DIRT: parts.ground = G_DIRT; break;
        case T_ROAD: parts.ground = G_ROAD; break;
        case T_MUD: parts.ground = G_MUD; break;
        case T_WHEAT:
            parts.ground = G_DIRT;
            parts.feature = F_WHEAT_CROP;
            break;
        case T_BERRY:
            parts.ground = defaultGroundForBiome(biome);
            parts.feature = F_BERRY_BUSH;
            break;
        case T_FISH:
            parts.ground = biome == B_OCEAN ? G_WATER : G_SHALLOWS;
            parts.feature = F_FISH_SHOAL;
            break;
        case T_RUINS:
            parts.ground = G_GRAVEL;
            parts.feature = F_RUINS;
            break;
        case T_GRAVEL: parts.ground = G_GRAVEL; break;
        case T_LAVA: parts.ground = G_LAVA; break;
        case T_ASH: parts.ground = G_ASH; break;
        case T_CASTLE_WALL:
            parts.ground = G_CASTLE_FLOOR;
            parts.feature = F_CASTLE_WALL;
            break;
        case T_CASTLE_FLOOR: parts.ground = G_CASTLE_FLOOR; break;
        case T_CASTLE_GATE:
            parts.ground = G_CASTLE_FLOOR;
            parts.feature = F_CASTLE_GATE;
            parts.featureState = gateLocked ? FS_LOCKED : gateOpen ? FS_OPEN : FS_CLOSED;
            break;
        case TERRAIN_COUNT:
            break;
    }

    if (parts.feature != F_NONE && parts.featureState == FS_DEFAULT)
        parts.featureState = featureStateFromResources(terrain, resources);
    parts.featureTraits = featureTraits(parts.feature);

    if (wear >= 80) parts.decals.push_back(VD_COBBLE_PATCH);
    else if (wear >= 55) parts.decals.push_back(VD_PACKED_PATH);
    else if (wear >= 25) parts.decals.push_back(VD_SCUFFS);
    if (wear >= 45) {
        parts.decals.push_back(terrain == T_SNOW ? VD_SNOW_TRAMPLED_PATH
                              : terrain == T_MUD ? VD_MUDDY_FOOTPRINTS
                              : VD_WHEEL_RUTS);
    }
    return parts;
}

VisualTileParts visualPartsForTile(const Tile& tile) {
    return visualPartsForTerrain(tile.terrain, tile.biome, tile.resources, tile.wear);
}

bool isConcealingTile(int x, int y) {
    return isConcealingTile(g, x, y);
}

bool isConcealingTile(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    return featureConceals(visualPartsForTile(game.map[y][x]).feature);
}

int movementPenaltyForTile(const Tile& tile) {
    VisualTileParts parts = visualPartsForTile(tile);
    return (parts.featureTraits & FT_SLOWS_MOVEMENT) ? 1 : 0;
}
