#include "render/ground_shader.h"

#include <algorithm>
#include <cmath>

namespace {

int clampByte(int v) {
    return std::max(0, std::min(255, v));
}

float clamp01Local(float v) {
    return std::max(0.0f, std::min(1.0f, v));
}

GroundShaderColor rgba(int r, int g, int b, int a = 255) {
    return GroundShaderColor{
        (uint8_t)clampByte(r),
        (uint8_t)clampByte(g),
        (uint8_t)clampByte(b),
        (uint8_t)clampByte(a),
    };
}

GroundShaderColor scaleColor(GroundShaderColor c, float f) {
    return rgba((int)std::lround(c.r * f), (int)std::lround(c.g * f),
                (int)std::lround(c.b * f), c.a);
}

GroundShaderColor blendColor(GroundShaderColor a, GroundShaderColor b, float t) {
    t = clamp01Local(t);
    return rgba((int)std::lround(a.r + (b.r - a.r) * t),
                (int)std::lround(a.g + (b.g - a.g) * t),
                (int)std::lround(a.b + (b.b - a.b) * t),
                (int)std::lround(a.a + (b.a - a.a) * t));
}

unsigned shaderHash(int x, int y, unsigned salt) {
    unsigned h = (unsigned)x * 374761393u + (unsigned)y * 668265263u + salt * 1442695041u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float hashUnit(int x, int y, unsigned salt) {
    return (shaderHash(x, y, salt) & 1023u) / 1023.0f;
}

float patchNoise(int x, int y, unsigned salt) {
    unsigned a = shaderHash(x / 3, y / 3, salt);
    unsigned b = shaderHash(x / 7, y / 6, salt + 13u);
    unsigned c = shaderHash(x / 17, y / 13, salt + 31u);
    float mixed = ((a & 255u) + (b & 255u) * 0.7f + (c & 255u) * 0.35f) / (255.0f * 2.05f);
    return clamp01Local(mixed);
}

GroundShaderColor baseForBiome(Biome biome) {
    switch (biome) {
        case B_DESERT:   return rgba(154, 126, 73);
        case B_SNOW:     return rgba(190, 202, 210);
        case B_SWAMP:    return rgba(45, 76, 52);
        case B_FOREST:   return rgba(28, 82, 42);
        case B_VOLCANIC: return rgba(58, 50, 48);
        case B_OCEAN:    return rgba(30, 74, 105);
        case B_TEMPERATE:
        default:         return rgba(45, 105, 48);
    }
}

GroundShaderColor baseForGround(GroundType ground, Biome biome) {
    switch (ground) {
        case G_GRASS:        return biome == B_FOREST ? rgba(38, 104, 44) : rgba(54, 116, 48);
        case G_MEADOW:       return rgba(74, 132, 67);
        case G_DIRT:         return rgba(106, 74, 45);
        case G_ROAD:         return rgba(91, 78, 60);
        case G_MUD:          return rgba(74, 61, 42);
        case G_SAND:         return rgba(166, 135, 78);
        case G_DUNES:        return rgba(184, 153, 91);
        case G_SNOW:         return rgba(205, 214, 220);
        case G_TUNDRA:       return rgba(137, 153, 124);
        case G_ICE:          return rgba(135, 178, 198);
        case G_WATER:        return rgba(23, 76, 122);
        case G_SHALLOWS:     return rgba(45, 116, 130);
        case G_MARSH:        return rgba(48, 79, 54);
        case G_GRAVEL:       return rgba(92, 91, 84);
        case G_ASH:          return rgba(57, 55, 53);
        case G_LAVA:         return rgba(115, 40, 24);
        case G_HILLS:        return rgba(74, 105, 54);
        case G_ROCKY:        return rgba(84, 82, 77);
        case G_CASTLE_FLOOR: return rgba(91, 74, 53);
    }
    return baseForBiome(biome);
}

bool isGreenGround(GroundType ground) {
    return ground == G_GRASS || ground == G_MEADOW || ground == G_HILLS || ground == G_TUNDRA;
}

bool isWaterGround(GroundType ground) {
    return ground == G_WATER || ground == G_SHALLOWS || ground == G_ICE;
}

bool isSnowGround(GroundType ground) {
    return ground == G_SNOW || ground == G_TUNDRA;
}

struct MaterialStyle {
    GroundShaderColor lowTint;
    GroundShaderColor highTint;
    float shadeMin = 0.94f;
    float shadeMax = 1.06f;
    float textureTint = 0.07f;
    int overlayAlpha = 22;
};

MaterialStyle styleForGround(GroundType ground) {
    switch (ground) {
        case G_GRASS:
            return {rgba(48, 91, 37), rgba(154, 151, 56), 0.90f, 1.09f, 0.09f, 26};
        case G_MEADOW:
            return {rgba(65, 119, 60), rgba(172, 159, 82), 0.92f, 1.10f, 0.08f, 22};
        case G_TUNDRA:
            return {rgba(112, 132, 105), rgba(194, 201, 184), 0.93f, 1.07f, 0.07f, 18};
        case G_HILLS:
            return {rgba(65, 87, 47), rgba(147, 130, 70), 0.91f, 1.08f, 0.08f, 22};
        case G_DIRT:
        case G_ROAD:
            return {rgba(77, 55, 38), rgba(145, 112, 72), 0.90f, 1.08f, 0.06f, 18};
        case G_MUD:
        case G_MARSH:
            return {rgba(45, 56, 36), rgba(92, 78, 47), 0.88f, 1.05f, 0.07f, 20};
        case G_SAND:
        case G_DUNES:
            return {rgba(132, 106, 62), rgba(211, 183, 112), 0.93f, 1.08f, 0.06f, 16};
        case G_WATER:
        case G_SHALLOWS:
            return {rgba(18, 82, 126), rgba(105, 181, 195), 0.95f, 1.07f, 0.05f, 12};
        case G_ICE:
        case G_SNOW:
            return {rgba(155, 184, 202), rgba(232, 237, 238), 0.96f, 1.06f, 0.05f, 12};
        case G_GRAVEL:
        case G_ROCKY:
        case G_CASTLE_FLOOR:
            return {rgba(66, 65, 61), rgba(139, 132, 112), 0.91f, 1.06f, 0.05f, 15};
        case G_ASH:
            return {rgba(42, 41, 39), rgba(94, 89, 82), 0.89f, 1.04f, 0.06f, 16};
        case G_LAVA:
            return {rgba(112, 30, 20), rgba(255, 112, 42), 0.94f, 1.10f, 0.08f, 22};
    }
    return {};
}

GroundShaderColor applySeasonToMaterial(GroundShaderColor c, GroundType ground, const GroundShaderContext& context) {
    float p = clamp01Local(context.seasonProgress);
    switch (context.season) {
        case SPRING:
            if (isGreenGround(ground)) c = blendColor(c, rgba(103, 170, 88), 0.12f + 0.06f * p);
            break;
        case SUMMER:
            if (isGreenGround(ground)) c = blendColor(c, rgba(180, 154, 72), 0.07f + 0.08f * p);
            break;
        case AUTUMN:
            if (isGreenGround(ground)) c = blendColor(c, rgba(178, 107, 50), 0.12f + 0.13f * p);
            else if (ground == G_DIRT || ground == G_ROAD || ground == G_GRAVEL)
                c = blendColor(c, rgba(150, 95, 50), 0.08f);
            break;
        case WINTER:
            if (isWaterGround(ground)) c = blendColor(c, rgba(180, 205, 218), 0.12f);
            else if (!isSnowGround(ground) && ground != G_LAVA)
                c = blendColor(c, rgba(205, 214, 218), 0.16f + 0.10f * p);
            break;
    }
    return c;
}

GroundShaderColor applyWeatherToMaterial(GroundShaderColor c, GroundType ground, const GroundShaderContext& context) {
    switch (context.weather) {
        case W_RAIN:
            if (!isWaterGround(ground) && ground != G_LAVA) c = blendColor(scaleColor(c, 0.94f), rgba(45, 62, 80), 0.06f);
            break;
        case W_STORM:
            if (!isWaterGround(ground) && ground != G_LAVA) c = blendColor(scaleColor(c, 0.88f), rgba(35, 49, 70), 0.11f);
            break;
        case W_SNOW:
            if (!isWaterGround(ground) && ground != G_LAVA) c = blendColor(c, rgba(220, 226, 228), 0.18f);
            break;
        case W_CLEAR:
        default:
            break;
    }
    return c;
}

GroundShaderColor overlayForGround(GroundType ground, GroundShaderColor a, GroundShaderColor b,
                                   float mix, int alpha, const GroundShaderContext& context) {
    GroundShaderColor c = blendColor(a, b, mix);
    c = applySeasonToMaterial(c, ground, context);
    c = applyWeatherToMaterial(c, ground, context);
    c.a = (uint8_t)clampByte(alpha);
    return c;
}

} // namespace

GroundShaderResult shadeGroundTile(const Tile& tile, const VisualTileParts& parts,
                                   int x, int y, const GroundShaderContext& context) {
    GroundShaderResult result;
    MaterialStyle style = styleForGround(parts.ground);
    float coarse = patchNoise(x, y, 3100u + (unsigned)parts.ground * 73u + (unsigned)tile.biome * 17u);
    float fine = hashUnit(x, y, 4300u + (unsigned)parts.ground * 41u);
    float mixed = clamp01Local(coarse * 0.78f + fine * 0.22f);
    if (parts.ground == G_WATER || parts.ground == G_SHALLOWS || parts.ground == G_LAVA) {
        float shimmer = hashUnit(x + context.tick / 18, y - context.tick / 24, 5100u + (unsigned)parts.ground);
        mixed = clamp01Local(mixed * 0.70f + shimmer * 0.30f);
    }

    GroundShaderColor base = baseForGround(parts.ground, tile.biome);
    base = blendColor(base, style.lowTint, std::max(0.0f, 0.24f - mixed * 0.24f));
    base = blendColor(base, style.highTint, std::max(0.0f, mixed - 0.58f) * 0.30f);
    base = applySeasonToMaterial(base, parts.ground, context);
    base = applyWeatherToMaterial(base, parts.ground, context);

    float shade = style.shadeMin + mixed * (style.shadeMax - style.shadeMin);
    if (tile.wear > 0 && (parts.ground == G_GRASS || parts.ground == G_MEADOW || parts.ground == G_TUNDRA)) {
        float wear = std::min(1.0f, tile.wear / 100.0f);
        base = blendColor(base, rgba(106, 82, 48), wear * 0.20f);
        shade -= wear * 0.035f;
    }
    result.baseFill = scaleColor(base, shade);

    GroundShaderColor mod = scaleColor(rgba(255, 255, 255), shade);
    GroundShaderColor tint = blendColor(style.lowTint, style.highTint, mixed);
    tint = applySeasonToMaterial(tint, parts.ground, context);
    tint = applyWeatherToMaterial(tint, parts.ground, context);
    result.textureMod = blendColor(mod, tint, style.textureTint);

    int alpha = style.overlayAlpha;
    if (tile.wear >= 55 && isGreenGround(parts.ground)) alpha += 6;
    result.overlayTint = overlayForGround(parts.ground, style.lowTint, style.highTint, mixed,
                                          alpha, context);
    result.drawOverlay = alpha > 0;
    return result;
}

