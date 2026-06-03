#pragma once

#include <string>

struct Entity;
struct Game;

extern Game g;

void updateSupply(Game& game, int owner);
void setStatus(const std::string& message);
void addActionMarker(int x, int y, char glyph);
void addPlayerFood(Game& game, int owner, int amount, Entity* depot);
void spendPlayerFood(Game& game, int owner, int amount);
