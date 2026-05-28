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
    const int BLUE          = 19;
    const int MED_BLUE      = 25;
    const int TEAL          = 30;
    const int BRIGHT_TEAL   = 37;
    const int CYAN          = 44;
    const int DARK_CYAN     = 23;
    const int ICE_BLUE      = 117;

    const int DARK_BROWN    = 52;
    const int BROWN         = 94;
    const int AMBER         = 130;
    const int TAN           = 137;
    const int LIGHT_TAN     = 180;
    const int DARK_GOLD     = 136;
    const int ORANGE        = 172;
    const int DARK_ORANGE   = 166;
    const int BRIGHT_ORANGE = 208;

    const int GOLD          = 178;
    const int BRIGHT_GOLD   = 220;
    const int YELLOW        = 226;
    const int WHEAT_GOLD    = 143;
    const int PALE_YELLOW   = 229;

    const int DARK_RED      = 124;
    const int RED           = 160;
    const int BRIGHT_RED    = 196;
    const int BERRY_RED     = 125;

    const int PURPLE        = 54;
    const int MAUVE         = 96;
    const int LAVENDER      = 140;
    const int DUSK_PURPLE   = 53;

    const int WHITE         = 231;
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
    init_pair(CP_FLOWERS,       C::LAVENDER,     bg);
    init_pair(CP_FLOWERS_BLUE,  C::MED_BLUE,     bg);
    init_pair(CP_MEADOW,        C::PALE_GREEN,   bg);

    init_pair(CP_FOREST,        C::MED_GREEN,    bg);
    init_pair(CP_FOREST_DARK,   C::DARK_GREEN,   bg);
    init_pair(CP_PINE,          C::PINE_GREEN,   bg);
    init_pair(CP_PALM,          C::GREEN,        bg);
    init_pair(CP_DEAD_TREE,     C::GRAY,         bg);

    init_pair(CP_MOUNTAIN,      C::LIGHT_GRAY,   bg);
    init_pair(CP_HILLS,         C::OLIVE,        bg);
    init_pair(CP_STONE,         C::MED_GRAY,     bg);

    init_pair(CP_WATER,         C::MED_BLUE,     C::DEEP_BLUE);
    init_pair(CP_WATER_SHIMMER, C::BRIGHT_TEAL,  C::DEEP_BLUE);
    init_pair(CP_SHALLOWS,      C::TEAL,         bg);
    init_pair(CP_MARSH,         C::SWAMP_GREEN,  bg);
    init_pair(CP_REEDS,         C::DARK_GOLD,    bg);

    init_pair(CP_GOLD,          C::BRIGHT_GOLD,  bg);
    init_pair(CP_GOLD_SHIMMER,  C::GOLD,         bg);

    init_pair(CP_SAND,          C::TAN,          bg);
    init_pair(CP_DUNES,         C::LIGHT_TAN,    bg);
    init_pair(CP_SNOW_GROUND,   C::SNOW_WHITE,   bg);
    init_pair(CP_ICE,           C::ICE_BLUE,     bg);

    init_pair(CP_DIRT,          C::BROWN,        bg);
    init_pair(CP_ROAD,          C::LIGHT_GRAY,   bg);

    init_pair(CP_WHEAT,         C::WHEAT_GOLD,   bg);
    init_pair(CP_WHEAT_GOLD,    C::BRIGHT_GOLD,  bg);
    init_pair(CP_BERRY,         C::BERRY_RED,    bg);

    init_pair(CP_RUINS,         C::GRAY,         bg);
    init_pair(CP_GRAVEL,        C::MED_GRAY,     bg);

    init_pair(CP_CASTLE_WALL,   C::BRIGHT_GRAY,  bg);
    init_pair(CP_CASTLE_FLOOR,  C::DARK_GOLD,    bg);
    init_pair(CP_CASTLE_GATE,   C::AMBER,        bg);

    init_pair(CP_AUT_TREE_EARLY, C::YELLOW_GREEN, bg);
    init_pair(CP_AUT_TREE_MID,   C::ORANGE,       bg);
    init_pair(CP_AUT_TREE_LATE,  C::BROWN,        bg);
    init_pair(CP_AUT_GRASS,      C::OLIVE,        bg);
    init_pair(CP_AUT_GRASS_LATE, C::BROWN,        bg);

    init_pair(CP_WIN_GROUND,     C::SNOW_WHITE,   bg);
    init_pair(CP_WIN_TREE,       C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_PINE,       C::PINE_GREEN,   bg);
    init_pair(CP_WIN_ICE,        C::ICE_BLUE,     C::NAVY);

    init_pair(CP_NIGHT_GRASS,    C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_TREE,     C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_WATER,    C::NAVY,         C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   C::DARKER_GRAY,  bg);
    init_pair(CP_NIGHT_GOLD,     C::DARK_GOLD,    bg);

    init_pair(CP_DAWN_SKY,       C::ORANGE,       bg);
    init_pair(CP_DUSK_SKY,       C::DUSK_PURPLE,  bg);

    init_pair(CP_PLAYER,         C::PLAYER_CYAN,  bg);
    init_pair(CP_PLAYER_NIGHT,   C::PLAYER_DIM,   bg);
    init_pair(CP_ENEMY,          C::ENEMY_RED,    bg);
    init_pair(CP_ENEMY_NIGHT,    C::ENEMY_DIM,    bg);

    init_pair(CP_PROJ_ARROW,     C::BRIGHT_GOLD,  bg);
    init_pair(CP_PROJ_BOULDER,   C::BRIGHT_GRAY,  bg);
    init_pair(CP_PROJ_TOWER,     C::BRIGHT_RED,   bg);

    init_pair(CP_UI_BAR,         C::UI_TEXT,      C::UI_BG);
    init_pair(CP_UI_TEXT,        C::UI_TEXT,      bg);
    init_pair(CP_UI_HIGH,        C::UI_HIGHLIGHT, bg);
    init_pair(CP_UI_DIM,         C::UI_DIM,       bg);
    init_pair(CP_UI_ACCENT,      C::UI_ACCENT,    bg);
    init_pair(CP_FOG,            C::DARKER_GRAY,  bg);
    init_pair(CP_FOG_EXPLORED,   C::DARK_GRAY,    bg);
    init_pair(CP_CURSOR,         C::NEAR_BLACK,   C::SNOW_WHITE);
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

    init_pair(CP_DEER,           C::TAN,          bg);
    init_pair(CP_WOLF,           C::LIGHT_GRAY,   bg);
    init_pair(CP_SHEEP,          C::SNOW_WHITE,   bg);
    init_pair(CP_MM_ANIMAL,      C::TAN,          C::NEAR_BLACK);
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
    case T_FLOWERS:      ch='*'; cp=CP_FLOWERS;     break;
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
    case T_WHEAT:        ch='%'; cp=CP_WHEAT;       break;
    case T_BERRY:        ch='*'; cp=CP_BERRY;       break;
    case T_RUINS:        ch='&'; cp=CP_RUINS;       break;
    case T_GRAVEL:       ch=':'; cp=CP_GRAVEL;      break;
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
                if (shouldShowSeasonAt(x,y,p*0.4f))                    cp = CP_AUT_TREE_EARLY;
                if (p>0.3f && shouldShowSeasonAt(x,y,(p-0.3f)*1.4f))   cp = CP_AUT_TREE_MID;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.5f)) { cp = CP_AUT_TREE_LATE; ch='t'; }
            }
            if (t==T_PINE && p>0.5f && shouldShowSeasonAt(x,y,(p-0.5f)*0.6f)) cp = CP_AUT_TREE_EARLY;
            if (t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW) {
                if (shouldShowSeasonAt(x,y,p*0.5f))                             cp = CP_AUT_GRASS;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.0f)) { cp=CP_AUT_GRASS_LATE; if(t==T_TALL_GRASS)ch=','; }
            }
            if (t==T_FLOWERS && shouldShowSeasonAt(x,y,p*0.7f))  { ch='.'; cp=CP_AUT_GRASS; }
            if (t==T_WHEAT   && shouldShowSeasonAt(x,y,p))        { ch=','; cp=CP_DIRT; }
            break;
        }
        case WINTER: {
            float p = sprog;
            // Snow blankets ground immediately, near-total by mid-winter
            float snowAmt = std::min(1.0f, 0.55f + p * 0.5f);
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

    if (night) {
        if (cp==CP_GRASS||cp==CP_GRASS_LIGHT||cp==CP_GRASS_DRY||cp==CP_TALL_GRASS||cp==CP_MEADOW
            ||cp==CP_AUT_GRASS||cp==CP_AUT_GRASS_LATE)
            cp = CP_NIGHT_GRASS;
        if (cp==CP_FOREST||cp==CP_FOREST_DARK||cp==CP_PINE||cp==CP_PALM||cp==CP_DEAD_TREE
            ||cp==CP_AUT_TREE_EARLY||cp==CP_AUT_TREE_MID||cp==CP_AUT_TREE_LATE||cp==CP_WIN_TREE)
            cp = CP_NIGHT_TREE;
        if (cp==CP_WATER||cp==CP_WATER_SHIMMER||cp==CP_SHALLOWS) cp = CP_NIGHT_WATER;
        if (cp==CP_SAND||cp==CP_DUNES||cp==CP_DIRT||cp==CP_ROAD||cp==CP_GRAVEL
            ||cp==CP_CASTLE_FLOOR||cp==CP_RUINS||cp==CP_WHEAT||cp==CP_WHEAT_GOLD)
            cp = CP_NIGHT_GROUND;
        if (cp==CP_GOLD||cp==CP_GOLD_SHIMMER) cp = CP_NIGHT_GOLD;
        if (cp==CP_WIN_GROUND||cp==CP_SNOW_GROUND) cp = CP_FOG_EXPLORED;
    }
}

// ============================================================
// MAP RENDER
// ============================================================
void renderMap() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    int panelW = 24; g.viewW = maxX - panelW - 1; g.viewH = maxY - 4;
    if (g.viewW < 30) g.viewW = maxX; if (g.viewH < 10) g.viewH = maxY - 2;

    if (g.cursorX < g.viewX+3)            g.viewX = g.cursorX - 3;
    if (g.cursorX > g.viewX+g.viewW-4)    g.viewX = g.cursorX - g.viewW + 4;
    if (g.cursorY < g.viewY+2)            g.viewY = g.cursorY - 2;
    if (g.cursorY > g.viewY+g.viewH-3)    g.viewY = g.cursorY - g.viewH + 3;
    g.viewX = std::max(0, std::min(g.viewX, MAP_W - g.viewW));
    g.viewY = std::max(0, std::min(g.viewY, MAP_H - g.viewH));

    bool night = isNight();

    // Precompute drag-selection box (map coords); -1 means no active box
    int boxX0 = -1, boxY0 = -1, boxX1 = -1, boxY1 = -1;
    if (g.dragging) {
        boxX0 = std::min(g.dragStartX, g.cursorX);
        boxY0 = std::min(g.dragStartY, g.cursorY);
        boxX1 = std::max(g.dragStartX, g.cursorX);
        boxY1 = std::max(g.dragStartY, g.cursorY);
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
                if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, ch); attroff(COLOR_PAIR(CP_CURSOR)); }
                else { attron(COLOR_PAIR(CP_FOG_EXPLORED)); mvaddch(scY, scX, ch); attroff(COLOR_PAIR(CP_FOG_EXPLORED)); }
                continue;
            }

            Entity* ent = entityAt(mx, my);
            if (ent && ent->alive) {
                ch = STATS[ent->type].glyph;
                if      (ent->owner == 0)     cp = night ? CP_PLAYER_NIGHT : CP_PLAYER;
                else if (ent->owner == 1)     cp = night ? CP_ENEMY_NIGHT  : CP_ENEMY;
                else if (ent->type == E_WOLF) cp = CP_WOLF;
                else if (ent->type == E_SHEEP)cp = CP_SHEEP;
                else                          cp = CP_DEER;
                if (ent->underConstruction && g.tick%10 < 5) ch = '#';
            }
            for (auto& p : g.projectiles) {
                if (!p.alive) continue;
                if ((int)roundf(p.x)==mx && (int)roundf(p.y)==my) { ch=p.glyph; cp=p.color; }
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

            bool onBoxBorder = (boxX0 >= 0)
                && mx >= boxX0 && mx <= boxX1 && my >= boxY0 && my <= boxY1
                && (mx == boxX0 || mx == boxX1 || my == boxY0 || my == boxY1);

            if (isCur) {
                attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, ch); attroff(COLOR_PAIR(CP_CURSOR));
            } else {
                int attr = COLOR_PAIR(cp);
                if (ent && ent->alive) attr |= A_BOLD;
                if (isSel)        attr |= A_UNDERLINE;
                if (onBoxBorder)  attr |= A_REVERSE;
                attron(attr); mvaddch(scY, scX, ch); attroff(attr);
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
    mvprintw(0, 9, "Gold:%-5d Wood:%-5d Food:%-5d Pop:%d/%d", p.gold, p.wood, p.food, p.supply, p.supplyMax);

    int iconX = maxX - 22;
    if (getBrightness() > 0.5f) { attron(COLOR_PAIR(CP_SUN)|A_BOLD); mvprintw(0,iconX,"*"); attroff(COLOR_PAIR(CP_SUN)|A_BOLD); }
    else { attron(COLOR_PAIR(CP_MOON)); mvprintw(0,iconX,"o"); attroff(COLOR_PAIR(CP_MOON)); }
    attron(COLOR_PAIR(CP_UI_BAR));
    mvprintw(0, iconX+1, " %-5s %-6s", getTimeName(), getSeasonName());
    attroff(COLOR_PAIR(CP_UI_BAR));

    // Terrain info bar
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(1, 0, '-', g.viewW); attroff(COLOR_PAIR(CP_UI_DIM));
    if (inBounds(g.cursorX, g.cursorY) && g.map[g.cursorY][g.cursorX].explored[0]) {
        Tile& ct = g.map[g.cursorY][g.cursorX];
        const char* bn[] = {"Temperate","Desert","Tundra","Swamp","Woodland"};
        const char* tn[] = {"Grassland","Tall Grass","Wildflowers","Meadow","Oak Forest","Pine Forest",
            "Palm Grove","Dead Tree","Mountain","Rolling Hills","Stone","Deep Water","Shallows",
            "Marshland","Reed Bed","Gold Deposit","Sandy Ground","Sand Dunes","Snow Cover","Frozen Ice",
            "Bare Earth","Stone Road","Wheat Field","Berry Bush","Ancient Ruins","Gravel",
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
            if (ent && ent->alive) {
                mch = isBuilding(ent->type) ? '#' : '*';
                if      (ent->owner == 0) mcp = CP_MM_PLAYER;
                else if (ent->owner == 1) mcp = CP_MM_ENEMY;
                else                      mcp = CP_MM_ANIMAL;
            }
        }
        attron(COLOR_PAIR(mcp)); mvaddch(mmY+my, panelX+1+mx, mch); attroff(COLOR_PAIR(mcp));
    }

    // Selection info panel
    int iy = mmY + mmH + 1;
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(iy-1, panelX, '-', panelW); attroff(COLOR_PAIR(CP_UI_DIM));

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
        attron(COLOR_PAIR(CP_PLAYER)|A_BOLD);
        mvprintw(iy++, panelX+1, "Group: %d units", (int)g.selectedIds.size());
        attroff(COLOR_PAIR(CP_PLAYER)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_TEXT));
        if (counts[0]) mvprintw(iy++, panelX+1, "  p x%d Peasant",  counts[0]);
        if (counts[1]) mvprintw(iy++, panelX+1, "  m x%d Militia",  counts[1]);
        if (counts[2]) mvprintw(iy++, panelX+1, "  a x%d Archer",   counts[2]);
        if (counts[3]) mvprintw(iy++, panelX+1, "  k x%d Knight",   counts[3]);
        if (counts[4]) mvprintw(iy++, panelX+1, "  c x%d Catapult", counts[4]);
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
                    case S_GATHERING: stDesc = (sel->gatherType==0) ? "Mining gold" : "Chopping wood"; break;
                    case S_BUILDING:  { Entity* b = findEntity(sel->targetId);
                                        if (b && !b->underConstruction && b->type==E_FARM)
                                            stDesc = "Tending farm";
                                        else
                                            stDesc = b ? (std::string("Building ") + STATS[b->type].name) : "Building";
                                        break; }
                    case S_RETURNING: stDesc = (sel->gatherType==0) ? "Carrying gold" : "Carrying wood"; break;
                    default:          stDesc = "Idle"; break;
                    }
                } else {
                    const char* sn[] = {"Idle","Moving","Attacking","Gathering","Building","Training","Returning","Dead"};
                    stDesc = sn[sel->state];
                }
                attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(iy++, panelX+1, "%s", stDesc.c_str()); attroff(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->carrying > 0) {
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Carrying: %d %s", sel->carrying, sel->gatherType==0?"gold":"wood");
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                }
            }
            if (sel->producing != E_NONE) {
                iy++;
                int pp = sel->prodProgress * 100 / std::max(1, sel->prodTime);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Training: %s", STATS[sel->producing].name);
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
                    if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE) mvprintw(iy++, panelX+1, "[T] Train");
                    if (sel->type==E_BLACKSMITH) mvprintw(iy++, panelX+1, "Speeds training");
                    if (sel->type==E_CHURCH)     mvprintw(iy++, panelX+1, "Heals nearby +Vision");
                    if (sel->type==E_MARKET)     mvprintw(iy++, panelX+1, "Passive gold income");
                    if (sel->type==E_FARM)        { mvprintw(iy++, panelX+1, "Generates food");
                                                     mvprintw(iy++, panelX+1, "Assign peasant to tend"); }
                    if (sel->type==E_LUMBER_CAMP) mvprintw(iy++, panelX+1, "Wood drop-off");
                    if (sel->type==E_MINING_CAMP) mvprintw(iy++, panelX+1, "Gold drop-off");
                    if (sel->type==E_MILL)        mvprintw(iy++, panelX+1, "Enables harvesting");
                    if (sel->type==E_CASTLE)     mvprintw(iy++, panelX+1, "+15 Supply, 300 HP");
                }
                attroff(COLOR_PAIR(CP_UI_ACCENT));
            }
        } else {
            attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy, panelX+1, "No selection"); attroff(COLOR_PAIR(CP_UI_DIM));
            iy += 2;
            attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend --"); attroff(COLOR_PAIR(CP_UI_DIM));
            attron(COLOR_PAIR(CP_UI_TEXT));
            mvprintw(iy++, panelX+1, "$ Gold   T Oak");
            mvprintw(iy++, panelX+1, "^ Mtn    Y Pine");
            mvprintw(iy++, panelX+1, "~ Water  n Hills");
            mvprintw(iy++, panelX+1, "# Castle & Ruins");
            attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
            attron(COLOR_PAIR(CP_PLAYER));
            mvprintw(iy++, panelX+1, "p Peasant  m Militia");
            mvprintw(iy++, panelX+1, "a Archer   k Knight");
            mvprintw(iy++, panelX+1, "c Catapult");
            attroff(COLOR_PAIR(CP_PLAYER)); iy++;
            attron(COLOR_PAIR(CP_DEER));
            mvprintw(iy++, panelX+1, "d Deer  s Sheep");
            mvprintw(iy++, panelX+1, "w Wolf");
            attroff(COLOR_PAIR(CP_DEER));
        }
    }

    // Bottom bars
    int botY2 = maxY-2, botY1 = maxY-1;
    attron(COLOR_PAIR(CP_UI_BAR)); mvhline(botY2, 0, ' ', maxX);
    if (g.mode == M_BUILD_SELECT)
        mvprintw(botY2, 1, " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [Esc] ");
    else if (g.mode == M_TRAIN_SELECT) {
        Entity* s2 = findEntity(g.selectedId);
        if (s2) {
            if (s2->type==E_TOWNHALL)  mvprintw(botY2, 1, " TRAIN: [P]easant(50g) [Esc] ");
            else if (s2->type==E_BARRACKS) mvprintw(botY2, 1, " TRAIN: [M]ilitia(60g) [A]rcher(70g) [C]atapult(180g+50w) [Esc] ");
            else if (s2->type==E_STABLE)   mvprintw(botY2, 1, " TRAIN: [K]night(120g) [Esc] ");
        }
    } else if (g.mode == M_PAUSED) {
        attron(A_BOLD); mvprintw(botY2, 1, " PAUSED - Press [P] to resume "); attroff(A_BOLD);
    } else if (g.mode == M_GAME_OVER) {
        attron(A_BOLD);
        if (g.winner==0) mvprintw(botY2, 1, " VICTORY! The realm is yours. [Q] Quit ");
        else             mvprintw(botY2, 1, " DEFEAT! Your kingdom has fallen. [Q] Quit ");
        attroff(A_BOLD);
    } else if (g.groupAssignPending) {
        attron(A_BOLD); mvprintw(botY2, 1, " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel "); attroff(A_BOLD);
    } else {
        mvprintw(botY2, 1, " Arrows:Move  Spc:Select  Enter:Cmd  B:Build  T:Train  A:All Mil  G:Group  1-9:Groups  P:Pause  Q:Quit ");
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

void render() { erase(); renderMap(); renderUI(); refresh(); }
