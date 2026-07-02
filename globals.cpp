#include "realm.h"

Game g;

// Sim RNG: splitmix64. Tiny, fast, and — unlike rand() — identical on
// every platform and libc, which lockstep multiplayer depends on.
void seedSimRng(unsigned long long seed) { g.rngState = seed; }
int simRand() {
    unsigned long long z = (g.rngState += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return (int)((z ^ (z >> 31)) & 0x7FFFFFFF);
}

const EntityStats STATS[] = {
    {"None",       ' ',   0, 0,0,0,0,  0,  0,  0, 1,1,  0,0, false},
    {"Peasant",    'p',  35, 3,1,3,8, 50,  0, 40, 1,1,  0,1, false},
    {"Militia",    'm',  70, 8,1,3,6, 60,  0, 50, 1,1,  0,1, false},
    {"Archer",     'a',  45, 6,5,3,7, 70,  0, 60, 1,1,  0,1, false},
    {"Knight",     'k', 110,14,1,2,5,120,  0, 80, 1,1,  0,2, false},
    {"Spearman",   'i',  55, 5,1,3,7, 40,  0, 50, 1,1,  0,1, false},
    {"Catapult",   'c',  70,25,8,8,12,150, 40, 90, 1,1,  0,3, false},
    {"Trebuchet",  'q',  90,45,12,6,60,200,250,200, 1,1, 0,4, false},
    {"Fishing Boat",'b', 40, 0,0,5, 0, 80, 50, 60, 1,1,  0,1, false},
    {"Warship",    'V',  70, 9,5,4, 7,150, 80,100, 1,1,  0,3, false},
    {"Transport",  'F',  80, 0,0,5, 0, 80, 40, 70, 1,1,  0,2, false},
    {"Ram",        '-',  80,28,1,6, 9, 70, 80, 80, 1,1,  0,3, false},
    {"Crossbowman",'x',  60, 9,4,3,12, 70, 30, 70, 1,1,  0,1, false},
    {"Hussar",     'u',  80, 7,1,1, 6, 80,  0, 60, 1,1,  0,2, false},
    {"Monk",       'n',  45, 0,0,3, 0, 60,  0, 60, 1,1,  0,1, false},
    {"Sapper",     'z',  50, 0,1,3, 0, 60, 20, 55, 1,1,  0,1, false},
    {"Wagon",      'g',  60, 0,0,5, 0, 40,  0, 50, 1,1,  0,1, false},
    {"Town Hall",  'H', 240, 0,0,0,0,200,150,100, 3,3, 10,0, true},
    {"House",      'h', 100, 0,0,0,0,  0, 50, 60, 2,2,  5,0, true},
    {"Barracks",   'B', 120, 0,0,0,0,  0,150, 80, 3,2,  0,0, true},
    {"Stable",     'S', 140, 0,0,0,0,  0,200,100, 3,2,  0,0, true},
    {"Tower",      'X', 130,10,7,0,7, 50,100, 70, 1,1,  0,0, true},
    {"Farm",       '%',  20, 0,0,0,0,  0,  0, 15, 1,1,  0,0, true},
    {"Blacksmith", 'A', 100, 0,0,0,0,  0,120, 70, 2,2,  0,0, true},
    {"Church",     '+', 120, 0,0,0,0, 80,100, 90, 2,2,  0,0, true},
    {"Market",     'M', 100, 0,0,0,0,  0,100, 60, 2,2,  0,0, true},
    {"Wall",       '#', 120, 0,0,0,0,  0, 20, 15, 1,1,  0,0, true},
    {"Gate",       'G',  90, 0,0,0,0,  0, 30, 20, 1,1,  0,0, true},
    // Castle is the 3x3 KEEP of a 7x7 walled compound: orderBuild expands the
    // placement into perimeter walls + four gates + courtyard floor.
    {"Castle",     'W', 350, 0,0,0,0,150,300,150, 3,3, 15,0, true},
    {"Lumber Camp",'L',  80, 0,0,0,0,  0, 80,  0, 2,2,  0,0, true},
    {"Mining Camp",'N',  80, 0,0,0,0, 30, 60,  0, 2,2,  0,0, true},
    {"Mill",       'O', 100, 0,0,0,0,  0,100, 80, 2,2,  0,0, true},
    {"Dock",       'D', 100, 0,0,0,0,  0,100, 70, 2,2,  0,0, true},
    {"Granary",    'U', 120, 0,0,0,0,  0, 80, 70, 2,2,  0,0, true},
    {"Tavern",     'P', 110, 0,0,0,0, 40, 60, 70, 2,2,  0,0, true},
    {"Well",       'Q',  60, 0,0,0,0, 10, 20, 30, 1,1,  0,0, true},
    {"Manor",      'R', 160, 0,0,0,0, 50,100, 80, 2,2, 10,0, true},
    {"Stonemason", 'Z', 110, 0,0,0,0, 60,120, 70, 2,2,  0,0, true},
    // Open-air hoard: huge, cheap storage whose goods sit in visible piles
    // on its 3x3 yard — and any enemy can march in and carry them off.
    {"Stockyard",  '=', 140, 0,0,0,0,  0, 60, 50, 3,3,  0,0, true},
    {"Shrine",     'I',  80, 0,0,0,0,  0,  0,  0, 1,1,  0,0, true},
    {"Watermill",  'C', 100, 0,0,0,0,  0,  0,  0, 2,2,  0,0, true},
    {"Trading Post",'E',100, 0,0,0,0,  0,  0,  0, 2,2,  0,0, true},
    {"Wolf Den",   'v', 120, 0,0,0,0,  0,  0,  0, 1,1,  0,0, true},
    {"Ruined Keep",'#', 400, 0,0,0,0,  0,  0,  0, 2,2,  0,0, true},
    {"Bridge",     '=',  60, 0,0,0,0,  0, 25, 20, 1,1,  0,0, true},
    {"Deer",       'd',  20, 0,0,2,0,  0,  0,  0, 1,1,  0,0, false},
    {"Wolf",       'w',  35, 4,1,2,8,  0,  0,  0, 1,1,  0,0, false},
    {"Sheep",      's',  12, 0,0,3,0,  0,  0,  0, 1,1,  0,0, false},
    {"Boar",       'o',  25, 3,1,2,8,  0,  0,  0, 1,1,  0,0, false},
    {"Bear",       'Y', 100,13,1,2,10, 0,  0,  0, 1,1,  0,0, false},
};

// ============================================================
// CIVILISATIONS — a bonus column and a denial column. The denials are
// the counter-play: Marcher cavalry runs into Fenland spear walls, the
// Fenlanders can't chase Hillfolk miners into the hills, and so on.
// ============================================================
const CivDef CIVS[] = {
    {"Freeholders",  "Peasants train 15% faster; sturdy all-rounders", "no edge in war"},
    {"Fenlanders",   "Farms yield +1, boats 25% cheaper, archers train fast", "no stables (no cavalry)"},
    {"Hillfolk",     "Miners +25%, walls/towers/gates +50% HP", "military trains slower; no war fleet"},
    {"Marcher Lords","Stable units 20% cheaper and train 25% faster", "no archers (crossbows come late)"},
};

const char* eraName(int era) {
    switch (era) {
        case ERA_HAMLET:     return "Hamlet";
        case ERA_TOWNSHIP:   return "Township";
        case ERA_STRONGHOLD: return "Stronghold";
    }
    return "?";
}

// The long, expensive click that defines the match arc.
bool eraUpCost(int fromEra, int& food, int& gold, int& wood, int& ticks) {
    if (fromEra == ERA_HAMLET)     { food = 175; gold = 100; wood = 0;   ticks = 900;  return true; }
    if (fromEra == ERA_TOWNSHIP)   { food = 450; gold = 300; wood = 150; ticks = 1300; return true; }
    return false;
}
