#pragma once

#include "realm.h"

constexpr int REALM_SAVE_VERSION = 9;
constexpr int REALM_MIN_SUPPORTED_SAVE_VERSION = 8;

bool isSupportedSaveVersion(int version);
bool migrateLoadedGame(Game& game, int fromVersion);
