#pragma once

#include "realm.h"

#include <SDL.h>

#include <string>

struct TilesetAssetRequest {
    EntityType type = E_NONE;
    std::string action;
    std::string direction;
    int frameIndex = 0;
    SDL_Color teamColor{35, 150, 220, 255};
};

struct TilesetAssetFrame {
    SDL_Texture* texture = nullptr;
    bool baseLoaded = false;
    bool maskLoaded = false;
    bool placeholder = false;
    int width = 0;
    int height = 0;
    std::string basePath;
    std::string maskPath;
    std::string status;
};

std::string tilesetEntitySlug(EntityType type);
TilesetAssetFrame tilesetLoadEntityFrame(SDL_Renderer* renderer, const TilesetAssetRequest& request);
bool tilesetEntityFrameExists(EntityType type, const std::string& action,
                              const std::string& direction, int frameIndex);
void tilesetAssetsClear();
