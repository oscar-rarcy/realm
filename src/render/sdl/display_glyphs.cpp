#include "render/sdl/sdl_map.h"
#include "realm.h"
#include "core/world_index.h"
#include "render/entity_visual_defs.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

const char* terrainGlyph(const Tile& t, int x, int y) {
    unsigned h = hash2(x, y, 1200u + (unsigned)g.tick/16u);
    switch (t.terrain) {
        // Interactable / meaningful resources use actual emojis.
        case T_GOLD:      return u8"🪨"; // tinted yellow by renderer, background stays biome
        case T_FOREST:    return (h&1u) ? u8"🌳" : u8"🌲";
        case T_PINE:      return u8"🌲";
        case T_PALM:      return u8"🌴";
        case T_DEAD_TREE: return u8"🪵";
        case T_WHEAT:     return u8"🌾";
        case T_BERRY:     return u8"🫐";
        case T_FISH:      return u8"🐟";

        // Decoration: non-emoji marks, designed to let colour/texture carry the tile.
        case T_GRASS: {
            static const char* a[] = {u8"·",u8"∙",u8"˙",u8"˖"}; return a[h&3u];
        }
        case T_TALL_GRASS: {
            static const char* a[] = {u8"⁝",u8"╵",u8"╷",u8"┆"}; return a[(h+(unsigned)g.tick/18u)&3u];
        }
        case T_FLOWERS: {
            static const char* a[] = {u8"✿",u8"✣",u8"✽",u8"·"}; return a[h&3u];
        }
        case T_MEADOW:    return (h&1u) ? u8"∙" : u8"ˑ";
        case T_MOUNTAIN:  return (h&1u) ? u8"▲" : u8"▴";
        case T_HILLS:     return (h&1u) ? u8"⌒" : u8"∩";
        case T_STONE:     return (h&1u) ? u8"▪" : u8"▫";
        case T_WATER:     return (h&1u) ? u8"≈" : u8"∼";
        case T_SHALLOWS:  return (h&1u) ? u8"≈" : u8"⌁";
        case T_MARSH:     return (h&1u) ? u8"≋" : u8"⌇";
        case T_REEDS:     return (h&1u) ? u8"╿" : u8"┆";
        case T_SAND:      return (h&1u) ? u8"·" : u8"˙";
        case T_DUNES:     return (h&1u) ? u8"∿" : u8"⌁";
        case T_SNOW:      return (h&1u) ? u8"·" : u8"˙";
        case T_ICE:       return (h&1u) ? u8"═" : u8"─";
        case T_DIRT:      return (h&1u) ? u8"∙" : u8"·";
        case T_ROAD:      return (h&1u) ? u8"─" : u8"═";
        case T_MUD:       return (h&1u) ? u8"∙" : u8"·";
        case T_RUINS:     return (h&1u) ? u8"⌂" : u8"⌐";
        case T_GRAVEL:    return (h&1u) ? u8"⁘" : u8"∴";
        case T_LAVA:      return (h&1u) ? u8"≈" : u8"✦";
        case T_ASH:       return (h&1u) ? u8"░" : u8"·";
        case T_CASTLE_WALL:  return u8"▓";
        case T_CASTLE_FLOOR: return (h&1u) ? u8"·" : u8"∙";
        case T_CASTLE_GATE:  return u8"▣";
        case TERRAIN_COUNT: break;
    }
    return "?";
}

const char* peasantGlyph(const Entity& e) {
    if (e.state == S_MOVING || e.state == S_RETURNING
        || (e.state != S_IDLE && e.pathIdx < (int)e.path.size())) return u8"🚶";
    if (e.state == S_GATHERING && (e.cargo.type == CR_FISH || e.cargo.type == CR_FOOD)) return u8"🧎";
    if (e.state == S_GATHERING || e.state == S_BUILDING || e.state == S_ATTACKING) return u8"🏌";
    return u8"🧍";
}

const char* tilesetEntityGlyph(const Entity& e, bool& hasTile) {
    const char* glyph = e.type == E_PEASANT ? peasantGlyph(e)
        : e.type == E_GATE && e.gateOpen ? u8"🚪"
        : isBridge(e.type) && e.facingDy != 0 ? u8"║"
        : isBridge(e.type) ? u8"═"
        : entitySdlGlyph(e.type);
    hasTile = glyph != nullptr;
    return glyph;
}

struct EntitySpriteSpec {
    std::string key;
    std::string displayName;
    std::string suggestedAsset;
    std::string description;
    int frameMs = 250;
    int frames = 2;
    bool loop = true;
    bool holdLast = false;
};

[[maybe_unused]] static int animationFrameForEntity(const Entity& e, const EntityActionAnimationSpec* anim,
                                                    int explicitFrame);

static std::string entityFrameAssetPath(EntityType type, const std::string& action,
                                        const std::string& direction, int frameIndex) {
    std::ostringstream ss;
    ss << "frame_";
    if (frameIndex < 10) ss << '0';
    ss << std::max(0, frameIndex) << "_base.png";
    return (std::filesystem::path("assets") / "tiles" / "entities" / entityAssetSlug(type)
        / action / direction / ss.str()).generic_string();
}

static std::filesystem::path groundTilePath(GroundType ground) {
    return std::filesystem::path("assets") / "tiles" / "grounds"
        / (std::string(groundTypeName(ground)) + ".png");
}

static std::filesystem::path featureManifestPath(FeatureType feature) {
    return std::filesystem::path("assets") / "tiles" / "features"
        / featureTypeName(feature) / "manifest.json";
}

static std::filesystem::path decalTilePath(VisualDecalType decal) {
    return std::filesystem::path("assets") / "tiles" / "decals"
        / (std::string(visualDecalName(decal)) + ".png");
}

static bool runtimeAssetExists(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) return true;
#if defined(REALM_WEB)
    if (!path.is_absolute()) return std::filesystem::exists(std::filesystem::path("/") / path);
#endif
    return false;
}

static bool runtimeGroundAssetLooksAccepted(GroundType ground) {
    std::filesystem::path path = groundTilePath(ground);
    if (!runtimeAssetExists(path)) return false;
    std::error_code ec;
    uintmax_t bytes = std::filesystem::file_size(path, ec);
#if defined(REALM_WEB)
    if (ec && !path.is_absolute()) {
        bytes = std::filesystem::file_size(std::filesystem::path("/") / path, ec);
    }
#endif
    return !ec && bytes >= 100000;
}

int displayFrameMs(const EntityActionAnimationSpec& anim) {
    if (anim.frameCount <= 0) return 250;
    for (int i = 0; i < anim.frameCount; ++i) {
        if (anim.frames[i].durationMs > 0) return anim.frames[i].durationMs;
    }
    return anim.transitionAfterMs > 0 ? anim.transitionAfterMs : 250;
}

EntitySpriteSpec entitySpriteSpec(const Game& game, const WorldIndex& world, const Entity& e) {
    EntitySpriteSpec spec;
    std::string name = STATS[e.type].name ? STATS[e.type].name : "Unknown";
    std::string slug = entityAssetSlug(e.type);
    if (const EntityActionAnimationSpec* anim = entityActionAnimationSpecFor(game, world, e)) {
        std::string action = anim->action;
        std::string direction = entityAnimationDirectionBucket(e);
        int frameIndex = animationFrameForEntity(e, anim, -1);
        spec.frameMs = displayFrameMs(*anim);
        spec.frames = anim->frameCount;
        spec.loop = anim->loop;
        spec.holdLast = anim->holdLast;
        spec.description = anim->description;
        spec.key = slug + "/" + action + "/" + direction;
        spec.displayName = name + " " + action + " " + direction;
        spec.suggestedAsset = entityFrameAssetPath(e.type, action, direction, frameIndex);
        return spec;
    }
    spec.key = slug;
    spec.displayName = name;
    spec.suggestedAsset = (std::filesystem::path("assets") / "tiles" / "entities" / slug
        / "manifest.json").generic_string();
    return spec;
}

bool hasEntityImageTile(const Game& game, const WorldIndex& world, const Entity& e) {
    return runtimeAssetExists(std::filesystem::path(entitySpriteSpec(game, world, e).suggestedAsset));
}

std::string terrainAssetKey(Terrain terrain) {
    Tile tile{};
    tile.terrain = terrain;
    tile.biome = (terrain == T_SNOW || terrain == T_PINE) ? B_SNOW : B_TEMPERATE;
    tile.resources = 100;
    VisualTileParts parts = visualPartsForTile(tile);
    if (parts.feature != F_NONE) return std::string("feature.") + featureTypeName(parts.feature);
    return std::string("ground.") + groundTypeName(parts.ground);
}

std::string entityAssetKey(EntityType type) {
    return std::string("entity.") + entityAssetSlug(type);
}

std::string effectAssetKey(const std::string& effectName) {
    return std::string("effect.") + lowerSlug(effectName);
}

bool hasTerrainImageTile(Terrain terrain) {
    Tile tile{};
    tile.terrain = terrain;
    tile.biome = (terrain == T_SNOW || terrain == T_PINE) ? B_SNOW : B_TEMPERATE;
    tile.resources = 100;
    VisualTileParts parts = visualPartsForTile(tile);
    if (!runtimeGroundAssetLooksAccepted(parts.ground)) return false;
    if (parts.feature == F_NONE) return true;
    return runtimeAssetExists(featureManifestPath(parts.feature));
}

void logMissingEntityImageTile(const Game& game, const WorldIndex& world, const Entity& e) {
    if (hasEntityImageTile(game, world, e)) return;
    EntitySpriteSpec spec = entitySpriteSpec(game, world, e);
    logMissingTile("entity", entityAssetKey(e.type) + "." + spec.key, spec.displayName,
                   std::string(1, STATS[e.type].glyph),
                   spec.suggestedAsset);
}

void logMissingTerrainImageTile(Terrain t) {
    if (hasTerrainImageTile(t)) return;
    std::string name = terrainName(t);
    Tile tile{};
    tile.terrain = t;
    tile.biome = (t == T_SNOW || t == T_PINE) ? B_SNOW : B_TEMPERATE;
    tile.resources = 100;
    VisualTileParts parts = visualPartsForTile(tile);
    std::string suggested = parts.feature == F_NONE
        ? "assets/tiles/grounds/" + std::string(groundTypeName(parts.ground)) + ".png"
        : "assets/tiles/features/" + std::string(featureTypeName(parts.feature)) + "/manifest.json";
    logMissingTile("terrain", terrainAssetKey(t), name,
                   std::string(1, terrainAsciiGlyph(t)),
                   suggested);
}

void logMissingVisualTileParts(const Tile& tile) {
    VisualTileParts parts = visualPartsForTile(tile);
    std::string ground = groundTypeName(parts.ground);
    if (!runtimeGroundAssetLooksAccepted(parts.ground)) {
        logMissingTile("ground", std::string("ground.") + ground, ground,
                       std::string(1, terrainAsciiGlyph(tile.terrain)),
                       "assets/tiles/grounds/" + ground + ".png");
    }
    if (parts.feature != F_NONE) {
        std::string feature = featureTypeName(parts.feature);
        std::string fallback = featureConceals(parts.feature) ? "front/back symbolic occluder" : terrainGlyph(tile, 0, 0);
        if (!runtimeAssetExists(featureManifestPath(parts.feature))) {
            logMissingTile("feature", std::string("feature.") + feature, feature, fallback,
                           "assets/tiles/features/" + feature + "/manifest.json");
        }
    }
    for (VisualDecalType decal : parts.decals) {
        std::string name = visualDecalName(decal);
        if (!runtimeAssetExists(decalTilePath(decal))) {
            logMissingTile("decal", std::string("decal.") + name, name, "procedural wear/decal fallback",
                           "assets/tiles/decals/" + name + ".png");
        }
    }
}

const char* featureOccluderGlyph(FeatureType feature) {
    switch (feature) {
        case F_FOREST: return u8"♣";
        case F_PINE: return u8"♠";
        case F_REEDS: return u8"╿";
        default: return "";
    }
}

void drawFeatureOccluderIfNeeded(Game& game, const WorldIndex& world, int mx, int my, SDL_Rect rect) {
    if (!inBounds(mx, my) || !game.map[my][mx].visible[0]) return;
    Entity* ent = renderEntityAt(game, world, mx, my);
    if (!ent || !ent->alive || isBuilding(ent->type)) return;
    VisualTileParts parts = visualPartsForTile(game.map[my][mx]);
    if (!featureConceals(parts.feature)) return;
    const char* glyph = featureOccluderGlyph(parts.feature);
    if (!glyph || !*glyph) return;
    Color col = rgb(105, 180, 95, 185);
    SDL_Rect top{rect.x + rect.w / 8, rect.y, rect.w * 3 / 4, rect.h * 3 / 4};
    drawCentered(glyph, top, col, false, false);
}

std::string tilesetEntityVisual(const Game& game, const WorldIndex& world, const Entity& e, bool& usesSymbolFont) {
    if (!e.alive || e.state == S_DEAD) {
        usesSymbolFont = true;
        if (e.type == E_DEER || e.type == E_SHEEP || e.type == E_BOAR || e.type == E_WOLF) {
            switch (animalCarcassVisualState(e)) {
                case ACVS_DEAD_UNHARVESTED: return u8"◼";
                case ACVS_PARTLY_HARVESTED: return u8"◧";
                case ACVS_MOSTLY_HARVESTED: return u8"◌";
                case ACVS_DEPLETED_SKELETON:
                case ACVS_ALIVE: return u8"☠";
            }
        }
        return e.deathTicks >= DEATH_DECAY_TICKS ? u8"☠" : u8"†";
    }
    logMissingEntityImageTile(game, world, e);
    bool hasTile = false;
    const char* glyph = tilesetEntityGlyph(e, hasTile);
    if (hasTile && glyph) {
        usesSymbolFont = true;
        return glyph;
    }
    usesSymbolFont = false;
    return std::string(1, STATS[e.type].glyph);
}

bool imageTilesetEnabled() {
#if defined(REALM_WEB)
    return false;
#else
    return displayMode == DM_EMOJI || labForcesImageTileset || envFlagEnabled("REALM_IMAGE_TILESET", false);
#endif
}

[[maybe_unused]] static SDL_Color toSdlColor(Color c) {
    return SDL_Color{c.r, c.g, c.b, c.a};
}

int animationFrameFor(const EntityActionAnimationSpec* anim, int explicitFrame = -1, int speedPercent = 100) {
    if (!anim || anim->frameCount <= 0) return 0;
    if (explicitFrame >= 0) return explicitFrame % std::max(1, anim->frameCount);
    int frameMs = std::max(1, displayFrameMs(*anim));
    int speed = std::max(10, speedPercent);
    int elapsedMs = (g.tick * TICK_MS * speed) / 100;
    if (!anim->loop && anim->holdLast) {
        int total = 0;
        for (int i = 0; i < anim->frameCount; ++i) total += std::max(1, anim->frames[i].durationMs);
        if (elapsedMs >= total) return anim->frameCount - 1;
    }
    return (elapsedMs / frameMs) % std::max(1, anim->frameCount);
}

[[maybe_unused]] static int animationFrameForEntity(const Entity& e, const EntityActionAnimationSpec* anim, int explicitFrame = -1) {
    if (explicitFrame >= 0) return animationFrameFor(anim, explicitFrame);
    if (anim && e.state == S_DEAD && std::strcmp(anim->action, "death") == 0 && anim->frameCount > 1) {
        return e.deathTicks >= DEATH_DECAY_TICKS ? 1 : 0;
    }
    return animationFrameFor(anim);
}

static bool drawEntityImageResolved(const Game& game, const WorldIndex& world, const Entity& e,
                                    SDL_Rect dst, Color modulation,
                                    const char* forcedAction,
                                    const char* forcedDirection,
                                    int explicitFrame,
                                    SDL_Color teamColor,
                                    TilesetAssetFrame* outFrame,
                                    double angleDegrees,
                                    int anchorScreenX,
                                    int anchorScreenY,
                                    SDL_Rect* outDrawRect) {
#if defined(REALM_WEB)
    (void)game; (void)world; (void)e; (void)dst; (void)modulation; (void)forcedAction; (void)forcedDirection;
    (void)explicitFrame; (void)teamColor; (void)outFrame; (void)angleDegrees; (void)anchorScreenX;
    (void)anchorScreenY; (void)outDrawRect;
    return false;
#else
    if (!imageTilesetEnabled()) return false;
    const char* action = forcedAction;
    const EntityActionAnimationSpec* anim = nullptr;
    if (action && *action) {
        anim = findEntityActionAnimationSpec(e.type, action);
    } else {
        anim = entityActionAnimationSpecFor(game, world, e);
        action = anim ? anim->action : "idle";
    }
    const char* direction = (forcedDirection && *forcedDirection) ? forcedDirection : entityAnimationDirectionBucket(e);
    int frameIndex = animationFrameForEntity(e, anim, explicitFrame);
    if (!tilesetEntityFrameExists(e.type, action ? action : "idle", direction ? direction : "front", frameIndex)) {
        return false;
    }
    if (teamColor.a == 0) teamColor = toSdlColor(ownerBg(e.owner == OWNER_NATURE ? 0 : e.owner));
    TilesetAssetRequest request{e.type, action ? action : "idle", direction ? direction : "front",
                                frameIndex, teamColor, dst.w, dst.h};
    TilesetAssetFrame frame = tilesetLoadEntityFrame(s.ren, request);
    if (outFrame) *outFrame = frame;
    if (!frame.texture) return false;
    bool mirrorHorizontal = !forcedDirection && entityAnimationMirrorHorizontal(e);
    SDL_Rect drawRect = dst;
    if (frame.hasAnchor && anchorScreenX >= 0 && anchorScreenY >= 0) {
        const TilesetPlacement& placement = frame.placement.valid ? frame.placement : TilesetPlacement{};
        int sourceW = std::max(1, placement.valid ? placement.sourceWidth : frame.anchorSourceWidth);
        int sourceH = std::max(1, placement.valid ? placement.sourceHeight : frame.anchorSourceHeight);
        int frameAnchorX = placement.valid ? placement.anchorX : frame.anchorX;
        int frameAnchorY = placement.valid ? placement.anchorY : frame.anchorY;
        int anchorX = mirrorHorizontal ? sourceW - frameAnchorX : frameAnchorX;
        int scaledAnchorX = (int)std::lround(anchorX * (drawRect.w / (double)sourceW));
        int scaledAnchorY = (int)std::lround(frameAnchorY * (drawRect.h / (double)sourceH));
        drawRect.x = anchorScreenX - scaledAnchorX;
        drawRect.y = anchorScreenY - scaledAnchorY;
    }
    if (outDrawRect) *outDrawRect = drawRect;

    SDL_SetTextureColorMod(frame.texture, modulation.r, modulation.g, modulation.b);
    SDL_SetTextureAlphaMod(frame.texture, modulation.a);
    SDL_RenderCopyEx(s.ren, frame.texture, nullptr, &drawRect, angleDegrees, nullptr,
                     mirrorHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
#endif
}

bool drawEntityImageTile(const Game& game, const WorldIndex& world, const Entity& e, SDL_Rect dst, Color modulation,
                         const char* forcedAction,
                         const char* forcedDirection,
                         int explicitFrame,
                         SDL_Color teamColor,
                         TilesetAssetFrame* outFrame,
                         double angleDegrees,
                         SDL_Rect* outDrawRect) {
    return drawEntityImageResolved(game, world, e, dst, modulation, forcedAction, forcedDirection,
                                   explicitFrame, teamColor, outFrame, angleDegrees, -1, -1, outDrawRect);
}

bool drawEntityImageAtAnchor(const Game& game, const WorldIndex& world, const Entity& e,
                             int anchorScreenX, int anchorScreenY, int targetWidth, int targetHeight,
                             Color modulation,
                             const char* forcedAction,
                             const char* forcedDirection,
                             int explicitFrame,
                             SDL_Color teamColor,
                             TilesetAssetFrame* outFrame,
                             SDL_Rect* outDrawRect,
                             double angleDegrees) {
    SDL_Rect dst{anchorScreenX - targetWidth / 2, anchorScreenY - targetHeight / 2,
                 std::max(1, targetWidth), std::max(1, targetHeight)};
    return drawEntityImageResolved(game, world, e, dst, modulation, forcedAction, forcedDirection,
                                   explicitFrame, teamColor, outFrame, angleDegrees,
                                   anchorScreenX, anchorScreenY, outDrawRect);
}

bool isResourceEmojiTerrain(Terrain t) {
    return t == T_GOLD || t == T_FOREST || t == T_PINE || t == T_PALM || t == T_DEAD_TREE
        || t == T_WHEAT || t == T_BERRY || t == T_FISH;
}

Color glyphColorForTerrain(const Tile& t, int x, int y) {
    (void)x; (void)y;
    switch (t.terrain) {
        case T_GOLD: return rgb(255, 218, 78); // yellow tint applied to rock emoji
        case T_WATER: case T_SHALLOWS: return rgb(175, 225, 238);
        case T_SNOW: case T_ICE: return rgb(245,245,245);
        case T_LAVA: return rgb(255, 190, 76);
        case T_ASH: return rgb(120,120,120);
        default: break;
    }
    Color bg = terrainBg(t, x, y);
    return blend(scale(bg, 1.45f), rgb(235,235,220), 0.25f);
}

bool isSelected(const Entity* e) {
    if (!e) return false;
    if (e->id == g.local.selectedId) return true;
    return std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), e->id) != g.local.selectedIds.end();
}

float visibleFadeAt(int x, int y) {
    if (!inBounds(x, y) || !g.map[y][x].explored[0]) return 0.0f;
    if (!g.map[y][x].visible[0]) return 0.34f;

    float nearest = 99.0f;
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (!inBounds(nx, ny)) continue;
            if (g.map[ny][nx].visible[0]) continue;
            nearest = std::min(nearest, std::sqrt((float)(dx * dx + dy * dy)));
        }
    }

    if (nearest <= 1.01f) return 0.76f;
    if (nearest <= 1.45f) return 0.82f;
    if (nearest <= 2.05f) return 0.91f;
    return 1.0f;
}

float torchStrength(EntityType type) {
    switch (type) {
        case E_TOWNHALL:    return 0.70f;
        case E_CASTLE:      return 0.74f;
        case E_TOWER:       return 0.64f;
        case E_CHURCH:      return 0.62f;
        case E_MARKET:      return 0.56f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 0.52f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:
        case E_DOCK:        return 0.42f;
        default:            return 0.0f;
    }
}

float torchRadius(EntityType type) {
    switch (type) {
        case E_CASTLE:      return 2.35f;
        case E_TOWNHALL:    return 2.25f;
        case E_TOWER:
        case E_CHURCH:      return 2.05f;
        case E_MARKET:      return 1.90f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 1.80f;
        case E_DOCK:        return 1.75f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:        return 1.45f;
        default:            return 0.0f;
    }
}

float torchFalloff(float dist, float radius, float strength) {
    return strength * std::pow(clamp01(1.0f - dist / radius), 2.65f);
}

float torchLightAt(int x, int y) {
    if (!inBounds(x, y) || !g.map[y][x].explored[0]) return 0.0f;
    float nightNeed = clamp01((0.88f - getBrightness(g)) / 0.88f);
    if (nightNeed <= 0.02f) return 0.0f;

    float light = 0.0f;
    for (const auto& e : g.entities) {
        if (!e.alive || e.underConstruction || !isBuilding(e.type)) continue;
        float strength = torchStrength(e.type);
        float radius = torchRadius(e.type);
        if (strength <= 0.0f || radius <= 0.0f) continue;

        const auto& st = STATS[e.type];
        int left = e.x, right = e.x + std::max(1, st.sizeW) - 1;
        int top = e.y, bottom = e.y + std::max(1, st.sizeH) - 1;
        int cx = e.x + st.sizeW / 2, cy = e.y + st.sizeH / 2;
        if (e.owner != 0 && (!inBounds(cx, cy) || !g.map[cy][cx].visible[0])) continue;

        int dx = 0;
        if (x < left) dx = left - x;
        else if (x > right) dx = x - right;
        int dy = 0;
        if (y < top) dy = top - y;
        else if (y > bottom) dy = y - bottom;
        float dist = std::sqrt((float)(dx * dx + dy * dy));
        if (dist > radius) continue;

        float local = torchFalloff(dist, radius, strength);
        if (dx == 0 && dy == 0) local *= 0.40f;
        light = std::max(light, local);
    }
    if (labLightOverride.enabled && labLightOverride.radius > 0.0f && labLightOverride.strength > 0.0f) {
        float dx = (float)(x - labLightOverride.x);
        float dy = (float)(y - labLightOverride.y);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= labLightOverride.radius) {
            float local = torchFalloff(d, labLightOverride.radius, labLightOverride.strength);
            light = std::max(light, local);
        }
    }
    return clamp01(light * nightNeed);
}

Color applyTorchTint(Color c, float torch, bool sprite) {
    if (torch <= 0.0f) return c;
    Color warm = rgb(255, 166, 72);
    float tint = sprite ? 0.42f : 0.38f;
    float lift = sprite ? 0.26f : 0.03f;
    c = blend(c, warm, torch * tint);
    return scale(c, 1.0f + torch * lift);
}

Color applyVisionAndLight(Color c, int x, int y) {
    float vis = visibleFadeAt(x, y);
    if (vis <= 0.0f) return rgb(8, 9, 12);
    if (vis < 1.0f) c = blend(scale(c, 0.34f + 0.66f * vis), rgb(5, 7, 12), (1.0f - vis) * 0.22f);

    float torch = torchLightAt(x, y);
    float nightNeed = clamp01((0.88f - getBrightness(g)) / 0.88f);
    if (nightNeed > 0.02f) {
        float localRelief = clamp01(torch * 3.2f);
        float unlit = 1.0f - localRelief;
        c = scale(c, 1.0f - nightNeed * 0.48f * unlit);
        c = blend(c, rgb(8, 15, 32), nightNeed * 0.08f * unlit);
    }

    return applyTorchTint(c, torch, false);
}

Color applyVisionToGlyph(Color c, int x, int y) {
    c = timeTint(c);
    float vis = visibleFadeAt(x, y);
    if (vis < 1.0f) c = scale(c, 0.50f + 0.50f * vis);
    return applyTorchTint(c, torchLightAt(x, y), true);
}

void hatch(SDL_Rect r, Color c, int step, bool diagonal) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    if (diagonal) {
        for (int x = r.x - r.h; x < r.x + r.w; x += step)
            SDL_RenderDrawLine(s.ren, x, r.y + r.h, x + r.h, r.y);
    } else {
        for (int y = r.y; y < r.y + r.h; y += step)
            SDL_RenderDrawLine(s.ren, r.x, y, r.x + r.w, y);
    }
}

static bool drawTextureFrame(const TilesetAssetFrame& frame, SDL_Rect dst, Color mod) {
    if (!frame.texture || frame.placeholder) return false;
    float cropFraction = 0.0f;
    if (dst.w < 48 && (frame.width != dst.w || frame.height != dst.h)) {
        cropFraction = dst.w <= 24 ? 0.045f : 0.045f * (48.0f - dst.w) / 24.0f;
    }
    SDL_Rect src{0, 0, frame.width, frame.height};
    if (cropFraction > 0.001f && frame.width > 0 && frame.height > 0) {
        int cropX = std::min(frame.width / 4, (int)std::lround(frame.width * cropFraction));
        int cropY = std::min(frame.height / 4, (int)std::lround(frame.height * cropFraction));
        src = SDL_Rect{cropX, cropY, std::max(1, frame.width - cropX * 2), std::max(1, frame.height - cropY * 2)};
    }
    SDL_SetTextureColorMod(frame.texture, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(frame.texture, mod.a);
    SDL_RenderCopy(s.ren, frame.texture, &src, &dst);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
}

static bool drawGroundTexture(SDL_Rect r, GroundType ground, Color mod) {
    if (!imageTilesetEnabled()) return false;
    return drawTextureFrame(tilesetLoadGroundTileScaled(s.ren, ground, r.w, r.h), r, mod);
}

bool drawUnknownGroundTexture(SDL_Rect r, int x, int y) {
    (void)x; (void)y;
    if (!imageTilesetEnabled()) return false;
    return drawTextureFrame(tilesetLoadUnknownGroundTileScaled(s.ren, r.w, r.h), r, rgb(118, 118, 124, 224));
}

void applyTerrainTexture(SDL_Rect r, const Tile& t, int x, int y) {
    VisualTileParts parts = visualPartsForTile(t);
    if (drawGroundTexture(r, parts.ground, applyVisionAndLight(timeTint(rgb(255, 255, 255)), x, y))) {
        return;
    }

    unsigned h = hash2(x,y,1900u);
    switch (t.terrain) {
        case T_TALL_GRASS:
        case T_REEDS:
            hatch(r, rgb(220,255,210,42), std::max(5, s.tile/3), false); break;
        case T_FOREST:
        case T_PINE:
            hatch(r, rgb(8,35,12,45), std::max(6, s.tile/3), true); break;
        case T_WATER:
        case T_SHALLOWS:
            hatch(r, rgb(190,235,255,38), std::max(6, s.tile/3), false); break;
        case T_DUNES:
        case T_SAND:
            hatch(r, rgb(255,230,160,32), std::max(7, s.tile/2), false); break;
        case T_GRAVEL:
        case T_STONE:
        case T_MOUNTAIN:
            if (h & 1u) {
                hatch(r, rgb(255,255,255,26), std::max(5, s.tile/3), true);
            }
            break;
        case T_LAVA:
            hatch(r, rgb(255,160,60,55), std::max(5, s.tile/3), true); break;
        default: break;
    }
}
