#include "realm.h"
#include "display.h"

// ============================================================
// 256-COLOR PALETTE
// xterm-256: 16-231 = 6x6x6 cube (16+36r+6g+b), 232-255 = grayscale
// ============================================================
namespace C {
    const int DARK_GREEN    = 22;
    const int MED_GREEN     = 28;
    const int GREEN         = 34;
    const int BRIGHT_GREEN  = 40;
    const int PALE_GREEN    = 114;
    const int OLIVE         = 70;
    const int YELLOW_GREEN  = 106;
    const int PINE_GREEN    = 23;
    const int SWAMP_GREEN   = 58;

    const int DEEP_BLUE     = 17;
    const int NAVY          = 18;
    const int MED_BLUE      = 25;
    const int TEAL          = 30;
    const int ICE_BLUE      = 117;

    const int BROWN         = 94;
    const int AMBER         = 130;
    const int TAN           = 137;
    const int LIGHT_TAN     = 180;
    const int DARK_GOLD     = 136;
    const int ORANGE        = 172;

    const int GOLD          = 178;
    const int BRIGHT_GOLD   = 220;
    const int WHEAT_GOLD    = 143;

    const int RED           = 160;
    const int BRIGHT_RED    = 196;
    const int BERRY_RED     = 125;

    const int LAVENDER      = 140;
    const int DUSK_PURPLE   = 53;

    const int SNOW_WHITE    = 255;
    const int BRIGHT_GRAY   = 252;
    const int LIGHT_GRAY    = 248;
    const int MED_GRAY      = 244;
    const int GRAY          = 240;
    const int DARK_GRAY     = 236;
    const int DARKER_GRAY   = 234;
    const int NEAR_BLACK    = 232;

    const int PLAYER_CYAN   = 39;
    const int PLAYER_DIM    = 31;
    const int ENEMY_RED     = 196;
    const int ENEMY_DIM     = 124;

    const int UI_BG         = 17;
    const int UI_TEXT       = 252;
    const int UI_HIGHLIGHT  = 220;
    const int UI_ACCENT     = 81;
    const int UI_DIM        = 240;
}

// Extra colour pair IDs used only by emoji mode. Kept high to avoid
// colliding with the project's CP_* enum. Valid on normal 256-pair terminals.
// These pairs are deliberately terrain/biome backgrounds first; resources such
// as gold/trees/wheat use their emoji glyph but stay on the underlying biome bg.
static constexpr int CP_EMOJI_TEMP_0     = 220;
static constexpr int CP_EMOJI_TEMP_1     = 221;
static constexpr int CP_EMOJI_TEMP_2     = 222;
static constexpr int CP_EMOJI_TEMP_3     = 223;
static constexpr int CP_EMOJI_TEMP_4     = 224;
static constexpr int CP_EMOJI_TEMP_5     = 225;
static constexpr int CP_EMOJI_FOREST_0   = 226;
static constexpr int CP_EMOJI_FOREST_1   = 227;
static constexpr int CP_EMOJI_FOREST_2   = 228;
static constexpr int CP_EMOJI_FOREST_3   = 229;
static constexpr int CP_EMOJI_FOREST_4   = 230;
static constexpr int CP_EMOJI_FOREST_5   = 231;
static constexpr int CP_EMOJI_DESERT_0   = 232;
static constexpr int CP_EMOJI_DESERT_1   = 233;
static constexpr int CP_EMOJI_DESERT_2   = 234;
static constexpr int CP_EMOJI_DESERT_3   = 235;
static constexpr int CP_EMOJI_SNOW_0     = 236;
static constexpr int CP_EMOJI_SNOW_1     = 237;
static constexpr int CP_EMOJI_SNOW_2     = 238;
static constexpr int CP_EMOJI_SNOW_3     = 239;
static constexpr int CP_EMOJI_SWAMP_0    = 240;
static constexpr int CP_EMOJI_SWAMP_1    = 241;
static constexpr int CP_EMOJI_SWAMP_2    = 242;
static constexpr int CP_EMOJI_SWAMP_3    = 243;
static constexpr int CP_EMOJI_VOLCANIC_0 = 244;
static constexpr int CP_EMOJI_VOLCANIC_1 = 245;
static constexpr int CP_EMOJI_VOLCANIC_2 = 246;
static constexpr int CP_EMOJI_VOLCANIC_3 = 247;
static constexpr int CP_EMOJI_OCEAN_0    = 248;
static constexpr int CP_EMOJI_OCEAN_1    = 249;
static constexpr int CP_EMOJI_OCEAN_2    = 250;
static constexpr int CP_EMOJI_OCEAN_3    = 251;
static constexpr int CP_EMOJI_WATER      = 252;
static constexpr int CP_EMOJI_SHALLOWS   = 253;
static constexpr int CP_EMOJI_DARK       = 254;
static constexpr int CP_EMOJI_EDGE       = 255;
static constexpr int CP_EMOJI_MAX        = CP_EMOJI_EDGE;
static bool gEmojiBiomePairsReady = false;

// ============================================================
// COLOR INIT
// ============================================================
void initColors() {
    start_color();
    use_default_colors();

    const int bg = -1;
    const bool emojiTiles = (displayMode == DM_EMOJI);

    // In ASCII mode, keep the original mostly-transparent backgrounds.
    // In full emoji mode, each map tile is two terminal cells wide, so give
    // terrain/entity colour pairs real backgrounds and paint both cells before
    // writing the emoji.
    auto tileBg = [&](int emojiBg) -> int {
        return emojiTiles ? emojiBg : bg;
    };

    init_pair(CP_GRASS,         C::GREEN,        tileBg(C::DARK_GREEN));
    init_pair(CP_GRASS_LIGHT,   C::BRIGHT_GREEN, tileBg(C::MED_GREEN));
    init_pair(CP_GRASS_DRY,     C::YELLOW_GREEN, tileBg(C::OLIVE));
    init_pair(CP_TALL_GRASS,    C::MED_GREEN,    tileBg(C::DARK_GREEN));
    init_pair(CP_FLOWERS,       C::LAVENDER,     tileBg(C::MED_GREEN));
    init_pair(CP_FLOWERS_BLUE,  C::MED_BLUE,     tileBg(C::MED_GREEN));
    init_pair(CP_FLOWERS_YELLOW,C::BRIGHT_GOLD,  tileBg(C::MED_GREEN));
    init_pair(CP_FLOWERS_RED,   C::BERRY_RED,    tileBg(C::MED_GREEN));
    init_pair(CP_MEADOW,        C::PALE_GREEN,   tileBg(C::GREEN));

    init_pair(CP_FOREST,        C::MED_GREEN,    tileBg(C::DARK_GREEN));
    init_pair(CP_FOREST_DARK,   C::DARK_GREEN,   tileBg(C::PINE_GREEN));
    init_pair(CP_PINE,          C::PINE_GREEN,   tileBg(C::DARK_GREEN));
    init_pair(CP_PALM,          C::GREEN,        tileBg(C::DARK_GREEN));
    init_pair(CP_DEAD_TREE,     C::GRAY,         tileBg(C::BROWN));

    init_pair(CP_MOUNTAIN,      C::LIGHT_GRAY,   tileBg(C::MED_GRAY));
    init_pair(CP_HILLS,         C::OLIVE,        tileBg(C::BROWN));
    init_pair(CP_STONE,         C::MED_GRAY,     tileBg(C::DARK_GRAY));

    init_pair(CP_WATER,         C::ICE_BLUE,     C::DEEP_BLUE);
    init_pair(CP_WATER_SHIMMER, C::SNOW_WHITE,   C::DEEP_BLUE);
    init_pair(CP_SHALLOWS,      C::SNOW_WHITE,   C::TEAL);
    init_pair(CP_MARSH,         C::SWAMP_GREEN,  tileBg(C::SWAMP_GREEN));
    init_pair(CP_REEDS,         C::DARK_GOLD,    tileBg(C::SWAMP_GREEN));

    init_pair(CP_GOLD,          C::BRIGHT_GOLD,  tileBg(C::DARK_GOLD));
    init_pair(CP_GOLD_SHIMMER,  C::GOLD,         tileBg(C::DARK_GOLD));

    init_pair(CP_SAND,          C::TAN,          tileBg(C::TAN));
    init_pair(CP_DUNES,         C::LIGHT_TAN,    tileBg(C::LIGHT_TAN));
    init_pair(CP_SNOW_GROUND,   C::SNOW_WHITE,   bg);
    init_pair(CP_ICE,           C::MED_BLUE,     C::ICE_BLUE);

    init_pair(CP_DIRT,          C::BROWN,        tileBg(C::BROWN));
    init_pair(CP_ROAD,          C::LIGHT_GRAY,   tileBg(C::DARK_GRAY));

    init_pair(CP_WHEAT,         C::WHEAT_GOLD,   tileBg(C::DARK_GOLD));
    init_pair(CP_WHEAT_GOLD,    C::BRIGHT_GOLD,  tileBg(C::DARK_GOLD));
    init_pair(CP_BERRY,         C::BERRY_RED,    C::DARK_GREEN);

    init_pair(CP_RUINS,         C::GRAY,         tileBg(C::DARK_GRAY));
    init_pair(CP_GRAVEL,        C::MED_GRAY,     tileBg(C::GRAY));

    init_pair(CP_CASTLE_WALL,   C::BRIGHT_GRAY,  tileBg(C::DARK_GRAY));
    init_pair(CP_CASTLE_FLOOR,  C::DARK_GOLD,    tileBg(C::BROWN));
    init_pair(CP_CASTLE_GATE,   C::AMBER,        tileBg(C::DARK_GRAY));

    init_pair(CP_AUT_TREE_EARLY, C::YELLOW_GREEN, tileBg(C::OLIVE));
    init_pair(CP_AUT_TREE_MID,   C::ORANGE,       tileBg(C::BROWN));
    init_pair(CP_AUT_TREE_LATE,  C::BROWN,        tileBg(C::BROWN));
    init_pair(CP_AUT_TREE_GOLD,  C::BRIGHT_GOLD,  tileBg(C::DARK_GOLD));
    init_pair(CP_AUT_TREE_RED,   C::RED,          tileBg(C::BROWN));
    init_pair(CP_AUT_GRASS,      C::OLIVE,        tileBg(C::OLIVE));
    init_pair(CP_AUT_GRASS_LATE, C::BROWN,        tileBg(C::BROWN));

    init_pair(CP_WIN_GROUND,     C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_TREE,       C::MED_GRAY,     bg);
    init_pair(CP_WIN_PINE,       C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_ICE,        C::ICE_BLUE,     C::NAVY);

    init_pair(CP_NIGHT_GRASS,    C::DARK_GREEN,   tileBg(C::NEAR_BLACK));
    init_pair(CP_NIGHT_TREE,     C::DARK_GRAY,    bg);
    init_pair(CP_NIGHT_WATER,    C::NAVY,         C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   C::DARKER_GRAY,  tileBg(C::NEAR_BLACK));
    init_pair(CP_NIGHT_GOLD,     C::DARK_GOLD,    tileBg(C::NEAR_BLACK));
    init_pair(CP_NIGHT_SNOW,     C::MED_GRAY,     bg);

    init_pair(CP_DAWN_SKY,       C::ORANGE,       bg);
    init_pair(CP_DUSK_SKY,       C::DUSK_PURPLE,  bg);

    init_pair(CP_PLAYER,         C::PLAYER_CYAN,  tileBg(C::NEAR_BLACK));
    init_pair(CP_PLAYER_NIGHT,   C::PLAYER_DIM,   tileBg(C::NEAR_BLACK));
    init_pair(CP_ENEMY,          C::ENEMY_RED,    tileBg(C::NEAR_BLACK));
    init_pair(CP_ENEMY_NIGHT,    C::ENEMY_DIM,    tileBg(C::NEAR_BLACK));

    // Ship deck: glyph sits on a wood-brown background tile so boats read as
    // solid hulls instead of single floating characters on open water.
    init_pair(CP_SHIP_PLAYER,    C::PLAYER_CYAN,  C::BROWN);
    init_pair(CP_SHIP_ENEMY,     C::ENEMY_RED,    C::BROWN);
    init_pair(CP_SHIP_P0,        C::PLAYER_CYAN,  C::BROWN);
    init_pair(CP_SHIP_P1,        C::ENEMY_RED,    C::BROWN);
    init_pair(CP_SHIP_P2,        C::ORANGE,       C::BROWN);
    init_pair(CP_SHIP_P3,        C::DUSK_PURPLE,  C::BROWN);

    init_pair(CP_PROJ_ARROW,     C::BRIGHT_GOLD,  tileBg(C::NEAR_BLACK));
    init_pair(CP_PROJ_BOULDER,   C::BRIGHT_GRAY,  tileBg(C::NEAR_BLACK));
    init_pair(CP_PROJ_TOWER,     C::BRIGHT_RED,   tileBg(C::NEAR_BLACK));

    // Weather overlays remain transparent so they do not repaint terrain.
    init_pair(CP_RAIN,           C::ICE_BLUE,     bg);
    init_pair(CP_SNOW_FALL,      C::SNOW_WHITE,   bg);

    init_pair(CP_UI_BAR,         C::UI_TEXT,      C::UI_BG);
    init_pair(CP_UI_TEXT,        C::UI_TEXT,      bg);
    init_pair(CP_UI_HIGH,        C::UI_HIGHLIGHT, bg);
    init_pair(CP_UI_DIM,         C::UI_DIM,       bg);
    init_pair(CP_UI_ACCENT,      C::UI_ACCENT,    bg);
    init_pair(CP_FOG,            C::DARKER_GRAY,  bg);
    init_pair(CP_FOG_EXPLORED,   C::DARK_GRAY,    tileBg(C::NEAR_BLACK));

    // Cliff faces: pale rock on earth-brown — reads as terrain relief, not wall.
    init_pair(CP_CLIFF,          C::LIGHT_GRAY,   C::BROWN);
    // Fallen-soldier markers: dim blood-red on dark ground.
    init_pair(CP_CORPSE,         C::BERRY_RED,    tileBg(C::DARKER_GRAY));
    // Night torches: a warm flame, gold/amber over the dark.
    init_pair(CP_TORCH,          C::BRIGHT_GOLD,  tileBg(C::BROWN));

    // Cursor: black-on-gold pops on snow, grass, water, and dark biomes alike.
    init_pair(CP_CURSOR,         C::NEAR_BLACK,   C::BRIGHT_GOLD);
    // Build placement preview: green footprint = canPlace, red = blocked.
    init_pair(CP_BUILD_OK,       C::NEAR_BLACK,   C::BRIGHT_GREEN);
    init_pair(CP_BUILD_BAD,      C::NEAR_BLACK,   C::RED);
    init_pair(CP_HP_GREEN,       C::BRIGHT_GREEN, bg);
    init_pair(CP_HP_YELLOW,      C::BRIGHT_GOLD,  bg);
    init_pair(CP_HP_RED,         C::RED,          bg);
    init_pair(CP_SUN,            C::BRIGHT_GOLD,  C::UI_BG);
    init_pair(CP_MOON,           C::SNOW_WHITE,   C::UI_BG);

    init_pair(CP_MM_PLAYER,      C::PLAYER_CYAN,  C::NEAR_BLACK);
    init_pair(CP_MM_ENEMY,       C::ENEMY_RED,    C::NEAR_BLACK);
    init_pair(CP_MM_WATER,       C::MED_BLUE,     C::NEAR_BLACK);
    init_pair(CP_MM_FOREST,      C::DARK_GREEN,   C::NEAR_BLACK);
    init_pair(CP_MM_GOLD,        C::GOLD,         C::NEAR_BLACK);
    init_pair(CP_MM_SAND,        C::TAN,          C::NEAR_BLACK);
    init_pair(CP_MM_SNOW,        C::SNOW_WHITE,   C::NEAR_BLACK);
    init_pair(CP_MM_MTN,         C::LIGHT_GRAY,   C::NEAR_BLACK);
    init_pair(CP_MM_CASTLE,      C::BRIGHT_GRAY,  C::NEAR_BLACK);

    init_pair(CP_SPRING_FLOWER,  C::LAVENDER,     tileBg(C::MED_GREEN));

    init_pair(CP_LAVA,           C::ORANGE,       C::RED);
    init_pair(CP_LAVA_HOT,       C::BRIGHT_GOLD,  C::RED);
    init_pair(CP_ASH,            C::DARK_GRAY,    tileBg(C::NEAR_BLACK));

    // Neutral animals do not get player ownership colours, but in emoji mode
    // still get a real terrain-like background so the tile stays filled.
    init_pair(CP_DEER,           C::TAN,          tileBg(C::DARK_GREEN));
    init_pair(CP_WOLF,           C::LIGHT_GRAY,   tileBg(C::DARK_GREEN));
    init_pair(CP_SHEEP,          C::SNOW_WHITE,   tileBg(C::DARK_GREEN));
    init_pair(CP_BOAR,           C::BROWN,        tileBg(C::DARK_GREEN));
    init_pair(CP_MM_ANIMAL,      C::TAN,          C::NEAR_BLACK);

    // Ownership background colour pairs.
    // Land units and buildings display the owner's colour as the BACKGROUND
    // so ownership is visible regardless of what glyph mode (ASCII/emoji)
    // is active. Ships keep CP_SHIP_* (wood deck bg) for their hull look.
    init_pair(CP_OWN_P0,       C::SNOW_WHITE,   C::PLAYER_CYAN);
    init_pair(CP_OWN_P0_NIGHT, C::LIGHT_GRAY,   C::PLAYER_DIM);
    init_pair(CP_OWN_P1,       C::SNOW_WHITE,   C::ENEMY_RED);
    init_pair(CP_OWN_P1_NIGHT, C::LIGHT_GRAY,   C::ENEMY_DIM);
    init_pair(CP_OWN_P2,       C::NEAR_BLACK,   C::ORANGE);
    init_pair(CP_OWN_P2_NIGHT, C::NEAR_BLACK,   C::AMBER);
    init_pair(CP_OWN_P3,       C::SNOW_WHITE,   C::DUSK_PURPLE);
    init_pair(CP_OWN_P3_NIGHT, C::LIGHT_GRAY,   C::GRAY);

    gEmojiBiomePairsReady = false;
    if (COLOR_PAIRS > CP_EMOJI_MAX) {
        // Temperate: six subtly different greens/olive tones. Later seasonal
        // selection chooses warmer/winter variants from the same range.
        init_pair(CP_EMOJI_TEMP_0,     C::GREEN,        C::DARK_GREEN);
        init_pair(CP_EMOJI_TEMP_1,     C::BRIGHT_GREEN, C::MED_GREEN);
        init_pair(CP_EMOJI_TEMP_2,     C::PALE_GREEN,   C::GREEN);
        init_pair(CP_EMOJI_TEMP_3,     C::YELLOW_GREEN, C::OLIVE);
        init_pair(CP_EMOJI_TEMP_4,     C::WHEAT_GOLD,   C::OLIVE);
        init_pair(CP_EMOJI_TEMP_5,     C::LIGHT_GRAY,   C::BROWN);

        // Forest: deep canopy, brighter clearings, autumn browns/golds.
        init_pair(CP_EMOJI_FOREST_0,   C::DARK_GREEN,   C::PINE_GREEN);
        init_pair(CP_EMOJI_FOREST_1,   C::GREEN,        C::DARK_GREEN);
        init_pair(CP_EMOJI_FOREST_2,   C::BRIGHT_GREEN, C::MED_GREEN);
        init_pair(CP_EMOJI_FOREST_3,   C::YELLOW_GREEN, C::OLIVE);
        init_pair(CP_EMOJI_FOREST_4,   C::ORANGE,       C::BROWN);
        init_pair(CP_EMOJI_FOREST_5,   C::BRIGHT_GOLD,  C::BROWN);

        // Other biomes get four variants each.
        init_pair(CP_EMOJI_DESERT_0,   C::BROWN,        C::TAN);
        init_pair(CP_EMOJI_DESERT_1,   C::NEAR_BLACK,   C::LIGHT_TAN);
        init_pair(CP_EMOJI_DESERT_2,   C::DARK_GOLD,    C::TAN);
        init_pair(CP_EMOJI_DESERT_3,   C::BRIGHT_GOLD,  C::LIGHT_TAN);

        init_pair(CP_EMOJI_SNOW_0,     C::LIGHT_GRAY,   C::SNOW_WHITE);
        init_pair(CP_EMOJI_SNOW_1,     C::MED_GRAY,     C::LIGHT_GRAY);
        init_pair(CP_EMOJI_SNOW_2,     C::ICE_BLUE,     C::SNOW_WHITE);
        init_pair(CP_EMOJI_SNOW_3,     C::NEAR_BLACK,   C::SNOW_WHITE);

        init_pair(CP_EMOJI_SWAMP_0,    C::DARK_GREEN,   C::SWAMP_GREEN);
        init_pair(CP_EMOJI_SWAMP_1,    C::BRIGHT_GREEN, C::DARK_GREEN);
        init_pair(CP_EMOJI_SWAMP_2,    C::OLIVE,        C::SWAMP_GREEN);
        init_pair(CP_EMOJI_SWAMP_3,    C::DARK_GOLD,    C::SWAMP_GREEN);

        init_pair(CP_EMOJI_VOLCANIC_0, C::DARK_GRAY,    C::NEAR_BLACK);
        init_pair(CP_EMOJI_VOLCANIC_1, C::ORANGE,       C::DARKER_GRAY);
        init_pair(CP_EMOJI_VOLCANIC_2, C::RED,          C::NEAR_BLACK);
        init_pair(CP_EMOJI_VOLCANIC_3, C::BRIGHT_GOLD,  C::RED);

        init_pair(CP_EMOJI_OCEAN_0,    C::MED_BLUE,     C::DEEP_BLUE);
        init_pair(CP_EMOJI_OCEAN_1,    C::ICE_BLUE,     C::NAVY);
        init_pair(CP_EMOJI_OCEAN_2,    C::SNOW_WHITE,   C::TEAL);
        init_pair(CP_EMOJI_OCEAN_3,    C::TEAL,         C::DEEP_BLUE);

        init_pair(CP_EMOJI_WATER,      C::ICE_BLUE,     C::DEEP_BLUE);
        init_pair(CP_EMOJI_SHALLOWS,   C::SNOW_WHITE,   C::TEAL);
        init_pair(CP_EMOJI_DARK,       C::DARK_GRAY,    C::NEAR_BLACK);
        init_pair(CP_EMOJI_EDGE,       C::BRIGHT_GOLD,  C::DARK_GREEN);
        gEmojiBiomePairsReady = true;
    }
}

// ============================================================
// OWNERSHIP COLOUR HELPER
// Returns the colour pair that should be applied to a land unit
// or building based on its owner.  Ships are excluded (callers
// handle CP_SHIP_* separately).  Animals/Gaia use their own
// type-specific pairs and are never passed here.
// ============================================================
static int ownerColorPair(int owner, bool night) {
    if (night) {
        switch (owner) {
            case 0:  return CP_OWN_P0_NIGHT;
            case 1:  return CP_OWN_P1_NIGHT;
            case 2:  return CP_OWN_P2_NIGHT;
            default: return CP_OWN_P3_NIGHT;
        }
    }
    switch (owner) {
        case 0:  return CP_OWN_P0;
        case 1:  return CP_OWN_P1;
        case 2:  return CP_OWN_P2;
        default: return CP_OWN_P3;
    }
}

static unsigned tileHash(int x, int y, unsigned salt = 0) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + salt * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float hash01(int x, int y, unsigned salt = 0) {
    return (tileHash(x, y, salt) & 0xFFFFu) / 65535.0f;
}

static float smooth01(float t) {
    return t * t * (3.0f - 2.0f * t);
}

static float paintedNoise(int x, int y, int scale, unsigned salt) {
    int x0 = x / scale, y0 = y / scale;
    float fx = (float)(x % scale) / (float)scale;
    float fy = (float)(y % scale) / (float)scale;
    fx = smooth01(fx); fy = smooth01(fy);

    float a = hash01(x0,   y0,   salt);
    float b = hash01(x0+1, y0,   salt);
    float c = hash01(x0,   y0+1, salt);
    float d = hash01(x0+1, y0+1, salt);
    float ab = a + (b - a) * fx;
    float cd = c + (d - c) * fx;
    return ab + (cd - ab) * fy;
}

static int biomeBoundaryCount(int x, int y, Biome b) {
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        int nx = x + dx, ny = y + dy;
        if (!inBounds(nx, ny)) continue;
        if (g.map[ny][nx].biome != b) count++;
    }
    return count;
}

static int terrainBoundaryCount(int x, int y, Terrain t) {
    int count = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        int nx = x + dx, ny = y + dy;
        if (!inBounds(nx, ny)) continue;
        if (g.map[ny][nx].terrain != t) count++;
    }
    return count;
}

static int clampShade(int v, int maxShade) {
    return std::max(0, std::min(v, maxShade));
}

static int paintedShadeFor(const Tile& tile, int x, int y, int maxShade) {
    // Two broad value-noise layers give airbrushed colour fields; a small hash
    // adds pixel-level hand-painted variation without making the board noisy.
    float broad  = paintedNoise(x, y, 16, 3);
    float detail = paintedNoise(x, y,  6, 9);
    float grain  = hash01(x, y, 17);
    float v = broad * 0.60f + detail * 0.30f + grain * 0.10f;

    int shade = (int)(v * (float)(maxShade + 1));
    shade = clampShade(shade, maxShade);

    // Boundaries get a little more contrast so biomes/terrain masses read
    // clearly. The sign is deterministic so it looks painted, not random.
    int biomeEdge = biomeBoundaryCount(x, y, tile.biome);
    int terrainEdge = terrainBoundaryCount(x, y, tile.terrain);
    if (biomeEdge > 0 || terrainEdge >= 5) {
        int push = (biomeEdge >= 3 || terrainEdge >= 6) ? 2 : 1;
        if (hash01(x, y, 27) < 0.55f) shade += push;
        else                          shade -= 1;
    }
    return clampShade(shade, maxShade);
}

static int emojiTerrainColorPair(const Tile& tile, int x, int y, bool night) {
    if (!gEmojiBiomePairsReady) {
        switch (tile.biome) {
            case B_DESERT:   return CP_SAND;
            case B_SNOW:     return CP_SNOW_GROUND;
            case B_SWAMP:    return CP_MARSH;
            case B_FOREST:   return CP_FOREST;
            case B_VOLCANIC: return CP_ASH;
            case B_OCEAN:    return CP_WATER;
            case B_TEMPERATE:
            default:         return CP_GRASS;
        }
    }

    // Strong physical surfaces keep their terrain colour. Resource overlays
    // such as gold, trees, wheat, berries, and fish deliberately do not change
    // the background; they sit on the underlying biome/ground colour.
    switch (tile.terrain) {
        case T_WATER:
        case T_FISH:
            return (paintedShadeFor(tile, x, y, 3) >= 2) ? CP_EMOJI_OCEAN_1 : CP_EMOJI_WATER;
        case T_SHALLOWS:
            return CP_EMOJI_SHALLOWS;
        case T_ICE:
            return (hash01(x, y, 39) < 0.5f) ? CP_EMOJI_SNOW_2 : CP_EMOJI_WATER;
        case T_SNOW:
            return CP_EMOJI_SNOW_0 + paintedShadeFor(tile, x, y, 3);
        case T_LAVA:
            return CP_EMOJI_VOLCANIC_2 + (int)(hash01(x, y, 41) < 0.35f);
        case T_ASH:
            return CP_EMOJI_VOLCANIC_0 + std::min(1, paintedShadeFor(tile, x, y, 3));
        default:
            break;
    }

    Season season = getSeason();
    int shade;
    if (night) return CP_EMOJI_DARK;

    switch (tile.biome) {
        case B_DESERT:
            shade = paintedShadeFor(tile, x, y, 3);
            return CP_EMOJI_DESERT_0 + shade;

        case B_SNOW:
            shade = paintedShadeFor(tile, x, y, 3);
            if (season == SUMMER && tile.terrain != T_SNOW) return CP_EMOJI_TEMP_1 + std::min(2, shade);
            if (season == SPRING && tile.terrain != T_SNOW && hash01(x,y,52) < 0.35f) return CP_EMOJI_TEMP_0 + std::min(2, shade);
            return CP_EMOJI_SNOW_0 + shade;

        case B_SWAMP:
            shade = paintedShadeFor(tile, x, y, 3);
            return CP_EMOJI_SWAMP_0 + shade;

        case B_FOREST:
            shade = paintedShadeFor(tile, x, y, 5);
            if (season == AUTUMN) shade = std::max(shade, 3);
            else if (season == WINTER && hash01(x,y,61) < 0.35f) return CP_EMOJI_SNOW_1 + std::min(2, shade % 3);
            else if (season == SPRING) shade = std::min(3, shade + 1);
            return CP_EMOJI_FOREST_0 + clampShade(shade, 5);

        case B_VOLCANIC:
            shade = paintedShadeFor(tile, x, y, 3);
            return CP_EMOJI_VOLCANIC_0 + shade;

        case B_OCEAN:
            shade = paintedShadeFor(tile, x, y, 3);
            return CP_EMOJI_OCEAN_0 + shade;

        case B_TEMPERATE:
        default:
            shade = paintedShadeFor(tile, x, y, 5);
            if (season == SPRING) shade = std::min(3, shade + 1);
            else if (season == SUMMER) shade = std::min(4, shade + (hash01(x,y,71) < 0.35f ? 1 : 0));
            else if (season == AUTUMN) shade = std::max(shade, 3);
            else if (season == WINTER && hash01(x,y,73) < 0.25f) return CP_EMOJI_SNOW_1 + std::min(2, shade % 3);
            return CP_EMOJI_TEMP_0 + clampShade(shade, 5);
    }
}

static const char* terrainSymbolVariant(Terrain t, char ch, int x, int y) {
    // Real resources/objects get emojis. Decorative ground keeps simple symbols.
    unsigned h = tileHash(x, y, 101) + (unsigned)(g.tick / 24);
    switch (t) {
        case T_FOREST:       return (ch == 't') ? u8"🪵" : ((h & 1u) ? u8"🌳" : u8"🌲");
        case T_PINE:         return u8"🌲";
        case T_PALM:         return u8"🌴";
        case T_DEAD_TREE:    return u8"🪵";
        case T_GOLD:         return u8"🪙";
        case T_WHEAT:        return u8"🌾";
        case T_BERRY:        return u8"🫐";
        case T_FISH:         return u8"🐟";

        case T_GRASS: {
            static const char* v[] = {u8"·", u8"∙", u8"ˑ", u8" "};
            return v[tileHash(x,y,111) % 4];
        }
        case T_TALL_GRASS: {
            static const char* v[] = {u8"╎", u8"╏", u8"⁝", u8"┆"};
            return v[(tileHash(x,y,113) + (unsigned)(g.tick/16)) % 4];
        }
        case T_FLOWERS: {
            static const char* v[] = {u8"✿", u8"✣", u8"✽", u8"·"};
            return v[tileHash(x,y,115) % 4];
        }
        case T_MEADOW: {
            static const char* v[] = {u8"∙", u8"·", u8"ˑ", u8"∴"};
            return v[tileHash(x,y,117) % 4];
        }
        case T_MOUNTAIN:     return (hash01(x,y,119) < 0.5f) ? u8"▲" : u8"△";
        case T_HILLS:        return (hash01(x,y,121) < 0.5f) ? u8"⌒" : u8"⌁";
        case T_STONE:        return (hash01(x,y,123) < 0.5f) ? u8"▪" : u8"▫";
        case T_WATER: {
            static const char* v[] = {u8"≈", u8"∼", u8"≋", u8"≈"};
            return v[(tileHash(x,y,125) + (unsigned)(g.tick/10)) % 4];
        }
        case T_SHALLOWS: {
            static const char* v[] = {u8"∼", u8"≈", u8"⌁", u8"∼"};
            return v[(tileHash(x,y,127) + (unsigned)(g.tick/12)) % 4];
        }
        case T_MARSH:        return (ch == '-') ? u8"∼" : ((h & 1u) ? u8"≋" : u8"⌁");
        case T_REEDS:        return (ch == '/') ? u8"╱" : (ch == '\\') ? u8"╲" : ((h & 1u) ? u8"╎" : u8"╏");
        case T_SAND: {
            static const char* v[] = {u8"·", u8"ˑ", u8"∴", u8" "};
            return v[tileHash(x,y,129) % 4];
        }
        case T_DUNES:        return (hash01(x,y,131) < 0.5f) ? u8"∿" : u8"⌒";
        case T_SNOW:         return (hash01(x,y,133) < 0.35f) ? u8"˚" : u8"·";
        case T_ICE:          return (hash01(x,y,135) < 0.5f) ? u8"═" : u8"─";
        case T_DIRT:         return (hash01(x,y,137) < 0.5f) ? u8"·" : u8"∙";
        case T_ROAD:         return (hash01(x,y,139) < 0.5f) ? u8"─" : u8"━";
        case T_MUD:          return (hash01(x,y,141) < 0.5f) ? u8"∙" : u8"⁘";
        case T_RUINS:        return (hash01(x,y,143) < 0.5f) ? u8"⌂" : u8"⌐";
        case T_GRAVEL:       return (hash01(x,y,145) < 0.5f) ? u8"⁘" : u8"▫";
        case T_LAVA:         return (ch == '*') ? u8"✦" : (ch == '=') ? u8"≋" : u8"≈";
        case T_ASH:          return (hash01(x,y,147) < 0.5f) ? u8"░" : u8"·";
        case T_CASTLE_WALL:  return u8"▓";
        case T_CASTLE_FLOOR: return (hash01(x,y,149) < 0.5f) ? u8"·" : u8"∙";
        case T_CASTLE_GATE:  return u8"▣";
        case T_BRIDGE:       return u8"🌉";
        case T_MONOLITH:     return u8"🗿";
    }
    return getCharEmoji(ch);
}

static int ownerPersonVariant(int owner) {
    if (owner < 0) return 0;
    return owner % 3;
}

static const char* peasantEmojiForState(const Entity& e) {
    static const char* standing[3] = { u8"🧍‍♂️", u8"🧍", u8"🧍‍♀️" };
    static const char* walking [3] = { u8"🚶‍♂️", u8"🚶", u8"🚶‍♀️" };
    static const char* kneeling[3] = { u8"🧎‍♂️", u8"🧎", u8"🧎‍♀️" };
    static const char* working [3] = { u8"🏌️‍♂️", u8"🏌️", u8"🏌️‍♀️" };

    int v = ownerPersonVariant(e.owner);
    if (e.state == S_MOVING || e.state == S_RETURNING || e.state == S_ENTERING)
        return walking[v];
    if (e.state == S_GATHERING && e.gatherType == 2)
        return kneeling[v];
    if (e.state == S_GATHERING || e.state == S_BUILDING || e.state == S_ATTACKING)
        return working[v];
    return standing[v];
}

static const char* emojiForEntityOnMap(const Entity& e) {
    if (e.type == E_PEASANT) return peasantEmojiForState(e);
    return getEntityEmoji(e.type);
}

// ============================================================
// TERRAIN VISUALS
// ============================================================
static bool shouldShowSeasonAt(int x, int y, float threshold) {
    int hash = ((x*7919 + y*6271) & 0xFFFF);
    return (float)hash / 65535.0f < threshold;
}

void getTerrainVisual(Terrain t, int x, int y, char& ch, int& cp) {
    Season season = getSeason();
    float sprog   = getSeasonProgress();
    float bright  = getBrightness();
    bool  night   = bright < 0.3f;
    Biome biome   = g.map[y][x].biome;

    switch (t) {
    case T_GRASS:        ch='.'; cp=CP_GRASS;       break;
    case T_TALL_GRASS:   ch='"'; cp=CP_TALL_GRASS;  break;
    case T_FLOWERS: {
        ch='*';
        static const int fcp[] = {CP_FLOWERS, CP_FLOWERS_BLUE, CP_FLOWERS_YELLOW, CP_FLOWERS_RED};
        cp = fcp[((unsigned)(x*7+y*13)^(unsigned)(x*3+y)) % 4];
        break;
    }
    case T_MEADOW:       ch=','; cp=CP_MEADOW;      break;
    case T_FOREST:       ch='T'; cp=CP_FOREST;      break;
    case T_PINE:         ch='Y'; cp=CP_PINE;        break;
    case T_PALM:         ch='y'; cp=CP_PALM;        break;
    case T_DEAD_TREE:    ch='t'; cp=CP_DEAD_TREE;   break;
    case T_MOUNTAIN:     ch='^'; cp=CP_MOUNTAIN;    break;
    case T_HILLS:        ch='n'; cp=CP_HILLS;       break;
    case T_STONE:        ch='o'; cp=CP_STONE;       break;
    case T_WATER:        ch='~'; cp=CP_WATER;       break;
    case T_SHALLOWS:     ch='~'; cp=CP_SHALLOWS;    break;
    case T_MARSH:        ch='='; cp=CP_MARSH;       break;
    case T_REEDS:        ch='|'; cp=CP_REEDS;       break;
    case T_GOLD:         ch='$'; cp=CP_GOLD;        break;
    case T_SAND:         ch='.'; cp=CP_SAND;        break;
    case T_DUNES:        ch='~'; cp=CP_DUNES;       break;
    case T_SNOW:         ch='.'; cp=CP_SNOW_GROUND; break;
    case T_ICE:          ch='='; cp=CP_ICE;         break;
    case T_DIRT:         ch='.'; cp=CP_DIRT;        break;
    case T_ROAD:         ch='#'; cp=CP_ROAD;        break;
    case T_MUD:          ch=','; cp=CP_DIRT;        break;
    case T_WHEAT:        ch='%'; cp=CP_WHEAT;       break;
    case T_BERRY:        ch=':'; cp=CP_BERRY;       break;
    case T_FISH:         ch=(g.tick%30<15)?'~':'"'; cp=CP_SHALLOWS; break;
    case T_RUINS:        ch='&'; cp=CP_RUINS;       break;
    case T_GRAVEL:       ch=':'; cp=CP_GRAVEL;      break;
    case T_LAVA: {
        int frame = (g.tick/4 + x*3 + y*5) % 6;
        ch = (frame < 2) ? '~' : (frame < 4) ? '=' : '*';
        cp = (frame == 1 || frame == 4) ? CP_LAVA_HOT : CP_LAVA;
        break;
    }
    case T_ASH:          ch='.'; cp=CP_ASH;         break;
    case T_CASTLE_WALL:  ch='#'; cp=CP_CASTLE_WALL; break;
    case T_CASTLE_FLOOR: ch='.'; cp=CP_CASTLE_FLOOR;break;
    case T_CASTLE_GATE:  ch='='; cp=CP_CASTLE_GATE; break;
    case T_BRIDGE:       ch='='; cp=CP_ROAD;        break;
    case T_MONOLITH:     ch='i'; cp=CP_STONE;       break;
    }

    // Water animation
    if (t == T_WATER) {
        int frame = (g.tick/6 + x + y) % 6;
        const char wc[] = {'~','~','-','~','~','-'};
        ch = wc[frame];
        cp = (frame==2||frame==5) ? CP_WATER_SHIMMER : CP_WATER;
    }
    if (t == T_SHALLOWS) { int f=(g.tick/8+x*3)%4; const char sc[]={'~','-','~','-'}; ch=sc[f]; }
    if (t == T_MARSH)    { int f=(g.tick/10+x)%3;  const char mc[]={'=','-','='};     ch=mc[f]; }
    if (t == T_REEDS)    { int f=(g.tick/12+x+y*3)%4; const char rc[]={'|','/','|','\\'}; ch=rc[f]; }
    if (t == T_GOLD) {
        int frame = (g.tick/4 + x*5 + y*3) % 8;
        cp = (frame < 2) ? CP_GOLD_SHIMMER : CP_GOLD;
    }

    if (biome != B_DESERT && biome != B_SNOW) {
        switch (season) {
        case SPRING:
            if (t==T_GRASS  && shouldShowSeasonAt(x,y,sprog*0.6f))  cp = CP_GRASS_LIGHT;
            if (t==T_FOREST && shouldShowSeasonAt(x,y,sprog*0.5f))  cp = CP_GRASS_LIGHT;
            if (sprog > 0.4f && t==T_GRASS && shouldShowSeasonAt(x,y,(sprog-0.4f)*1.5f))
                if ((x*13+y*7)%11==0) { ch='*'; cp=CP_SPRING_FLOWER; }
            break;
        case SUMMER:
            if (t==T_FOREST) cp = CP_FOREST_DARK;
            if (t==T_GRASS && shouldShowSeasonAt(x,y,0.3f)) cp = CP_GRASS_DRY;
            if (t==T_WHEAT) { ch='%'; cp=CP_WHEAT_GOLD; }
            break;
        case AUTUMN: {
            float p = sprog;
            if (t==T_FOREST) {
                // Per-tree hash gives orange/gold/red variety across the canopy.
                int tv = (x*3571 + y*2371) % 3;
                int earlyC = (tv==0) ? CP_AUT_TREE_EARLY : (tv==1) ? CP_AUT_TREE_GOLD : CP_AUT_TREE_RED;
                int midC   = (tv==0) ? CP_AUT_TREE_MID   : (tv==1) ? CP_AUT_TREE_EARLY : CP_AUT_TREE_GOLD;
                if (shouldShowSeasonAt(x,y,p*0.4f))                    cp = earlyC;
                if (p>0.3f && shouldShowSeasonAt(x,y,(p-0.3f)*1.4f))   cp = midC;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.5f)) { cp = CP_AUT_TREE_LATE; ch='t'; }
            }
            if (t==T_PINE && p>0.5f && shouldShowSeasonAt(x,y,(p-0.5f)*0.6f)) cp = CP_AUT_TREE_EARLY;
            if (t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW) {
                if (shouldShowSeasonAt(x,y,p*0.5f))                             cp = CP_AUT_GRASS;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.0f)) { cp=CP_AUT_GRASS_LATE; if(t==T_TALL_GRASS)ch=','; }
            }
            // Flowers gone by autumn — immediately fade to grass colour.
            if (t==T_FLOWERS) { ch='.'; cp=(p>0.5f) ? CP_AUT_GRASS_LATE : CP_AUT_GRASS; }
            if (t==T_WHEAT   && shouldShowSeasonAt(x,y,p))        { ch=','; cp=CP_DIRT; }
            // Late autumn: first frost dusts the ground with light snow patches.
            if (p > 0.65f) {
                float frost = (p - 0.65f) * 2.86f; // 0→1 over the last 35% of autumn
                if (t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW||t==T_FLOWERS||t==T_DIRT)
                    if (shouldShowSeasonAt(x+50,y+50, frost*0.45f)) { ch='.'; cp=CP_WIN_GROUND; }
            }
            break;
        }
        case WINTER: {
            float p = sprog;
            // Patchy snow: never a full blanket — always some bare ground visible.
            float snowAmt = std::min(0.80f, 0.30f + p * 0.55f);
            if (t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW||t==T_FLOWERS||t==T_DIRT||t==T_BERRY||t==T_GRAVEL)
                if (shouldShowSeasonAt(x,y,snowAmt)) { ch='.'; cp=CP_WIN_GROUND; }
            if (t==T_HILLS && shouldShowSeasonAt(x,y,snowAmt)) cp=CP_WIN_GROUND;
            if (t==T_WHEAT) { ch='.'; cp=CP_WIN_GROUND; }
            if (t==T_FOREST) {
                // ~50% of forest tiles get snow-laden boughs (white 'T'), rest are bare.
                if (hash01(x,y,83) < 0.50f) { ch='T'; cp=CP_SNOW_FALL; }
                else                         { ch='t'; cp=CP_WIN_TREE; }
            }
            if (t==T_PINE) cp = (hash01(x,y,89) < 0.60f) ? CP_SNOW_FALL : CP_WIN_PINE;
            // Tail-end visual thaw for snow-dusted non-snow terrain (hills etc).
            if (p > 0.85f) {
                float thaw = (p-0.85f)*6.67f;
                if ((cp==CP_WIN_GROUND) && t!=T_SNOW && shouldShowSeasonAt(x+100,y+100,thaw))
                    { ch='.'; cp=CP_GRASS; }
            }
            break;
        }}
    }

    // Tundra (B_SNOW) is its own seasonal cycle. Native T_SNOW tiles used to
    // stay snowy year-round, which read weird in summer. Now the tundra
    // partially thaws in spring/autumn and almost fully greens in summer.
    if (biome == B_SNOW && t == T_SNOW) {
        switch (season) {
        case SUMMER:
            // Mostly bare ground and patchy grass.
            if (shouldShowSeasonAt(x, y, 0.70f)) { ch='.'; cp=CP_GRASS_DRY; }
            else                                 { ch=','; cp=CP_GRASS_LIGHT; }
            break;
        case SPRING: {
            // Snow lingers near start, then patches of grass break through.
            float thaw = std::min(1.0f, sprog * 1.4f);
            if (shouldShowSeasonAt(x, y, thaw)) { ch='.'; cp=CP_GRASS_DRY; }
            break;
        }
        case AUTUMN: {
            // First frost — grass mostly, snow returns late.
            float freeze = std::min(1.0f, sprog * 1.2f);
            if (!shouldShowSeasonAt(x, y, freeze)) { ch=','; cp=CP_GRASS_DRY; }
            break;
        }
        case WINTER:
            break; // already full snow
        }
    }

    if (night) {
        if (cp==CP_GRASS||cp==CP_GRASS_LIGHT||cp==CP_GRASS_DRY||cp==CP_TALL_GRASS||cp==CP_MEADOW
            ||cp==CP_AUT_GRASS||cp==CP_AUT_GRASS_LATE
            ||cp==CP_SPRING_FLOWER
            ||cp==CP_FLOWERS||cp==CP_FLOWERS_BLUE||cp==CP_FLOWERS_YELLOW||cp==CP_FLOWERS_RED
            ||cp==CP_BERRY||cp==CP_MARSH||cp==CP_REEDS)
            cp = CP_NIGHT_GRASS;
        if (cp==CP_FOREST||cp==CP_FOREST_DARK||cp==CP_PINE||cp==CP_PALM||cp==CP_DEAD_TREE
            ||cp==CP_AUT_TREE_EARLY||cp==CP_AUT_TREE_MID||cp==CP_AUT_TREE_LATE
            ||cp==CP_AUT_TREE_GOLD||cp==CP_AUT_TREE_RED
            ||cp==CP_WIN_TREE||cp==CP_WIN_PINE)
            cp = CP_NIGHT_TREE;
        if (cp==CP_WATER||cp==CP_WATER_SHIMMER||cp==CP_SHALLOWS) cp = CP_NIGHT_WATER;
        if (cp==CP_SAND||cp==CP_DUNES||cp==CP_DIRT||cp==CP_ROAD||cp==CP_GRAVEL
            ||cp==CP_CASTLE_FLOOR||cp==CP_RUINS||cp==CP_WHEAT||cp==CP_WHEAT_GOLD
            ||cp==CP_HILLS||cp==CP_STONE)
            cp = CP_NIGHT_GROUND;
        if (cp==CP_GOLD||cp==CP_GOLD_SHIMMER) cp = CP_NIGHT_GOLD;
        // Snow tiles darken but stay distinctly lighter than bare ground at night.
        if (cp==CP_WIN_GROUND||cp==CP_SNOW_GROUND) cp = CP_NIGHT_SNOW;
    }
}

// ============================================================
// MAP RENDER
// ============================================================
void renderMap() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    int panelW = 24;
    int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
    int mapCols = maxX - panelW - 1;
    g.viewW = mapCols / tileW; g.viewH = maxY - 4;
    if (g.viewW < 30) g.viewW = maxX / tileW; if (g.viewH < 10) g.viewH = maxY - 2;
    g.viewW = std::max(1, std::min(g.viewW, MAP_W)); g.viewH = std::min(g.viewH, MAP_H);

    // Keyboard cursor: pan the view to keep it inside the margins.
    // Mouse cursor: never — the pointer pins a screen cell, and panning here
    // would change which map tile that cell shows, yanking the cursor tile
    // out from under a stationary pointer (and feeding back into another
    // pan). Mouse view movement is edge-scroll + minimap only.
    if (!g.cursorByMouse) {
        if (g.cursorX < g.viewX+3)            g.viewX = g.cursorX - 3;
        if (g.cursorX > g.viewX+g.viewW-4)    g.viewX = g.cursorX - g.viewW + 4;
        if (g.cursorY < g.viewY+2)            g.viewY = g.cursorY - 2;
        if (g.cursorY > g.viewY+g.viewH-3)    g.viewY = g.cursorY - g.viewH + 3;
    }
    g.viewX = std::max(0, std::min(g.viewX, MAP_W - g.viewW));
    g.viewY = std::max(0, std::min(g.viewY, MAP_H - g.viewH));

    bool night = isNight();

    // Selected ranged unit/tower: precompute range-ring centre + radius.
    int ringX = -1, ringY = -1, ringR = 0;
    Entity* selR = findEntity(g.selectedId);
    if (selR && selR->alive && selR->owner == 0) {
        int rng = STATS[selR->type].range;
        if (selR->type == E_ARCHER && (g.players[0].research & R_CROSSBOWS)) rng += 2;
        if (rng > 1) {
            auto& ss = STATS[selR->type];
            ringX = selR->x + ss.sizeW/2; ringY = selR->y + ss.sizeH/2; ringR = rng;
        }
    }

    // Precompute building footprint preview for M_BUILD_PLACE.
    // bldPrev[y][x] = 0 not in footprint, 1 valid, 2 blocked.
    static unsigned char bldPrev[MAP_H][MAP_W];
    memset(bldPrev, 0, sizeof(bldPrev));
    if (g.mode == M_BUILD_PLACE && g.buildPending != E_NONE) {
        Entity* sel = findEntity(g.selectedId);
        int ignoreId = (sel && sel->alive) ? sel->id : -1;
        bool ok = canPlace(g.buildPending, g.cursorX, g.cursorY, 0, ignoreId);
        auto& s = STATS[g.buildPending];
        // Castle previews its full 7x7 compound, not just the keep.
        int pw = s.sizeW, ph = s.sizeH;
        if (g.buildPending == E_CASTLE) { pw = 7; ph = 7; }
        for (int dy = 0; dy < ph; dy++) for (int dx = 0; dx < pw; dx++) {
            int nx = g.cursorX+dx, ny = g.cursorY+dy;
            if (inBounds(nx, ny)) bldPrev[ny][nx] = ok ? 1 : 2;
        }
    }

    // Precompute wall drag preview line (Bresenham)
    static bool wallPrev[MAP_H][MAP_W];
    memset(wallPrev, 0, sizeof(wallPrev));
    if (g.mode == M_WALL_DRAG && g.dragging) {
        int x0=g.wallDragX, y0=g.wallDragY, x1=g.cursorX, y1=g.cursorY;
        int dx=std::abs(x1-x0), sx=x0<x1?1:-1;
        int dy=-std::abs(y1-y0), sy2=y0<y1?1:-1;
        int err=dx+dy;
        while (true) {
            if (inBounds(x0,y0)) wallPrev[y0][x0] = true;
            if (x0==x1 && y0==y1) break;
            int e2=2*err;
            if (e2>=dy){err+=dy; x0+=sx;}
            if (e2<=dx){err+=dx; y0+=sy2;}
        }
    }

    for (int sy = 0; sy < g.viewH; sy++) { int my = g.viewY + sy;
        for (int sx = 0; sx < g.viewW; sx++) { int mx = g.viewX + sx;
            int scY = sy+2, scX = sx * tileW;
            auto clearTile = [&](int y, int x) {
                if (displayMode == DM_ASCII) mvaddch(y, x, ' ');
                else                         mvaddstr(y, x, "  ");
            };
            if (!inBounds(mx, my)) { clearTile(scY, scX); continue; }
            Tile& tile = g.map[my][mx];
            bool vis = tile.visible[0], expl = tile.explored[0];
            bool isCur = (mx == g.cursorX && my == g.cursorY);

            if (!expl) {
                if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); clearTile(scY, scX); attroff(COLOR_PAIR(CP_CURSOR)); }
                else { clearTile(scY, scX); }
                continue;
            }

            char ch; int cp;
            getTerrainVisual(tile.terrain, mx, my, ch, cp);
            // Cliff rim: a highland tile bordering lower ground renders as an
            // escarpment so plateau edges read as hard walls. Ramps (T_HILLS)
            // keep their own look — they're the way up.
            if (tile.elev > 0 && tile.terrain != T_HILLS) {
                static const int d4r[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
                for (auto& d : d4r) {
                    int nx = mx+d[0], ny = my+d[1];
                    if (inBounds(nx,ny) && g.map[ny][nx].elev < tile.elev) {
                        ch = '#'; cp = CP_CLIFF; break;
                    }
                }
            }
            // Scattered loot glints on the ground until someone hauls it off.
            if      (tile.lootGold > 0) { ch = '$'; cp = CP_GOLD_SHIMMER; }
            else if (tile.lootWood > 0) { ch = '='; cp = CP_DEAD_TREE; }
            else if (tile.lootFood > 0) { ch = '%'; cp = CP_WHEAT; }
            int terrainCp = (displayMode == DM_EMOJI) ? emojiTerrainColorPair(tile, mx, my, night) : cp;
            if (displayMode == DM_EMOJI) cp = terrainCp;

            if (!vis) {
                if (isCur) {
                    attron(COLOR_PAIR(CP_CURSOR));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else { mvaddstr(scY, scX, "  "); mvprintw(scY, scX, "%s", terrainSymbolVariant(tile.terrain, ch, mx, my)); }
                    attroff(COLOR_PAIR(CP_CURSOR));
                } else {
                    attron(COLOR_PAIR(CP_FOG_EXPLORED));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else { mvaddstr(scY, scX, "  "); mvprintw(scY, scX, "%s", terrainSymbolVariant(tile.terrain, ch, mx, my)); }
                    attroff(COLOR_PAIR(CP_FOG_EXPLORED));
                }
                continue;
            }

            // Wall drag preview overrides terrain.
            // Emoji mode shows ■ (solid block) matching the completed wall glyph.
            if (wallPrev[my][mx]) { ch = '#'; cp = (displayMode == DM_EMOJI) ? ownerColorPair(0, night) : CP_PLAYER; }

            // Use a chtype-wide draw glyph so completed walls can use the ACS solid block.
            chtype drawCh = (chtype)ch;
            if (wallPrev[my][mx]) drawCh = ACS_CKBOARD;

            Entity* ent = entityAt(mx, my);
            // Cloaking: enemy units fade at night/storm unless a friendly eye is close.
            // Wheat fields also conceal enemies — units in crops need close detection.
            bool inCrop = ent && !isBuilding(ent->type) && tile.terrain == T_WHEAT;
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && (isConcealing() || inCrop) && !isDetectedBy(mx, my, 0)) ent = nullptr;

            // Body tile: catapult/ram/deployed-trebuchet extend one cell right.
            if (displayMode == DM_ASCII && !ent && inBounds(mx-1, my)) {
                Entity* leftEnt = entityAt(mx-1, my);
                bool isTwoTile = leftEnt && leftEnt->alive && !leftEnt->underConstruction &&
                    (leftEnt->type == E_CATAPULT || leftEnt->type == E_RAM ||
                     leftEnt->type == E_TREBUCHET);   // treb is two tiles packed OR deployed
                if (isTwoTile) {
                    bool inCropLeft = !isBuilding(leftEnt->type) && g.map[my][mx-1].terrain == T_WHEAT;
                    bool leftCloaked = leftEnt->owner != 0 && leftEnt->owner < MAX_PLAYERS
                                    && (isConcealing() || inCropLeft) && !isDetectedBy(mx-1, my, 0);
                    if (!leftCloaked) {
                        bool bodyIsSel = (leftEnt->id == g.selectedId);
                        if (!bodyIsSel) for (int sid : g.selectedIds) if (sid == leftEnt->id) { bodyIsSel = true; break; }
                        int bcp = ownerColorPair(leftEnt->owner, night);
                        char sc;
                        if      (leftEnt->type == E_CATAPULT)  sc = 'c';
                        else if (leftEnt->type == E_RAM)       sc = 'r';
                        else if (leftEnt->packed == 0 && leftEnt->packTicks == 0)
                                                                sc = 'Q'; // counterweight base
                        else                                    sc = 'o'; // wagon wheels
                        int sattr = COLOR_PAIR(bcp) | A_BOLD;
                        if (bodyIsSel) sattr |= A_REVERSE;
                        if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, sc); attroff(COLOR_PAIR(CP_CURSOR)); }
                        else        { attron(sattr); mvaddch(scY, scX, sc); attroff(sattr); }
                        continue;
                    }
                }
            }

            // Render priority + stack count. When multiple units share a tile,
            // prefer the highest-value military so e.g. knights show through a
            // pile of peasants. Also count any same-owner combat units on the
            // tile so we can show an uppercase glyph for stacks of 2+.
            int stackedMil = 0;
            if (ent && ent->alive && !isBuilding(ent->type)) {
                auto isMil = [](EntityType t) {
                    return t==E_MILITIA||t==E_ARCHER||t==E_KNIGHT||t==E_CATAPULT
                        || t==E_WARSHIP||t==E_SPEARMAN||t==E_CROSSBOWMAN||t==E_HUSSAR;
                };
                int prio = isMil(ent->type) ? 1 : 0;
                for (auto& other : g.entities) {
                    if (!other.alive || other.state == S_GARRISONED) continue;
                    if (other.x != mx || other.y != my) continue;
                    if (other.owner != ent->owner) continue;
                    if (!isMil(other.type)) continue;
                    stackedMil++;
                    if (prio == 0) { ent = &other; prio = 1; }
                }
            }
            // emojiStr: the UTF-8 string to display in emoji mode.
            // Initialised to terrain glyph; overridden when an entity is present.
            const char* emojiStr = nullptr;

            if (ent && ent->alive) {
                ch = STATS[ent->type].glyph;
                // ASCII mode: uppercase glyph signals a stack of 2+ military.
                // Emoji mode: no uppercase equivalent — stack not indicated.
                if (displayMode == DM_ASCII && stackedMil >= 2 && ch >= 'a' && ch <= 'z')
                    ch = ch - 'a' + 'A';
                drawCh = (chtype)ch;

                // Default emoji is the entity's body symbol. Peasants get
                // state/owner-specific standing/walking/kneeling/working glyphs.
                emojiStr = emojiForEntityOnMap(*ent);

                // Colour pair: player-owned units/buildings use owner colour
                // backgrounds. Gaia animals keep the terrain/biome background.
                // ASCII mode preserves the older animal/ship colour treatment.
                if (ent->owner == OWNER_NATURE) {
                    if (displayMode == DM_EMOJI) {
                        cp = terrainCp;
                    } else {
                        if      (ent->type == E_WOLF)  cp = CP_WOLF;
                        else if (ent->type == E_SHEEP) cp = CP_SHEEP;
                        else if (ent->type == E_BOAR)  cp = CP_BOAR;
                        else                           cp = CP_DEER;
                    }
                } else {
                    cp = ownerColorPair(ent->owner, night);
                }
                // All boats get a wood-brown deck regardless of display mode.
                // Glyph colour is per-player so each side's fleet is identifiable.
                if (isNaval(ent->type) && ent->owner < MAX_PLAYERS) {
                    static const int shipCp[] = { CP_SHIP_P0, CP_SHIP_P1, CP_SHIP_P2, CP_SHIP_P3 };
                    cp = shipCp[ent->owner];
                }
                // Farms are always wheat-gold — ownership doesn't change their colour.
                if (ent->type == E_FARM && !ent->underConstruction)
                    cp = (getSeason() == SUMMER) ? CP_WHEAT_GOLD : CP_WHEAT;

                // State-specific glyph overrides (gate, construction, siege engines, alert).
                if (ent->type == E_GATE && !ent->underConstruction) {
                    ch = ent->gateOpen ? '-' : '|';
                    drawCh = (chtype)ch;
                    emojiStr = u8"🚪";
                }
                if (ent->underConstruction && g.tick%10 < 5) {
                    ch = '#'; drawCh = (chtype)ch;
                    emojiStr = u8"🚧";  // pulsing during construction
                }
                // Dwarf-Fortress-style solid wall block when complete.
                // Emoji mode uses ■ (same visual intent, but valid UTF-8).
                if (ent->type == E_WALL && !ent->underConstruction) {
                    drawCh = ACS_CKBOARD;
                    emojiStr = u8"🧱";
                }
                // Keep: 3×3 per-cell pattern in ASCII mode — solid corners,
                // edged walls, the lord's hall at the centre.
                if (ent->type == E_CASTLE && !ent->underConstruction && displayMode == DM_ASCII) {
                    int dx = mx - ent->x, dy = my - ent->y;
                    bool corner = (dx == 0 || dx == 2) && (dy == 0 || dy == 2);
                    if (corner)                  drawCh = ACS_CKBOARD;
                    else if (dy == 0 || dy == 2) { ch = '='; drawCh = (chtype)ch; }
                    else if (dx == 0 || dx == 2) { ch = '|'; drawCh = (chtype)ch; }
                    else                         { ch = 'W'; drawCh = (chtype)ch; }
                }
                // Siege engine arm animations stay ASCII-only. Emoji mode uses
                // one proper unit emoji in the entity's single 2-column tile.
                if (displayMode == DM_ASCII && ent->type == E_CATAPULT) {
                    // Arm shows as raised only for the first 3 ticks after firing —
                    // a brief thump, then rests horizontal until the next shot.
                    bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_CATAPULT].atkSpeed - 3;
                    ch = firing ? '/' : '-'; drawCh = (chtype)ch;
                }
                if (displayMode == DM_ASCII && ent->type == E_RAM) {
                    bool ramming = ent->state==S_ATTACKING && ent->atkCd > STATS[E_RAM].atkSpeed*2/3;
                    ch = ramming ? '=' : '-'; drawCh = (chtype)ch;
                }
                if (displayMode == DM_ASCII && ent->type == E_TREBUCHET) {
                    if (ent->packTicks > 0)      { ch = ent->packed ? 'q' : 'Q'; }  // transition
                    else if (ent->packed == 1)   { ch = 'q'; }                       // packed wagon
                    else {
                        // Deployed: arm rests as 'L', flips to '/' for 5 ticks after a shot.
                        bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_TREBUCHET].atkSpeed - 5;
                        ch = firing ? '/' : 'L';
                    }
                    drawCh = (chtype)ch;
                }
                // Peasant work/idle cycle (ASCII only — emoji peasants have their
                // own state-aware glyph). Staggered per-id so a busy village
                // doesn't strobe in sync.
                if (displayMode == DM_ASCII && ent->type == E_PEASANT) {
                    int cyc = (g.tick + ent->id*5) % 30;
                    if      (ent->state == S_GATHERING && cyc < 3) { ch = '*'; drawCh = (chtype)ch; }
                    else if (ent->state == S_BUILDING  && cyc < 3) { ch = '+'; drawCh = (chtype)ch; }
                    else if (ent->state == S_RETURNING && cyc < 2) { ch = ','; drawCh = (chtype)ch; }
                    else if (ent->state == S_IDLE) {
                        // Slow daydream pulse: '?' shown ~1 s every ~20 s, staggered.
                        int slow = (g.tick + ent->id*47) % 250;
                        if (slow < 12) { ch = '?'; drawCh = (chtype)ch; }
                    }
                }
                // Recently in combat: gentle '!' pulse — ~1.5 Hz, not strobing.
                if (ent->alertTicks > 0 && (g.tick % 8) < 4) {
                    ch = '!'; drawCh = (chtype)ch;
                    emojiStr = "!";
                }
                // Broken men flee under a blinking '?'; captives are marked '"'.
                if (ent->state == S_ROUTING) {
                    if ((g.tick % 6) < 3) { ch = '?'; drawCh = (chtype)ch; }
                    emojiStr = u8"💨";
                } else if (ent->prisoner) {
                    ch = '"'; drawCh = (chtype)ch;
                    emojiStr = u8"⛓";
                }
            }
            // Projectile overwrites terrain/entity glyph; keep ASCII char for colour lookup.
            for (auto& p : g.projectiles) {
                if (!p.alive) continue;
                if ((int)roundf(p.x)==mx && (int)roundf(p.y)==my) {
                    ch = p.glyph; cp = (displayMode == DM_EMOJI) ? terrainCp : p.color; drawCh = (chtype)ch;
                    // Projectiles sit on the underlying biome background.
                    emojiStr = (p.color == CP_PROJ_BOULDER) ? u8"🪨" : u8"•";
                }
            }

            // When no entity is present, terrain drives the emoji/symbol string.
            if (!emojiStr) emojiStr = (displayMode == DM_EMOJI) ? terrainSymbolVariant(tile.terrain, ch, mx, my) : getCharEmoji(ch);

            bool isSel = false;

            // Single selection highlight
            Entity* sel = findEntity(g.selectedId);
            if (sel && !isCur) {
                auto& ss = STATS[sel->type];
                if (ss.isBuilding) {
                    if (mx>=sel->x && mx<sel->x+ss.sizeW && my>=sel->y && my<sel->y+ss.sizeH) isSel = true;
                } else if (mx==sel->x && my==sel->y) isSel = true;
            }
            // Group selection highlight
            if (!isSel && !g.selectedIds.empty()) {
                for (int sid : g.selectedIds) {
                    Entity* se = findEntity(sid);
                    if (se && mx==se->x && my==se->y) { isSel = true; break; }
                }
            }

            bool onRangeRing = (ringR > 0)
                && std::max(std::abs(mx - ringX), std::abs(my - ringY)) == ringR;

            // Unified draw: ASCII uses mvaddch/chtype; emoji uses mvprintw with UTF-8.
            // All subsequent positions use absolute mv* coords so ncurses' internal
            // cursor model (which counts bytes, not columns) doesn't accumulate.
            auto drawAt = [&](int y, int x, chtype dch, const char* estr) {
                if (displayMode == DM_ASCII) {
                    mvaddch(y, x, dch);
                } else {
                    // Full emoji cells are two terminal columns wide. Paint both
                    // cells first so the background colour fills the whole tile,
                    // then write the emoji over that coloured tile.
                    mvaddstr(y, x, "  ");
                    mvprintw(y, x, "%s", estr);
                }
            };

            if (bldPrev[my][mx]) {
                // Footprint overlay wins over cursor so the build outline reads cleanly.
                int cpFP = (bldPrev[my][mx] == 1) ? CP_BUILD_OK : CP_BUILD_BAD;
                attron(COLOR_PAIR(cpFP)|A_BOLD);
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(cpFP)|A_BOLD);
            } else if (isCur) {
                attron(COLOR_PAIR(CP_CURSOR));
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(CP_CURSOR));
            } else if (onRangeRing && !ent) {
                // Subtle range-ring marker on empty tiles only.
                attron(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
                drawAt(scY, scX, '.', u8"·");
                attroff(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
            } else {
                int attr = COLOR_PAIR(cp);
                if (ent && ent->alive) attr |= A_BOLD;
                // Selection highlight: A_REVERSE swaps owner bg ↔ fg so the
                // player/enemy colour becomes the cell foreground — distinct
                // from the ownership background on surrounding tiles.
                if (isSel) attr |= A_REVERSE;
                attron(attr);
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(attr);
            }
        }
    }

    // The fallen linger on the field a while — a dim '%' on the death tile,
    // fading after ~200 ticks (g.corpses; render-only, never sim state).
    attron(COLOR_PAIR(CP_CORPSE)|A_DIM);
    for (auto& c : g.corpses) {
        if (g.tick - c.tick > 200) continue;
        if (!inBounds(c.x, c.y) || !g.map[c.y][c.x].visible[0]) continue;
        int sx = c.x - g.viewX, sy = c.y - g.viewY;
        if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) continue;
        if (entityAt(c.x, c.y)) continue;   // don't paint under the living
        if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, '%');
        else                         mvprintw(sy+2, sx * tileW, u8"🩸");
    }
    attroff(COLOR_PAIR(CP_CORPSE)|A_DIM);

    // Hearth smoke rises from finished houses — a two-cell plume that drifts
    // and curls each second (render-only). Strongest at dawn/dusk when the
    // fires are stoked; a faint wisp persists through the night.
    if (isDawn() || isDusk() || isNight()) {
        bool stoked = isDawn() || isDusk();
        attron(COLOR_PAIR(CP_UI_DIM));
        for (auto& e : g.entities) {
            if (!e.alive || e.underConstruction || e.owner >= MAX_PLAYERS) continue;
            if (e.type != E_HOUSE && e.type != E_TAVERN && e.type != E_MANOR) continue;
            // Lower puff sits just above the roof; a higher wisp trails it when stoked.
            int phase = (g.tick / 8 + e.id);
            for (int h = 0; h < (stoked ? 2 : 1); h++) {
                int smx = e.x + ((phase + h) % 3 == 0 ? (((phase>>1)&1) ? 1 : -1) : 0);
                int smy = e.y - 1 - h;
                if (!inBounds(smx, smy) || !g.map[smy][smx].visible[0]) continue;
                int sx = smx - g.viewX, sy = smy - g.viewY;
                if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) continue;
                if (entityAt(smx, smy)) continue;
                char puff = (h == 0)
                    ? ((phase % 2) ? '~' : '\'')
                    : ((phase % 2) ? '.'  : '`');
                if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, puff);
                else                         mvprintw(sy+2, sx * tileW, h ? u8"·" : u8"〜");
            }
        }
        attroff(COLOR_PAIR(CP_UI_DIM));
    }

    // Night torches: defensive works and the town hall keep a flame burning
    // after dark, flickering on a per-building stagger (render-only).
    if (isNight()) {
        for (auto& e : g.entities) {
            if (!e.alive || e.underConstruction || e.owner >= MAX_PLAYERS) continue;
            if (e.type != E_TOWER && e.type != E_GATE && e.type != E_CASTLE
                && e.type != E_TOWNHALL && e.type != E_WALL) continue;
            // A wall lights a torch only now and then, so ramparts twinkle
            // rather than blaze; keeps/towers/gates always carry one.
            if (e.type == E_WALL && ((g.tick/15 + e.id) % 4) != 0) continue;
            int tx = e.x, ty = e.y - 1;
            if (!inBounds(tx, ty) || !g.map[ty][tx].visible[0]) continue;
            int sx = tx - g.viewX, sy = ty - g.viewY;
            if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) continue;
            if (entityAt(tx, ty)) continue;
            // Flicker: bold flame most ticks, a dimmer ember on the off-beat.
            bool bright = ((g.tick/4 + e.id*3) % 3) != 0;
            attron(COLOR_PAIR(CP_TORCH) | (bright ? A_BOLD : A_DIM));
            char fl = bright ? 'i' : '.';
            if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, fl);
            else                         mvprintw(sy+2, sx * tileW, bright ? u8"🔥" : u8"·");
            attroff(COLOR_PAIR(CP_TORCH) | (bright ? A_BOLD : A_DIM));
        }
    }

    // Snowflakes: drawn every tick; hash seed changes every 12 ticks (~1 second)
    // so each flake stays visible for roughly a second before positions reshuffle.
    if (g.weather == W_SNOW) {
        int frame = g.tick / 12;
        for (int sy = 0; sy < g.viewH; sy++) for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[0]) continue;
            if (entityAt(mx, my)) continue;
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(frame*83492791u));
            if ((int)(h % 100) >= 1) continue;
            attron(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
            if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, '*');
            else                         mvprintw(sy+2, sx * tileW, u8"✦");
            attroff(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
        }
    }

    // Rain / storm: fast flicker kept on a short interval for patter effect.
    if ((g.weather == W_RAIN || g.weather == W_STORM) && (g.tick % 4) == 0) {
        int density = (g.weather == W_STORM) ? 2 : 1;
        for (int sy = 0; sy < g.viewH; sy++) for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[0]) continue;
            if (entityAt(mx, my)) continue;
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(g.tick*83492791u));
            if ((int)(h % 100) >= density) continue;
            attron(COLOR_PAIR(CP_RAIN)|A_BOLD);
            if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, '.');
            else                         mvprintw(sy+2, sx * tileW, u8"·");
            attroff(COLOR_PAIR(CP_RAIN)|A_BOLD);
        }
    }

    // Drag-selection box: screen-space overlay drawn on top of everything,
    // like AoE/StarCraft — visible over fog, units, weather, the lot.
    // (It used to be a per-tile branch, which the fog early-out skipped,
    // so boxes dragged across unexplored ground were invisible.)
    if (g.dragging && g.mode != M_WALL_DRAG) {
        int bx0 = std::min(g.dragStartX, g.cursorX), bx1 = std::max(g.dragStartX, g.cursorX);
        int by0 = std::min(g.dragStartY, g.cursorY), by1 = std::max(g.dragStartY, g.cursorY);
        attron(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
        auto borderCell = [&](int mx, int my) {
            int sx = mx - g.viewX, sy = my - g.viewY;
            if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) return;
            if (displayMode == DM_ASCII) mvaddch(sy+2, sx * tileW, ' ');
            else                         mvaddstr(sy+2, sx * tileW, "  ");
        };
        for (int mx = bx0; mx <= bx1; mx++) { borderCell(mx, by0); borderCell(mx, by1); }
        for (int my = by0; my <= by1; my++) { borderCell(bx0, my); borderCell(bx1, my); }
        attroff(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
    }
}

void render() { erase(); renderMap(); renderUI(); refresh(); }
