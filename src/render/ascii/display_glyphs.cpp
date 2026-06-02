#include "realm.h"
#include "input_keys.h"

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

// ============================================================
// COLOR INIT
// ============================================================
void initColors() {
    start_color();
    use_default_colors();
    int bg = -1;

    init_pair(CP_GRASS,         C::GREEN,        bg);
    init_pair(CP_GRASS_LIGHT,   C::BRIGHT_GREEN, bg);
    init_pair(CP_GRASS_DRY,     C::YELLOW_GREEN, bg);
    init_pair(CP_TALL_GRASS,    C::MED_GREEN,    bg);
    init_pair(CP_FLOWERS,        C::LAVENDER,     bg);
    init_pair(CP_FLOWERS_BLUE,   C::MED_BLUE,     bg);
    init_pair(CP_FLOWERS_YELLOW, C::BRIGHT_GOLD,  bg);
    init_pair(CP_FLOWERS_RED,    C::BERRY_RED,    bg);
    init_pair(CP_MEADOW,        C::PALE_GREEN,   bg);

    init_pair(CP_FOREST,        C::MED_GREEN,    bg);
    init_pair(CP_FOREST_DARK,   C::DARK_GREEN,   bg);
    init_pair(CP_PINE,          C::PINE_GREEN,   bg);
    init_pair(CP_PALM,          C::GREEN,        bg);
    init_pair(CP_DEAD_TREE,     C::GRAY,         bg);

    init_pair(CP_MOUNTAIN,      C::LIGHT_GRAY,   bg);
    init_pair(CP_HILLS,         C::OLIVE,        bg);
    init_pair(CP_STONE,         C::MED_GRAY,     bg);

    init_pair(CP_WATER,         C::ICE_BLUE,     C::DEEP_BLUE);
    init_pair(CP_WATER_SHIMMER, C::SNOW_WHITE,   C::DEEP_BLUE);
    init_pair(CP_SHALLOWS,      C::SNOW_WHITE,   C::TEAL);
    init_pair(CP_MARSH,         C::SWAMP_GREEN,  bg);
    init_pair(CP_REEDS,         C::DARK_GOLD,    bg);

    init_pair(CP_GOLD,          C::BRIGHT_GOLD,  bg);
    init_pair(CP_GOLD_SHIMMER,  C::GOLD,         bg);

    init_pair(CP_SAND,          C::TAN,          bg);
    init_pair(CP_DUNES,         C::LIGHT_TAN,    bg);
    init_pair(CP_SNOW_GROUND,   C::SNOW_WHITE,   bg);
    init_pair(CP_ICE,           C::MED_BLUE,     C::ICE_BLUE);

    init_pair(CP_DIRT,          C::BROWN,        bg);
    init_pair(CP_ROAD,          C::LIGHT_GRAY,   bg);

    init_pair(CP_WHEAT,         C::WHEAT_GOLD,   bg);
    init_pair(CP_WHEAT_GOLD,    C::BRIGHT_GOLD,  bg);
    init_pair(CP_BERRY,         C::BERRY_RED,    C::DARK_GREEN);

    init_pair(CP_RUINS,         C::GRAY,         bg);
    init_pair(CP_GRAVEL,        C::MED_GRAY,     bg);

    init_pair(CP_CASTLE_WALL,   C::BRIGHT_GRAY,  bg);
    init_pair(CP_CASTLE_FLOOR,  C::DARK_GOLD,    bg);
    init_pair(CP_CASTLE_GATE,   C::AMBER,        bg);

    init_pair(CP_AUT_TREE_EARLY, C::YELLOW_GREEN, bg);
    init_pair(CP_AUT_TREE_MID,   C::ORANGE,       bg);
    init_pair(CP_AUT_TREE_LATE,  C::BROWN,        bg);
    init_pair(CP_AUT_TREE_GOLD,  C::BRIGHT_GOLD,  bg);
    init_pair(CP_AUT_TREE_RED,   C::RED,          bg);
    init_pair(CP_AUT_GRASS,      C::OLIVE,        bg);
    init_pair(CP_AUT_GRASS_LATE, C::BROWN,        bg);

    init_pair(CP_WIN_GROUND,     C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_TREE,       C::MED_GRAY,     bg);
    init_pair(CP_WIN_PINE,       C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_ICE,        C::ICE_BLUE,     C::NAVY);

    init_pair(CP_NIGHT_GRASS,    C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_TREE,     C::DARK_GRAY,    bg);
    init_pair(CP_NIGHT_WATER,    C::NAVY,         C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   C::DARKER_GRAY,  bg);
    init_pair(CP_NIGHT_GOLD,     C::DARK_GOLD,    bg);
    init_pair(CP_NIGHT_SNOW,     C::MED_GRAY,     bg);

    init_pair(CP_DAWN_SKY,       C::ORANGE,       bg);
    init_pair(CP_DUSK_SKY,       C::DUSK_PURPLE,  bg);

    init_pair(CP_PLAYER,         C::PLAYER_CYAN,  bg);
    init_pair(CP_PLAYER_NIGHT,   C::PLAYER_DIM,   bg);
    init_pair(CP_ENEMY,          C::ENEMY_RED,    bg);
    init_pair(CP_ENEMY_NIGHT,    C::ENEMY_DIM,    bg);
    // Ship deck: glyph sits on a wood-brown background tile so boats read as
    // solid hulls instead of single floating characters on open water.
    init_pair(CP_SHIP_PLAYER,    C::PLAYER_CYAN,  C::BROWN);
    init_pair(CP_SHIP_ENEMY,     C::ENEMY_RED,    C::BROWN);
    init_pair(CP_SHIP_P0,        C::PLAYER_CYAN,  C::BROWN);
    init_pair(CP_SHIP_P1,        C::ENEMY_RED,    C::BROWN);
    init_pair(CP_SHIP_P2,        C::BRIGHT_GOLD,  C::BROWN);
    init_pair(CP_SHIP_P3,        C::LAVENDER,     C::BROWN);

    init_pair(CP_PROJ_ARROW,     C::BRIGHT_GOLD,  bg);
    init_pair(CP_PROJ_BOULDER,   C::BRIGHT_GRAY,  bg);
    init_pair(CP_PROJ_TOWER,     C::BRIGHT_RED,   bg);
    // Rain: a transparent blue dot — foreground colour only, no background fill.
    init_pair(CP_RAIN,           C::ICE_BLUE,     bg);
    // Falling snow: white glyph on transparent bg so flakes take the terrain's background.
    init_pair(CP_SNOW_FALL,      C::SNOW_WHITE,   bg);

    init_pair(CP_UI_BAR,         C::UI_TEXT,      C::UI_BG);
    init_pair(CP_UI_TEXT,        C::UI_TEXT,      bg);
    init_pair(CP_UI_HIGH,        C::UI_HIGHLIGHT, bg);
    init_pair(CP_UI_DIM,         C::UI_DIM,       bg);
    init_pair(CP_UI_ACCENT,      C::UI_ACCENT,    bg);
    init_pair(CP_FOG,            C::DARKER_GRAY,  bg);
    init_pair(CP_FOG_EXPLORED,   C::DARK_GRAY,    bg);
    // Cursor: black-on-gold pops on snow, grass, water, and dark biomes alike.
    init_pair(CP_CURSOR,         C::NEAR_BLACK,   C::BRIGHT_GOLD);
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
    init_pair(CP_SPRING_FLOWER,  C::LAVENDER,     bg);

    init_pair(CP_LAVA,           C::ORANGE,       C::RED);
    init_pair(CP_LAVA_HOT,       C::BRIGHT_GOLD,  C::RED);
    init_pair(CP_ASH,            C::DARK_GRAY,    bg);
    init_pair(CP_DEER,           C::TAN,          bg);
    init_pair(CP_WOLF,           C::LIGHT_GRAY,   bg);
    init_pair(CP_SHEEP,          C::SNOW_WHITE,   bg);
    init_pair(CP_BOAR,           C::BROWN,        bg);
    init_pair(CP_MM_ANIMAL,      C::TAN,          C::NEAR_BLACK);

    // Ownership background colour pairs.
    // Land units and buildings display the owner's colour as the BACKGROUND
    // so ownership is visible regardless of what glyph mode (ASCII/emoji)
    // is active.  Ships keep CP_SHIP_* (wood deck bg) for their hull look.
    //
    // Player 0 (human)  — cyan background
    init_pair(CP_OWN_P0,       C::SNOW_WHITE,   C::PLAYER_CYAN);
    init_pair(CP_OWN_P0_NIGHT, C::LIGHT_GRAY,   C::PLAYER_DIM);
    // Player 1 (AI 1)   — red background
    init_pair(CP_OWN_P1,       C::SNOW_WHITE,   C::ENEMY_RED);
    init_pair(CP_OWN_P1_NIGHT, C::LIGHT_GRAY,   C::ENEMY_DIM);
    // Player 2 (AI 2)   — orange background (distinct from red)
    init_pair(CP_OWN_P2,       C::NEAR_BLACK,   C::ORANGE);
    init_pair(CP_OWN_P2_NIGHT, C::NEAR_BLACK,   C::AMBER);
    // Player 3 (AI 3)   — purple background
    init_pair(CP_OWN_P3,       C::SNOW_WHITE,   C::DUSK_PURPLE);
    init_pair(CP_OWN_P3_NIGHT, C::LIGHT_GRAY,   C::GRAY);
}

// ============================================================
// OWNERSHIP COLOUR HELPER
// Returns the colour pair that should be applied to a land unit
// or building based on its owner.  Ships are excluded (callers
// handle CP_SHIP_* separately).  Animals/Gaia use their own
// type-specific pairs and are never passed here.
// ============================================================
int ownerColorPair(int owner, bool night) {
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
    case TERRAIN_COUNT:  ch='?'; cp=CP_GRASS;       break;
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
            if (t==T_FOREST) { ch='t'; cp=CP_WIN_TREE; }
            if (t==T_PINE)   cp=CP_WIN_PINE;
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
