#pragma once

#include "core/game_context.h"

#include <string>

struct SaveRequest {
    std::string path;
    int slot = 0;
    PlayerId issuer = 0;
};

struct LoadRequest {
    std::string path;
    int slot = 0;
    PlayerId issuer = 0;
};

struct SaveLoadResult {
    bool ok = false;
    std::string path;
    std::string message;
    std::string error;
};

SaveLoadResult saveGameService(GameContext& context, const SaveRequest& request);
SaveLoadResult loadGameService(GameContext& context, const LoadRequest& request);
