#include "render/sdl/sdl_terminal.h"
#include "realm.h"
#include "view_state.h"

#include <cmath>

void isoTileCenterFromScreenOffset(int sx, int sy, int& cx, int& cy) {
    int ox, oy; isoOrigin(ox, oy);
    int hw = isoHalfW();
    int hh = isoHalfH();
    float fx = sx - (isoCameraViewX() - view.viewX);
    float fy = sy - (isoCameraViewY() - view.viewY);
    cx = (int)std::lround(ox + (fx - fy) * hw);
    cy = (int)std::lround(oy + (fx + fy) * hh + hh);
}

void isoScreenToOffsetFloat(int px, int py, float& sx, float& sy) {
    int ox, oy; isoOrigin(ox, oy);
    int hw = isoHalfW();
    int hh = isoHalfH();
    float fx = (px - ox) / (float)hw;
    float fy = (py - oy - hh) / (float)hh;
    sx = (fy + fx) * 0.5f + (isoCameraViewX() - view.viewX);
    sy = (fy - fx) * 0.5f + (isoCameraViewY() - view.viewY);
}

bool pointInDiamond(int px, int py, int cx, int cy, int hw, int hh) {
    if (hw <= 0 || hh <= 0) return false;
    float dx = std::abs(px - cx) / (float)hw;
    float dy = std::abs(py - cy) / (float)hh;
    return dx + dy <= 1.0f;
}

void normalizeWebAsciiViewportPoint(int& px, int& py) {
#if defined(REALM_WEB)
    if (displayMode != DM_ASCII || !s.viewportOnly) return;
    int outW = 0, outH = 0;
    SDL_GetRendererOutputSize(s.ren, &outW, &outH);
    if (s.winW <= 0 || s.winH <= 0 || outW <= 0 || outH <= 0) return;
    float sx = outW / (float)s.winW;
    float sy = outH / (float)s.winH;
    if (!std::isfinite(sx) || !std::isfinite(sy) || sx <= 1.01f || sy <= 1.01f) return;
    px = (int)std::lround(px / sx);
    py = (int)std::lround(py / sy);
#else
    (void)px;
    (void)py;
#endif
}

bool screenToMap(int px, int py, int& mx, int& my) {
    normalizeWebAsciiViewportPoint(px, py);
    if (displayMode == DM_ASCII && !isAsciiMobileGui() && !s.viewportOnly) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;
        int sx = (px - mr.x) / std::max(1, mapCellW);
        int sy = (py - mr.y) / std::max(1, mapCellH);
        if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) return false;
        mx = view.viewX + sx;
        my = view.viewY + sy;
        return inBounds(mx, my);
    }

    if (tilesetHudConsumesPointer(px, py)) return false;

    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

    if (isAsciiMobileGui() && !s.viewportOnly) {
        int cellW = 8, cellH = 15;
        asciiMobileCellMetrics(cellW, cellH);
        int sx = (px - mr.x) / std::max(1, cellW);
        int sy = (py - mr.y) / std::max(1, cellH);
        mx = view.viewX + sx;
        my = view.viewY + sy;
        int cols = std::max(1, std::min(MAP_W, mr.w / std::max(1, cellW)));
        int rows = std::max(1, std::min(MAP_H, mr.h / std::max(1, cellH)));
        return inBounds(mx, my) && sx < cols && sy < rows;
    }

    if (!s.isometric) {
        int sx = (px - mr.x) / s.tile;
        int sy = (py - mr.y) / s.tile;
        mx = view.viewX + sx;
        my = view.viewY + sy;
        return inBounds(mx,my) && sx < view.viewW && sy < view.viewH;
    }

    int hw = isoHalfW();
    int hh = isoHalfH();
    float sxF = 0.0f, syF = 0.0f;
    isoScreenToOffsetFloat(px, py, sxF, syF);
    int baseX = (int)std::floor(sxF);
    int baseY = (int)std::floor(syF);
    IsoOffsetBounds b = isoVisibleOffsetBounds();

    for (int dy = -1; dy <= 2; ++dy) {
        for (int dx = -1; dx <= 2; ++dx) {
            int sx = baseX + dx, sy = baseY + dy;
            if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) continue;
            int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
            if (!pointInDiamond(px, py, cx, cy, hw, hh)) continue;
            mx = view.viewX + sx;
            my = view.viewY + sy;
            return inBounds(mx,my);
        }
    }
    return false;
}

bool screenToMapOffset(int px, int py, int& sxOut, int& syOut) {
    normalizeWebAsciiViewportPoint(px, py);
    if (displayMode == DM_ASCII && !isAsciiMobileGui() && !s.viewportOnly) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;
        sxOut = (px - mr.x) / std::max(1, mapCellW);
        syOut = (py - mr.y) / std::max(1, mapCellH);
        return sxOut >= 0 && syOut >= 0 && sxOut < view.viewW && syOut < view.viewH;
    }

    if (tilesetHudConsumesPointer(px, py)) return false;

    SDL_Rect mr = mapRect();
    if (px < mr.x || py < mr.y || px >= mr.x + mr.w || py >= mr.y + mr.h) return false;

    if (isAsciiMobileGui() && !s.viewportOnly) {
        int cellW = 8, cellH = 15;
        asciiMobileCellMetrics(cellW, cellH);
        sxOut = (px - mr.x) / std::max(1, cellW);
        syOut = (py - mr.y) / std::max(1, cellH);
        return true;
    }

    if (!s.isometric) {
        sxOut = (px - mr.x) / std::max(1, s.tile);
        syOut = (py - mr.y) / std::max(1, s.tile);
        return true;
    }

    int hw = isoHalfW();
    int hh = isoHalfH();
    float sxF = 0.0f, syF = 0.0f;
    isoScreenToOffsetFloat(px, py, sxF, syF);
    int baseX = (int)std::floor(sxF);
    int baseY = (int)std::floor(syF);
    IsoOffsetBounds b = isoVisibleOffsetBounds();

    for (int dy = -1; dy <= 2; ++dy) {
        for (int dx = -1; dx <= 2; ++dx) {
            int sx = baseX + dx, sy = baseY + dy;
            if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) continue;
            int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
            if (!pointInDiamond(px, py, cx, cy, hw, hh)) continue;
            sxOut = sx;
            syOut = sy;
            return true;
        }
    }

    sxOut = baseX;
    syOut = baseY;
    return true;
}

bool mapTileScreenCenter(int mx, int my, int& px, int& py) {
    if (!inBounds(mx, my)) return false;
    if (displayMode == DM_ASCII && !isAsciiMobileGui() && !s.viewportOnly) {
        SDL_GetWindowSize(s.win, &s.winW, &s.winH);
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
        int mapCellW = 9, mapCellH = 18;
        terminalMapCellMetrics(mapCellW, mapCellH);
        SDL_Rect mr = terminalMapPixelRect(frame);
        int sx = mx - view.viewX;
        int sy = my - view.viewY;
        if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) return false;
        px = mr.x + sx * mapCellW + mapCellW / 2;
        py = mr.y + sy * mapCellH + mapCellH / 2;
        return px >= 0 && py >= 0 && px < s.winW && py < s.winH;
    }

    SDL_Rect mr = mapRect();
    int sx = mx - view.viewX;
    int sy = my - view.viewY;

    if (!s.isometric) {
        if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) return false;
        px = mr.x + sx * s.tile + s.tile / 2;
        py = mr.y + sy * s.tile + s.tile / 2;
    } else {
        IsoOffsetBounds b = isoVisibleOffsetBounds();
        if (sx < b.minSx || sx > b.maxSx || sy < b.minSy || sy > b.maxSy) return false;
        isoTileCenterFromScreenOffset(sx, sy, px, py);
    }

    return px >= mr.x && py >= mr.y && px < mr.x + mr.w && py < mr.y + mr.h;
}

bool mapTileAtViewportCenter(int& mx, int& my, int& px, int& py) {
    if (displayMode == DM_ASCII && !isAsciiMobileGui() && !s.viewportOnly) {
        TerminalFrame frame = makeBlankTerminalFrame();
        updateTerminalCamera(frame.cols, frame.rows, false);
        mx = view.viewX + view.viewW / 2;
        my = view.viewY + view.viewH / 2;
        mx = std::max(0, std::min(mx, MAP_W - 1));
        my = std::max(0, std::min(my, MAP_H - 1));
        return mapTileScreenCenter(mx, my, px, py);
    }

    SDL_Rect safe = mapSafeRect();
    px = safe.x + safe.w / 2;
    py = safe.y + safe.h / 2;
    if (screenToMap(px, py, mx, my)) return true;

    if (s.isometric) {
        float sx = 0.0f, sy = 0.0f;
        isoScreenToOffsetFloat(px, py, sx, sy);
        mx = view.viewX + (int)std::lround(sx);
        my = view.viewY + (int)std::lround(sy);
    } else {
        mx = view.viewX + topDownSafeColumns() / 2;
        my = view.viewY + topDownSafeRows() / 2;
    }
    mx = std::max(0, std::min(mx, MAP_W - 1));
    my = std::max(0, std::min(my, MAP_H - 1));
    return true;
}
