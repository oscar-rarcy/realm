#include "render/sdl/sdl_map.h"
#include "render/sdl/sdl_profiler.h"
#include "realm.h"
#include "commands/command.h"
#include "core/entity_facing.h"
#include "core/game_events.h"
#include "core/entity_motion.h"
#include "core/world_index.h"
#include "entity_animation.h"
#include "render/render_model.h"
#include "view_state.h"
#include "tileset_assets.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

Entity* renderFindEntity(Game& game, const WorldIndex& world, int id) {
    return findEntity(game, world, id);
}

Entity* renderEntityAt(Game& game, const WorldIndex& world, int x, int y) {
    return entityAt(game, world, x, y);
}

static Entity* renderCorpseAt(Game& game, const WorldIndex& world, int x, int y) {
    return corpseAt(game, world, x, y);
}

bool renderCanPlace(Game& game, const WorldIndex& world, EntityType type, int x, int y, int owner, int ignoreEntityId) {
    return canPlace(game, world, type, x, y, owner, ignoreEntityId);
}

static const TileRenderInfo* tileInfoAt(const RenderModel& model, int mx, int my) {
    int sx = mx - model.viewX;
    int sy = my - model.viewY;
    if (sx < 0 || sy < 0 || sx >= model.viewW || sy >= model.viewH) return nullptr;
    size_t index = (size_t)sy * (size_t)model.viewW + (size_t)sx;
    if (index >= model.tiles.size()) return nullptr;
    return &model.tiles[index];
}

static const ActionMarkerRenderInfo* actionMarkerAt(const RenderModel& model, int mx, int my) {
    for (const ActionMarkerRenderInfo& marker : model.actionMarkers) {
        if (marker.x == mx && marker.y == my) return &marker;
    }
    return nullptr;
}

static const ProjectileRenderInfo* projectileAt(const RenderModel& model, int mx, int my) {
    for (const ProjectileRenderInfo& projectile : model.projectiles) {
        if (projectile.tileX == mx && projectile.tileY == my) return &projectile;
    }
    return nullptr;
}

static Color projectileGlyphColor(int colorPair) {
    switch (colorPair) {
        case CP_PROJ_BOULDER: return rgb(210, 210, 210);
        case CP_PROJ_TOWER: return rgb(255, 120, 105);
        case CP_PROJ_ARROW:
        default:
            return rgb(255, 220, 120);
    }
}

int keyToInput(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z) return 'a' + (int)(key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9) return '0' + (int)(key - SDLK_0);
    switch (key) {
        case SDLK_UP: return KEY_UP;
        case SDLK_DOWN: return KEY_DOWN;
        case SDLK_LEFT: return KEY_LEFT;
        case SDLK_RIGHT: return KEY_RIGHT;
        case SDLK_RETURN: return '\n';
        case SDLK_KP_ENTER: return '\n';
        case SDLK_ESCAPE: return 27;
        case SDLK_SPACE: return ' ';
        case SDLK_PAGEUP: return KEY_PPAGE;
        case SDLK_PAGEDOWN: return KEY_NPAGE;
        case SDLK_HOME: return KEY_HOME;
        case SDLK_END: return KEY_END;
        case SDLK_EQUALS: return '=';
        case SDLK_MINUS: return '-';
        default: return 0;
    }
}

const char* seasonNameSafe() { return getSeasonName(g); }
const char* timeNameSafe() { return getTimeName(g); }
const char* weatherName() {
    switch (g.weather) {
        case W_RAIN: return "Rain";
        case W_STORM: return "Storm";
        case W_SNOW: return "Snow";
        default: return "Clear";
    }
}

std::string trimPanelLine(const std::string& s, size_t maxLen) {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen - 1) + "~";
}

std::string cursorTileSummary() {
    if (!inBounds(view.cursorX, view.cursorY)) return "Tile: out of bounds";
    const Tile& t = g.map[view.cursorY][view.cursorX];
    std::ostringstream ss;
    ss << terrainName(t.terrain) << " / " << biomeName(t.biome);
    if (t.resources > 0) ss << " / " << t.resources << " res";
    return trimPanelLine(ss.str());
}

static int entityFootprintWidth(const Entity& ent) {
    return std::max(1, STATS[ent.type].sizeW);
}

static int entityFootprintHeight(const Entity& ent) {
    return std::max(1, STATS[ent.type].sizeH);
}

static bool entityFootprintCoversTile(const Entity& ent, int x, int y) {
    return x >= ent.x && y >= ent.y
        && x < ent.x + entityFootprintWidth(ent)
        && y < ent.y + entityFootprintHeight(ent);
}

static bool entityFootprintHasVisibleTile(const Entity& ent) {
    for (int dy = 0; dy < entityFootprintHeight(ent); ++dy) {
        for (int dx = 0; dx < entityFootprintWidth(ent); ++dx) {
            int mx = ent.x + dx;
            int my = ent.y + dy;
            if (inBounds(mx, my) && g.map[my][mx].visible[0]) return true;
        }
    }
    return false;
}

std::string cursorStackSummary() {
    if (!inBounds(view.cursorX, view.cursorY) || !g.map[view.cursorY][view.cursorX].visible[0]) return "Stack: not visible";
    std::ostringstream ss;
    int count = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.state == S_GARRISONED) continue;
        auto& st = STATS[e.type];
        if (!entityFootprintCoversTile(e, view.cursorX, view.cursorY)) continue;
        if (count++ > 0) ss << ", ";
        if (e.owner == 0) ss << "You ";
        else if (e.owner == OWNER_NATURE) ss << "Neutral ";
        else ss << "P" << (e.owner + 1) << ' ';
        ss << st.name;
        if (count >= 3) break;
    }
    if (count == 0) return "Stack: empty";
    if (count < (int)g.entities.size()) {
        int more = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            if (entityFootprintCoversTile(e, view.cursorX, view.cursorY)) more++;
        }
        if (more > count) ss << " +" << (more - count);
    }
    return trimPanelLine("Stack: " + ss.str());
}

struct TileVisual {
    bool visible = false;
    bool explored = false;
    bool cursor = false;
    bool selected = false;
    Entity* ent = nullptr;
    const ProjectileRenderInfo* projectile = nullptr;
    Color bg = rgb(0,0,0);
    Color fg = rgb(230,230,220);
    std::string glyph;
    bool emoji = false;
    bool tint = false;
};

static bool tilesetIndicatorsEnabled() {
    return displayMode == DM_EMOJI;
}

static float indicatorPulse() {
    return 0.5f + 0.5f * std::sin(SDL_GetTicks() / 260.0f);
}

static int indicatorOutlineStroke() {
    return 5;
}

static int indicatorInnerStroke() {
    return 2;
}

static Color indicatorOutlineColor(int alpha = 230) {
    return rgb(30, 58, 48, alpha);
}

static Color indicatorYellowColor(int alpha = 230) {
    return rgb(255, 225, 70, alpha);
}

static Color indicatorCommandColor(int alpha = 230) {
    return rgb(238, 70, 56, alpha);
}

static Color indicatorPathColor(int alpha = 230) {
    return rgb(74, 166, 255, alpha);
}

static void fillDownTriangle(int cx, int topY, int width, int height, Color c);

static Color indicatorActionMarkerColor(char glyph, int alpha = 230) {
    switch (glyph) {
        case '+': return rgb(74, 218, 118, alpha);
        case 'x': return rgb(74, 166, 255, alpha);
        case '!': return rgb(238, 70, 56, alpha);
        case '#': return rgb(244, 178, 62, alpha);
        case 'r': return rgb(76, 214, 224, alpha);
        default:  return indicatorYellowColor(alpha);
    }
}

static void drawThickLine(int x1, int y1, int x2, int y2, Color c, int thickness) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    int radius = std::max(0, thickness / 2);
    for (int oy = -radius; oy <= radius; ++oy) {
        for (int ox = -radius; ox <= radius; ++ox) {
            if (std::abs(ox) + std::abs(oy) > radius) continue;
            SDL_RenderDrawLine(s.ren, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
        }
    }
}

static void drawDottedThickLine(int x1, int y1, int x2, int y2, Color c, int thickness, int dash, int gap) {
    float dx = (float)(x2 - x1);
    float dy = (float)(y2 - y1);
    float length = std::sqrt(dx * dx + dy * dy);
    if (length < 1.0f) return;
    dash = std::max(2, dash);
    gap = std::max(1, gap);
    float ux = dx / length;
    float uy = dy / length;
    for (float start = 0.0f; start < length; start += (float)(dash + gap)) {
        float end = std::min(length, start + (float)dash);
        int sx = (int)std::lround(x1 + ux * start);
        int sy = (int)std::lround(y1 + uy * start);
        int ex = (int)std::lround(x1 + ux * end);
        int ey = (int)std::lround(y1 + uy * end);
        drawThickLine(sx, sy, ex, ey, c, thickness);
    }
}

static void drawHoverCornerPass(SDL_Rect b, int len, Color c, int thickness) {
    int x0 = b.x;
    int y0 = b.y;
    int x1 = b.x + b.w - 1;
    int y1 = b.y + b.h - 1;
    drawThickLine(x0, y0, x0 + len, y0, c, thickness);
    drawThickLine(x0, y0, x0, y0 + len, c, thickness);
    drawThickLine(x1, y0, x1 - len, y0, c, thickness);
    drawThickLine(x1, y0, x1, y0 + len, c, thickness);
    drawThickLine(x0, y1, x0 + len, y1, c, thickness);
    drawThickLine(x0, y1, x0, y1 - len, c, thickness);
    drawThickLine(x1, y1, x1 - len, y1, c, thickness);
    drawThickLine(x1, y1, x1, y1 - len, c, thickness);
}

static void drawHoverCornersRect(SDL_Rect r) {
    float pulse = indicatorPulse();
    int out = 3 + (int)std::lround(pulse * 2.0f);
    SDL_Rect b{r.x - out, r.y - out, r.w + out * 2, r.h + out * 2};
    int len = std::max(7, std::min(b.w, b.h) / 3);
    drawHoverCornerPass(b, len, indicatorOutlineColor(), indicatorOutlineStroke());
    drawHoverCornerPass(b, len, indicatorYellowColor(210 + (int)std::lround(pulse * 35.0f)), indicatorInnerStroke());
}

static SDL_Rect featureSpriteRectIso(int cx, int cy, int hw, int hh) {
    int size = std::max(16, (int)std::lround(hw * 1.12f));
    return SDL_Rect{cx - size / 2, cy + hh - size, size, size};
}

static void drawAttackCenterIndicator(int cx, int cy, int hw, int hh) {
    float pulse = indicatorPulse();
    int markW = std::max(7, (int)std::lround(hw * (0.34f + pulse * 0.04f)));
    int markH = std::max(4, (int)std::lround(hh * (0.36f + pulse * 0.05f)));
    int stroke = indicatorOutlineStroke();
    int inner = indicatorInnerStroke();

    Color shadow = indicatorOutlineColor();
    Color command = indicatorCommandColor(220 + (int)std::lround(pulse * 25.0f));
    int lineY = std::max(2, markH / 2);
    int lineX = std::max(3, markW / 3);
    drawThickLine(cx, cy - lineY, cx, cy + lineY / 2, shadow, stroke);
    drawThickLine(cx - lineX, cy, cx + lineX, cy, shadow, stroke);
    drawThickLine(cx, cy - lineY, cx, cy + lineY / 2, command, inner);
    drawThickLine(cx - lineX, cy, cx + lineX, cy, command, inner);
    fillDiamond(cx, cy + lineY, std::max(2, inner + 1), std::max(1, inner), command);
}

static void drawActionMarkerIndicator(char glyph, int cx, int cy, int spanX, int spanY) {
    float pulse = indicatorPulse();
    int outline = std::max(2, indicatorOutlineStroke() - 1);
    int width = std::max(7, spanX);
    int height = std::max(6, spanY);
    Color fill = indicatorActionMarkerColor(glyph, 220 + (int)std::lround(pulse * 25.0f));
    fillDownTriangle(cx, cy, width + outline * 2, height + outline * 2, indicatorOutlineColor(226));
    fillDownTriangle(cx, cy + outline, width, height, fill);
}

static void fillDownTriangle(int cx, int topY, int width, int height, Color c) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    int half = std::max(1, width / 2);
    height = std::max(1, height);
    for (int y = 0; y <= height; ++y) {
        float t = y / (float)height;
        int span = std::max(0, (int)std::lround(half * (1.0f - t)));
        SDL_RenderDrawLine(s.ren, cx - span, topY + y, cx + span, topY + y);
    }
}

static void drawSelectionTriangle(int cx, int topY, int width, int height) {
    int outline = indicatorOutlineStroke();
    fillDownTriangle(cx, topY, width + outline * 2, height + outline * 2, indicatorOutlineColor());
    fillDownTriangle(cx, topY + outline, width, height, indicatorYellowColor(245));
}

static bool tilesetMovingSpritePassEnabled() {
    return tilesetIndicatorsEnabled();
}

static bool handledByTilesetSpritePass(const Entity* ent) {
    return tilesetMovingSpritePassEnabled() && ent && ent->alive && isUnit(ent->type) && !isBuilding(ent->type);
}

enum class EntityMotionProfile {
    None,
    PaperWalk,
    HeavySlide,
    BoatSlide,
};

struct EntityMotionSegment {
    int seq = 0;
    int fromX = 0;
    int fromY = 0;
    int toX = 0;
    int toY = 0;
    Uint32 startedMs = 0;
    int durationMs = 0;
};

struct ProjectileMotionSegment {
    int seq = 0;
    float fromX = 0.0f;
    float fromY = 0.0f;
    float toX = 0.0f;
    float toY = 0.0f;
    Uint32 startedMs = 0;
    int durationMs = 0;
};

struct SpriteMotionSample {
    float x = 0.0f;
    float y = 0.0f;
    float progress = 1.0f;
    int liftPx = 0;
    double angleDegrees = 0.0;
    int explicitFrame = -1;
    bool moving = false;
};

static std::unordered_map<int, EntityMotionSegment> entityMotionCache;
static std::unordered_map<int, ProjectileMotionSegment> projectileMotionCache;
static int motionCacheLastTick = -1;

static void resetMotionCacheIfNeeded(const Game& game) {
    if (motionCacheLastTick >= 0 && game.tick < motionCacheLastTick) {
        entityMotionCache.clear();
        projectileMotionCache.clear();
    }
    motionCacheLastTick = game.tick;
}

static int elapsedMoveMsFromGameTick(const Game& game, int startedTick, int durationMs) {
    int elapsedTicks = std::max(0, game.tick - startedTick);
    return std::min(durationMs, elapsedTicks * TICK_MS);
}

static void syncEntityMotionCache(const Game& game, const RenderModel& model) {
    resetMotionCacheIfNeeded(game);
    Uint32 now = SDL_GetTicks();
    std::vector<int> present;
    present.reserve(model.entities.size());
    for (const EntityRenderInfo& info : model.entities) {
        present.push_back(info.id);
        if (!info.visible || !isUnit(info.type) || isBuilding(info.type)) continue;
        if (info.visualMoveSeq <= 0 || info.visualMoveDurationTicks <= 0) continue;
        if (info.visualMoveFromX == info.visualMoveToX && info.visualMoveFromY == info.visualMoveToY) continue;
        int durationMs = std::max(TICK_MS, info.visualMoveDurationTicks * TICK_MS);
        EntityMotionSegment& segment = entityMotionCache[info.id];
        if (segment.seq != info.visualMoveSeq
            || segment.fromX != info.visualMoveFromX || segment.fromY != info.visualMoveFromY
            || segment.toX != info.visualMoveToX || segment.toY != info.visualMoveToY) {
            segment.seq = info.visualMoveSeq;
            segment.fromX = info.visualMoveFromX;
            segment.fromY = info.visualMoveFromY;
            segment.toX = info.visualMoveToX;
            segment.toY = info.visualMoveToY;
            segment.durationMs = durationMs;
            segment.startedMs = now - elapsedMoveMsFromGameTick(game, info.visualMoveStartedTick, durationMs);
        }
    }
    for (auto it = entityMotionCache.begin(); it != entityMotionCache.end();) {
        if (std::find(present.begin(), present.end(), it->first) == present.end()) it = entityMotionCache.erase(it);
        else ++it;
    }
}

static int projectileCacheKey(const ProjectileRenderInfo& projectile, int fallbackIndex) {
    return projectile.visualId > 0 ? projectile.visualId : -1 - fallbackIndex;
}

static void syncProjectileMotionCache(const Game& game, const RenderModel& model) {
    resetMotionCacheIfNeeded(game);
    Uint32 now = SDL_GetTicks();
    std::vector<int> present;
    present.reserve(model.projectiles.size());
    for (int i = 0; i < (int)model.projectiles.size(); ++i) {
        const ProjectileRenderInfo& info = model.projectiles[i];
        int key = projectileCacheKey(info, i);
        present.push_back(key);
        if (info.visualMoveSeq <= 0 || info.visualMoveDurationTicks <= 0) continue;
        int durationMs = std::max(1, info.visualMoveDurationTicks) * TICK_MS;
        ProjectileMotionSegment& segment = projectileMotionCache[key];
        if (segment.seq != info.visualMoveSeq
            || segment.fromX != info.visualMoveFromX || segment.fromY != info.visualMoveFromY
            || segment.toX != info.visualMoveToX || segment.toY != info.visualMoveToY) {
            segment.seq = info.visualMoveSeq;
            segment.fromX = info.visualMoveFromX;
            segment.fromY = info.visualMoveFromY;
            segment.toX = info.visualMoveToX;
            segment.toY = info.visualMoveToY;
            segment.durationMs = durationMs;
            segment.startedMs = now - elapsedMoveMsFromGameTick(game, info.visualMoveStartedTick, durationMs);
        }
    }
    for (auto it = projectileMotionCache.begin(); it != projectileMotionCache.end();) {
        if (std::find(present.begin(), present.end(), it->first) == present.end()) it = projectileMotionCache.erase(it);
        else ++it;
    }
}

static EntityMotionProfile motionProfileForEntity(const Entity& ent, const EntityActionAnimationSpec* anim) {
    if (isNaval(ent.type)) return EntityMotionProfile::BoatSlide;
    if (isSiege(ent.type) || ent.type == E_RAM) return EntityMotionProfile::HeavySlide;
    if (anim && anim->family && std::strstr(anim->family, "gait")) return EntityMotionProfile::PaperWalk;
    if (entityHasActivePathMotion(ent)) {
        return EntityMotionProfile::PaperWalk;
    }
    return EntityMotionProfile::None;
}

static int frameForMotionProgress(const EntityActionAnimationSpec* anim, float progress, bool moving) {
    if (!moving || !anim || anim->frameCount <= 1) return -1;
    return std::min(anim->frameCount - 1, (int)std::floor(clamp01(progress) * anim->frameCount));
}

static SpriteMotionSample sampleEntityMotion(const Entity& ent, const EntityActionAnimationSpec* anim) {
    SpriteMotionSample sample;
    sample.x = (float)ent.x;
    sample.y = (float)ent.y;
    auto it = entityMotionCache.find(ent.id);
    if (it == entityMotionCache.end() || it->second.durationMs <= 0) return sample;

    const EntityMotionSegment& segment = it->second;
    Uint32 now = SDL_GetTicks();
    float progress = clamp01((now - segment.startedMs) / (float)std::max(1, segment.durationMs));
    sample.progress = progress;
    sample.moving = progress < 1.0f;
    sample.x = segment.fromX + (segment.toX - segment.fromX) * progress;
    sample.y = segment.fromY + (segment.toY - segment.fromY) * progress;
    if (!sample.moving) {
        sample.x = (float)segment.toX;
        sample.y = (float)segment.toY;
        return sample;
    }

    EntityMotionProfile profile = motionProfileForEntity(ent, anim);
    float wave = std::sin(progress * 3.14159265f);
    float stride = std::sin(progress * 3.14159265f * 2.0f);
    if (profile == EntityMotionProfile::PaperWalk) {
        sample.liftPx = (int)std::lround(std::max(1.0f, s.tile * 0.09f) * wave);
        sample.angleDegrees = stride * 4.5;
    } else if (profile == EntityMotionProfile::HeavySlide) {
        sample.liftPx = 0;
        sample.angleDegrees = stride * 1.2;
    } else if (profile == EntityMotionProfile::BoatSlide) {
        sample.liftPx = (int)std::lround(std::max(1.0f, s.tile * 0.025f) * stride);
        sample.angleDegrees = stride * 1.0;
    }
    sample.explicitFrame = frameForMotionProgress(anim, progress, sample.moving);
    return sample;
}

static SpriteMotionSample sampleProjectileMotion(const ProjectileRenderInfo& projectile, int fallbackIndex) {
    SpriteMotionSample sample;
    sample.x = projectile.x;
    sample.y = projectile.y;
    int key = projectileCacheKey(projectile, fallbackIndex);
    auto it = projectileMotionCache.find(key);
    if (it != projectileMotionCache.end() && it->second.durationMs > 0) {
        const ProjectileMotionSegment& segment = it->second;
        Uint32 now = SDL_GetTicks();
        float progress = clamp01((now - segment.startedMs) / (float)std::max(1, segment.durationMs));
        sample.progress = progress;
        sample.moving = progress < 1.0f;
        sample.x = segment.fromX + (segment.toX - segment.fromX) * progress;
        sample.y = segment.fromY + (segment.toY - segment.fromY) * progress;
    }

    float totalDx = projectile.tx - projectile.visualSpawnX;
    float totalDy = projectile.ty - projectile.visualSpawnY;
    float total = std::sqrt(totalDx * totalDx + totalDy * totalDy);
    if (total > 0.1f) {
        float remainDx = projectile.tx - sample.x;
        float remainDy = projectile.ty - sample.y;
        float flightProgress = clamp01(1.0f - std::sqrt(remainDx * remainDx + remainDy * remainDy) / total);
        float arc = std::sin(flightProgress * 3.14159265f);
        float arcScale = (projectile.type == PT_CATAPULT_BOULDER || projectile.type == PT_TREBUCHET_BOULDER)
            ? 0.62f : 0.34f;
        sample.liftPx = (int)std::lround(std::max(1.0f, s.tile * arcScale) * arc);
    }
    return sample;
}

static void isoTileCenterFloat(float mx, float my, int& cx, int& cy) {
    int ox = 0, oy = 0;
    isoOrigin(ox, oy);
    int hw = isoHalfW();
    int hh = isoHalfH();
    float sx = mx - isoCameraViewX();
    float sy = my - isoCameraViewY();
    cx = (int)std::lround(ox + (sx - sy) * hw);
    cy = (int)std::lround(oy + (sx + sy) * hh + hh);
}

static void topDownTileCenterFloat(float mx, float my, int& cx, int& cy) {
    SDL_Rect mr = mapRect();
    cx = (int)std::lround(mr.x + (mx - view.viewX + 0.5f) * s.tile);
    cy = (int)std::lround(mr.y + (my - view.viewY + 0.5f) * s.tile);
}

static bool entityHasMultiTileFootprint(const Entity& ent) {
    const EntityStats& stats = STATS[ent.type];
    return stats.sizeW > 1 || stats.sizeH > 1;
}

static bool entityDrawsFromFootprintTile(Game& game, const WorldIndex& world, const RenderModel& model,
                                         const Entity& ent, int mx, int my) {
    if (!entityHasMultiTileFootprint(ent)) return true;

    const EntityStats& stats = STATS[ent.type];
    int x0 = std::max(ent.x, model.viewX);
    int y0 = std::max(ent.y, model.viewY);
    int x1 = std::min(ent.x + stats.sizeW, model.viewX + model.viewW);
    int y1 = std::min(ent.y + stats.sizeH, model.viewY + model.viewH);
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const TileRenderInfo* info = tileInfoAt(model, x, y);
            Entity* visibleEntity = info && info->visible ? renderEntityAt(game, world, x, y) : nullptr;
            if (visibleEntity && visibleEntity->id == ent.id) return x == mx && y == my;
        }
    }
    return mx == ent.x && my == ent.y;
}

static int entityFootprintSpriteSize(const Entity& ent, int fallbackSize) {
    if (!entityHasMultiTileFootprint(ent)) return fallbackSize;
    const EntityStats& stats = STATS[ent.type];
    int footprintScale = std::max(1, std::max(stats.sizeW, stats.sizeH));
    return std::max(fallbackSize, fallbackSize * footprintScale);
}

static void isoEntityFootprintCenter(const Entity& ent, int fallbackCx, int fallbackCy, int& cx, int& cy) {
    if (!entityHasMultiTileFootprint(ent)) {
        cx = fallbackCx;
        cy = fallbackCy;
        return;
    }
    const EntityStats& stats = STATS[ent.type];
    isoTileCenterFloat(ent.x + (stats.sizeW - 1) * 0.5f,
                       ent.y + (stats.sizeH - 1) * 0.5f,
                       cx, cy);
}

static int isoEntityFeetAnchorY(int tileCenterY) {
    return tileCenterY + isoHalfH();
}

static int isoEntityFootprintAnchorY(const Entity& ent, int footprintCenterY) {
    if (!entityHasMultiTileFootprint(ent)) return isoEntityFeetAnchorY(footprintCenterY);
    const EntityStats& stats = STATS[ent.type];
    return footprintCenterY + (int)std::lround((stats.sizeW + stats.sizeH) * isoHalfH() * 0.5f);
}

static int entityFootprintHpBarWidth(const Entity& ent) {
    if (!entityHasMultiTileFootprint(ent)) return std::max(8, s.tile);
    const EntityStats& stats = STATS[ent.type];
    return std::max(8, s.tile * std::max(stats.sizeW, stats.sizeH));
}

static void drawEntityHpBarAt(const Entity& ent, int cx, int cy, int width) {
    if (!ent.alive || ent.hp >= ent.maxHp) return;
    int barW = std::max(8, width);
    SDL_Rect hb{cx - barW / 2, cy, barW, 3};
    setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
    hb.w = std::max(1, barW * ent.hp / std::max(1, ent.maxHp));
    setDraw(ent.hp*3 > ent.maxHp*2 ? rgb(65,230,90) : ent.hp*3 > ent.maxHp ? rgb(230,210,70) : rgb(230,60,55));
    SDL_RenderFillRect(s.ren, &hb);
}

static void drawEntitySpriteAt(Game& game, const WorldIndex& world, const Entity& ent,
                               const SpriteMotionSample& motion, int cx, int cy, bool isometric) {
    const EntityActionAnimationSpec* anim = entityActionAnimationSpecFor(game, world, ent);
    int glyphSize = std::max(12, (int)(s.tile * (imageTilesetEnabled() ? 1.55f : 0.96f)));
    glyphSize = entityFootprintSpriteSize(ent, glyphSize);
    int entityCx = cx;
    int entityCy = cy;
    if (isometric) isoEntityFootprintCenter(ent, cx, cy, entityCx, entityCy);
    int lift = motion.liftPx;
    SDL_Rect fallbackRect{entityCx - glyphSize / 2, entityCy - glyphSize / 2 - lift, glyphSize, glyphSize};
    SDL_Rect imageDrawRect = fallbackRect;
    Color mod = applyVisionToGlyph(rgb(255,255,255), ent.x, ent.y);
    int anchorY = isometric ? isoEntityFootprintAnchorY(ent, entityCy) : entityCy;
    bool drewImage = drawEntityImageAtAnchor(game, world, ent,
                                             entityCx, anchorY - lift, glyphSize, glyphSize, mod,
                                             anim ? anim->action : nullptr,
                                             nullptr,
                                             motion.explicitFrame,
                                             SDL_Color{0,0,0,0},
                                             nullptr,
                                             &imageDrawRect,
                                             motion.angleDegrees);
    if (!drewImage) {
        bool usesSymbolFont = false;
        std::string glyph = tilesetEntityVisual(game, world, ent, usesSymbolFont);
        Color fg = ent.owner == OWNER_NATURE ? rgb(245,245,235) : rgb(255,255,255);
        drawCentered(glyph, fallbackRect, applyVisionToGlyph(fg, ent.x, ent.y), usesSymbolFont, usesSymbolFont);
    }
    if (isometric) {
        drawEntityHpBarAt(ent, entityCx, anchorY - 5, entityFootprintHpBarWidth(ent));
    } else {
        drawEntityHpBarAt(ent, entityCx, entityCy + s.tile / 2 - 4, std::max(8, s.tile - 4));
    }
}

static void drawProjectileSpriteAt(const ProjectileRenderInfo& projectile, const SpriteMotionSample& motion,
                                   int cx, int cy, bool isometric) {
    int glyphSize = std::max(10, (int)std::lround(s.tile * 0.66f));
    int shadowW = std::max(3, isometric ? isoHalfW() / 7 : s.tile / 8);
    int shadowH = std::max(2, isometric ? isoHalfH() / 7 : s.tile / 12);
    if (isometric) fillDiamond(cx, cy, shadowW, shadowH, rgb(20, 18, 12, 80));
    else {
        SDL_Rect shadow{cx - shadowW, cy + s.tile / 7, shadowW * 2, shadowH * 2};
        setDraw(rgb(20,18,12,70)); SDL_RenderFillRect(s.ren, &shadow);
    }
    SDL_Rect gr{cx - glyphSize / 2, cy - glyphSize / 2 - motion.liftPx, glyphSize, glyphSize};
    if (imageTilesetEnabled()) {
        TilesetAssetFrame frame = tilesetLoadProjectileTileScaled(s.ren, projectile.type, gr.w, gr.h);
        if (frame.texture && !frame.placeholder) {
            Color mod = applyVisionToGlyph(rgb(255,255,255), projectile.tileX, projectile.tileY);
            SDL_SetTextureColorMod(frame.texture, mod.r, mod.g, mod.b);
            SDL_SetTextureAlphaMod(frame.texture, mod.a);
            SDL_RenderCopy(s.ren, frame.texture, nullptr, &gr);
            SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
            SDL_SetTextureAlphaMod(frame.texture, 255);
            return;
        }
    }
    drawCentered(std::string(1, projectile.glyph), gr,
                 applyVisionToGlyph(projectileGlyphColor(projectile.color), projectile.tileX, projectile.tileY),
                 false, false);
}

struct SpriteDrawItem {
    enum class Kind { Entity, Projectile, FeatureFront } kind = Kind::Entity;
    int entityId = -1;
    int projectileIndex = -1;
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    int tileX = -1;
    int tileY = -1;
    SpriteMotionSample motion;
};

struct IsoMapLayerCache {
    SDL_Texture* texture = nullptr;
    bool valid = false;
    bool unavailable = false;
    int winW = 0;
    int winH = 0;
    SDL_Rect mapRect{0, 0, 0, 0};
    int viewX = 0;
    int viewY = 0;
    int viewW = 0;
    int viewH = 0;
    int tile = 0;
    int mode = 0;
    int isoViewMilliX = 0;
    int isoViewMilliY = 0;
    unsigned staticLayerHash = 0;
};

static IsoMapLayerCache isoMapLayerCache;

struct IsoTileDrawItem {
    int mx = 0;
    int my = 0;
    int cx = 0;
    int cy = 0;
};

static void cacheHashMix(unsigned& h, unsigned value) {
    h ^= value;
    h *= 16777619u;
}

static unsigned hashIsoStaticLayerForCache(const RenderModel& model) {
    unsigned h = 2166136261u;
    cacheHashMix(h, (unsigned)std::max(0, std::min(255, (int)std::lround(getBrightness(g) * 64.0f))));
    cacheHashMix(h, (unsigned)getSeason(g));
    cacheHashMix(h, (unsigned)std::max(0, std::min(255, (int)std::lround(getSeasonProgress(g) * 255.0f))));
    cacheHashMix(h, (unsigned)g.weather);
    for (const TileRenderInfo& info : model.tiles) {
        cacheHashMix(h, (unsigned)(info.x + 257 * info.y));
        cacheHashMix(h, (unsigned)info.terrain);
        cacheHashMix(h, info.visible ? 1u : 0u);
        cacheHashMix(h, info.explored ? 3u : 0u);
        cacheHashMix(h, (unsigned)info.visualParts.ground);
        cacheHashMix(h, (unsigned)info.visualParts.feature);
        cacheHashMix(h, (unsigned)info.visualParts.featureState);
        cacheHashMix(h, info.gateOpen ? 5u : 0u);
        cacheHashMix(h, info.gateLocked ? 7u : 0u);
        for (VisualDecalType decal : info.visualParts.decals) {
            cacheHashMix(h, (unsigned)decal + 11u);
        }
    }
    for (const EntityRenderInfo& info : model.entities) {
        if (!info.visible || (isUnit(info.type) && !isBuilding(info.type))) continue;
        cacheHashMix(h, (unsigned)info.id);
        cacheHashMix(h, (unsigned)info.type);
        cacheHashMix(h, (unsigned)(info.x + 257 * info.y));
        cacheHashMix(h, (unsigned)info.owner);
        cacheHashMix(h, (unsigned)info.state);
        cacheHashMix(h, info.underConstruction ? 13u : 0u);
        cacheHashMix(h, (unsigned)info.buildingState);
        cacheHashMix(h, (unsigned)info.animalCarcassState);
        cacheHashMix(h, (unsigned)info.transportState);
    }
    return h;
}

static int cameraMilli(float value) {
    return (int)std::lround(value * 1000.0f);
}

static bool isoMapLayerCacheMatches(SDL_Rect mr, unsigned staticLayerHash) {
    return isoMapLayerCache.texture
        && isoMapLayerCache.valid
        && !isoMapLayerCache.unavailable
        && isoMapLayerCache.winW == s.winW
        && isoMapLayerCache.winH == s.winH
        && isoMapLayerCache.mapRect.x == mr.x
        && isoMapLayerCache.mapRect.y == mr.y
        && isoMapLayerCache.mapRect.w == mr.w
        && isoMapLayerCache.mapRect.h == mr.h
        && isoMapLayerCache.viewX == view.viewX
        && isoMapLayerCache.viewY == view.viewY
        && isoMapLayerCache.viewW == view.viewW
        && isoMapLayerCache.viewH == view.viewH
        && isoMapLayerCache.tile == s.tile
        && isoMapLayerCache.mode == (int)g.mode
        && isoMapLayerCache.isoViewMilliX == cameraMilli(s.isoCameraActive ? s.isoViewX : (float)view.viewX)
        && isoMapLayerCache.isoViewMilliY == cameraMilli(s.isoCameraActive ? s.isoViewY : (float)view.viewY)
        && isoMapLayerCache.staticLayerHash == staticLayerHash;
}

static bool ensureIsoMapLayerCacheTexture() {
    if (isoMapLayerCache.unavailable) return false;
    if (isoMapLayerCache.texture
        && isoMapLayerCache.winW == s.winW
        && isoMapLayerCache.winH == s.winH) {
        return true;
    }
    clearIsoMapLayerCache();
    isoMapLayerCache.texture = SDL_CreateTexture(s.ren, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_TARGET, s.winW, s.winH);
    if (!isoMapLayerCache.texture) {
        isoMapLayerCache.unavailable = true;
        std::cerr << "realm: isometric map layer cache disabled: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetTextureBlendMode(isoMapLayerCache.texture, SDL_BLENDMODE_BLEND);
    isoMapLayerCache.winW = s.winW;
    isoMapLayerCache.winH = s.winH;
    return true;
}

static void updateIsoMapLayerCacheKey(SDL_Rect mr, unsigned staticLayerHash) {
    isoMapLayerCache.valid = true;
    isoMapLayerCache.mapRect = mr;
    isoMapLayerCache.viewX = view.viewX;
    isoMapLayerCache.viewY = view.viewY;
    isoMapLayerCache.viewW = view.viewW;
    isoMapLayerCache.viewH = view.viewH;
    isoMapLayerCache.tile = s.tile;
    isoMapLayerCache.mode = (int)g.mode;
    isoMapLayerCache.isoViewMilliX = cameraMilli(s.isoCameraActive ? s.isoViewX : (float)view.viewX);
    isoMapLayerCache.isoViewMilliY = cameraMilli(s.isoCameraActive ? s.isoViewY : (float)view.viewY);
    isoMapLayerCache.staticLayerHash = staticLayerHash;
}

static std::vector<IsoTileDrawItem> buildIsoTileDrawItems(IsoOffsetBounds b) {
    RealmProfileScope scope("map.iso_tile_draw_items");
    std::vector<IsoTileDrawItem> items;
    int minSum = b.minSx + b.minSy;
    int maxSum = b.maxSx + b.maxSy;
    int approxW = std::max(0, b.maxSx - b.minSx + 1);
    int approxH = std::max(0, b.maxSy - b.minSy + 1);
    items.reserve((size_t)approxW * (size_t)approxH);
    for (int sum = minSum; sum <= maxSum; ++sum) {
        for (int sy = b.minSy; sy <= b.maxSy; ++sy) {
            int sx = sum - sy;
            if (sx < b.minSx || sx > b.maxSx) continue;
            int mx = view.viewX + sx, my = view.viewY + sy;
            if (!inBounds(mx, my)) continue;
            IsoTileDrawItem item;
            item.mx = mx;
            item.my = my;
            isoTileCenterFromScreenOffset(sx, sy, item.cx, item.cy);
            items.push_back(item);
        }
    }
    return items;
}

void clearIsoMapLayerCache() {
    if (isoMapLayerCache.texture) SDL_DestroyTexture(isoMapLayerCache.texture);
    isoMapLayerCache = IsoMapLayerCache{};
}

static float entityTileDepthFraction() {
    return 0.5f;
}

static float projectileTileDepthFraction() {
    return 0.5f;
}

static float featureFrontDepthFraction(FeatureType feature) {
    switch (feature) {
        case F_FOREST:
        case F_PINE:
        case F_REEDS:
            return 0.5f;
        default:
            return 0.5f;
    }
}

static int spriteDrawKindOrder(SpriteDrawItem::Kind kind) {
    switch (kind) {
        case SpriteDrawItem::Kind::Entity: return 0;
        case SpriteDrawItem::Kind::Projectile: return 1;
        case SpriteDrawItem::Kind::FeatureFront: return 2;
    }
    return 0;
}

static bool tileNeedsFeatureFront(const TileRenderInfo& info) {
    return info.visible && featureConceals(info.visualParts.feature);
}

static std::vector<SpriteDrawItem> buildSpriteDrawItems(Game& game, const WorldIndex& world, const RenderModel& model) {
    RealmProfileScope scope("map.sprite_items");
    syncEntityMotionCache(game, model);
    syncProjectileMotionCache(game, model);

    std::vector<SpriteDrawItem> items;
    for (const TileRenderInfo& info : model.tiles) {
        if (!tileNeedsFeatureFront(info)) continue;
        SpriteDrawItem item;
        item.kind = SpriteDrawItem::Kind::FeatureFront;
        item.x = (float)info.x;
        item.y = (float)info.y;
        item.tileX = info.x;
        item.tileY = info.y;
        item.depth = item.x + item.y + featureFrontDepthFraction(info.visualParts.feature);
        items.push_back(item);
    }
    for (const EntityRenderInfo& info : model.entities) {
        Entity* ent = renderFindEntity(game, world, info.id);
        if (!handledByTilesetSpritePass(ent) || !info.visible) continue;
        const EntityActionAnimationSpec* anim = entityActionAnimationSpecFor(game, world, *ent);
        SpriteMotionSample motion = sampleEntityMotion(*ent, anim);
        SpriteDrawItem item;
        item.kind = SpriteDrawItem::Kind::Entity;
        item.entityId = ent->id;
        item.x = motion.x;
        item.y = motion.y;
        item.depth = motion.x + motion.y + entityTileDepthFraction();
        item.motion = motion;
        items.push_back(item);
    }
    for (int i = 0; i < (int)model.projectiles.size(); ++i) {
        const ProjectileRenderInfo& projectile = model.projectiles[i];
        if (!projectile.visible || !projectile.alive) continue;
        SpriteMotionSample motion = sampleProjectileMotion(projectile, i);
        SpriteDrawItem item;
        item.kind = SpriteDrawItem::Kind::Projectile;
        item.projectileIndex = i;
        item.x = motion.x;
        item.y = motion.y;
        item.depth = motion.x + motion.y + projectileTileDepthFraction();
        item.motion = motion;
        items.push_back(item);
    }
    std::sort(items.begin(), items.end(), [](const SpriteDrawItem& a, const SpriteDrawItem& b) {
        if (a.depth != b.depth) return a.depth < b.depth;
        if (a.y != b.y) return a.y < b.y;
        int ak = spriteDrawKindOrder(a.kind);
        int bk = spriteDrawKindOrder(b.kind);
        if (ak != bk) return ak < bk;
        return a.entityId < b.entityId;
    });
    return items;
}

static void drawFeatureFrontIso(Game& game, const WorldIndex& world, int mx, int my) {
    int cx = 0, cy = 0;
    isoTileCenterFloat((float)mx, (float)my, cx, cy);
    SDL_Rect featureRect = featureSpriteRectIso(cx, cy, isoHalfW(), isoHalfH());
    drawFeatureOccluderIfNeeded(game, world, mx, my, featureRect);
}

static void drawFeatureFrontTopDown(Game& game, const WorldIndex& world, int mx, int my) {
    SDL_Rect mr = mapRect();
    SDL_Rect r{mr.x + (mx - view.viewX) * s.tile,
               mr.y + (my - view.viewY) * s.tile,
               s.tile, s.tile};
    drawFeatureOccluderIfNeeded(game, world, mx, my, r);
}

static void drawMovingSpritesIso(Game& game, const WorldIndex& world, const RenderModel& model) {
    if (!tilesetMovingSpritePassEnabled()) return;
    RealmProfileScope scope("map.moving_sprites_iso");
    std::vector<SpriteDrawItem> items = buildSpriteDrawItems(game, world, model);
    for (const SpriteDrawItem& item : items) {
        if (item.kind == SpriteDrawItem::Kind::FeatureFront) {
            drawFeatureFrontIso(game, world, item.tileX, item.tileY);
            continue;
        }
        int cx = 0, cy = 0;
        isoTileCenterFloat(item.x, item.y, cx, cy);
        if (item.kind == SpriteDrawItem::Kind::Entity) {
            Entity* ent = renderFindEntity(game, world, item.entityId);
            if (ent) drawEntitySpriteAt(game, world, *ent, item.motion, cx, cy, true);
        } else if (item.projectileIndex >= 0 && item.projectileIndex < (int)model.projectiles.size()) {
            drawProjectileSpriteAt(model.projectiles[item.projectileIndex], item.motion, cx, cy, true);
        }
    }
}

static void drawMovingSpritesTopDown(Game& game, const WorldIndex& world, const RenderModel& model) {
    if (!tilesetMovingSpritePassEnabled()) return;
    RealmProfileScope scope("map.moving_sprites_top_down");
    std::vector<SpriteDrawItem> items = buildSpriteDrawItems(game, world, model);
    for (const SpriteDrawItem& item : items) {
        if (item.kind == SpriteDrawItem::Kind::FeatureFront) {
            drawFeatureFrontTopDown(game, world, item.tileX, item.tileY);
            continue;
        }
        int cx = 0, cy = 0;
        topDownTileCenterFloat(item.x, item.y, cx, cy);
        if (item.kind == SpriteDrawItem::Kind::Entity) {
            Entity* ent = renderFindEntity(game, world, item.entityId);
            if (ent) drawEntitySpriteAt(game, world, *ent, item.motion, cx, cy, false);
        } else if (item.projectileIndex >= 0 && item.projectileIndex < (int)model.projectiles.size()) {
            drawProjectileSpriteAt(model.projectiles[item.projectileIndex], item.motion, cx, cy, false);
        }
    }
}

TileVisual makeTileVisual(Game& game, const WorldIndex& world, const RenderModel& model, int mx, int my) {
    TileVisual v;
    const Tile& tile = game.map[my][mx];
    const TileRenderInfo* tileInfo = tileInfoAt(model, mx, my);
    v.visible = tileInfo ? tileInfo->visible : tile.visible[0];
    v.explored = tileInfo ? tileInfo->explored : tile.explored[0];
    v.cursor = (mx == view.cursorX && my == view.cursorY);
    if (!v.explored) { v.bg = rgb(8,9,12); return v; }

    v.ent = v.visible ? renderEntityAt(game, world, mx, my) : nullptr;
    if (!v.ent && v.visible) v.ent = renderCorpseAt(game, world, mx, my);
    v.projectile = (v.visible && !v.ent) ? projectileAt(model, mx, my) : nullptr;
    v.bg = terrainBg(tile, mx, my);

    if (v.ent && v.ent->alive && v.ent->owner != OWNER_NATURE && (isUnit(v.ent->type) || isBuilding(v.ent->type)))
        v.bg = timeTint(ownerBg(v.ent->owner));
    v.bg = applyVisionAndLight(v.bg, mx, my);
    if (v.cursor && !tilesetIndicatorsEnabled()) v.bg = blend(v.bg, rgb(225, 190, 50), 0.78f);

    v.fg = glyphColorForTerrain(tile, mx, my);
    if (displayMode == DM_ASCII) {
        v.emoji = false;
        if (v.visible && v.ent && v.ent->alive) {
            v.glyph.assign(1, STATS[v.ent->type].glyph);
            v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(230,230,210) : rgb(255,255,255);
        } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
            v.glyph.assign(1, v.ent->deathTicks >= DEATH_DECAY_TICKS ? '*' : '%');
            v.fg = rgb(180,180,170);
        } else if (v.visible && v.projectile) {
            v.glyph.assign(1, v.projectile->glyph);
            v.fg = projectileGlyphColor(v.projectile->color);
        } else if (v.visible) {
            v.glyph.assign(1, terrainAsciiGlyph(tile.terrain));
        } else {
            v.glyph = "."; v.fg = rgb(95,95,105,150);
        }
    } else if (v.visible && v.ent && v.ent->alive) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(game, world, *v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(245,245,235) : rgb(255,255,255);
        v.tint = usesSymbolFont;
    } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(game, world, *v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = rgb(190,190,180);
        v.tint = false;
    } else if (v.visible && v.projectile) {
        v.glyph.assign(1, v.projectile->glyph);
        v.emoji = false;
        v.tint = false;
        v.fg = projectileGlyphColor(v.projectile->color);
    } else if (v.visible) {
        logMissingTerrainImageTile(tile.terrain);
        logMissingVisualTileParts(tile);
        if (imageTilesetEnabled() && hasTerrainImageTile(tile.terrain)) {
            v.glyph.clear();
            v.emoji = false;
            v.tint = false;
        } else {
            v.glyph = terrainGlyph(tile, mx, my);
            v.emoji = isResourceEmojiTerrain(tile.terrain);
            v.tint = v.emoji;
        }
    } else {
        v.glyph = "·"; v.emoji = false; v.fg = rgb(95,95,105,150);
    }
    v.fg = applyVisionToGlyph(v.fg, mx, my);

    v.selected = (v.visible && v.ent && isSelected(v.ent));
    if (v.visible && !v.ent && !v.projectile && !tilesetIndicatorsEnabled()) {
        const ActionMarkerRenderInfo* marker = actionMarkerAt(model, mx, my);
        if (marker && marker->ticks > 0 && (g.tick % 6) < 4) {
            v.glyph = (marker->glyph == '#') ? u8"■" : (marker->glyph == '!') ? "!" : u8"×";
            v.emoji = false;
            v.tint = false;
            v.fg = rgb(255,235,105);
        }
    }
    return v;
}

void drawTile(Game& game, const WorldIndex& world, const RenderModel& model, int mx, int my, SDL_Rect r) {
    const Tile& tile = game.map[my][mx];
    TileVisual v = makeTileVisual(game, world, model, mx, my);

    if (!v.explored) {
        setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
        drawUnknownGroundTexture(r, mx, my);
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(24,28,34,120)); SDL_RenderDrawRect(s.ren, &r);
        return;
    }

    setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
    applyTerrainTexture(r, tile, mx, my);

    bool tileSpriteMovedToOverlay = handledByTilesetSpritePass(v.ent) || (tilesetMovingSpritePassEnabled() && v.projectile);
    if (!v.glyph.empty() && !tileSpriteMovedToOverlay) drawCentered(v.glyph, r, v.fg, v.emoji, v.tint);
    if (!tilesetMovingSpritePassEnabled()) drawFeatureOccluderIfNeeded(game, world, mx, my, r);

    // HP sliver for damaged visible entities.
    if (!tileSpriteMovedToOverlay && v.visible && v.ent && v.ent->alive && v.ent->hp < v.ent->maxHp) {
        int w = std::max(1, r.w * v.ent->hp / std::max(1, v.ent->maxHp));
        SDL_Rect hb{r.x+2, r.y+r.h-4, std::max(1, r.w-4), 2};
        setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
        hb.w = std::max(1, w-4);
        setDraw(v.ent->hp*3 > v.ent->maxHp*2 ? rgb(65,230,90) : v.ent->hp*3 > v.ent->maxHp ? rgb(230,210,70) : rgb(230,60,55));
        SDL_RenderFillRect(s.ren, &hb);
    }

    if (v.selected && !tilesetIndicatorsEnabled()) {
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(255,255,255,180));
        SDL_RenderDrawRect(s.ren, &r);
        SDL_Rect r2{r.x+1,r.y+1,r.w-2,r.h-2}; SDL_RenderDrawRect(s.ren, &r2);
    }
    if (v.cursor && !tilesetIndicatorsEnabled()) {
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(40,20,0,230));
        SDL_RenderDrawRect(s.ren, &r);
    }
}

void toggleFullscreen() {
#if defined(REALM_WEB)
    EM_ASM({
        if (typeof window !== 'undefined' && typeof window.realmToggleFullscreen === 'function') {
            window.realmToggleFullscreen();
        }
    });
#else
    s.fullscreen = !s.fullscreen;
    if (SDL_SetWindowFullscreen(s.win, s.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) != 0) {
        std::cerr << "realm: fullscreen toggle failed: " << SDL_GetError() << "\n";
        s.fullscreen = !s.fullscreen;
        return;
    }
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    updateViewMetrics(true);
    emitUiStatusEvent(-1, s.fullscreen ? "Fullscreen." : "Windowed.");
#endif
}

static EntityType activeBuildPreviewType() {
    if (s.mobileBuildType > E_NONE && s.mobileBuildType < E_TYPE_COUNT) return s.mobileBuildType;
    if (g.mode == M_BUILD_PLACE && g.local.buildPending > E_NONE && g.local.buildPending < E_TYPE_COUNT) {
        return g.local.buildPending;
    }
    return E_NONE;
}

struct CursorOverlayState {
    bool valid = false;
    bool optionsValid = false;
    int cursorX = -1;
    int cursorY = -1;
    int selectedId = -1;
    std::vector<int> selectedIds;
    GameMode mode = M_NORMAL;
    EntityType buildType = E_NONE;
    int tick = -1;
    CommandOptions options;
    CommandActionKind recommended = CommandActionKind::None;
    Uint32 hoverChangedTicks = 0;
};

static CursorOverlayState cursorOverlay;

struct CommandContextMenuState {
    bool open = false;
    CommandOptions options;
    std::vector<SDL_Rect> itemRects;
    SDL_Rect frame{0, 0, 0, 0};
    int targetX = -1;
    int targetY = -1;
    int anchorX = 0;
    int anchorY = 0;
    Uint32 openedTicks = 0;
};

static CommandContextMenuState commandContextMenu;

void commandContextMenuClose();

static bool commandOptionIsSelectable(const CommandOption& option) {
    return option.enabled && !commandIsEmpty(option.command);
}

static void layoutCommandContextMenu() {
    commandContextMenu.itemRects.clear();
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    int maxLabelW = textWidth("Commands", font);
    for (const CommandOption& option : commandContextMenu.options.options) {
        maxLabelW = std::max(maxLabelW, textWidth(option.label, font));
    }

    int rowH = std::max(24, textLineHeight(font) + 8);
    int pad = 8;
    int headerH = 24;
    int itemCount = std::max(1, (int)commandContextMenu.options.options.size());
    int w = std::max(144, std::min(240, maxLabelW + 44));
    int h = pad + headerH + itemCount * rowH + pad;
    int x = commandContextMenu.anchorX + 14;
    int y = commandContextMenu.anchorY - h / 2;

    if (x + w > s.winW - 8) x = commandContextMenu.anchorX - w - 14;
    if (x < 8) x = 8;
    if (y + h > s.winH - s.bottomH - 8) y = s.winH - s.bottomH - h - 8;
    if (y < s.topH + 8) y = s.topH + 8;

    commandContextMenu.frame = SDL_Rect{x, y, w, h};
    int rowY = y + pad + headerH;
    for (int i = 0; i < itemCount; ++i) {
        commandContextMenu.itemRects.push_back(SDL_Rect{x + pad, rowY + i * rowH, w - pad * 2, rowH - 2});
    }
}

void commandContextMenuOpen(const Game& game, const WorldIndex& world, const CommandPreviewRequest& request, int anchorX, int anchorY) {
    CommandOptions options = resolveCommandOptions(game, world, request);
    bool hasSelectable = false;
    for (const CommandOption& option : options.options) {
        if (commandOptionIsSelectable(option)) {
            hasSelectable = true;
            break;
        }
    }
    if (!hasSelectable) {
        commandContextMenuClose();
        return;
    }

    commandContextMenu.open = true;
    commandContextMenu.options = std::move(options);
    commandContextMenu.targetX = request.target.x;
    commandContextMenu.targetY = request.target.y;
    commandContextMenu.anchorX = anchorX;
    commandContextMenu.anchorY = anchorY;
    commandContextMenu.openedTicks = SDL_GetTicks();
    layoutCommandContextMenu();
}

void commandContextMenuClose() {
    commandContextMenu = CommandContextMenuState{};
}

bool commandContextMenuIsOpen() {
    return commandContextMenu.open;
}

int commandContextMenuOptionCount() {
    return commandContextMenu.open ? (int)commandContextMenu.options.options.size() : 0;
}

bool commandContextMenuTakeCommand(int px, int py, Command& outCommand) {
    if (!commandContextMenu.open) return false;
    for (int i = 0; i < (int)commandContextMenu.itemRects.size()
            && i < (int)commandContextMenu.options.options.size(); ++i) {
        if (!pointInRect(px, py, commandContextMenu.itemRects[i])) continue;
        const CommandOption& option = commandContextMenu.options.options[i];
        if (!commandOptionIsSelectable(option)) return false;
        outCommand = option.command;
        commandContextMenuClose();
        return true;
    }
    return false;
}

void drawCommandContextMenu() {
    if (!commandContextMenu.open) return;
    layoutCommandContextMenu();
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);

    SDL_Rect frame = commandContextMenu.frame;
    setDraw(rgb(2, 5, 8, 232));
    SDL_RenderFillRect(s.ren, &frame);
    setDraw(rgb(255, 226, 95, 235));
    SDL_RenderDrawRect(s.ren, &frame);
    SDL_Rect inner{frame.x + 1, frame.y + 1, frame.w - 2, frame.h - 2};
    setDraw(rgb(70, 92, 98, 210));
    SDL_RenderDrawRect(s.ren, &inner);

    std::ostringstream title;
    title << "Commands " << commandContextMenu.targetX << "," << commandContextMenu.targetY;
    drawTextFit(frame.x + 8, frame.y + 7, title.str(), rgb(255, 235, 145), frame.w - 16, font);

    if (commandContextMenu.options.options.empty()) {
        drawTextFit(frame.x + 8, frame.y + 32, "No commands", rgb(150, 160, 168), frame.w - 16, font);
        return;
    }

    for (int i = 0; i < (int)commandContextMenu.itemRects.size()
            && i < (int)commandContextMenu.options.options.size(); ++i) {
        const CommandOption& option = commandContextMenu.options.options[i];
        SDL_Rect row = commandContextMenu.itemRects[i];
        bool hovered = pointInRect(s.mouseX, s.mouseY, row);
        bool recommended = i == commandContextMenu.options.recommendedIndex;
        Color rowBg = hovered ? rgb(48, 66, 70, 238)
                    : recommended ? rgb(36, 48, 44, 224)
                    : rgb(14, 20, 24, 208);
        setDraw(rowBg);
        SDL_RenderFillRect(s.ren, &row);
        setDraw(hovered ? rgb(255, 235, 145, 230) : rgb(72, 88, 94, 190));
        SDL_RenderDrawRect(s.ren, &row);

        Color text = option.enabled ? (recommended ? rgb(255, 235, 145) : rgb(226, 232, 226))
                                    : rgb(116, 124, 130);
        std::string label = recommended ? std::string("> ") + option.label : std::string("  ") + option.label;
        drawTextFit(row.x + 8, row.y + 5, label, text, row.w - 16, font);
    }
}

static CommandPreviewMode overlayPreviewMode() {
    if (activeBuildPreviewType() != E_NONE) return CommandPreviewMode::BuildPlace;
    if (g.mode == M_RALLY_SET) return CommandPreviewMode::Rally;
    if (g.mode == M_ATTACK_MOVE) return CommandPreviewMode::AttackMove;
    if (g.mode == M_PATROL_SET) return CommandPreviewMode::Patrol;
    return CommandPreviewMode::Context;
}

static bool cursorOverlayInputsChanged(int cursorX, int cursorY, int selectedId,
                                       const std::vector<int>& selectedIds,
                                       GameMode mode, EntityType buildType, int tick) {
    return !cursorOverlay.optionsValid
        || cursorOverlay.cursorX != cursorX
        || cursorOverlay.cursorY != cursorY
        || cursorOverlay.selectedId != selectedId
        || cursorOverlay.selectedIds != selectedIds
        || cursorOverlay.mode != mode
        || cursorOverlay.buildType != buildType
        || cursorOverlay.tick != tick;
}

static void updateCursorOverlayState(const Game& game, const WorldIndex& world) {
    cursorOverlay.valid = tilesetIndicatorsEnabled() && inBounds(view.cursorX, view.cursorY);
    if (!cursorOverlay.valid) return;

    EntityType buildType = activeBuildPreviewType();
    bool hoverMoved = cursorOverlay.cursorX != view.cursorX || cursorOverlay.cursorY != view.cursorY;
    if (hoverMoved || cursorOverlay.hoverChangedTicks == 0) {
        cursorOverlay.hoverChangedTicks = SDL_GetTicks();
    }

    if (!cursorOverlayInputsChanged(view.cursorX, view.cursorY, game.local.selectedId,
                                    game.local.selectedIds, game.mode, buildType, game.tick)) {
        return;
    }

    CommandPreviewRequest request;
    request.issuer = 0;
    request.selection = currentSelection(game);
    request.target = {view.cursorX, view.cursorY};
    request.mode = overlayPreviewMode();
    request.buildType = buildType;

    cursorOverlay.cursorX = view.cursorX;
    cursorOverlay.cursorY = view.cursorY;
    cursorOverlay.selectedId = game.local.selectedId;
    cursorOverlay.selectedIds = game.local.selectedIds;
    cursorOverlay.mode = game.mode;
    cursorOverlay.buildType = buildType;
    cursorOverlay.tick = game.tick;
    cursorOverlay.options = resolveCommandOptions(game, world, request);
    const CommandOption* recommended = recommendedCommandOption(cursorOverlay.options);
    cursorOverlay.recommended = recommended ? recommended->kind : CommandActionKind::None;
    cursorOverlay.optionsValid = true;
}

static std::vector<int> selectedOverlayIds() {
    std::vector<int> ids = g.local.selectedIds;
    if (ids.empty() && g.local.selectedId >= 0) ids.push_back(g.local.selectedId);
    else if (g.local.selectedId >= 0 && std::find(ids.begin(), ids.end(), g.local.selectedId) == ids.end()) {
        ids.push_back(g.local.selectedId);
    }
    return ids;
}

static bool dragSelectionBounds(int& x0, int& y0, int& x1, int& y1) {
    if (!s.leftDown) return false;
    x0 = std::max(0, std::min(s.dragStartX, view.cursorX));
    x1 = std::min(MAP_W - 1, std::max(s.dragStartX, view.cursorX));
    y0 = std::max(0, std::min(s.dragStartY, view.cursorY));
    y1 = std::min(MAP_H - 1, std::max(s.dragStartY, view.cursorY));
    return x0 <= x1 && y0 <= y1;
}

static void drawPathArrowHead(int fromX, int fromY, int tipX, int tipY, Color c, int thickness) {
    float dx = (float)(tipX - fromX);
    float dy = (float)(tipY - fromY);
    float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    float ux = dx / len;
    float uy = dy / len;
    float px = -uy;
    float py = ux;
    float headLen = std::max(7.0f, s.tile * 0.28f);
    float headW = std::max(5.0f, s.tile * 0.18f);
    int ax = (int)std::lround(tipX - ux * headLen + px * headW);
    int ay = (int)std::lround(tipY - uy * headLen + py * headW);
    int bx = (int)std::lround(tipX - ux * headLen - px * headW);
    int by = (int)std::lround(tipY - uy * headLen - py * headW);
    drawThickLine(tipX, tipY, ax, ay, c, thickness);
    drawThickLine(tipX, tipY, bx, by, c, thickness);
}

static void drawRightDragPath(const std::vector<SDL_Point>& points) {
    if (!s.rightDown || s.rightDragPath.size() < 2 || points.size() < 2) return;
    int outline = indicatorOutlineStroke();
    int inner = indicatorInnerStroke();
    Color shadow = indicatorOutlineColor(235);
    Color blue = indicatorPathColor(235);
    for (size_t i = 1; i < points.size(); ++i) {
        drawThickLine(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, shadow, outline);
    }
    for (size_t i = 1; i < points.size(); ++i) {
        drawThickLine(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, blue, inner);
    }
    drawPathArrowHead(points[points.size() - 2].x, points[points.size() - 2].y,
                      points.back().x, points.back().y, shadow, outline);
    drawPathArrowHead(points[points.size() - 2].x, points[points.size() - 2].y,
                      points.back().x, points.back().y, blue, inner);
}

static void drawRightDragPathIso() {
    if (!s.rightDown || s.rightDragPath.size() < 2) return;
    std::vector<SDL_Point> points;
    points.reserve(s.rightDragPath.size());
    for (auto point : s.rightDragPath) {
        int cx = 0, cy = 0;
        isoTileCenterFromScreenOffset(point.first - view.viewX, point.second - view.viewY, cx, cy);
        points.push_back(SDL_Point{cx, cy});
    }
    drawRightDragPath(points);
}

static void drawRightDragPathTopDown() {
    if (!s.rightDown || s.rightDragPath.size() < 2) return;
    SDL_Rect mr = mapRect();
    std::vector<SDL_Point> points;
    points.reserve(s.rightDragPath.size());
    for (auto point : s.rightDragPath) {
        int sx = point.first - view.viewX;
        int sy = point.second - view.viewY;
        points.push_back(SDL_Point{mr.x + sx * s.tile + s.tile / 2,
                                   mr.y + sy * s.tile + s.tile / 2});
    }
    drawRightDragPath(points);
}

static void drawFootprintCornersIsoBounds(int x0, int y0, int x1, int y1, bool backLayer) {
    int hw = isoHalfW(), hh = isoHalfH();
    int outX = std::max(3, (int)std::lround(hw * 0.12f));
    int outY = std::max(2, (int)std::lround(hh * 0.12f));
    int legX = std::max(7, (int)std::lround(hw * 0.34f));
    int legY = std::max(4, (int)std::lround(hh * 0.34f));
    float pulse = indicatorPulse();
    Color yellow = indicatorYellowColor(210 + (int)std::lround(pulse * 35.0f));

    auto tileCenter = [](int mx, int my, int& cx, int& cy) {
        isoTileCenterFromScreenOffset(mx - view.viewX, my - view.viewY, cx, cy);
    };

    int topCx = 0, topCy = 0, rightCx = 0, rightCy = 0, bottomCx = 0, bottomCy = 0, leftCx = 0, leftCy = 0;
    tileCenter(x0, y0, topCx, topCy);
    tileCenter(x1, y0, rightCx, rightCy);
    tileCenter(x1, y1, bottomCx, bottomCy);
    tileCenter(x0, y1, leftCx, leftCy);

    int topX = topCx, topY = topCy - hh - outY;
    int rightX = rightCx + hw + outX, rightY = rightCy;
    int bottomX = bottomCx, bottomY = bottomCy + hh + outY;
    int leftX = leftCx - hw - outX, leftY = leftCy;

    auto pass = [&](Color color, int thickness) {
        if (backLayer) {
            drawThickLine(topX, topY, topX - legX, topY + legY, color, thickness);
            drawThickLine(topX, topY, topX + legX, topY + legY, color, thickness);
            drawThickLine(rightX, rightY, rightX - legX, rightY - legY, color, thickness);
            drawThickLine(leftX, leftY, leftX + legX, leftY - legY, color, thickness);
        } else {
            drawThickLine(rightX, rightY, rightX - legX, rightY + legY, color, thickness);
            drawThickLine(bottomX, bottomY, bottomX - legX, bottomY - legY, color, thickness);
            drawThickLine(bottomX, bottomY, bottomX + legX, bottomY - legY, color, thickness);
            drawThickLine(leftX, leftY, leftX + legX, leftY + legY, color, thickness);
        }
    };

    pass(indicatorOutlineColor(), indicatorOutlineStroke());
    pass(yellow, indicatorInnerStroke());
}

static void drawDragSelectionCornersIso(bool backLayer) {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!dragSelectionBounds(x0, y0, x1, y1)) return;
    drawFootprintCornersIsoBounds(x0, y0, x1, y1, backLayer);
}

static void drawDragSelectionCornersTopDown() {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!dragSelectionBounds(x0, y0, x1, y1)) return;
    SDL_Rect mr = mapRect();
    SDL_Rect r{mr.x + (x0 - view.viewX) * s.tile,
               mr.y + (y0 - view.viewY) * s.tile,
               (x1 - x0 + 1) * s.tile,
               (y1 - y0 + 1) * s.tile};
    drawHoverCornersRect(r);
}

static size_t selectionMaskIndex(int x, int y) {
    return (size_t)y * (size_t)MAP_W + (size_t)x;
}

static bool selectionMaskAt(const std::vector<unsigned char>& mask, int x, int y) {
    return inBounds(x, y) && mask[selectionMaskIndex(x, y)] != 0;
}

static void markTileMask(std::vector<unsigned char>& mask, int x, int y) {
    if (inBounds(x, y)) mask[selectionMaskIndex(x, y)] = 1;
}

static void markEntityFootprintMask(std::vector<unsigned char>& mask, const Entity& ent) {
    for (int dy = 0; dy < entityFootprintHeight(ent); ++dy) {
        for (int dx = 0; dx < entityFootprintWidth(ent); ++dx) {
            markTileMask(mask, ent.x + dx, ent.y + dy);
        }
    }
}

static bool maskBounds(const std::vector<unsigned char>& mask, int& x0, int& y0, int& x1, int& y1) {
    x0 = MAP_W;
    y0 = MAP_H;
    x1 = -1;
    y1 = -1;
    for (int my = 0; my < MAP_H; ++my) {
        for (int mx = 0; mx < MAP_W; ++mx) {
            if (!selectionMaskAt(mask, mx, my)) continue;
            x0 = std::min(x0, mx);
            y0 = std::min(y0, my);
            x1 = std::max(x1, mx);
            y1 = std::max(y1, my);
        }
    }
    return x0 <= x1 && y0 <= y1;
}

static std::vector<unsigned char> selectedFootprintMask(const WorldIndex& world) {
    std::vector<unsigned char> mask((size_t)MAP_W * (size_t)MAP_H, 0);
    for (int id : selectedOverlayIds()) {
        Entity* ent = renderFindEntity(g, world, id);
        if (!ent || !ent->alive || !entityFootprintHasVisibleTile(*ent)) continue;
        markEntityFootprintMask(mask, *ent);
    }
    return mask;
}

static std::vector<unsigned char> cursorFootprintMask(const WorldIndex& world) {
    std::vector<unsigned char> mask((size_t)MAP_W * (size_t)MAP_H, 0);
    if (!cursorOverlay.valid || !inBounds(cursorOverlay.cursorX, cursorOverlay.cursorY)) return mask;

    Entity* ent = renderEntityAt(g, world, cursorOverlay.cursorX, cursorOverlay.cursorY);
    if (ent && ent->alive && ent->state != S_GARRISONED && entityFootprintHasVisibleTile(*ent)) {
        markEntityFootprintMask(mask, *ent);
    } else {
        markTileMask(mask, cursorOverlay.cursorX, cursorOverlay.cursorY);
    }
    return mask;
}

static void drawSelectionOverlayIsoPass(const std::vector<unsigned char>& mask, bool backLayer,
                                        Color color, int thickness, int dash, int gap) {
    int hw = isoHalfW(), hh = isoHalfH();
    for (int my = 0; my < MAP_H; ++my) {
        for (int mx = 0; mx < MAP_W; ++mx) {
            if (!selectionMaskAt(mask, mx, my)) continue;
            int cx = 0, cy = 0;
            isoTileCenterFromScreenOffset(mx - view.viewX, my - view.viewY, cx, cy);
            int topX = cx, topY = cy - hh;
            int rightX = cx + hw, rightY = cy;
            int bottomX = cx, bottomY = cy + hh;
            int leftX = cx - hw, leftY = cy;
            if (backLayer) {
                if (!selectionMaskAt(mask, mx, my - 1)) {
                    drawDottedThickLine(topX, topY, rightX, rightY, color, thickness, dash, gap);
                }
                if (!selectionMaskAt(mask, mx - 1, my)) {
                    drawDottedThickLine(leftX, leftY, topX, topY, color, thickness, dash, gap);
                }
            } else {
                if (!selectionMaskAt(mask, mx + 1, my)) {
                    drawDottedThickLine(rightX, rightY, bottomX, bottomY, color, thickness, dash, gap);
                }
                if (!selectionMaskAt(mask, mx, my + 1)) {
                    drawDottedThickLine(bottomX, bottomY, leftX, leftY, color, thickness, dash, gap);
                }
            }
        }
    }
}

static void drawSelectionOverlayIso(const WorldIndex& world, bool backLayer) {
    std::vector<unsigned char> mask = selectedFootprintMask(world);
    int dash = std::max(4, (int)std::lround(s.tile * 0.12f));
    int gap = std::max(3, (int)std::lround(s.tile * 0.08f));
    drawSelectionOverlayIsoPass(mask, backLayer, indicatorOutlineColor(), indicatorOutlineStroke(), dash, gap);
    drawSelectionOverlayIsoPass(mask, backLayer, indicatorYellowColor(235), indicatorInnerStroke(), dash, gap);
}

static void drawSelectionOverlayTopDownPass(const std::vector<unsigned char>& mask,
                                            Color color, int thickness, int dash, int gap) {
    SDL_Rect mr = mapRect();
    for (int my = 0; my < MAP_H; ++my) {
        for (int mx = 0; mx < MAP_W; ++mx) {
            if (!selectionMaskAt(mask, mx, my)) continue;
            int x0 = mr.x + (mx - view.viewX) * s.tile;
            int y0 = mr.y + (my - view.viewY) * s.tile;
            int x1 = x0 + s.tile;
            int y1 = y0 + s.tile;
            if (!selectionMaskAt(mask, mx, my - 1)) {
                drawDottedThickLine(x0, y0, x1, y0, color, thickness, dash, gap);
            }
            if (!selectionMaskAt(mask, mx + 1, my)) {
                drawDottedThickLine(x1, y0, x1, y1, color, thickness, dash, gap);
            }
            if (!selectionMaskAt(mask, mx, my + 1)) {
                drawDottedThickLine(x1, y1, x0, y1, color, thickness, dash, gap);
            }
            if (!selectionMaskAt(mask, mx - 1, my)) {
                drawDottedThickLine(x0, y1, x0, y0, color, thickness, dash, gap);
            }
        }
    }
}

static void drawSelectionOverlayTopDown(const WorldIndex& world) {
    std::vector<unsigned char> mask = selectedFootprintMask(world);
    int dash = std::max(4, (int)std::lround(s.tile * 0.12f));
    int gap = std::max(3, (int)std::lround(s.tile * 0.08f));
    drawSelectionOverlayTopDownPass(mask, indicatorOutlineColor(), indicatorOutlineStroke(), dash, gap);
    drawSelectionOverlayTopDownPass(mask, indicatorYellowColor(235), indicatorInnerStroke(), dash, gap);
}

static void drawCommandMenuTargetTriangleIso() {
    if (!commandContextMenu.open || !inBounds(commandContextMenu.targetX, commandContextMenu.targetY)) return;
    if (!g.map[commandContextMenu.targetY][commandContextMenu.targetX].visible[0]) return;
    int cx = 0, cy = 0;
    isoTileCenterFromScreenOffset(commandContextMenu.targetX - view.viewX,
                                  commandContextMenu.targetY - view.viewY, cx, cy);
    int hh = isoHalfH();
    SDL_Rect mr = mapRect();
    int triW = std::max(8, (int)std::lround(s.tile * 0.34f));
    int triH = std::max(6, (int)std::lround(s.tile * 0.24f));
    int topY = cy - hh - triH - std::max(4, (int)std::lround(s.tile * 0.12f));
    if (topY < mr.y + 4) topY = cy + hh + 4;
    topY = std::max(mr.y + 4, std::min(topY, mr.y + mr.h - triH - indicatorOutlineStroke() * 2 - 4));
    int markerCx = std::max(mr.x + triW / 2 + 4, std::min(cx, mr.x + mr.w - triW / 2 - 4));
    drawSelectionTriangle(markerCx, topY, triW, triH);
}

static void drawCommandMenuTargetTriangleTopDown() {
    if (!commandContextMenu.open || !inBounds(commandContextMenu.targetX, commandContextMenu.targetY)) return;
    if (!g.map[commandContextMenu.targetY][commandContextMenu.targetX].visible[0]) return;
    SDL_Rect mr = mapRect();
    int sx = commandContextMenu.targetX - view.viewX;
    int sy = commandContextMenu.targetY - view.viewY;
    if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) return;
    int triW = std::max(8, (int)std::lround(s.tile * 0.34f));
    int triH = std::max(6, (int)std::lround(s.tile * 0.24f));
    int cx = mr.x + sx * s.tile + s.tile / 2;
    int topY = mr.y + sy * s.tile - triH - std::max(4, (int)std::lround(s.tile * 0.12f));
    if (topY < mr.y + 4) topY = mr.y + (sy + 1) * s.tile + 4;
    topY = std::max(mr.y + 4, std::min(topY, mr.y + mr.h - triH - indicatorOutlineStroke() * 2 - 4));
    int markerCx = std::max(mr.x + triW / 2 + 4, std::min(cx, mr.x + mr.w - triW / 2 - 4));
    drawSelectionTriangle(markerCx, topY, triW, triH);
}

static void drawActionMarkerTriangleIso(const ActionMarkerRenderInfo& marker) {
    if (marker.ticks <= 0) return;
    int cx = 0, cy = 0;
    isoTileCenterFromScreenOffset(marker.x - view.viewX, marker.y - view.viewY, cx, cy);
    SDL_Rect mr = mapRect();
    int triW = std::max(8, (int)std::lround(s.tile * 0.26f));
    int triH = std::max(6, (int)std::lround(s.tile * 0.20f));
    int gap = std::max(3, (int)std::lround(s.tile * 0.07f));
    int topY = cy - isoHalfH() - triH - gap;
    topY = std::max(mr.y + 4, std::min(topY, mr.y + mr.h - triH - indicatorOutlineStroke() * 2 - 4));
    int markerCx = std::max(mr.x + triW / 2 + 4, std::min(cx, mr.x + mr.w - triW / 2 - 4));
    drawActionMarkerIndicator(marker.glyph, markerCx, topY, triW, triH);
}

static void drawActionMarkersIso(const RenderModel& model) {
    for (const ActionMarkerRenderInfo& marker : model.actionMarkers) {
        const TileRenderInfo* tileInfo = tileInfoAt(model, marker.x, marker.y);
        if (!tileInfo || !tileInfo->visible) continue;
        drawActionMarkerTriangleIso(marker);
    }
}

static void drawActionMarkersTopDown(const RenderModel& model) {
    SDL_Rect mr = mapRect();
    int triW = std::max(8, (int)std::lround(s.tile * 0.26f));
    int triH = std::max(6, (int)std::lround(s.tile * 0.20f));
    int gap = std::max(3, (int)std::lround(s.tile * 0.07f));
    for (const ActionMarkerRenderInfo& marker : model.actionMarkers) {
        if (marker.ticks <= 0) continue;
        const TileRenderInfo* tileInfo = tileInfoAt(model, marker.x, marker.y);
        if (!tileInfo || !tileInfo->visible) continue;
        int sx = marker.x - view.viewX;
        int sy = marker.y - view.viewY;
        if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) continue;
        int cx = mr.x + sx * s.tile + s.tile / 2;
        int topY = mr.y + sy * s.tile - triH - gap;
        topY = std::max(mr.y + 4, std::min(topY, mr.y + mr.h - triH - indicatorOutlineStroke() * 2 - 4));
        int markerCx = std::max(mr.x + triW / 2 + 4, std::min(cx, mr.x + mr.w - triW / 2 - 4));
        drawActionMarkerIndicator(marker.glyph, markerCx, topY, triW, triH);
    }
}

static void drawCursorOverlayIsoBack(const WorldIndex& world) {
    if (!cursorOverlay.valid) return;
    std::vector<unsigned char> mask = cursorFootprintMask(world);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!maskBounds(mask, x0, y0, x1, y1)) return;
    int hw = isoHalfW(), hh = isoHalfH();
    drawFootprintCornersIsoBounds(x0, y0, x1, y1, true);
    if (cursorOverlay.recommended == CommandActionKind::Attack) {
        int cx = 0, cy = 0;
        isoTileCenterFloat((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, cx, cy);
        drawAttackCenterIndicator(cx, cy, hw, hh);
    }
}

static void drawCursorOverlayIsoFront(const WorldIndex& world) {
    if (!cursorOverlay.valid) return;
    std::vector<unsigned char> mask = cursorFootprintMask(world);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!maskBounds(mask, x0, y0, x1, y1)) return;
    float age = std::min(1.0f, (SDL_GetTicks() - cursorOverlay.hoverChangedTicks) / 120.0f);
    drawFootprintCornersIsoBounds(x0, y0, x1, y1, false);
    if (age < 1.0f) {
        int cx = 0, cy = 0;
        isoTileCenterFloat((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, cx, cy);
        int hw = isoHalfW(), hh = isoHalfH();
        int expandX = std::max(1, (int)std::lround(hw * (1.0f - age) * 0.18f));
        int expandY = std::max(1, (int)std::lround(hh * (1.0f - age) * 0.18f));
        drawDiamondOutline(cx, cy, hw + expandX, hh + expandY, rgb(255, 235, 95, (int)std::lround((1.0f - age) * 115.0f)));
    }
}

static void drawCursorOverlayTopDown(const WorldIndex& world) {
    if (!cursorOverlay.valid) return;
    std::vector<unsigned char> mask = cursorFootprintMask(world);
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    if (!maskBounds(mask, x0, y0, x1, y1)) return;
    SDL_Rect mr = mapRect();
    SDL_Rect r{mr.x + (x0 - view.viewX) * s.tile,
               mr.y + (y0 - view.viewY) * s.tile,
               (x1 - x0 + 1) * s.tile,
               (y1 - y0 + 1) * s.tile};
    drawHoverCornersRect(r);
}

static void drawTilesetOverlayIsoBack(const Game& game, const WorldIndex& world) {
    if (!tilesetIndicatorsEnabled()) return;
    RealmProfileScope scope("map.overlay_iso_back");
    updateCursorOverlayState(game, world);
    drawRightDragPathIso();
    if (s.leftDown) drawDragSelectionCornersIso(true);
    else drawCursorOverlayIsoBack(world);
    drawSelectionOverlayIso(world, true);
}

static void drawTilesetOverlayIsoFront(const Game& game, const WorldIndex& world, const RenderModel& model) {
    if (!tilesetIndicatorsEnabled()) return;
    RealmProfileScope scope("map.overlay_iso_front");
    updateCursorOverlayState(game, world);
    if (s.leftDown) drawDragSelectionCornersIso(false);
    else drawCursorOverlayIsoFront(world);
    drawSelectionOverlayIso(world, false);
    drawActionMarkersIso(model);
    drawCommandMenuTargetTriangleIso();
}

static void drawTilesetOverlayTopDown(const Game& game, const WorldIndex& world, const RenderModel& model) {
    if (!tilesetIndicatorsEnabled()) return;
    RealmProfileScope scope("map.overlay_top_down");
    updateCursorOverlayState(game, world);
    drawRightDragPathTopDown();
    if (s.leftDown) drawDragSelectionCornersTopDown();
    else drawCursorOverlayTopDown(world);
    drawSelectionOverlayTopDown(world);
    drawActionMarkersTopDown(model);
    drawCommandMenuTargetTriangleTopDown();
}

void drawMobileBuildPreviewTopDown(const WorldIndex& world) {
    EntityType bt = activeBuildPreviewType();
    if (bt == E_NONE) return;
    SDL_Rect mr = mapRect();
    bool ok = renderCanPlace(g, world, bt, view.cursorX, view.cursorY, 0, g.local.selectedId);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    Color fill = ok ? rgb(70,210,120,72) : rgb(230,65,65,78);
    Color edge = ok ? rgb(130,255,170,220) : rgb(255,120,110,230);
    for (int dy = 0; dy < STATS[bt].sizeH; ++dy) {
        for (int dx = 0; dx < STATS[bt].sizeW; ++dx) {
            int mx = view.cursorX + dx, my = view.cursorY + dy;
            if (!inBounds(mx, my)) continue;
            int sx = mx - view.viewX, sy = my - view.viewY;
            if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) continue;
            SDL_Rect r{mr.x + sx * s.tile, mr.y + sy * s.tile, s.tile, s.tile};
            setDraw(fill); SDL_RenderFillRect(s.ren, &r);
            setDraw(edge); SDL_RenderDrawRect(s.ren, &r);
        }
    }
}

void drawMobileBuildPreviewIso(const WorldIndex& world) {
    EntityType bt = activeBuildPreviewType();
    if (bt == E_NONE) return;
    bool ok = renderCanPlace(g, world, bt, view.cursorX, view.cursorY, 0, g.local.selectedId);
    Color fill = ok ? rgb(70,210,120,72) : rgb(230,65,65,78);
    Color edge = ok ? rgb(130,255,170,220) : rgb(255,120,110,230);
    for (int dy = 0; dy < STATS[bt].sizeH; ++dy) {
        for (int dx = 0; dx < STATS[bt].sizeW; ++dx) {
            int mx = view.cursorX + dx, my = view.cursorY + dy;
            if (!inBounds(mx, my)) continue;
            int cx, cy; isoTileCenterFromScreenOffset(mx - view.viewX, my - view.viewY, cx, cy);
            fillDiamond(cx, cy, isoHalfW(), isoHalfH(), fill);
            drawDiamondOutline(cx, cy, isoHalfW(), isoHalfH(), edge);
        }
    }
}

static void drawIsoTileBaseAt(Game& game, const WorldIndex& world, const RenderModel& model,
                              int mx, int my, int cx, int cy) {
    int hw = isoHalfW(), hh = isoHalfH();
    const Tile& tile = game.map[my][mx];
    TileVisual v = makeTileVisual(game, world, model, mx, my);
    fillDiamond(cx, cy, hw, hh, v.bg);
    if (v.explored) applyTerrainTextureIso(cx, cy, hw, hh, tile, mx, my);
    if (!v.explored) {
        drawUnknownGroundTextureIso(cx, cy, hw, hh, mx, my);
        drawDiamondOutline(cx, cy, hw, hh, rgb(20,22,26,160));
    }
}

void drawIsoTileBase(Game& game, const WorldIndex& world, const RenderModel& model, int mx, int my) {
    int sx = mx - view.viewX, sy = my - view.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    drawIsoTileBaseAt(game, world, model, mx, my, cx, cy);
}

static void drawIsoTileForegroundAt(Game& game, const WorldIndex& world, const RenderModel& model,
                                    int mx, int my, int cx, int cy) {
    int hw = isoHalfW(), hh = isoHalfH();
    TileVisual v = makeTileVisual(game, world, model, mx, my);

    bool entityDrawsHere = !v.ent || entityDrawsFromFootprintTile(game, world, model, *v.ent, mx, my);
    bool tileSpriteMovedToOverlay = (entityDrawsHere && handledByTilesetSpritePass(v.ent))
        || (tilesetMovingSpritePassEnabled() && v.projectile);
    if (!v.glyph.empty() && !tileSpriteMovedToOverlay && entityDrawsHere) {
        // Upright sprite/glyph over the flat isometric board.  The diamond is
        // isometric; the emoji/text itself is not skewed.
        int glyphSize = v.emoji ? std::max(16, (int)(s.tile * 0.96f)) : std::max(12, (int)(s.tile * 0.78f));
        if (v.visible && v.ent && imageTilesetEnabled()) {
            glyphSize = std::max(glyphSize, (int)(s.tile * 1.55f));
            glyphSize = entityFootprintSpriteSize(*v.ent, glyphSize);
        }
        int entityCx = cx;
        int entityCy = cy;
        if (v.visible && v.ent) isoEntityFootprintCenter(*v.ent, cx, cy, entityCx, entityCy);
        SDL_Rect gr{entityCx - glyphSize/2, entityCy - glyphSize/2, glyphSize, glyphSize};
        SDL_Rect imageDrawRect = gr;
        bool drewImage = false;
        if (v.visible && v.ent) {
            Color mod = applyVisionToGlyph(rgb(255,255,255), mx, my);
            drewImage = drawEntityImageAtAnchor(game, world, *v.ent,
                                                entityCx, isoEntityFootprintAnchorY(*v.ent, entityCy), glyphSize, glyphSize, mod,
                                                nullptr, nullptr, -1, SDL_Color{0,0,0,0},
                                                nullptr, &imageDrawRect);
        }
        if (!drewImage) {
            drawCentered(v.glyph, gr, v.visible ? v.fg : scale(v.fg, 0.55f), v.emoji, v.tint);
        }
        if (!tilesetMovingSpritePassEnabled()) {
            drawFeatureOccluderIfNeeded(game, world, mx, my, drewImage ? imageDrawRect : gr);
        }
    }
    if (!tilesetMovingSpritePassEnabled() && !v.ent && v.glyph.empty()) {
        SDL_Rect featureRect = featureSpriteRectIso(cx, cy, hw, hh);
        drawFeatureOccluderIfNeeded(game, world, mx, my, featureRect);
    }
    if (v.selected && !tilesetIndicatorsEnabled()) {
        drawDiamondOutline(cx, cy, hw-1, hh-1, rgb(255,255,255,210));
        if (hw > 4 && hh > 3) drawDiamondOutline(cx, cy, hw-4, hh-3, rgb(255,255,255,110));
    }
    if (v.cursor && !tilesetIndicatorsEnabled()) {
        drawDiamondOutline(cx, cy, hw, hh, rgb(40,20,0,240));
        if (hw > 3 && hh > 2) drawDiamondOutline(cx, cy, hw-3, hh-2, rgb(255,245,150,210));
    }
}

void drawIsoTileForeground(Game& game, const WorldIndex& world, const RenderModel& model, int mx, int my) {
    int sx = mx - view.viewX, sy = my - view.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    drawIsoTileForegroundAt(game, world, model, mx, my, cx, cy);
}

static void drawIsoDynamicIndicators(const WorldIndex& world, const RenderModel& model, IsoOffsetBounds b) {
    RealmProfileScope scope("map.dynamic_indicators_iso");
    for (const EntityRenderInfo& info : model.entities) {
        if (!info.visible || info.hp >= info.maxHp) continue;
        Entity* ent = renderFindEntity(g, world, info.id);
        if (!ent || !ent->alive || handledByTilesetSpritePass(ent)) continue;
        int sx = ent->x - view.viewX;
        int sy = ent->y - view.viewY;
        if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) continue;
        int cx = 0, cy = 0;
        isoTileCenterFromScreenOffset(sx, sy, cx, cy);
        int barCx = cx;
        int barCy = cy;
        isoEntityFootprintCenter(*ent, cx, cy, barCx, barCy);
        drawEntityHpBarAt(*ent, barCx, isoEntityFootprintAnchorY(*ent, barCy) - 5, entityFootprintHpBarWidth(*ent));
    }
}

void drawMapIso(const WorldIndex& world) {
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);
    RenderModel model;
    {
        RealmProfileScope scope("map.render_model");
        model = buildRenderModel(g, ui.actionMarkers, 0, view.viewX, view.viewY, view.viewW, view.viewH);
    }

    IsoOffsetBounds b = isoVisibleOffsetBounds();
    bool drewCachedLayer = false;
    const bool allowIsoMapLayerCache = tilesetIndicatorsEnabled();
    unsigned staticLayerHash = hashIsoStaticLayerForCache(model);
    if (allowIsoMapLayerCache && ensureIsoMapLayerCacheTexture()) {
        if (!isoMapLayerCacheMatches(mr, staticLayerHash)) {
            RealmProfileScope rebuildScope("map.iso_layer_cache_rebuild");
            SDL_Texture* previousTarget = SDL_GetRenderTarget(s.ren);
            SDL_Rect previousClip{};
            SDL_bool clipEnabled = SDL_RenderIsClipEnabled(s.ren);
            if (clipEnabled) SDL_RenderGetClipRect(s.ren, &previousClip);

            SDL_SetRenderTarget(s.ren, isoMapLayerCache.texture);
            SDL_RenderSetClipRect(s.ren, nullptr);
            SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
            setDraw(rgb(0, 0, 0, 0));
            SDL_RenderClear(s.ren);
            SDL_RenderSetClipRect(s.ren, &mr);
            std::vector<IsoTileDrawItem> tileDrawItems = buildIsoTileDrawItems(b);
            {
                RealmProfileScope scope("map.iso_base_tiles");
                for (const IsoTileDrawItem& item : tileDrawItems) {
                    drawIsoTileBaseAt(g, world, model, item.mx, item.my, item.cx, item.cy);
                }
            }
            {
                RealmProfileScope scope("map.iso_foreground_tiles");
                for (const IsoTileDrawItem& item : tileDrawItems) {
                    drawIsoTileForegroundAt(g, world, model, item.mx, item.my, item.cx, item.cy);
                }
            }

            SDL_SetRenderTarget(s.ren, previousTarget);
            if (clipEnabled) SDL_RenderSetClipRect(s.ren, &previousClip);
            else SDL_RenderSetClipRect(s.ren, nullptr);
            updateIsoMapLayerCacheKey(mr, staticLayerHash);
        }
        {
            RealmProfileScope scope("map.iso_layer_cache_copy");
            SDL_RenderCopy(s.ren, isoMapLayerCache.texture, &mr, &mr);
        }
        drewCachedLayer = true;
    }

    if (!drewCachedLayer) {
        std::vector<IsoTileDrawItem> tileDrawItems = buildIsoTileDrawItems(b);
        {
            RealmProfileScope scope("map.iso_base_tiles");
            for (const IsoTileDrawItem& item : tileDrawItems) {
                drawIsoTileBaseAt(g, world, model, item.mx, item.my, item.cx, item.cy);
            }
        }
        {
            RealmProfileScope scope("map.iso_foreground_tiles");
            for (const IsoTileDrawItem& item : tileDrawItems) {
                drawIsoTileForegroundAt(g, world, model, item.mx, item.my, item.cx, item.cy);
            }
        }
    }

    drawIsoDynamicIndicators(world, model, b);
    if (s.leftDown && !tilesetIndicatorsEnabled()) {
        int x0 = std::max(0, std::min(s.dragStartX, view.cursorX));
        int x1 = std::min(MAP_W - 1, std::max(s.dragStartX, view.cursorX));
        int y0 = std::max(0, std::min(s.dragStartY, view.cursorY));
        int y1 = std::min(MAP_H - 1, std::max(s.dragStartY, view.cursorY));
        for (int my = y0; my <= y1; ++my) {
            for (int mx = x0; mx <= x1; ++mx) {
                int cx, cy; isoTileCenterFromScreenOffset(mx-view.viewX, my-view.viewY, cx, cy);
                fillDiamond(cx, cy, isoHalfW(), isoHalfH(), rgb(255,255,255,32));
                drawDiamondOutline(cx, cy, isoHalfW(), isoHalfH(), rgb(255,255,255,145));
            }
        }
    }
    drawMobileBuildPreviewIso(world);
    drawTilesetOverlayIsoBack(g, world);
    drawMovingSpritesIso(g, world, model);
    drawTilesetOverlayIsoFront(g, world, model);
    SDL_RenderSetClipRect(s.ren, nullptr);
}

void drawMap(const WorldIndex& world) {
    if (s.isometric) { drawMapIso(world); return; }
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);
    RenderModel model;
    {
        RealmProfileScope scope("map.render_model");
        model = buildRenderModel(g, ui.actionMarkers, 0, view.viewX, view.viewY, view.viewW, view.viewH);
    }

    {
        RealmProfileScope scope("map.top_down_tiles");
        for (int sy=0; sy<view.viewH; ++sy) {
            for (int sx=0; sx<view.viewW; ++sx) {
                int mx = view.viewX + sx, my = view.viewY + sy;
                if (!inBounds(mx, my)) continue;
                SDL_Rect r{mr.x + sx*s.tile, mr.y + sy*s.tile, s.tile, s.tile};
                drawTile(g, world, model, mx, my, r);
            }
        }
    }

    // Drag selection rectangle.
    if (s.leftDown && !tilesetIndicatorsEnabled()) {
        int mx0 = s.dragStartX, my0 = s.dragStartY;
        int mx1 = view.cursorX, my1 = view.cursorY;
        int x0 = std::min(mx0,mx1) - view.viewX;
        int x1 = std::max(mx0,mx1) - view.viewX;
        int y0 = std::min(my0,my1) - view.viewY;
        int y1 = std::max(my0,my1) - view.viewY;
        SDL_Rect sel{mr.x+x0*s.tile, mr.y+y0*s.tile, (x1-x0+1)*s.tile, (y1-y0+1)*s.tile};
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(255,255,255,70)); SDL_RenderFillRect(s.ren, &sel);
        setDraw(rgb(255,255,255,190)); SDL_RenderDrawRect(s.ren, &sel);
    }
    drawMobileBuildPreviewTopDown(world);
    drawMovingSpritesTopDown(g, world, model);
    drawTilesetOverlayTopDown(g, world, model);
    SDL_RenderSetClipRect(s.ren, nullptr);
}
