#pragma once

#include <string>

struct Entity;
struct Game;

void updateSupply(Game& game, int owner);
void addPlayerFood(Game& game, int owner, int amount, Entity* depot);
void spendPlayerFood(Game& game, int owner, int amount);
