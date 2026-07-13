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


// ============================================================
// COLOR INIT
// ============================================================
void initColors() {
    start_color();
    use_default_colors();

    const int bg = -1;

    // Terrain/entity colour pairs keep mostly-transparent backgrounds so the
    // ASCII glyph sits on the terminal's default background.
    auto tileBg = [&](int) -> int { return bg; };

    init_pair(CP_GRASS,         C::GREEN,        tileBg(C::DARK_GREEN));
    init_pair(CP_GRASS_LIGHT,   C::BRIGHT_GREEN, tileBg(C::MED_GREEN));
    init_pair(CP_GRASS_DRY,     C::YELLOW_GREEN, tileBg(C::OLIVE));
    init_pair(CP_TALL_GRASS,    C::MED_GREEN,    tileBg(C::DARK_GREEN));
    init_pair(CP_HEATH,         96,              tileBg(C::DARK_GREEN));   // heather purple on moss
    init_pair(CP_MM_HEATH,      139,             C::NEAR_BLACK);
    // Candlelight spilling from doorways: candle-gold at the threshold,
    // an orange ember wash on the fringe. Firelight is gold/orange, never
    // brown — the old amber-130 fringe (#af5f00) read as rust, and warm
    // light only reads warm against the cool twilight/night ambient.
    init_pair(CP_TORCHLIT,      222,             bg);   // candle gold (255,215,135)
    init_pair(CP_TORCHLIT_DIM,  172,             bg);   // ember orange (215,135,0)
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

    // Moonlight, one shade off pitch: the unlit world sinks toward black so
    // candle pools and team colours carry the night. Hues follow the eye's
    // night vision (Purkinje shift): warm colours die first, so what little
    // survives is blue-green — moonlit grass is teal-dark, snow glows faint
    // blue, and wheat fades to a pale ghost of its gold, never brown.
    init_pair(CP_NIGHT_GRASS,    C::PINE_GREEN,   tileBg(C::NEAR_BLACK));
    init_pair(CP_NIGHT_TREE,     C::DARKER_GRAY,  bg);
    init_pair(CP_NIGHT_WATER,    C::DEEP_BLUE,    C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   233,             tileBg(C::NEAR_BLACK));
    init_pair(CP_NIGHT_GOLD,     101,             tileBg(C::NEAR_BLACK));   // moonlit wheat: desaturated khaki (135,135,95)
    init_pair(CP_NIGHT_SNOW,     60,              bg);                      // moonlit snow is blue (95,95,135)

    // Twilight, one tint per terrain family. Dusk is the blue hour: the sun
    // is gone and the land is lit only by the sky dome, so everything cools
    // and desaturates — sage grass, teal trees, violet-slate earth, water
    // catching lavender off the sky. Dawn answers in rose: the morning air
    // is clearer than evening's, so first light is pink-grey warming toward
    // gold — fresh green, rose-mauve earth, alpenglow on the snow.
    init_pair(CP_DUSK_GRASS,     65,              bg);   // sage (95,135,95)
    init_pair(CP_DUSK_TREE,      C::PINE_GREEN,   bg);   // teal silhouette
    init_pair(CP_DUSK_GROUND,    60,              bg);   // violet slate (95,95,135)
    init_pair(CP_DUSK_SHIMMER,   140,             C::DEEP_BLUE);   // lavender crests
    init_pair(CP_DUSK_SNOW,      103,             bg);   // slate lavender (135,135,175)
    init_pair(CP_DAWN_GRASS,     71,              bg);   // fresh green (95,175,95)
    init_pair(CP_DAWN_TREE,      65,              bg);   // sage catching first light
    init_pair(CP_DAWN_GROUND,    95,              bg);   // rose mauve (135,95,95)
    init_pair(CP_DAWN_SHIMMER,   174,             C::DEEP_BLUE);   // rose crests
    init_pair(CP_DAWN_SNOW,      181,             bg);   // alpenglow (215,175,175)

    init_pair(CP_DAWN_SKY,       217,             bg);   // dawn rose, not orange
    init_pair(CP_DUSK_SKY,       97,              bg);   // dusk violet (135,95,175)

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

    // Neutral animals do not get player ownership colours.
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

    // Overlay the player's chosen team colour (and AI colours that avoid it).
    applyTeamColors();
}

// ============================================================
// TEAM COLOURS — the player picks one on the splash; opponents are dealt
// distinct colours that are never the player's. The owner→colour mapping is
// re-skinned into the existing CP_OWN_P*/CP_SHIP_P*/CP_MM_* pairs, so the rest
// of the renderer (ownerColorPair etc.) is untouched.
// ============================================================
struct TeamColor { const char* name; int day, night, text; };
static const TeamColor TEAM_COLORS[] = {
    {"Blue",    C::PLAYER_CYAN, C::PLAYER_DIM, C::SNOW_WHITE},
    {"Red",     C::ENEMY_RED,   C::ENEMY_DIM,  C::SNOW_WHITE},
    {"Green",   40,             28,            C::NEAR_BLACK},   // night keeps a readable green
    {"Gold",    C::GOLD,        C::DARK_GOLD,  C::NEAR_BLACK},
    {"Purple",  99,             54,            C::SNOW_WHITE},
    {"Magenta", 170,            90,            C::SNOW_WHITE},
    {"Orange",  208,            C::AMBER,      C::NEAR_BLACK},
    {"Teal",    C::TEAL,        C::PINE_GREEN, C::SNOW_WHITE},
};
static const int NUM_TEAM_COLORS = (int)(sizeof(TEAM_COLORS)/sizeof(TEAM_COLORS[0]));

int  numTeamColors()          { return NUM_TEAM_COLORS; }
const char* teamColorName(int i){ return (i >= 0 && i < NUM_TEAM_COLORS) ? TEAM_COLORS[i].name : "?"; }

void applyTeamColors() {
    int pc = g.playerColor; if (pc < 0 || pc >= NUM_TEAM_COLORS) pc = 0;
    // The non-player colours, in palette order — AIs draw from these.
    int others[NUM_TEAM_COLORS], no = 0;
    for (int i = 0; i < NUM_TEAM_COLORS; i++) if (i != pc) others[no++] = i;
    // The LOCAL seat wears the chosen colour (colours are per-machine
    // presentation — in multiplayer each player sees themself in their own
    // pick); every other seat draws a distinct non-player colour.
    int ownerTeam[MAX_PLAYERS];
    int nxt = 0;
    for (int o = 0; o < MAX_PLAYERS; o++)
        ownerTeam[o] = (o == g.localPlayer) ? pc : others[nxt++ % no];

    const int bg = C::NEAR_BLACK;
    for (int o = 0; o < MAX_PLAYERS; o++) {
        const TeamColor& t = TEAM_COLORS[ownerTeam[o]];
        init_pair(CP_OWN_P0 + o,       t.text, t.day);     // glyph on owner-coloured bg
        int nfg = (t.text == C::NEAR_BLACK) ? C::NEAR_BLACK : C::LIGHT_GRAY;
        init_pair(CP_OWN_P0_NIGHT + o, nfg, t.night);
        init_pair(CP_SHIP_P0 + o,      t.day, C::BROWN);    // owner-coloured hull glyph
    }
    const TeamColor& pt = TEAM_COLORS[pc];
    const TeamColor& et = TEAM_COLORS[others[0]];           // generic "enemy" tint
    init_pair(CP_PLAYER,       pt.day,   bg);
    init_pair(CP_PLAYER_NIGHT, pt.night, bg);
    init_pair(CP_ENEMY,        et.day,   bg);
    init_pair(CP_ENEMY_NIGHT,  et.night, bg);
    init_pair(CP_SHIP_PLAYER,  pt.day,   C::BROWN);
    init_pair(CP_SHIP_ENEMY,   et.day,   C::BROWN);
    init_pair(CP_MM_PLAYER,    pt.day,   bg);
    init_pair(CP_MM_ENEMY,     et.day,   bg);
}

// ============================================================
// OWNERSHIP COLOUR HELPER
// Returns the colour pair that should be applied to a land unit
// or building based on its owner.  Ships are excluded (callers
// handle CP_SHIP_* separately).  Animals/Gaia use their own
// type-specific pairs and are never passed here.
// ============================================================
static int ownerColorPair(int owner, bool night) {
    // CP_OWN_P0..P7 and their night twins are contiguous, one per seat.
    int o = (owner >= 0 && owner < MAX_PLAYERS) ? owner : MAX_PLAYERS - 1;
    return (night ? CP_OWN_P0_NIGHT : CP_OWN_P0) + o;
}

static unsigned tileHash(int x, int y, unsigned salt = 0) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + salt * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float hash01(int x, int y, unsigned salt = 0) {
    return (tileHash(x, y, salt) & 0xFFFFu) / 65535.0f;
}

// ============================================================
// TERRAIN VISUALS
// ============================================================
static bool shouldShowSeasonAt(int x, int y, float threshold) {
    int hash = ((x*7919 + y*6271) & 0xFFFF);
    return (float)hash / 65535.0f < threshold;
}

// --- Gentle tides on large open seas (render-only, never touches the sim) ----
// Rivers and small lakes keep their lively shimmer; only large *blobby* bodies
// of deep water — i.e. seas / open salt water — get a slow tidal swell. A body
// must be both big in area and wide in both dimensions, so a long thin river
// doesn't qualify. The mask is cached and rebuilt once per match.
static bool gSeaMask[MAP_H][MAP_W];
static unsigned long long gSeaMaskSeed = ~0ull;

static void rebuildSeaMask() {
    static bool seen[MAP_H][MAP_W];
    for (int y=0; y<MAP_H; y++) for (int x=0; x<MAP_W; x++) { gSeaMask[y][x]=false; seen[y][x]=false; }
    const int   SEA_MIN_AREA = 500;   // tiles
    const int   SEA_MIN_SPAN = 28;    // must be wide in BOTH dims (excludes rivers)
    static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    std::vector<std::pair<int,int>> stack, body;
    for (int sy=0; sy<MAP_H; sy++) for (int sx=0; sx<MAP_W; sx++) {
        if (seen[sy][sx]) continue;
        if (g.map[sy][sx].terrain != T_WATER) { seen[sy][sx]=true; continue; }
        body.clear(); stack.clear(); stack.push_back({sx,sy}); seen[sy][sx]=true;
        int minx=sx, maxx=sx, miny=sy, maxy=sy;
        while (!stack.empty()) {
            auto [cx,cy] = stack.back(); stack.pop_back(); body.push_back({cx,cy});
            minx=std::min(minx,cx); maxx=std::max(maxx,cx); miny=std::min(miny,cy); maxy=std::max(maxy,cy);
            for (auto& d : d4) { int nx=cx+d[0], ny=cy+d[1];
                if (inBounds(nx,ny) && !seen[ny][nx] && g.map[ny][nx].terrain==T_WATER) { seen[ny][nx]=true; stack.push_back({nx,ny}); } }
        }
        if ((int)body.size() >= SEA_MIN_AREA && (maxx-minx) >= SEA_MIN_SPAN && (maxy-miny) >= SEA_MIN_SPAN)
            for (auto& p : body) gSeaMask[p.second][p.first] = true;
    }
}
static inline bool isSeaTile(int x, int y) {
    if (gSeaMaskSeed != g.simSeed) { rebuildSeaMask(); gSeaMaskSeed = g.simSeed; }
    return inBounds(x,y) && gSeaMask[y][x];
}
static inline bool bordersSea(int x, int y) {
    return isSeaTile(x+1,y) || isSeaTile(x-1,y) || isSeaTile(x,y+1) || isSeaTile(x,y-1);
}

void getTerrainVisual(Terrain t, int x, int y, char& ch, int& cp, int lit) {
    Season season = getSeason();
    float sprog   = getSeasonProgress();
    float bright  = getBrightness();
    // Torchlight from a nearby building holds the night back on this tile
    // (lit 1 = glow fringe, 2 = amber core — tinted at the end of this fn).
    bool  night   = (bright < 0.3f) && lit == 0;
    Biome biome   = g.map[y][x].biome;

    switch (t) {
    case T_GRASS:        ch='.'; cp=CP_GRASS;       break;
    case T_TALL_GRASS:   ch='"'; cp=CP_TALL_GRASS;  break;
    case T_HEATH:        ch=':'; cp=CP_HEATH;       break;
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
        if (isSeaTile(x, y)) {
            // Open sea: a slow tidal swell — long wavelength, one soft crest
            // band drifting through. Calmer than the choppy lake/river shimmer.
            int phase = ((g.tick/16) + (x + y)/10) % 10;
            ch = (phase==4 || phase==5) ? '-' : '~';
            cp = (phase==5) ? CP_WATER_SHIMMER : CP_WATER;
        } else {
            int frame = (g.tick/6 + x + y) % 6;
            const char wc[] = {'~','~','-','~','~','-'};
            ch = wc[frame];
            cp = (frame==2||frame==5) ? CP_WATER_SHIMMER : CP_WATER;
        }
    }
    if (t == T_SHALLOWS) {
        if (bordersSea(x, y)) {
            // Tide lapping the shore: a gentle, slow wash in and out.
            int f = (g.tick/14 + x + y) % 8;
            ch = (f < 1) ? '-' : '~';
            if (f == 0) cp = CP_WATER_SHIMMER;
        } else {
            int f=(g.tick/8+x*3)%4; const char sc[]={'~','-','~','-'}; ch=sc[f];
        }
    }
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

    // The harvest reads as HARVEST at a glance: wheat keeps its gold through
    // every season and every hour (summer's brighter shade stays).
    if (t == T_WHEAT && cp != CP_WHEAT_GOLD) { ch = '%'; cp = CP_WHEAT; }

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
            ||cp==CP_CASTLE_FLOOR||cp==CP_RUINS
            ||cp==CP_HILLS||cp==CP_STONE||cp==CP_HEATH)
            cp = CP_NIGHT_GROUND;
        // Gold stays gold after dark — coin and corn both.
        if (cp==CP_GOLD||cp==CP_GOLD_SHIMMER||cp==CP_WHEAT||cp==CP_WHEAT_GOLD) cp = CP_NIGHT_GOLD;
        // Snow tiles darken but stay distinctly lighter than bare ground at night.
        if (cp==CP_WIN_GROUND||cp==CP_SNOW_GROUND) cp = CP_NIGHT_SNOW;
    }
    // Twilight: the same families cool into dusk's blue hour or warm out of
    // dawn's rose-grey (see the pair definitions). Gold and wheat keep their
    // day colours — that's the golden hour doing what it does, and the
    // harvest stays readable at a glance. Lit tiles skip the tint; the
    // candle pass below owns them.
    else if (lit == 0 && (isDusk() || isDawn())) {
        bool dawn = isDawn();
        if (cp==CP_GRASS||cp==CP_GRASS_LIGHT||cp==CP_GRASS_DRY||cp==CP_TALL_GRASS||cp==CP_MEADOW
            ||cp==CP_AUT_GRASS||cp==CP_AUT_GRASS_LATE
            ||cp==CP_SPRING_FLOWER
            ||cp==CP_FLOWERS||cp==CP_FLOWERS_BLUE||cp==CP_FLOWERS_YELLOW||cp==CP_FLOWERS_RED
            ||cp==CP_BERRY||cp==CP_MARSH||cp==CP_REEDS)
            cp = dawn ? CP_DAWN_GRASS : CP_DUSK_GRASS;
        if (cp==CP_FOREST||cp==CP_FOREST_DARK||cp==CP_PINE||cp==CP_PALM||cp==CP_DEAD_TREE
            ||cp==CP_AUT_TREE_EARLY||cp==CP_AUT_TREE_MID||cp==CP_AUT_TREE_LATE
            ||cp==CP_AUT_TREE_GOLD||cp==CP_AUT_TREE_RED
            ||cp==CP_WIN_TREE||cp==CP_WIN_PINE)
            cp = dawn ? CP_DAWN_TREE : CP_DUSK_TREE;
        // Water is the sky's mirror: only the wave crests take the tint.
        if (cp==CP_WATER_SHIMMER) cp = dawn ? CP_DAWN_SHIMMER : CP_DUSK_SHIMMER;
        if (cp==CP_SAND||cp==CP_DUNES||cp==CP_DIRT||cp==CP_ROAD||cp==CP_GRAVEL
            ||cp==CP_CASTLE_FLOOR||cp==CP_RUINS
            ||cp==CP_HILLS||cp==CP_STONE||cp==CP_HEATH)
            cp = dawn ? CP_DAWN_GROUND : CP_DUSK_GROUND;
        if (cp==CP_WIN_GROUND||cp==CP_SNOW_GROUND) cp = dawn ? CP_DAWN_SNOW : CP_DUSK_SNOW;
    }

    // Candlelight: after dusk the ground around a doorway bathes warm and
    // STEADY — a bright pool at the threshold, a quiet ember wash beyond.
    // (The old fringe "guttered" tiles at random, which read as flicker
    // noise rather than living light.) Water won't take it.
    if (lit > 0 && bright < 0.45f
        && cp != CP_NIGHT_WATER && cp != CP_WATER && cp != CP_WATER_SHIMMER
        && cp != CP_SHALLOWS && cp != CP_LAVA && cp != CP_LAVA_HOT) {
        cp = (lit >= 2) ? CP_TORCHLIT : CP_TORCHLIT_DIM;
    }
}

// ============================================================
// MAP RENDER
// ============================================================
// Render-only overlays drawn on top of the map: corpses, hearth smoke,
// weather, birds, the drag-selection box, and off-screen battle arrows.
static void drawMapOverlays(int tileW) {
    // Rally flag: while one of your production buildings is selected, its
    // rally point shows as a bold '>' so a set rally is never invisible.
    if (g.selectedIds.empty()) {
        Entity* rsel = findEntity(g.selectedId);
        if (rsel && rsel->alive && rsel->owner == g.localPlayer
            && isBuilding(rsel->type) && rsel->rallySet) {
            int sx = (rsel->rallyX - g.viewX) * tileW, sy = rsel->rallyY - g.viewY + 2;
            if (rsel->rallyX >= g.viewX && rsel->rallyX < g.viewX + g.viewW
                && rsel->rallyY >= g.viewY && rsel->rallyY < g.viewY + g.viewH) {
                attron(COLOR_PAIR(CP_GOLD)|A_BOLD);
                mvaddch(sy, sx, '>');
                attroff(COLOR_PAIR(CP_GOLD)|A_BOLD);
            }
        }
    }

    // The fallen linger on the field a while — a dim '%' on the death tile,
    // fading after ~200 ticks (g.corpses; render-only, never sim state).
    attron(COLOR_PAIR(CP_CORPSE)|A_DIM);
    for (auto& c : g.corpses) {
        if (g.tick - c.tick > 200) continue;
        if (!inBounds(c.x, c.y) || !g.map[c.y][c.x].visible[g.localPlayer]) continue;
        int sx = c.x - g.viewX, sy = c.y - g.viewY;
        if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) continue;
        if (entityAt(c.x, c.y)) continue;   // don't paint under the living
        mvaddch(sy+2, sx * tileW, '%');
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
                if (!inBounds(smx, smy) || !g.map[smy][smx].visible[g.localPlayer]) continue;
                int sx = smx - g.viewX, sy = smy - g.viewY;
                if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) continue;
                if (entityAt(smx, smy)) continue;
                char puff = (h == 0)
                    ? ((phase % 2) ? '~' : '\'')
                    : ((phase % 2) ? '.'  : '`');
                mvaddch(sy+2, sx * tileW, puff);
            }
        }
        attroff(COLOR_PAIR(CP_UI_DIM));
    }

    // Snowflakes: drawn every tick; hash seed changes every 12 ticks (~1 second)
    // so each flake stays visible for roughly a second before positions reshuffle.
    if (g.weather == W_SNOW) {
        int frame = g.tick / 12;
        for (int sy = 0; sy < g.viewH; sy++) for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[g.localPlayer]) continue;
            if (entityAt(mx, my)) continue;
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(frame*83492791u));
            if ((int)(h % 100) >= 1) continue;
            attron(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
            mvaddch(sy+2, sx * tileW, '*');
            attroff(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
        }
    }

    // Rain / storm: fast flicker kept on a short interval for patter effect.
    if ((g.weather == W_RAIN || g.weather == W_STORM) && (g.tick % 4) == 0) {
        int density = (g.weather == W_STORM) ? 2 : 1;
        for (int sy = 0; sy < g.viewH; sy++) for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx, my = g.viewY + sy;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[g.localPlayer]) continue;
            if (entityAt(mx, my)) continue;
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(g.tick*83492791u));
            if ((int)(h % 100) >= density) continue;
            attron(COLOR_PAIR(CP_RAIN)|A_BOLD);
            mvaddch(sy+2, sx * tileW, '.');
            attroff(COLOR_PAIR(CP_RAIN)|A_BOLD);
        }
    }

    // Ambient birds: a small flock drifts east across the sky. Render-only
    // flavour — never drawn over units or fog.
    {
        const int FLOCK = 5;
        for (int b = 0; b < FLOCK; b++) {
            int mx = (g.tick/5 + b*53) % (MAP_W + 24) - 12;       // drift + wrap
            int my = 5 + (b*MAP_H)/FLOCK + ((g.tick/40 + b) % 5) - 2;
            if (!inBounds(mx,my) || !g.map[my][mx].visible[g.localPlayer] || entityAt(mx,my)) continue;
            int sx = mx - g.viewX, sy = my - g.viewY;
            if (sx < 0 || sy < 0 || sx >= g.viewW || sy >= g.viewH) continue;
            bool flap = ((g.tick/3 + b) & 1);
            attron(COLOR_PAIR(CP_UI_DIM));
            mvaddch(sy+2, sx*tileW, flap ? 'v' : '^');
            attroff(COLOR_PAIR(CP_UI_DIM));
        }
    }

    // Hearth smoke by day — life around settled homes. Night ambience is the
    // steady candlelit doorways painted by the light mask; the old blinking
    // torch sparks above rooftops are gone (they read as flickering icons).
    if (getBrightness() >= 0.3f) {
        for (auto& e : g.entities) {
            if (!e.alive || !isBuilding(e.type) || e.underConstruction) continue;
            bool hearth = (e.type==E_HOUSE || e.type==E_TOWNHALL || e.type==E_MANOR ||
                           e.type==E_TAVERN || e.type==E_BLACKSMITH);
            if (!hearth || !inBounds(e.x,e.y) || !g.map[e.y][e.x].visible[g.localPlayer]) continue;
            int sx = e.x - g.viewX, sy = e.y - g.viewY;
            if (sx < 0 || sy < 1 || sx >= g.viewW || sy >= g.viewH) continue;  // need a row above
            int ph = (g.tick/6 + e.id*3);
            if ((ph % 4) < 3) {                                 // intermittent puffs
                int puffX = sx + (((ph/8) % 3) - 1);            // drift with the wind
                int puffY = sy - 1 - ((ph/4) % 2);             // rise one-two rows
                int pmx = g.viewX + puffX, pmy = g.viewY + puffY;
                if (puffX >= 0 && puffY >= 0 && puffX < g.viewW &&
                    inBounds(pmx,pmy) && !entityAt(pmx,pmy)) {
                    attron(COLOR_PAIR(CP_UI_DIM));
                    mvaddch(puffY+2, puffX*tileW, ((ph/4)&1)?'%':'*');
                    attroff(COLOR_PAIR(CP_UI_DIM));
                }
            }
        }
    }

    // Drag-selection box: screen-space overlay drawn on top of everything,
    // like AoE/StarCraft — visible over fog, units, weather, the lot.
    // (It used to be a per-tile branch, which the fog early-out skipped,
    // so boxes dragged across unexplored ground were invisible.)
    if (g.dragging && g.mode != M_WALL_DRAG) {
        int bx0 = std::min(g.dragStartX, g.cursorX), bx1 = std::max(g.dragStartX, g.cursorX);
        int by0 = std::min(g.dragStartY, g.cursorY), by1 = std::max(g.dragStartY, g.cursorY);
        // Clamp to the visible map viewport (screen-cell coords).
        int sx0 = std::max(0, bx0 - g.viewX), sx1 = std::min(g.viewW - 1, bx1 - g.viewX);
        int sy0 = std::max(0, by0 - g.viewY), sy1 = std::min(g.viewH - 1, by1 - g.viewY);
        if (sx0 <= sx1 && sy0 <= sy1) {
#ifdef USE_SDL_SHIM
            // GUI: a soft translucent gold marquee with a crisp thin border —
            // the modern RTS look, instead of a chunky one-cell block ring.
            shimOverlayRect(sx0 * tileW, sy0 + 2, sx1 * tileW + (tileW - 1), sy1 + 2,
                            255, 205, 60, 46, 230);
#else
            // Terminal: a thin gold line outline (box-drawing glyphs) rather
            // than solid reversed blocks — reads as a slim marquee.
            attron(COLOR_PAIR(CP_SUN) | A_BOLD);
            auto put = [&](int sx, int sy, chtype acs, char ascii) {
                if (sx < 0 || sx >= g.viewW || sy < 0 || sy >= g.viewH) return;
                (void)acs;
                mvaddch(sy + 2, sx * tileW, (chtype)ascii);
            };
            for (int sx = sx0; sx <= sx1; sx++) { put(sx, sy0, ACS_HLINE, '-'); put(sx, sy1, ACS_HLINE, '-'); }
            for (int sy = sy0; sy <= sy1; sy++) { put(sx0, sy, ACS_VLINE, '|'); put(sx1, sy, ACS_VLINE, '|'); }
            put(sx0, sy0, ACS_ULCORNER, '+'); put(sx1, sy0, ACS_URCORNER, '+');
            put(sx0, sy1, ACS_LLCORNER, '+'); put(sx1, sy1, ACS_LRCORNER, '+');
            attroff(COLOR_PAIR(CP_SUN) | A_BOLD);
#endif
        }
    }

    // Off-screen alert arrows: a blinking edge marker pointing toward any of the
    // player's units/buildings that are fighting or routing out of view — so a
    // battle you can't see still announces itself. (Render-only.)
    if ((g.tick % 12) < 9) {
        int vx0 = g.viewX, vy0 = g.viewY, vx1 = g.viewX + g.viewW - 1, vy1 = g.viewY + g.viewH - 1;
        int edgeX[16], edgeY[16], nd = 0, drawn = 0;
        attron(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
        for (auto& e : g.entities) {
            if (drawn >= 8) break;
            if (!e.alive || e.owner != g.localPlayer) continue;
            if (!(e.alertTicks > 0 || e.state == S_ROUTING)) continue;     // only trouble
            if (e.x >= vx0 && e.x <= vx1 && e.y >= vy0 && e.y <= vy1) continue;  // on-screen
            int cxm = std::max(vx0, std::min(e.x, vx1));
            int cym = std::max(vy0, std::min(e.y, vy1));
            int sx = cxm - g.viewX, sy = cym - g.viewY;
            char arrow = (e.y < vy0) ? '^' : (e.y > vy1) ? 'v' : (e.x < vx0) ? '<' : '>';
            bool dup = false;
            for (int i = 0; i < nd; i++) if (edgeX[i]==sx && edgeY[i]==sy) { dup = true; break; }
            if (dup) continue;
            if (nd < 16) { edgeX[nd]=sx; edgeY[nd]=sy; nd++; }
            if (sx >= 0 && sx < g.viewW && sy >= 0 && sy < g.viewH) {
                mvaddch(sy + 2, sx * tileW, (chtype)arrow);
                drawn++;
            }
        }
        attroff(COLOR_PAIR(CP_UI_HIGH) | A_BOLD);
    }
}

static unsigned char litMask[MAP_H][MAP_W];   // 0 dark, 1 glow fringe, 2 torch core
static unsigned char wallGrid[MAP_H][MAP_W];  // wall/gate/tower occupancy (connectivity)
static unsigned char bldPrev[MAP_H][MAP_W];
static bool          wallPrev[MAP_H][MAP_W];

// Per-frame precompute: the night torch-light mask, the selected ranged
// unit/tower range ring, and the build / wall-drag placement previews.
// Fills the file-static masks the map loop reads below.
static void rmPreparePass(int& ringX, int& ringY, int& ringR) {
    // Candlelight: from dusk on, every standing building spills a warm,
    // STEADY pool from its open doorway — bright by the threshold, an ember
    // wash further out. The doorway sits centre of the south face (where the
    // architecture pass draws the '+' door), so big halls pool their light in
    // front of the door rather than glowing evenly all round. Walls and gates
    // stay dark — no doorway, no candle. All owners glow (a world effect):
    // villages become constellations after dark.
    memset(litMask, 0, sizeof(litMask));
    memset(wallGrid, 0, sizeof(wallGrid));
    bool lamps = (getBrightness() < 0.45f);   // candles are lit from dusk
    for (auto& b : g.entities) {
        if (!b.alive || !isBuilding(b.type)) continue;
        int bx2 = b.x + STATS[b.type].sizeW - 1, by2 = b.y + STATS[b.type].sizeH - 1;
        // Connectivity grid for the wall renderer (built + building alike).
        if (b.type == E_WALL || b.type == E_GATE || b.type == E_TOWER)
            for (int yy = b.y; yy <= by2; yy++) for (int xx = b.x; xx <= bx2; xx++)
                if (inBounds(xx, yy)) wallGrid[yy][xx] = 1;
        if (!lamps || b.underConstruction) continue;
        if (b.type == E_WALL || b.type == E_GATE) continue;
        // Great halls have wide doors and many candles; cottages keep a
        // close, homely pool.
        float coreR, fringeR;
        switch (b.type) {
            case E_TOWNHALL: case E_CASTLE:
            case E_CHURCH:   case E_TAVERN:  coreR = 2.0f; fringeR = 4.0f; break;
            default:                         coreR = 1.4f; fringeR = 2.8f; break;
        }
        int doorX = (b.x + bx2) / 2, doorY = by2;   // the '+' on the south face
        int R = (int)fringeR + 1;
        for (int yy = b.y - R; yy <= by2 + R; yy++) for (int xx = b.x - R; xx <= bx2 + R; xx++) {
            if (!inBounds(xx, yy)) continue;
            // Fringe falls off from the footprint rectangle (the whole house
            // is warm); the bright core spills from the door tile itself.
            int cx = std::max(b.x, std::min(xx, bx2));
            int cy = std::max(b.y, std::min(yy, by2));
            float dRect2 = (float)((xx-cx)*(xx-cx) + (yy-cy)*(yy-cy));
            float dDoor2 = (float)((xx-doorX)*(xx-doorX) + (yy-doorY)*(yy-doorY));
            unsigned char lv = 0;
            if      (dDoor2 <= coreR*coreR)     lv = 2;
            else if (dRect2 <= fringeR*fringeR) lv = 1;
            if (lv > litMask[yy][xx]) litMask[yy][xx] = lv;
        }
    }

    // Selected ranged unit/tower: precompute range-ring centre + radius.
    ringX = -1; ringY = -1; ringR = 0;
    Entity* selR = findEntity(g.selectedId);
    if (selR && selR->alive && selR->owner == g.localPlayer) {
        int rng = STATS[selR->type].range;
        if (selR->type == E_ARCHER && (g.players[g.localPlayer].research & R_CROSSBOWS)) rng += 2;
        if (rng > 1) {
            auto& ss = STATS[selR->type];
            ringX = selR->x + ss.sizeW/2; ringY = selR->y + ss.sizeH/2; ringR = rng;
        }
    }

    // Precompute building footprint preview for M_BUILD_PLACE.
    // bldPrev[y][x] = 0 not in footprint, 1 valid, 2 blocked.
    memset(bldPrev, 0, sizeof(bldPrev));
    if (g.mode == M_BUILD_PLACE && g.buildPending != E_NONE) {
        Entity* sel = findEntity(g.selectedId);
        int ignoreId = (sel && sel->alive) ? sel->id : -1;
        bool ok = canPlace(g.buildPending, g.cursorX, g.cursorY, g.localPlayer, ignoreId);
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
}

void renderMap() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    int panelW = 24;
    int tileW = 1;
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

    int ringX, ringY, ringR;
    rmPreparePass(ringX, ringY, ringR);

    for (int sy = 0; sy < g.viewH; sy++) { int my = g.viewY + sy;
        for (int sx = 0; sx < g.viewW; sx++) { int mx = g.viewX + sx;
            int scY = sy+2, scX = sx * tileW;
            auto clearTile = [&](int y, int x) { mvaddch(y, x, ' '); };
            if (!inBounds(mx, my)) { clearTile(scY, scX); continue; }
            Tile& tile = g.map[my][mx];
            bool vis = tile.visible[g.localPlayer], expl = tile.explored[g.localPlayer];
            bool isCur = (mx == g.cursorX && my == g.cursorY);

            if (!expl) {
                if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); clearTile(scY, scX); attroff(COLOR_PAIR(CP_CURSOR)); }
                else { clearTile(scY, scX); }
                continue;
            }

            char ch; int cp;
            getTerrainVisual(tile.terrain, mx, my, ch, cp, litMask[my][mx]);
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
            if (!vis) {
                if (isCur) {
                    attron(COLOR_PAIR(CP_CURSOR));
                    mvaddch(scY, scX, ch);
                    attroff(COLOR_PAIR(CP_CURSOR));
                } else {
                    attron(COLOR_PAIR(CP_FOG_EXPLORED));
                    mvaddch(scY, scX, ch);
                    attroff(COLOR_PAIR(CP_FOG_EXPLORED));
                }
                continue;
            }

            // Wall drag preview overrides terrain.
            if (wallPrev[my][mx]) { ch = '#'; cp = CP_PLAYER; }

            // Use a chtype-wide draw glyph so completed walls can use the ACS solid block.
            chtype drawCh = (chtype)ch;
            // Roads read as cobbles (walkable texture), never as fences.
            if (tile.terrain == T_ROAD) drawCh = ACS_BULLET;
            // Ruined castle masonry joins the wall-line language: straight
            // stretches are wall lines, corners and breaks are stone blocks.
            if (tile.terrain == T_CASTLE_WALL) {
                auto tw = [&](int xx, int yy){ return inBounds(xx,yy)
                    && (g.map[yy][xx].terrain==T_CASTLE_WALL || g.map[yy][xx].terrain==T_CASTLE_GATE
                        || wallGrid[yy][xx]); };
                bool n = tw(mx, my-1), s2 = tw(mx, my+1);
                bool w = tw(mx-1, my), e2 = tw(mx+1, my);
                bool horiz = (w || e2) && !(n || s2);
                bool vert  = (n || s2) && !(w || e2);
                drawCh = horiz ? ACS_HLINE : vert ? ACS_VLINE : ACS_CKBOARD;
            }
            if (wallPrev[my][mx]) drawCh = ACS_CKBOARD;

            Entity* ent = entityAt(mx, my);
            // Fields are walkable: a unit standing in the crops draws over
            // the wheat (entityAt returns the lowest id, often the farm).
            bool onField = ent && ent->type == E_FARM && !ent->underConstruction;
            if (onField) {
                for (auto& u : g.entities) {
                    if (!u.alive || isBuilding(u.type) || u.state == S_GARRISONED) continue;
                    if (u.x == mx && u.y == my) { ent = &u; break; }
                }
            }
            // Cloaking: enemy units fade at night/storm unless a friendly eye is close.
            // Wheat fields also conceal enemies — units in crops need close detection.
            bool inCrop = ent && !isBuilding(ent->type) && (tile.terrain == T_WHEAT || onField);
            if (ent && ent->alive && ent->owner != g.localPlayer && ent->owner < MAX_PLAYERS
                && (isConcealing() || inCrop) && !isDetectedBy(mx, my, g.localPlayer)) ent = nullptr;

            // Body tile: catapult/ram/deployed-trebuchet extend one cell right.
            if (!ent && inBounds(mx-1, my)) {
                Entity* leftEnt = entityAt(mx-1, my);
                bool isTwoTile = leftEnt && leftEnt->alive && !leftEnt->underConstruction &&
                    (leftEnt->type == E_CATAPULT || leftEnt->type == E_RAM ||
                     leftEnt->type == E_TREBUCHET);   // treb is two tiles packed OR deployed
                if (isTwoTile) {
                    bool inCropLeft = !isBuilding(leftEnt->type) && g.map[my][mx-1].terrain == T_WHEAT;
                    bool leftCloaked = leftEnt->owner != g.localPlayer && leftEnt->owner < MAX_PLAYERS
                                    && (isConcealing() || inCropLeft) && !isDetectedBy(mx-1, my, g.localPlayer);
                    if (!leftCloaked) {
                        bool bodyIsSel = (leftEnt->id == g.selectedId);
                        if (!bodyIsSel) for (int sid : g.selectedIds) if (sid == leftEnt->id) { bodyIsSel = true; break; }
                        int bcp = ownerColorPair(leftEnt->owner, night && !litMask[my][mx]);
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
            if (ent && ent->alive) {
                ch = STATS[ent->type].glyph;
                // Uppercase glyph signals a stack of 2+ military on the tile.
                if (stackedMil >= 2 && ch >= 'a' && ch <= 'z')
                    ch = ch - 'a' + 'A';
                drawCh = (chtype)ch;

                // Masonry pass — the castle's own language, extended only to
                // the big civic buildings. The roofs experiment read as
                // cartoons; what worked about the castle was TEXTURE:
                // checkerboard stone corners, plain edges, one letter.
                // 2x2 buildings stay as solid letter blocks (they were fine).
                if (isBuilding(ent->type) && !ent->underConstruction
                    && STATS[ent->type].sizeW >= 3 && ent->type != E_CASTLE
                    && ent->type != E_STOCKYARD) {
                    int bdx = mx - ent->x, bdy = my - ent->y;
                    int bw = STATS[ent->type].sizeW, bh = STATS[ent->type].sizeH;
                    bool corner = (bdx == 0 || bdx == bw-1) && (bdy == 0 || bdy == bh-1);
                    if (corner)                      drawCh = ACS_CKBOARD;
                    else if (bdx == 0 || bdx == bw-1) { ch = '|'; drawCh = (chtype)ch; }
                    else if (bdy == 0)                { ch = '='; drawCh = (chtype)ch; }
                    else if (bdy == bh-1 && bh >= 3)  { ch = '+'; drawCh = (chtype)ch; }  // door
                    else { ch = STATS[ent->type].glyph; drawCh = (chtype)ch; }
                }

                // Colour pair: player-owned units/buildings use owner colour
                // backgrounds; Gaia animals keep their type-specific colours.
                // (Restored: a bad edit dropped this block, and units took
                // whatever colour the ground under them had.)
                if (ent->owner == OWNER_NATURE) {
                    if      (ent->type == E_WOLF)  cp = CP_WOLF;
                    else if (ent->type == E_SHEEP) cp = CP_SHEEP;
                    else if (ent->type == E_BOAR)  cp = CP_BOAR;
                    else                           cp = CP_DEER;
                } else {
                    cp = ownerColorPair(ent->owner, night && litMask[my][mx] == 0);
                }
                // All boats get a wood-brown deck; glyph colour is per-player
                // so each side's fleet is identifiable.
                if (isNaval(ent->type) && ent->owner < MAX_PLAYERS) {
                    cp = CP_SHIP_P0 + ent->owner;   // CP_SHIP_P0..P7 contiguous
                }
                // Farms are always wheat-gold — ownership doesn't change their
                // colour, and neither does being half-sown (a growing field in
                // the owner's team colour read as "green corn").
                // Corn is corn: a farm is always the '%' cornfield glyph, in
                // every season and from the moment it's sown — no growth
                // stages, no tilled-row or stubble variants, matching wild corn.
                if (ent->type == E_FARM) {
                    cp = (getSeason() == SUMMER) ? CP_WHEAT_GOLD : CP_WHEAT;
                    ch = '%';
                    drawCh = (chtype)ch;
                }

                // State-specific glyph overrides (gate, construction, siege engines, alert).
                // Fortifications read as LINES: straight curtain-wall runs are
                // wall lines; corners, junctions and lone stubs are bastion
                // blocks. Gates take the run's orientation — closed bars the
                // line, open leaves a passage dot.
                if ((ent->type == E_WALL || ent->type == E_GATE) && !ent->underConstruction) {
                    auto tw = [&](int xx, int yy){ return inBounds(xx,yy)
                        && (wallGrid[yy][xx]
                            || g.map[yy][xx].terrain==T_CASTLE_WALL
                            || g.map[yy][xx].terrain==T_CASTLE_GATE); };
                    bool n = tw(mx, my-1), s2 = tw(mx, my+1);
                    bool w = tw(mx-1, my), e2 = tw(mx+1, my);
                    bool horiz = (w || e2) && !(n || s2);
                    bool vert  = (n || s2) && !(w || e2);
                    if (ent->type == E_GATE)
                        drawCh = ent->gateOpen ? ACS_BULLET : (vert ? ACS_VLINE : ACS_HLINE);
                    else
                        drawCh = horiz ? ACS_HLINE : vert ? ACS_VLINE : ACS_CKBOARD;
                }
                if (ent->underConstruction && ent->type != E_FARM && g.tick%10 < 5) {
                    ch = '#'; drawCh = (chtype)ch;
                }

                // Keep: 3×3 per-cell pattern — solid corners, edged walls,
                // the lord's hall at the centre.
                if (ent->type == E_CASTLE && !ent->underConstruction) {
                    int dx = mx - ent->x, dy = my - ent->y;
                    bool corner = (dx == 0 || dx == 2) && (dy == 0 || dy == 2);
                    if (corner)                  drawCh = ACS_CKBOARD;
                    else if (dy == 0 || dy == 2) { ch = '='; drawCh = (chtype)ch; }
                    else if (dx == 0 || dx == 2) { ch = '|'; drawCh = (chtype)ch; }
                    else                         { ch = 'W'; drawCh = (chtype)ch; }
                }
                // Stockyard: the hoard is VISIBLE, tile by tile — top row
                // gold, middle wood, bottom food. Each tile is one pile of
                // up to 100; its glyph fades in as the pile grows, so a fat
                // yard reads as treasure from across the map (and to your
                // enemies: these piles are what CMD_RAID steals from).
                if (ent->type == E_STOCKYARD && !ent->underConstruction) {
                    int dx = mx - ent->x, dy = my - ent->y;
                    int amt, pcp;
                    char full, half;
                    if (dy == 0)      { amt = ent->storeGold; full = '$'; half = '$'; pcp = CP_GOLD; }
                    else if (dy == 1) { amt = ent->storeWood; full = '='; half = '-'; pcp = CP_DEAD_TREE; }
                    else              { amt = depotFoodSum(*ent); full = '%'; half = '"'; pcp = CP_WHEAT; }
                    int pile = std::max(0, std::min(100, amt - dx * 100));
                    if      (pile == 0)  { ch = '.'; drawCh = (chtype)ch; }
                    else if (pile < 50)  { ch = half; drawCh = (chtype)ch; cp = pcp; }
                    else                 { ch = full; drawCh = (chtype)ch | A_BOLD; cp = pcp; }
                }

                // Siege engine arm animations.
                if (ent->type == E_CATAPULT) {
                    // Arm shows as raised only for the first 3 ticks after firing —
                    // a brief thump, then rests horizontal until the next shot.
                    bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_CATAPULT].atkSpeed - 3;
                    ch = firing ? '/' : '-'; drawCh = (chtype)ch;
                }
                if (ent->type == E_RAM) {
                    bool ramming = ent->state==S_ATTACKING && ent->atkCd > STATS[E_RAM].atkSpeed*2/3;
                    ch = ramming ? '=' : '-'; drawCh = (chtype)ch;
                }
                if (ent->type == E_TREBUCHET) {
                    if (ent->packTicks > 0)      { ch = ent->packed ? 'q' : 'Q'; }  // transition
                    else if (ent->packed == 1)   { ch = 'q'; }                       // packed wagon
                    else {
                        // Deployed: arm rests as 'L', flips to '/' for 5 ticks after a shot.
                        bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_TREBUCHET].atkSpeed - 5;
                        ch = firing ? '/' : 'L';
                    }
                    drawCh = (chtype)ch;
                }
                // Peasant work/idle cycle. Staggered per-id so a busy village
                // doesn't strobe in sync.
                if (ent->type == E_PEASANT) {
                    int cyc = (g.tick + ent->id*5) % 30;
                    // ONE flashing icon for harvesting (gathering or hauling the
                    // load home count as the same job) and a DISTINCT one for
                    // building — so a busy resource node reads as a single clear
                    // pulse instead of a clutter of mixed symbols.
                    if      ((ent->state == S_GATHERING || ent->state == S_RETURNING) && cyc < 3) { ch = '*'; drawCh = (chtype)ch; }
                    else if (ent->state == S_BUILDING  && cyc < 3) { ch = '+'; drawCh = (chtype)ch; }
                    else if (ent->state == S_IDLE) {
                        // Slow daydream pulse: '?' shown ~1 s every ~20 s, staggered.
                        int slow = (g.tick + ent->id*47) % 250;
                        if (slow < 12) { ch = '?'; drawCh = (chtype)ch; }
                    }
                }
                // Recently in combat: gentle '!' pulse — ~1.5 Hz, not strobing.
                if (ent->alertTicks > 0 && (g.tick % 8) < 4) {
                    ch = '!'; drawCh = (chtype)ch;
                }
                // Broken men flee under a blinking '?'; captives are marked '"'.
                if (ent->state == S_ROUTING) {
                    if ((g.tick % 6) < 3) { ch = '?'; drawCh = (chtype)ch; }
                } else if (ent->prisoner) {
                    ch = '"'; drawCh = (chtype)ch;
                }
            }
            // Projectile overwrites terrain/entity glyph.
            for (auto& p : g.projectiles) {
                if (!p.alive) continue;
                if ((int)roundf(p.x)==mx && (int)roundf(p.y)==my) {
                    ch = p.glyph; cp = p.color; drawCh = (chtype)ch;
                }
            }

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

            auto drawAt = [&](int y, int x, chtype dch) { mvaddch(y, x, dch); };

            if (bldPrev[my][mx]) {
                // Footprint overlay wins over cursor so the build outline reads
                // cleanly. Glyph carries the verdict too ('+' fits / 'x' blocked)
                // so the preview reads without red-green colour vision.
                int cpFP = (bldPrev[my][mx] == 1) ? CP_BUILD_OK : CP_BUILD_BAD;
                attron(COLOR_PAIR(cpFP)|A_BOLD);
                drawAt(scY, scX, (bldPrev[my][mx] == 1) ? '+' : 'x');
                attroff(COLOR_PAIR(cpFP)|A_BOLD);
            } else if (isCur) {
                attron(COLOR_PAIR(CP_CURSOR));
                drawAt(scY, scX, drawCh);
                attroff(COLOR_PAIR(CP_CURSOR));
            } else if (onRangeRing && !ent) {
                // Subtle range-ring marker on empty tiles only.
                attron(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
                drawAt(scY, scX, '.');
                attroff(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
            } else {
                int attr = COLOR_PAIR(cp);
                if (ent && ent->alive) attr |= A_BOLD;
                // Selection highlight: A_REVERSE swaps owner bg ↔ fg so the
                // player/enemy colour becomes the cell foreground — distinct
                // from the ownership background on surrounding tiles.
                if (isSel) attr |= A_REVERSE;
                attron(attr);
                drawAt(scY, scX, drawCh);
                attroff(attr);
            }
        }
    }
    drawMapOverlays(tileW);
}

void render() { erase(); renderMap(); renderUI(); refresh(); }
