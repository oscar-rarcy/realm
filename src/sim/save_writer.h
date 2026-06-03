#pragma once

#include <string>

struct Game;

bool writeSaveFile(const Game& game, const std::string& path);
