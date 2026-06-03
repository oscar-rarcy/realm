#include "render/sdl/sdl_terminal.h"
#include "realm.h"
#include "core/world_index.h"
#include "view_state.h"

Color termBg() { return rgb(3, 5, 8); }
Color termFg() { return rgb(218, 224, 218); }
Color termDim() { return rgb(128, 143, 150); }
Color termBar() { return rgb(12, 32, 58); }
Color termHigh() { return rgb(255, 230, 120); }
Color termAccent() { return rgb(145, 220, 245); }

float terminalZoomScale() {
    return std::max(0.58f, std::min(1.85f, s.tile / 24.0f));
}

void terminalCellMetrics(int& cellW, int& cellH) {
    int w = 0, h = 0;
    if (s.mono) TTF_SizeText(s.mono, "M", &w, &h);
    cellW = std::max(8, w);
    cellH = std::max(16, s.mono ? TTF_FontLineSkip(s.mono) : h);
}

void terminalMapCellMetrics(int& cellW, int& cellH) {
    terminalCellMetrics(cellW, cellH);
    float zoom = terminalZoomScale();
    cellW = std::max(5, (int)std::lround(cellW * zoom));
    cellH = std::max(9, (int)std::lround(cellH * zoom));
}

SDL_Rect terminalMapPixelRect(const TerminalFrame& frame) {
    int panelW = 24;
    int panelX = frame.cols - panelW;
    int mapCols = panelX >= 1 ? panelX - 1 : frame.cols;
    int topRows = 2;
    int bottomRows = 2;
    return SDL_Rect{0, topRows * frame.cellH,
                    std::max(1, mapCols * frame.cellW),
                    std::max(1, (frame.rows - topRows - bottomRows) * frame.cellH)};
}

TerminalFrame makeBlankTerminalFrame() {
    int cellW = 9, cellH = 18;
    terminalCellMetrics(cellW, cellH);
    int cols = std::max(80, s.winW / std::max(1, cellW));
    int rows = std::max(24, s.winH / std::max(1, cellH));
    TerminalCell base{' ', termFg(), termBg()};
    TerminalFrame frame;
    frame.cols = cols;
    frame.rows = rows;
    frame.cellW = cellW;
    frame.cellH = cellH;
    frame.cells.assign((size_t)cols * (size_t)rows, base);
    return frame;
}

void termPut(TerminalFrame& frame, int x, int y, char ch, Color fg, Color bg) {
    if (x < 0 || y < 0 || x >= frame.cols || y >= frame.rows) return;
    frame.at(x, y) = TerminalCell{ch, fg, bg};
}

void termPutString(TerminalFrame& frame, int x, int y, const std::string& text,
                          Color fg, Color bg) {
    if (y < 0 || y >= frame.rows) return;
    for (size_t i = 0; i < text.size() && x + (int)i < frame.cols; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 126) ch = '?';
        termPut(frame, x + (int)i, y, (char)ch, fg, bg);
    }
}

void termFillH(TerminalFrame& frame, int y, int x, int count, char ch, Color fg, Color bg) {
    for (int i = 0; i < count; ++i) termPut(frame, x + i, y, ch, fg, bg);
}

void termFillV(TerminalFrame& frame, int x, int y, int count, char ch, Color fg, Color bg) {
    for (int i = 0; i < count; ++i) termPut(frame, x, y + i, ch, fg, bg);
}

std::string termTrunc(const std::string& value, int width) {
    if (width <= 0) return "";
    if ((int)value.size() <= width) return value;
    if (width == 1) return value.substr(0, 1);
    return value.substr(0, (size_t)width - 1) + "~";
}

Color ownerTermFg(int owner) {
    if (owner == 0) return rgb(160, 210, 255);
    if (owner > 0 && owner < MAX_PLAYERS) return rgb(255, 150, 120);
    return rgb(220, 220, 190);
}

void clampTerminalView() {
    view.viewX = std::max(0, std::min(view.viewX, MAP_W - view.viewW));
    view.viewY = std::max(0, std::min(view.viewY, MAP_H - view.viewH));
}

void updateTerminalCamera(int cols, int rows, bool keepCursor) {
    int panelW = 24;
    int uiCellW = 9, uiCellH = 18;
    int mapCellW = 9, mapCellH = 18;
    terminalCellMetrics(uiCellW, uiCellH);
    terminalMapCellMetrics(mapCellW, mapCellH);
    int mapCols = cols - panelW - 1;
    if (mapCols < 30) mapCols = cols;
    int mapRows = rows - 4;
    if (mapRows < 10) mapRows = rows - 2;
    int mapPixelW = std::max(1, mapCols * uiCellW);
    int mapPixelH = std::max(1, mapRows * uiCellH);
    view.viewW = std::max(1, mapPixelW / std::max(1, mapCellW));
    view.viewH = std::max(1, mapPixelH / std::max(1, mapCellH));
    view.viewW = std::min(view.viewW, MAP_W);
    view.viewH = std::min(view.viewH, MAP_H);
    if (keepCursor) {
        if (view.cursorX < view.viewX + 3) view.viewX = view.cursorX - 3;
        if (view.cursorX > view.viewX + view.viewW - 4) view.viewX = view.cursorX - view.viewW + 4;
        if (view.cursorY < view.viewY + 3) view.viewY = view.cursorY - 3;
        if (view.cursorY > view.viewY + view.viewH - 3) view.viewY = view.cursorY - view.viewH + 3;
    }
    clampTerminalView();
}

TerminalCell terminalMapCell(const WorldIndex& world, int mx, int my) {
    const Tile& tile = g.map[my][mx];
    TerminalCell cell{' ', termDim(), termBg()};
    if (!tile.explored[0]) return cell;

    cell.ch = tile.visible[0] ? terrainAsciiGlyph(tile.terrain) : '.';
    cell.fg = tile.visible[0] ? glyphColorForTerrain(tile, mx, my) : rgb(95, 95, 105);
    cell.bg = tile.visible[0] ? scale(terrainBg(tile, mx, my), 0.35f) : rgb(8, 9, 12);

    Entity* ent = tile.visible[0] ? renderEntityAt(g, world, mx, my) : nullptr;
    if (ent && ent->alive) {
        cell.ch = STATS[ent->type].glyph;
        cell.fg = ownerTermFg(ent->owner);
        if (ent->owner != OWNER_NATURE) cell.bg = scale(ownerBg(ent->owner), 0.55f);
    }

    for (const auto& m : ui.actionMarkers) {
        if (m.x == mx && m.y == my && m.ticks > 0 && (g.tick % 6) < 4) {
            cell.ch = m.glyph;
            cell.fg = termHigh();
            break;
        }
    }

    if (s.leftDown && view.dragging) {
        int x0 = std::min(s.dragStartX, view.cursorX);
        int x1 = std::max(s.dragStartX, view.cursorX);
        int y0 = std::min(s.dragStartY, view.cursorY);
        int y1 = std::max(s.dragStartY, view.cursorY);
        if (mx >= x0 && mx <= x1 && my >= y0 && my <= y1) {
            cell.bg = blend(cell.bg, rgb(255, 255, 255), 0.24f);
            cell.fg = blend(cell.fg, rgb(255, 255, 255), 0.30f);
        }
    }

    bool selected = ent && (ent->id == g.local.selectedId ||
        std::find(g.local.selectedIds.begin(), g.local.selectedIds.end(), ent->id) != g.local.selectedIds.end());
    if (selected) {
        cell.fg = rgb(10, 10, 12);
        cell.bg = rgb(240, 240, 230);
    }
    if (mx == view.cursorX && my == view.cursorY) {
        cell.fg = rgb(20, 16, 0);
        cell.bg = rgb(255, 226, 95);
    }
    return cell;
}

void terminalDrawTop(TerminalFrame& frame) {
    termFillH(frame, 0, 0, frame.cols, ' ', termFg(), termBar());
    Player& p = g.players[0];
    int idleCount = 0, idleBldg = 0, popForecast = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != 0) continue;
        if (e.type == E_PEASANT && e.state == S_IDLE) idleCount++;
        if (isBuilding(e.type) && !e.underConstruction) {
            bool producer = (e.type == E_TOWNHALL || e.type == E_BARRACKS || e.type == E_STABLE || e.type == E_DOCK);
            if (producer && e.producing == E_NONE && e.queue.empty()) idleBldg++;
            if (e.producing != E_NONE) popForecast += STATS[e.producing].supplyUsed;
            for (int qt : e.queue) popForecast += STATS[(EntityType)qt].supplyUsed;
        }
    }
    std::ostringstream ss;
    ss << " REALM  Gold:" << p.gold << " Wood:" << p.wood << " Food:" << p.food
       << " Pop:" << p.supply << "/" << p.supplyMax << "(+" << popForecast << ")"
       << " Idle:" << idleCount << "/" << idleBldg;
    termPutString(frame, 0, 0, termTrunc(ss.str(), frame.cols), termFg(), termBar());

    std::string weather = (g.weather == W_STORM) ? "Storm" : (g.weather == W_RAIN) ? "Rain" :
                          (g.weather == W_SNOW) ? "Snow" : "Clear";
    std::ostringstream right;
    right << (getBrightness(g) > 0.5f ? "*" : "o") << " " << timeNameSafe()
          << " " << seasonNameSafe() << " " << weather;
    int rx = std::max(0, frame.cols - (int)right.str().size() - 1);
    termPutString(frame, rx, 0, right.str(), termFg(), termBar());
}

void terminalDrawTerrainBar(TerminalFrame& frame) {
    int w = std::max(0, std::min(view.viewW, frame.cols));
    termFillH(frame, 1, 0, w, '-', termDim(), termBg());
    if (!inBounds(view.cursorX, view.cursorY) || !g.map[view.cursorY][view.cursorX].explored[0]) return;
    const Tile& ct = g.map[view.cursorY][view.cursorX];
    std::ostringstream ss;
    ss << terrainName(ct.terrain) << " [" << biomeName(ct.biome) << "]";
    if (ct.resources > 0) ss << " Res:" << ct.resources;
    termPutString(frame, 1, 1, termTrunc(ss.str(), std::max(0, w - 2)), termFg(), termBg());
}

[[maybe_unused]] static void terminalDrawMap(TerminalFrame& frame, const WorldIndex& world) {
    for (int sy = 0; sy < view.viewH; ++sy) {
        int my = view.viewY + sy;
        int row = sy + 2;
        if (row < 0 || row >= frame.rows - 2) continue;
        for (int sx = 0; sx < view.viewW; ++sx) {
            int mx = view.viewX + sx;
            if (!inBounds(mx, my) || sx >= frame.cols) continue;
            TerminalCell cell = terminalMapCell(world, mx, my);
            termPut(frame, sx, row, cell.ch, cell.fg, cell.bg);
        }
    }
}

void terminalDrawMinimap(TerminalFrame& frame, const WorldIndex& world, int panelX, int panelW) {
    termPutString(frame, panelX + 1, 0, "Map", termAccent(), termBar());
    int mmW = panelW - 2;
    int mmH = std::min(view.viewH / 3, 14);
    int mmY = 1;
    for (int my = 0; my < mmH; ++my) {
        for (int mx = 0; mx < mmW; ++mx) {
            int mapX = mx * MAP_W / std::max(1, mmW);
            int mapY = my * MAP_H / std::max(1, mmH);
            char ch = ' ';
            Color fg = termDim();
            if (g.map[mapY][mapX].explored[0]) {
                Terrain t = g.map[mapY][mapX].terrain;
                ch = terrainAsciiGlyph(t);
                if (t == T_WATER || t == T_SHALLOWS) { fg = rgb(90, 150, 220); }
                else if (t == T_MOUNTAIN || t == T_STONE) { fg = rgb(150, 150, 150); }
                else if (t == T_GOLD) { fg = rgb(235, 210, 70); }
                else if (t == T_CASTLE_WALL || t == T_CASTLE_GATE) { fg = rgb(190, 190, 190); }
                else { fg = rgb(90, 135, 90); }
            }
            if (g.map[mapY][mapX].visible[0]) {
                Entity* ent = renderEntityAt(g, world, mapX, mapY);
                if (ent && ent->alive) {
                    ch = isBuilding(ent->type) ? '#' : '*';
                    fg = ownerTermFg(ent->owner);
                }
            }
            termPut(frame, panelX + 1 + mx, mmY + my, ch, fg, termBg());
        }
    }
}

void terminalDrawSelection(TerminalFrame& frame, const WorldIndex& world, int panelX, int panelW, int startY) {
    int y = startY;
    auto line = [&](const std::string& text, Color fg = termFg()) {
        if (y >= frame.rows - 2) return;
        termPutString(frame, panelX + 1, y++, termTrunc(text, panelW - 2), fg, termBg());
    };

    if (inBounds(view.cursorX, view.cursorY)) {
        const Tile& ct = g.map[view.cursorY][view.cursorX];
        line(std::string("Tile: ") + terrainName(ct.terrain));
        line(std::string("Biome: ") + biomeName(ct.biome));
        if (ct.resources > 0) {
            std::ostringstream res; res << "Resource: " << ct.resources;
            line(res.str(), termHigh());
        }
        int stack = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (view.cursorX >= e.x && view.cursorX < e.x + st.sizeW && view.cursorY >= e.y && view.cursorY < e.y + st.sizeH)
                : (view.cursorX == e.x && view.cursorY == e.y);
            if (!covers) continue;
            if (stack == 0) line("Stack:");
            if (stack < 3) line(std::string(" ") + st.name);
            stack++;
        }
        if (stack == 0) line("Stack: empty", termDim());
        else if (stack > 3) {
            std::ostringstream more; more << "+" << (stack - 3) << " more";
            line(more.str());
        }
        if (g.local.diagnostics) {
            std::ostringstream ds; ds << "Diag T" << g.tick << " M:" << modeName(g.mode);
            line(ds.str(), termHigh());
            std::ostringstream ds2; ds2 << "Ent:" << g.entities.size() << " Proj:" << g.projectiles.size();
            line(ds2.str(), termHigh());
            std::ostringstream ds3; ds3 << "Seed:" << g.seed << " AI:" << g.startupAIs;
            line(ds3.str(), termHigh());
        }
        y++;
    }

    if (g.local.selectedIds.size() > 1) {
        std::ostringstream gs; gs << "Group: " << g.local.selectedIds.size() << " units";
        line(gs.str(), termHigh());
        line("[Enter] Move/Attack", termAccent());
        line("[G] Assign to group", termAccent());
        line("[A] Select all mil.", termAccent());
        line("[1-9] Groups", termAccent());
        return;
    }

    Entity* sel = renderFindEntity(g, world, g.local.selectedId);
    if (!sel) {
        line("No selection", termDim());
        y++;
        line("-- Legend (ASCII) --", termDim());
        line("$ Gold   T Oak");
        line("^ Mtn    Y Pine");
        line("~ Water  n Hills");
        line(": Berry  % Wheat");
        line("# Castle & Ruins");
        y++;
        line("p Peasant  m Militia", ownerTermFg(0));
        line("a Archer   k Knight", ownerTermFg(0));
        line("c Catapult", ownerTermFg(0));
        y++;
        line("d Deer  s Sheep", ownerTermFg(OWNER_NATURE));
        line("w Wolf  o Boar", ownerTermFg(OWNER_NATURE));
        return;
    }

    auto& st = STATS[sel->type];
    line(st.name, ownerTermFg(sel->owner));
    int barW = panelW - 4;
    int filled = sel->hp * barW / std::max(1, sel->maxHp);
    std::string hp = "HP";
    hp.append((size_t)std::max(0, filled), '|');
    hp.append((size_t)std::max(0, barW - filled), '-');
    line(hp);
    std::ostringstream hpNum; hpNum << sel->hp << " / " << sel->maxHp;
    line(hpNum.str());
    if (isUnit(sel->type)) {
        std::ostringstream stats; stats << "ATK " << st.atk << "  RNG " << st.range;
        line(stats.str());
        line(stateName(sel->state), termAccent());
        if (sel->cargo.amount > 0) {
            std::ostringstream cargo; cargo << "Carrying: " << sel->cargo.amount << " " << cargoResourceName(sel->cargo.type);
            line(cargo.str(), termHigh());
        }
    }
    if (sel->producing != E_NONE) {
        int pct = sel->trainProgress * 100 / std::max(1, sel->trainTime);
        line(std::string("Training: ") + STATS[sel->producing].name, termHigh());
        std::ostringstream pctLine; pctLine << pct << "%";
        line(pctLine.str(), termHigh());
    }
    if (!sel->queue.empty()) {
        std::ostringstream q; q << "Queue: " << sel->queue.size();
        line(q.str(), termDim());
    }
    if (sel->underConstruction) {
        std::ostringstream b; b << "Building: " << (sel->hp * 100 / std::max(1, sel->maxHp)) << "%";
        line(b.str(), termHigh());
    }
    y++;
    if (sel->owner == 0) {
        if (sel->type == E_PEASANT) {
            line("[B] Build", termAccent());
            line("[Enter] Move/Gather", termAccent());
        } else if (isUnit(sel->type)) {
            line("[Enter] Move/Attack", termAccent());
        } else if (isBuilding(sel->type) && !sel->underConstruction) {
            if (isTrainProducer(sel->type)) line("[T] Train", termAccent());
            if (sel->type == E_MARKET) line("[R] Trade", termAccent());
            if (sel->type == E_BLACKSMITH) line("[R] Research", termAccent());
            if (sel->type == E_FARM) {
                line("Generates food", termAccent());
                std::ostringstream ripe; ripe << "Ripe: " << sel->storedFood << " / 20";
                line(ripe.str(), termAccent());
            }
        }
    }
}

void terminalDrawPanel(TerminalFrame& frame, const WorldIndex& world) {
    int panelW = 24;
    int panelX = frame.cols - panelW;
    if (panelX < 1) return;
    termFillV(frame, panelX - 1, 0, frame.rows, '|', termDim(), termBg());
    terminalDrawMinimap(frame, world, panelX, panelW);
    int mmH = std::min(view.viewH / 3, 14);
    int y = 1 + mmH + 1;
    termFillH(frame, y - 1, panelX, panelW, '-', termDim(), termBg());
    terminalDrawSelection(frame, world, panelX, panelW, y);
}

void terminalDrawBottom(TerminalFrame& frame, const WorldIndex& world) {
    int botY2 = frame.rows - 2;
    int botY1 = frame.rows - 1;
    termFillH(frame, botY2, 0, frame.cols, ' ', termFg(), termBar());
    termFillH(frame, botY1, 0, frame.cols, ' ', termFg(), termBar());
    std::string line;
    if (g.mode == M_BUILD_SELECT)
        line = " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ";
    else if (g.mode == M_BUILD_PLACE) {
        const char* name = (g.local.buildPending != E_NONE) ? STATS[g.local.buildPending].name : "building";
        line = std::string(" PLACE ") + name + ": Arrows/Mouse, [Enter]/Click build, [Esc]/RClick cancel ";
    }
    else if (g.mode == M_TRAIN_SELECT) {
        line = trainPromptFor(renderFindEntity(g, world, g.local.selectedId));
    }
    else if (g.mode == M_MARKET_TRADE)
        line = " MARKET: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g  [Esc] ";
    else if (g.mode == M_PAUSED)
        line = " PAUSED - Press [P] to resume ";
    else if (g.mode == M_GAME_OVER)
        line = (g.winner == 0) ? " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit "
                               : " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ";
    else if (g.local.groupAssignPending)
        line = " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel ";
    else if (g.mode == M_PATROL_SET)
        line = " PATROL: Move cursor + Enter or click target. [Esc] cancel ";
    else
        line = " Arrows:Move  Spc:Select  Enter:Cmd  Shift+RClick:Waypoint Z:Patrol  B:Build T:Train ?:Help V:Save Q:Resign ";
    termPutString(frame, 1, botY2, termTrunc(line, frame.cols - 2), termFg(), termBar());
    if (ui.statusTimer > 0) {
        termPutString(frame, 1, botY1, termTrunc(">> " + ui.statusMsg, frame.cols - 14), termHigh(), termBar());
    }
    std::ostringstream pos; pos << "(" << view.cursorX << "," << view.cursorY << ")";
    termPutString(frame, std::max(0, frame.cols - (int)pos.str().size() - 1), botY1, pos.str(), termDim(), termBar());
}

void registerTerminalKeyTokens(const TerminalFrame& frame, const WorldIndex& world) {
    int y = frame.rows - 2;
    std::string line;
    std::vector<std::pair<std::string, int>> tokens;
    if (g.mode == M_BUILD_SELECT) {
        line = " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ";
        tokens = terminalBuildTokens();
    } else if (g.mode == M_BUILD_PLACE) {
        const char* name = (g.local.buildPending != E_NONE) ? STATS[g.local.buildPending].name : "building";
        line = std::string(" PLACE ") + name + ": [Enter] build  [Esc] cancel ";
        tokens = {{"[Enter]", '\n'}, {"[Esc]", 27}};
    } else if (g.mode == M_TRAIN_SELECT) {
        Entity* sel = renderFindEntity(g, world, g.local.selectedId);
        line = trainPromptFor(sel);
        tokens = sel ? trainOptionTokensFor(sel->type) : std::vector<std::pair<std::string, int>>{{"Esc", 27}};
    } else if (g.mode == M_MARKET_TRADE) {
        line = " MARKET: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g  [Esc] ";
        tokens = {{"[G]", 'g'}, {"[W]", 'w'}, {"[F]", 'f'}, {"[V]", 'v'}, {"[Esc]", 27}};
    } else if (g.mode == M_PAUSED) {
        line = " PAUSED - Press [P] to resume ";
        tokens = {{"[P]", 'p'}};
    } else if (g.mode == M_GAME_OVER) {
        line = (g.winner == 0) ? " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit "
                               : " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ";
        tokens = {{"Enter", '\n'}, {"Q", 'q'}, {"[X]", 'x'}};
    } else if (g.mode == M_PATROL_SET) {
        line = " PATROL: [Enter] set target  [Esc] cancel ";
        tokens = {{"[Enter]", '\n'}, {"[Esc]", 27}};
    } else {
        line = " Arrows:Move  Spc:Select  Enter:Cmd  Z:Patrol B:Build T:Train ?:Help D:Diag V:Save L:Load Q:Resign X:Hold ";
        tokens = {{"B:Build", 'b'}, {"T:Train", 't'}, {"?:Help", '?'}, {"D:Diag", 'd'},
                  {"V:Save", 'v'}, {"L:Load", 'l'}, {"Q:Resign", 'q'}, {"X:Hold", 'x'}, {"Z:Patrol", 'z'}};
    }
    size_t searchFrom = 0;
    for (const auto& token : tokens) {
        size_t pos = line.find(token.first, searchFrom);
        if (pos == std::string::npos) pos = line.find(token.first);
        if (pos == std::string::npos || (int)pos >= frame.cols - 1) continue;
        int cells = std::min((int)token.first.size(), std::max(1, frame.cols - 1 - (int)pos));
        SDL_Rect r{(int)pos * frame.cellW, y * frame.cellH, cells * frame.cellW, frame.cellH};
        registerKeyHit(r, token.second);
        drawHoverMark(r, termHigh());
        searchFrom = pos + token.first.size();
    }
}

void terminalDrawHelpOverlay(TerminalFrame& frame) {
    if (!g.local.helpOverlay) return;
    int w = std::min(frame.cols - 4, 78);
    int h = std::min(frame.rows - 4, 24);
    if (w < 20 || h < 8) return;
    int x = std::max(1, (frame.cols - w) / 2);
    int y = std::max(1, (frame.rows - h) / 2);
    Color fg = termFg();
    Color bg = rgb(6, 8, 11);
    for (int yy = 0; yy < h; ++yy) {
        termFillH(frame, y + yy, x, w, ' ', fg, bg);
    }
    termFillH(frame, y, x, w, '-', termDim(), bg);
    termFillH(frame, y + h - 1, x, w, '-', termDim(), bg);
    termFillV(frame, x, y, h, '|', termDim(), bg);
    termFillV(frame, x + w - 1, y, h, '|', termDim(), bg);
    termPut(frame, x, y, '+', termDim(), bg);
    termPut(frame, x + w - 1, y, '+', termDim(), bg);
    termPut(frame, x, y + h - 1, '+', termDim(), bg);
    termPut(frame, x + w - 1, y + h - 1, '+', termDim(), bg);

    int row = y + 1;
    termPutString(frame, x + 2, row++, "Help", termHigh(), bg);
    int n = 0;
    const CommandHelpBinding* commands = gameplayHelpBindings(n);
    for (int i = 0; i < n && row < y + h - 5; ++i) {
        std::ostringstream line;
        line << commands[i].keys << "  " << commands[i].label << " - " << commands[i].help;
        termPutString(frame, x + 2, row++, termTrunc(line.str(), w - 4), fg, bg);
    }
    if (row < y + h - 4) {
        termPutString(frame, x + 2, row++, "Food: berries, hunting, farms, wheat, and fishing feed your stockpile.", termDim(), bg);
    }
    if (row < y + h - 3) {
        termPutString(frame, x + 2, row++, "Winter drains food from living units; starvation damages units.", termDim(), bg);
    }
    termPutString(frame, x + 2, y + h - 2, "Press ? to close", termHigh(), bg);
}

TerminalFrame buildAsciiTerminalFrame(const WorldIndex& world) {
    TerminalFrame frame = makeBlankTerminalFrame();
    updateTerminalCamera(frame.cols, frame.rows, !s.middleDown);
    terminalDrawTop(frame);
    terminalDrawTerrainBar(frame);
    terminalDrawPanel(frame, world);
    terminalDrawBottom(frame, world);
    terminalDrawHelpOverlay(frame);
    return frame;
}

void drawTerminalCellAt(const SDL_Rect& r, const TerminalCell& cell, TTF_Font* font) {
    setDraw(cell.bg);
    SDL_RenderFillRect(s.ren, &r);
    if (cell.ch == ' ') return;
    std::string text(1, cell.ch);
    SDL_Texture* tex = cachedText(font ? font : s.mono, text, cell.fg);
    if (!tex) return;
    int w = 0, h = 0;
    SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
    if (w <= 0 || h <= 0) return;
    float scale = std::min(r.w / (float)w, r.h / (float)h);
    int dw = std::max(1, (int)std::lround(w * scale));
    int dh = std::max(1, (int)std::lround(h * scale));
    SDL_Rect dst{r.x + (r.w - dw) / 2, r.y + (r.h - dh) / 2, dw, dh};
    SDL_SetTextureColorMod(tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(tex, cell.fg.a);
    SDL_RenderCopy(s.ren, tex, nullptr, &dst);
}

void drawAsciiTerminalMap(const TerminalFrame& frame, const WorldIndex& world) {
    int mapCellW = 9, mapCellH = 18;
    terminalMapCellMetrics(mapCellW, mapCellH);
    SDL_Rect mr = terminalMapPixelRect(frame);
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &mr);
    SDL_RenderSetClipRect(s.ren, &mr);
    for (int sy = 0; sy < view.viewH; ++sy) {
        int my = view.viewY + sy;
        for (int sx = 0; sx < view.viewW; ++sx) {
            int mx = view.viewX + sx;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx * mapCellW, mr.y + sy * mapCellH, mapCellW, mapCellH};
            drawTerminalCellAt(r, terminalMapCell(world, mx, my), s.mono);
        }
    }
    SDL_RenderSetClipRect(s.ren, nullptr);
}

void drawAsciiTerminalFrame(const WorldIndex& world, bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    TerminalFrame frame = buildAsciiTerminalFrame(world);
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    for (int y = 0; y < frame.rows; ++y) {
        for (int x = 0; x < frame.cols; ++x) {
            const TerminalCell& cell = frame.at(x, y);
            SDL_Rect r{x * frame.cellW, y * frame.cellH, frame.cellW, frame.cellH};
            drawTerminalCellAt(r, cell, s.mono);
        }
    }
    drawAsciiTerminalMap(frame, world);
    registerTerminalKeyTokens(frame, world);
    if (present) SDL_RenderPresent(s.ren);
}

void updateAsciiMobileCamera(int cols, int rows) {
    view.viewW = std::max(1, std::min(cols, MAP_W));
    view.viewH = std::max(1, std::min(rows, MAP_H));
    view.viewX = view.cursorX - view.viewW / 2;
    view.viewY = view.cursorY - view.viewH / 2;
    view.viewX = std::max(0, std::min(view.viewX, MAP_W - view.viewW));
    view.viewY = std::max(0, std::min(view.viewY, MAP_H - view.viewH));
}

void drawAsciiMobileMap(const WorldIndex& world) {
    SDL_Rect mr = mapRect();
    int cellW = 8, cellH = 15;
    asciiMobileCellMetrics(cellW, cellH);
    int cols = std::max(1, std::min(MAP_W, mr.w / std::max(1, cellW)));
    int rows = std::max(1, std::min(MAP_H, mr.h / std::max(1, cellH)));
    updateAsciiMobileCamera(cols, rows);

    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &mr);
    SDL_RenderSetClipRect(s.ren, &mr);
    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    for (int sy = 0; sy < view.viewH; ++sy) {
        int my = view.viewY + sy;
        for (int sx = 0; sx < view.viewW; ++sx) {
            int mx = view.viewX + sx;
            if (!inBounds(mx, my)) continue;
            SDL_Rect r{mr.x + sx * cellW, mr.y + sy * cellH, cellW, cellH};
            drawTerminalCellAt(r, terminalMapCell(world, mx, my), font);
        }
    }
    SDL_RenderSetClipRect(s.ren, nullptr);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &mr);
}

void drawAsciiMobileMiniMapText(const WorldIndex& world, SDL_Rect r) {
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &r);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &r);

    TTF_Font* font = s.monoSmall ? s.monoSmall : s.mono;
    int cellW = 8, cellH = 15;
    if (font) {
        int w = 0, h = 0;
        TTF_SizeText(font, "M", &w, &h);
        cellW = std::max(7, w);
        cellH = std::max(13, TTF_FontLineSkip(font));
    }
    int cols = std::max(1, (r.w - 6) / cellW);
    int rows = std::max(1, (r.h - 6) / cellH);
    int x0 = r.x + 3;
    int y0 = r.y + 3;
    for (int yy = 0; yy < rows; ++yy) {
        for (int xx = 0; xx < cols; ++xx) {
            int mx = xx * MAP_W / std::max(1, cols);
            int my = yy * MAP_H / std::max(1, rows);
            char ch = ' ';
            Color fg = termDim();
            if (g.map[my][mx].explored[0]) {
                Terrain t = g.map[my][mx].terrain;
                ch = terrainAsciiGlyph(t);
                if (t == T_WATER || t == T_SHALLOWS) { fg = rgb(90, 150, 220); }
                else if (t == T_MOUNTAIN || t == T_STONE) { fg = rgb(150, 150, 150); }
                else if (t == T_GOLD) { fg = rgb(235, 210, 70); }
                else if (t == T_CASTLE_WALL || t == T_CASTLE_GATE) { fg = rgb(190, 190, 190); }
                else { fg = rgb(90, 135, 90); }
            }
            if (g.map[my][mx].visible[0]) {
                Entity* ent = renderEntityAt(g, world, mx, my);
                if (ent && ent->alive) {
                    ch = isBuilding(ent->type) ? '#' : '*';
                    fg = ownerTermFg(ent->owner);
                }
            }
            drawText(x0 + xx * cellW, y0 + yy * cellH, std::string(1, ch), fg, font);
        }
    }

    SDL_Rect viewportRect{
        x0 + view.viewX * std::max(1, cols * cellW) / MAP_W,
        y0 + view.viewY * std::max(1, rows * cellH) / MAP_H,
        std::max(3, view.viewW * std::max(1, cols * cellW) / MAP_W),
        std::max(3, view.viewH * std::max(1, rows * cellH) / MAP_H)
    };
    setDraw(termHigh());
    SDL_RenderDrawRect(s.ren, &viewportRect);
}

void drawAsciiMobileHelpOverlay() {
    if (!g.local.helpOverlay) return;
    int pad = mobileSafePad();
    SDL_Rect r{pad, pad, std::max(1, s.winW - pad * 2), std::max(1, s.winH - pad * 2)};
    SDL_SetRenderDrawBlendMode(s.ren, SDL_BLENDMODE_BLEND);
    setDraw(rgb(3, 5, 8, 245));
    SDL_RenderFillRect(s.ren, &r);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &r);
    int x = r.x + 14;
    int y = r.y + 12;
    int w = std::max(1, r.w - 28);
    drawTextFit(x, y, "REALM HELP", termHigh(), w, s.mono); y += 28;
    drawTextFit(x, y, "Tap map: select or command. Drag map: pan.", termFg(), w, s.monoSmall ? s.monoSmall : s.mono); y += 22;
    drawTextFit(x, y, "Use terminal buttons for build, attack, rally, pause, and idle.", termFg(), w, s.monoSmall ? s.monoSmall : s.mono); y += 26;
    int n = 0;
    const CommandHelpBinding* commands = gameplayHelpBindings(n);
    for (int i = 0; i < n && y < r.y + r.h - 42; ++i) {
        std::ostringstream line;
        line << commands[i].label << " - " << commands[i].help;
        drawTextFit(x, y, trimPanelLine(line.str(), 62), termDim(), w, s.monoSmall ? s.monoSmall : s.mono);
        y += 19;
    }
    drawTextFit(x, r.y + r.h - 28, "Tap [Help] to close", termHigh(), w, s.monoSmall ? s.monoSmall : s.mono);
}

void drawAsciiMobileHud(const WorldIndex& world) {
    SDL_Rect pr = panelRect();
    int pad = mobileSafePad();
    setDraw(termBg());
    SDL_RenderFillRect(s.ren, &pr);
    setDraw(termDim());
    SDL_RenderDrawRect(s.ren, &pr);

    Player& p = g.players[0];
    std::ostringstream res;
    res << "REALM  G:" << p.gold << " W:" << p.wood << " F:" << p.food
        << " Pop:" << p.supply << "/" << p.supplyMax;
    int y = pr.y + pad;
    int textW = std::max(1, pr.w - pad * 2);
    drawTextFit(pr.x + pad, y, res.str(), termFg(), textW, s.monoSmall ? s.monoSmall : s.mono);
    y += 22;

    std::ostringstream tile;
    if (inBounds(view.cursorX, view.cursorY)) {
        const Tile& ct = g.map[view.cursorY][view.cursorX];
        tile << "Tile: " << terrainName(ct.terrain);
        if (ct.resources > 0) tile << " Res:" << ct.resources;
    } else {
        tile << "Tile: unknown";
    }
    SDL_Rect mm = miniMapRect();
    int summaryW = mobilePortrait() ? std::max(1, mm.x - (pr.x + pad) - 10) : textW;
    drawTextFit(pr.x + pad, y, termTrunc(tile.str(), 54), termDim(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    y += 20;
    drawTextFit(pr.x + pad, y, mobileSelectionSummary(world), termHigh(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    y += 20;
    if (s.mobileBuildType != E_NONE) {
        drawTextFit(pr.x + pad, y, std::string("Placing ") + STATS[s.mobileBuildType].name,
                    termAccent(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    } else if (g.mode == M_RALLY_SET || g.mode == M_ATTACK_MOVE || g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE || g.mode == M_PATROL_SET) {
        drawTextFit(pr.x + pad, y, modeName(g.mode), termAccent(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    } else if (ui.statusTimer > 0) {
        drawTextFit(pr.x + pad, y, ">> " + ui.statusMsg, termHigh(), summaryW, s.monoSmall ? s.monoSmall : s.mono);
    }

    drawAsciiMobileMiniMapText(world, mm);
    for (const MobileButton& b : mobileHudButtons(world)) {
        bool active = (b.id == "build" && (g.mode == M_BUILD_SELECT || g.mode == M_BUILD_PLACE))
                   || (b.id == "attack" && g.mode == M_ATTACK_MOVE)
                   || (b.id == "rally" && g.mode == M_RALLY_SET);
        drawConsoleButton(b, active, b.id == "cancel");
    }
}

void drawAsciiMobileFrame(const WorldIndex& world, bool present) {
    SDL_GetWindowSize(s.win, &s.winW, &s.winH);
    applyRendererOutputScale();
    s.isometric = false;
    setDraw(termBg());
    SDL_RenderClear(s.ren);
    drawAsciiMobileMap(world);
    drawAsciiMobileHud(world);
    drawAsciiMobileHelpOverlay();
    if (present) SDL_RenderPresent(s.ren);
}


