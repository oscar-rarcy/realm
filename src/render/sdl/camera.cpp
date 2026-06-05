#include "render/sdl/sdl_terminal.h"
#include "realm.h"
#include "view_state.h"

#include <array>
#include <cmath>

namespace {

constexpr std::array<int, 16> kTilesetZoomTiles = {
    14, 17, 21, 26, 31, 38, 47, 57,
    70, 86, 105, 129, 157, 192, 235, 288,
};

constexpr std::array<int, 16> kAsciiZoomTiles = {
    14, 15, 16, 17, 18, 19, 21, 22,
    24, 26, 28, 30, 33, 35, 38, 44,
};

const std::array<int, 16>& activeZoomTiles() {
    return displayMode == DM_EMOJI ? kTilesetZoomTiles : kAsciiZoomTiles;
}

int nearestZoomIndex(const std::array<int, 16>& tiles, int tilePx) {
    int best = 0;
    int bestDistance = std::abs(tilePx - tiles[0]);
    for (int i = 1; i < (int)tiles.size(); ++i) {
        int distance = std::abs(tilePx - tiles[(size_t)i]);
        if (distance < bestDistance) {
            best = i;
            bestDistance = distance;
        }
    }
    return best;
}

int nearestZoomTile(const std::array<int, 16>& tiles, int tilePx) {
    return tiles[(size_t)nearestZoomIndex(tiles, tilePx)];
}

}

bool mobileForcedByEnv(bool& value) {
    const char* env = std::getenv("REALM_MOBILE_GUI");
    if (!env || !*env) return false;
    std::string v(env);
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char ch) { return (char)std::tolower(ch); });
    value = !(v == "0" || v == "false" || v == "off" || v == "no");
    return true;
}

bool isMobileGui() {
    bool forced = false;
    if (mobileForcedByEnv(forced)) return forced;
    int shortSide = std::min(s.winW, s.winH);
    return s.winW < 760 || s.winH < 560 || (s.winH > s.winW && s.winW <= 900) || shortSide <= 520;
}

bool mobilePortrait() {
    if (!isMobileGui()) return false;
    if (s.mobileOrientation == 1) return true;
    if (s.mobileOrientation == 2) return false;
    return s.winH >= s.winW;
}

bool isAsciiMobileGui() {
    return displayMode == DM_ASCII && isMobileGui();
}

void asciiMobileCellMetrics(int& cellW, int& cellH) {
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    int w = 0, h = 0;
    if (font) TTF_SizeText(font, "M", &w, &h);
    cellW = std::max(8, w);
    cellH = std::max(15, font ? TTF_FontLineSkip(font) : h);
    if (s.asciiSquareMapCells) {
        int side = std::max(cellW, cellH);
        cellW = side;
        cellH = side;
    }
}

int mobileSafePad() {
    return std::max(8, (int)std::lround(10.0f * s.mobileUiScale));
}

int mobileHudExtent() {
    if (mobilePortrait()) {
        int preferred = (int)std::lround(s.winH * 0.42f);
        int maxHud = std::max(250, s.winH - 220);
        return std::max(250, std::min(preferred, maxHud));
    }
    int preferred = (int)std::lround(s.winW * 0.34f);
    int maxHud = std::max(280, s.winW - 320);
    return std::max(280, std::min(preferred, maxHud));
}

SDL_Rect mapRect() {
    if (s.viewportOnly) {
        return SDL_Rect{0, 0, std::max(1, s.winW), std::max(1, s.winH)};
    }
    if (isMobileGui()) {
        int hud = mobileHudExtent();
        if (mobilePortrait()) {
            return SDL_Rect{0, 0, s.winW, std::max(1, s.winH - hud)};
        }
        return SDL_Rect{0, 0, std::max(1, s.winW - hud), s.winH};
    }
    return SDL_Rect{0, s.topH, std::max(1, s.winW - s.panelW), std::max(1, s.winH - s.topH - s.bottomH)};
}

int mapSafeMargin() {
    return std::max(24, std::min(56, s.tile + 10));
}

SDL_Rect insetRect(SDL_Rect r, int inset) {
    inset = std::max(0, std::min(inset, std::min(r.w, r.h) / 3));
    return SDL_Rect{r.x + inset, r.y + inset,
                    std::max(1, r.w - inset * 2),
                    std::max(1, r.h - inset * 2)};
}

SDL_Rect mapSafeRect() {
    return insetRect(mapRect(), mapSafeMargin());
}

SDL_Rect panelRect() {
    if (isMobileGui()) {
        int hud = mobileHudExtent();
        if (mobilePortrait()) return SDL_Rect{0, std::max(1, s.winH - hud), s.winW, hud};
        return SDL_Rect{std::max(1, s.winW - hud), 0, hud, s.winH};
    }
    return SDL_Rect{s.winW - s.panelW, 0, s.panelW, s.winH};
}

SDL_Rect miniMapRect() {
    if (s.viewportOnly) return SDL_Rect{0, 0, 0, 0};
    SDL_Rect pr = panelRect();
    if (isMobileGui()) {
        int pad = mobileSafePad();
        if (mobilePortrait()) {
            int w = std::max(118, std::min(pr.w / 3, 180));
            int h = std::max(74, std::min(110, pr.h / 3));
            return SDL_Rect{pr.x + pr.w - pad - w, pr.y + pad + 36, w, h};
        }
        int w = std::max(1, pr.w - pad * 2);
        int h = std::max(88, std::min(132, pr.h / 4));
        return SDL_Rect{pr.x + pad, pr.y + pad + 42, w, h};
    }
    return SDL_Rect{pr.x + 14, 12, std::max(1, pr.w - 28), 110};
}

int isoHalfW() { return std::max(8, s.tile); }
int isoHalfH() { return std::max(5, s.tile / 2); }

float isoCameraViewX() {
    return (s.isometric && displayMode == DM_EMOJI && s.isoCameraActive) ? s.isoViewX : (float)view.viewX;
}

float isoCameraViewY() {
    return (s.isometric && displayMode == DM_EMOJI && s.isoCameraActive) ? s.isoViewY : (float)view.viewY;
}

void syncIsoCameraToView() {
    s.isoCameraActive = true;
    s.isoViewX = (float)view.viewX;
    s.isoViewY = (float)view.viewY;
}

static void setIsoCamera(float x, float y) {
    s.isoCameraActive = true;
    s.isoViewX = x;
    s.isoViewY = y;
    view.viewX = (int)std::floor(s.isoViewX);
    view.viewY = (int)std::floor(s.isoViewY);
}

void isoOrigin(int& ox, int& oy) {
    SDL_Rect mr = mapRect();
    int hw = isoHalfW();
    int hh = isoHalfH();
    int bboxW = std::max(1, (view.viewW + view.viewH) * hw);
    int bboxH = std::max(1, (view.viewW + view.viewH) * hh + hh);
    // Centre the diamond block inside the map pane.  The x-origin is the
    // screen centre of map tile (viewX, viewY).
    ox = mr.x + mr.w / 2 - ((view.viewW - view.viewH) * hw) / 2;
    oy = mr.y + std::max(0, (mr.h - bboxH) / 2);
    (void)bboxW;
}





IsoOffsetBounds isoOffsetBoundsForRect(SDL_Rect mr, int expand) {
    float minSx = 1e9f, minSy = 1e9f;
    float maxSx = -1e9f, maxSy = -1e9f;
    const int px[4] = {mr.x, mr.x + mr.w - 1, mr.x, mr.x + mr.w - 1};
    const int py[4] = {mr.y, mr.y, mr.y + mr.h - 1, mr.y + mr.h - 1};
    for (int i = 0; i < 4; ++i) {
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(px[i], py[i], sx, sy);
        minSx = std::min(minSx, sx); maxSx = std::max(maxSx, sx);
        minSy = std::min(minSy, sy); maxSy = std::max(maxSy, sy);
    }

    return IsoOffsetBounds{
        (int)std::floor(minSx) - expand,
        (int)std::ceil(maxSx) + expand,
        (int)std::floor(minSy) - expand,
        (int)std::ceil(maxSy) + expand
    };
}

IsoOffsetBounds isoVisibleOffsetBounds() {
    // Expand by a small border so partially visible diamonds at pane edges draw.
    return isoOffsetBoundsForRect(mapRect(), 3);
}

IsoOffsetBounds isoSafeOffsetBounds() {
    return isoOffsetBoundsForRect(mapSafeRect(), 0);
}

int topDownFullColumnsForRect(SDL_Rect mr) {
    return std::max(1, std::min(MAP_W, mr.w / std::max(1, s.tile)));
}

int topDownFullRowsForRect(SDL_Rect mr) {
    return std::max(1, std::min(MAP_H, mr.h / std::max(1, s.tile)));
}

int topDownSafeColumns() { return topDownFullColumnsForRect(mapSafeRect()); }
int topDownSafeRows() { return topDownFullRowsForRect(mapSafeRect()); }

void cameraBounds(int& minX, int& maxX, int& minY, int& maxY) {
    if (s.isometric) {
        IsoOffsetBounds b = isoSafeOffsetBounds();
        minX = -b.maxSx;
        maxX = MAP_W - 1 - b.minSx;
        minY = -b.maxSy;
        maxY = MAP_H - 1 - b.minSy;
        return;
    }

    int safeW = topDownSafeColumns();
    int safeH = topDownSafeRows();
    int insetTiles = std::max(1, mapSafeMargin() / std::max(1, s.tile));
    minX = -insetTiles;
    minY = -insetTiles;
    maxX = MAP_W - safeW + insetTiles;
    maxY = MAP_H - safeH + insetTiles;
}



void fillDiamond(int cx, int cy, int hw, int hh, Color c) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
#if SDL_VERSION_ATLEAST(2, 0, 18)
    SDL_Color color{c.r, c.g, c.b, c.a};
    SDL_Vertex vertices[4] = {
        {SDL_FPoint{(float)cx, (float)(cy - hh)}, color, SDL_FPoint{0.0f, 0.0f}},
        {SDL_FPoint{(float)(cx + hw), (float)cy}, color, SDL_FPoint{0.0f, 0.0f}},
        {SDL_FPoint{(float)cx, (float)(cy + hh)}, color, SDL_FPoint{0.0f, 0.0f}},
        {SDL_FPoint{(float)(cx - hw), (float)cy}, color, SDL_FPoint{0.0f, 0.0f}},
    };
    int indices[6] = {0, 1, 3, 1, 2, 3};
    if (SDL_RenderGeometry(s.ren, nullptr, vertices, 4, indices, 6) == 0) return;
#endif
    setDraw(c);
    for (int dy = -hh; dy <= hh; ++dy) {
        float t = 1.0f - std::abs(dy) / (float)std::max(1, hh);
        int span = std::max(0, (int)std::round(hw * t));
        SDL_RenderDrawLine(s.ren, cx - span, cy + dy, cx + span, cy + dy);
    }
}

void drawDiamondOutline(int cx, int cy, int hw, int hh, Color c) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    SDL_RenderDrawLine(s.ren, cx, cy - hh, cx + hw, cy);
    SDL_RenderDrawLine(s.ren, cx + hw, cy, cx, cy + hh);
    SDL_RenderDrawLine(s.ren, cx, cy + hh, cx - hw, cy);
    SDL_RenderDrawLine(s.ren, cx - hw, cy, cx, cy - hh);
}

void hatchDiamond(int cx, int cy, int hw, int hh, Color c, int step) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    step = std::max(3, step);
    for (int dy = -hh + step; dy < hh; dy += step) {
        float t = 1.0f - std::abs(dy) / (float)std::max(1, hh);
        int span = std::max(0, (int)std::round(hw * t));
        SDL_RenderDrawLine(s.ren, cx - span, cy + dy, cx + span, cy + dy);
    }
}

void sparkleDiamond(int cx, int cy, int hw, int hh, Color c, int x, int y) {
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(c);
    unsigned h = hash2(x, y, 7811u);
    int count = 1 + (h & 3u);
    for (int i = 0; i < count; ++i) {
        int lx = (int)((hash2(x, y, 7900u + i) % (unsigned)(hw * 2 + 1)) - hw);
        int maxY = std::max(1, (int)(hh * (1.0f - std::abs(lx)/(float)std::max(1, hw))));
        int ly = (int)((hash2(x, y, 8000u + i) % (unsigned)(maxY * 2 + 1)) - maxY);
        SDL_RenderDrawPoint(s.ren, cx + lx, cy + ly);
        if (s.tile >= 28) SDL_RenderDrawPoint(s.ren, cx + lx + 1, cy + ly);
    }
}

static bool drawTextureFrameIso(const TilesetAssetFrame& frame, int cx, int cy, int hw, int hh, Color mod) {
    if (!frame.texture || frame.placeholder) return false;
    SDL_Rect dst{cx - hw, cy - hh, hw * 2 + 1, hh * 2 + 1};
    SDL_SetTextureColorMod(frame.texture, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(frame.texture, mod.a);
    SDL_RenderCopy(s.ren, frame.texture, nullptr, &dst);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
}

static Color groundShaderOverlayColorIso(const GroundShaderResult& shader, int x, int y) {
    Color overlay = timeTint(groundShaderColor(shader.overlayTint));
    overlay.a = (Uint8)std::max(0, std::min(255, (int)std::lround(shader.overlayTint.a * visibleFadeAt(x, y))));
    return overlay;
}

static void drawGroundShaderOverlayIso(int cx, int cy, int hw, int hh,
                                       const GroundShaderResult& shader, int x, int y) {
    if (!shader.drawOverlay || shader.overlayTint.a == 0) return;
    Color overlay = groundShaderOverlayColorIso(shader, x, y);
    if (overlay.a == 0) return;
    fillDiamond(cx, cy, hw, hh, overlay);
}

static SDL_Rect featureSpriteRectIso(int cx, int cy, int hw, int hh) {
    int size = std::max(16, (int)std::lround(hw * 1.12f));
    return SDL_Rect{cx - size / 2, cy + hh - size, size, size};
}

bool drawUnknownGroundTextureIso(int cx, int cy, int hw, int hh, int x, int y) {
    (void)x; (void)y;
    if (!imageTilesetEnabled()) return false;
    return drawTextureFrameIso(
        tilesetLoadUnknownGroundTileIso(s.ren, hw * 2 + 1, hh * 2 + 1),
        cx, cy, hw, hh, rgb(118, 118, 124, 224));
}

static bool drawFeatureTextureIso(int cx, int cy, int hw, int hh, FeatureType feature,
                                  FeatureState state, const char* layer, Color mod) {
    if (!imageTilesetEnabled() || feature == F_NONE || !layer || !*layer) return false;
    SDL_Rect dst = featureSpriteRectIso(cx, cy, hw, hh);
    TilesetAssetFrame frame = tilesetLoadFeatureTileScaled(s.ren, feature, state, layer, dst.w, dst.h);
    if (!frame.texture || frame.placeholder) return false;
    SDL_SetTextureColorMod(frame.texture, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(frame.texture, mod.a);
    SDL_RenderCopy(s.ren, frame.texture, nullptr, &dst);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
}

static bool drawDecalTextureIso(int cx, int cy, int hw, int hh, VisualDecalType decal, Color mod) {
    if (!imageTilesetEnabled()) return false;
    SDL_Rect dst{cx - hw, cy - hh, hw * 2 + 1, hh * 2 + 1};
    TilesetAssetFrame frame = tilesetLoadDecalTileScaled(s.ren, decal, dst.w, dst.h);
    if (!frame.texture || frame.placeholder) return false;
    SDL_SetTextureColorMod(frame.texture, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(frame.texture, mod.a);
    SDL_RenderCopy(s.ren, frame.texture, nullptr, &dst);
    SDL_SetTextureColorMod(frame.texture, 255, 255, 255);
    SDL_SetTextureAlphaMod(frame.texture, 255);
    return true;
}

static void drawVisualTilePartImagesIso(int cx, int cy, int hw, int hh,
                                        const VisualTileParts& parts, Color mod) {
    for (VisualDecalType decal : parts.decals) {
        drawDecalTextureIso(cx, cy, hw, hh, decal, mod);
    }
    if (parts.feature != F_NONE) {
        if (drawFeatureTextureIso(cx, cy, hw, hh, parts.feature, parts.featureState, "base", mod)) return;
        if (drawFeatureTextureIso(cx, cy, hw, hh, parts.feature, parts.featureState, "back", mod)) return;
        if (parts.featureState != FS_FULL && drawFeatureTextureIso(cx, cy, hw, hh, parts.feature, FS_FULL, "base", mod)) return;
        if (parts.featureState != FS_FULL) drawFeatureTextureIso(cx, cy, hw, hh, parts.feature, FS_FULL, "back", mod);
    }
}

void applyTerrainTextureIso(int cx, int cy, int hw, int hh, const Tile& t, int x, int y) {
    VisualTileParts parts = visualPartsForTile(t);
    GroundShaderResult shader = shadeGroundTileForCurrentGame(t, parts, x, y);
    Color mod = applyVisionAndLight(timeTint(groundShaderColor(shader.textureMod)), x, y);
    bool drewGround = false;
    if (imageTilesetEnabled()) {
        drewGround = drawTextureFrameIso(
                tilesetLoadGroundTileIso(s.ren, parts.ground, hw * 2 + 1, hh * 2 + 1),
                cx, cy, hw, hh, mod);
    }

    if (!drewGround) {
        switch (t.terrain) {
            case T_TALL_GRASS:
            case T_REEDS:
                hatchDiamond(cx, cy, hw, hh, rgb(225,255,210,48), std::max(4, s.tile/5)); break;
            case T_FOREST:
            case T_PINE:
                hatchDiamond(cx, cy, hw, hh, rgb(5,30,10,58), std::max(5, s.tile/4)); break;
            case T_WATER:
            case T_SHALLOWS:
                hatchDiamond(cx, cy, hw, hh, rgb(200,240,255,42), std::max(5, s.tile/4)); break;
            case T_SAND:
            case T_DUNES:
                hatchDiamond(cx, cy, hw, hh, rgb(255,230,165,36), std::max(5, s.tile/4)); break;
            case T_STONE:
            case T_GRAVEL:
            case T_MOUNTAIN:
                hatchDiamond(cx, cy, hw, hh, rgb(255,255,255,24), std::max(5, s.tile/4)); break;
            case T_LAVA:
                hatchDiamond(cx, cy, hw, hh, rgb(255,160,60,58), std::max(4, s.tile/5)); break;
            case T_FLOWERS:
                sparkleDiamond(cx, cy, hw, hh, rgb(255,220,250,70), x, y); break;
            default:
                if (terrainFamily(t.terrain) == 0) sparkleDiamond(cx, cy, hw, hh, rgb(230,255,210,32), x, y);
                break;
        }
    }
    drawGroundShaderOverlayIso(cx, cy, hw, hh, shader, x, y);
    drawVisualTilePartImagesIso(cx, cy, hw, hh, parts, mod);
}

void updateViewMetrics(bool keepCursor) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    SDL_Rect mr = mapRect();
    int originalViewX = view.viewX;
    int originalViewY = view.viewY;
    if (!s.isometric || displayMode != DM_EMOJI) s.isoCameraActive = false;

    if (s.isometric) {
        int hw = isoHalfW();
        int hh = isoHalfH();
        int sumByWidth  = (mr.w + hw - 1) / std::max(1, hw) + 4;
        int sumByHeight = (mr.h + hh - 1) / std::max(1, hh) + 4;
        int targetSum = std::max(12, std::max(sumByWidth, sumByHeight));
        int aspectW = std::max(6, (targetSum * 3) / 5);
        int aspectH = std::max(6, targetSum - aspectW);
        if (aspectW > MAP_W) {
            aspectW = MAP_W;
            aspectH = std::max(6, targetSum - aspectW);
        }
        if (aspectH > MAP_H) {
            aspectH = MAP_H;
            aspectW = std::max(6, targetSum - aspectH);
        }
        view.viewW = std::max(1, std::min(MAP_W, aspectW));
        view.viewH = std::max(1, std::min(MAP_H, aspectH));
    } else {
        int tile = std::max(8, s.tile);
        view.viewW = std::max(1, std::min(MAP_W, (mr.w + tile - 1) / tile));
        view.viewH = std::max(1, std::min(MAP_H, (mr.h + tile - 1) / tile));
    }

    if (keepCursor) {
        if (s.isometric) {
            IsoOffsetBounds b = isoSafeOffsetBounds();
            int minOffsetX = b.minSx;
            int maxOffsetX = b.maxSx;
            int minOffsetY = b.minSy;
            int maxOffsetY = b.maxSy;
            if (minOffsetX > maxOffsetX) { minOffsetX = b.minSx; maxOffsetX = b.maxSx; }
            if (minOffsetY > maxOffsetY) { minOffsetY = b.minSy; maxOffsetY = b.maxSy; }

            int offsetX = view.cursorX - view.viewX;
            int offsetY = view.cursorY - view.viewY;
            if (offsetX < minOffsetX) view.viewX = view.cursorX - minOffsetX;
            if (offsetX > maxOffsetX) view.viewX = view.cursorX - maxOffsetX;
            if (offsetY < minOffsetY) view.viewY = view.cursorY - minOffsetY;
            if (offsetY > maxOffsetY) view.viewY = view.cursorY - maxOffsetY;
        } else {
            int fullW = topDownSafeColumns();
            int fullH = topDownSafeRows();
            int insetTiles = std::max(1, mapSafeMargin() / std::max(1, s.tile));
            if (view.cursorX < view.viewX + insetTiles) view.viewX = view.cursorX - insetTiles;
            if (view.cursorY < view.viewY + insetTiles) view.viewY = view.cursorY - insetTiles;
            if (view.cursorX >= view.viewX + fullW) view.viewX = view.cursorX - fullW + 1 + insetTiles;
            if (view.cursorY >= view.viewY + fullH) view.viewY = view.cursorY - fullH + 1 + insetTiles;
        }
    }
    if (s.isometric && displayMode == DM_EMOJI
        && (!s.isoCameraActive || view.viewX != originalViewX || view.viewY != originalViewY)) {
        syncIsoCameraToView();
    }
    int minX, maxX, minY, maxY;
    cameraBounds(minX, maxX, minY, maxY);
    if (s.isometric && displayMode == DM_EMOJI) {
        setIsoCamera(std::max((float)minX, std::min(isoCameraViewX(), (float)maxX)),
                     std::max((float)minY, std::min(isoCameraViewY(), (float)maxY)));
    } else {
        view.viewX = std::max(minX, std::min(view.viewX, maxX));
        view.viewY = std::max(minY, std::min(view.viewY, maxY));
    }
}

void clampView() {
    int minX, maxX, minY, maxY;
    cameraBounds(minX, maxX, minY, maxY);
    if (s.isometric && displayMode == DM_EMOJI) {
        if (!s.isoCameraActive) syncIsoCameraToView();
        setIsoCamera(std::max((float)minX, std::min(isoCameraViewX(), (float)maxX)),
                     std::max((float)minY, std::min(isoCameraViewY(), (float)maxY)));
    } else {
        view.viewX = std::max(minX, std::min(view.viewX, maxX));
        view.viewY = std::max(minY, std::min(view.viewY, maxY));
    }
}

void centerViewOnTile(int mx, int my) {
    updateViewMetrics(false);
    mx = std::max(0, std::min(mx, MAP_W - 1));
    my = std::max(0, std::min(my, MAP_H - 1));

    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        view.viewX = mx - view.viewW / 2;
        view.viewY = my - view.viewH / 2;
        clampTerminalView();
        return;
    }

    if (s.isometric) {
        SDL_Rect safe = mapSafeRect();
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(safe.x + safe.w / 2, safe.y + safe.h / 2, sx, sy);
        view.viewX = mx - (int)std::lround(sx);
        view.viewY = my - (int)std::lround(sy);
        if (displayMode == DM_EMOJI) syncIsoCameraToView();
    } else {
        view.viewX = mx - topDownSafeColumns() / 2;
        view.viewY = my - topDownSafeRows() / 2;
    }
    clampView();
}

bool screenToMiniMapTile(int px, int py, int& mx, int& my, bool clampToMiniMap) {
    if (s.viewportOnly) return false;
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int panelW = 24;
        int panelX = frame.cols - panelW;
        if (panelX < 1) return false;
        int mmW = panelW - 2;
        int mmH = std::max(1, std::min(view.viewH / 3, 14));
        SDL_Rect r{(panelX + 1) * frame.cellW, frame.cellH,
                   mmW * frame.cellW, mmH * frame.cellH};
        if (clampToMiniMap) {
            px = std::max(r.x, std::min(px, r.x + r.w - 1));
            py = std::max(r.y, std::min(py, r.y + r.h - 1));
        } else if (px < r.x || py < r.y || px >= r.x + r.w || py >= r.y + r.h) {
            return false;
        }

        int lx = std::max(0, std::min(px - r.x, r.w - 1));
        int ly = std::max(0, std::min(py - r.y, r.h - 1));
        mx = std::max(0, std::min(lx * MAP_W / std::max(1, r.w), MAP_W - 1));
        my = std::max(0, std::min(ly * MAP_H / std::max(1, r.h), MAP_H - 1));
        return true;
    }

    SDL_Rect r = miniMapRect();
    if (clampToMiniMap) {
        px = std::max(r.x, std::min(px, r.x + r.w - 1));
        py = std::max(r.y, std::min(py, r.y + r.h - 1));
    } else if (px < r.x || py < r.y || px >= r.x + r.w || py >= r.y + r.h) {
        return false;
    }

    int lx = std::max(0, std::min(px - r.x, r.w - 1));
    int ly = std::max(0, std::min(py - r.y, r.h - 1));
    mx = std::max(0, std::min(lx * MAP_W / std::max(1, r.w), MAP_W - 1));
    my = std::max(0, std::min(ly * MAP_H / std::max(1, r.h), MAP_H - 1));
    return true;
}

bool moveViewFromMiniMap(int px, int py, bool clampToMiniMap) {
    int mx = 0, my = 0;
    if (!screenToMiniMapTile(px, py, mx, my, clampToMiniMap)) return false;
    view.cursorX = mx;
    view.cursorY = my;
    centerViewOnTile(mx, my);
    return true;
}









void chooseZoomAnchor(int requestedX, int requestedY, int& anchorX, int& anchorY, int& mx, int& my) {
    if (requestedX >= 0 && requestedY >= 0 && screenToMap(requestedX, requestedY, mx, my)) {
        anchorX = requestedX;
        anchorY = requestedY;
        return;
    }
    if (mapTileScreenCenter(view.cursorX, view.cursorY, anchorX, anchorY)) {
        mx = view.cursorX;
        my = view.cursorY;
        return;
    }
    mapTileAtViewportCenter(mx, my, anchorX, anchorY);
}

int zoomDefaultTilePx() {
    return nearestZoomTile(activeZoomTiles(), displayMode == DM_EMOJI ? 44 : 36);
}

int zoomMaxTilePx() {
    return activeZoomTiles().back();
}

void resetZoomForDisplayMode() {
    s.tile = zoomDefaultTilePx();
    updateViewMetrics(true);
}

static int zoomTileAfterSteps(int currentTile, int steps) {
    const auto& tiles = activeZoomTiles();
    int index = nearestZoomIndex(tiles, currentTile);
    index = std::max(0, std::min((int)tiles.size() - 1, index + steps));
    return tiles[(size_t)index];
}

void setZoom(int newTile, int anchorX, int anchorY) {
    int oldTile = s.tile;
    newTile = nearestZoomTile(activeZoomTiles(), newTile);
    if (newTile == oldTile) return;

    int oldMx = view.cursorX, oldMy = view.cursorY;
    int fixedX = anchorX, fixedY = anchorY;
    chooseZoomAnchor(anchorX, anchorY, fixedX, fixedY, oldMx, oldMy);

    s.tile = newTile;
    view.cursorX = std::max(0, std::min(oldMx, MAP_W-1));
    view.cursorY = std::max(0, std::min(oldMy, MAP_H-1));
    updateViewMetrics(false);

    int newSx = 0, newSy = 0;
    if (screenToMapOffset(fixedX, fixedY, newSx, newSy)) {
        view.viewX = view.cursorX - newSx;
        view.viewY = view.cursorY - newSy;
        if (s.isometric && displayMode == DM_EMOJI) syncIsoCameraToView();
        clampView();
    } else {
        centerViewOnTile(view.cursorX, view.cursorY);
    }
    // Text textures may be re-used scaled; no need to rebuild font cache.
}

void zoomBySteps(int steps, int anchorX, int anchorY) {
    setZoom(zoomTileAfterSteps(s.tile, steps), anchorX, anchorY);
}

void startMiddlePan(int px, int py) {
    s.middleDown = true;
    s.leftDown = false;
    view.dragging = false;
    s.panStartMouseX = px;
    s.panStartMouseY = py;
    s.panStartViewX = view.viewX;
    s.panStartViewY = view.viewY;
    s.panStartIsoViewX = isoCameraViewX();
    s.panStartIsoViewY = isoCameraViewY();
}

void updateMiddlePan(int px, int py) {
    int dx = px - s.panStartMouseX;
    int dy = py - s.panStartMouseY;
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        view.viewX = s.panStartViewX - (int)std::lround(dx / (float)std::max(1, mapCellW));
        view.viewY = s.panStartViewY - (int)std::lround(dy / (float)std::max(1, mapCellH));
    } else if (s.isometric && displayMode == DM_EMOJI) {
        int hw = std::max(1, isoHalfW());
        int hh = std::max(1, isoHalfH());
        float viewDx = -0.5f * (dx / (float)hw + dy / (float)hh);
        float viewDy =  0.5f * (dx / (float)hw - dy / (float)hh);
        setIsoCamera(s.panStartIsoViewX + viewDx, s.panStartIsoViewY + viewDy);
    } else if (s.isometric) {
        int hw = std::max(1, isoHalfW());
        int hh = std::max(1, isoHalfH());
        float viewDx = -0.5f * (dx / (float)hw + dy / (float)hh);
        float viewDy =  0.5f * (dx / (float)hw - dy / (float)hh);
        view.viewX = s.panStartViewX + (int)std::lround(viewDx);
        view.viewY = s.panStartViewY + (int)std::lround(viewDy);
    } else {
        view.viewX = s.panStartViewX - (int)std::lround(dx / (float)std::max(1, s.tile));
        view.viewY = s.panStartViewY - (int)std::lround(dy / (float)std::max(1, s.tile));
    }
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) clampTerminalView();
    else clampView();
}

void moveCursorToViewCenter() {
    if (displayMode == DM_ASCII && !isAsciiMobileGui()) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        view.cursorX = view.viewX + view.viewW / 2;
        view.cursorY = view.viewY + view.viewH / 2;
    } else if (s.isometric) {
        SDL_Rect safe = mapSafeRect();
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(safe.x + safe.w / 2, safe.y + safe.h / 2, sx, sy);
        view.cursorX = view.viewX + (int)std::lround(sx);
        view.cursorY = view.viewY + (int)std::lround(sy);
    } else {
        view.cursorX = view.viewX + topDownSafeColumns() / 2;
        view.cursorY = view.viewY + topDownSafeRows() / 2;
    }
    view.cursorX = std::max(0, std::min(view.cursorX, MAP_W - 1));
    view.cursorY = std::max(0, std::min(view.cursorY, MAP_H - 1));
}
