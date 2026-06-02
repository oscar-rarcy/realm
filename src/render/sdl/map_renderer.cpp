#include "render/sdl/sdl_map.h"
#include "render/visual_model.h"
#include "view_state.h"

static const RenderModel* activeRenderModel = nullptr;

static const TileRenderInfo* activeTileInfoAt(int mx, int my) {
    if (!activeRenderModel) return nullptr;
    int sx = mx - activeRenderModel->viewX;
    int sy = my - activeRenderModel->viewY;
    if (sx < 0 || sy < 0 || sx >= activeRenderModel->viewW || sy >= activeRenderModel->viewH) return nullptr;
    size_t index = (size_t)sy * (size_t)activeRenderModel->viewW + (size_t)sx;
    if (index >= activeRenderModel->tiles.size()) return nullptr;
    return &activeRenderModel->tiles[index];
}

static const ActionMarkerRenderInfo* activeActionMarkerAt(int mx, int my) {
    if (!activeRenderModel) return nullptr;
    for (const ActionMarkerRenderInfo& marker : activeRenderModel->actionMarkers) {
        if (marker.x == mx && marker.y == my) return &marker;
    }
    return nullptr;
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

const char* seasonNameSafe() { return getSeasonName(); }
const char* timeNameSafe() { return getTimeName(); }
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

std::string cursorStackSummary() {
    if (!inBounds(view.cursorX, view.cursorY) || !g.map[view.cursorY][view.cursorX].visible[0]) return "Stack: not visible";
    std::ostringstream ss;
    int count = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.state == S_GARRISONED) continue;
        auto& st = STATS[e.type];
        bool covers = st.isBuilding
            ? (view.cursorX >= e.x && view.cursorX < e.x + st.sizeW && view.cursorY >= e.y && view.cursorY < e.y + st.sizeH)
            : (view.cursorX == e.x && view.cursorY == e.y);
        if (!covers) continue;
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
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (view.cursorX >= e.x && view.cursorX < e.x + st.sizeW && view.cursorY >= e.y && view.cursorY < e.y + st.sizeH)
                : (view.cursorX == e.x && view.cursorY == e.y);
            if (covers) more++;
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
    Color bg = rgb(0,0,0);
    Color fg = rgb(230,230,220);
    std::string glyph;
    bool emoji = false;
    bool tint = false;
};

TileVisual makeTileVisual(int mx, int my) {
    TileVisual v;
    const Tile& tile = g.map[my][mx];
    const TileRenderInfo* tileInfo = activeTileInfoAt(mx, my);
    v.visible = tileInfo ? tileInfo->visible : tile.visible[0];
    v.explored = tileInfo ? tileInfo->explored : tile.explored[0];
    if (!v.explored) { v.bg = rgb(8,9,12); return v; }

    v.ent = v.visible ? entityAt(mx,my) : nullptr;
    if (!v.ent && v.visible) v.ent = corpseAt(mx, my);
    v.cursor = (mx == view.cursorX && my == view.cursorY);
    v.bg = terrainBg(tile, mx, my);

    if (v.ent && v.ent->alive && v.ent->owner != OWNER_NATURE && (isUnit(v.ent->type) || isBuilding(v.ent->type)))
        v.bg = timeTint(ownerBg(v.ent->owner));
    v.bg = applyVisionAndLight(v.bg, mx, my);
    if (v.cursor) v.bg = blend(v.bg, rgb(225, 190, 50), 0.78f);

    v.fg = glyphColorForTerrain(tile, mx, my);
    if (displayMode == DM_ASCII) {
        v.emoji = false;
        if (v.visible && v.ent && v.ent->alive) {
            v.glyph.assign(1, STATS[v.ent->type].glyph);
            v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(230,230,210) : rgb(255,255,255);
        } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
            v.glyph.assign(1, v.ent->deathTicks >= DEATH_DECAY_TICKS ? '*' : '%');
            v.fg = rgb(180,180,170);
        } else if (v.visible) {
            v.glyph.assign(1, terrainAscii(tile.terrain));
        } else {
            v.glyph = "."; v.fg = rgb(95,95,105,150);
        }
    } else if (v.visible && v.ent && v.ent->alive) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(*v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = (v.ent->owner == OWNER_NATURE) ? rgb(245,245,235) : rgb(255,255,255);
        v.tint = usesSymbolFont;
    } else if (v.visible && v.ent && v.ent->state == S_DEAD) {
        bool usesSymbolFont = false;
        v.glyph = tilesetEntityVisual(*v.ent, usesSymbolFont);
        v.emoji = usesSymbolFont;
        v.fg = rgb(190,190,180);
        v.tint = false;
    } else if (v.visible) {
        logMissingTerrainImageTile(tile.terrain);
        logMissingVisualTileParts(tile);
        v.glyph = terrainGlyph(tile, mx, my);
        v.emoji = isResourceEmojiTerrain(tile.terrain);
        v.tint = v.emoji;
    } else {
        v.glyph = "·"; v.emoji = false; v.fg = rgb(95,95,105,150);
    }
    v.fg = applyVisionToGlyph(v.fg, mx, my);

    v.selected = (v.visible && v.ent && isSelected(v.ent));
    if (v.visible && !v.ent) {
        const ActionMarkerRenderInfo* marker = activeActionMarkerAt(mx, my);
        if (marker && marker->ticks > 0 && (g.tick % 6) < 4) {
            v.glyph = (marker->glyph == '#') ? u8"■" : (marker->glyph == '!') ? "!" : u8"×";
            v.emoji = false;
            v.tint = false;
            v.fg = rgb(255,235,105);
        }
    }
    return v;
}

void drawTile(int mx, int my, SDL_Rect r) {
    const Tile& tile = g.map[my][mx];
    TileVisual v = makeTileVisual(mx, my);

    if (!v.explored) {
        setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(24,28,34,120)); SDL_RenderDrawRect(s.ren, &r);
        return;
    }

    setDraw(v.bg); SDL_RenderFillRect(s.ren, &r);
    applyTerrainTexture(r, tile, mx, my);

    if (!v.glyph.empty()) drawCentered(v.glyph, r, v.fg, v.emoji, v.tint);
    drawFeatureOccluderIfNeeded(mx, my, r);

    // HP sliver for damaged visible entities.
    if (v.visible && v.ent && v.ent->alive && v.ent->hp < v.ent->maxHp) {
        int w = std::max(1, r.w * v.ent->hp / std::max(1, v.ent->maxHp));
        SDL_Rect hb{r.x+2, r.y+r.h-4, std::max(1, r.w-4), 2};
        setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
        hb.w = std::max(1, w-4);
        setDraw(v.ent->hp*3 > v.ent->maxHp*2 ? rgb(65,230,90) : v.ent->hp*3 > v.ent->maxHp ? rgb(230,210,70) : rgb(230,60,55));
        SDL_RenderFillRect(s.ren, &hb);
    }

    if (v.selected) {
        SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
        setDraw(rgb(255,255,255,180));
        SDL_RenderDrawRect(s.ren, &r);
        SDL_Rect r2{r.x+1,r.y+1,r.w-2,r.h-2}; SDL_RenderDrawRect(s.ren, &r2);
    }

    if (v.cursor) {
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
    setStatus(s.fullscreen ? "Fullscreen." : "Windowed.");
#endif
}

void drawMobileBuildPreviewTopDown() {
    if (!isMobileGui() || s.mobileBuildType == E_NONE) return;
    SDL_Rect mr = mapRect();
    EntityType bt = s.mobileBuildType;
    bool ok = canPlace(bt, view.cursorX, view.cursorY, 0);
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

void drawMobileBuildPreviewIso() {
    if (!isMobileGui() || s.mobileBuildType == E_NONE) return;
    EntityType bt = s.mobileBuildType;
    bool ok = canPlace(bt, view.cursorX, view.cursorY, 0);
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

void drawIsoTileBase(int mx, int my) {
    int sx = mx - view.viewX, sy = my - view.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    int hw = isoHalfW(), hh = isoHalfH();
    const Tile& tile = g.map[my][mx];
    TileVisual v = makeTileVisual(mx, my);
    fillDiamond(cx, cy, hw, hh, v.bg);
    if (v.explored) applyTerrainTextureIso(cx, cy, hw, hh, tile, mx, my);
    if (!v.explored) drawDiamondOutline(cx, cy, hw, hh, rgb(20,22,26,160));
}

void drawIsoTileForeground(int mx, int my) {
    int sx = mx - view.viewX, sy = my - view.viewY;
    int cx, cy; isoTileCenterFromScreenOffset(sx, sy, cx, cy);
    int hw = isoHalfW(), hh = isoHalfH();
    TileVisual v = makeTileVisual(mx, my);

    if (!v.glyph.empty()) {
        // Upright sprite/glyph over the flat isometric board.  The diamond is
        // isometric; the emoji/text itself is not skewed.
        int glyphSize = v.emoji ? std::max(16, (int)(s.tile * 0.96f)) : std::max(12, (int)(s.tile * 0.78f));
        if (v.visible && v.ent && imageTilesetEnabled()) {
            glyphSize = std::max(glyphSize, (int)(s.tile * 1.55f));
        }
        SDL_Rect gr{cx - glyphSize/2, cy - glyphSize/2, glyphSize, glyphSize};
        bool drewImage = false;
        if (v.visible && v.ent) {
            Color mod = applyVisionToGlyph(rgb(255,255,255), mx, my);
            drewImage = drawEntityImageTile(*v.ent, gr, mod);
        }
        if (!drewImage) {
            drawCentered(v.glyph, gr, v.visible ? v.fg : scale(v.fg, 0.55f), v.emoji, v.tint);
        }
        drawFeatureOccluderIfNeeded(mx, my, gr);
    }

    if (v.visible && v.ent && v.ent->alive && v.ent->hp < v.ent->maxHp) {
        int barW = std::max(8, s.tile);
        SDL_Rect hb{cx - barW/2, cy + hh - 5, barW, 3};
        setDraw(rgb(80,20,20,190)); SDL_RenderFillRect(s.ren, &hb);
        hb.w = std::max(1, barW * v.ent->hp / std::max(1, v.ent->maxHp));
        setDraw(v.ent->hp*3 > v.ent->maxHp*2 ? rgb(65,230,90) : v.ent->hp*3 > v.ent->maxHp ? rgb(230,210,70) : rgb(230,60,55));
        SDL_RenderFillRect(s.ren, &hb);
    }

    if (v.selected) {
        drawDiamondOutline(cx, cy, hw-1, hh-1, rgb(255,255,255,210));
        if (hw > 4 && hh > 3) drawDiamondOutline(cx, cy, hw-4, hh-3, rgb(255,255,255,110));
    }
    if (v.cursor) {
        drawDiamondOutline(cx, cy, hw, hh, rgb(40,20,0,240));
        if (hw > 3 && hh > 2) drawDiamondOutline(cx, cy, hw-3, hh-2, rgb(255,245,150,210));
    }
}

void drawMapIso() {
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);
    RenderModel model = buildRenderModel(g, 0, view.viewX, view.viewY, view.viewW, view.viewH);
    activeRenderModel = &model;

    IsoOffsetBounds b = isoVisibleOffsetBounds();
    int minSum = b.minSx + b.minSy;
    int maxSum = b.maxSx + b.maxSy;
    for (int sum = minSum; sum <= maxSum; ++sum) {
        for (int sy = b.minSy; sy <= b.maxSy; ++sy) {
            int sx = sum - sy;
            if (sx < b.minSx || sx > b.maxSx) continue;
            int mx = view.viewX + sx, my = view.viewY + sy;
            if (!inBounds(mx, my)) continue;
            drawIsoTileBase(mx, my);
        }
    }
    for (int sum = minSum; sum <= maxSum; ++sum) {
        for (int sy = b.minSy; sy <= b.maxSy; ++sy) {
            int sx = sum - sy;
            if (sx < b.minSx || sx > b.maxSx) continue;
            int mx = view.viewX + sx, my = view.viewY + sy;
            if (!inBounds(mx, my)) continue;
            drawIsoTileForeground(mx, my);
        }
    }

    if (s.leftDown) {
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
    drawMobileBuildPreviewIso();
    activeRenderModel = nullptr;
    SDL_RenderSetClipRect(s.ren, nullptr);
}

void drawMap() {
    if (s.isometric) { drawMapIso(); return; }
    SDL_Rect mr = mapRect();
    setDraw(rgb(4,6,8)); SDL_RenderFillRect(s.ren, &mr);
    updateViewMetrics(!s.middleDown);
    SDL_RenderSetClipRect(s.ren, &mr);
    RenderModel model = buildRenderModel(g, 0, view.viewX, view.viewY, view.viewW, view.viewH);
    activeRenderModel = &model;

    for (int sy=0; sy<view.viewH; ++sy) {
        for (int sx=0; sx<view.viewW; ++sx) {
            int mx = view.viewX + sx, my = view.viewY + sy;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx*s.tile, mr.y + sy*s.tile, s.tile, s.tile};
            drawTile(mx,my,r);
        }
    }

    // Drag selection rectangle.
    if (s.leftDown) {
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
    drawMobileBuildPreviewTopDown();
    activeRenderModel = nullptr;
    SDL_RenderSetClipRect(s.ren, nullptr);
}
