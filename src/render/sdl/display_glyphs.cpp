#include "render/sdl/sdl_map.h"
#include "realm.h"
#include "core/world_index.h"

char terrainAscii(Terrain t) {
    switch (t) {
        case T_GRASS: return '.';
        case T_TALL_GRASS: return '"';
        case T_FLOWERS: return '*';
        case T_MEADOW: return ',';
        case T_FOREST: return 'T';
        case T_PINE: return 'Y';
        case T_PALM: return 'y';
        case T_DEAD_TREE: return 't';
        case T_MOUNTAIN: return '^';
        case T_HILLS: return 'n';
        case T_STONE: return 'o';
        case T_WATER: return '~';
        case T_SHALLOWS: return '~';
        case T_MARSH: return '=';
        case T_REEDS: return '|';
        case T_GOLD: return '$';
        case T_SAND: return '.';
        case T_DUNES: return ',';
        case T_SNOW: return '*';
        case T_ICE: return '=';
        case T_DIRT: return '.';
        case T_ROAD: return '#';
        case T_MUD: return ',';
        case T_WHEAT: return '%';
        case T_BERRY: return ':';
        case T_FISH: return '~';
        case T_RUINS: return '&';
        case T_GRAVEL: return ':';
        case T_LAVA: return '~';
        case T_ASH: return '.';
        case T_CASTLE_WALL: return '#';
        case T_CASTLE_FLOOR: return '.';
        case T_CASTLE_GATE: return '|';
        case TERRAIN_COUNT: break;
    }
    return '?';
}

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
    if (e.state == S_MOVING || e.state == S_RETURNING || e.pathIdx < (int)e.path.size()) return u8"🚶";
    if (e.state == S_GATHERING && (e.cargo.type == CR_FISH || e.cargo.type == CR_FOOD)) return u8"🧎";
    if (e.state == S_GATHERING || e.state == S_BUILDING || e.state == S_ATTACKING) return u8"🏌";
    return u8"🧍";
}

const char* tilesetEntityGlyph(const Entity& e, bool& hasTile) {
    hasTile = true;
    switch (e.type) {
        case E_PEASANT: return peasantGlyph(e);
        case E_MILITIA: return u8"🤺";
        case E_ARCHER: return u8"🏹";
        case E_KNIGHT: return u8"🐎";
        case E_SPEARMAN: return u8"🗡";
        case E_CATAPULT: return u8"🛞";
        case E_TREBUCHET: return u8"🎯";
        case E_FISHING_BOAT: return u8"🛶";
        case E_WARSHIP: return u8"🚢";
        case E_TRANSPORT: return u8"⛴";
        case E_RAM: return u8"🪵";
        case E_TOWNHALL: return u8"🏛";
        case E_HOUSE: return u8"🏠";
        case E_BARRACKS: return u8"🏕";
        case E_STABLE: return u8"🐴";
        case E_TOWER: return u8"🗼";
        case E_FARM: return u8"🌾";
        case E_BLACKSMITH: return u8"⚒";
        case E_CHURCH: return u8"⛪";
        case E_MARKET: return u8"🏪";
        case E_WALL: return u8"🧱";
        case E_GATE: return e.gateOpen ? u8"🚪" : u8"🧱";
        case E_CASTLE: return u8"🏰";
        case E_LUMBER_CAMP: return u8"🪵";
        case E_MINING_CAMP: return u8"⛏";
        case E_MILL: return u8"⚙";
        case E_DOCK: return u8"⚓";
        case E_DEER: return u8"🦌";
        case E_WOLF: return u8"🐺";
        case E_SHEEP: return u8"🐑";
        case E_BOAR: return u8"🐗";
        default: break;
    }
    hasTile = false;
    return nullptr;
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

int displayFrameMs(const EntityActionAnimationSpec& anim) {
    if (anim.frameCount <= 0) return 250;
    for (int i = 0; i < anim.frameCount; ++i) {
        if (anim.frames[i].durationMs > 0) return anim.frames[i].durationMs;
    }
    return anim.transitionAfterMs > 0 ? anim.transitionAfterMs : 250;
}

EntitySpriteSpec entitySpriteSpec(const Entity& e) {
    EntitySpriteSpec spec;
    std::string name = STATS[e.type].name ? STATS[e.type].name : "Unknown";
    std::string slug = lowerSlug(name);
    WorldIndex world = buildWorldIndex(g);
    if (const EntityActionAnimationSpec* anim = entityActionAnimationSpecFor(g, world, e)) {
        std::string action = anim->action;
        std::string direction = entityAnimationDirectionBucket(e);
        spec.frameMs = displayFrameMs(*anim);
        spec.frames = anim->frameCount;
        spec.loop = anim->loop;
        spec.holdLast = anim->holdLast;
        spec.description = anim->description;
        spec.key = slug + "/" + action + "/" + direction;
        spec.displayName = name + " " + action + " " + direction;
        spec.suggestedAsset = "assets/tiles/entities/" + spec.key + "/frame_00_base.png";
        return spec;
    }
    spec.key = slug;
    spec.displayName = name;
    spec.suggestedAsset = "assets/tiles/entities/" + slug + ".png";
    return spec;
}

bool hasEntityImageTile(EntityType type) {
#if !defined(REALM_WEB)
    return tilesetEntityFrameExists(type, "idle", "front", 0);
#else
    (void)type;
    return false;
#endif
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
    return std::string("entity.") + lowerSlug(STATS[type].name ? STATS[type].name : "unknown");
}

std::string effectAssetKey(const std::string& effectName) {
    return std::string("effect.") + lowerSlug(effectName);
}

bool hasTerrainImageTile(Terrain terrain) {
#if !defined(REALM_WEB)
    Tile tile{};
    tile.terrain = terrain;
    tile.biome = (terrain == T_SNOW || terrain == T_PINE) ? B_SNOW : B_TEMPERATE;
    tile.resources = 100;
    VisualTileParts parts = visualPartsForTile(tile);
    namespace fs = std::filesystem;
    fs::path ground = fs::path("assets") / "tiles" / "grounds" / (std::string(groundTypeName(parts.ground)) + ".png");
    if (!fs::exists(ground)) return false;
    if (parts.feature == F_NONE) return true;
    fs::path feature = fs::path("assets") / "tiles" / "features" / featureTypeName(parts.feature) / "manifest.json";
    return fs::exists(feature);
#else
    (void)terrain;
    return false;
#endif
}

void logMissingEntityImageTile(const Entity& e) {
    if (hasEntityImageTile(e.type)) return;
    EntitySpriteSpec spec = entitySpriteSpec(e);
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
                   std::string(1, terrainAscii(t)),
                   suggested);
}

void logMissingVisualTileParts(const Tile& tile) {
    VisualTileParts parts = visualPartsForTile(tile);
    std::string ground = groundTypeName(parts.ground);
    logMissingTile("ground", std::string("ground.") + ground, ground,
                   std::string(1, terrainAscii(tile.terrain)),
                   "assets/tiles/grounds/" + ground + ".png");
    if (parts.feature != F_NONE) {
        std::string feature = featureTypeName(parts.feature);
        std::string fallback = featureConceals(parts.feature) ? "front/back symbolic occluder" : terrainGlyph(tile, 0, 0);
        logMissingTile("feature", std::string("feature.") + feature, feature, fallback,
                       "assets/tiles/features/" + feature + "/manifest.json");
    }
    for (VisualDecalType decal : parts.decals) {
        std::string name = visualDecalName(decal);
        logMissingTile("decal", std::string("decal.") + name, name, "procedural wear/decal fallback",
                       "assets/tiles/decals/" + name + ".png");
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

void drawFeatureOccluderIfNeeded(int mx, int my, SDL_Rect rect) {
    if (!inBounds(mx, my) || !g.map[my][mx].visible[0]) return;
    Entity* ent = sdlEntityAt(mx, my);
    if (!ent || !ent->alive || isBuilding(ent->type)) return;
    VisualTileParts parts = visualPartsForTile(g.map[my][mx]);
    if (!featureConceals(parts.feature)) return;
    const char* glyph = featureOccluderGlyph(parts.feature);
    if (!glyph || !*glyph) return;
    Color col = rgb(105, 180, 95, 185);
    SDL_Rect top{rect.x + rect.w / 8, rect.y, rect.w * 3 / 4, rect.h * 3 / 4};
    drawCentered(glyph, top, col, false, false);
}

std::string tilesetEntityVisual(const Entity& e, bool& usesSymbolFont) {
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
    logMissingEntityImageTile(e);
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
    return labForcesImageTileset || envFlagEnabled("REALM_IMAGE_TILESET", false);
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

bool drawEntityImageTile(const Entity& e, SDL_Rect dst, Color modulation,
                                const char* forcedAction,
                                const char* forcedDirection,
                                int explicitFrame,
                                SDL_Color teamColor,
                                TilesetAssetFrame* outFrame) {
#if defined(REALM_WEB)
    (void)e; (void)dst; (void)modulation; (void)forcedAction; (void)forcedDirection;
    (void)explicitFrame; (void)teamColor; (void)outFrame;
    return false;
#else
    if (!imageTilesetEnabled()) return false;
    const char* action = forcedAction;
    const EntityActionAnimationSpec* anim = nullptr;
    if (action && *action) {
        anim = findEntityActionAnimationSpec(e.type, action);
    } else {
        WorldIndex world = buildWorldIndex(g);
        anim = entityActionAnimationSpecFor(g, world, e);
        action = anim ? anim->action : "idle";
    }
    const char* direction = (forcedDirection && *forcedDirection) ? forcedDirection : entityAnimationDirectionBucket(e);
    int frameIndex = animationFrameForEntity(e, anim, explicitFrame);
    if (!tilesetEntityFrameExists(e.type, action ? action : "idle", direction ? direction : "front", frameIndex)) {
        return false;
    }
    if (teamColor.a == 0) teamColor = toSdlColor(ownerBg(e.owner == OWNER_NATURE ? 0 : e.owner));
    TilesetAssetRequest request{e.type, action ? action : "idle", direction ? direction : "front", frameIndex, teamColor};
    TilesetAssetFrame frame = tilesetLoadEntityFrame(s.ren, request);
    if (outFrame) *outFrame = frame;
    if (!frame.texture) return false;
    bool mirrorHorizontal = !forcedDirection && entityAnimationMirrorHorizontal(e);

    SDL_SetTextureColorMod(frame.texture, modulation.r, modulation.g, modulation.b);
    SDL_SetTextureAlphaMod(frame.texture, modulation.a);
    SDL_RenderCopyEx(s.ren, frame.texture, nullptr, &dst, 0.0, nullptr,
                     mirrorHorizontal ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
#endif
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
    if (e->id == g.selectedId) return true;
    return std::find(g.selectedIds.begin(), g.selectedIds.end(), e->id) != g.selectedIds.end();
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
        case E_TOWNHALL:    return 0.44f;
        case E_CASTLE:      return 0.48f;
        case E_TOWER:       return 0.42f;
        case E_CHURCH:      return 0.38f;
        case E_MARKET:      return 0.34f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 0.30f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:
        case E_DOCK:        return 0.23f;
        default:            return 0.0f;
    }
}

float torchRadius(EntityType type) {
    switch (type) {
        case E_CASTLE:      return 5.6f;
        case E_TOWNHALL:    return 5.1f;
        case E_TOWER:
        case E_CHURCH:      return 4.6f;
        case E_MARKET:      return 4.0f;
        case E_BARRACKS:
        case E_STABLE:
        case E_BLACKSMITH:  return 3.7f;
        case E_DOCK:        return 3.5f;
        case E_HOUSE:
        case E_LUMBER_CAMP:
        case E_MINING_CAMP:
        case E_MILL:        return 2.9f;
        default:            return 0.0f;
    }
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

        float local = strength * std::pow(clamp01(1.0f - dist / radius), 1.85f);
        if (dx == 0 && dy == 0) local *= 0.40f;
        light = std::max(light, local);
    }
    if (labLightOverride.enabled && labLightOverride.radius > 0.0f && labLightOverride.strength > 0.0f) {
        float dx = (float)(x - labLightOverride.x);
        float dy = (float)(y - labLightOverride.y);
        float d = std::sqrt(dx * dx + dy * dy);
        if (d <= labLightOverride.radius) {
            float local = labLightOverride.strength
                * std::pow(clamp01(1.0f - d / labLightOverride.radius), 1.85f);
            light = std::max(light, local);
        }
    }
    return clamp01(light * nightNeed);
}

Color applyVisionAndLight(Color c, int x, int y) {
    float vis = visibleFadeAt(x, y);
    if (vis <= 0.0f) return rgb(8, 9, 12);
    if (vis < 1.0f) c = blend(scale(c, 0.34f + 0.66f * vis), rgb(5, 7, 12), (1.0f - vis) * 0.22f);

    float torch = torchLightAt(x, y);
    if (torch > 0.0f) {
        c = blend(c, rgb(238, 122, 52), torch * 0.14f);
        c = scale(c, 1.0f + torch * 0.12f);
    }
    return c;
}

Color applyVisionToGlyph(Color c, int x, int y) {
    c = timeTint(c);
    float vis = visibleFadeAt(x, y);
    if (vis < 1.0f) c = scale(c, 0.50f + 0.50f * vis);
    float torch = torchLightAt(x, y);
    if (torch > 0.0f) c = blend(c, rgb(238, 150, 82), torch * 0.10f);
    return c;
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

void applyTerrainTexture(SDL_Rect r, const Tile& t, int x, int y) {
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
