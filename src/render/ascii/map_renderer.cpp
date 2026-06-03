#include "realm.h"
#include "render/visual_model.h"
#include "view_state.h"
#include "input_keys.h"

void renderMap() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    int panelW = 24; view.viewW = maxX - panelW - 1; view.viewH = maxY - 4;
    if (view.viewW < 30) view.viewW = maxX;
    if (view.viewH < 10) view.viewH = maxY - 2;
    view.viewW = std::min(view.viewW, MAP_W); view.viewH = std::min(view.viewH, MAP_H);

    if (view.cursorX < view.viewX+3)            view.viewX = view.cursorX - 3;
    if (view.cursorX > view.viewX+view.viewW-4)    view.viewX = view.cursorX - view.viewW + 4;
    if (view.cursorY < view.viewY+2)            view.viewY = view.cursorY - 2;
    if (view.cursorY > view.viewY+view.viewH-3)    view.viewY = view.cursorY - view.viewH + 3;
    view.viewX = std::max(0, std::min(view.viewX, MAP_W - view.viewW));
    view.viewY = std::max(0, std::min(view.viewY, MAP_H - view.viewH));

    RenderModel model = buildRenderModel(g, 0, view.viewX, view.viewY, view.viewW, view.viewH);
    WorldIndex world = buildWorldIndex(g);
    std::vector<const TileRenderInfo*> tileInfos(view.viewW * view.viewH, nullptr);
    for (const TileRenderInfo& tileInfo : model.tiles) {
        int sx = tileInfo.x - view.viewX;
        int sy = tileInfo.y - view.viewY;
        if (sx >= 0 && sy >= 0 && sx < view.viewW && sy < view.viewH) {
            tileInfos[sy * view.viewW + sx] = &tileInfo;
        }
    }
    auto tileInfoAt = [&](int mx, int my) -> const TileRenderInfo* {
        int sx = mx - view.viewX;
        int sy = my - view.viewY;
        if (sx < 0 || sy < 0 || sx >= view.viewW || sy >= view.viewH) return nullptr;
        return tileInfos[sy * view.viewW + sx];
    };

    bool night = isNight(g);

    // Selected ranged unit/tower: precompute range-ring centre + radius.
    int ringX = -1, ringY = -1, ringR = 0;
    Entity* selR = findEntity(g, world, g.selectedId);
    if (selR && selR->alive && selR->owner == 0) {
        int rng = STATS[selR->type].range;
        if (selR->type == E_ARCHER && (g.players[0].research & R_CROSSBOWS)) rng += 2;
        if (rng > 1) {
            auto& ss = STATS[selR->type];
            ringX = selR->x + ss.sizeW/2; ringY = selR->y + ss.sizeH/2; ringR = rng;
        }
    }

    // Precompute drag-selection box (map coords); -1 means no active box
    int boxX0 = -1, boxY0 = -1, boxX1 = -1, boxY1 = -1;
    if (view.dragging && g.mode != M_WALL_DRAG) {
        boxX0 = std::min(view.dragStartX, view.cursorX);
        boxY0 = std::min(view.dragStartY, view.cursorY);
        boxX1 = std::max(view.dragStartX, view.cursorX);
        boxY1 = std::max(view.dragStartY, view.cursorY);
    }

    // Precompute wall drag preview line (Bresenham)
    static bool wallPrev[MAP_H][MAP_W];
    memset(wallPrev, 0, sizeof(wallPrev));
    if (g.mode == M_WALL_DRAG && view.dragging) {
        int x0=view.wallDragX, y0=view.wallDragY, x1=view.cursorX, y1=view.cursorY;
        int dx=std::abs(x1-x0), sx=x0<x1?1:-1;
        int dy=-std::abs(y1-y0), sy2=y0<y1?1:-1;
        int err=dx+dy;
        while (true) {
            if (inBounds(x0,y0)) wallPrev[y0][x0] = true;
            if (x0==x1 && y0==y1) break;
            int e2=2*err;
            if (e2>=dy){err+=dy; x0+=sx;}
            if (e2<=dx){err+=dx; y0+=sy2;}
        }
    }

    for (int sy = 0; sy < view.viewH; sy++) { int my = view.viewY + sy;
        for (int sx = 0; sx < view.viewW; sx++) { int mx = view.viewX + sx;
            int scY = sy+2, scX = sx;
            if (!inBounds(mx, my)) { mvaddch(scY, scX, ' '); continue; }
            const TileRenderInfo* tileInfo = tileInfoAt(mx, my);
            Terrain terrain = tileInfo ? tileInfo->terrain : g.map[my][mx].terrain;
            bool vis = tileInfo ? tileInfo->visible : g.map[my][mx].visible[0];
            bool expl = tileInfo ? tileInfo->explored : g.map[my][mx].explored[0];
            bool isCur = (mx == view.cursorX && my == view.cursorY);

            if (!expl) {
                if (isCur) { attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, ' '); attroff(COLOR_PAIR(CP_CURSOR)); }
                else { mvaddch(scY, scX, ' '); }
                continue;
            }

            char ch; int cp;
            getTerrainVisual(terrain, mx, my, ch, cp);

            if (!vis) {
                if (isCur) {
                    attron(COLOR_PAIR(CP_CURSOR));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else                         mvprintw(scY, scX, "%s", getCharEmoji(ch));
                    attroff(COLOR_PAIR(CP_CURSOR));
                } else {
                    attron(COLOR_PAIR(CP_FOG_EXPLORED));
                    if (displayMode == DM_ASCII) mvaddch(scY, scX, ch);
                    else                         mvprintw(scY, scX, "%s", getCharEmoji(ch));
                    attroff(COLOR_PAIR(CP_FOG_EXPLORED));
                }
                continue;
            }

            // Wall drag preview overrides terrain.
            // Emoji mode shows ■ (solid block) matching the completed wall glyph.
            if (wallPrev[my][mx]) { ch = '#'; cp = CP_PLAYER; }

            // Use a chtype-wide draw glyph so completed walls can use the ACS solid block.
            chtype drawCh = (chtype)ch;
            if (wallPrev[my][mx]) drawCh = ACS_CKBOARD;

            Entity* ent = entityAt(g, world, mx, my);
            if (!ent) ent = corpseAt(g, world, mx, my);
            // Cloaking: enemy units fade at night/storm unless a friendly eye is close.
            // Wheat fields also conceal enemies — units in crops need close detection.
            bool inCrop = ent && !isBuilding(ent->type) && terrain == T_WHEAT;
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && (isConcealing(g) || inCrop) && !isDetectedBy(g, mx, my, 0)) ent = nullptr;

            // Catapult/ram body tile: render the body from the adjacent slot so
            // the normal terrain pass for this cell cannot overwrite it.
            if (displayMode == DM_ASCII && !ent && inBounds(mx-1, my)) {
                Entity* leftEnt = entityAt(g, world, mx-1, my);
                if (leftEnt && leftEnt->alive && !leftEnt->underConstruction
                    && (leftEnt->type == E_CATAPULT || leftEnt->type == E_RAM
                        || (leftEnt->type == E_TREBUCHET && leftEnt->packed == 0 && leftEnt->packTicks == 0))) {
                    const TileRenderInfo* leftTileInfo = tileInfoAt(mx - 1, my);
                    bool inCropLeft = (leftTileInfo ? leftTileInfo->terrain : g.map[my][mx-1].terrain) == T_WHEAT;
                    bool leftCloaked = leftEnt->owner != 0 && leftEnt->owner < MAX_PLAYERS
                        && (isConcealing(g) || inCropLeft) && !isDetectedBy(g, mx-1, my, 0);
                    if (!leftCloaked) {
                        char sc = (leftEnt->type == E_CATAPULT) ? 'c' : (leftEnt->type == E_TREBUCHET ? 'q' : 'r');
                        bool bodyIsSel = leftEnt->id == g.selectedId;
                        if (!bodyIsSel)
                            for (int sid : g.selectedIds) if (sid == leftEnt->id) { bodyIsSel = true; break; }
                        int bcp = ownerColorPair(leftEnt->owner, night);
                        int sattr = COLOR_PAIR(bcp) | A_BOLD;
                        if (bodyIsSel) sattr |= A_REVERSE;
                        if (isCur) {
                            attron(COLOR_PAIR(CP_CURSOR)); mvaddch(scY, scX, sc); attroff(COLOR_PAIR(CP_CURSOR));
                        } else {
                            attron(sattr); mvaddch(scY, scX, sc); attroff(sattr);
                        }
                        continue;
                    }
                }
            }

            // Render priority + stack count. When multiple units share a tile,
            // prefer the highest-value military so e.g. knights show through a
            // pile of peasants. Also count any same-owner combat units on the
            // tile so we can show an uppercase glyph for stacks of 2+.
            int stackedMil = 0;
            if (ent && ent->alive && !isBuilding(ent->type)) {
                auto isMil = [](EntityType t) {
                    return t==E_MILITIA||t==E_ARCHER||t==E_KNIGHT||t==E_SPEARMAN||t==E_CATAPULT||t==E_TREBUCHET
                        || t==E_WARSHIP;
                };
                int prio = isMil(ent->type) ? 1 : 0;
                for (auto& other : g.entities) {
                    if (!other.alive || other.state == S_GARRISONED) continue;
                    if (other.x != mx || other.y != my) continue;
                    if (other.owner != ent->owner) continue;
                    if (!isMil(other.type)) continue;
                    stackedMil++;
                    if (prio == 0) { ent = &other; prio = 1; }
                }
            }
            // emojiStr: the UTF-8 string to display in emoji mode.
            // Initialised to terrain glyph; overridden when an entity is present.
            const char* emojiStr = nullptr;

            if (ent && ent->alive) {
                ch = STATS[ent->type].glyph;
                // ASCII mode: uppercase glyph signals a stack of 2+ military.
                // Emoji mode: no uppercase equivalent — stack not indicated.
                if (displayMode == DM_ASCII && stackedMil >= 2 && ch >= 'a' && ch <= 'z')
                    ch = ch - 'a' + 'A';
                drawCh = (chtype)ch;

                // Default emoji is the entity's body symbol.
                emojiStr = getEntityEmoji(ent->type);

                // Colour pair: ownership → background colour.
                // Completed farms keep wheat colours regardless of owner.
                // Gaia/nature (animals) use type-specific foreground colours with
                // no ownership background — they are neutral entities.
                // Ships keep the wood-deck background for their hull look.
                if (ent->type == E_FARM && !ent->underConstruction)
                    cp = (getSeason(g) == SUMMER) ? CP_WHEAT_GOLD : CP_WHEAT;
                else if (ent->owner == OWNER_NATURE) {
                    if      (ent->type == E_WOLF)  cp = CP_WOLF;
                    else if (ent->type == E_SHEEP) cp = CP_SHEEP;
                    else if (ent->type == E_BOAR)  cp = CP_BOAR;
                    else                           cp = CP_DEER;
                } else {
                    // Player-owned land units and buildings: background = owner colour.
                    cp = ownerColorPair(ent->owner, night);
                }
                // Ships override — wood-deck background preserved on water.
                if (isNaval(ent->type)) {
                    switch (ent->owner) {
                        case 0: cp = CP_SHIP_P0; break;
                        case 1: cp = CP_SHIP_P1; break;
                        case 2: cp = CP_SHIP_P2; break;
                        case 3: cp = CP_SHIP_P3; break;
                        default: cp = CP_SHIP_ENEMY; break;
                    }
                }

                // State-specific glyph overrides (gate, construction, siege engines, alert).
                if (ent->type == E_GATE && !ent->underConstruction) {
                    ch = ent->gateOpen ? '-' : '|';
                    drawCh = (chtype)ch;
                    // ▬ = open (horizontal bar), ║ = closed (double vertical)
                    emojiStr = ent->gateOpen ? "\xe2\x96\xac" : "\xe2\x95\x91";
                }
                if (ent->underConstruction && g.tick%10 < 5) {
                    ch = '#'; drawCh = (chtype)ch;
                    emojiStr = "\xe2\x96\xa0";  // ■ pulsing during construction
                }
                // Dwarf-Fortress-style solid wall block when complete.
                // Emoji mode uses ■ (same visual intent, but valid UTF-8).
                if (ent->type == E_WALL && !ent->underConstruction) {
                    drawCh = ACS_CKBOARD;
                    emojiStr = "\xe2\x96\xa0";  // ■ U+25A0
                }
                if (ent->type == E_CASTLE && !ent->underConstruction && displayMode == DM_ASCII) {
                    int dx = mx - ent->x, dy = my - ent->y;
                    bool corner = (dx == 0 || dx == 3) && (dy == 0 || dy == 3);
                    if (corner)                     drawCh = ACS_CKBOARD;
                    else if (dy == 0 || dy == 3) { ch = '='; drawCh = (chtype)ch; }
                    else if (dx == 0 || dx == 3) { ch = '|'; drawCh = (chtype)ch; }
                    else                         { ch = '.'; drawCh = (chtype)ch; }
                }
                // Siege engine arm animations.
                // Catapult: arm at rest = ◄ (loaded), arm firing = ╱ (swinging).
                if (ent->type == E_CATAPULT) {
                    bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_CATAPULT].atkSpeed - 3;
                    ch = firing ? '/' : '-'; drawCh = (chtype)ch;
                    emojiStr = firing ? "\xe2\x95\xb1" : "\xe2\x97\x84"; // ╱ : ◄
                }
                if (ent->type == E_TREBUCHET) {
                    bool firing = ent->state==S_ATTACKING && ent->atkCd > STATS[E_TREBUCHET].atkSpeed - 5;
                    ch = ent->packed ? 'q' : (firing ? '/' : 'L');
                    drawCh = (chtype)ch;
                    emojiStr = ent->packed ? "q" : (firing ? "\xe2\x95\xb1" : "L");
                }
                // Ram: approaching = ► (pointer), ramming = ▶ (larger triangle, impact).
                if (ent->type == E_RAM) {
                    bool ramming = ent->state==S_ATTACKING && ent->atkCd > STATS[E_RAM].atkSpeed*2/3;
                    ch = ramming ? '=' : '-'; drawCh = (chtype)ch;
                    emojiStr = ramming ? "\xe2\x96\xb6" : "\xe2\x96\xba"; // ▶ : ►
                }
                // Peasant work/idle cycle (ASCII only — emoji peasants have their
                // own state-aware glyph). Staggered per-id so a busy village
                // doesn't strobe in sync.
                if (displayMode == DM_ASCII && ent->type == E_PEASANT) {
                    int cyc = (g.tick + ent->id*5) % 30;
                    if      (ent->state == S_GATHERING && cyc < 3) { ch = '*'; drawCh = (chtype)ch; }
                    else if (ent->state == S_BUILDING  && cyc < 3) { ch = '+'; drawCh = (chtype)ch; }
                    else if (ent->state == S_RETURNING && cyc < 2) { ch = ','; drawCh = (chtype)ch; }
                    else if (ent->state == S_IDLE) {
                        // Slow daydream pulse: '?' shown ~1 s every ~20 s, staggered.
                        int slow = (g.tick + ent->id*47) % 250;
                        if (slow < 12) { ch = '?'; drawCh = (chtype)ch; }
                    }
                }
                // Recently in combat: gentle '!' pulse — ~1.5 Hz, not strobing.
                if (ent->alertTicks > 0 && (g.tick % 8) < 4) {
                    ch = '!'; drawCh = (chtype)ch;
                    emojiStr = "!";
                }
            } else if (ent && ent->state == S_DEAD) {
                bool decayed = ent->deathTicks >= DEATH_DECAY_TICKS;
                ch = decayed ? '*' : '%';
                drawCh = (chtype)ch;
                cp = CP_RUINS;
                emojiStr = decayed ? "\xe2\x98\xa0" : "\xe2\x80\xa0"; // ☠ : †
            }
            // Projectile overwrites terrain/entity glyph; keep ASCII char for colour lookup.
            for (auto& p : g.projectiles) {
                if (!p.alive) continue;
                if ((int)roundf(p.x)==mx && (int)roundf(p.y)==my) {
                    ch = p.glyph; cp = p.color; drawCh = (chtype)ch;
                    // Projectile emoji: boulder → ● (solid circle), arrow/bolt → · (dot)
                    emojiStr = (p.color == CP_PROJ_BOULDER)
                               ? "\xe2\x97\x8f"   // ● U+25CF
                               : "\xc2\xb7";       // · U+00B7
                }
            }
            for (const auto& m : model.actionMarkers) {
                if (m.x == mx && m.y == my && m.ticks > 0 && (g.tick % 6) < 4) {
                    ch = m.glyph; cp = CP_UI_HIGH; drawCh = (chtype)ch;
                    emojiStr = (m.glyph == '!') ? "!" : (m.glyph == '#') ? "\xe2\x96\xa0" : "\xc3\x97";
                }
            }

            // When no entity is present, terrain char drives the emoji string.
            if (!emojiStr) emojiStr = getCharEmoji(ch);

            bool isSel = false;

            // Single selection highlight
            Entity* sel = findEntity(g, world, g.selectedId);
            if (sel && !isCur) {
                auto& ss = STATS[sel->type];
                if (ss.isBuilding) {
                    if (mx>=sel->x && mx<sel->x+ss.sizeW && my>=sel->y && my<sel->y+ss.sizeH) isSel = true;
                } else if (mx==sel->x && my==sel->y) isSel = true;
            }
            // Group selection highlight
            if (!isSel && !g.selectedIds.empty()) {
                for (int sid : g.selectedIds) {
                    Entity* se = findEntity(g, world, sid);
                    if (se && mx==se->x && my==se->y) { isSel = true; break; }
                }
            }

            bool onBoxBorder = (boxX0 >= 0)
                && mx >= boxX0 && mx <= boxX1 && my >= boxY0 && my <= boxY1
                && (mx == boxX0 || mx == boxX1 || my == boxY0 || my == boxY1);
            bool onRangeRing = (ringR > 0)
                && std::max(std::abs(mx - ringX), std::abs(my - ringY)) == ringR;

            // Unified draw: ASCII uses mvaddch/chtype; emoji uses mvprintw with UTF-8.
            // All subsequent positions use absolute mv* coords so ncurses' internal
            // cursor model (which counts bytes, not columns) doesn't accumulate.
            auto drawAt = [&](int y, int x, chtype dch, const char* estr) {
                if (displayMode == DM_ASCII) mvaddch(y, x, dch);
                else                         mvprintw(y, x, "%s", estr);
            };

            if (isCur) {
                attron(COLOR_PAIR(CP_CURSOR));
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(CP_CURSOR));
            } else if (onBoxBorder) {
                // Vivid selection-box border that pops on any terrain.
                attron(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(COLOR_PAIR(CP_SUN)|A_BOLD|A_REVERSE);
            } else if (onRangeRing && !ent) {
                // Subtle range-ring marker on empty tiles only.
                attron(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
                drawAt(scY, scX, '.', "\xc2\xb7");  // · U+00B7
                attroff(COLOR_PAIR(CP_UI_HIGH)|A_DIM);
            } else {
                int attr = COLOR_PAIR(cp);
                if (ent && ent->alive) attr |= A_BOLD;
                // Selection highlight: A_REVERSE swaps owner bg ↔ fg so the
                // player/enemy colour becomes the cell foreground — distinct
                // from the ownership background on surrounding tiles.
                if (isSel) attr |= A_REVERSE;
                attron(attr);
                drawAt(scY, scX, drawCh, emojiStr);
                attroff(attr);
            }

        }
    }

    // Weather overlay: very gentle pulse — ~1.5 Hz, never overlays units/buildings.
    if (g.weather != W_CLEAR && (g.tick % 8) == 0) {
        bool snowWeather = (g.weather == W_SNOW);
        int density = (g.weather == W_STORM) ? 2 : 1; // percent — very sparse
        int frame = g.tick;
        for (int sy = 0; sy < view.viewH; sy++) for (int sx = 0; sx < view.viewW; sx++) {
            int mx = view.viewX + sx, my = view.viewY + sy;
            const TileRenderInfo* tileInfo = tileInfoAt(mx, my);
            if (!inBounds(mx,my) || !tileInfo || !tileInfo->visible) continue;
            if (entityAt(g, world, mx, my) || corpseAt(g, world, mx, my)) continue; // don't paint over units/buildings/corpses
            unsigned h = ((unsigned)(mx*73856093u) ^ (unsigned)(my*19349663u) ^ (unsigned)(frame*83492791u));
            if ((int)(h % 100) >= density) continue;
            if (snowWeather) {
                // Transparent-bg white glyph: flake adopts whatever terrain colour is beneath it.
                attron(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
                if (displayMode == DM_ASCII) mvaddch(sy+2, sx, '*');
                else                         mvprintw(sy+2, sx, "\xe2\x9c\xa6"); // ✦
                attroff(COLOR_PAIR(CP_SNOW_FALL)|A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_RAIN)|A_BOLD);
                if (displayMode == DM_ASCII) mvaddch(sy+2, sx, '.');
                else                         mvprintw(sy+2, sx, "\xc2\xb7");     // ·
                attroff(COLOR_PAIR(CP_RAIN)|A_BOLD);
            }
        }
    }
}

// ============================================================
// UI RENDER
