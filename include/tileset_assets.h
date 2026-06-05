#pragma once

#include "core/game_types.h"

#include <SDL.h>

#include <string>

struct TilesetAssetRequest {
    EntityType type = E_NONE;
    std::string action;
    std::string direction;
    int frameIndex = 0;
    SDL_Color teamColor{35, 150, 220, 255};
    int targetWidth = 0;
    int targetHeight = 0;
};

enum class TilesetProjection {
    TileSpace,
    TileOverlay,
    UprightWorld,
    FootprintWorld,
    ProjectileWorld,
    ScreenUi,
};

enum class TilesetAnchorKind {
    None,
    Center,
    Feet,
    FootprintOrigin,
    WorldPosition,
    ScreenPosition,
};

enum class TilesetScalePolicy {
    SourcePixels,
    EntityTileZoom155,
    TileFill,
    Projectile,
    Ui,
};

enum class TilesetDepthLayer {
    Ground,
    Decal,
    FeatureBack,
    Entity,
    FeatureFront,
    Projectile,
    Overlay,
    Ui,
};

struct TilesetPlacement {
    bool valid = false;
    TilesetProjection projection = TilesetProjection::UprightWorld;
    TilesetAnchorKind anchorKind = TilesetAnchorKind::Feet;
    TilesetScalePolicy scalePolicy = TilesetScalePolicy::EntityTileZoom155;
    TilesetDepthLayer depth = TilesetDepthLayer::Entity;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int anchorX = 0;
    int anchorY = 0;
    int footprintWidth = 1;
    int footprintHeight = 1;
};

struct TilesetAssetFrame {
    SDL_Texture* texture = nullptr;
    bool baseLoaded = false;
    bool maskLoaded = false;
    bool placeholder = false;
    bool hasAnchor = false;
    TilesetPlacement placement;
    int width = 0;
    int height = 0;
    int anchorX = 0;
    int anchorY = 0;
    int anchorSourceWidth = 0;
    int anchorSourceHeight = 0;
    std::string basePath;
    std::string maskPath;
    std::string status;
};

std::string tilesetEntitySlug(EntityType type);
TilesetAssetFrame tilesetLoadEntityFrame(SDL_Renderer* renderer, const TilesetAssetRequest& request);
bool tilesetEntityFrameExists(EntityType type, const std::string& action,
                              const std::string& direction, int frameIndex);
TilesetAssetFrame tilesetLoadGroundTile(SDL_Renderer* renderer, GroundType ground);
TilesetAssetFrame tilesetLoadUnknownGroundTile(SDL_Renderer* renderer);
TilesetAssetFrame tilesetLoadGroundTileScaled(SDL_Renderer* renderer, GroundType ground,
                                              int width, int height);
TilesetAssetFrame tilesetLoadUnknownGroundTileScaled(SDL_Renderer* renderer,
                                                     int width, int height);
TilesetAssetFrame tilesetLoadGroundTileIso(SDL_Renderer* renderer, GroundType ground,
                                           int width, int height);
TilesetAssetFrame tilesetLoadUnknownGroundTileIso(SDL_Renderer* renderer,
                                                  int width, int height);
TilesetAssetFrame tilesetLoadFeatureTileScaled(SDL_Renderer* renderer, FeatureType feature,
                                               FeatureState state, const std::string& layer,
                                               int width, int height);
TilesetAssetFrame tilesetLoadDecalTileScaled(SDL_Renderer* renderer, VisualDecalType decal,
                                             int width, int height);
TilesetAssetFrame tilesetLoadProjectileTileScaled(SDL_Renderer* renderer, ProjectileType projectile,
                                                  int width, int height);
TilesetAssetFrame tilesetLoadEffectUiTileScaled(SDL_Renderer* renderer, const std::string& assetId,
                                                int width, int height);
void tilesetAssetsClear();
