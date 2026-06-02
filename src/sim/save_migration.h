#pragma once

#include "sim/save_schema.h"

struct Game;

bool isSupportedSaveVersion(int version);
bool migrateLoadedGame(Game& game, int fromVersion);
