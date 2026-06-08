#include "render/sdl/sdl_hud.h"
#include "render/sdl/sdl_profiler.h"
#include "realm.h"
#include "core/build_service.h"
#include "core/world_index.h"
#include "render/render_model.h"
#include "view_state.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include <vector>

namespace {

struct HudAction {
    const char* id;
    const char* icon;
    const char* label;
    int key;
    bool enabled;
};

struct MiniMapPixelCache {
    SDL_Texture* texture = nullptr;
    SDL_PixelFormat* format = nullptr;
    int w = 0;
    int h = 0;
    bool contentValid = false;
};

MiniMapPixelCache miniMapPixelCache;

struct MiniMapCellColorCache {
    std::vector<Uint32> colors;
    int tick = -1;
    float dayPhase = -1.0f;
    float seasonPhase = -1.0f;
    int weather = -1;
};

MiniMapCellColorCache miniMapCellColorCache;

struct BuildGridLayout {
    int x0 = 0;
    int y0 = 0;
    int cols = 3;
    int gap = 8;
    int cardW = 76;
    int cardH = 64;
};

bool isTilesetDesktopHudEligible() {
    return displayMode == DM_EMOJI && !isMobileGui() && !s.viewportOnly;
}

int hudColumnWidth() {
    return std::max(292, std::min(372, (int)std::lround(s.winW * 0.31)));
}

Color hudText() { return rgb(239, 241, 235); }
Color hudMuted() { return rgb(170, 176, 178); }
Color hudLine() { return rgb(255, 255, 255, 28); }
Color hudAccent() { return rgb(202, 72, 218); }
Color hudWarn() { return rgb(224, 86, 88); }
Color hudOk() { return rgb(108, 205, 136); }
Color hudPanelFill() { return rgb(13, 15, 19, 188); }

int buildCardCount() {
    int count = 0;
    const BuildRule* rules = buildRules(count);
    int shown = 0;
    for (int i = 0; i < count; ++i) {
        if (rules[i].menuHotkey != '\0') ++shown;
    }
    return shown;
}

BuildGridLayout buildGridLayout(SDL_Rect overlay) {
    BuildGridLayout layout;
    layout.x0 = overlay.x + 18;
    layout.y0 = std::max(232, std::min(274, s.winH / 3));
    layout.cols = 3;
    layout.gap = s.winH <= 740 ? 6 : 8;
    layout.cardW = (overlay.w - 44 - layout.gap * (layout.cols - 1)) / layout.cols;
    int rows = std::max(1, (buildCardCount() + layout.cols - 1) / layout.cols);
    int availableH = std::max(1, s.winH - layout.y0 - 18);
    layout.cardH = std::max(54, std::min(72, (availableH - layout.gap * (rows - 1)) / rows));
    return layout;
}

std::array<SDL_Rect, 3> systemButtonRects(SDL_Rect overlay) {
    const int size = 40;
    const int gap = 16;
    int y = overlay.y + 48;
    int x = overlay.x + overlay.w - size - 22;
    return {{
        SDL_Rect{x, y, size, size},
        SDL_Rect{x - (size + gap), y, size, size},
        SDL_Rect{x - 2 * (size + gap), y, size, size},
    }};
}

SDL_Rect closeButtonRect(SDL_Rect overlay) {
    return SDL_Rect{overlay.x + overlay.w - 68, overlay.y + 42, 46, 46};
}

SDL_Rect actionButtonRectAt(SDL_Rect overlay, int index) {
    int x = overlay.x + 18;
    int y = tilesetHudMiniMapRect().y + tilesetHudMiniMapRect().h + 20;
    const int gap = 10;
    const int size = std::max(38, std::min(48, (overlay.w - 36 - gap * 4) / 5));
    return SDL_Rect{x + index * (size + gap), y, size, size};
}

SDL_Rect selectionCardRect(SDL_Rect overlay) {
    int cardH = std::max(150, std::min(178, s.winH / 4));
    return SDL_Rect{overlay.x + 18, std::max(overlay.y + 404, s.winH - cardH - 42), overlay.w - 36, cardH};
}

std::array<HudAction, 5> hudActionsFor(const Entity* sel) {
    const bool owned = sel && sel->owner == 0;
    const bool canTrain = owned && isBuilding(sel->type) && !sel->underConstruction && isTrainProducer(sel->type);
    return {{
        {canTrain ? "train" : "build", canTrain ? "house" : "build", canTrain ? "Train" : "Build",
         canTrain ? 't' : 'b', canTrain || (owned && sel->type == E_PEASANT)},
        {"move", "flag", "Move", '\n', owned && !isBuilding(sel->type)},
        {"gather", "gather", "Gather", '\n', owned && sel->type == E_PEASANT},
        {"attack", "attack", "Attack", 'a', owned && !isBuilding(sel->type)},
        {"stop", "stop", "Stop", 'x', owned},
    }};
}

bool ensureMiniMapPixelCache(int w, int h) {
    if (w <= 0 || h <= 0) return false;
    if (miniMapPixelCache.texture && miniMapPixelCache.w == w && miniMapPixelCache.h == h) return true;
    if (miniMapPixelCache.texture) SDL_DestroyTexture(miniMapPixelCache.texture);
    SDL_PixelFormat* format = miniMapPixelCache.format;
    miniMapPixelCache = MiniMapPixelCache{};
    miniMapPixelCache.format = format ? format : SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    if (!miniMapPixelCache.format) return false;
    miniMapPixelCache.texture = SDL_CreateTexture(s.ren, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!miniMapPixelCache.texture) return false;
    SDL_SetTextureBlendMode(miniMapPixelCache.texture, SDL_BLENDMODE_BLEND);
    miniMapPixelCache.w = w;
    miniMapPixelCache.h = h;
    return true;
}

const TileRenderInfo* miniMapTileInfoAt(const RenderModel& model, int mx, int my) {
    int sx = mx - model.viewX;
    int sy = my - model.viewY;
    if (sx < 0 || sy < 0 || sx >= model.viewW || sy >= model.viewH) return nullptr;
    size_t index = (size_t)sy * (size_t)model.viewW + (size_t)sx;
    if (index >= model.tiles.size()) return nullptr;
    return &model.tiles[index];
}

const EntityRenderInfo* miniMapEntityAt(const RenderModel& model, int mx, int my) {
    for (auto it = model.entities.rbegin(); it != model.entities.rend(); ++it) {
        const EntityRenderInfo& entity = *it;
        if (!entity.visible || entity.owner == OWNER_NATURE) continue;
        const EntityStats& stats = STATS[entity.type];
        int w = std::max(1, stats.sizeW);
        int h = std::max(1, stats.sizeH);
        if (mx >= entity.x && my >= entity.y && mx < entity.x + w && my < entity.y + h) return &entity;
    }
    return nullptr;
}

Color miniMapCellColor(const RenderModel& model, int mx, int my) {
    const TileRenderInfo* info = miniMapTileInfoAt(model, mx, my);
    if (!info) return rgb(5, 6, 8, 90);
    Tile tile{};
    tile.terrain = info->terrain;
    tile.resources = info->resources;
    tile.biome = info->biome;
    tile.wear = info->wear;
    Color c = info->explored ? terrainBg(tile, mx, my) : rgb(5, 6, 8, 90);
    const EntityRenderInfo* entity = info->visible ? miniMapEntityAt(model, mx, my) : nullptr;
    if (entity) c = ownerBg(entity->owner);
    c.a = info->visible ? 185 : 95;
    return c;
}

bool refreshMiniMapCellColors(const RenderModel& model, bool& changed) {
    changed = false;
    if (!miniMapPixelCache.format) return false;
    if (miniMapCellColorCache.tick == g.tick
        && miniMapCellColorCache.dayPhase == g.dayPhase
        && miniMapCellColorCache.seasonPhase == g.seasonPhase
        && miniMapCellColorCache.weather == g.weather
        && miniMapCellColorCache.colors.size() == (size_t)MAP_W * (size_t)MAP_H) {
        return true;
    }

    RealmProfileScope scope("hud.tileset_minimap_cells");
    changed = true;
    miniMapCellColorCache.colors.resize((size_t)MAP_W * (size_t)MAP_H);
    for (int my = 0; my < MAP_H; ++my) {
        for (int mx = 0; mx < MAP_W; ++mx) {
            Color c = miniMapCellColor(model, mx, my);
            miniMapCellColorCache.colors[(size_t)my * (size_t)MAP_W + (size_t)mx] =
                SDL_MapRGBA(miniMapPixelCache.format, c.r, c.g, c.b, c.a);
        }
    }
    miniMapCellColorCache.tick = g.tick;
    miniMapCellColorCache.dayPhase = g.dayPhase;
    miniMapCellColorCache.seasonPhase = g.seasonPhase;
    miniMapCellColorCache.weather = g.weather;
    return true;
}

void drawHudAsset(const std::string& assetId, SDL_Rect dst, Color tint = rgb(255,255,255)) {
    if (dst.w <= 0 || dst.h <= 0) return;
    TilesetAssetFrame frame = tilesetLoadScreenUiTileScaled(s.ren, assetId, dst.w, dst.h);
    if (frame.texture) {
        SDL_SetTextureColorMod(frame.texture, tint.r, tint.g, tint.b);
        SDL_SetTextureAlphaMod(frame.texture, tint.a);
        SDL_RenderCopy(s.ren, frame.texture, nullptr, &dst);
        SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
        SDL_SetTextureAlphaMod(frame.texture, 255);
        return;
    }
    setDraw(rgb(22, 24, 28, 210));
    SDL_RenderFillRect(s.ren, &dst);
    setDraw(rgb(255, 255, 255, 40));
    SDL_RenderDrawRect(s.ren, &dst);
}

void drawHudIcon(const char* icon, SDL_Rect dst, Color tint = rgb(255,255,255)) {
    drawHudAsset(std::string("hud/icons/") + icon, dst, tint);
}

void drawHudMaterial(const char* name, SDL_Rect dst, Color tint = rgb(255,255,255)) {
    drawHudAsset(std::string("hud/materials/") + name, dst, tint);
}

void drawCard(SDL_Rect r, bool selected = false, bool unavailable = false, bool hovered = false) {
    const char* material = unavailable ? "build_card_unavailable" : (selected ? "build_card_selected" : "build_card");
    drawHudMaterial(material, r);
    setDraw(unavailable ? rgb(8, 9, 11, hovered ? 105 : 132) : rgb(13, 15, 19, selected ? 192 : (hovered ? 136 : 164)));
    SDL_RenderFillRect(s.ren, &r);
    if (hovered) {
        setDraw(unavailable ? rgb(255, 120, 132, 55) : rgb(255, 255, 255, 38));
        SDL_RenderFillRect(s.ren, &r);
    }
    setDraw(selected ? rgb(204, 72, 226, 210) : (hovered ? rgb(255, 255, 255, 142) : rgb(255, 255, 255, 26)));
    SDL_RenderDrawRect(s.ren, &r);
    if (hovered) {
        setDraw(unavailable ? rgb(255, 118, 128, 150) : rgb(255, 255, 255, 150));
        SDL_RenderDrawLine(s.ren, r.x + 2, r.y + 2, r.x + r.w - 3, r.y + 2);
        SDL_RenderDrawLine(s.ren, r.x + 2, r.y + r.h - 3, r.x + r.w - 3, r.y + r.h - 3);
    }
}

void drawIconButton(SDL_Rect r, const char* icon, int key, bool active = false, bool danger = false) {
    bool hovered = key != 0 && rectHovered(r);
    drawHudMaterial(active ? "button_slot_selected" : "button_slot", r);
    setDraw(active ? rgb(37, 25, 43, 220) : (hovered ? rgb(31, 34, 39, 226) : rgb(20, 22, 26, 205)));
    SDL_RenderFillRect(s.ren, &r);
    Color tint = danger ? rgb(235, 120, 120) : (active ? rgb(255, 238, 255) : (hovered ? rgb(255, 255, 255) : rgb(225, 228, 226)));
    int iconPad = std::max(8, r.w / 5);
    SDL_Rect ir{r.x + iconPad, r.y + iconPad, r.w - iconPad * 2, r.h - iconPad * 2};
    drawHudIcon(icon, ir, tint);
    registerKeyHit(r, key);
    if (hovered) {
        setDraw(danger ? rgb(255, 105, 112, 52) : rgb(255, 255, 255, 42));
        SDL_RenderFillRect(s.ren, &r);
        setDraw(active ? rgb(215, 92, 238, 220) : rgb(255, 255, 255, 145));
        SDL_RenderDrawRect(s.ren, &r);
        SDL_Rect inner{r.x + 1, r.y + 1, std::max(1, r.w - 2), std::max(1, r.h - 2)};
        setDraw(danger ? rgb(255, 112, 122, 135) : rgb(255, 255, 255, 108));
        SDL_RenderDrawRect(s.ren, &inner);
    }
}

void drawResourceChip(int x, int y, const char* icon, const std::string& value,
                      const std::string& delta = "", Color deltaColor = hudMuted()) {
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    SDL_Rect iconRect{x, y - 1, 20, 20};
    drawHudIcon(icon, iconRect);
    drawText(x + 29, y, value, hudText(), font);
    if (!delta.empty()) {
        int deltaX = x + 29 + textWidth(value, font) + 10;
        drawText(deltaX, y, delta, deltaColor, font);
    }
}

void drawRightFade() {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    int startX = std::max(0, s.winW - hudColumnWidth() - 220);
    int width = std::max(1, s.winW - startX);
    for (int x = startX; x < s.winW; x += 4) {
        float t = (float)(x - startX) / (float)width;
        t = std::pow(std::clamp(t, 0.0f, 1.0f), 1.7f);
        int alpha = (int)std::lround(238.0f * t);
        setDraw(rgb(0, 0, 0, alpha));
        SDL_Rect strip{x, 0, std::min(4, s.winW - x), s.winH};
        SDL_RenderFillRect(s.ren, &strip);
    }
    int startY = std::max(0, s.winH - 360);
    int height = std::max(1, s.winH - startY);
    for (int y = startY; y < s.winH; y += 4) {
        float t = (float)(y - startY) / (float)height;
        t = std::pow(std::clamp(t, 0.0f, 1.0f), 1.55f);
        int alpha = (int)std::lround(165.0f * t);
        setDraw(rgb(0, 0, 0, alpha));
        SDL_Rect strip{std::max(0, s.winW - hudColumnWidth() - 260), y,
                       std::min(s.winW, hudColumnWidth() + 260), std::min(4, s.winH - y)};
        SDL_RenderFillRect(s.ren, &strip);
    }
}

void drawSystemButtons(SDL_Rect overlay) {
    auto buttons = systemButtonRects(overlay);
    drawIconButton(buttons[0], "fullscreen", SDLK_F11);
    drawIconButton(buttons[1], "settings", '?');
    drawIconButton(buttons[2], "menu", 'q');
}

Entity* selectedEntity(const WorldIndex& world) {
    return renderFindEntity(g, world, g.local.selectedId);
}

EntityType buildPreviewType() {
    if (g.local.buildPending != E_NONE) return g.local.buildPending;
    int count = 0;
    const BuildRule* rules = buildRules(count);
    for (int i = 0; i < count; ++i) {
        if (rules[i].menuHotkey == '\0') continue;
        return rules[i].buildingType;
    }
    return E_NONE;
}

void drawResources(SDL_Rect overlay, EntityType previewType = E_NONE) {
    const Player& p = g.players[0];
    int x = overlay.x + 22;
    int y = overlay.y + 136;
    int secondX = overlay.x + overlay.w - 142;
    drawResourceChip(x, y, "gold", std::to_string(p.gold));
    drawResourceChip(secondX, y, "wood", std::to_string(p.wood));
    y += 34;
    drawResourceChip(x, y, "food", std::to_string(p.food));
    drawResourceChip(secondX, y, "population", std::to_string(p.supply) + "/" + std::to_string(p.supplyMax));

    if (previewType == E_NONE) return;
    y += 34;
    int afterGold = p.gold - STATS[previewType].costGold;
    int afterWood = p.wood - STATS[previewType].costWood;
    Color goldColor = afterGold < 0 ? hudWarn() : hudOk();
    Color woodColor = afterWood < 0 ? hudWarn() : hudOk();
    std::string goldDelta = STATS[previewType].costGold > 0 ? "-" + std::to_string(STATS[previewType].costGold) : "";
    std::string woodDelta = STATS[previewType].costWood > 0 ? "-" + std::to_string(STATS[previewType].costWood) : "";
    drawResourceChip(x, y, "gold", std::to_string(p.gold) + " > " + std::to_string(afterGold),
                     goldDelta, goldColor);
    drawResourceChip(secondX, y, "wood", std::to_string(p.wood) + " > " + std::to_string(afterWood),
                     woodDelta, woodColor);
}

void drawActionButtons(SDL_Rect overlay, const Entity* sel) {
    std::array<HudAction, 5> actions = hudActionsFor(sel);

    for (int i = 0; i < (int)actions.size(); ++i) {
        const HudAction& action = actions[(size_t)i];
        SDL_Rect r = actionButtonRectAt(overlay, i);
        drawIconButton(r, action.icon, action.enabled ? action.key : 0, false, !action.enabled);
        if (!action.enabled) {
            setDraw(rgb(5, 6, 8, 115));
            SDL_RenderFillRect(s.ren, &r);
        }
    }
}

void drawSelectionCard(SDL_Rect overlay, const WorldIndex& world, const Entity* sel) {
    if (!sel) return;
    SDL_Rect card = selectionCardRect(overlay);
    drawHudMaterial("portrait_frame", card);
    setDraw(hudPanelFill());
    SDL_RenderFillRect(s.ren, &card);
    setDraw(hudLine());
    SDL_RenderDrawRect(s.ren, &card);

    int x = card.x + 20;
    int y = card.y + 16;
    SDL_Rect badge{x, y + 1, 22, 22};
    drawHudMaterial("resource_chip", badge);
    drawHudIcon(isBuilding(sel->type) ? "house" : "population", SDL_Rect{badge.x + 4, badge.y + 4, 14, 14}, hudAccent());
    drawTextFit(x + 34, y - 1, STATS[sel->type].name, hudText(), card.w - 150, s.mono);
    drawTextFit(x + 34, y + 21, isBuilding(sel->type) ? "Structure" : "Unit", hudMuted(), card.w - 150, s.monoSmall ? s.monoSmall : s.mono);

    y += 56;
    drawTextFit(x, y, "HP", hudMuted(), 48, s.monoSmall ? s.monoSmall : s.mono);
    y += 22;
    std::ostringstream hp;
    hp << sel->hp << "/" << sel->maxHp;
    drawTextFit(x, y, hp.str(), hudText(), 110, s.mono);
    int barW = std::max(1, card.w - 128);
    SDL_Rect bar{x, y + 26, barW, 7};
    setDraw(rgb(45, 45, 52, 230));
    SDL_RenderFillRect(s.ren, &bar);
    SDL_Rect fill = bar;
    fill.w = std::max(1, (int)std::lround(bar.w * std::clamp(sel->hp / (double)std::max(1, sel->maxHp), 0.0, 1.0)));
    setDraw(hudAccent());
    SDL_RenderFillRect(s.ren, &fill);

    int statusY = card.y + card.h - 44;
    drawTextFit(x, statusY, "STATUS", hudMuted(), card.w - 40, s.monoSmall ? s.monoSmall : s.mono);
    drawTextFit(x, statusY + 20, stateName(sel->state), rgb(224, 126, 238), card.w - 40, s.monoSmall ? s.monoSmall : s.mono);

    SDL_Color teamColor{ownerBg(sel->owner).r, ownerBg(sel->owner).g, ownerBg(sel->owner).b, 255};
    int portraitH = std::max(148, std::min(200, card.h + 28));
    int portraitW = (int)std::lround(portraitH * 0.76);
    int anchorX = card.x + card.w - 54;
    int anchorY = card.y + card.h + 26;
    drawEntityImageAtAnchor(g, world, *sel, anchorX, anchorY, portraitW, portraitH,
                            rgb(255,255,255), "idle", "front", 0, teamColor);
}

void drawTilesetMiniMap(const RenderModel& model, SDL_Rect r) {
    RealmProfileScope scope("hud.tileset_minimap");
    const bool hovered = rectHovered(r);
    setDraw(rgb(9, 10, 13, 172));
    SDL_RenderFillRect(s.ren, &r);
    SDL_Rect mapArea = miniMapContentRect(r);
    bool drewTexture = false;
    bool cellsChanged = false;
    if (!miniMapUsesIsometricProjection()
            && ensureMiniMapPixelCache(mapArea.w, mapArea.h)
            && refreshMiniMapCellColors(model, cellsChanged)) {
        bool ready = miniMapPixelCache.contentValid && !cellsChanged;
        if (!ready) {
            void* pixels = nullptr;
            int pitch = 0;
            if (SDL_LockTexture(miniMapPixelCache.texture, nullptr, &pixels, &pitch) == 0) {
                RealmProfileScope uploadScope("hud.tileset_minimap_upload");
                for (int yy = 0; yy < mapArea.h; ++yy) {
                    Uint32* row = reinterpret_cast<Uint32*>((uint8_t*)pixels + yy * pitch);
                    int my = yy * MAP_H / std::max(1, mapArea.h);
                    for (int xx = 0; xx < mapArea.w; ++xx) {
                        int mx = xx * MAP_W / std::max(1, mapArea.w);
                        row[xx] = miniMapCellColorCache.colors[(size_t)my * (size_t)MAP_W + (size_t)mx];
                    }
                }
                SDL_UnlockTexture(miniMapPixelCache.texture);
                miniMapPixelCache.contentValid = true;
                ready = true;
            }
        }
        if (ready) {
            SDL_RenderCopy(s.ren, miniMapPixelCache.texture, nullptr, &mapArea);
            drewTexture = true;
        }
    }
    if (!drewTexture) {
        SDL_RenderSetClipRect(s.ren, &mapArea);
        if (miniMapUsesIsometricProjection()) {
            for (int my = 0; my < MAP_H; ++my) {
                for (int mx = 0; mx < MAP_W; ++mx) {
                    int px = 0, py = 0;
                    miniMapWorldToScreen(mx, my, mapArea, px, py);
                    Color c = miniMapCellColor(model, mx, my);
                    setDraw(c);
                    SDL_RenderDrawPoint(s.ren, px, py);
                }
            }
        } else {
            for (int yy = 0; yy < mapArea.h; ++yy) {
                int my = yy * MAP_H / std::max(1, mapArea.h);
                for (int xx = 0; xx < mapArea.w; ++xx) {
                    int mx = xx * MAP_W / std::max(1, mapArea.w);
                    Color c = miniMapCellColor(model, mx, my);
                    setDraw(c);
                    SDL_RenderDrawPoint(s.ren, mapArea.x + xx, mapArea.y + yy);
                }
            }
        }
        SDL_RenderSetClipRect(s.ren, nullptr);
    }

    int vx0 = view.viewX, vy0 = view.viewY;
    int vx1 = view.viewX + view.viewW, vy1 = view.viewY + view.viewH;
    if (s.isometric) {
        IsoOffsetBounds b = isoVisibleOffsetBounds();
        vx0 = view.viewX + b.minSx;
        vx1 = view.viewX + b.maxSx + 1;
        vy0 = view.viewY + b.minSy;
        vy1 = view.viewY + b.maxSy + 1;
    }
    if (vx1 > vx0 && vy1 > vy0) {
        auto miniCoord = [](int origin, int value, int size, int mapSize) {
            return origin + (int)std::floor((double)value * (double)size / (double)mapSize);
        };
        SDL_RenderSetClipRect(s.ren, &mapArea);
        if (miniMapUsesIsometricProjection()) {
            int x00 = 0, y00 = 0, x10 = 0, y10 = 0, x11 = 0, y11 = 0, x01 = 0, y01 = 0;
            miniMapWorldToScreen(vx0, vy0, mapArea, x00, y00);
            miniMapWorldToScreen(vx1 - 1, vy0, mapArea, x10, y10);
            miniMapWorldToScreen(vx1 - 1, vy1 - 1, mapArea, x11, y11);
            miniMapWorldToScreen(vx0, vy1 - 1, mapArea, x01, y01);
            setDraw(rgb(255, 255, 255, 158));
            SDL_RenderDrawLine(s.ren, x00, y00, x10, y10);
            SDL_RenderDrawLine(s.ren, x10, y10, x11, y11);
            SDL_RenderDrawLine(s.ren, x11, y11, x01, y01);
            SDL_RenderDrawLine(s.ren, x01, y01, x00, y00);
        } else {
            SDL_Rect viewRect{
                miniCoord(mapArea.x, vx0, mapArea.w, MAP_W),
                miniCoord(mapArea.y, vy0, mapArea.h, MAP_H),
                std::max(2, miniCoord(0, vx1 - vx0, mapArea.w, MAP_W)),
                std::max(2, miniCoord(0, vy1 - vy0, mapArea.h, MAP_H))
            };
            const bool dominates = viewRect.w > mapArea.w * 3 / 4 && viewRect.h > mapArea.h * 3 / 4;
            setDraw(dominates ? rgb(255, 255, 255, 78) : rgb(255, 255, 255, 158));
            SDL_RenderDrawRect(s.ren, &viewRect);
            if (!dominates) {
                SDL_Rect inner{viewRect.x + 1, viewRect.y + 1, std::max(1, viewRect.w - 2), std::max(1, viewRect.h - 2)};
                setDraw(rgb(0, 0, 0, 115));
                SDL_RenderDrawRect(s.ren, &inner);
            }
        }
    }
    SDL_RenderSetClipRect(s.ren, nullptr);
    if (hovered) {
        setDraw(rgb(255, 255, 255, 18));
        SDL_RenderFillRect(s.ren, &r);
    }
    setDraw(hovered ? rgb(255, 255, 255, 118) : rgb(255, 255, 255, 34));
    SDL_RenderDrawRect(s.ren, &r);
}

std::string buildCardName(EntityType type) {
    switch (type) {
        case E_BLACKSMITH: return "Armory";
        case E_LUMBER_CAMP: return "Lumber";
        case E_MINING_CAMP: return "Mine";
        case E_WOODEN_BRIDGE: return "Wood Br.";
        case E_STONE_BRIDGE: return "Stone Br.";
        default: return STATS[type].name ? STATS[type].name : "Unknown";
    }
}

EntityType hoveredBuildCard(SDL_Rect overlay) {
    if (g.mode != M_BUILD_SELECT && g.mode != M_BUILD_PLACE) return E_NONE;
    int count = 0;
    const BuildRule* rules = buildRules(count);
    BuildGridLayout layout = buildGridLayout(overlay);
    int shown = 0;
    for (int i = 0; i < count; ++i) {
        if (rules[i].menuHotkey == '\0') continue;
        int col = shown % layout.cols;
        int row = shown / layout.cols;
        SDL_Rect r{layout.x0 + col * (layout.cardW + layout.gap), layout.y0 + row * (layout.cardH + layout.gap), layout.cardW, layout.cardH};
        if (pointInRect(s.mouseX, s.mouseY, r)) return rules[i].buildingType;
        ++shown;
    }
    return g.local.buildPending;
}

void drawBuildGallery(SDL_Rect overlay, const WorldIndex& world) {
    EntityType preview = hoveredBuildCard(overlay);
    if (preview == E_NONE) preview = buildPreviewType();

    drawIconButton(closeButtonRect(overlay), "close", 27, false, true);
    drawResources(overlay, preview);

    int count = 0;
    const BuildRule* rules = buildRules(count);
    BuildGridLayout layout = buildGridLayout(overlay);
    int shown = 0;
    const Player& p = g.players[0];
    auto compactCost = [](EntityType type) {
        std::ostringstream cost;
        if (STATS[type].costGold > 0) cost << STATS[type].costGold << "g";
        if (STATS[type].costWood > 0) {
            if (cost.tellp() > 0) cost << ' ';
            cost << STATS[type].costWood << "w";
        }
        std::string out = cost.str();
        return out.empty() ? std::string("free") : out;
    };
    for (int i = 0; i < count; ++i) {
        if (rules[i].menuHotkey == '\0') continue;
        EntityType type = rules[i].buildingType;
        int col = shown % layout.cols;
        int row = shown / layout.cols;
        SDL_Rect r{layout.x0 + col * (layout.cardW + layout.gap), layout.y0 + row * (layout.cardH + layout.gap), layout.cardW, layout.cardH};
        bool selected = (type == preview);
        bool affordable = p.gold >= STATS[type].costGold && p.wood >= STATS[type].costWood;
        drawCard(r, selected, !affordable, rectHovered(r));
        registerKeyHit(r, rules[i].menuHotkey);
        SDL_Color teamColor{ownerBg(0).r, ownerBg(0).g, ownerBg(0).b, 255};
        Entity dummy{};
        dummy.type = type;
        dummy.owner = 0;
        dummy.alive = true;
        int iconSize = std::max(26, std::min(34, r.h - 30));
        SDL_Rect iconRect{r.x + 8, r.y + 22, iconSize, iconSize};
        drawEntityImageTile(g, world, dummy, iconRect, affordable ? rgb(255,255,255) : rgb(120,120,125),
                            "idle", "front", 0, teamColor);
        drawTextFit(r.x + 8, r.y + 8, buildCardName(type), affordable ? hudText() : rgb(135, 138, 142),
                    r.w - 16, s.monoSmall ? s.monoSmall : s.mono);
        drawTextFit(r.x + 8, r.y + r.h - 21, compactCost(type), affordable ? hudMuted() : rgb(120, 118, 120),
                    r.w - 16, s.monoSmall ? s.monoSmall : s.mono);
        ++shown;
    }
}

} // namespace

bool tilesetHudEnabled() {
    return isTilesetDesktopHudEligible();
}

SDL_Rect tilesetHudOverlayRect() {
    int w = hudColumnWidth();
    return SDL_Rect{std::max(0, s.winW - w), 0, w, s.winH};
}

SDL_Rect tilesetHudMiniMapRect() {
    SDL_Rect overlay = tilesetHudOverlayRect();
    int w = std::max(164, overlay.w - 44);
    int h = std::max(70, std::min(94, s.winH / 9));
    return SDL_Rect{overlay.x + 22, overlay.y + 216, w, h};
}

bool tilesetHudConsumesPointer(int px, int py) {
    if (!tilesetHudEnabled()) return false;
    return pointInRect(px, py, tilesetHudOverlayRect());
}

bool tilesetHudClickableAt(int px, int py) {
    if (!tilesetHudConsumesPointer(px, py)) return false;
    SDL_Rect overlay = tilesetHudOverlayRect();
    const bool buildMode = g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE;
    if (buildMode) {
        if (pointInRect(px, py, closeButtonRect(overlay))) return true;
        int count = 0;
        const BuildRule* rules = buildRules(count);
        BuildGridLayout layout = buildGridLayout(overlay);
        int shown = 0;
        for (int i = 0; i < count; ++i) {
            if (rules[i].menuHotkey == '\0') continue;
            int col = shown % layout.cols;
            int row = shown / layout.cols;
            SDL_Rect r{layout.x0 + col * (layout.cardW + layout.gap),
                       layout.y0 + row * (layout.cardH + layout.gap),
                       layout.cardW, layout.cardH};
            if (pointInRect(px, py, r)) return true;
            ++shown;
        }
        return false;
    }

    for (SDL_Rect r : systemButtonRects(overlay)) {
        if (pointInRect(px, py, r)) return true;
    }
    if (pointInRect(px, py, tilesetHudMiniMapRect())) return true;

    for (int i = 0; i < 5; ++i) {
        if (pointInRect(px, py, actionButtonRectAt(overlay, i))) return true;
    }
    return false;
}

void drawTilesetHud(const WorldIndex& world) {
    if (!tilesetHudEnabled()) return;
    {
        RealmProfileScope scope("hud.right_fade");
        drawRightFade();
    }
    SDL_Rect overlay = tilesetHudOverlayRect();

    const bool buildMode = g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE;
    if (buildMode) {
        RealmProfileScope scope("hud.build_gallery");
        drawBuildGallery(overlay, world);
        return;
    }

    {
        RealmProfileScope scope("hud.system_buttons");
        drawSystemButtons(overlay);
    }
    {
        RealmProfileScope scope("hud.resources");
        drawResources(overlay);
    }
    SDL_Rect mini = tilesetHudMiniMapRect();
    RenderModel miniMapModel = buildRenderModel(g, 0, 0, 0, MAP_W, MAP_H);
    drawTilesetMiniMap(miniMapModel, mini);

    Entity* sel = selectedEntity(world);
    {
        RealmProfileScope scope("hud.action_buttons");
        drawActionButtons(overlay, sel);
    }
    {
        RealmProfileScope scope("hud.selection_card");
        drawSelectionCard(overlay, world, sel);
    }

    if (!sel && g.local.selectedIds.size() > 1) {
        RealmProfileScope scope("hud.group_card");
        SDL_Rect card{overlay.x + 18, std::max(overlay.y + 420, s.winH - 188), overlay.w - 36, 132};
        drawHudMaterial("portrait_frame", card);
        setDraw(hudPanelFill());
        SDL_RenderFillRect(s.ren, &card);
        std::ostringstream group;
        group << g.local.selectedIds.size() << " units selected";
        drawTextFit(card.x + 20, card.y + 24, group.str(), hudText(), card.w - 40, s.mono);
        drawTextFit(card.x + 20, card.y + 58, "Right-click the map to command the group.", hudMuted(),
                    card.w - 40, s.monoSmall ? s.monoSmall : s.mono);
    }
}

void clearTilesetHudCaches() {
    if (miniMapPixelCache.texture) SDL_DestroyTexture(miniMapPixelCache.texture);
    if (miniMapPixelCache.format) SDL_FreeFormat(miniMapPixelCache.format);
    miniMapPixelCache = MiniMapPixelCache{};
    miniMapCellColorCache = MiniMapCellColorCache{};
}
