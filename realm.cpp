// REALM - A Medieval ASCII RTS (256-color edition)
// Compile: g++ -std=c++17 -O2 -o realm realm.cpp -lncurses
// Controls: Arrow keys=cursor, Space=select, Enter=command, B=build, T=train, P=pause, Q=quit

#include <ncurses.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <cstring>

// ============================================================
// CONSTANTS
// ============================================================
const int MAP_W = 140;
const int MAP_H = 90;
const int TICK_MS = 80;
const int FOG_RADIUS = 7;
const int GATHER_RATE = 8;
const int GATHER_TICKS = 15;
const int DAY_LENGTH = 1500;
const int SEASON_LENGTH = 3000;
const int CARRY_MAX = 20;
const int OWNER_NATURE = 2;

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
    T_DIRT, T_ROAD,
    T_WHEAT, T_BERRY,
    T_RUINS, T_GRAVEL,
    T_CASTLE_WALL, T_CASTLE_FLOOR, T_CASTLE_GATE
};

enum EntityType {
    E_NONE = 0,
    E_PEASANT, E_MILITIA, E_ARCHER, E_KNIGHT, E_CATAPULT,
    E_TOWNHALL, E_HOUSE, E_BARRACKS, E_STABLE, E_TOWER,
    E_FARM, E_BLACKSMITH, E_CHURCH, E_MARKET, E_WALL, E_CASTLE,
    E_DEER, E_WOLF, E_SHEEP
};

enum EntityState { S_IDLE, S_MOVING, S_ATTACKING, S_GATHERING, S_BUILDING, S_TRAINING, S_RETURNING, S_DEAD };
enum GameMode { M_NORMAL, M_BUILD_SELECT, M_TRAIN_SELECT, M_PAUSED, M_GAME_OVER };
enum Biome { B_TEMPERATE, B_DESERT, B_SNOW, B_SWAMP, B_FOREST };
enum Season { SPRING = 0, SUMMER, AUTUMN, WINTER };

// ============================================================
// ENTITY STATS
// ============================================================
struct EntityStats {
    const char* name; char glyph;
    int maxHp, atk, range, speed, atkSpeed, costGold, costWood, trainTime;
    int sizeW, sizeH, supplyProvided, supplyUsed; bool isBuilding;
};
static const EntityStats STATS[] = {
    {"None",       ' ',   0, 0,0,0,0,  0, 0,  0, 1,1, 0,0, false},
    {"Peasant",    'p',  30, 3,1,3,8, 50, 0, 40, 1,1, 0,1, false},
    {"Militia",    'm',  55, 8,1,3,6, 60, 0, 50, 1,1, 0,1, false},
    {"Archer",     'a',  35, 6,5,3,7, 70, 0, 60, 1,1, 0,1, false},
    {"Knight",     'k',  90,14,1,2,5,120, 0, 80, 1,1, 0,2, false},
    {"Catapult",   'c',  70,25,8,5,12,180,50,100, 1,1, 0,3, false},
    {"Town Hall",  'H', 200, 0,0,0,0,  0, 0,  0, 3,3, 10,0, true},
    {"House",      'h',  80, 0,0,0,0,  0,50, 60, 2,2,  5,0, true},
    {"Barracks",   'B', 120, 0,0,0,0,  0,150,80, 3,2,  0,0, true},
    {"Stable",     'S', 140, 0,0,0,0,  0,200,100,3,2,  0,0, true},
    {"Tower",      'X', 100,10,7,0,8, 50,100,70, 1,1,  0,0, true},
    {"Farm",       'F',  60, 0,0,0,0,  0, 60, 50, 2,2,  0,0, true},
    {"Blacksmith", 'A', 100, 0,0,0,0,  0,120, 70, 2,2,  0,0, true},
    {"Church",     '+', 120, 0,0,0,0, 80,100, 90, 2,2,  0,0, true},
    {"Market",     'M', 100, 0,0,0,0,  0,100, 60, 2,2,  0,0, true},
    {"Wall",       '#',  80, 0,0,0,0,  0, 20, 15, 1,1,  0,0, true},
    {"Castle",     'W', 300, 0,0,0,0,100,250,150, 4,4,  15,0, true},
    {"Deer",       'd',  20, 0,0,0,0,  0,  0,  0, 1,1,  0,0, false},
    {"Wolf",       'w',  35, 7,1,2, 8,  0,  0,  0, 1,1,  0,0, false},
    {"Sheep",      's',  12, 0,0,0,0,  0,  0,  0, 1,1,  0,0, false},
};
bool isUnit(EntityType t) { return (t >= E_PEASANT && t <= E_CATAPULT) || (t >= E_DEER && t <= E_SHEEP); }
bool isBuilding(EntityType t) { return t >= E_TOWNHALL && t <= E_CASTLE; }
bool isRanged(EntityType t) { return t == E_ARCHER || t == E_CATAPULT; }

// ============================================================
// 256-COLOR PALETTE
// xterm-256 indices:
//   0-7: standard, 8-15: bright
//   16-231: 6x6x6 cube = 16 + 36*r + 6*g + b  (r,g,b: 0-5)
//   232-255: grayscale (232=darkest, 255=lightest)
// ============================================================
// Named color indices for readability
namespace C {
    // Greens
    const int DARK_GREEN    = 22;   // deep forest
    const int MED_GREEN     = 28;   // standard tree
    const int GREEN         = 34;   // bright foliage
    const int BRIGHT_GREEN  = 40;   // spring lime
    const int PALE_GREEN    = 114;  // light spring
    const int OLIVE         = 70;   // dried grass
    const int YELLOW_GREEN  = 106;  // late summer
    const int PINE_GREEN    = 23;   // blue-green pine
    const int SWAMP_GREEN   = 58;   // murky

    // Blues
    const int DEEP_BLUE     = 17;   // deep water
    const int NAVY          = 18;
    const int BLUE          = 19;
    const int MED_BLUE      = 25;   // water surface
    const int TEAL          = 30;   // shallows
    const int BRIGHT_TEAL   = 37;
    const int CYAN          = 44;   // bright water
    const int DARK_CYAN     = 23;   // night water
    const int ICE_BLUE      = 117;  // frozen

    // Browns & Oranges
    const int DARK_BROWN    = 52;   // dark earth
    const int BROWN         = 94;   // standard brown
    const int AMBER         = 130;  // autumn mid
    const int TAN           = 137;  // sand
    const int LIGHT_TAN     = 180;  // pale sand
    const int DARK_GOLD     = 136;  // muted gold
    const int ORANGE        = 172;  // autumn bright
    const int DARK_ORANGE   = 166;  // rust
    const int BRIGHT_ORANGE = 208;  // fire

    // Yellows
    const int GOLD          = 178;  // gold ore
    const int BRIGHT_GOLD   = 220;  // shiny gold
    const int YELLOW        = 226;  // bright yellow
    const int WHEAT_GOLD    = 143;  // wheat field
    const int PALE_YELLOW   = 229;  // cream

    // Reds
    const int DARK_RED      = 124;
    const int RED           = 160;
    const int BRIGHT_RED    = 196;
    const int BERRY_RED     = 125;  // dark magenta-red

    // Purples
    const int PURPLE        = 54;
    const int MAUVE         = 96;
    const int LAVENDER      = 140;  // flowers
    const int DUSK_PURPLE   = 53;

    // Whites & Grays
    const int WHITE         = 231;
    const int SNOW_WHITE    = 255;
    const int BRIGHT_GRAY   = 252;
    const int LIGHT_GRAY    = 248;
    const int MED_GRAY      = 244;
    const int GRAY          = 240;
    const int DARK_GRAY     = 236;
    const int DARKER_GRAY   = 234;
    const int NEAR_BLACK    = 232;

    // Player/enemy
    const int PLAYER_CYAN   = 39;   // bright cyan
    const int PLAYER_DIM    = 31;   // dimmer cyan
    const int ENEMY_RED     = 196;
    const int ENEMY_DIM     = 124;

    // UI
    const int UI_BG         = 17;   // dark blue bg
    const int UI_TEXT        = 252;
    const int UI_HIGHLIGHT  = 220;
    const int UI_ACCENT     = 81;   // light blue
    const int UI_DIM        = 240;
}

// ============================================================
// COLOR PAIR IDS
// ============================================================
enum {
    // === TERRAIN BASE (day, temperate) ===
    CP_GRASS = 1,        // green on black
    CP_GRASS_LIGHT,      // light green (spring)
    CP_GRASS_DRY,        // yellow-green (summer)
    CP_TALL_GRASS,
    CP_FLOWERS,
    CP_FLOWERS_BLUE,
    CP_MEADOW,

    CP_FOREST,           // medium green
    CP_FOREST_DARK,      // deep green
    CP_PINE,             // blue-green
    CP_PALM,
    CP_DEAD_TREE,

    CP_MOUNTAIN,
    CP_HILLS,
    CP_STONE,

    CP_WATER,            // blue on deep blue
    CP_WATER_SHIMMER,    // lighter frame
    CP_SHALLOWS,         // teal
    CP_MARSH,
    CP_REEDS,

    CP_GOLD,             // bright gold
    CP_GOLD_SHIMMER,     // animation frame

    CP_SAND,
    CP_DUNES,
    CP_SNOW_GROUND,
    CP_ICE,

    CP_DIRT,
    CP_ROAD,

    CP_WHEAT,
    CP_WHEAT_GOLD,       // summer wheat
    CP_BERRY,

    CP_RUINS,
    CP_GRAVEL,

    CP_CASTLE_WALL,
    CP_CASTLE_FLOOR,
    CP_CASTLE_GATE,

    // === AUTUMN VARIANTS ===
    CP_AUT_TREE_EARLY,   // yellow-green
    CP_AUT_TREE_MID,     // amber/orange
    CP_AUT_TREE_LATE,    // brown, bare
    CP_AUT_GRASS,        // olive/brown
    CP_AUT_GRASS_LATE,   // brown

    // === WINTER VARIANTS ===
    CP_WIN_GROUND,       // snow covered
    CP_WIN_TREE,         // bare/gray
    CP_WIN_PINE,         // pine with snow tinge
    CP_WIN_ICE,          // frozen water

    // === NIGHT VARIANTS ===
    CP_NIGHT_GRASS,
    CP_NIGHT_TREE,
    CP_NIGHT_WATER,
    CP_NIGHT_GROUND,     // generic dark ground
    CP_NIGHT_GOLD,       // gold glows slightly

    // === DAWN/DUSK ===
    CP_DAWN_SKY,         // warm tint
    CP_DUSK_SKY,         // purple tint

    // === ENTITIES ===
    CP_PLAYER,
    CP_PLAYER_NIGHT,
    CP_ENEMY,
    CP_ENEMY_NIGHT,

    // === PROJECTILE ===
    CP_PROJ_ARROW,
    CP_PROJ_BOULDER,
    CP_PROJ_TOWER,

    // === UI ===
    CP_UI_BAR,
    CP_UI_TEXT,
    CP_UI_HIGH,
    CP_UI_DIM,
    CP_UI_ACCENT,
    CP_FOG,
    CP_FOG_EXPLORED,
    CP_CURSOR,
    CP_HP_GREEN,
    CP_HP_YELLOW,
    CP_HP_RED,
    CP_SUN,
    CP_MOON,
    CP_MM_PLAYER,
    CP_MM_ENEMY,
    CP_MM_WATER,
    CP_MM_FOREST,
    CP_MM_GOLD,
    CP_MM_SAND,
    CP_MM_SNOW,
    CP_MM_MTN,
    CP_MM_CASTLE,

    CP_SPRING_FLOWER,

    CP_DEER,
    CP_WOLF,
    CP_SHEEP,
    CP_MM_ANIMAL,

    CP_COUNT // sentinel
};

// ============================================================
// DATA STRUCTURES (same as before)
// ============================================================
struct Projectile { float x,y,tx,ty; char glyph; int color,life; bool alive; };

struct Tile {
    Terrain terrain; int resources;
    bool visible[2], explored[2]; Biome biome;
};
struct Entity {
    int id; EntityType type; int owner, x, y, hp, maxHp;
    EntityState state; int targetId, targetX, targetY;
    std::vector<std::pair<int,int>> path; int pathIdx;
    int moveCd, atkCd, gatherCd, gatherType;
    EntityType producing; int prodProgress, prodTime;
    bool underConstruction, alive; int rallyX, rallyY;
    int carrying;
};
struct Player { int gold, wood, supply, supplyMax; bool alive; };
struct Game {
    Tile map[MAP_H][MAP_W];
    std::vector<Entity> entities;
    std::vector<Projectile> projectiles;
    int nextId; Player players[3]; int tick;
    GameMode mode; int cursorX, cursorY, viewX, viewY, viewW, viewH;
    int selectedId; std::string statusMsg; int statusTimer;
    int winner, aiTimer, farmTimer;
    float dayPhase, seasonPhase;
};
static Game g;

// ============================================================
// TIME
// ============================================================
float getBrightness() { return std::max(0.0f, std::min(1.0f, sinf(g.dayPhase * M_PI))); }
Season getSeason() { return (Season)((int)g.seasonPhase % 4); }
float getSeasonProgress() { return g.seasonPhase - (int)g.seasonPhase; }
const char* getSeasonName() { const char* n[]={"Spring","Summer","Autumn","Winter"}; return n[getSeason()]; }
const char* getTimeName() {
    float b=getBrightness();
    if (b>0.85f) return "Noon";
    if (b>0.6f) return "Day";
    if (b>0.35f) return g.dayPhase<0.5f?"Dawn":"Dusk";
    if (b>0.15f) return "Twilight";
    return "Night";
}
bool isNight() { return getBrightness()<0.3f; }
bool isDusk() { float b=getBrightness(); return b>=0.3f&&b<0.55f&&g.dayPhase>0.5f; }
bool isDawn() { float b=getBrightness(); return b>=0.3f&&b<0.55f&&g.dayPhase<0.5f; }

bool shouldShowSeasonAt(int x, int y, float threshold) {
    int hash=((x*7919+y*6271)&0xFFFF);
    return (float)hash/65535.0f < threshold;
}

// ============================================================
// HELPERS
// ============================================================
int dist(int x1,int y1,int x2,int y2) { return std::max(std::abs(x1-x2),std::abs(y1-y2)); }
int mdist(int x1,int y1,int x2,int y2) { return std::abs(x1-x2)+std::abs(y1-y2); }
bool inBounds(int x,int y) { return x>=0&&x<MAP_W&&y>=0&&y<MAP_H; }
bool isPassable(int x,int y) {
    if (!inBounds(x,y)) return false;
    Terrain t=g.map[y][x].terrain;
    return t!=T_MOUNTAIN&&t!=T_WATER&&t!=T_ICE&&t!=T_STONE&&t!=T_CASTLE_WALL;
}
void setStatus(const std::string& msg) { g.statusMsg=msg; g.statusTimer=35; }
Entity* findEntity(int id) { for (auto& e:g.entities) if (e.id==id&&e.alive) return &e; return nullptr; }
Entity* findDepot(Entity& e) {
    Entity* best=nullptr; int bestD=99999;
    for (auto& o:g.entities) { if (!o.alive||o.owner!=e.owner||o.underConstruction) continue;
        if (o.type==E_TOWNHALL||o.type==E_CASTLE) { int d=mdist(e.x,e.y,o.x,o.y); if (d<bestD){bestD=d;best=&o;} } }
    return best;
}
Entity* entityAt(int x,int y) {
    for (auto& e:g.entities) { if (!e.alive) continue; auto& s=STATS[e.type];
        if (s.isBuilding) { if (x>=e.x&&x<e.x+s.sizeW&&y>=e.y&&y<e.y+s.sizeH) return &e; }
        else if (e.x==x&&e.y==y) return &e; }
    return nullptr;
}
Entity* entityAtOwner(int x,int y,int owner) {
    for (auto& e:g.entities) { if (!e.alive||e.owner!=owner) continue; auto& s=STATS[e.type];
        if (s.isBuilding) { if (x>=e.x&&x<e.x+s.sizeW&&y>=e.y&&y<e.y+s.sizeH) return &e; }
        else if (e.x==x&&e.y==y) return &e; }
    return nullptr;
}
bool canPlace(EntityType type,int x,int y,int owner) {
    auto& s=STATS[type];
    for (int dy=0;dy<s.sizeH;dy++) for (int dx=0;dx<s.sizeW;dx++) {
        int nx=x+dx,ny=y+dy;
        if (!inBounds(nx,ny)||!isPassable(nx,ny)) return false;
        if (g.map[ny][nx].terrain==T_GOLD) return false;
        if (entityAt(nx,ny)) return false;
    }
    return true;
}
void updateSupply(int owner) {
    int mx=0,used=0;
    for (auto& e:g.entities) { if (!e.alive||e.owner!=owner) continue;
        if (!e.underConstruction) mx+=STATS[e.type].supplyProvided;
        used+=STATS[e.type].supplyUsed; }
    g.players[owner].supplyMax=mx; g.players[owner].supply=used;
}

// ============================================================
// PROJECTILES
// ============================================================
void spawnProjectile(int sx,int sy,int tx,int ty,char gl,int col) {
    float dx=(float)(tx-sx),dy=(float)(ty-sy);
    float len=sqrtf(dx*dx+dy*dy); if (len<0.1f) return;
    Projectile p; p.x=(float)sx; p.y=(float)sy; p.tx=(float)tx; p.ty=(float)ty;
    p.glyph=gl; p.color=col; p.life=(int)(len/1.5f)+2; p.alive=true;
    g.projectiles.push_back(p);
}
void tickProjectiles() {
    for (auto& p:g.projectiles) { if (!p.alive) continue;
        float dx=p.tx-p.x,dy=p.ty-p.y,len=sqrtf(dx*dx+dy*dy);
        if (len<1.5f||p.life<=0) { p.alive=false; continue; }
        p.x+=(dx/len)*1.5f; p.y+=(dy/len)*1.5f; p.life--; }
    if (g.tick%30==0) g.projectiles.erase(std::remove_if(g.projectiles.begin(),g.projectiles.end(),
        [](const Projectile& p){return !p.alive;}),g.projectiles.end());
}

// ============================================================
// PATHFINDING
// ============================================================
std::vector<std::pair<int,int>> findPath(int sx,int sy,int tx,int ty,int maxSteps=300) {
    if (sx==tx&&sy==ty) return {};
    if (!isPassable(tx,ty)) {
        int bestD=9999,bx=tx,by=ty;
        for (int dy=-1;dy<=1;dy++) for (int dx=-1;dx<=1;dx++) {
            int nx=tx+dx,ny=ty+dy;
            if (isPassable(nx,ny)){int d=mdist(sx,sy,nx,ny);if(d<bestD){bestD=d;bx=nx;by=ny;}}
        }
        tx=bx;ty=by;
    }
    static int visited[MAP_H][MAP_W]; static int vgen=0; vgen++;
    struct Node{int x,y,px,py;};
    std::queue<Node> q; q.push({sx,sy,-1,-1}); visited[sy][sx]=vgen;
    static Node hist[MAP_W*MAP_H]; int hc=0; bool found=false; int fi=-1;
    while (!q.empty()&&hc<maxSteps*4) {
        Node cur=q.front();q.pop();int idx=hc;hist[hc++]=cur;
        if (cur.x==tx&&cur.y==ty){found=true;fi=idx;break;}
        static const int dx8[]={0,1,1,1,0,-1,-1,-1},dy8[]={-1,-1,0,1,1,1,0,-1};
        for (int i=0;i<8;i++){
            int nx=cur.x+dx8[i],ny=cur.y+dy8[i];
            if (!inBounds(nx,ny)||visited[ny][nx]==vgen) continue;
            if (!isPassable(nx,ny)&&!(nx==tx&&ny==ty)) continue;
            Entity* occ=entityAt(nx,ny);
            if (occ&&isBuilding(occ->type)&&!(nx==tx&&ny==ty)) continue;
            visited[ny][nx]=vgen;q.push({nx,ny,cur.x,cur.y});
        }
    }
    if (!found) return {};
    std::vector<std::pair<int,int>> path;
    int cx=hist[fi].x,cy=hist[fi].y;
    path.push_back({cx,cy});int px=hist[fi].px,py=hist[fi].py;
    while (px!=-1){path.push_back({px,py});
        for (int i=fi-1;i>=0;i--){if(hist[i].x==px&&hist[i].y==py){px=hist[i].px;py=hist[i].py;fi=i;break;}}
        if((int)path.size()>600)break;}
    std::reverse(path.begin(),path.end());
    if (!path.empty()) path.erase(path.begin());
    return path;
}

// ============================================================
// NOISE & MAP GEN
// ============================================================
static float noiseGrid[32][32];
void initNoise(){for(int y=0;y<32;y++)for(int x=0;x<32;x++)noiseGrid[y][x]=(float)(rand()%1000)/1000.0f;}
float lerp(float a,float b,float t){return a+t*(b-a);}
float sampleNoise(float fx,float fy){
    int x0=(int)fx%31,y0=(int)fy%31,x1=x0+1,y1=y0+1;
    float tx=fx-(int)fx,ty=fy-(int)fy;
    return lerp(lerp(noiseGrid[y0][x0],noiseGrid[y0][x1],tx),lerp(noiseGrid[y1][x0],noiseGrid[y1][x1],tx),ty);
}

void placeCastleRuin(int cx,int cy,int size) {
    for (int dy=0;dy<size;dy++) for (int dx=0;dx<size;dx++) {
        int x=cx+dx,y=cy+dy; if (!inBounds(x,y)) continue;
        bool isEdge=(dx==0||dx==size-1||dy==0||dy==size-1);
        bool isCorner=(dx==0||dx==size-1)&&(dy==0||dy==size-1);
        bool isGate=!isCorner&&isEdge&&(dx==size/2||dy==size/2);
        if (isGate) g.map[y][x].terrain=T_CASTLE_GATE;
        else if (isEdge) g.map[y][x].terrain=(rand()%4!=0)?T_CASTLE_WALL:T_RUINS;
        else g.map[y][x].terrain=T_CASTLE_FLOOR;
        g.map[y][x].resources=0;
    }
    int corners[][2]={{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c:corners) if (inBounds(c[0],c[1])) g.map[c[1]][c[0]].terrain=T_CASTLE_WALL;
}

void generateMap() {
    initNoise();
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        float n1=sampleNoise(x*0.08f,y*0.08f),n2=sampleNoise(x*0.05f+10,y*0.05f+10);
        Biome b=B_TEMPERATE;
        if (n1>0.7f) b=B_DESERT; else if (n1<0.25f) b=B_SNOW;
        else if (n2>0.7f) b=B_SWAMP; else if (n2<0.3f&&n1>0.4f&&n1<0.6f) b=B_FOREST;
        g.map[y][x]={T_GRASS,0,{false,false},{false,false},b};
    }
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        Tile& t=g.map[y][x]; int r=rand()%100;
        switch(t.biome){
        case B_TEMPERATE:
            if(r<5)t.terrain=T_TALL_GRASS;else if(r<8)t.terrain=T_FLOWERS;
            else if(r<10)t.terrain=T_MEADOW;else if(r<14){t.terrain=T_FOREST;t.resources=100+rand()%100;}
            else t.terrain=T_GRASS; break;
        case B_DESERT:
            if(r<60)t.terrain=T_SAND;else if(r<75)t.terrain=T_DUNES;
            else if(r<80)t.terrain=T_GRAVEL;else if(r<85){t.terrain=T_PALM;t.resources=60+rand()%40;}
            else t.terrain=T_SAND; break;
        case B_SNOW:
            if(r<60)t.terrain=T_SNOW;else if(r<75){t.terrain=T_PINE;t.resources=80+rand()%60;}
            else if(r<80)t.terrain=T_STONE;else t.terrain=T_SNOW; break;
        case B_SWAMP:
            if(r<30)t.terrain=T_MARSH;else if(r<45)t.terrain=T_REEDS;
            else if(r<55)t.terrain=T_SHALLOWS;else if(r<65){t.terrain=T_DEAD_TREE;t.resources=40+rand()%30;}
            else t.terrain=T_TALL_GRASS; break;
        case B_FOREST:
            if(r<40){t.terrain=T_FOREST;t.resources=100+rand()%100;}
            else if(r<55){t.terrain=T_PINE;t.resources=80+rand()%60;}
            else if(r<60)t.terrain=T_BERRY;else if(r<65)t.terrain=T_TALL_GRASS;
            else t.terrain=T_GRASS; break;
        }
    }
    // Mountains
    for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++){
        float n=sampleNoise(x*0.12f+5,y*0.12f+5);
        if(n>0.78f){g.map[y][x].terrain=T_MOUNTAIN;g.map[y][x].resources=0;}
        else if(n>0.72f&&g.map[y][x].biome!=B_DESERT){if(rand()%3==0)g.map[y][x].terrain=T_HILLS;}
    }
    // Rivers
    for(int r=0;r<4;r++){
        int rx,ry; if(r%2==0){rx=rand()%MAP_W;ry=0;}else{rx=0;ry=rand()%MAP_H;}
        int len=60+rand()%40;float angle=(rand()%628)/100.0f;
        for(int i=0;i<len;i++){
            int wx=rx+(int)(cos(angle)*i),wy=ry+(int)(sin(angle)*i);
            angle+=((rand()%100)-50)/200.0f;
            for(int dy=-1;dy<=1;dy++)for(int dx=-1;dx<=1;dx++){
                int nx=wx+dx,ny=wy+dy;
                if(inBounds(nx,ny)&&g.map[ny][nx].terrain!=T_MOUNTAIN){
                    if(dx==0&&dy==0)g.map[ny][nx].terrain=T_WATER;
                    else if(rand()%3==0)g.map[ny][nx].terrain=T_SHALLOWS;
                }
            }
        }
    }
    // Lakes
    for(int l=0;l<6;l++){
        int cx=20+rand()%(MAP_W-40),cy=20+rand()%(MAP_H-40),sz=3+rand()%4;
        for(int dy=-sz;dy<=sz;dy++)for(int dx=-sz;dx<=sz;dx++){
            if(dx*dx+dy*dy>sz*sz)continue;int nx=cx+dx,ny=cy+dy;
            if(inBounds(nx,ny)&&g.map[ny][nx].terrain!=T_MOUNTAIN){
                if(dx*dx+dy*dy<(sz-1)*(sz-1))g.map[ny][nx].terrain=T_WATER;
                else if(rand()%2==0)g.map[ny][nx].terrain=T_SHALLOWS;
                else g.map[ny][nx].terrain=T_REEDS;
            }
        }
    }
    // Gold
    auto placeGold=[](int cx,int cy,int count){
        for(int i=0;i<count;i++){int gx=cx+(rand()%7)-3,gy=cy+(rand()%5)-2;
            if(inBounds(gx,gy)&&g.map[gy][gx].terrain!=T_WATER&&g.map[gy][gx].terrain!=T_MOUNTAIN&&g.map[gy][gx].terrain!=T_SHALLOWS)
                {g.map[gy][gx].terrain=T_GOLD;g.map[gy][gx].resources=300+rand()%300;}}
    };
    placeGold(14,12,6);placeGold(MAP_W-16,MAP_H-14,6);
    for(int i=0;i<10;i++)placeGold(15+rand()%(MAP_W-30),15+rand()%(MAP_H-30),3+rand()%3);
    // Stone
    for(int i=0;i<12;i++){int sx=10+rand()%(MAP_W-20),sy=10+rand()%(MAP_H-20);
        for(int j=0;j<3;j++){int nx=sx+rand()%4-2,ny=sy+rand()%4-2;
            if(inBounds(nx,ny)&&g.map[ny][nx].terrain==T_GRASS)g.map[ny][nx].terrain=T_STONE;}}
    // Roads
    int midX=MAP_W/2,midY=MAP_H/2;
    auto makeRoad=[&](int sx,int sy,int ex,int ey){
        int cx=sx,cy=sy;while(cx!=ex||cy!=ey){
            if(inBounds(cx,cy)&&g.map[cy][cx].terrain!=T_WATER&&g.map[cy][cx].terrain!=T_MOUNTAIN&&g.map[cy][cx].terrain!=T_GOLD&&g.map[cy][cx].terrain!=T_SHALLOWS)
                g.map[cy][cx].terrain=T_ROAD;
            if(rand()%2==0){if(cx<ex)cx++;else if(cx>ex)cx--;}else{if(cy<ey)cy++;else if(cy>ey)cy--;}
            if(rand()%5==0){cx+=(rand()%3)-1;cy+=(rand()%3)-1;}
            cx=std::max(0,std::min(cx,MAP_W-1));cy=std::max(0,std::min(cy,MAP_H-1));}
    };
    makeRoad(15,15,midX,midY);makeRoad(MAP_W-15,MAP_H-15,midX,midY);makeRoad(midX,5,midX,MAP_H-5);
    // Castles
    placeCastleRuin(MAP_W/2-4,MAP_H/2-4,8);
    placeCastleRuin(MAP_W/4,MAP_H/4,6);
    placeCastleRuin(3*MAP_W/4,3*MAP_H/4,6);
    // Ruins
    for(int i=0;i<15;i++){int rx=10+rand()%(MAP_W-20),ry=10+rand()%(MAP_H-20);
        for(int j=0;j<3+rand()%4;j++){int nx=rx+rand()%5-2,ny=ry+rand()%5-2;
            if(inBounds(nx,ny)&&g.map[ny][nx].terrain==T_GRASS)g.map[ny][nx].terrain=T_RUINS;}}
    // Wheat
    for(int i=0;i<12;i++){int wx=10+rand()%(MAP_W-20),wy=10+rand()%(MAP_H-20);
        if(g.map[wy][wx].biome!=B_TEMPERATE)continue;int sz=2+rand()%3;
        for(int dy=-sz;dy<=sz;dy++)for(int dx=-sz;dx<=sz;dx++){int nx=wx+dx,ny=wy+dy;
            if(inBounds(nx,ny)&&g.map[ny][nx].terrain==T_GRASS&&rand()%2==0)g.map[ny][nx].terrain=T_WHEAT;}}
    // Clear starts
    auto clearArea=[](int cx,int cy,int r){
        for(int dy=-r;dy<=r+4;dy++)for(int dx=-r;dx<=r+4;dx++){int x=cx+dx,y=cy+dy;
            if(inBounds(x,y)&&g.map[y][x].terrain!=T_GOLD)g.map[y][x].terrain=T_GRASS;}};
    clearArea(4,4,6);clearArea(MAP_W-11,MAP_H-11,6);
    placeGold(14,9,5);placeGold(MAP_W-16,MAP_H-11,5);
}

// ============================================================
// ENTITY, FOG, ORDERS, GAME LOGIC, AI
// (identical to previous version - included verbatim)
// ============================================================
int spawnEntity(EntityType type,int owner,int x,int y,bool built=true){
    Entity e{};e.id=g.nextId++;e.type=type;e.owner=owner;e.x=x;e.y=y;
    e.maxHp=STATS[type].maxHp;e.hp=built?e.maxHp:1;e.state=S_IDLE;
    e.targetId=-1;e.targetX=-1;e.targetY=-1;e.producing=E_NONE;
    e.underConstruction=!built;e.alive=true;
    e.rallyX=x+STATS[type].sizeW;e.rallyY=y+STATS[type].sizeH;
    g.entities.push_back(e);updateSupply(owner);return e.id;
}

void updateFog(){
    for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++){g.map[y][x].visible[0]=false;g.map[y][x].visible[1]=false;}
    int nightPen=isNight()?2:(isDusk()||isDawn())?1:0;
    for(auto& e:g.entities){if(!e.alive||e.owner>=OWNER_NATURE)continue;
        int r=FOG_RADIUS-nightPen;
        if(isBuilding(e.type))r+=2;if(e.type==E_TOWER)r+=4;if(e.type==E_CASTLE)r+=3;if(e.type==E_CHURCH)r+=3;
        if(r<3)r=3;auto& s=STATS[e.type];int cx=e.x+s.sizeW/2,cy=e.y+s.sizeH/2;
        for(int dy=-r;dy<=r;dy++)for(int dx=-r;dx<=r;dx++){int nx=cx+dx,ny=cy+dy;
            if(inBounds(nx,ny)&&dx*dx+dy*dy<=r*r){g.map[ny][nx].visible[e.owner]=true;g.map[ny][nx].explored[e.owner]=true;}}
    }
}

Entity* findNearestEnemy(Entity& e,int range){
    Entity* best=nullptr;int bestD=range+1;
    for(auto& o:g.entities){if(!o.alive||o.owner==e.owner)continue;
        int d=dist(e.x,e.y,o.x,o.y);if(d<bestD){bestD=d;best=&o;}}return best;
}
void orderMove(Entity& e,int tx,int ty){e.state=S_MOVING;e.targetX=tx;e.targetY=ty;e.targetId=-1;e.path=findPath(e.x,e.y,tx,ty);e.pathIdx=0;}
void orderAttack(Entity& e,int tid){Entity* t=findEntity(tid);if(!t)return;e.state=S_ATTACKING;e.targetId=tid;}
void orderGather(Entity& e,int tx,int ty){
    if(e.type!=E_PEASANT)return;Terrain ter=g.map[ty][tx].terrain;
    bool isW=(ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
    if(ter!=T_GOLD&&!isW)return;e.state=S_GATHERING;e.targetX=tx;e.targetY=ty;
    e.gatherType=(ter==T_GOLD)?0:1;e.path=findPath(e.x,e.y,tx,ty);e.pathIdx=0;e.gatherCd=0;
    e.carrying=0;e.rallyX=tx;e.rallyY=ty;
}
void orderBuild(Entity& e,EntityType bt,int bx,int by){
    if(e.type!=E_PEASANT)return;Player& p=g.players[e.owner];
    if(p.gold<STATS[bt].costGold||p.wood<STATS[bt].costWood){if(e.owner==0)setStatus("Not enough resources!");return;}
    if(!canPlace(bt,bx,by,e.owner)){if(e.owner==0)setStatus("Can't build there!");return;}
    p.gold-=STATS[bt].costGold;p.wood-=STATS[bt].costWood;
    int bid=spawnEntity(bt,e.owner,bx,by,false);
    e.state=S_BUILDING;e.targetId=bid;e.targetX=bx;e.targetY=by;
    e.path=findPath(e.x,e.y,bx-1,by);e.pathIdx=0;
}
void orderTrain(Entity& bld,EntityType ut){
    if(!isBuilding(bld.type)||bld.underConstruction)return;
    if(bld.producing!=E_NONE){if(bld.owner==0)setStatus("Already training!");return;}
    Player& p=g.players[bld.owner];
    if(p.gold<STATS[ut].costGold||p.wood<STATS[ut].costWood){if(bld.owner==0)setStatus("Not enough resources!");return;}
    if(p.supply+STATS[ut].supplyUsed>p.supplyMax){if(bld.owner==0)setStatus("Need more houses!");return;}
    p.gold-=STATS[ut].costGold;p.wood-=STATS[ut].costWood;
    bld.producing=ut;bld.prodProgress=0;bld.prodTime=STATS[ut].trainTime;bld.state=S_TRAINING;
}

void moveAlongPath(Entity& e){
    if(e.pathIdx>=(int)e.path.size()){e.path.clear();e.pathIdx=0;if(e.state==S_MOVING)e.state=S_IDLE;return;}
    if(e.moveCd>0){e.moveCd--;return;}
    auto[nx,ny]=e.path[e.pathIdx];Entity* blk=entityAt(nx,ny);
    if(blk&&blk->id!=e.id){if(e.state==S_MOVING||e.state==S_GATHERING||e.state==S_BUILDING){
        e.path=findPath(e.x,e.y,e.path.back().first,e.path.back().second);e.pathIdx=0;}return;}
    e.x=nx;e.y=ny;e.pathIdx++;Terrain ter=g.map[ny][nx].terrain;
    int spd=STATS[e.type].speed;
    if(ter==T_ROAD||ter==T_DIRT||ter==T_CASTLE_FLOOR)spd=std::max(1,spd-1);
    else if(ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW)spd+=1;
    if(getSeason()==WINTER)spd=std::max(spd,STATS[e.type].speed+1);
    e.moveCd=spd;
}

void tickEntity(Entity& e){
    if(!e.alive)return;
    if(e.producing!=E_NONE&&!e.underConstruction){
        int bonus=0;for(auto& o:g.entities)if(o.alive&&o.owner==e.owner&&o.type==E_BLACKSMITH&&!o.underConstruction){bonus=1;break;}
        e.prodProgress+=1+bonus;
        if(e.prodProgress>=e.prodTime){auto& bs=STATS[e.type];bool placed=false;
            for(int r=0;r<=3&&!placed;r++)for(int dy=-r;dy<=bs.sizeH+r&&!placed;dy++)
                for(int dx=-r;dx<=bs.sizeW+r&&!placed;dx++){int nx=e.x+dx,ny=e.y+dy;
                    if(!inBounds(nx,ny)||!isPassable(nx,ny)||entityAt(nx,ny))continue;
                    spawnEntity(e.producing,e.owner,nx,ny);placed=true;}
            e.producing=E_NONE;e.state=S_IDLE;if(e.owner==0&&placed)setStatus("Training complete!");}}
    if(e.underConstruction){bool hasB=false;
        for(auto& o:g.entities)if(o.alive&&o.owner==e.owner&&o.state==S_BUILDING&&o.targetId==e.id)
            if(dist(o.x,o.y,e.x,e.y)<=STATS[e.type].sizeW+1)hasB=true;
        if(hasB){e.hp+=2;if(e.hp>=e.maxHp){e.hp=e.maxHp;e.underConstruction=false;updateSupply(e.owner);
            if(e.owner==0)setStatus(std::string(STATS[e.type].name)+" complete!");
            for(auto& o:g.entities)if(o.alive&&o.state==S_BUILDING&&o.targetId==e.id)o.state=S_IDLE;}}return;}
    if(!isUnit(e.type))return;
    switch(e.state){
    case S_IDLE:if(e.type!=E_PEASANT&&STATS[e.type].atk>0){Entity* en=findNearestEnemy(e,STATS[e.type].range+1);if(en)orderAttack(e,en->id);}break;
    case S_MOVING:moveAlongPath(e);if(e.path.empty()||e.pathIdx>=(int)e.path.size())e.state=S_IDLE;break;
    case S_ATTACKING:{Entity* t=findEntity(e.targetId);if(!t||!t->alive){e.state=S_IDLE;break;}
        int d=dist(e.x,e.y,t->x,t->y);
        if(d<=STATS[e.type].range){if(e.atkCd<=0){t->hp-=STATS[e.type].atk;e.atkCd=STATS[e.type].atkSpeed;
            if(isRanged(e.type)){char pc=(e.type==E_CATAPULT)?'o':'-';int pcol=(e.type==E_CATAPULT)?CP_PROJ_BOULDER:CP_PROJ_ARROW;
                spawnProjectile(e.x,e.y,t->x,t->y,pc,pcol);}
            if(t->hp<=0){t->alive=false;t->state=S_DEAD;e.state=S_IDLE;updateSupply(t->owner);}}else e.atkCd--;}
        else{if(e.path.empty()||e.pathIdx>=(int)e.path.size()){e.path=findPath(e.x,e.y,t->x,t->y);e.pathIdx=0;}moveAlongPath(e);}break;}
    case S_GATHERING:{int d=dist(e.x,e.y,e.targetX,e.targetY);
        if(d<=1){Tile& tile=g.map[e.targetY][e.targetX];
            bool isW=(tile.terrain==T_FOREST||tile.terrain==T_PINE||tile.terrain==T_PALM||tile.terrain==T_DEAD_TREE);
            if((tile.terrain==T_GOLD||isW)&&tile.resources>0){e.gatherCd++;
                if(e.gatherCd>=GATHER_TICKS){e.gatherCd=0;int amt=std::min(GATHER_RATE,tile.resources);tile.resources-=amt;
                    e.carrying+=amt;
                    if(tile.resources<=0)tile.terrain=T_DIRT;
                    if(e.carrying>=CARRY_MAX||tile.resources<=0){
                        Entity* dep=findDepot(e);
                        if(dep){e.state=S_RETURNING;e.targetId=dep->id;e.targetX=dep->x;e.targetY=dep->y;
                            e.path=findPath(e.x,e.y,dep->x,dep->y);e.pathIdx=0;}
                        else e.state=S_IDLE;}}}else e.state=S_IDLE;}
        else{moveAlongPath(e);if(e.path.empty()&&dist(e.x,e.y,e.targetX,e.targetY)>1){
            e.path=findPath(e.x,e.y,e.targetX,e.targetY);e.pathIdx=0;if(e.path.empty())e.state=S_IDLE;}}break;}
    case S_RETURNING:{
        Entity* dep=findEntity(e.targetId);
        if(!dep||!dep->alive){dep=findDepot(e);
            if(!dep){e.state=S_IDLE;break;}
            e.targetId=dep->id;e.targetX=dep->x;e.targetY=dep->y;
            e.path=findPath(e.x,e.y,dep->x,dep->y);e.pathIdx=0;}
        int d=dist(e.x,e.y,dep->x,dep->y);
        if(d<=STATS[dep->type].sizeW+1){
            if(e.gatherType==0)g.players[e.owner].gold+=e.carrying;
            else g.players[e.owner].wood+=e.carrying;
            e.carrying=0;
            Tile& rt=g.map[e.rallyY][e.rallyX];
            bool isW=(rt.terrain==T_FOREST||rt.terrain==T_PINE||rt.terrain==T_PALM||rt.terrain==T_DEAD_TREE);
            if((rt.terrain==T_GOLD||isW)&&rt.resources>0){
                e.state=S_GATHERING;e.targetX=e.rallyX;e.targetY=e.rallyY;
                e.path=findPath(e.x,e.y,e.rallyX,e.rallyY);e.pathIdx=0;}
            else e.state=S_IDLE;}
        else{moveAlongPath(e);
            if(e.path.empty()&&dist(e.x,e.y,dep->x,dep->y)>STATS[dep->type].sizeW+1){
                e.path=findPath(e.x,e.y,dep->x,dep->y);e.pathIdx=0;}}
        break;}
    case S_BUILDING:{Entity* bld=findEntity(e.targetId);if(!bld||!bld->alive||!bld->underConstruction){e.state=S_IDLE;break;}
        int d=dist(e.x,e.y,bld->x,bld->y);if(d>STATS[bld->type].sizeW+1){moveAlongPath(e);
            if(e.path.empty()){e.path=findPath(e.x,e.y,bld->x-1,bld->y);e.pathIdx=0;}}break;}
    default:break;}
}

void tickTowers(){for(auto& e:g.entities){if(!e.alive||e.underConstruction||e.type!=E_TOWER)continue;
    Entity* en=findNearestEnemy(e,STATS[E_TOWER].range);if(en){if(e.atkCd<=0){en->hp-=STATS[E_TOWER].atk;e.atkCd=STATS[E_TOWER].atkSpeed;
        spawnProjectile(e.x,e.y,en->x,en->y,'*',CP_PROJ_TOWER);
        if(en->hp<=0){en->alive=false;en->state=S_DEAD;updateSupply(en->owner);}}else e.atkCd--;}}}

void tickFarms(){g.farmTimer++;if(g.farmTimer<40)return;g.farmTimer=0;
    int bonus=(getSeason()==SUMMER)?2:(getSeason()==WINTER)?-1:0;
    for(int p=0;p<2;p++){int farms=0;for(auto& e:g.entities)if(e.alive&&e.owner==p&&e.type==E_FARM&&!e.underConstruction)farms++;
        g.players[p].gold+=std::max(0,farms*(3+bonus));}}
void tickMarkets(){if(g.tick%50!=0)return;for(int p=0;p<2;p++){int m=0;
    for(auto& e:g.entities)if(e.alive&&e.owner==p&&e.type==E_MARKET&&!e.underConstruction)m++;g.players[p].gold+=m*5;}}
void tickChurches(){if(g.tick%20!=0)return;for(auto& e:g.entities){if(!e.alive||e.type!=E_CHURCH||e.underConstruction)continue;
    for(auto& u:g.entities){if(!u.alive||u.owner!=e.owner||!isUnit(u.type))continue;
        if(dist(u.x,u.y,e.x,e.y)<=3&&u.hp<u.maxHp)u.hp=std::min(u.maxHp,u.hp+1);}}}

void tickAnimals(){
    static int atick=0; atick++;
    for(auto& e:g.entities){
        if(!e.alive||e.owner!=OWNER_NATURE)continue;
        // Deer and sheep: flee from nearby non-nature units
        if(e.type==E_DEER||e.type==E_SHEEP){
            if(e.state!=S_MOVING||e.path.empty()){
                for(auto& o:g.entities){
                    if(!o.alive||o.owner==OWNER_NATURE||!isUnit(o.type))continue;
                    if(dist(e.x,e.y,o.x,o.y)<=4){
                        int fx=std::max(1,std::min(e.x+(e.x-o.x)*4,MAP_W-2));
                        int fy=std::max(1,std::min(e.y+(e.y-o.y)*4,MAP_H-2));
                        if(isPassable(fx,fy))orderMove(e,fx,fy);
                        break;}}}}
        // All animals: random wander when idle
        if(e.state==S_IDLE&&atick%(35+(e.id%25))==0){
            int wx=e.x+(rand()%9)-4,wy=e.y+(rand()%9)-4;
            wx=std::max(1,std::min(wx,MAP_W-2));wy=std::max(1,std::min(wy,MAP_H-2));
            if(isPassable(wx,wy))orderMove(e,wx,wy);}
    }
}

void checkWin(){for(int p=0;p<2;p++){bool has=false;
    for(auto& e:g.entities)if(e.alive&&e.owner==p&&(e.type==E_TOWNHALL||e.type==E_CASTLE))has=true;
    if(!has){g.players[p].alive=false;g.winner=1-p;g.mode=M_GAME_OVER;}}}

// AI
int aiCount(int o,EntityType t){int c=0;for(auto& e:g.entities)if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)c++;return c;}
int aiCountAll(int o,EntityType t){int c=0;for(auto& e:g.entities)if(e.alive&&e.owner==o&&e.type==t)c++;return c;}
Entity* aiIdle(int o,EntityType t){for(auto& e:g.entities)if(e.alive&&e.owner==o&&e.type==t&&e.state==S_IDLE&&!e.underConstruction)return &e;return nullptr;}
Entity* aiBldg(int o,EntityType t){for(auto& e:g.entities)if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)return &e;return nullptr;}
void aiGather(int o){for(auto& e:g.entities){if(!e.alive||e.owner!=o||e.type!=E_PEASANT||e.state!=S_IDLE)continue;
    int bestD=9999;int bx=-1,by=-1;for(int y=0;y<MAP_H;y++)for(int x=0;x<MAP_W;x++){
        Terrain t=g.map[y][x].terrain;bool isR=(t==T_GOLD||t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE);
        if(isR&&g.map[y][x].resources>0){int d=mdist(e.x,e.y,x,y);if(d<bestD){bestD=d;bx=x;by=y;}}}
    if(bx>=0)orderGather(e,bx,by);}}
void aiBuildSpot(int o,EntityType bt,int& ox,int& oy){
    Entity* th=aiBldg(o,E_TOWNHALL);if(!th)th=aiBldg(o,E_CASTLE);if(!th)return;
    for(int r=3;r<15;r++)for(int a=0;a<20;a++){int bx=th->x+(rand()%(r*2+1))-r,by=th->y+(rand()%(r*2+1))-r;
        if(canPlace(bt,bx,by,o)){ox=bx;oy=by;return;}}}

void tickAI(){g.aiTimer++;if(g.aiTimer<15)return;g.aiTimer=0;
    int o=1;Player& p=g.players[o];
    int peas=aiCount(o,E_PEASANT),mil=aiCount(o,E_MILITIA),arch=aiCount(o,E_ARCHER),kni=aiCount(o,E_KNIGHT);
    int hous=aiCountAll(o,E_HOUSE),bar=aiCount(o,E_BARRACKS),stb=aiCount(o,E_STABLE);
    aiGather(o);
    if(peas<6){Entity* th=aiBldg(o,E_TOWNHALL);if(th&&th->producing==E_NONE&&p.gold>=50)orderTrain(*th,E_PEASANT);}
    if(p.supply+2>=p.supplyMax&&hous<6&&p.wood>=50){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_HOUSE,bx,by);if(bx>=0)orderBuild(*b,E_HOUSE,bx,by);}}
    if(bar==0&&p.wood>=150&&peas>=3){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_BARRACKS,bx,by);if(bx>=0)orderBuild(*b,E_BARRACKS,bx,by);}}
    if(bar>0){Entity* br=aiBldg(o,E_BARRACKS);if(br&&br->producing==E_NONE){
        if(mil<4&&p.gold>=60)orderTrain(*br,E_MILITIA);else if(arch<3&&p.gold>=70)orderTrain(*br,E_ARCHER);}}
    if(stb==0&&mil>=3&&p.wood>=200){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_STABLE,bx,by);if(bx>=0)orderBuild(*b,E_STABLE,bx,by);}}
    if(stb>0){Entity* st=aiBldg(o,E_STABLE);if(st&&st->producing==E_NONE&&kni<3&&p.gold>=120)orderTrain(*st,E_KNIGHT);}
    if(aiCountAll(o,E_TOWER)<2&&p.wood>=100&&p.gold>=50){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_TOWER,bx,by);if(bx>=0)orderBuild(*b,E_TOWER,bx,by);}}
    if(aiCountAll(o,E_FARM)<2&&p.wood>=60&&bar>0){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_FARM,bx,by);if(bx>=0)orderBuild(*b,E_FARM,bx,by);}}
    if(aiCount(o,E_BLACKSMITH)==0&&bar>0&&p.wood>=120){Entity* b=aiIdle(o,E_PEASANT);if(b){int bx=-1,by=-1;aiBuildSpot(o,E_BLACKSMITH,bx,by);if(bx>=0)orderBuild(*b,E_BLACKSMITH,bx,by);}}
    int army=mil+arch+kni;
    if(army>=6){Entity* pt=nullptr;for(auto& e:g.entities)if(e.alive&&e.owner==0){if(e.type==E_TOWNHALL||e.type==E_CASTLE){pt=&e;break;}pt=&e;}
        if(pt)for(auto& e:g.entities)if(e.alive&&e.owner==o&&isUnit(e.type)&&e.type!=E_PEASANT&&e.state==S_IDLE)orderAttack(e,pt->id);}
    Entity* th=aiBldg(o,E_TOWNHALL);if(!th)th=aiBldg(o,E_CASTLE);
    if(th)for(auto& en:g.entities){if(!en.alive||en.owner==o)continue;if(dist(en.x,en.y,th->x,th->y)<15){
        for(auto& d:g.entities)if(d.alive&&d.owner==o&&isUnit(d.type)&&d.type!=E_PEASANT&&d.state==S_IDLE)orderAttack(d,en.id);break;}}
}

// ============================================================
// 256-COLOR INIT
// ============================================================
void initColors() {
    start_color();
    use_default_colors();
    // Verify 256 color support, fallback gracefully
    // All init_pair(id, fg_color_index, bg_color_index)
    int bg = -1; // transparent/default terminal bg

    // Terrain - day/temperate
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

    // Autumn
    init_pair(CP_AUT_TREE_EARLY, C::YELLOW_GREEN, bg);
    init_pair(CP_AUT_TREE_MID,   C::ORANGE,       bg);
    init_pair(CP_AUT_TREE_LATE,  C::BROWN,        bg);
    init_pair(CP_AUT_GRASS,      C::OLIVE,        bg);
    init_pair(CP_AUT_GRASS_LATE, C::BROWN,        bg);

    // Winter
    init_pair(CP_WIN_GROUND,     C::SNOW_WHITE,   bg);
    init_pair(CP_WIN_TREE,       C::LIGHT_GRAY,   bg);
    init_pair(CP_WIN_PINE,       C::PINE_GREEN,   bg);
    init_pair(CP_WIN_ICE,        C::ICE_BLUE,     C::NAVY);

    // Night
    init_pair(CP_NIGHT_GRASS,    C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_TREE,     C::DARK_GREEN,   bg);
    init_pair(CP_NIGHT_WATER,    C::NAVY,         C::NEAR_BLACK);
    init_pair(CP_NIGHT_GROUND,   C::DARKER_GRAY,  bg);
    init_pair(CP_NIGHT_GOLD,     C::DARK_GOLD,    bg);

    // Dawn/Dusk
    init_pair(CP_DAWN_SKY,       C::ORANGE,       bg);
    init_pair(CP_DUSK_SKY,       C::DUSK_PURPLE,  bg);

    // Entities
    init_pair(CP_PLAYER,         C::PLAYER_CYAN,  bg);
    init_pair(CP_PLAYER_NIGHT,   C::PLAYER_DIM,   bg);
    init_pair(CP_ENEMY,          C::ENEMY_RED,    bg);
    init_pair(CP_ENEMY_NIGHT,    C::ENEMY_DIM,    bg);

    // Projectiles
    init_pair(CP_PROJ_ARROW,     C::BRIGHT_GOLD,  bg);
    init_pair(CP_PROJ_BOULDER,   C::BRIGHT_GRAY,  bg);
    init_pair(CP_PROJ_TOWER,     C::BRIGHT_RED,   bg);

    // UI
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
// TERRAIN VISUALS (256 color, season & time aware)
// ============================================================
void getTerrainVisual(Terrain t, int x, int y, char& ch, int& cp) {
    Season season = getSeason();
    float sprog = getSeasonProgress();
    float bright = getBrightness();
    bool night = bright < 0.3f;
    bool twi = bright >= 0.15f && bright < 0.4f;
    Biome biome = g.map[y][x].biome;

    // Base appearance
    switch(t) {
    case T_GRASS:        ch='.'; cp=CP_GRASS; break;
    case T_TALL_GRASS:   ch='"'; cp=CP_TALL_GRASS; break;
    case T_FLOWERS:      ch='*'; cp=CP_FLOWERS; break;
    case T_MEADOW:       ch=','; cp=CP_MEADOW; break;
    case T_FOREST:       ch='T'; cp=CP_FOREST; break;
    case T_PINE:         ch='Y'; cp=CP_PINE; break;
    case T_PALM:         ch='y'; cp=CP_PALM; break;
    case T_DEAD_TREE:    ch='t'; cp=CP_DEAD_TREE; break;
    case T_MOUNTAIN:     ch='^'; cp=CP_MOUNTAIN; break;
    case T_HILLS:        ch='n'; cp=CP_HILLS; break;
    case T_STONE:        ch='o'; cp=CP_STONE; break;
    case T_WATER:        ch='~'; cp=CP_WATER; break;
    case T_SHALLOWS:     ch='~'; cp=CP_SHALLOWS; break;
    case T_MARSH:        ch='='; cp=CP_MARSH; break;
    case T_REEDS:        ch='|'; cp=CP_REEDS; break;
    case T_GOLD:         ch='$'; cp=CP_GOLD; break;
    case T_SAND:         ch='.'; cp=CP_SAND; break;
    case T_DUNES:        ch='~'; cp=CP_DUNES; break;
    case T_SNOW:         ch='.'; cp=CP_SNOW_GROUND; break;
    case T_ICE:          ch='='; cp=CP_ICE; break;
    case T_DIRT:         ch='.'; cp=CP_DIRT; break;
    case T_ROAD:         ch='#'; cp=CP_ROAD; break;
    case T_WHEAT:        ch='%'; cp=CP_WHEAT; break;
    case T_BERRY:        ch='*'; cp=CP_BERRY; break;
    case T_RUINS:        ch='&'; cp=CP_RUINS; break;
    case T_GRAVEL:       ch=':'; cp=CP_GRAVEL; break;
    case T_CASTLE_WALL:  ch='#'; cp=CP_CASTLE_WALL; break;
    case T_CASTLE_FLOOR: ch='.'; cp=CP_CASTLE_FLOOR; break;
    case T_CASTLE_GATE:  ch='='; cp=CP_CASTLE_GATE; break;
    }

    // === Water animation ===
    if (t==T_WATER) {
        int frame = (g.tick/6 + x + y) % 6;
        const char wc[]={'~','~','-','~','~','-'};
        ch = wc[frame];
        cp = (frame==2||frame==5) ? CP_WATER_SHIMMER : CP_WATER;
    }
    if (t==T_SHALLOWS) {
        int frame = (g.tick/8 + x*3) % 4;
        const char sc[]={'~','-','~','-'};
        ch = sc[frame];
    }
    if (t==T_MARSH) {
        int frame = (g.tick/10 + x) % 3;
        const char mc[]={'=','-','='};
        ch = mc[frame];
    }
    if (t==T_REEDS) {
        int frame = (g.tick/12 + x + y*3) % 4;
        const char rc[]={'|','/','|','\\'};
        ch = rc[frame];
    }
    // Gold shimmer
    if (t==T_GOLD) {
        int frame = (g.tick/4 + x*5 + y*3) % 8;
        cp = (frame < 2) ? CP_GOLD_SHIMMER : CP_GOLD;
    }

    // === Skip desert/snow biomes for seasonal changes ===
    if (biome != B_DESERT && biome != B_SNOW) {

        // === SEASONAL OVERRIDES ===
        switch(season) {
        case SPRING:
            // Fresh bright greens emerge
            if (t==T_GRASS && shouldShowSeasonAt(x,y,sprog*0.6f))
                cp = CP_GRASS_LIGHT;
            if (t==T_FOREST && shouldShowSeasonAt(x,y,sprog*0.5f))
                cp = CP_GRASS_LIGHT; // bright spring canopy
            // Flowers bloom gradually
            if (sprog>0.4f && t==T_GRASS && shouldShowSeasonAt(x,y,(sprog-0.4f)*1.5f)) {
                if ((x*13+y*7)%11==0) { ch='*'; cp=CP_SPRING_FLOWER; }
            }
            break;

        case SUMMER:
            // Lush deep greens, golden wheat
            if (t==T_FOREST) cp = CP_FOREST_DARK;
            if (t==T_GRASS && shouldShowSeasonAt(x,y,0.3f)) cp = CP_GRASS_DRY;
            if (t==T_WHEAT) { ch='%'; cp = CP_WHEAT_GOLD; }
            break;

        case AUTUMN: {
            float p = sprog;
            // Trees: green -> yellow-green -> amber -> brown -> bare
            if (t==T_FOREST) {
                if (shouldShowSeasonAt(x,y,p*0.4f)) cp = CP_AUT_TREE_EARLY;
                if (p>0.3f && shouldShowSeasonAt(x,y,(p-0.3f)*1.4f)) cp = CP_AUT_TREE_MID;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.5f)) { cp = CP_AUT_TREE_LATE; ch='t'; }
            }
            // Pines change less but some browning
            if (t==T_PINE && p>0.5f && shouldShowSeasonAt(x,y,(p-0.5f)*0.6f))
                cp = CP_AUT_TREE_EARLY;
            // Grass dries
            if ((t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW)) {
                if (shouldShowSeasonAt(x,y,p*0.5f)) cp = CP_AUT_GRASS;
                if (p>0.6f && shouldShowSeasonAt(x,y,(p-0.6f)*2.0f)) { cp = CP_AUT_GRASS_LATE; if(t==T_TALL_GRASS) ch=','; }
            }
            // Flowers wilt
            if (t==T_FLOWERS && shouldShowSeasonAt(x,y,p*0.7f)) { ch='.'; cp=CP_AUT_GRASS; }
            // Wheat harvested
            if (t==T_WHEAT && shouldShowSeasonAt(x,y,p)) { ch=','; cp=CP_DIRT; }
            break;
        }
        case WINTER: {
            float p = sprog;
            // Snow covers ground
            if ((t==T_GRASS||t==T_TALL_GRASS||t==T_MEADOW||t==T_FLOWERS||t==T_DIRT) && shouldShowSeasonAt(x,y,p*0.8f))
                { ch='.'; cp=CP_WIN_GROUND; }
            // Trees bare
            if (t==T_FOREST && shouldShowSeasonAt(x,y,p*0.7f)) { ch='t'; cp=CP_WIN_TREE; }
            // Pines get snow-dusted
            if (t==T_PINE && shouldShowSeasonAt(x,y,p*0.4f)) cp=CP_WIN_PINE;
            // Wheat gone
            if (t==T_WHEAT) { ch='.'; cp=shouldShowSeasonAt(x,y,p*0.5f)?CP_WIN_GROUND:CP_DIRT; }
            // Shallows freeze
            if (t==T_SHALLOWS && p>0.3f && shouldShowSeasonAt(x,y,(p-0.3f)*1.4f)) { ch='='; cp=CP_WIN_ICE; }
            // Hills snow
            if (t==T_HILLS && shouldShowSeasonAt(x,y,p*0.6f)) cp=CP_WIN_GROUND;
            // Berry bare
            if (t==T_BERRY && shouldShowSeasonAt(x,y,p*0.5f)) { ch='.'; cp=CP_WIN_TREE; }
            // Late winter thaw
            if (p>0.85f) {
                float thaw=(p-0.85f)*6.67f;
                if (cp==CP_WIN_GROUND && t!=T_SNOW && shouldShowSeasonAt(x+100,y+100,thaw))
                    { ch='.'; cp=CP_GRASS; }
            }
            break;
        }}
    }

    // === NIGHT / TWILIGHT OVERRIDES ===
    if (night) {
        // Map most colors to dark variants
        if (cp==CP_GRASS||cp==CP_GRASS_LIGHT||cp==CP_GRASS_DRY||cp==CP_TALL_GRASS||cp==CP_MEADOW
            ||cp==CP_AUT_GRASS||cp==CP_AUT_GRASS_LATE)
            cp = CP_NIGHT_GRASS;
        if (cp==CP_FOREST||cp==CP_FOREST_DARK||cp==CP_PINE||cp==CP_PALM||cp==CP_DEAD_TREE
            ||cp==CP_AUT_TREE_EARLY||cp==CP_AUT_TREE_MID||cp==CP_AUT_TREE_LATE||cp==CP_WIN_TREE)
            cp = CP_NIGHT_TREE;
        if (cp==CP_WATER||cp==CP_WATER_SHIMMER||cp==CP_SHALLOWS)
            cp = CP_NIGHT_WATER;
        if (cp==CP_SAND||cp==CP_DUNES||cp==CP_DIRT||cp==CP_ROAD||cp==CP_GRAVEL
            ||cp==CP_CASTLE_FLOOR||cp==CP_RUINS||cp==CP_WHEAT||cp==CP_WHEAT_GOLD)
            cp = CP_NIGHT_GROUND;
        if (cp==CP_GOLD||cp==CP_GOLD_SHIMMER)
            cp = CP_NIGHT_GOLD; // gold still faintly glows
        // Snow stays somewhat visible at night
        if (cp==CP_WIN_GROUND||cp==CP_SNOW_GROUND)
            cp = CP_FOG_EXPLORED; // dim white
    }
}

// ============================================================
// RENDERING
// ============================================================
void renderMap() {
    int maxY,maxX; getmaxyx(stdscr,maxY,maxX);
    int panelW=24; g.viewW=maxX-panelW-1; g.viewH=maxY-4;
    if(g.viewW<30)g.viewW=maxX; if(g.viewH<10)g.viewH=maxY-2;

    if(g.cursorX<g.viewX+3)g.viewX=g.cursorX-3;
    if(g.cursorX>g.viewX+g.viewW-4)g.viewX=g.cursorX-g.viewW+4;
    if(g.cursorY<g.viewY+2)g.viewY=g.cursorY-2;
    if(g.cursorY>g.viewY+g.viewH-3)g.viewY=g.cursorY-g.viewH+3;
    g.viewX=std::max(0,std::min(g.viewX,MAP_W-g.viewW));
    g.viewY=std::max(0,std::min(g.viewY,MAP_H-g.viewH));

    bool night = isNight();

    for(int sy=0;sy<g.viewH;sy++){int my=g.viewY+sy;
        for(int sx=0;sx<g.viewW;sx++){int mx=g.viewX+sx;
            int scY=sy+2,scX=sx;
            if(!inBounds(mx,my)){mvaddch(scY,scX,' ');continue;}
            Tile& tile=g.map[my][mx];bool vis=tile.visible[0],expl=tile.explored[0];
            if(!expl){mvaddch(scY,scX,' ');continue;}

            char ch; int cp;
            getTerrainVisual(tile.terrain,mx,my,ch,cp);

            if(!vis){attron(COLOR_PAIR(CP_FOG_EXPLORED));mvaddch(scY,scX,ch);attroff(COLOR_PAIR(CP_FOG_EXPLORED));continue;}

            // Entity
            Entity* ent=entityAt(mx,my);
            if(ent&&ent->alive){
                ch=STATS[ent->type].glyph;
                if(ent->owner==0) cp=night?CP_PLAYER_NIGHT:CP_PLAYER;
                else if(ent->owner==1) cp=night?CP_ENEMY_NIGHT:CP_ENEMY;
                else if(ent->type==E_WOLF) cp=CP_WOLF;
                else if(ent->type==E_SHEEP) cp=CP_SHEEP;
                else cp=CP_DEER;
                if(ent->underConstruction&&g.tick%10<5)ch='#';
            }
            // Projectile
            for(auto& p:g.projectiles){if(!p.alive)continue;
                if((int)roundf(p.x)==mx&&(int)roundf(p.y)==my){ch=p.glyph;cp=p.color;}}

            bool isCur=(mx==g.cursorX&&my==g.cursorY);
            bool isSel=false; Entity* sel=findEntity(g.selectedId);
            if(sel&&!isCur){auto& ss=STATS[sel->type];
                if(ss.isBuilding){if(mx>=sel->x&&mx<sel->x+ss.sizeW&&my>=sel->y&&my<sel->y+ss.sizeH)isSel=true;}
                else if(mx==sel->x&&my==sel->y)isSel=true;}

            if(isCur){attron(COLOR_PAIR(CP_CURSOR));mvaddch(scY,scX,ch);attroff(COLOR_PAIR(CP_CURSOR));}
            else{int attr=COLOR_PAIR(cp);
                if(ent&&ent->alive)attr|=A_BOLD;
                if(isSel)attr|=A_UNDERLINE;
                attron(attr);mvaddch(scY,scX,ch);attroff(attr);}
        }
    }
}

void renderUI(){
    int maxY,maxX;getmaxyx(stdscr,maxY,maxX);
    Player& p=g.players[0];int panelW=24,panelX=maxX-panelW;

    // Top bar
    attron(COLOR_PAIR(CP_UI_BAR)|A_BOLD);mvhline(0,0,' ',maxX);
    mvprintw(0,1," REALM ");attroff(A_BOLD);
    mvprintw(0,9,"Gold:%-5d Wood:%-5d Pop:%d/%d",p.gold,p.wood,p.supply,p.supplyMax);

    // Time/season
    int iconX=maxX-22;
    if(getBrightness()>0.5f){attron(COLOR_PAIR(CP_SUN)|A_BOLD);mvprintw(0,iconX,"*");attroff(COLOR_PAIR(CP_SUN)|A_BOLD);}
    else{attron(COLOR_PAIR(CP_MOON));mvprintw(0,iconX,"o");attroff(COLOR_PAIR(CP_MOON));}
    attron(COLOR_PAIR(CP_UI_BAR));mvprintw(0,iconX+1," %-5s %-6s",getTimeName(),getSeasonName());
    attroff(COLOR_PAIR(CP_UI_BAR));

    // Terrain info bar
    attron(COLOR_PAIR(CP_UI_DIM));mvhline(1,0,'-',g.viewW);attroff(COLOR_PAIR(CP_UI_DIM));
    if(inBounds(g.cursorX,g.cursorY)&&g.map[g.cursorY][g.cursorX].explored[0]){
        Tile& ct=g.map[g.cursorY][g.cursorX];
        const char* bn[]={"Temperate","Desert","Tundra","Swamp","Woodland"};
        const char* tn[]={"Grassland","Tall Grass","Wildflowers","Meadow","Oak Forest","Pine Forest",
            "Palm Grove","Dead Tree","Mountain","Rolling Hills","Stone","Deep Water","Shallows",
            "Marshland","Reed Bed","Gold Deposit","Sandy Ground","Sand Dunes","Snow Cover","Frozen Ice",
            "Bare Earth","Stone Road","Wheat Field","Berry Bush","Ancient Ruins","Gravel",
            "Castle Wall","Castle Floor","Castle Gate"};
        attron(COLOR_PAIR(CP_UI_TEXT));mvprintw(1,1,"%-16s",tn[ct.terrain]);attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM));mvprintw(1,18,"[%s]",bn[ct.biome]);attroff(COLOR_PAIR(CP_UI_DIM));
        if(ct.resources>0){attron(COLOR_PAIR(CP_UI_HIGH));mvprintw(1,30,"Res:%d",ct.resources);attroff(COLOR_PAIR(CP_UI_HIGH));}
    }

    // Panel separator
    for(int y=0;y<maxY;y++){attron(COLOR_PAIR(CP_UI_DIM));mvaddch(y,panelX-1,'|');attroff(COLOR_PAIR(CP_UI_DIM));}

    // Minimap
    attron(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);mvprintw(0,panelX+1,"Map");attroff(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);
    int mmW=panelW-2,mmH=std::min(g.viewH/3,14),mmY=1;
    for(int my=0;my<mmH;my++)for(int mx=0;mx<mmW;mx++){
        int mapX=mx*MAP_W/mmW,mapY=my*MAP_H/mmH;char mch=' ';int mcp=CP_FOG;
        if(g.map[mapY][mapX].explored[0]){Terrain t=g.map[mapY][mapX].terrain;
            if(t==T_WATER||t==T_SHALLOWS){mch='~';mcp=CP_MM_WATER;}
            else if(t==T_MOUNTAIN||t==T_STONE){mch='^';mcp=CP_MM_MTN;}
            else if(t==T_FOREST||t==T_PINE||t==T_PALM){mch='.';mcp=CP_MM_FOREST;}
            else if(t==T_GOLD){mch='$';mcp=CP_MM_GOLD;}
            else if(t==T_SAND||t==T_DUNES){mch='.';mcp=CP_MM_SAND;}
            else if(t==T_SNOW||t==T_ICE){mch='.';mcp=CP_MM_SNOW;}
            else if(t==T_CASTLE_WALL||t==T_CASTLE_GATE){mch='#';mcp=CP_MM_CASTLE;}
            else{mch='.';mcp=CP_FOG;}}
        if(g.map[mapY][mapX].visible[0]){Entity* ent=entityAt(mapX,mapY);if(ent&&ent->alive){
            mch=isBuilding(ent->type)?'#':'*';
            if(ent->owner==0)mcp=CP_MM_PLAYER;
            else if(ent->owner==1)mcp=CP_MM_ENEMY;
            else mcp=CP_MM_ANIMAL;}}
        attron(COLOR_PAIR(mcp));mvaddch(mmY+my,panelX+1+mx,mch);attroff(COLOR_PAIR(mcp));}

    // Selection info
    int iy=mmY+mmH+1;
    attron(COLOR_PAIR(CP_UI_DIM));mvhline(iy-1,panelX,'-',panelW);attroff(COLOR_PAIR(CP_UI_DIM));
    Entity* sel=findEntity(g.selectedId);
    if(sel){auto& st=STATS[sel->type];int nc=(sel->owner==0)?CP_PLAYER:CP_ENEMY;
        attron(COLOR_PAIR(nc)|A_BOLD);mvprintw(iy++,panelX+1,"%-20s",st.name);attroff(COLOR_PAIR(nc)|A_BOLD);
        int barW=panelW-4,filled=sel->hp*barW/std::max(1,sel->maxHp);
        int pct=sel->hp*100/std::max(1,sel->maxHp);
        int hc=(pct>60)?CP_HP_GREEN:(pct>30)?CP_HP_YELLOW:CP_HP_RED;
        mvprintw(iy,panelX+1,"HP");
        for(int i=0;i<barW;i++){int c=(i<filled)?hc:CP_FOG;
            attron(COLOR_PAIR(c));mvaddch(iy,panelX+3+i,(i<filled)?'|':'-');attroff(COLOR_PAIR(c));}
        iy++;attron(COLOR_PAIR(CP_UI_TEXT));mvprintw(iy++,panelX+1,"%d / %d",sel->hp,sel->maxHp);attroff(COLOR_PAIR(CP_UI_TEXT));
        if(isUnit(sel->type)){attron(COLOR_PAIR(CP_UI_TEXT));mvprintw(iy++,panelX+1,"ATK %-3d  RNG %-2d",st.atk,st.range);attroff(COLOR_PAIR(CP_UI_TEXT));
            const char* sn[]={"Idle","Moving","Attacking","Gathering","Building","Training","Returning","Dead"};
            attron(COLOR_PAIR(CP_UI_ACCENT));mvprintw(iy++,panelX+1,"%s",sn[sel->state]);attroff(COLOR_PAIR(CP_UI_ACCENT));
            if(sel->carrying>0){attron(COLOR_PAIR(CP_UI_HIGH));
                mvprintw(iy++,panelX+1,"Carrying: %d %s",sel->carrying,sel->gatherType==0?"gold":"wood");
                attroff(COLOR_PAIR(CP_UI_HIGH));}}
        if(sel->producing!=E_NONE){iy++;int pp=sel->prodProgress*100/std::max(1,sel->prodTime);
            attron(COLOR_PAIR(CP_UI_HIGH));mvprintw(iy++,panelX+1,"Training: %s",STATS[sel->producing].name);
            int pb=panelW-4,pf=pp*pb/100;for(int i=0;i<pb;i++){int c=(i<pf)?CP_UI_HIGH:CP_FOG;
                attron(COLOR_PAIR(c));mvaddch(iy,panelX+1+i,(i<pf)?'=':'-');attroff(COLOR_PAIR(c));}
            iy++;mvprintw(iy++,panelX+1,"%d%%",pp);attroff(COLOR_PAIR(CP_UI_HIGH));}
        if(sel->underConstruction){int bp=sel->hp*100/std::max(1,sel->maxHp);
            attron(COLOR_PAIR(CP_UI_HIGH));mvprintw(iy++,panelX+1,"Building: %d%%",bp);attroff(COLOR_PAIR(CP_UI_HIGH));}
        iy++;if(sel->owner==0){attron(COLOR_PAIR(CP_UI_DIM));mvhline(iy-1,panelX,'-',panelW);attroff(COLOR_PAIR(CP_UI_DIM));
            attron(COLOR_PAIR(CP_UI_ACCENT));
            if(sel->type==E_PEASANT){mvprintw(iy++,panelX+1,"[B] Build");mvprintw(iy++,panelX+1,"[Enter] Move/Gather");}
            else if(isUnit(sel->type))mvprintw(iy++,panelX+1,"[Enter] Move/Attack");
            else if(isBuilding(sel->type)&&!sel->underConstruction){
                if(sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE)mvprintw(iy++,panelX+1,"[T] Train");
                if(sel->type==E_BLACKSMITH)mvprintw(iy++,panelX+1,"Speeds training");
                if(sel->type==E_CHURCH)mvprintw(iy++,panelX+1,"Heals nearby +Vision");
                if(sel->type==E_MARKET)mvprintw(iy++,panelX+1,"Passive gold income");
                if(sel->type==E_FARM)mvprintw(iy++,panelX+1,"Generates gold");
                if(sel->type==E_CASTLE)mvprintw(iy++,panelX+1,"+15 Supply, 300 HP");}
            attroff(COLOR_PAIR(CP_UI_ACCENT));}
    } else {
        attron(COLOR_PAIR(CP_UI_DIM));mvprintw(iy,panelX+1,"No selection");attroff(COLOR_PAIR(CP_UI_DIM));
        iy+=2;attron(COLOR_PAIR(CP_UI_DIM));mvprintw(iy++,panelX+1,"-- Legend --");attroff(COLOR_PAIR(CP_UI_DIM));
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(iy++,panelX+1,"$ Gold   T Oak");mvprintw(iy++,panelX+1,"^ Mtn    Y Pine");
        mvprintw(iy++,panelX+1,"~ Water  n Hills");mvprintw(iy++,panelX+1,"# Castle & Ruins");
        attroff(COLOR_PAIR(CP_UI_TEXT));iy++;
        attron(COLOR_PAIR(CP_PLAYER));
        mvprintw(iy++,panelX+1,"p Peasant  m Militia");mvprintw(iy++,panelX+1,"a Archer   k Knight");
        mvprintw(iy++,panelX+1,"c Catapult");attroff(COLOR_PAIR(CP_PLAYER));
    }

    // Bottom bars
    int botY2=maxY-2,botY1=maxY-1;
    attron(COLOR_PAIR(CP_UI_BAR));mvhline(botY2,0,' ',maxX);
    if(g.mode==M_BUILD_SELECT)
        mvprintw(botY2,1," BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [A]rmory [C]hurch [M]arket [K]Castle [Esc] ");
    else if(g.mode==M_TRAIN_SELECT){Entity* s2=findEntity(g.selectedId);if(s2){
        if(s2->type==E_TOWNHALL)mvprintw(botY2,1," TRAIN: [P]easant(50g) [Esc] ");
        else if(s2->type==E_BARRACKS)mvprintw(botY2,1," TRAIN: [M]ilitia(60g) [A]rcher(70g) [C]atapult(180g+50w) [Esc] ");
        else if(s2->type==E_STABLE)mvprintw(botY2,1," TRAIN: [K]night(120g) [Esc] ");}}
    else if(g.mode==M_PAUSED){attron(A_BOLD);mvprintw(botY2,1," PAUSED - Press [P] to resume ");attroff(A_BOLD);}
    else if(g.mode==M_GAME_OVER){attron(A_BOLD);
        if(g.winner==0)mvprintw(botY2,1," VICTORY! The realm is yours. [Q] Quit ");
        else mvprintw(botY2,1," DEFEAT! Your kingdom has fallen. [Q] Quit ");attroff(A_BOLD);}
    else mvprintw(botY2,1," Arrows:Cursor  Space:Select  Enter:Command  B:Build  T:Train  Tab:Next  H:Home  P:Pause  Q:Quit ");
    attroff(COLOR_PAIR(CP_UI_BAR));

    mvhline(botY1,0,' ',maxX);
    if(g.statusTimer>0){attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);mvprintw(botY1,1,">> %s",g.statusMsg.c_str());attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);g.statusTimer--;}
    attron(COLOR_PAIR(CP_UI_DIM));mvprintw(botY1,maxX-12,"(%d,%d)",g.cursorX,g.cursorY);attroff(COLOR_PAIR(CP_UI_DIM));
}

void render(){erase();renderMap();renderUI();refresh();}

// ============================================================
// INPUT
// ============================================================
void handleInput(int ch){
    if(ch==ERR)return;
    if(ch=='q'||ch=='Q'){endwin();exit(0);}
    if((ch=='p'||ch=='P')&&(g.mode==M_NORMAL||g.mode==M_PAUSED)){g.mode=(g.mode==M_PAUSED)?M_NORMAL:M_PAUSED;return;}
    if(g.mode==M_PAUSED||g.mode==M_GAME_OVER)return;

    if(g.mode==M_BUILD_SELECT){Entity* sel=findEntity(g.selectedId);
        if(!sel||sel->type!=E_PEASANT){g.mode=M_NORMAL;return;}EntityType tb=E_NONE;
        switch(ch){case 'h':case 'H':tb=E_HOUSE;break;case 'b':case 'B':tb=E_BARRACKS;break;
            case 's':case 'S':tb=E_STABLE;break;case 't':case 'T':tb=E_TOWER;break;
            case 'f':case 'F':tb=E_FARM;break;case 'w':case 'W':tb=E_WALL;break;
            case 'a':case 'A':tb=E_BLACKSMITH;break;case 'c':case 'C':tb=E_CHURCH;break;
            case 'm':case 'M':tb=E_MARKET;break;case 'k':case 'K':tb=E_CASTLE;break;
            case 27:g.mode=M_NORMAL;return;default:return;}
        if(tb!=E_NONE){orderBuild(*sel,tb,g.cursorX,g.cursorY);g.mode=M_NORMAL;}return;}

    if(g.mode==M_TRAIN_SELECT){Entity* sel=findEntity(g.selectedId);if(!sel){g.mode=M_NORMAL;return;}
        EntityType tt=E_NONE;
        if(sel->type==E_TOWNHALL){if(ch=='p'||ch=='P')tt=E_PEASANT;}
        else if(sel->type==E_BARRACKS){if(ch=='m'||ch=='M')tt=E_MILITIA;else if(ch=='a'||ch=='A')tt=E_ARCHER;else if(ch=='c'||ch=='C')tt=E_CATAPULT;}
        else if(sel->type==E_STABLE){if(ch=='k'||ch=='K')tt=E_KNIGHT;}
        if(tt!=E_NONE){orderTrain(*sel,tt);g.mode=M_NORMAL;}if(ch==27)g.mode=M_NORMAL;return;}

    switch(ch){
    case KEY_UP:g.cursorY--;break;case KEY_DOWN:g.cursorY++;break;
    case KEY_LEFT:g.cursorX--;break;case KEY_RIGHT:g.cursorX++;break;
    case ' ':{Entity* ent=entityAtOwner(g.cursorX,g.cursorY,0);
        if(ent){g.selectedId=ent->id;setStatus(std::string("Selected: ")+STATS[ent->type].name);}
        else{Entity* any=entityAt(g.cursorX,g.cursorY);
            if(any&&any->alive&&g.map[g.cursorY][g.cursorX].visible[0]){g.selectedId=any->id;setStatus(std::string("Enemy ")+STATS[any->type].name);}
            else g.selectedId=-1;}break;}
    case '\n':case '\r':case KEY_ENTER:{Entity* sel=findEntity(g.selectedId);
        if(!sel||sel->owner!=0||!isUnit(sel->type))break;
        Entity* tgt=entityAt(g.cursorX,g.cursorY);
        if(tgt&&tgt->alive&&tgt->owner==1&&g.map[g.cursorY][g.cursorX].visible[0]){orderAttack(*sel,tgt->id);setStatus("Attacking!");}
        else if(sel->type==E_PEASANT){Terrain ter=g.map[g.cursorY][g.cursorX].terrain;
            bool isW=(ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
            if((ter==T_GOLD||isW)&&g.map[g.cursorY][g.cursorX].resources>0){orderGather(*sel,g.cursorX,g.cursorY);setStatus(ter==T_GOLD?"Mining gold...":"Chopping wood...");}
            else{orderMove(*sel,g.cursorX,g.cursorY);setStatus("Moving...");}}
        else{orderMove(*sel,g.cursorX,g.cursorY);setStatus("Moving...");}break;}
    case 'b':case 'B':{Entity* sel=findEntity(g.selectedId);
        if(sel&&sel->owner==0&&sel->type==E_PEASANT){g.mode=M_BUILD_SELECT;setStatus("Select building to place at cursor...");}
        else setStatus("Select a peasant first!");break;}
    case 't':case 'T':{Entity* sel=findEntity(g.selectedId);
        if(sel&&sel->owner==0&&isBuilding(sel->type)&&!sel->underConstruction){
            if(sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE){g.mode=M_TRAIN_SELECT;setStatus("Select unit to train...");}
            else setStatus("This building can't train.");}else setStatus("Select a production building!");break;}
    case '\t':{int sid=g.selectedId;bool found=false,past=(sid<0);
        for(auto& e:g.entities){if(!e.alive||e.owner!=0||!isUnit(e.type))continue;
            if(!past){if(e.id==sid)past=true;continue;}g.selectedId=e.id;g.cursorX=e.x;g.cursorY=e.y;found=true;break;}
        if(!found)for(auto& e:g.entities){if(!e.alive||e.owner!=0||!isUnit(e.type))continue;
            g.selectedId=e.id;g.cursorX=e.x;g.cursorY=e.y;break;}break;}
    case 'h':case 'H':for(auto& e:g.entities)if(e.alive&&e.owner==0&&(e.type==E_TOWNHALL||e.type==E_CASTLE)){
        g.selectedId=e.id;g.cursorX=e.x+1;g.cursorY=e.y+1;break;}break;
    case 27:g.selectedId=-1;g.mode=M_NORMAL;break;}
    g.cursorX=std::max(0,std::min(g.cursorX,MAP_W-1));g.cursorY=std::max(0,std::min(g.cursorY,MAP_H-1));
}

// ============================================================
// INIT & MAIN
// ============================================================
void initGame(){
    srand((unsigned)time(nullptr));
    g.entities.reserve(512); g.projectiles.reserve(256);
    g.nextId=1;g.tick=0;g.mode=M_NORMAL;g.selectedId=-1;g.winner=-1;
    g.aiTimer=0;g.farmTimer=0;g.statusTimer=0;g.dayPhase=0.25f;g.seasonPhase=0.0f;
    g.players[0]={300,200,0,0,true};g.players[1]={300,200,0,0,true};
    g.players[OWNER_NATURE]={0,0,0,0,true};
    generateMap();
    spawnEntity(E_TOWNHALL,0,5,5);for(int i=0;i<4;i++)spawnEntity(E_PEASANT,0,9+i,9);
    spawnEntity(E_TOWNHALL,1,MAP_W-9,MAP_H-9);for(int i=0;i<4;i++)spawnEntity(E_PEASANT,1,MAP_W-8+i,MAP_H-5);
    updateSupply(0);updateSupply(1);g.cursorX=7;g.cursorY=7;g.viewX=0;g.viewY=0;
    // Wild deer in open terrain
    for(int i=0,t=0;i<25&&t<600;t++){int ax=10+rand()%(MAP_W-20),ay=10+rand()%(MAP_H-20);
        Terrain tr=g.map[ay][ax].terrain;
        if((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)&&!entityAt(ax,ay))
            {spawnEntity(E_DEER,OWNER_NATURE,ax,ay);i++;}}
    // Wolves in forested areas
    for(int i=0,t=0;i<12&&t<600;t++){int ax=10+rand()%(MAP_W-20),ay=10+rand()%(MAP_H-20);
        Terrain tr=g.map[ay][ax].terrain;
        if((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS)&&!entityAt(ax,ay))
            {spawnEntity(E_WOLF,OWNER_NATURE,ax,ay);i++;}}
    // Domestic sheep near each TC
    for(int i=0,t=0;i<5&&t<200;t++){int ax=8+(rand()%7)-3,ay=8+(rand()%7)-3;
        ax=std::max(1,std::min(ax,MAP_W-2));ay=std::max(1,std::min(ay,MAP_H-2));
        if(isPassable(ax,ay)&&!entityAt(ax,ay)){spawnEntity(E_SHEEP,OWNER_NATURE,ax,ay);i++;}}
    for(int i=0,t=0;i<5&&t<200;t++){int ax=(MAP_W-9)+(rand()%7)-3,ay=(MAP_H-9)+(rand()%7)-3;
        ax=std::max(1,std::min(ax,MAP_W-2));ay=std::max(1,std::min(ay,MAP_H-2));
        if(isPassable(ax,ay)&&!entityAt(ax,ay)){spawnEntity(E_SHEEP,OWNER_NATURE,ax,ay);i++;}}
    updateFog();
}

int main(){
    initscr();cbreak();noecho();keypad(stdscr,TRUE);curs_set(0);timeout(TICK_MS);
    initColors();initGame();
    setStatus("Dawn breaks over the realm. Select peasants [Space] and gather [Enter].");
    while(true){int ch=getch();handleInput(ch);
        if(g.mode!=M_PAUSED&&g.mode!=M_GAME_OVER){g.tick++;
            g.dayPhase+=1.0f/DAY_LENGTH;if(g.dayPhase>=1.0f)g.dayPhase-=1.0f;
            g.seasonPhase+=1.0f/SEASON_LENGTH;if(g.seasonPhase>=4.0f)g.seasonPhase-=4.0f;
            for(int i=0;i<(int)g.entities.size();i++)tickEntity(g.entities[i]);
            tickTowers();tickProjectiles();tickFarms();tickMarkets();tickChurches();tickAnimals();tickAI();updateFog();
            if(g.tick%100==0){g.entities.erase(std::remove_if(g.entities.begin(),g.entities.end(),
                [](const Entity& e){return !e.alive&&e.state==S_DEAD;}),g.entities.end());checkWin();}}
        render();}
    endwin();return 0;
}
