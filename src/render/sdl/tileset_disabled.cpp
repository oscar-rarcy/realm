#include "tileset_assets.h"
#include "render/entity_visual_defs.h"
#include "render/sdl/sdl_hud.h"

std::string tilesetEntitySlug(EntityType type) {
    return entityAssetSlug(type);
}

TilesetAssetFrame tilesetLoadEntityFrame(SDL_Renderer* renderer, const TilesetAssetRequest& request) {
    (void)renderer;
    (void)request;
    return {};
}

TilesetPlacement tilesetResolveEntityFramePlacement(EntityType type, const std::string& action,
                                                    const std::string& direction, int frameIndex) {
    (void)type;
    (void)action;
    (void)direction;
    (void)frameIndex;
    return {};
}

bool tilesetEntityFrameExists(EntityType type, const std::string& action,
                              const std::string& direction, int frameIndex) {
    (void)type;
    (void)action;
    (void)direction;
    (void)frameIndex;
    return false;
}

TilesetAssetFrame tilesetLoadGroundTile(SDL_Renderer* renderer, GroundType ground) {
    (void)renderer;
    (void)ground;
    return {};
}

TilesetAssetFrame tilesetLoadUnknownGroundTile(SDL_Renderer* renderer) {
    (void)renderer;
    return {};
}

TilesetAssetFrame tilesetLoadGroundTileScaled(SDL_Renderer* renderer, GroundType ground,
                                              int width, int height) {
    (void)renderer;
    (void)ground;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadUnknownGroundTileScaled(SDL_Renderer* renderer,
                                                     int width, int height) {
    (void)renderer;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadGroundTileIso(SDL_Renderer* renderer, GroundType ground,
                                           int width, int height) {
    (void)renderer;
    (void)ground;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadUnknownGroundTileIso(SDL_Renderer* renderer,
                                                  int width, int height) {
    (void)renderer;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadFeatureTileScaled(SDL_Renderer* renderer, FeatureType feature,
                                               FeatureState state, const std::string& layer,
                                               int width, int height) {
    (void)renderer;
    (void)feature;
    (void)state;
    (void)layer;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadDecalTileScaled(SDL_Renderer* renderer, VisualDecalType decal,
                                             int width, int height) {
    (void)renderer;
    (void)decal;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadProjectileTileScaled(SDL_Renderer* renderer, ProjectileType projectile,
                                                  int width, int height) {
    (void)renderer;
    (void)projectile;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadEffectUiTileScaled(SDL_Renderer* renderer, const std::string& assetId,
                                                int width, int height) {
    (void)renderer;
    (void)assetId;
    (void)width;
    (void)height;
    return {};
}

TilesetAssetFrame tilesetLoadScreenUiTileScaled(SDL_Renderer* renderer, const std::string& assetId,
                                                int width, int height) {
    (void)renderer;
    (void)assetId;
    (void)width;
    (void)height;
    return {};
}

void tilesetAssetsClear() {}

bool tilesetHudEnabled() {
    return false;
}

SDL_Rect tilesetHudOverlayRect() {
    return SDL_Rect{0, 0, 0, 0};
}

SDL_Rect tilesetHudMiniMapRect() {
    return SDL_Rect{0, 0, 0, 0};
}

bool tilesetHudConsumesPointer(int px, int py) {
    (void)px;
    (void)py;
    return false;
}

bool tilesetHudClickableAt(int px, int py) {
    (void)px;
    (void)py;
    return false;
}

void drawTilesetHud(const WorldIndex& world) {
    (void)world;
}

void clearTilesetHudCaches() {}
