#include "realm.h"

void realmSrand(unsigned seed) {
    realmSrand(g, seed);
}

void realmSrand(Game& game, unsigned seed) {
    game.rngState = seed ? seed : 1u;
}

int realmRand(Game& game) {
    game.rngState = game.rngState * 1664525u + 1013904223u;
    return (int)((game.rngState >> 1) & 0x7fffffffu);
}

int realmRand() {
    return realmRand(g);
}

int  dist(int x1,int y1,int x2,int y2)  { return std::max(std::abs(x1-x2), std::abs(y1-y2)); }
int  mdist(int x1,int y1,int x2,int y2) { return std::abs(x1-x2) + std::abs(y1-y2); }
bool inBounds(int x, int y)              { return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H; }
