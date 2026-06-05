#pragma once

#include "core/game_state_types.h"

#include <cstdint>

struct GroundShaderColor {
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

struct GroundShaderContext {
    Season season = SUMMER;
    float seasonProgress = 0.0f;
    Weather weather = W_CLEAR;
    int tick = 0;
};

struct GroundShaderResult {
    GroundShaderColor baseFill;
    GroundShaderColor textureMod;
    GroundShaderColor overlayTint;
    bool drawOverlay = false;
};

GroundShaderResult shadeGroundTile(const Tile& tile, const VisualTileParts& parts,
                                   int x, int y, const GroundShaderContext& context);

