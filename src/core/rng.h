#pragma once

struct Game;

void realmSrand(Game& game, unsigned seed);
int realmRand(Game& game);
int dist(int x1, int y1, int x2, int y2);
int mdist(int x1, int y1, int x2, int y2);
bool inBounds(int x, int y);
