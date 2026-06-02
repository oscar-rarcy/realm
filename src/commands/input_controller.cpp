#include "realm.h"
#include "view_state.h"
#include "input_keys.h"

#include "commands/command.h"

static void dispatchBuildCommand(EntityType type, int x, int y, int endX = -1, int endY = -1) {
    Command command;
    command.selection = currentSelection();
    command.targetTile = {x, y};
    command.entityType = type;
    if (endX >= 0 && endY >= 0) {
        command.type = CommandType::BuildLine;
        command.lineEnd = {endX, endY};
    } else {
        command.type = CommandType::Build;
    }
    dispatchCommand(g, command);
}

static void dispatchTrainCommand(EntityType type) {
    Command command;
    command.type = CommandType::Train;
    command.selection = currentSelection();
    command.entityType = type;
    dispatchCommand(g, command);
}

static void dispatchTileCommand(CommandType type, int x, int y) {
    Command command;
    command.type = type;
    command.selection = currentSelection();
    command.targetTile = {x, y};
    dispatchCommand(g, command);
}

static void dispatchSimpleCommand(CommandType type) {
    Command command;
    command.type = type;
    command.selection = currentSelection();
    dispatchCommand(g, command);
}

static void dispatchSlotCommand(CommandType type, int slot) {
    Command command;
    command.type = type;
    command.selection = currentSelection();
    command.slot = slot;
    dispatchCommand(g, command);
}

static void moveCursorToSelected() {
    Entity* selected = findEntity(g.selectedId);
    if (!selected) return;
    view.cursorX = selected->x;
    view.cursorY = selected->y;
}

void handleInput(int ch) {
    if (ch == ERR) return;
    if (ch == 'q' || ch == 'Q') {
        dispatchSimpleCommand(CommandType::Resign);
        return;
    }
    if ((ch=='\n'||ch==KEY_ENTER||ch=='\r') && g.mode==M_GAME_OVER) {
        dispatchSimpleCommand(CommandType::Resign); return;
    }
    if ((ch=='p'||ch=='P') && (g.mode==M_NORMAL||g.mode==M_PAUSED)) {
        dispatchSimpleCommand(CommandType::TogglePause); return;
    }
    if (g.mode==M_PAUSED || g.mode==M_GAME_OVER) return;

    // Build mode
    if (g.mode == M_BUILD_SELECT) {
        Entity* sel = findEntity(g.selectedId);
        if (!sel || sel->type != E_PEASANT) { g.mode = M_NORMAL; return; }
        EntityType tb = E_NONE;
        switch (ch) {
        case 'h': case 'H': tb = E_HOUSE;      break;
        case 'b': case 'B': tb = E_BARRACKS;   break;
        case 's': case 'S': tb = E_STABLE;     break;
        case 't': case 'T': tb = E_TOWER;      break;
        case 'f': case 'F': tb = E_FARM;       break;
        case 'w': case 'W': {
            // Wall uses click-drag mode instead of point placement
            Entity* sel2 = findEntity(g.selectedId);
            if (sel2 && sel2->owner==0 && sel2->type==E_PEASANT) {
                g.buildPending = E_WALL;
                g.mode = M_WALL_DRAG;
                setStatus("Click and drag to draw wall line...");
            }
            return;
        }
        case 'a': case 'A': tb = E_BLACKSMITH; break;
        case 'c': case 'C': tb = E_CHURCH;     break;
        case 'm': case 'M': tb = E_MARKET;     break;
        case 'k': case 'K': tb = E_CASTLE;      break;
        case 'l': case 'L': tb = E_LUMBER_CAMP; break;
        case 'n': case 'N': tb = E_MINING_CAMP; break;
        case 'i': case 'I': tb = E_MILL;        break;
        case 'g': case 'G': tb = E_GATE;        break;
        case 'd': case 'D': tb = E_DOCK;        break;
        case 27: g.mode = M_NORMAL; return;
        default: return;
        }
        if (tb != E_NONE) { dispatchBuildCommand(tb, view.cursorX, view.cursorY); g.mode = M_NORMAL; }
        return;
    }

    // Train mode
    if (g.mode == M_TRAIN_SELECT) {
        Entity* sel = findEntity(g.selectedId);
        if (!sel) { g.mode = M_NORMAL; return; }
        EntityType tt = E_NONE;
        if      (sel->type == E_TOWNHALL) { if (ch=='p'||ch=='P') tt = E_PEASANT; }
        else if (sel->type == E_BARRACKS) {
            if (ch=='m'||ch=='M') tt = E_MILITIA;
            else if (ch=='a'||ch=='A') tt = E_ARCHER;
            else if (ch=='s'||ch=='S') tt = E_SPEARMAN;
            else if (ch=='c'||ch=='C') tt = E_CATAPULT;
            else if (ch=='r'||ch=='R') tt = E_RAM;
        }
        else if (sel->type == E_STABLE) { if (ch=='k'||ch=='K') tt = E_KNIGHT; }
        else if (sel->type == E_CASTLE) {
            if (ch=='p'||ch=='P') tt = E_PEASANT;
            else if (ch=='t'||ch=='T') tt = E_TREBUCHET;
        }
        else if (sel->type == E_DOCK)   {
            if      (ch=='b'||ch=='B') tt = E_FISHING_BOAT;
            else if (ch=='w'||ch=='W') tt = E_WARSHIP;
            else if (ch=='t'||ch=='T') tt = E_TRANSPORT;
        }
        if (tt != E_NONE) {
            dispatchTrainCommand(tt);
            // Keep train mode open so repeated unit keys queue more units. This
            // avoids P becoming Pause immediately after queueing a peasant.
            if (findEntity(g.selectedId)) g.mode = M_TRAIN_SELECT;
        }
        if (ch == 27) g.mode = M_NORMAL;
        return;
    }

    // Market resource trade.
    if (g.mode == M_MARKET_TRADE) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        Entity* market = findEntity(g.selectedId);
        if (!market || market->type != E_MARKET || market->owner != 0 || market->underConstruction) {
            g.mode = M_NORMAL; return;
        }
        auto dispatchTrade = [&](MarketTradeType type) {
            Command command;
            command.type = CommandType::MarketTrade;
            command.selection = currentSelection();
            command.marketTrade = type;
            dispatchCommand(g, command);
            g.mode = M_NORMAL;
        };
        if (ch == 'g' || ch == 'G') dispatchTrade(MarketTradeType::GoldForWood);
        else if (ch == 'w' || ch == 'W') dispatchTrade(MarketTradeType::WoodForGold);
        else if (ch == 'f' || ch == 'F') dispatchTrade(MarketTradeType::GoldForFood);
        else if (ch == 'v' || ch == 'V') dispatchTrade(MarketTradeType::FoodForGold);
        return;
    }

    // Wall drag mode
    if (g.mode == M_WALL_DRAG) {
        if (ch == 27) { g.mode = M_NORMAL; view.dragging = false; return; }
        // Cursor movement — preview updates in render
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        // Space / Enter: set start (first press) or confirm placement (second press)
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (!view.dragging) {
                view.dragging = true;
                view.wallDragX = view.cursorX; view.wallDragY = view.cursorY;
                setStatus("Wall start set — move cursor then press Space/Enter to place");
            } else {
                Entity* sel = findEntity(g.selectedId);
                if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT) {
                    dispatchBuildCommand(E_WALL, view.wallDragX, view.wallDragY, view.cursorX, view.cursorY);
                }
                view.dragging = false; g.mode = M_NORMAL;
            }
            goto clamp;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX  = view.viewX + me.x;
            int mapY  = view.viewY + mapSY;
            bool inMap = (mapSY>=0 && view.viewW>0 && me.x<view.viewW && inBounds(mapX,mapY));
            if (!inMap) goto clamp;
            view.cursorX = mapX; view.cursorY = mapY;
            if (me.bstate & BUTTON1_PRESSED) {
                view.dragging = true;
                view.wallDragX = mapX; view.wallDragY = mapY;
            } else if (me.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
                if (view.dragging || (me.bstate & BUTTON1_CLICKED)) {
                    Entity* sel = findEntity(g.selectedId);
                    if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT) {
                        dispatchBuildCommand(E_WALL, view.wallDragX, view.wallDragY, mapX, mapY);
                    }
                }
                view.dragging = false;
                g.mode = M_NORMAL;
            }
        }
        goto clamp;
    }

    // Rally point selection — next click/Enter sets the selected building's rally.
    if (g.mode == M_RALLY_SET) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commit = [](int tx, int ty) {
            dispatchTileCommand(CommandType::SetRally, tx, ty);
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(view.cursorX, view.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX = view.viewX + me.x, mapY = view.viewY + mapSY;
            if (mapSY >= 0 && view.viewW > 0 && me.x < view.viewW && inBounds(mapX, mapY)) {
                view.cursorX = mapX; view.cursorY = mapY;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(mapX, mapY);
            }
        }
        goto clamp;
    }

    // Attack-move target selection.
    if (g.mode == M_ATTACK_MOVE) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        if (ch == KEY_UP)    { view.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { view.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { view.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { view.cursorX++; goto clamp; }
        auto commit = [](int tx, int ty) {
            dispatchTileCommand(CommandType::AttackMove, tx, ty);
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(view.cursorX, view.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX = view.viewX + me.x, mapY = view.viewY + mapSY;
            if (mapSY >= 0 && me.x < view.viewW && inBounds(mapX, mapY)) {
                view.cursorX = mapX; view.cursorY = mapY;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(mapX, mapY);
            }
        }
        goto clamp;
    }

    // Research selection from the blacksmith.
    if (g.mode == M_RESEARCH_SELECT) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        Entity* bs = findEntity(g.selectedId);
        if (!bs || bs->type != E_BLACKSMITH || bs->underConstruction) {
            g.mode = M_NORMAL; return;
        }
        auto dispatchResearch = [&](ResearchId id) {
            Command command;
            command.type = CommandType::Research;
            command.selection = currentSelection();
            command.researchId = id;
            dispatchCommand(g, command);
            g.mode = M_NORMAL;
        };
        if (ch == 'i' || ch == 'I') dispatchResearch(ResearchId::IronWeapons);
        else if (ch == 'c' || ch == 'C') dispatchResearch(ResearchId::Crossbows);
        else if (ch == 'p' || ch == 'P') dispatchResearch(ResearchId::Pikes);
        else if (ch == 'w' || ch == 'W') dispatchResearch(ResearchId::Counterweight);
        else if (ch == 'h' || ch == 'H') dispatchResearch(ResearchId::PlateHelm);
        return;
    }

    switch (ch) {
    case KEY_UP:    view.cursorY--; break;
    case KEY_DOWN:  view.cursorY++; break;
    case KEY_LEFT:  view.cursorX--; break;
    case KEY_RIGHT: view.cursorX++; break;

    // Fast cursor pan — 10 tiles per press. Crossing a 140x90 map takes
    // dozens of presses otherwise. Shift+arrows on most terminals.
    case KEY_SR:     view.cursorY -= 10; break;   // Shift+Up
    case KEY_SF:     view.cursorY += 10; break;   // Shift+Down
    case KEY_SLEFT:  view.cursorX -= 10; break;
    case KEY_SRIGHT: view.cursorX += 10; break;
    case KEY_PPAGE:  view.cursorY -= 10; break;   // PgUp
    case KEY_NPAGE:  view.cursorY += 10; break;   // PgDn
    case KEY_HOME:   view.cursorX -= 10; break;
    case KEY_END:    view.cursorX += 10; break;

    case '?':
        g.helpOverlay = !g.helpOverlay;
        setStatus(g.helpOverlay ? "Help open." : "Help closed.");
        break;

    case 'd': case 'D':
        {
            Entity* sel = findEntity(g.selectedId);
            if (sel && sel->alive && sel->owner == 0 && sel->type == E_TREBUCHET) {
                dispatchSimpleCommand(CommandType::ToggleTrebuchetPacked);
            } else {
                dispatchSimpleCommand(CommandType::ToggleDiagnostics);
            }
        }
        break;

    case 'v': case 'V':
        dispatchSlotCommand(CommandType::Save, 0);
        break;

    case KEY_F(5): case KEY_F(6): case KEY_F(7): case KEY_F(8): {
        int slot = ch - KEY_F(5) + 1;
        dispatchSlotCommand(CommandType::Save, slot);
        break;
    }

    case 'l': case 'L':
        dispatchSlotCommand(CommandType::Load, 0);
        break;

    case KEY_F(9): case KEY_F(10): case KEY_F(11): case KEY_F(12): {
        int slot = ch - KEY_F(9) + 1;
        dispatchSlotCommand(CommandType::Load, slot);
        break;
    }

    case ' ': {
        dispatchTileCommand(CommandType::Select, view.cursorX, view.cursorY);
        break;
    }

    case '\n': case '\r': case KEY_ENTER: {
        dispatchCommand(g, resolveContextCommand(g, currentSelection(), {view.cursorX, view.cursorY}));
        break;
    }

    case 'b': case 'B': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->owner==0 && sel->type==E_PEASANT) {
            g.mode = M_BUILD_SELECT;
            setStatus("Select building to place at cursor...");
        } else setStatus("Select a peasant first!");
        break;
    }

    case 't': case 'T': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->owner==0 && isBuilding(sel->type) && !sel->underConstruction) {
            if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK||sel->type==E_CASTLE) {
                g.mode = M_TRAIN_SELECT;
                setStatus("Select unit to train...");
            } else setStatus("This building can't train.");
        } else setStatus("Select a production building!");
        break;
    }

    // Eject garrison from selected building or transport
    case 'U': case 'u': {
        dispatchSimpleCommand(CommandType::EjectGarrison);
        break;
    }

    // Rally point (production buildings) / research menu (blacksmith)
    case 'R': case 'r': {
        Entity* sel = findEntity(g.selectedId);
        if (!sel || sel->owner != 0 || !isBuilding(sel->type) || sel->underConstruction) {
            setStatus("Select a production building first.");
            break;
        }
        if (sel->type == E_MARKET) {
            g.mode = M_MARKET_TRADE;
            setStatus("Trade: [G] 40g->30w  [W] 40w->30g  [F] 50g->30f  [V] 40f->30g");
        } else if (sel->type == E_BLACKSMITH) {
            g.mode = M_RESEARCH_SELECT;
            setStatus("Research: [I]ron [C]rossbows [P]ikes [W]eight [H]elm  [Esc]");
        } else if (sel->type == E_TOWNHALL || sel->type == E_CASTLE
                || sel->type == E_BARRACKS || sel->type == E_STABLE || sel->type == E_DOCK) {
            g.mode = M_RALLY_SET;
            setStatus("Click a tile (or move cursor + Enter) to set rally. [Esc]");
        } else {
            setStatus("Nothing to rally or research here.");
        }
        break;
    }

    // Cycle to the next idle peasant
    case '.': case ',': {
        int sid = g.selectedId; bool past = (sid < 0); Entity* pick = nullptr;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != 0 || e.type != E_PEASANT || e.state != S_IDLE) continue;
            if (!past) { if (e.id == sid) past = true; continue; }
            pick = &e; break;
        }
        if (!pick) for (auto& e : g.entities) {
            if (!e.alive || e.owner != 0 || e.type != E_PEASANT || e.state != S_IDLE) continue;
            pick = &e; break;
        }
        if (pick) {
            g.selectedId = pick->id; g.selectedIds.clear();
            view.cursorX = pick->x; view.cursorY = pick->y;
            setStatus("Idle peasant selected");
        } else setStatus("No idle peasants");
        break;
    }

    // Gate toggle: cycle auto -> locked-open -> locked-closed -> auto
    case 'O': {
        dispatchSimpleCommand(CommandType::ToggleGate);
        break;
    }

    // Debug: reveal entire map (Shift+S)
    case 'S': {
        dispatchSimpleCommand(CommandType::RevealMapDebug);
        break;
    }

    // 'A' is overloaded:
    //   with no selection or non-military selection → select all military
    //   with military selected → enter attack-move mode (next click = a-move target)
    case 'A': {
        bool hasMilitarySel = false;
        if (!g.selectedIds.empty()) {
            for (int id : g.selectedIds) {
                Entity* e = findEntity(id);
                if (e && isMilitary(e->type))
                    { hasMilitarySel = true; break; }
            }
        } else if (g.selectedId >= 0) {
            Entity* e = findEntity(g.selectedId);
            if (e && isMilitary(e->type))
                hasMilitarySel = true;
        }
        if (hasMilitarySel) {
            g.mode = M_ATTACK_MOVE;
            setStatus("Attack-move: click destination. [Esc] cancel");
        } else {
            g.selectedIds.clear(); g.selectedId = -1;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != 0 || e.state == S_GARRISONED) continue;
                if (isMilitary(e.type)) {
                    g.selectedIds.push_back(e.id);
                    if (g.selectedId < 0) { g.selectedId=e.id; view.cursorX=e.x; view.cursorY=e.y; }
                }
            }
            if (g.selectedIds.empty()) setStatus("No military units!");
            else setStatus(std::to_string(g.selectedIds.size()) + " military units selected");
        }
        break;
    }

    // Hold position — stop and ignore auto-aggro until explicitly ordered.
    case 'X': case 'x': {
        dispatchSimpleCommand(CommandType::HoldPosition);
        break;
    }

    // Enter group assign mode
    case 'G': {
        if (g.selectedIds.empty() && g.selectedId >= 0) {
            g.selectedIds.push_back(g.selectedId);
        }
        if (!g.selectedIds.empty()) {
            g.groupAssignPending = true;
            setStatus("Press [1]-[9] to assign group...");
        }
        break;
    }

    // Control groups 1-9
    case '1': case '2': case '3': case '4': case '5':
    case '6': case '7': case '8': case '9': {
        int idx = ch - '1';
        if (g.groupAssignPending) {
            dispatchSlotCommand(CommandType::AssignControlGroup, idx);
        } else {
            dispatchSlotCommand(CommandType::RecallControlGroup, idx);
            moveCursorToSelected();
        }
        break;
    }

    // Cycle through own units
    case '\t': {
        int sid = g.selectedId; bool found = false, past = (sid < 0);
        for (auto& e : g.entities) {
            if (!e.alive || e.owner!=0 || !isUnit(e.type) || e.state==S_GARRISONED) continue;
            if (!past) { if (e.id==sid) past=true; continue; }
            g.selectedId=e.id; g.selectedIds.clear(); view.cursorX=e.x; view.cursorY=e.y; found=true; break;
        }
        if (!found) for (auto& e : g.entities) {
            if (!e.alive || e.owner!=0 || !isUnit(e.type) || e.state==S_GARRISONED) continue;
            g.selectedId=e.id; g.selectedIds.clear(); view.cursorX=e.x; view.cursorY=e.y; break;
        }
        break;
    }

    // Home to town hall
    case 'h': {
        for (auto& e : g.entities)
            if (e.alive && e.owner==0 && (e.type==E_TOWNHALL||e.type==E_CASTLE)) {
                g.selectedId=e.id; g.selectedIds.clear(); view.cursorX=e.x+1; view.cursorY=e.y+1; break;
            }
        break;
    }

    case 27: // Escape
        g.selectedId = -1;
        g.selectedIds.clear();
        g.groupAssignPending = false;
        g.mode = M_NORMAL;
        break;

    case KEY_MOUSE: {
        MEVENT me;
        if (getmouse(&me) != OK) break;
        int mapSY = me.y - 2;
        int mapX  = view.viewX + me.x;
        int mapY  = view.viewY + mapSY;
        // Minimap click → jump viewport. Minimap sits at panelX+1..+mmW, mmY..+mmH.
        {
            int maxY2, maxX2; getmaxyx(stdscr, maxY2, maxX2); (void)maxY2;
            int panelW = 24, panelX = maxX2 - panelW;
            int mmX = panelX + 1, mmY = 1, mmW = panelW - 2;
            int mmH = std::min(view.viewH/3, 14);
            if (me.x >= mmX && me.x < mmX+mmW && me.y >= mmY && me.y < mmY+mmH) {
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED
                              | BUTTON3_CLICKED | BUTTON3_PRESSED)) {
                    int mx = (me.x - mmX) * MAP_W / mmW;
                    int my = (me.y - mmY) * MAP_H / mmH;
                    view.viewX = std::max(0, std::min(mx - view.viewW/2, MAP_W - view.viewW));
                    view.viewY = std::max(0, std::min(my - view.viewH/2, MAP_H - view.viewH));
                    view.cursorX = mx; view.cursorY = my;
                    view.dragging = false;
                }
                break;
            }
        }
        bool inMap = (mapSY >= 0 && view.viewW > 0 && me.x < view.viewW && inBounds(mapX, mapY));
        if (!inMap) { view.dragging = false; break; }

        // Hover-track the cursor, but only when the mouse actually crossed into a new map
        // cell. Without this, every stale ncurses motion event would yank the cursor back
        // to the OS mouse position, fighting keyboard arrow input. Clicks/drags still pin
        // the cursor regardless of last-cell state.
        static int lastMx = -9999, lastMy = -9999;
        bool clickEvt = (me.bstate & (BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON1_CLICKED
                                    |BUTTON1_DOUBLE_CLICKED|BUTTON3_CLICKED|BUTTON3_PRESSED)) != 0;
        if (clickEvt || mapX != lastMx || mapY != lastMy) {
            view.cursorX = mapX; view.cursorY = mapY;
            lastMx = mapX; lastMy = mapY;
        }

        if (me.bstate & BUTTON1_DOUBLE_CLICKED) {
            // Select all of clicked unit type within the current viewport.
            Command command;
            command.type = CommandType::SelectAllOfTypeInView;
            command.targetTile = {mapX, mapY};
            dispatchCommand(g, command);
            view.dragging = false;
            break;
        }
        if (me.bstate & BUTTON1_PRESSED) {
            // Start of left-button drag/click
            view.dragging    = true;
            view.dragStartX  = mapX;
            view.dragStartY  = mapY;
        }
        else if (me.bstate & BUTTON1_RELEASED) {
            if (view.dragging) {
                view.dragging = false;
                bool moved = (std::abs(mapX - view.dragStartX) + std::abs(mapY - view.dragStartY)) > 1;
                if (moved) {
                    // Box select: all own units inside the rectangle
                    Command command;
                    command.type = CommandType::BoxSelect;
                    command.targetTile = {view.dragStartX, view.dragStartY};
                    command.boxEnd = {mapX, mapY};
                    dispatchCommand(g, command);
                } else {
                    // Click: select entity at cursor
                    Command command;
                    command.type = CommandType::Select;
                    command.targetTile = {mapX, mapY};
                    dispatchCommand(g, command);
                }
            }
        }
        else if (me.bstate & BUTTON1_CLICKED) {
            // Terminals that report CLICKED instead of PRESSED+RELEASED
            view.dragging = false;
            Command command;
            command.type = CommandType::Select;
            command.targetTile = {mapX, mapY};
            dispatchCommand(g, command);
        }
        else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
            // Right click: issue command at cursor position
            view.dragging = false;
            dispatchCommand(g, resolveContextCommand(g, currentSelection(), {mapX, mapY}));
        }
        // All other events (pure movement): cursor already updated above
        break;
    }
    }

    clamp:
    view.cursorX = std::max(0, std::min(view.cursorX, MAP_W-1));
    view.cursorY = std::max(0, std::min(view.cursorY, MAP_H-1));
}
