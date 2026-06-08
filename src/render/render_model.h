#pragma once

#include "core/game_types.h"

#include <string>
#include <vector>

struct Game;

struct TileRenderInfo {
    int x = 0;
    int y = 0;
    Terrain terrain = T_GRASS;
    int resources = 0;
    Biome biome = B_TEMPERATE;
    int wear = 0;
    VisualTileParts visualParts;
    bool gateOpen = false;
    bool gateLocked = false;
    bool visible = false;
    bool explored = false;
};

struct EntityRenderInfo {
    int id = -1;
    EntityType type = E_NONE;
    int owner = OWNER_NATURE;
    int x = 0;
    int y = 0;
    int hp = 0;
    int maxHp = 0;
    EntityState state = S_IDLE;
    int targetId = -1;
    int targetX = -1;
    int targetY = -1;
    int facingDx = 0;
    int facingDy = 0;
    int visualMoveFromX = 0;
    int visualMoveFromY = 0;
    int visualMoveToX = 0;
    int visualMoveToY = 0;
    int visualMoveStartedTick = 0;
    int visualMoveDurationTicks = 0;
    int visualMoveSeq = 0;
    int alertTicks = 0;
    bool underConstruction = false;
    bool attackMove = false;
    bool holdPosition = false;
    bool packed = false;
    int packTicks = 0;
    bool rallySet = false;
    int rallyX = -1;
    int rallyY = -1;
    bool visible = false;
    bool selected = false;
    BuildingVisualState buildingState = BVS_COMPLETE;
    AnimalCarcassVisualState animalCarcassState = ACVS_ALIVE;
    TransportVisualState transportState = TVS_EMPTY;
};

struct ActionMarkerRenderInfo {
    int x = 0;
    int y = 0;
    int ticks = 0;
    char glyph = 0;
};

struct ProjectileRenderInfo {
    int visualId = 0;
    ProjectileType type = PT_ARROW;
    float x = 0.0f;
    float y = 0.0f;
    float tx = 0.0f;
    float ty = 0.0f;
    float visualSpawnX = 0.0f;
    float visualSpawnY = 0.0f;
    float visualMoveFromX = 0.0f;
    float visualMoveFromY = 0.0f;
    float visualMoveToX = 0.0f;
    float visualMoveToY = 0.0f;
    int visualMoveStartedTick = 0;
    int visualMoveDurationTicks = 0;
    int visualMoveSeq = 0;
    int tileX = 0;
    int tileY = 0;
    char glyph = 0;
    int color = 0;
    int life = 0;
    bool alive = false;
    bool visible = false;
};

struct EffectRenderInfo {
    std::string assetId;
    int x = 0;
    int y = 0;
    int ticks = 0;
    bool worldSpace = true;
};

struct UiOverlayRenderInfo {
    std::string assetId;
    int x = 0;
    int y = 0;
    int ticks = 0;
    char glyph = 0;
    bool screenSpace = false;
};

struct RenderModel {
    int viewX = 0;
    int viewY = 0;
    int viewW = 0;
    int viewH = 0;
    GameMode mode = M_NORMAL;
    EntityType buildPreviewType = E_NONE;
    std::vector<TileRenderInfo> tiles;
    std::vector<EntityRenderInfo> entities;
    std::vector<ProjectileRenderInfo> projectiles;
    std::vector<EffectRenderInfo> effects;
    std::vector<UiOverlayRenderInfo> uiOverlays;
    std::vector<ActionMarkerRenderInfo> actionMarkers;
};

RenderModel buildRenderModel(const Game& game, const std::vector<ActionMarker>& actionMarkers,
                             int observerOwner, int viewX, int viewY, int viewW, int viewH);
RenderModel buildRenderModel(const Game& game, int observerOwner, int viewX, int viewY, int viewW, int viewH);
