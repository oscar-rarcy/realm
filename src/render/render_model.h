#pragma once

#include "core/game_types.h"

#include <vector>

struct Game;

struct TileRenderInfo {
    int x = 0;
    int y = 0;
    Terrain terrain = T_GRASS;
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

struct RenderModel {
    int viewX = 0;
    int viewY = 0;
    int viewW = 0;
    int viewH = 0;
    GameMode mode = M_NORMAL;
    EntityType buildPreviewType = E_NONE;
    std::vector<TileRenderInfo> tiles;
    std::vector<EntityRenderInfo> entities;
    std::vector<ActionMarkerRenderInfo> actionMarkers;
};

RenderModel buildRenderModel(const Game& game, const std::vector<ActionMarker>& actionMarkers,
                             int observerOwner, int viewX, int viewY, int viewW, int viewH);
RenderModel buildRenderModel(const Game& game, int observerOwner, int viewX, int viewY, int viewW, int viewH);
