#pragma once

#include <string>

struct Entity;
struct Game;

extern Game g;

void updateSupply(int owner);
void updateSupply(Game& game, int owner);
void setStatus(const std::string& message);
void addActionMarker(int x, int y, char glyph);
void addPlayerFood(int owner, int amount, Entity* depot);
void addPlayerFood(Game& game, int owner, int amount, Entity* depot);
void spendPlayerFood(int owner, int amount);
void spendPlayerFood(Game& game, int owner, int amount);
void resetDetectMapCache();
