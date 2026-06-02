#pragma once

#include "realm.h"

#include <vector>

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
    std::vector<TileRenderInfo> tiles;
    std::vector<EntityRenderInfo> entities;
    std::vector<ActionMarkerRenderInfo> actionMarkers;
};

RenderModel buildRenderModel(const Game& game, int observerOwner, int viewX, int viewY, int viewW, int viewH);
