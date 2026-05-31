#include "realm.h"

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
    init_pair(CP_SNOW_GROUND,   C::LIGHT_GRAY,   C::SNOW_WHITE);
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

    init_pair(CP_WIN_GROUND,     C::LIGHT_GRAY,   C::SNOW_WHITE);
    init_pair(CP_WIN_TREE,       C::DARK_GREEN,   C::SNOW_WHITE);
    init_pair(CP_WIN_PINE,       C::PINE_GREEN,   C::SNOW_WHITE);
    init_pair(CP_WIN_ICE,        C::ICE_BLUE,     C::NAVY);

    init_pair(CP_NIGHT_GRASS,    C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_TREE,     C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_WATER,    C::NAVY,         C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   C::DARKER_GRAY,  bg);
    init_pair(CP_NIGHT_GOLD,     C::DARK_GOLD,    bg);
    init_pair(CP_NIGHT_SNOW,     C::LIGHT_GRAY,   C::DARKER_GRAY);

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
            if (t==T_FOREST && shouldShowSeasonAt(x,y,0.35f+p*0.55f)) { ch='t'; cp=CP_WIN_TREE; }
            if (t==T_PINE   && shouldShowSeasonAt(x,y,0.25f+p*0.4f))          cp=CP_WIN_PINE;
            // Rivers and marshes freeze over
            float freezeAmt = std::min(1.0f, 0.25f + p * 1.1f);
            if (t==T_WATER  && shouldShowSeasonAt(x,y,freezeAmt)) { ch='='; cp=CP_WIN_ICE; }
            if (t==T_SHALLOWS && shouldShowSeasonAt(x,y,freezeAmt)) { ch='='; cp=CP_WIN_ICE; }
            if (t==T_MARSH  && shouldShowSeasonAt(x,y,freezeAmt)) { ch='='; cp=CP_WIN_ICE; }
            if (t==T_REEDS  && shouldShowSeasonAt(x,y,freezeAmt)) { ch='='; cp=CP_WIN_ICE; }
            // Thaw at the tail end of winter
            if (p > 0.85f) {
                float thaw = (p-0.85f)*6.67f;
                if ((cp==CP_WIN_GROUND) && t!=T_SNOW && shouldShowSeasonAt(x+100,y+100,thaw))
                    { ch='.'; cp=CP_GRASS; }
                if (cp==CP_WIN_ICE && (t==T_WATER||t==T_SHALLOWS) && shouldShowSeasonAt(x+200,y+200,thaw*0.7f))
                    { ch='~'; cp=CP_WATER; }
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
    int panelW = 24; g.viewW = maxX - panelW - 1; g.viewH = maxY - 4;
    if (g.viewW < 30) g.viewW = maxX;
    if (g.viewH < 10) g.viewH = maxY - 2;
    g.viewW = std::min(g.viewW, MAP_W); g.viewH = std::min(g.viewH, MAP_H);

    if (g.cursorX < g.viewX+3)            g.viewX = g.cursorX - 3;
    if (g.cursorX > g.viewX+g.viewW-4)    g.viewX = g.cursorX - g.viewW + 4;
    if (g.cursorY < g.viewY+2)            g.viewY = g.cursorY - 2;
    if (g.cursorY > g.viewY+g.viewH-3)    g.viewY = g.cursorY - g.viewH + 3;
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

    // Precompute drag-selection box (map coords); -1 means no active box
    int boxX0 = -1, boxY0 = -1, boxX1 = -1, boxY1 = -1;
    if (g.dragging && g.mode != M_WALL_DRAG) {
        boxX0 = std::min(g.dragStartX, g.cursorX);
        boxY0 = std::min(g.dragStartY, g.cursorY);
        boxX1 = std::max(g.dragStartX, g.cursorX);
        boxY1 = std::max(g.dragStartY, g.cursorY);
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
            int scY = sy+2, scX = sx;
            if (!inBounds(mx, my)) { mvaddch(scY, scX, ' '); continue; }
            Tile& tile = g.map[my][mx];
            bool vis = tile.visible[0], expl = tile.explored[0];
            bool isCur = (mx == g.cursorX && my == g.cursorY);

            if (!expl) {
                if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, ' '); attroff(COLOR_PAIR(CP_CURSOR)); }
                else { mvaddch(scY, scX, ' '); }
                continue;
            }

            char ch; int cp;
            getTerrainVisual(tile.terrain, mx, my, ch, cp);

            if (!vis) {
                if (isCur) {
                    attron(COLOR_PAIR(CP_CURSOR));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else                         mvprintw(scY, scX, "%s", getCharEmoji(ch));
                    attroff(COLOR_PAIR(CP_CURSOR));
                } else {
                    attron(COLOR_PAIR(CP_FOG_EXPLORED));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else                         mvprintw(scY, scX, "%s", getCharEmoji(ch));
                    attroff(COLOR_PAIR(CP_FOG_EXPLORED));
                }
                continue;
            }

            // Wall drag preview overrides terrain.
            // Emoji mode shows ■ (solid block) matching the completed wall glyph.
            if (wallPrev[my][mx]) { ch = '#'; cp = CP_PLAYER; }

            // Use a chtype-wide draw glyph so completed walls can use the ACS solid block.
            chtype drawCh = (chtype)ch;
            if (wallPrev[my][mx]) drawCh = ACS_CKBOARD;

            Entity* ent = entityAt(mx, my);
            // Cloaking: enemy units fade at night/storm unless a friendly eye is close.
            // Wheat fields also conceal enemies — units in crops need close detection.
            bool inCrop = ent && !isBuilding(ent->type) && tile.terrain == T_WHEAT;
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && (isConcealing() || inCrop) && !isDetectedBy(mx, my, 0)) ent = nullptr;
            // Render priority + stack count. When multiple units share a tile,
            // prefer the highest-value military so e.g. knights show through a
            // pile of peasants. Also count any same-owner combat units on the
            // tile so we can show an uppercase glyph for stacks of 2+.
            int stackedMil = 0;
            if (ent && ent->alive && !isBuilding(ent->type)) {
                auto isMil = [](EntityType t) {
                    return t==E_MILITIA||t==E_ARCHER||t==E_KNIGHT||t==E_CATAPULT
                        || t==E_WARSHIP;
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

                // Default emoji is the entity's body symbol.
                emojiStr = getEntityEmoji(ent->type);

                // Colour pair: ownership → background colour.
                // Farms keep wheat colours regardless of owner.
                // Gaia/nature (animals) use type-specific foreground colours with
                // no ownership background — they are neutral entities.
                // Ships keep the wood-deck background for their hull look.
                if (ent->type == E_FARM)
                    cp = (g.tick%40 < 20) ? CP_WHEAT : CP_WHEAT_GOLD;
                else if (ent->owner == OWNER_NATURE) {
                    if      (ent->type == E_WOLF)  cp = CP_WOLF;
                    else if (ent->type == E_SHEEP) cp = CP_SHEEP;
                    else if (ent->type == E_BOAR)  cp = CP_BOAR;
                    else                           cp = CP_DEER;
                } else {
                    // Player-owned land units and buildings: background = owner colour.
                    cp = ownerColorPair(ent->owner, night);
                }
                // Ships override — wood-deck background preserved on water.
                if (isNaval(ent->type))
                    cp = (ent->owner == 0) ? CP_SHIP_PLAYER : CP_SHIP_ENEMY;

                // State-specific glyph overrides (gate, construction, siege engines, alert).
                if (ent->type == E_GATE && !ent->underConstruction) {
                    ch = ent->gateOpen ? '-' : '|';
                    drawCh = (chtype)ch;
                    // ▬ = open (horizontal bar), ║ = closed (double vertical)
                    emojiStr = ent->gateOpen ? "\xe2\x96\xac" : "\xe2\x95\x91";
                }
                if (ent->underConstruction && g.tick%10 < 5) {
                    ch = '#'; drawCh = (chtype)ch;
                    emojiStr = "\xe2\x96\xa0";  // ■ pulsing during construction
                }
                // Dwarf-Fortress-style solid wall block when complete.
                // Emoji mode uses ■ (same visual intent, but valid UTF-8).
                if (ent->type == E_WALL && !ent->underConstruction) {
                    drawCh = ACS_CKBOARD;
                    emojiStr = "\xe2\x96\xa0";  // ■ U+25A0
                }
                // Siege engine arm animations.
                // Catapult: arm at rest = ◄ (loaded), arm firing = ╱ (swinging).
                if (ent->type == E_CATAPULT) {
                    bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_CATAPULT].atkSpeed*2/3;
                    ch = firing ? '/' : '-'; drawCh = (chtype)ch;
                    emojiStr = firing ? "\xe2\x95\xb1" : "\xe2\x97\x84"; // ╱ : ◄
                }
                // Ram: approaching = ► (pointer), ramming = ▶ (larger triangle, impact).
                if (ent->type == E_RAM) {
                    bool ramming = ent->state==S_ATTACKING && ent->atkCd > STATS[E_RAM].atkSpeed*2/3;
                    ch = ramming ? '=' : '-'; drawCh = (chtype)ch;
                    emojiStr = ramming ? "\xe2\x96\xb6" : "\xe2\x96\xba"; // ▶ : ►
                }
                // Recently in combat: gentle '!' pulse — ~1.5 Hz, not strobing.
                if (ent->alertTicks > 0 && (g.tick % 8) < 4) {
                    ch = '!'; drawCh = (chtype)ch;
                    emojiStr = "!";
                }
            }
            // Projectile overwrites terrain/entity glyph; keep ASCII char for colour lookup.
            for (auto& p : g.projectiles) {
                if (!p.alive) continue;
                if ((int)roundf(p.x)==mx && (int)roundf(p.y)==my) {
                    ch = p.glyph; cp = p.color; drawCh = (chtype)ch;
                    // Projectile emoji: boulder → ● (solid circle), arrow/bolt → · (dot)
                    emojiStr = (p.color == CP_PROJ_BOULDER)
                               ? "\xe2\x97\x8f"   // ● U+25CF
                               : "\xc2\xb7";       // · U+00B7
                }
            }
            for (const auto& m : g.actionMarkers) {
                if (m.x == mx && m.y == my && m.ticks > 0 && (g.tick % 6) < 4) {
                    ch = m.glyph; cp = CP_UI_HIGH; drawCh = (chtype)ch;
                    emojiStr = (m.glyph == '!') ? "!" : (m.glyph == '#') ? "\xe2\x96\xa0" : "\xc3\x97";
                }
            }

            // When no entity is present, terrain char drives the emoji string.
            if (!emojiStr) emojiStr = getCharEmoji(ch);

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

            bool onBoxBorder = (boxX0 >= 0)
                && mx >= boxX0 && mx <= boxX1 && my >= boxY0 && my <= boxY1
                && (mx == boxX0 || mx == boxX1 || my == boxY0 || my == boxY1);
            bool onRangeRing = (ringR > 0)
                && std::max(std::abs(mx - ringX), std::abs(my - ringY)) == ringR;

            // Unified draw: ASCII uses mvaddch/chtype; emoji uses mvprintw with UTF-8.
            // All subsequent positions use absolute mv* coords so ncurses' internal
            // cursor model (which counts bytes, not columns) doesn't accumulate.
            auto drawAt = [&](int y, int x, chtype dch, const char* estr) {
                if (displayMode == DM_ASCII) mvaddch(y, x, dch);
                else                         mvprintw(y, x, "%s", estr);
            };

            if (isCur) {
                attron(COLOR_PAIR(CP_CURSOR));
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(CP_CURSOR));
            } else if (onBoxBorder) {
                // Vivid selection-box border that pops on any terrain.
                attron(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
            } else if (onRangeRing && !ent) {
                // Subtle range-ring marker on empty tiles only.
                attron(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
                drawAt(scY, scX, '.', "\xc2\xb7");  // · U+00B7
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

            // Siege engines render as 2 chars: arm then body.
            // ASCII: body char 'c' or 'r'.  Emoji: ⊙ (catapult barrel) or ▬ (ram).
            if (ent && ent->alive && (ent->type==E_CATAPULT||ent->type==E_RAM)
                    && sx+1 < g.viewW && !entityAt(mx+1, my)) {
                char sc = (ent->type==E_CATAPULT) ? 'c' : 'r';
                const char* bodyEmoji = (ent->type==E_CATAPULT)
                    ? "\xe2\x8a\x99"   // ⊙ U+2299 catapult barrel
                    : "\xe2\x96\xac";  // ▬ U+25AC ram body
                int sattr = COLOR_PAIR(cp) | A_BOLD;
                if (isSel) sattr |= A_REVERSE;
                attron(sattr);
                if (displayMode == DM_ASCII) mvaddch(scY, scX+1, sc);
                else                         mvprintw(scY, scX+1, "%s", bodyEmoji);
                attroff(sattr);
            }
        }
    }

    // Weather overlay: very gentle pulse — ~1.5 Hz, never overlays units/buildings.
    if (g.weather != W_CLEAR && (g.tick % 8) == 0) {
        bool snowWeather = (g.weather == W_SNOW);
        int density = (g.weather == W_STORM) ? 2 : 1; // percent — very sparse
        int frame = g.tick;
        for (int sy = 0; sy < g.viewH; sy++) for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[0]) continue;
            if (entityAt(mx, my)) continue; // don't paint over units/buildings
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(frame*83492791u));
            if ((int)(h % 100) >= density) continue;
            if (snowWeather) {
                // Transparent-bg white glyph: flake adopts whatever terrain colour is beneath it.
                attron(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
                if (displayMode == DM_ASCII) mvaddch(sy+2, sx, '*');
                else                         mvprintw(sy+2, sx, "\xe2\x9c\xa6"); // ✦
                attroff(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_RAIN)|A_BOLD);
                if (displayMode == DM_ASCII) mvaddch(sy+2, sx, '.');
                else                         mvprintw(sy+2, sx, "\xc2\xb7");     // ·
                attroff(COLOR_PAIR(CP_RAIN)|A_BOLD);
            }
        }
    }
}

// ============================================================
// UI RENDER
// ============================================================
void renderUI() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    Player& p = g.players[0]; int panelW = 24, panelX = maxX - panelW;

    // Top bar
    attron(COLOR_PAIR(CP_UI_BAR)|A_BOLD); mvhline(0, 0, ' ', maxX);
    mvprintw(0, 1, " REALM "); attroff(A_BOLD);
    int idleCount = 0, idleBldg = 0, popForecast = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != 0) continue;
        if (e.type == E_PEASANT && e.state == S_IDLE) idleCount++;
        if (isBuilding(e.type) && !e.underConstruction) {
            bool producer = (e.type==E_TOWNHALL||e.type==E_BARRACKS||e.type==E_STABLE||e.type==E_DOCK);
            if (producer && e.producing == E_NONE && e.queue.empty()) idleBldg++;
            if (e.producing != E_NONE) popForecast += STATS[e.producing].supplyUsed;
            for (int qt : e.queue) popForecast += STATS[(EntityType)qt].supplyUsed;
        }
    }
    mvprintw(0, 9, "Gold:%-5d Wood:%-5d Food:%-5d Pop:%d/%d(+%d) Idle:%d/%d",
             p.gold, p.wood, p.food, p.supply, p.supplyMax, popForecast, idleCount, idleBldg);

    int iconX = maxX - 22;
    if (getBrightness() > 0.5f) {
        attron(COLOR_PAIR(CP_SUN)|A_BOLD);
        // Emoji mode: ✦ (U+2726 BLACK FOUR POINTED STAR, width-1) for sun.
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "\xe2\x9c\xa6" : "*");
        attroff(COLOR_PAIR(CP_SUN)|A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_MOON));
        // Emoji mode: ◐ (U+25D0 CIRCLE WITH LEFT HALF BLACK, width-1) for moon.
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "\xe2\x97\x90" : "o");
        attroff(COLOR_PAIR(CP_MOON));
    }
    attron(COLOR_PAIR(CP_UI_BAR));
    const char* wn = (g.weather == W_STORM) ? "Storm" : (g.weather == W_RAIN) ? "Rain " : (g.weather == W_SNOW) ? "Snow " : "Clear";
    mvprintw(0, iconX+1, " %-5s %-6s %s", getTimeName(), getSeasonName(), wn);
    attroff(COLOR_PAIR(CP_UI_BAR));

    // Terrain info bar
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(1, 0, '-', g.viewW); attroff(COLOR_PAIR(CP_UI_DIM));
    if (inBounds(g.cursorX, g.cursorY) && g.map[g.cursorY][g.cursorX].explored[0]) {
        Tile& ct = g.map[g.cursorY][g.cursorX];
        const char* bn[] = {"Temperate","Desert","Tundra","Swamp","Woodland","Volcanic","Ocean"};
        const char* tn[] = {"Grassland","Tall Grass","Wildflowers","Meadow","Oak Forest","Pine Forest",
            "Palm Grove","Dead Tree","Mountain","Rolling Hills","Stone","Deep Water","Shallows",
            "Marshland","Reed Bed","Gold Deposit","Sandy Ground","Sand Dunes","Snow Cover","Frozen Ice",
            "Bare Earth","Stone Road","Mud","Wheat Field","Berry Bush","Fish Shoal","Ancient Ruins","Gravel",
            "Lava Fissure","Volcanic Ash",
            "Castle Wall","Castle Floor","Castle Gate"};
        attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(1, 1, "%-16s", tn[ct.terrain]); attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(1, 18, "[%s]", bn[ct.biome]); attroff(COLOR_PAIR(CP_UI_DIM));
        if (ct.resources > 0) { attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(1, 30, "Res:%d", ct.resources); attroff(COLOR_PAIR(CP_UI_HIGH)); }
    }

    // Panel separator
    for (int y = 0; y < maxY; y++) { attron(COLOR_PAIR(CP_UI_DIM)); mvaddch(y, panelX-1, '|'); attroff(COLOR_PAIR(CP_UI_DIM)); }

    // Minimap
    attron(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD); mvprintw(0, panelX+1, "Map"); attroff(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);
    int mmW = panelW-2, mmH = std::min(g.viewH/3, 14), mmY = 1;
    for (int my = 0; my < mmH; my++) for (int mx = 0; mx < mmW; mx++) {
        int mapX = mx*MAP_W/mmW, mapY = my*MAP_H/mmH;
        char mch = ' '; int mcp = CP_FOG;
        if (g.map[mapY][mapX].explored[0]) {
            Terrain t = g.map[mapY][mapX].terrain;
            if (t==T_WATER||t==T_SHALLOWS)              { mch='~'; mcp=CP_MM_WATER;  }
            else if (t==T_MOUNTAIN||t==T_STONE)          { mch='^'; mcp=CP_MM_MTN;   }
            else if (t==T_FOREST||t==T_PINE||t==T_PALM)  { mch='.'; mcp=CP_MM_FOREST;}
            else if (t==T_GOLD)                           { mch='$'; mcp=CP_MM_GOLD;  }
            else if (t==T_SAND||t==T_DUNES)               { mch='.'; mcp=CP_MM_SAND;  }
            else if (t==T_SNOW||t==T_ICE)                 { mch='.'; mcp=CP_MM_SNOW;  }
            else if (t==T_CASTLE_WALL||t==T_CASTLE_GATE)  { mch='#'; mcp=CP_MM_CASTLE;}
            else { mch='.'; mcp=CP_FOG; }
        }
        if (g.map[mapY][mapX].visible[0]) {
            Entity* ent = entityAt(mapX, mapY);
            // Hide cloaked enemies from the minimap as well.
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && isConcealing() && !isDetectedBy(mapX, mapY, 0)) ent = nullptr;
            if (ent && ent->alive) {
                // Mirror main-map crop/cloaking on the minimap.
                bool mmInCrop = !isBuilding(ent->type) && g.map[mapY][mapX].terrain == T_WHEAT;
                if (ent->owner != 0 && ent->owner < MAX_PLAYERS
                    && (isConcealing() || mmInCrop) && !isDetectedBy(mapX, mapY, 0))
                    ent = nullptr;
            }
            if (ent && ent->alive) {
                mch = isBuilding(ent->type) ? '#' : '*';
                if      (ent->owner == 0)            mcp = CP_MM_PLAYER;
                else if (ent->owner < MAX_PLAYERS)   mcp = CP_MM_ENEMY;
                else                                  mcp = CP_MM_ANIMAL;
            }
        }
        attron(COLOR_PAIR(mcp)); mvaddch(mmY+my, panelX+1+mx, mch); attroff(COLOR_PAIR(mcp));
    }

    // Selection info panel
    int iy = mmY + mmH + 1;
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(iy-1, panelX, '-', panelW); attroff(COLOR_PAIR(CP_UI_DIM));
    if (inBounds(g.cursorX, g.cursorY)) {
        const Tile& ct = g.map[g.cursorY][g.cursorX];
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(iy++, panelX+1, "Tile: %.14s", terrainName(ct.terrain));
        mvprintw(iy++, panelX+1, "Biome: %.13s", biomeName(ct.biome));
        if (ct.resources > 0) mvprintw(iy++, panelX+1, "Resource: %d", ct.resources);
        int stack = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (g.cursorX>=e.x && g.cursorX<e.x+st.sizeW && g.cursorY>=e.y && g.cursorY<e.y+st.sizeH)
                : (g.cursorX==e.x && g.cursorY==e.y);
            if (!covers) continue;
            if (stack == 0) mvprintw(iy++, panelX+1, "Stack:");
            if (stack < 3) mvprintw(iy++, panelX+2, "%.16s", st.name);
            stack++;
        }
        if (stack == 0) mvprintw(iy++, panelX+1, "Stack: empty");
        else if (stack > 3) mvprintw(iy++, panelX+2, "+%d more", stack - 3);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        if (g.diagnostics) {
            attron(COLOR_PAIR(CP_UI_HIGH));
            mvprintw(iy++, panelX+1, "Diag T%d M:%s", g.tick, modeName(g.mode));
            mvprintw(iy++, panelX+1, "Ent:%d Proj:%d", (int)g.entities.size(), (int)g.projectiles.size());
            mvprintw(iy++, panelX+1, "Seed:%u AI:%d", g.seed, g.startupAIs);
            attroff(COLOR_PAIR(CP_UI_HIGH));
        }
        iy++;
    }

    if (g.selectedIds.size() > 1) {
        // Multi-unit group summary
        int counts[6] = {0};
        for (int sid : g.selectedIds) {
            Entity* e = findEntity(sid); if (!e || !e->alive) continue;
            switch (e->type) {
            case E_PEASANT:  counts[0]++; break; case E_MILITIA:  counts[1]++; break;
            case E_ARCHER:   counts[2]++; break; case E_KNIGHT:   counts[3]++; break;
            case E_CATAPULT: counts[4]++; break; default: counts[5]++; break;
            }
        }
        attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        mvprintw(iy++, panelX+1, "Group: %d units", (int)g.selectedIds.size());
        attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_TEXT));
        // Use the entity glyph/emoji for each unit type in the group summary.
        if (counts[0]) mvprintw(iy++, panelX+1, "  %s x%d Peasant",  getEntityEmoji(E_PEASANT),  counts[0]);
        if (counts[1]) mvprintw(iy++, panelX+1, "  %s x%d Militia",  getEntityEmoji(E_MILITIA),  counts[1]);
        if (counts[2]) mvprintw(iy++, panelX+1, "  %s x%d Archer",   getEntityEmoji(E_ARCHER),   counts[2]);
        if (counts[3]) mvprintw(iy++, panelX+1, "  %s x%d Knight",   getEntityEmoji(E_KNIGHT),   counts[3]);
        if (counts[4]) mvprintw(iy++, panelX+1, "  %s x%d Catapult", getEntityEmoji(E_CATAPULT), counts[4]);
        if (counts[5]) mvprintw(iy++, panelX+1, "  + x%d Other",    counts[5]);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        iy++;
        attron(COLOR_PAIR(CP_UI_ACCENT));
        mvprintw(iy++, panelX+1, "[Enter] Move/Attack");
        mvprintw(iy++, panelX+1, "[G] Assign to group");
        mvprintw(iy++, panelX+1, "[A] Select all mil.");
        mvprintw(iy++, panelX+1, "[1-9] Groups");
        attroff(COLOR_PAIR(CP_UI_ACCENT));
    } else {
        Entity* sel = findEntity(g.selectedId);
        if (sel) {
            auto& st = STATS[sel->type];
            int nc = (sel->owner == 0) ? CP_PLAYER : CP_ENEMY;
            attron(COLOR_PAIR(nc)|A_BOLD); mvprintw(iy++, panelX+1, "%-20s", st.name); attroff(COLOR_PAIR(nc)|A_BOLD);
            int barW = panelW-4, filled = sel->hp * barW / std::max(1, sel->maxHp);
            int pct = sel->hp * 100 / std::max(1, sel->maxHp);
            int hc = (pct>60) ? CP_HP_GREEN : (pct>30) ? CP_HP_YELLOW : CP_HP_RED;
            mvprintw(iy, panelX+1, "HP");
            for (int i = 0; i < barW; i++) {
                int c = (i < filled) ? hc : CP_FOG;
                attron(COLOR_PAIR(c)); mvaddch(iy, panelX+3+i, (i<filled)?'|':'-'); attroff(COLOR_PAIR(c));
            }
            iy++;
            attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(iy++, panelX+1, "%d / %d", sel->hp, sel->maxHp); attroff(COLOR_PAIR(CP_UI_TEXT));
            if (isUnit(sel->type)) {
                attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(iy++, panelX+1, "ATK %-3d  RNG %-2d", st.atk, st.range); attroff(COLOR_PAIR(CP_UI_TEXT));
                std::string stDesc;
                if (sel->type == E_PEASANT) {
                    switch (sel->state) {
                    case S_IDLE:      stDesc = "Idle"; break;
                    case S_MOVING:    stDesc = "Moving"; break;
                    case S_ATTACKING: stDesc = "Fighting"; break;
                    case S_GATHERING:
                        if      (sel->cargo.type == CR_GOLD) stDesc = "Mining gold";
                        else if (sel->cargo.type == CR_WOOD) stDesc = "Chopping wood";
                        else if (sel->cargo.type == CR_FISH) stDesc = "Fishing";
                        else                                stDesc = "Picking berries";
                        break;
                    case S_BUILDING:  { Entity* b = findEntity(sel->targetId);
                                        if (b && !b->underConstruction && b->type==E_FARM)
                                            stDesc = "Tending farm";
                                        else
                                            stDesc = b ? (std::string("Building ") + STATS[b->type].name) : "Building";
                                        break; }
                    case S_RETURNING:
                        if      (sel->cargo.type == CR_GOLD) stDesc = "Carrying gold";
                        else if (sel->cargo.type == CR_WOOD) stDesc = "Carrying wood";
                        else if (sel->cargo.type == CR_FISH) stDesc = "Carrying fish";
                        else                                stDesc = "Carrying food";
                        break;
                    default:          stDesc = "Idle"; break;
                    }
                } else {
                    stDesc = stateName(sel->state);
                }
                attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(iy++, panelX+1, "%s", stDesc.c_str()); attroff(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->cargo.amount > 0) {
                    const char* what = cargoResourceName(sel->cargo.type);
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Carrying: %d %s", sel->cargo.amount, what);
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                }
                // Transport cargo display + unload hint
                if (sel->type == E_TRANSPORT && sel->owner == 0) {
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Cargo: %d/%d", (int)sel->garrison.size(), garrisonCap(E_TRANSPORT));
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                    attron(COLOR_PAIR(CP_UI_ACCENT));
                    mvprintw(iy++, panelX+1, "[U] Unload");
                    attroff(COLOR_PAIR(CP_UI_ACCENT));
                }
            }
            if (sel->producing != E_NONE) {
                iy++;
                int pp = sel->trainProgress * 100 / std::max(1, sel->trainTime);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Training: %s", STATS[sel->producing].name);
                int pb = panelW-4, pf = pp*pb/100;
                for (int i = 0; i < pb; i++) { int c=(i<pf)?CP_UI_HIGH:CP_FOG; attron(COLOR_PAIR(c)); mvaddch(iy, panelX+1+i, (i<pf)?'=':'-'); attroff(COLOR_PAIR(c)); }
                iy++; mvprintw(iy++, panelX+1, "%d%%", pp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            if (!sel->queue.empty()) {
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Queue: %d", (int)sel->queue.size());
                int n = std::min((int)sel->queue.size(), panelW-4);
                for (int i = 0; i < n; i++) {
                    // Each queue slot is one map-cell-width apart regardless of mode.
                    if (displayMode == DM_ASCII)
                        mvaddch(iy, panelX+1+i, STATS[(EntityType)sel->queue[i]].glyph);
                    else
                        mvprintw(iy, panelX+1+i, "%s", getEntityEmoji(sel->queue[i]));
                }
                iy++;
                attroff(COLOR_PAIR(CP_UI_DIM));
            }
            if (sel->researching != 0) {
                iy++;
                int pp = sel->researchProgress * 100 / std::max(1, sel->researchTime);
                const char* rn = (sel->researching == R_IRON_WEAPONS) ? "Iron Weapons" :
                                 (sel->researching == R_CROSSBOWS)   ? "Crossbows"    : "Research";
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Researching: %s", rn);
                int pb = panelW-4, pf = pp*pb/100;
                for (int i = 0; i < pb; i++) { int c=(i<pf)?CP_UI_HIGH:CP_FOG; attron(COLOR_PAIR(c)); mvaddch(iy, panelX+1+i, (i<pf)?'=':'-'); attroff(COLOR_PAIR(c)); }
                iy++; mvprintw(iy++, panelX+1, "%d%%", pp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            if (sel->underConstruction) {
                int bp = sel->hp * 100 / std::max(1, sel->maxHp);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Building: %d%%", bp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            iy++;
            if (sel->owner == 0) {
                attron(COLOR_PAIR(CP_UI_DIM)); mvhline(iy-1, panelX, '-', panelW); attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->type == E_PEASANT) { mvprintw(iy++, panelX+1, "[B] Build"); mvprintw(iy++, panelX+1, "[Enter] Move/Gather"); }
                else if (isUnit(sel->type)) mvprintw(iy++, panelX+1, "[Enter] Move/Attack");
                else if (isBuilding(sel->type) && !sel->underConstruction) {
                    if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK) mvprintw(iy++, panelX+1, "[T] Train");
                    if (sel->type==E_DOCK)        mvprintw(iy++, panelX+1, "Fish drop-off");
                    if (sel->type==E_BLACKSMITH) mvprintw(iy++, panelX+1, "Speeds training");
                    if (sel->type==E_CHURCH)     mvprintw(iy++, panelX+1, "Heals nearby +Vision");
                    if (sel->type==E_MARKET)     mvprintw(iy++, panelX+1, "Passive gold income");
                    if (sel->type==E_FARM)        { mvprintw(iy++, panelX+1, "Generates food");
                                                     mvprintw(iy++, panelX+1, "Assign peasant to tend");
                                                     mvprintw(iy++, panelX+1, "Ripe: %d / 20", sel->storedFood); }
                    if (sel->type==E_LUMBER_CAMP) mvprintw(iy++, panelX+1, "Wood drop-off");
                    if (sel->type==E_MINING_CAMP) mvprintw(iy++, panelX+1, "Gold drop-off");
                    if (sel->type==E_MILL)        mvprintw(iy++, panelX+1, "Enables harvesting");
                    if (sel->type==E_GATE) {
                        mvprintw(iy++, panelX+1, sel->gateOpen ? "State: Open" : "State: Closed");
                        mvprintw(iy++, panelX+1, sel->gateLocked ? "Mode: Locked" : "Mode: Auto");
                        mvprintw(iy++, panelX+1, "[O] Toggle/Lock");
                    }
                    if (sel->type==E_CASTLE)     mvprintw(iy++, panelX+1, "+15 Supply, 350 HP");
                    if (canGarrisonIn(sel->type)) {
                        mvprintw(iy++, panelX+1, "Garrison: %d/%d",
                                 (int)sel->garrison.size(), garrisonCap(sel->type));
                        mvprintw(iy++, panelX+1, "[U] Eject all");
                    }
                }
                attroff(COLOR_PAIR(CP_UI_ACCENT));
            }
        } else {
            attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy, panelX+1, "No selection"); attroff(COLOR_PAIR(CP_UI_DIM));
            iy += 2;
            if (displayMode == DM_ASCII) {
                attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend (ASCII) --"); attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_UI_TEXT));
                mvprintw(iy++, panelX+1, "$ Gold   T Oak");
                mvprintw(iy++, panelX+1, "^ Mtn    Y Pine");
                mvprintw(iy++, panelX+1, "~ Water  n Hills");
                mvprintw(iy++, panelX+1, ": Berry  %% Wheat");
                mvprintw(iy++, panelX+1, "# Castle & Ruins");
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                attron(COLOR_PAIR(CP_OWN_P0));
                mvprintw(iy++, panelX+1, "p Peasant  m Militia");
                mvprintw(iy++, panelX+1, "a Archer   k Knight");
                mvprintw(iy++, panelX+1, "c Catapult");
                attroff(COLOR_PAIR(CP_OWN_P0)); iy++;
                attron(COLOR_PAIR(CP_DEER));
                mvprintw(iy++, panelX+1, "d Deer  s Sheep");
                mvprintw(iy++, panelX+1, "w Wolf  o Boar");
                attroff(COLOR_PAIR(CP_DEER));
            } else {
                // Emoji legend: show Unicode symbols + ownership colour key.
                attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend (Emoji) --"); attroff(COLOR_PAIR(CP_UI_DIM));
                // Terrain (two per row, explicit x for column 2)
                attron(COLOR_PAIR(CP_UI_TEXT));
                // Use mvprintw at explicit positions so multi-byte UTF-8 doesn't
                // misalign ncurses' internal cursor for the right-column label.
                mvprintw(iy,   panelX+1,  "\xe2\x99\xa6 Gold");     // ♦
                mvprintw(iy++, panelX+10, "\xe2\x99\xa3 Oak");      // ♣
                mvprintw(iy,   panelX+1,  "\xe2\x96\xb2 Mtn");      // ▲
                mvprintw(iy++, panelX+10, "\xe2\x86\x91 Pine");      // ↑
                mvprintw(iy,   panelX+1,  "\xe2\x89\x88 Water");     // ≈
                mvprintw(iy++, panelX+10, "\xe2\x88\xa9 Hills");     // ∩
                mvprintw(iy,   panelX+1,  "\xe2\x88\xb7 Berry");     // ∷
                mvprintw(iy++, panelX+10, "\xc2\xa7 Wheat");         // §
                mvprintw(iy,   panelX+1,  "\xe2\x96\xa0 Wall");      // ■
                mvprintw(iy++, panelX+10, "\xc2\xb6 Ruins");         // ¶
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                // Military units with player-colour background
                attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
                mvprintw(iy,   panelX+1,  "\xe2\x99\x9f Peasant");   // ♟
                mvprintw(iy++, panelX+12, "\xe2\x99\x99 Militia");   // ♙
                mvprintw(iy,   panelX+1,  "\xe2\x99\x97 Archer");    // ♝
                mvprintw(iy++, panelX+12, "\xe2\x99\x9e Knight");    // ♞
                mvprintw(iy++, panelX+1,  "\xe2\x8a\x99 Catapult");  // ⊙
                attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD); iy++;
                // Animals (neutral — no ownership background)
                attron(COLOR_PAIR(CP_DEER));
                mvprintw(iy,   panelX+1,  "\xe2\x96\xb7 Deer");      // ▷
                mvprintw(iy++, panelX+10, "\xe2\x97\x8c Sheep");     // ◌
                mvprintw(iy,   panelX+1,  "\xe2\x97\x81 Wolf");      // ◁
                mvprintw(iy++, panelX+10, "\xe2\x97\x8f Boar");      // ●
                attroff(COLOR_PAIR(CP_DEER)); iy++;
                // Ownership colour key
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Bg=owner colour:");
                attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_OWN_P0)); mvprintw(iy, panelX+1, "You"); attroff(COLOR_PAIR(CP_OWN_P0));
                attron(COLOR_PAIR(CP_OWN_P1)); mvprintw(iy, panelX+5, "P2");  attroff(COLOR_PAIR(CP_OWN_P1));
                attron(COLOR_PAIR(CP_OWN_P2)); mvprintw(iy, panelX+8, "P3");  attroff(COLOR_PAIR(CP_OWN_P2));
                attron(COLOR_PAIR(CP_OWN_P3)); mvprintw(iy, panelX+11,"P4");  attroff(COLOR_PAIR(CP_OWN_P3));
                iy++;
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Sel=reversed bg");
                attroff(COLOR_PAIR(CP_UI_DIM));
            }
        }
    }

    // Bottom bars
    int botY2 = maxY-2, botY1 = maxY-1;
    attron(COLOR_PAIR(CP_UI_BAR)); mvhline(botY2, 0, ' ', maxX);
    if (g.mode == M_BUILD_SELECT)
        mvprintw(botY2, 1, " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ");
    else if (g.mode == M_TRAIN_SELECT) {
        Entity* s2 = findEntity(g.selectedId);
        if (s2) {
            if (s2->type==E_TOWNHALL)  mvprintw(botY2, 1, " TRAIN: [P]easant(50g), repeat keys to queue [Esc] ");
            else if (s2->type==E_BARRACKS) mvprintw(botY2, 1, " TRAIN: [M]ilitia [A]rcher [C]atapult [R]am, repeat keys to queue [Esc] ");
            else if (s2->type==E_STABLE)   mvprintw(botY2, 1, " TRAIN: [K]night, repeat keys to queue [Esc] ");
            else if (s2->type==E_DOCK)     mvprintw(botY2, 1, " TRAIN: [B]oat [W]arship [T]ransport, repeat keys to queue [Esc] ");
        }
    } else if (g.mode == M_WALL_DRAG) {
        if (g.dragging)
            mvprintw(botY2, 1, " WALL: Drag to cursor position — release to place  [Esc] Cancel ");
        else
            mvprintw(botY2, 1, " WALL: Click and drag to draw wall line  [Esc] Cancel ");
    } else if (g.mode == M_PAUSED) {
        attron(A_BOLD); mvprintw(botY2, 1, " PAUSED - Press [P] to resume "); attroff(A_BOLD);
    } else if (g.mode == M_GAME_OVER) {
        attron(A_BOLD);
        if (g.winner==0) mvprintw(botY2, 1, " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit ");
        else             mvprintw(botY2, 1, " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ");
        attroff(A_BOLD);
    } else if (g.groupAssignPending) {
        attron(A_BOLD); mvprintw(botY2, 1, " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel "); attroff(A_BOLD);
    } else {
        mvprintw(botY2, 1, " Arrows:Move  Spc:Select  Enter:Cmd  B:Build T:Train ?:Help D:Diag V:Save L:Load Q:Resign X:Exit ");
    }
    attroff(COLOR_PAIR(CP_UI_BAR));

    mvhline(botY1, 0, ' ', maxX);
    if (g.statusTimer > 0) {
        attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        mvprintw(botY1, 1, ">> %s", g.statusMsg.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        g.statusTimer--;
    }
    attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(botY1, maxX-12, "(%d,%d)", g.cursorX, g.cursorY); attroff(COLOR_PAIR(CP_UI_DIM));
}

static void renderHelpOverlay() {
    if (!g.helpOverlay) return;
    int maxY, maxX;
    getmaxyx(stdscr, maxY, maxX);
    int w = std::min(maxX - 4, 78);
    int h = std::min(maxY - 4, 24);
    int x = std::max(1, (maxX - w) / 2);
    int y = std::max(1, (maxY - h) / 2);
    attron(COLOR_PAIR(CP_UI_BAR));
    for (int yy = 0; yy < h; yy++) mvhline(y + yy, x, ' ', w);
    mvhline(y, x, '-', w);
    mvhline(y + h - 1, x, '-', w);
    mvvline(y, x, '|', h);
    mvvline(y, x + w - 1, '|', h);
    mvaddch(y, x, '+');
    mvaddch(y, x + w - 1, '+');
    mvaddch(y + h - 1, x, '+');
    mvaddch(y + h - 1, x + w - 1, '+');
    attroff(COLOR_PAIR(CP_UI_BAR));

    int row = y + 1;
    attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
    mvprintw(row++, x + 2, "Realm Help");
    attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
    int n = 0;
    const CommandBinding* commands = gameplayCommands(n);
    attron(COLOR_PAIR(CP_UI_TEXT));
    for (int i = 0; i < n && row < y + h - 6; i++)
        mvprintw(row++, x + 2, "%-16s %-14s %.36s", commands[i].keys, commands[i].label, commands[i].help);
    row++;
    mvprintw(row++, x + 2, "Food: berries, hunting, farms/mills, wheat, fishing.");
    mvprintw(row++, x + 2, "Winter drains food; starvation damages units.");
    mvprintw(row++, x + 2, "Owner backgrounds mark factions; animals are neutral.");
    mvprintw(row++, x + 2, "! means recent combat; x/+/# are command markers.");
    attroff(COLOR_PAIR(CP_UI_TEXT));
    attron(COLOR_PAIR(CP_UI_HIGH));
    mvprintw(y + h - 2, x + 2, "Press ? to close");
    attroff(COLOR_PAIR(CP_UI_HIGH));
}

void render() { erase(); renderMap(); renderUI(); renderHelpOverlay(); refresh(); }
