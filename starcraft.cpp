// SECTOR - A Starcraft-esque ASCII RTS (SDL2)
// Compile: g++ -std=c++17 -O2 -o sector starcraft.cpp $(sdl2-config --cflags --libs)
// Double-click the binary or run: ./sector

#include <SDL2/SDL.h>
#include <vector>
#include <queue>
#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <string>
#include <cstring>

// ============================================================
// EMBEDDED 6x10 BITMAP FONT (CP437-style, printable ASCII 32-126)
// Each character is 6 pixels wide, 10 pixels tall, stored as 10 bytes
// ============================================================
static const unsigned char FONT_DATA[][10] = {
    // 32 ' '
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 33 '!'
    {0x00,0x08,0x08,0x08,0x08,0x08,0x00,0x08,0x00,0x00},
    // 34 '"'
    {0x00,0x14,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 35 '#'
    {0x00,0x14,0x3E,0x14,0x14,0x3E,0x14,0x00,0x00,0x00},
    // 36 '$'
    {0x00,0x08,0x1E,0x28,0x1C,0x0A,0x3C,0x08,0x00,0x00},
    // 37 '%'
    {0x00,0x30,0x32,0x04,0x08,0x10,0x26,0x06,0x00,0x00},
    // 38 '&'
    {0x00,0x18,0x24,0x18,0x2A,0x24,0x1A,0x00,0x00,0x00},
    // 39 '''
    {0x00,0x08,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 40 '('
    {0x00,0x04,0x08,0x08,0x08,0x08,0x08,0x04,0x00,0x00},
    // 41 ')'
    {0x00,0x10,0x08,0x08,0x08,0x08,0x08,0x10,0x00,0x00},
    // 42 '*'
    {0x00,0x00,0x14,0x08,0x3E,0x08,0x14,0x00,0x00,0x00},
    // 43 '+'
    {0x00,0x00,0x08,0x08,0x3E,0x08,0x08,0x00,0x00,0x00},
    // 44 ','
    {0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x08,0x10,0x00},
    // 45 '-'
    {0x00,0x00,0x00,0x00,0x3E,0x00,0x00,0x00,0x00,0x00},
    // 46 '.'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x08,0x00,0x00},
    // 47 '/'
    {0x00,0x02,0x04,0x04,0x08,0x10,0x10,0x20,0x00,0x00},
    // 48 '0'
    {0x00,0x1C,0x22,0x26,0x2A,0x32,0x22,0x1C,0x00,0x00},
    // 49 '1'
    {0x00,0x08,0x18,0x08,0x08,0x08,0x08,0x1C,0x00,0x00},
    // 50 '2'
    {0x00,0x1C,0x22,0x02,0x0C,0x10,0x20,0x3E,0x00,0x00},
    // 51 '3'
    {0x00,0x1C,0x22,0x02,0x0C,0x02,0x22,0x1C,0x00,0x00},
    // 52 '4'
    {0x00,0x04,0x0C,0x14,0x24,0x3E,0x04,0x04,0x00,0x00},
    // 53 '5'
    {0x00,0x3E,0x20,0x3C,0x02,0x02,0x22,0x1C,0x00,0x00},
    // 54 '6'
    {0x00,0x0C,0x10,0x20,0x3C,0x22,0x22,0x1C,0x00,0x00},
    // 55 '7'
    {0x00,0x3E,0x02,0x04,0x08,0x08,0x08,0x08,0x00,0x00},
    // 56 '8'
    {0x00,0x1C,0x22,0x22,0x1C,0x22,0x22,0x1C,0x00,0x00},
    // 57 '9'
    {0x00,0x1C,0x22,0x22,0x1E,0x02,0x04,0x18,0x00,0x00},
    // 58 ':'
    {0x00,0x00,0x00,0x08,0x00,0x00,0x08,0x00,0x00,0x00},
    // 59 ';'
    {0x00,0x00,0x00,0x08,0x00,0x00,0x08,0x08,0x10,0x00},
    // 60 '<'
    {0x00,0x04,0x08,0x10,0x20,0x10,0x08,0x04,0x00,0x00},
    // 61 '='
    {0x00,0x00,0x00,0x3E,0x00,0x3E,0x00,0x00,0x00,0x00},
    // 62 '>'
    {0x00,0x10,0x08,0x04,0x02,0x04,0x08,0x10,0x00,0x00},
    // 63 '?'
    {0x00,0x1C,0x22,0x02,0x04,0x08,0x00,0x08,0x00,0x00},
    // 64 '@'
    {0x00,0x1C,0x22,0x2E,0x2A,0x2E,0x20,0x1C,0x00,0x00},
    // 65 'A'
    {0x00,0x08,0x14,0x22,0x22,0x3E,0x22,0x22,0x00,0x00},
    // 66 'B'
    {0x00,0x3C,0x22,0x22,0x3C,0x22,0x22,0x3C,0x00,0x00},
    // 67 'C'
    {0x00,0x1C,0x22,0x20,0x20,0x20,0x22,0x1C,0x00,0x00},
    // 68 'D'
    {0x00,0x3C,0x22,0x22,0x22,0x22,0x22,0x3C,0x00,0x00},
    // 69 'E'
    {0x00,0x3E,0x20,0x20,0x3C,0x20,0x20,0x3E,0x00,0x00},
    // 70 'F'
    {0x00,0x3E,0x20,0x20,0x3C,0x20,0x20,0x20,0x00,0x00},
    // 71 'G'
    {0x00,0x1C,0x22,0x20,0x2E,0x22,0x22,0x1C,0x00,0x00},
    // 72 'H'
    {0x00,0x22,0x22,0x22,0x3E,0x22,0x22,0x22,0x00,0x00},
    // 73 'I'
    {0x00,0x1C,0x08,0x08,0x08,0x08,0x08,0x1C,0x00,0x00},
    // 74 'J'
    {0x00,0x0E,0x04,0x04,0x04,0x04,0x24,0x18,0x00,0x00},
    // 75 'K'
    {0x00,0x22,0x24,0x28,0x30,0x28,0x24,0x22,0x00,0x00},
    // 76 'L'
    {0x00,0x20,0x20,0x20,0x20,0x20,0x20,0x3E,0x00,0x00},
    // 77 'M'
    {0x00,0x22,0x36,0x2A,0x2A,0x22,0x22,0x22,0x00,0x00},
    // 78 'N'
    {0x00,0x22,0x32,0x2A,0x2A,0x26,0x22,0x22,0x00,0x00},
    // 79 'O'
    {0x00,0x1C,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00},
    // 80 'P'
    {0x00,0x3C,0x22,0x22,0x3C,0x20,0x20,0x20,0x00,0x00},
    // 81 'Q'
    {0x00,0x1C,0x22,0x22,0x22,0x2A,0x24,0x1A,0x00,0x00},
    // 82 'R'
    {0x00,0x3C,0x22,0x22,0x3C,0x28,0x24,0x22,0x00,0x00},
    // 83 'S'
    {0x00,0x1C,0x22,0x20,0x1C,0x02,0x22,0x1C,0x00,0x00},
    // 84 'T'
    {0x00,0x3E,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00},
    // 85 'U'
    {0x00,0x22,0x22,0x22,0x22,0x22,0x22,0x1C,0x00,0x00},
    // 86 'V'
    {0x00,0x22,0x22,0x22,0x22,0x14,0x14,0x08,0x00,0x00},
    // 87 'W'
    {0x00,0x22,0x22,0x22,0x2A,0x2A,0x36,0x22,0x00,0x00},
    // 88 'X'
    {0x00,0x22,0x22,0x14,0x08,0x14,0x22,0x22,0x00,0x00},
    // 89 'Y'
    {0x00,0x22,0x22,0x14,0x08,0x08,0x08,0x08,0x00,0x00},
    // 90 'Z'
    {0x00,0x3E,0x02,0x04,0x08,0x10,0x20,0x3E,0x00,0x00},
    // 91 '['
    {0x00,0x1C,0x10,0x10,0x10,0x10,0x10,0x1C,0x00,0x00},
    // 92 '\'
    {0x00,0x20,0x10,0x10,0x08,0x04,0x04,0x02,0x00,0x00},
    // 93 ']'
    {0x00,0x1C,0x04,0x04,0x04,0x04,0x04,0x1C,0x00,0x00},
    // 94 '^'
    {0x00,0x08,0x14,0x22,0x00,0x00,0x00,0x00,0x00,0x00},
    // 95 '_'
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x3E,0x00,0x00},
    // 96 '`'
    {0x00,0x10,0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    // 97 'a'
    {0x00,0x00,0x00,0x1C,0x02,0x1E,0x22,0x1E,0x00,0x00},
    // 98 'b'
    {0x00,0x20,0x20,0x3C,0x22,0x22,0x22,0x3C,0x00,0x00},
    // 99 'c'
    {0x00,0x00,0x00,0x1C,0x22,0x20,0x22,0x1C,0x00,0x00},
    // 100 'd'
    {0x00,0x02,0x02,0x1E,0x22,0x22,0x22,0x1E,0x00,0x00},
    // 101 'e'
    {0x00,0x00,0x00,0x1C,0x22,0x3E,0x20,0x1C,0x00,0x00},
    // 102 'f'
    {0x00,0x0C,0x10,0x3C,0x10,0x10,0x10,0x10,0x00,0x00},
    // 103 'g'
    {0x00,0x00,0x00,0x1E,0x22,0x22,0x1E,0x02,0x1C,0x00},
    // 104 'h'
    {0x00,0x20,0x20,0x3C,0x22,0x22,0x22,0x22,0x00,0x00},
    // 105 'i'
    {0x00,0x08,0x00,0x18,0x08,0x08,0x08,0x1C,0x00,0x00},
    // 106 'j'
    {0x00,0x04,0x00,0x0C,0x04,0x04,0x04,0x24,0x18,0x00},
    // 107 'k'
    {0x00,0x20,0x20,0x24,0x28,0x30,0x28,0x24,0x00,0x00},
    // 108 'l'
    {0x00,0x18,0x08,0x08,0x08,0x08,0x08,0x1C,0x00,0x00},
    // 109 'm'
    {0x00,0x00,0x00,0x34,0x2A,0x2A,0x2A,0x22,0x00,0x00},
    // 110 'n'
    {0x00,0x00,0x00,0x3C,0x22,0x22,0x22,0x22,0x00,0x00},
    // 111 'o'
    {0x00,0x00,0x00,0x1C,0x22,0x22,0x22,0x1C,0x00,0x00},
    // 112 'p'
    {0x00,0x00,0x00,0x3C,0x22,0x22,0x3C,0x20,0x20,0x00},
    // 113 'q'
    {0x00,0x00,0x00,0x1E,0x22,0x22,0x1E,0x02,0x02,0x00},
    // 114 'r'
    {0x00,0x00,0x00,0x2C,0x30,0x20,0x20,0x20,0x00,0x00},
    // 115 's'
    {0x00,0x00,0x00,0x1E,0x20,0x1C,0x02,0x3C,0x00,0x00},
    // 116 't'
    {0x00,0x10,0x10,0x3C,0x10,0x10,0x10,0x0C,0x00,0x00},
    // 117 'u'
    {0x00,0x00,0x00,0x22,0x22,0x22,0x22,0x1E,0x00,0x00},
    // 118 'v'
    {0x00,0x00,0x00,0x22,0x22,0x22,0x14,0x08,0x00,0x00},
    // 119 'w'
    {0x00,0x00,0x00,0x22,0x2A,0x2A,0x2A,0x14,0x00,0x00},
    // 120 'x'
    {0x00,0x00,0x00,0x22,0x14,0x08,0x14,0x22,0x00,0x00},
    // 121 'y'
    {0x00,0x00,0x00,0x22,0x22,0x22,0x1E,0x02,0x1C,0x00},
    // 122 'z'
    {0x00,0x00,0x00,0x3E,0x04,0x08,0x10,0x3E,0x00,0x00},
    // 123 '{'
    {0x00,0x04,0x08,0x08,0x10,0x08,0x08,0x04,0x00,0x00},
    // 124 '|'
    {0x00,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x00,0x00},
    // 125 '}'
    {0x00,0x10,0x08,0x08,0x04,0x08,0x08,0x10,0x00,0x00},
    // 126 '~'
    {0x00,0x00,0x00,0x10,0x2A,0x04,0x00,0x00,0x00,0x00},
};

// ============================================================
// CONSTANTS
// ============================================================
const int CHAR_W = 8;   // pixel width per cell
const int CHAR_H = 12;  // pixel height per cell
const int GRID_W = 130; // cells across window
const int GRID_H = 55;  // cells down window
const int WIN_W = GRID_W * CHAR_W;
const int WIN_H = GRID_H * CHAR_H;

const int MAP_W = 120;
const int MAP_H = 80;
const int TICK_MS = 80;
const int FOG_RADIUS = 7;
const int GATHER_RATE = 8;
const int GATHER_TICKS = 15;

// ============================================================
// COLORS
// ============================================================
struct Color { Uint8 r, g, b; };

const Color COL_BLACK    = {0, 0, 0};
const Color COL_DGRAY    = {40, 40, 50};
const Color COL_GRAY     = {100, 100, 110};
const Color COL_WHITE    = {200, 200, 210};
const Color COL_BRIGHT   = {255, 255, 255};
const Color COL_GREEN    = {0, 200, 0};
const Color COL_DGREEN   = {0, 100, 0};
const Color COL_BLUE     = {60, 60, 220};
const Color COL_CYAN     = {0, 200, 220};
const Color COL_TEAL     = {0, 150, 160};
const Color COL_RED      = {220, 40, 40};
const Color COL_ORANGE   = {220, 140, 0};
const Color COL_YELLOW   = {220, 220, 0};
const Color COL_PURPLE   = {160, 0, 200};
const Color COL_MINERAL  = {80, 120, 255};
const Color COL_GAS      = {0, 220, 80};
const Color COL_LAVA     = {180, 60, 0};
const Color COL_PLAYER   = {0, 180, 255};
const Color COL_ENEMY    = {255, 50, 50};
const Color COL_FOG      = {30, 30, 40};
const Color COL_EXPLORED = {50, 50, 60};
const Color COL_VOID     = {10, 8, 20};
const Color COL_STAR     = {140, 140, 160};
const Color COL_UI_BG    = {20, 20, 35};
const Color COL_UI_BORDER= {60, 60, 100};
const Color COL_UI_TEXT  = {180, 180, 200};
const Color COL_UI_HIGH  = {255, 220, 80};

// ============================================================
// ENUMS
// ============================================================
enum Terrain { T_VOID, T_GROUND, T_MINERALS, T_VESPENE, T_ROCK, T_LAVA, T_DEBRIS };

enum EntityType {
    E_NONE = 0,
    E_SCV, E_MARINE, E_FIREBAT, E_MEDIC, E_GHOST, E_SIEGE_TANK,
    E_COMMAND_CENTER, E_SUPPLY_DEPOT, E_BARRACKS, E_FACTORY,
    E_ENGINEERING_BAY, E_BUNKER, E_MISSILE_TURRET, E_REFINERY
};

enum EntityState {
    S_IDLE, S_MOVING, S_ATTACKING, S_GATHERING, S_BUILDING, S_TRAINING, S_DEAD
};

enum GameMode {
    M_NORMAL, M_BUILD_SELECT, M_TRAIN_SELECT, M_PAUSED, M_GAME_OVER
};

// ============================================================
// STATS
// ============================================================
struct EntityStats {
    const char* name;
    char glyph;
    int maxHp, atk, range, speed, atkSpeed;
    int costMin, costGas;
    int trainTime;
    int sizeW, sizeH;
    int supplyProvided, supplyUsed;
    bool isBuilding;
};

static const EntityStats STATS[] = {
    {"None",           ' ',   0, 0,0,0,0,  0,0,  0, 1,1, 0,0, false},
    // E_SCV
    {"SCV",            's',  60, 5,1,3,8, 50,0, 30, 1,1, 0,1, false},
    // E_MARINE
    {"Marine",         'm',  40, 6,5,3,5, 50,0, 40, 1,1, 0,1, false},
    // E_FIREBAT
    {"Firebat",        'f',  50,16,1,3,6, 50,25,40, 1,1, 0,1, false},
    // E_MEDIC
    {"Medic",          '+',  60,-4,2,3,6, 50,25,50, 1,1, 0,1, false},
    // E_GHOST
    {"Ghost",          'g',  45,10,7,2,7, 75,75,80, 1,1, 0,1, false},
    // E_SIEGE_TANK
    {"Siege Tank",     'T',  150,30,8,4,10,150,100,80,1,1, 0,2, false},
    // E_COMMAND_CENTER
    {"Command Center", 'C', 200, 0,0,0,0,  0,0,  0, 4,3, 10,0, true},
    // E_SUPPLY_DEPOT
    {"Supply Depot",   'D', 100, 0,0,0,0,  0,0, 50, 2,2, 8,0, true},
    // E_BARRACKS
    {"Barracks",       'B', 120, 0,0,0,0,  0,0, 70, 3,2, 0,0, true},
    // E_FACTORY
    {"Factory",        'F', 150, 0,0,0,0,  0,0,100, 3,2, 0,0, true},
    // E_ENGINEERING_BAY
    {"Eng. Bay",       'E', 100, 0,0,0,0,  0,0, 60, 3,2, 0,0, true},
    // E_BUNKER
    {"Bunker",         '#', 120,12,6,0,6,  0,0, 50, 2,2, 0,0, true},
    // E_MISSILE_TURRET
    {"Missile Turret", 'X', 100,14,7,0,6, 75,0, 40, 1,1, 0,0, true},
    // E_REFINERY
    {"Refinery",       'R',  80, 0,0,0,0,  0,0, 50, 2,2, 0,0, true},
};

bool isUnit(EntityType t) { return t >= E_SCV && t <= E_SIEGE_TANK; }
bool isBldg(EntityType t) { return t >= E_COMMAND_CENTER && t <= E_REFINERY; }

// ============================================================
// DATA STRUCTURES
// ============================================================
struct Tile {
    Terrain terrain;
    int resources;
    bool visible[2];
    bool explored[2];
    bool hasStar; // decorative
};

struct Entity {
    int id;
    EntityType type;
    int owner;
    int x, y;
    int hp, maxHp;
    EntityState state;
    int targetId, targetX, targetY;
    std::vector<std::pair<int,int>> path;
    int pathIdx;
    int moveCd, atkCd, gatherCd, gatherType;
    EntityType producing;
    int prodProgress, prodTime;
    bool underConstruction;
    bool alive;
    int rallyX, rallyY;
};

struct Player {
    int minerals, gas;
    int supply, supplyMax;
    bool alive;
};

struct Game {
    Tile map[MAP_H][MAP_W];
    std::vector<Entity> entities;
    int nextId;
    Player players[2];
    int tick;
    GameMode mode;
    int cursorX, cursorY;
    int viewX, viewY, viewW, viewH;
    int selectedId;
    std::string statusMsg;
    int statusTimer;
    int winner;
    int aiTimer;
    // SDL
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* fontTex;
    bool running;
};

static Game g;

// ============================================================
// HELPERS
// ============================================================
int dist(int x1, int y1, int x2, int y2) { return std::max(std::abs(x1-x2), std::abs(y1-y2)); }
int mdist(int x1, int y1, int x2, int y2) { return std::abs(x1-x2) + std::abs(y1-y2); }
bool inBounds(int x, int y) { return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H; }
bool isPassable(int x, int y) {
    if (!inBounds(x,y)) return false;
    Terrain t = g.map[y][x].terrain;
    return t != T_ROCK && t != T_LAVA;
}
void setStatus(const std::string& msg) { g.statusMsg = msg; g.statusTimer = 40; }

Entity* findEntity(int id) {
    for (auto& e : g.entities) if (e.id == id && e.alive) return &e;
    return nullptr;
}

Entity* entityAt(int x, int y) {
    for (auto& e : g.entities) {
        if (!e.alive) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) {
            if (x >= e.x && x < e.x+s.sizeW && y >= e.y && y < e.y+s.sizeH) return &e;
        } else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

Entity* entityAtOwner(int x, int y, int owner) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) {
            if (x >= e.x && x < e.x+s.sizeW && y >= e.y && y < e.y+s.sizeH) return &e;
        } else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

bool canPlace(EntityType type, int x, int y) {
    auto& s = STATS[type];
    for (int dy = 0; dy < s.sizeH; dy++)
        for (int dx = 0; dx < s.sizeW; dx++) {
            int nx = x+dx, ny = y+dy;
            if (!inBounds(nx,ny) || !isPassable(nx,ny)) return false;
            if (g.map[ny][nx].terrain == T_MINERALS || g.map[ny][nx].terrain == T_VESPENE) return false;
            if (entityAt(nx,ny)) return false;
        }
    return true;
}

void updateSupply(int owner) {
    int mx = 0, used = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;
        if (!e.underConstruction) mx += STATS[e.type].supplyProvided;
        used += STATS[e.type].supplyUsed;
    }
    g.players[owner].supplyMax = mx;
    g.players[owner].supply = used;
}

// ============================================================
// PATHFINDING
// ============================================================
std::vector<std::pair<int,int>> findPath(int sx, int sy, int tx, int ty, int maxSteps = 200) {
    if (sx == tx && sy == ty) return {};
    if (!isPassable(tx,ty)) {
        int bestD = 9999, bx = tx, by = ty;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++) {
                int nx = tx+dx, ny = ty+dy;
                if (isPassable(nx,ny)) { int d = mdist(sx,sy,nx,ny); if (d<bestD){bestD=d;bx=nx;by=ny;} }
            }
        tx=bx; ty=by;
    }
    static int visited[MAP_H][MAP_W]; static int vgen = 0; vgen++;
    struct Node { int x,y,px,py; };
    std::queue<Node> q; q.push({sx,sy,-1,-1}); visited[sy][sx] = vgen;
    static Node hist[MAP_W*MAP_H]; int hc = 0; bool found = false; int fi = -1;
    while (!q.empty() && hc < maxSteps*4) {
        Node cur = q.front(); q.pop(); int idx = hc; hist[hc++] = cur;
        if (cur.x==tx && cur.y==ty) { found=true; fi=idx; break; }
        static const int dx8[]={0,1,1,1,0,-1,-1,-1}, dy8[]={-1,-1,0,1,1,1,0,-1};
        for (int i=0;i<8;i++) {
            int nx=cur.x+dx8[i], ny=cur.y+dy8[i];
            if (!inBounds(nx,ny)||visited[ny][nx]==vgen) continue;
            if (!isPassable(nx,ny)&&!(nx==tx&&ny==ty)) continue;
            Entity* occ=entityAt(nx,ny);
            if (occ&&isBldg(occ->type)&&!(nx==tx&&ny==ty)) continue;
            visited[ny][nx]=vgen; q.push({nx,ny,cur.x,cur.y});
        }
    }
    if (!found) return {};
    std::vector<std::pair<int,int>> path;
    int cx=hist[fi].x, cy=hist[fi].y;
    path.push_back({cx,cy}); int px=hist[fi].px, py=hist[fi].py;
    while (px!=-1) {
        path.push_back({px,py});
        for (int i=fi-1;i>=0;i--) {
            if (hist[i].x==px&&hist[i].y==py) { int npx=hist[i].px,npy=hist[i].py; px=npx;py=npy;fi=i;break; }
        }
        if ((int)path.size()>500) break;
    }
    std::reverse(path.begin(),path.end());
    if (!path.empty()) path.erase(path.begin());
    return path;
}

// ============================================================
// MAP GEN
// ============================================================
void generateMap() {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            g.map[y][x] = {T_GROUND, 0, {false,false}, {false,false}, (rand()%30==0)};
        }
    // Void border
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (x < 2 || x >= MAP_W-2 || y < 2 || y >= MAP_H-2)
                g.map[y][x].terrain = T_VOID;

    // Rock formations
    for (int i = 0; i < 20; i++) {
        int cx = 10+rand()%(MAP_W-20), cy = 10+rand()%(MAP_H-20);
        int sz = 2+rand()%4;
        for (int j = 0; j < sz*sz; j++) {
            int rx = cx+(rand()%(sz*2+1))-sz, ry = cy+(rand()%(sz*2+1))-sz;
            if (inBounds(rx,ry) && rand()%2==0) g.map[ry][rx].terrain = T_ROCK;
        }
    }
    // Lava rivers
    for (int i = 0; i < 5; i++) {
        int cx = 20+rand()%(MAP_W-40), cy = 20+rand()%(MAP_H-40);
        int len = 6+rand()%12; int dx=(rand()%3)-1, dy=(rand()%3)-1;
        if (!dx&&!dy) dx=1;
        for (int j=0;j<len;j++) {
            int lx=cx+dx*j+(rand()%3-1), ly=cy+dy*j+(rand()%3-1);
            if (inBounds(lx,ly)) g.map[ly][lx].terrain = T_LAVA;
        }
    }
    // Mineral patches
    auto placeMinerals = [](int cx, int cy, int count) {
        for (int i=0;i<count;i++) {
            int mx=cx+(rand()%7)-3, my=cy+(rand()%5)-2;
            if (inBounds(mx,my)&&g.map[my][mx].terrain==T_GROUND) {
                g.map[my][mx].terrain = T_MINERALS;
                g.map[my][mx].resources = 400+rand()%300;
            }
        }
    };
    auto placeGas = [](int cx, int cy) {
        if (inBounds(cx,cy)) {
            g.map[cy][cx].terrain = T_VESPENE;
            g.map[cy][cx].resources = 500;
        }
    };
    // Near bases
    placeMinerals(12, 10, 6); placeGas(16, 7); placeGas(8, 13);
    placeMinerals(MAP_W-14, MAP_H-12, 6); placeGas(MAP_W-18, MAP_H-9); placeGas(MAP_W-10, MAP_H-15);
    // Expansions
    for (int i = 0; i < 6; i++) {
        int ex = 15+rand()%(MAP_W-30), ey = 15+rand()%(MAP_H-30);
        placeMinerals(ex, ey, 4+rand()%3);
        placeGas(ex+5, ey+2);
    }
    // Clear start areas
    auto clearArea = [](int cx, int cy, int r) {
        for (int dy=-r;dy<=r+4;dy++) for (int dx=-r;dx<=r+4;dx++) {
            int x=cx+dx,y=cy+dy;
            if (inBounds(x,y)&&g.map[y][x].terrain!=T_MINERALS&&g.map[y][x].terrain!=T_VESPENE)
                g.map[y][x].terrain = T_GROUND;
        }
    };
    clearArea(5,5,5); clearArea(MAP_W-10,MAP_H-10,5);
    // Debris (decoration)
    for (int i=0;i<60;i++) {
        int x=3+rand()%(MAP_W-6), y=3+rand()%(MAP_H-6);
        if (g.map[y][x].terrain==T_GROUND) g.map[y][x].terrain = T_DEBRIS;
    }
}

// ============================================================
// ENTITY CREATION
// ============================================================
int spawnEntity(EntityType type, int owner, int x, int y, bool built=true) {
    Entity e{}; e.id = g.nextId++; e.type = type; e.owner = owner;
    e.x = x; e.y = y; e.maxHp = STATS[type].maxHp;
    e.hp = built ? e.maxHp : 1; e.state = S_IDLE;
    e.targetId = -1; e.targetX = -1; e.targetY = -1;
    e.pathIdx = 0; e.underConstruction = !built; e.alive = true;
    e.producing = E_NONE;
    e.rallyX = x+STATS[type].sizeW; e.rallyY = y+STATS[type].sizeH;
    g.entities.push_back(e);
    updateSupply(owner);
    return e.id;
}

// ============================================================
// FOG OF WAR
// ============================================================
void updateFog() {
    for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
        g.map[y][x].visible[0]=false; g.map[y][x].visible[1]=false;
    }
    for (auto& e : g.entities) {
        if (!e.alive) continue;
        int r = FOG_RADIUS;
        if (isBldg(e.type)) r += 2;
        if (e.type == E_MISSILE_TURRET) r += 4;
        if (e.type == E_GHOST) r += 2;
        auto& s = STATS[e.type];
        int cx = e.x+s.sizeW/2, cy = e.y+s.sizeH/2;
        for (int dy=-r;dy<=r;dy++) for (int dx=-r;dx<=r;dx++) {
            int nx=cx+dx,ny=cy+dy;
            if (inBounds(nx,ny)&&dx*dx+dy*dy<=r*r) {
                g.map[ny][nx].visible[e.owner]=true;
                g.map[ny][nx].explored[e.owner]=true;
            }
        }
    }
}

// ============================================================
// ORDERS
// ============================================================
Entity* findNearestEnemy(Entity& e, int range) {
    Entity* best=nullptr; int bestD=range+1;
    for (auto& o:g.entities) { if (!o.alive||o.owner==e.owner) continue;
        int d=dist(e.x,e.y,o.x,o.y); if (d<bestD){bestD=d;best=&o;} }
    return best;
}

void orderMove(Entity& e, int tx, int ty) {
    e.state=S_MOVING; e.targetX=tx; e.targetY=ty; e.targetId=-1;
    e.path=findPath(e.x,e.y,tx,ty); e.pathIdx=0;
}
void orderAttack(Entity& e, int tid) {
    Entity* t=findEntity(tid); if (!t) return;
    e.state=S_ATTACKING; e.targetId=tid;
}
void orderGather(Entity& e, int tx, int ty) {
    if (e.type!=E_SCV) return;
    Terrain ter=g.map[ty][tx].terrain;
    if (ter!=T_MINERALS&&ter!=T_VESPENE) return;
    e.state=S_GATHERING; e.targetX=tx; e.targetY=ty;
    e.gatherType=(ter==T_MINERALS)?0:1;
    e.path=findPath(e.x,e.y,tx,ty); e.pathIdx=0; e.gatherCd=0;
}
void orderBuild(Entity& e, EntityType bt, int bx, int by) {
    if (e.type!=E_SCV) return;
    Player& p=g.players[e.owner];
    if (p.minerals<STATS[bt].costMin||p.gas<STATS[bt].costGas) { if (e.owner==0) setStatus("Not enough resources!"); return; }
    if (!canPlace(bt,bx,by)) { if (e.owner==0) setStatus("Can't build there!"); return; }
    p.minerals-=STATS[bt].costMin; p.gas-=STATS[bt].costGas;
    int bid=spawnEntity(bt,e.owner,bx,by,false);
    e.state=S_BUILDING; e.targetId=bid; e.targetX=bx; e.targetY=by;
    e.path=findPath(e.x,e.y,bx-1,by); e.pathIdx=0;
}
void orderTrain(Entity& bld, EntityType ut) {
    if (!isBldg(bld.type)||bld.underConstruction) return;
    if (bld.producing!=E_NONE) { if (bld.owner==0) setStatus("Already training!"); return; }
    Player& p=g.players[bld.owner];
    if (p.minerals<STATS[ut].costMin||p.gas<STATS[ut].costGas) { if (bld.owner==0) setStatus("Not enough resources!"); return; }
    if (p.supply+STATS[ut].supplyUsed>p.supplyMax) { if (bld.owner==0) setStatus("Need more supply depots!"); return; }
    p.minerals-=STATS[ut].costMin; p.gas-=STATS[ut].costGas;
    bld.producing=ut; bld.prodProgress=0; bld.prodTime=STATS[ut].trainTime; bld.state=S_TRAINING;
}

// ============================================================
// GAME LOGIC
// ============================================================
void moveAlongPath(Entity& e) {
    if (e.pathIdx>=(int)e.path.size()) { e.path.clear(); e.pathIdx=0; if (e.state==S_MOVING) e.state=S_IDLE; return; }
    if (e.moveCd>0) { e.moveCd--; return; }
    auto [nx,ny] = e.path[e.pathIdx];
    Entity* blk = entityAt(nx,ny);
    if (blk&&blk->id!=e.id) {
        if (e.state==S_MOVING||e.state==S_GATHERING) {
            e.path=findPath(e.x,e.y,e.path.back().first,e.path.back().second);
            e.pathIdx=0;
        }
        return;
    }
    e.x=nx; e.y=ny; e.pathIdx++; e.moveCd=STATS[e.type].speed;
}

void tickEntity(Entity& e) {
    if (!e.alive) return;
    // Production
    if (e.producing!=E_NONE&&!e.underConstruction) {
        e.prodProgress++;
        if (e.prodProgress>=e.prodTime) {
            auto& bs=STATS[e.type]; bool placed=false;
            for (int r=0;r<=3&&!placed;r++)
                for (int dy=-r;dy<=bs.sizeH+r&&!placed;dy++)
                    for (int dx=-r;dx<=bs.sizeW+r&&!placed;dx++) {
                        int nx=e.x+dx,ny=e.y+dy;
                        if (!inBounds(nx,ny)||!isPassable(nx,ny)||entityAt(nx,ny)) continue;
                        spawnEntity(e.producing,e.owner,nx,ny); placed=true;
                    }
            e.producing=E_NONE; e.state=S_IDLE;
            if (e.owner==0&&placed) setStatus("Unit ready!");
        }
    }
    // Construction
    if (e.underConstruction) {
        bool hasBuilder=false;
        for (auto& o:g.entities)
            if (o.alive&&o.owner==e.owner&&o.state==S_BUILDING&&o.targetId==e.id)
                if (dist(o.x,o.y,e.x,e.y)<=STATS[e.type].sizeW+1) hasBuilder=true;
        if (hasBuilder) {
            e.hp+=2;
            if (e.hp>=e.maxHp) {
                e.hp=e.maxHp; e.underConstruction=false; updateSupply(e.owner);
                if (e.owner==0) setStatus(std::string(STATS[e.type].name)+" online!");
                for (auto& o:g.entities) if (o.alive&&o.state==S_BUILDING&&o.targetId==e.id) o.state=S_IDLE;
            }
        }
        return;
    }
    if (!isUnit(e.type)) return;

    switch (e.state) {
    case S_IDLE: {
        if (e.type!=E_SCV&&e.type!=E_MEDIC) {
            Entity* en=findNearestEnemy(e,STATS[e.type].range+1);
            if (en) orderAttack(e,en->id);
        }
        // Medic auto-heal
        if (e.type==E_MEDIC) {
            for (auto& o:g.entities) {
                if (o.alive&&o.owner==e.owner&&isUnit(o.type)&&o.hp<o.maxHp&&dist(e.x,e.y,o.x,o.y)<=2) {
                    o.hp=std::min(o.maxHp,o.hp+2); break;
                }
            }
        }
        break;
    }
    case S_MOVING: moveAlongPath(e); if (e.path.empty()||e.pathIdx>=(int)e.path.size()) e.state=S_IDLE; break;
    case S_ATTACKING: {
        Entity* t=findEntity(e.targetId);
        if (!t||!t->alive) { e.state=S_IDLE; break; }
        int d=dist(e.x,e.y,t->x,t->y);
        if (d<=STATS[e.type].range) {
            if (e.atkCd<=0) {
                t->hp-=STATS[e.type].atk; e.atkCd=STATS[e.type].atkSpeed;
                if (t->hp<=0) { t->alive=false; t->state=S_DEAD; e.state=S_IDLE; updateSupply(t->owner); }
            } else e.atkCd--;
        } else {
            if (e.path.empty()||e.pathIdx>=(int)e.path.size()) { e.path=findPath(e.x,e.y,t->x,t->y); e.pathIdx=0; }
            moveAlongPath(e);
        }
        break;
    }
    case S_GATHERING: {
        int d=dist(e.x,e.y,e.targetX,e.targetY);
        if (d<=1) {
            Tile& tile=g.map[e.targetY][e.targetX];
            if ((tile.terrain==T_MINERALS||tile.terrain==T_VESPENE)&&tile.resources>0) {
                e.gatherCd++;
                if (e.gatherCd>=GATHER_TICKS) {
                    e.gatherCd=0; int amt=std::min(GATHER_RATE,tile.resources); tile.resources-=amt;
                    if (e.gatherType==0) g.players[e.owner].minerals+=amt;
                    else g.players[e.owner].gas+=amt;
                    if (tile.resources<=0) { tile.terrain=T_DEBRIS; e.state=S_IDLE; }
                }
            } else e.state=S_IDLE;
        } else {
            moveAlongPath(e);
            if (e.path.empty()&&dist(e.x,e.y,e.targetX,e.targetY)>1) {
                e.path=findPath(e.x,e.y,e.targetX,e.targetY); e.pathIdx=0;
                if (e.path.empty()) e.state=S_IDLE;
            }
        }
        break;
    }
    case S_BUILDING: {
        Entity* bld=findEntity(e.targetId);
        if (!bld||!bld->alive||!bld->underConstruction) { e.state=S_IDLE; break; }
        int d=dist(e.x,e.y,bld->x,bld->y);
        if (d>STATS[bld->type].sizeW+1) {
            moveAlongPath(e);
            if (e.path.empty()) { e.path=findPath(e.x,e.y,bld->x-1,bld->y); e.pathIdx=0; }
        }
        break;
    }
    default: break;
    }
}

void tickDefenses() {
    for (auto& e:g.entities) {
        if (!e.alive||e.underConstruction) continue;
        if (e.type!=E_BUNKER&&e.type!=E_MISSILE_TURRET) continue;
        Entity* en=findNearestEnemy(e,STATS[e.type].range);
        if (en) {
            if (e.atkCd<=0) {
                en->hp-=STATS[e.type].atk; e.atkCd=STATS[e.type].atkSpeed;
                if (en->hp<=0) { en->alive=false; en->state=S_DEAD; updateSupply(en->owner); }
            } else e.atkCd--;
        }
    }
}

void checkWin() {
    for (int p=0;p<2;p++) {
        bool hasCC=false;
        for (auto& e:g.entities) if (e.alive&&e.owner==p&&e.type==E_COMMAND_CENTER) hasCC=true;
        if (!hasCC) { g.players[p].alive=false; g.winner=1-p; g.mode=M_GAME_OVER; }
    }
}

// ============================================================
// AI
// ============================================================
int aiCount(int owner, EntityType t) { int c=0; for (auto& e:g.entities) if (e.alive&&e.owner==owner&&e.type==t&&!e.underConstruction) c++; return c; }
int aiCountAll(int owner, EntityType t) { int c=0; for (auto& e:g.entities) if (e.alive&&e.owner==owner&&e.type==t) c++; return c; }
Entity* aiIdle(int owner, EntityType t) { for (auto& e:g.entities) if (e.alive&&e.owner==owner&&e.type==t&&e.state==S_IDLE&&!e.underConstruction) return &e; return nullptr; }
Entity* aiBldg(int owner, EntityType t) { for (auto& e:g.entities) if (e.alive&&e.owner==owner&&e.type==t&&!e.underConstruction) return &e; return nullptr; }

void aiGather(int owner) {
    for (auto& e:g.entities) {
        if (!e.alive||e.owner!=owner||e.type!=E_SCV||e.state!=S_IDLE) continue;
        int bestD=9999; int bx=-1,by=-1;
        for (int y=0;y<MAP_H;y++) for (int x=0;x<MAP_W;x++) {
            auto& t=g.map[y][x];
            if ((t.terrain==T_MINERALS||t.terrain==T_VESPENE)&&t.resources>0) {
                int d=mdist(e.x,e.y,x,y); if (d<bestD){bestD=d;bx=x;by=y;}
            }
        }
        if (bx>=0) orderGather(e,bx,by);
    }
}

void aiBuildSpot(int owner, EntityType bt, int& ox, int& oy) {
    Entity* cc=aiBldg(owner,E_COMMAND_CENTER); if (!cc) return;
    for (int r=3;r<15;r++) for (int a=0;a<20;a++) {
        int bx=cc->x+(rand()%(r*2+1))-r, by=cc->y+(rand()%(r*2+1))-r;
        if (canPlace(bt,bx,by)) { ox=bx; oy=by; return; }
    }
}

void tickAI() {
    g.aiTimer++; if (g.aiTimer<15) return; g.aiTimer=0;
    int o=1; Player& p=g.players[o];
    int scvs=aiCount(o,E_SCV), marines=aiCount(o,E_MARINE), firebats=aiCount(o,E_FIREBAT);
    int ghosts=aiCount(o,E_GHOST), tanks=aiCount(o,E_SIEGE_TANK);
    int depots=aiCountAll(o,E_SUPPLY_DEPOT), barracks=aiCount(o,E_BARRACKS);
    int factories=aiCount(o,E_FACTORY);

    aiGather(o);

    // SCVs
    if (scvs<6) { Entity* cc=aiBldg(o,E_COMMAND_CENTER); if (cc&&cc->producing==E_NONE&&p.minerals>=50) orderTrain(*cc,E_SCV); }
    // Supply
    if (p.supply+2>=p.supplyMax&&depots<6&&p.minerals>=STATS[E_SUPPLY_DEPOT].costMin) {
        Entity* b=aiIdle(o,E_SCV); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_SUPPLY_DEPOT,bx,by); if (bx>=0) orderBuild(*b,E_SUPPLY_DEPOT,bx,by); }
    }
    // Barracks
    if (barracks==0&&p.minerals>=STATS[E_BARRACKS].costMin&&scvs>=3) {
        Entity* b=aiIdle(o,E_SCV); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if (bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    // Marines/Firebats
    if (barracks>0) {
        Entity* br=aiBldg(o,E_BARRACKS);
        if (br&&br->producing==E_NONE) {
            if (marines<5&&p.minerals>=50) orderTrain(*br,E_MARINE);
            else if (firebats<3&&p.minerals>=50&&p.gas>=25) orderTrain(*br,E_FIREBAT);
        }
    }
    // Factory
    if (factories==0&&marines>=4&&p.minerals>=STATS[E_FACTORY].costMin) {
        Entity* b=aiIdle(o,E_SCV); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FACTORY,bx,by); if (bx>=0) orderBuild(*b,E_FACTORY,bx,by); }
    }
    // Tanks
    if (factories>0) {
        Entity* f=aiBldg(o,E_FACTORY);
        if (f&&f->producing==E_NONE&&tanks<2&&p.minerals>=150&&p.gas>=100) orderTrain(*f,E_SIEGE_TANK);
    }
    // Turrets
    int turrets=aiCountAll(o,E_MISSILE_TURRET);
    if (turrets<2&&p.minerals>=75&&barracks>0) {
        Entity* b=aiIdle(o,E_SCV); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MISSILE_TURRET,bx,by); if (bx>=0) orderBuild(*b,E_MISSILE_TURRET,bx,by); }
    }
    // Attack
    int army=marines+firebats+ghosts+tanks;
    if (army>=7) {
        Entity* pt=nullptr;
        for (auto& e:g.entities) if (e.alive&&e.owner==0) { if (e.type==E_COMMAND_CENTER){pt=&e;break;} pt=&e; }
        if (pt) for (auto& e:g.entities) {
            if (e.alive&&e.owner==o&&isUnit(e.type)&&e.type!=E_SCV&&e.state==S_IDLE)
                orderAttack(e,pt->id);
        }
    }
    // Defend
    Entity* cc=aiBldg(o,E_COMMAND_CENTER);
    if (cc) for (auto& en:g.entities) {
        if (!en.alive||en.owner==o) continue;
        if (dist(en.x,en.y,cc->x,cc->y)<15) {
            for (auto& d:g.entities)
                if (d.alive&&d.owner==o&&isUnit(d.type)&&d.type!=E_SCV&&d.state==S_IDLE)
                    orderAttack(d,en.id);
            break;
        }
    }
}

// ============================================================
// SDL RENDERING
// ============================================================
void initSDL() {
    SDL_Init(SDL_INIT_VIDEO);
    g.window = SDL_CreateWindow("SECTOR - ASCII RTS",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    g.renderer = SDL_CreateRenderer(g.window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    SDL_SetRenderDrawBlendMode(g.renderer, SDL_BLENDMODE_BLEND);
}

void drawChar(int gx, int gy, char ch, Color fg, Color bg = COL_BLACK) {
    int px = gx * CHAR_W;
    int py = gy * CHAR_H;
    // Background
    if (bg.r || bg.g || bg.b) {
        SDL_SetRenderDrawColor(g.renderer, bg.r, bg.g, bg.b, 255);
        SDL_Rect r = {px, py, CHAR_W, CHAR_H};
        SDL_RenderFillRect(g.renderer, &r);
    }
    // Character
    if (ch < 32 || ch > 126) return;
    const unsigned char* glyph = FONT_DATA[ch - 32];
    SDL_SetRenderDrawColor(g.renderer, fg.r, fg.g, fg.b, 255);
    for (int row = 0; row < 10; row++) {
        unsigned char bits = glyph[row];
        for (int col = 0; col < 6; col++) {
            if (bits & (0x20 >> col)) {
                SDL_RenderDrawPoint(g.renderer, px + col + 1, py + row + 1);
            }
        }
    }
}

void drawString(int gx, int gy, const char* str, Color fg, Color bg = COL_BLACK) {
    for (int i = 0; str[i]; i++)
        drawChar(gx + i, gy, str[i], fg, bg);
}

void drawStringN(int gx, int gy, const char* str, int maxLen, Color fg, Color bg = COL_BLACK) {
    for (int i = 0; str[i] && i < maxLen; i++)
        drawChar(gx + i, gy, str[i], fg, bg);
}

void drawHLine(int gx, int gy, int len, char ch, Color fg) {
    for (int i = 0; i < len; i++) drawChar(gx+i, gy, ch, fg);
}

void renderGame() {
    SDL_SetRenderDrawColor(g.renderer, 0, 0, 0, 255);
    SDL_RenderClear(g.renderer);

    int scrW, scrH;
    SDL_GetRendererOutputSize(g.renderer, &scrW, &scrH);
    int gridW = scrW / CHAR_W;
    int gridH = scrH / CHAR_H;

    int panelW = 22;
    g.viewW = gridW - panelW - 1;
    g.viewH = gridH - 3;
    if (g.viewW < 20) g.viewW = gridW - 2;
    if (g.viewH < 10) g.viewH = gridH - 2;

    // Auto-scroll
    if (g.cursorX < g.viewX+3) g.viewX = g.cursorX-3;
    if (g.cursorX > g.viewX+g.viewW-4) g.viewX = g.cursorX-g.viewW+4;
    if (g.cursorY < g.viewY+2) g.viewY = g.cursorY-2;
    if (g.cursorY > g.viewY+g.viewH-3) g.viewY = g.cursorY-g.viewH+3;
    g.viewX = std::max(0, std::min(g.viewX, MAP_W - g.viewW));
    g.viewY = std::max(0, std::min(g.viewY, MAP_H - g.viewH));

    // === TOP BAR ===
    for (int x = 0; x < gridW; x++) drawChar(x, 0, ' ', COL_UI_TEXT, COL_UI_BG);
    char topbar[256];
    snprintf(topbar, sizeof(topbar), " SECTOR  Min: %d  Gas: %d  Supply: %d/%d",
        g.players[0].minerals, g.players[0].gas, g.players[0].supply, g.players[0].supplyMax);
    drawString(0, 0, topbar, COL_UI_HIGH, COL_UI_BG);

    // === MAP ===
    for (int sy = 0; sy < g.viewH; sy++) {
        int my = g.viewY + sy;
        for (int sx = 0; sx < g.viewW; sx++) {
            int mx = g.viewX + sx;
            int gy_pos = sy + 1;
            int gx_pos = sx;

            if (!inBounds(mx, my)) { drawChar(gx_pos, gy_pos, ' ', COL_BLACK); continue; }
            Tile& tile = g.map[my][mx];
            bool vis = tile.visible[0], expl = tile.explored[0];

            if (!expl) { drawChar(gx_pos, gy_pos, ' ', COL_VOID, COL_VOID); continue; }

            char ch = ' '; Color fg = COL_GRAY; Color bg = COL_BLACK;
            switch (tile.terrain) {
                case T_VOID:     ch = ' '; fg = COL_VOID; break;
                case T_GROUND:   ch = tile.hasStar ? '.' : ' '; fg = COL_DGRAY; break;
                case T_MINERALS: ch = '*'; fg = COL_MINERAL; break;
                case T_VESPENE:  ch = '&'; fg = COL_GAS; break;
                case T_ROCK:     ch = '#'; fg = COL_GRAY; break;
                case T_LAVA:     ch = '~'; fg = COL_LAVA; bg = {40,10,0}; break;
                case T_DEBRIS:   ch = ','; fg = COL_DGRAY; break;
            }

            if (!vis) {
                // Explored but not visible - dim
                fg = COL_EXPLORED;
                bg = COL_BLACK;
            }

            // Entity
            Entity* ent = vis ? entityAt(mx, my) : nullptr;
            if (ent && ent->alive) {
                ch = STATS[ent->type].glyph;
                fg = (ent->owner == 0) ? COL_PLAYER : COL_ENEMY;
                if (ent->underConstruction && g.tick % 10 < 5) ch = '#';
                bg = COL_BLACK;
            }

            bool isCur = (mx == g.cursorX && my == g.cursorY);
            if (isCur) { bg = COL_WHITE; fg = COL_BLACK; }

            // Selected highlight
            Entity* sel = findEntity(g.selectedId);
            if (sel && !isCur) {
                auto& ss = STATS[sel->type];
                bool isSel = false;
                if (ss.isBuilding) {
                    if (mx>=sel->x&&mx<sel->x+ss.sizeW&&my>=sel->y&&my<sel->y+ss.sizeH) isSel=true;
                } else if (mx==sel->x&&my==sel->y) isSel=true;
                if (isSel) bg = {0, 40, 0};
            }

            drawChar(gx_pos, gy_pos, ch, fg, bg);
        }
    }

    // === RIGHT PANEL ===
    int px = g.viewW + 1;
    // Panel background
    for (int y = 0; y < gridH; y++)
        for (int x = px; x < gridW; x++)
            drawChar(x, y, ' ', COL_UI_TEXT, COL_UI_BG);
    // Separator
    for (int y = 0; y < gridH; y++) drawChar(px - 1, y, '|', COL_UI_BORDER);

    // Minimap
    drawString(px+1, 1, "-- Minimap --", COL_UI_BORDER, COL_UI_BG);
    int mmW = std::min(panelW - 2, 18);
    int mmH = std::min(g.viewH / 2, 14);
    int mmY = 2;
    for (int my = 0; my < mmH; my++) {
        for (int mx = 0; mx < mmW; mx++) {
            int mapX = mx * MAP_W / mmW, mapY = my * MAP_H / mmH;
            char mch = ' '; Color mcol = COL_DGRAY; Color mbg = {15,15,25};
            if (g.map[mapY][mapX].explored[0]) {
                switch (g.map[mapY][mapX].terrain) {
                    case T_ROCK: mch='.'; mcol=COL_GRAY; break;
                    case T_LAVA: mch='~'; mcol=COL_LAVA; break;
                    case T_MINERALS: mch='*'; mcol=COL_MINERAL; break;
                    case T_VESPENE: mch='&'; mcol=COL_GAS; break;
                    default: mch='.'; mcol={30,30,45}; break;
                }
            }
            if (g.map[mapY][mapX].visible[0]) {
                Entity* ent = entityAt(mapX, mapY);
                if (ent && ent->alive) {
                    mch = isBldg(ent->type) ? '#' : '*';
                    mcol = (ent->owner==0) ? COL_PLAYER : COL_ENEMY;
                }
            }
            drawChar(px+1+mx, mmY+my, mch, mcol, mbg);
        }
    }

    // Info panel
    int iy = mmY + mmH + 1;
    Entity* sel = findEntity(g.selectedId);
    if (sel) {
        auto& st = STATS[sel->type];
        drawString(px+1, iy++, st.name, COL_UI_HIGH, COL_UI_BG);
        // HP bar
        int barW = std::min(panelW-4, 16);
        int filled = sel->hp * barW / sel->maxHp;
        char hpstr[32]; snprintf(hpstr, sizeof(hpstr), "HP %d/%d", sel->hp, sel->maxHp);
        drawString(px+1, iy++, hpstr, COL_UI_TEXT, COL_UI_BG);
        for (int i = 0; i < barW; i++) {
            Color c = (i < filled) ? ((sel->hp*100/sel->maxHp>50) ? COL_GREEN : COL_RED) : COL_DGRAY;
            drawChar(px+1+i, iy, '|', c, COL_UI_BG);
        }
        iy++;
        if (isUnit(sel->type)) {
            char atkstr[32]; snprintf(atkstr, sizeof(atkstr), "ATK:%d RNG:%d", st.atk, st.range);
            drawString(px+1, iy++, atkstr, COL_UI_TEXT, COL_UI_BG);
            const char* states[] = {"Idle","Moving","Attack","Gather","Build","Train","Dead"};
            char ststr[32]; snprintf(ststr, sizeof(ststr), "<%s>", states[sel->state]);
            drawString(px+1, iy++, ststr, COL_TEAL, COL_UI_BG);
        }
        if (sel->producing != E_NONE) {
            int pct = sel->prodProgress*100/std::max(1,sel->prodTime);
            char pstr[32]; snprintf(pstr, sizeof(pstr), "Train: %d%%", pct);
            drawString(px+1, iy++, pstr, COL_YELLOW, COL_UI_BG);
            drawString(px+1, iy++, STATS[sel->producing].name, COL_UI_TEXT, COL_UI_BG);
        }
        if (sel->underConstruction) {
            int pct = sel->hp*100/sel->maxHp;
            char bstr[32]; snprintf(bstr, sizeof(bstr), "Build: %d%%", pct);
            drawString(px+1, iy++, bstr, COL_ORANGE, COL_UI_BG);
        }
        iy++;
        if (sel->owner == 0) {
            if (sel->type == E_SCV) {
                drawString(px+1, iy++, "[B] Build", COL_TEAL, COL_UI_BG);
                drawString(px+1, iy++, "[Enter] Cmd", COL_TEAL, COL_UI_BG);
            } else if (isUnit(sel->type)) {
                drawString(px+1, iy++, "[Enter] Cmd", COL_TEAL, COL_UI_BG);
            } else if (isBldg(sel->type) && !sel->underConstruction) {
                drawString(px+1, iy++, "[T] Train", COL_TEAL, COL_UI_BG);
            }
        }
    } else {
        drawString(px+1, iy, "No selection", COL_DGRAY, COL_UI_BG);
    }

    // === BOTTOM BAR ===
    int botY = gridH - 1;
    for (int x = 0; x < gridW; x++) drawChar(x, botY, ' ', COL_UI_TEXT, COL_UI_BG);

    if (g.mode == M_BUILD_SELECT) {
        drawString(1, botY, "BUILD: [D]epot [B]arracks [F]actory [T]urret [R]efinery [Esc]", COL_UI_HIGH, COL_UI_BG);
    } else if (g.mode == M_TRAIN_SELECT) {
        Entity* s2 = findEntity(g.selectedId);
        if (s2) {
            if (s2->type==E_COMMAND_CENTER) drawString(1,botY,"TRAIN: [S]CV(50m) [Esc]",COL_UI_HIGH,COL_UI_BG);
            else if (s2->type==E_BARRACKS) drawString(1,botY,"TRAIN: [M]arine(50m) [F]irebat(50m+25g) [Esc]",COL_UI_HIGH,COL_UI_BG);
            else if (s2->type==E_FACTORY) drawString(1,botY,"TRAIN: [T]ank(150m+100g) [Esc]",COL_UI_HIGH,COL_UI_BG);
        }
    } else if (g.mode == M_PAUSED) {
        drawString(1, botY, "*** PAUSED *** [P] Resume", COL_YELLOW, COL_UI_BG);
    } else if (g.mode == M_GAME_OVER) {
        const char* msg = (g.winner==0) ? "*** VICTORY! *** [Q] Quit" : "*** DEFEAT! *** [Q] Quit";
        drawString(1, botY, msg, (g.winner==0)?COL_GREEN:COL_RED, COL_UI_BG);
    } else {
        drawString(1, botY, "Arrows:Move  Space:Select  Enter:Cmd  B:Build  T:Train  P:Pause  Q:Quit", COL_UI_TEXT, COL_UI_BG);
    }

    // Status message
    if (g.statusTimer > 0) {
        drawString(1, botY - 1, (">> " + g.statusMsg).c_str(), COL_UI_HIGH);
        g.statusTimer--;
    }

    // Cursor pos
    char posstr[16]; snprintf(posstr, sizeof(posstr), "(%d,%d)", g.cursorX, g.cursorY);
    drawString(gridW - 10, botY, posstr, COL_DGRAY, COL_UI_BG);

    SDL_RenderPresent(g.renderer);
}

// ============================================================
// INPUT
// ============================================================
void handleKey(SDL_Keycode key) {
    if (key == SDLK_q) { g.running = false; return; }
    if (key == SDLK_p) {
        if (g.mode==M_PAUSED) g.mode=M_NORMAL;
        else if (g.mode==M_NORMAL) g.mode=M_PAUSED;
        return;
    }
    if (g.mode==M_PAUSED||g.mode==M_GAME_OVER) return;

    if (g.mode == M_BUILD_SELECT) {
        Entity* sel=findEntity(g.selectedId);
        if (!sel||sel->type!=E_SCV) { g.mode=M_NORMAL; return; }
        EntityType tb=E_NONE;
        switch (key) {
            case SDLK_d: tb=E_SUPPLY_DEPOT; break;
            case SDLK_b: tb=E_BARRACKS; break;
            case SDLK_f: tb=E_FACTORY; break;
            case SDLK_t: tb=E_MISSILE_TURRET; break;
            case SDLK_r: tb=E_REFINERY; break;
            case SDLK_ESCAPE: g.mode=M_NORMAL; return;
            default: return;
        }
        if (tb!=E_NONE) { orderBuild(*sel,tb,g.cursorX,g.cursorY); g.mode=M_NORMAL; }
        return;
    }

    if (g.mode == M_TRAIN_SELECT) {
        Entity* sel=findEntity(g.selectedId);
        if (!sel) { g.mode=M_NORMAL; return; }
        EntityType tt=E_NONE;
        if (sel->type==E_COMMAND_CENTER) { if (key==SDLK_s) tt=E_SCV; }
        else if (sel->type==E_BARRACKS) {
            if (key==SDLK_m) tt=E_MARINE;
            else if (key==SDLK_f) tt=E_FIREBAT;
        }
        else if (sel->type==E_FACTORY) { if (key==SDLK_t) tt=E_SIEGE_TANK; }
        if (tt!=E_NONE) { orderTrain(*sel,tt); g.mode=M_NORMAL; }
        if (key==SDLK_ESCAPE) g.mode=M_NORMAL;
        return;
    }

    // Normal mode
    switch (key) {
    case SDLK_UP: case SDLK_w: g.cursorY--; break;
    case SDLK_DOWN: case SDLK_s: g.cursorY++; break;
    case SDLK_LEFT: case SDLK_a: g.cursorX--; break;
    case SDLK_RIGHT: case SDLK_d: g.cursorX++; break;
    case SDLK_SPACE: {
        Entity* ent=entityAtOwner(g.cursorX,g.cursorY,0);
        if (ent) { g.selectedId=ent->id; setStatus(std::string("Selected: ")+STATS[ent->type].name); }
        else {
            Entity* any=entityAt(g.cursorX,g.cursorY);
            if (any&&any->alive&&g.map[g.cursorY][g.cursorX].visible[0]) {
                g.selectedId=any->id; setStatus(std::string("Enemy: ")+STATS[any->type].name);
            } else g.selectedId=-1;
        }
        break;
    }
    case SDLK_RETURN: {
        Entity* sel=findEntity(g.selectedId);
        if (!sel||sel->owner!=0||!isUnit(sel->type)) break;
        Entity* tgt=entityAt(g.cursorX,g.cursorY);
        if (tgt&&tgt->alive&&tgt->owner==1&&g.map[g.cursorY][g.cursorX].visible[0]) {
            orderAttack(*sel,tgt->id); setStatus("Attacking!");
        } else if (sel->type==E_SCV) {
            Terrain ter=g.map[g.cursorY][g.cursorX].terrain;
            if ((ter==T_MINERALS||ter==T_VESPENE)&&g.map[g.cursorY][g.cursorX].resources>0) {
                orderGather(*sel,g.cursorX,g.cursorY);
                setStatus(ter==T_MINERALS?"Mining minerals...":"Harvesting vespene...");
            } else { orderMove(*sel,g.cursorX,g.cursorY); setStatus("Moving out."); }
        } else { orderMove(*sel,g.cursorX,g.cursorY); setStatus("Moving out."); }
        break;
    }
    case SDLK_b: {
        Entity* sel=findEntity(g.selectedId);
        if (sel&&sel->owner==0&&sel->type==E_SCV) { g.mode=M_BUILD_SELECT; setStatus("Place building at cursor..."); }
        else setStatus("Select an SCV first!");
        break;
    }
    case SDLK_t: {
        Entity* sel=findEntity(g.selectedId);
        if (sel&&sel->owner==0&&isBldg(sel->type)&&!sel->underConstruction) {
            if (sel->type==E_COMMAND_CENTER||sel->type==E_BARRACKS||sel->type==E_FACTORY) {
                g.mode=M_TRAIN_SELECT; setStatus("Select unit to train...");
            }
        }
        break;
    }
    case SDLK_TAB: {
        int sid=g.selectedId; bool found=false, past=(sid<0);
        for (auto& e:g.entities) {
            if (!e.alive||e.owner!=0||!isUnit(e.type)) continue;
            if (!past) { if (e.id==sid) past=true; continue; }
            g.selectedId=e.id; g.cursorX=e.x; g.cursorY=e.y; found=true; break;
        }
        if (!found) for (auto& e:g.entities) {
            if (!e.alive||e.owner!=0||!isUnit(e.type)) continue;
            g.selectedId=e.id; g.cursorX=e.x; g.cursorY=e.y; break;
        }
        break;
    }
    case SDLK_c: {
        // Center on command center
        for (auto& e:g.entities) if (e.alive&&e.owner==0&&e.type==E_COMMAND_CENTER) {
            g.selectedId=e.id; g.cursorX=e.x+1; g.cursorY=e.y+1; break;
        }
        break;
    }
    case SDLK_ESCAPE: g.selectedId=-1; g.mode=M_NORMAL; break;
    }

    g.cursorX = std::max(0, std::min(g.cursorX, MAP_W-1));
    g.cursorY = std::max(0, std::min(g.cursorY, MAP_H-1));
}

// ============================================================
// INIT GAME
// ============================================================
void initGame() {
    srand((unsigned)time(nullptr));
    g.nextId=1; g.tick=0; g.mode=M_NORMAL; g.selectedId=-1; g.winner=-1;
    g.aiTimer=0; g.statusTimer=0; g.running=true;
    g.players[0] = {400, 100, 0, 0, true};
    g.players[1] = {400, 100, 0, 0, true};
    generateMap();
    // Player
    spawnEntity(E_COMMAND_CENTER, 0, 5, 5);
    for (int i=0;i<4;i++) spawnEntity(E_SCV, 0, 10+i, 9);
    // AI
    spawnEntity(E_COMMAND_CENTER, 1, MAP_W-10, MAP_H-9);
    for (int i=0;i<4;i++) spawnEntity(E_SCV, 1, MAP_W-9+i, MAP_H-5);
    updateSupply(0); updateSupply(1);
    g.cursorX=7; g.cursorY=7; g.viewX=0; g.viewY=0;
    updateFog();
}

// ============================================================
// MAIN
// ============================================================
int main(int argc, char* argv[]) {
    initSDL();
    initGame();
    setStatus("Welcome, Commander. Select SCVs and mine minerals.");

    Uint32 lastTick = SDL_GetTicks();

    while (g.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) g.running = false;
            if (ev.type == SDL_KEYDOWN) handleKey(ev.key.keysym.sym);
        }

        Uint32 now = SDL_GetTicks();
        if (now - lastTick >= (Uint32)TICK_MS && g.mode != M_PAUSED && g.mode != M_GAME_OVER) {
            lastTick = now;
            g.tick++;
            for (int i=0;i<(int)g.entities.size();i++) tickEntity(g.entities[i]);
            tickDefenses();
            tickAI();
            updateFog();
            if (g.tick%100==0) {
                g.entities.erase(std::remove_if(g.entities.begin(),g.entities.end(),
                    [](const Entity& e){return !e.alive&&e.state==S_DEAD;}), g.entities.end());
                checkWin();
            }
        }

        renderGame();
        SDL_Delay(8); // ~120fps render cap
    }

    SDL_DestroyRenderer(g.renderer);
    SDL_DestroyWindow(g.window);
    SDL_Quit();
    return 0;
}
