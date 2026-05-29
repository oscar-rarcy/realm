#include "realm.h"

void handleInput(int ch) {
    if (ch == ERR) return;
    if (ch == 'q' || ch == 'Q') { endwin(); exit(0); }
    if ((ch=='p'||ch=='P') && (g.mode==M_NORMAL||g.mode==M_PAUSED)) {
        g.mode = (g.mode==M_PAUSED) ? M_NORMAL : M_PAUSED; return;
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
        if (tb != E_NONE) { orderBuild(*sel, tb, g.cursorX, g.cursorY); g.mode = M_NORMAL; }
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
            else if (ch=='c'||ch=='C') tt = E_CATAPULT;
        }
        else if (sel->type == E_STABLE) { if (ch=='k'||ch=='K') tt = E_KNIGHT; }
        else if (sel->type == E_DOCK)   { if (ch=='b'||ch=='B') tt = E_FISHING_BOAT; }
        if (tt != E_NONE) { orderTrain(*sel, tt); g.mode = M_NORMAL; }
        if (ch == 27) g.mode = M_NORMAL;
        return;
    }

    // Wall drag mode
    if (g.mode == M_WALL_DRAG) {
        if (ch == 27) { g.mode = M_NORMAL; g.dragging = false; return; }
        // Cursor movement — preview updates in render
        if (ch == KEY_UP)    { g.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { g.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { g.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { g.cursorX++; goto clamp; }
        // Space / Enter: set start (first press) or confirm placement (second press)
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            if (!g.dragging) {
                g.dragging = true;
                g.wallDragX = g.cursorX; g.wallDragY = g.cursorY;
                setStatus("Wall start set — move cursor then press Space/Enter to place");
            } else {
                Entity* sel = findEntity(g.selectedId);
                if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT) {
                    int x0=g.wallDragX, y0=g.wallDragY, x1=g.cursorX, y1=g.cursorY;
                    int dx=std::abs(x1-x0), sx2=x0<x1?1:-1;
                    int dy=-std::abs(y1-y0), sy2=y0<y1?1:-1;
                    int err=dx+dy; int firstId=-1;
                    while (true) {
                        if (canPlace(E_WALL,x0,y0,0) && g.players[0].wood>=20) {
                            g.players[0].wood -= 20;
                            int wid = spawnEntity(E_WALL, 0, x0, y0, false);
                            if (firstId < 0) firstId = wid;
                        }
                        if (x0==x1 && y0==y1) break;
                        int e2=2*err;
                        if (e2>=dy){err+=dy; x0+=sx2;}
                        if (e2<=dx){err+=dx; y0+=sy2;}
                    }
                    if (firstId >= 0) { orderHelp(*sel, firstId); setStatus("Building walls..."); }
                }
                g.dragging = false; g.mode = M_NORMAL;
            }
            goto clamp;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX  = g.viewX + me.x;
            int mapY  = g.viewY + mapSY;
            bool inMap = (mapSY>=0 && g.viewW>0 && me.x<g.viewW && inBounds(mapX,mapY));
            if (!inMap) goto clamp;
            g.cursorX = mapX; g.cursorY = mapY;
            if (me.bstate & BUTTON1_PRESSED) {
                g.dragging = true;
                g.wallDragX = mapX; g.wallDragY = mapY;
            } else if (me.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
                if (g.dragging || (me.bstate & BUTTON1_CLICKED)) {
                    Entity* sel = findEntity(g.selectedId);
                    if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT) {
                        int x0=g.wallDragX, y0=g.wallDragY, x1=mapX, y1=mapY;
                        int dx=std::abs(x1-x0), sx2=x0<x1?1:-1;
                        int dy=-std::abs(y1-y0), sy2=y0<y1?1:-1;
                        int err=dx+dy; int firstId=-1;
                        while (true) {
                            if (canPlace(E_WALL,x0,y0,0) && g.players[0].wood>=20) {
                                g.players[0].wood -= 20;
                                int wid = spawnEntity(E_WALL, 0, x0, y0, false);
                                if (firstId < 0) firstId = wid;
                            }
                            if (x0==x1 && y0==y1) break;
                            int e2=2*err;
                            if (e2>=dy){err+=dy; x0+=sx2;}
                            if (e2<=dx){err+=dx; y0+=sy2;}
                        }
                        if (firstId >= 0) { orderHelp(*sel, firstId); setStatus("Building walls..."); }
                    }
                }
                g.dragging = false;
                g.mode = M_NORMAL;
            }
        }
        goto clamp;
    }

    // Rally point selection — next click/Enter sets the selected building's rally.
    if (g.mode == M_RALLY_SET) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        if (ch == KEY_UP)    { g.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { g.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { g.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { g.cursorX++; goto clamp; }
        auto commit = [](int tx, int ty) {
            Entity* sel = findEntity(g.selectedId);
            if (sel && sel->alive && sel->owner == 0) {
                sel->rallyX = tx; sel->rallyY = ty; sel->rallySet = 1;
                setStatus("Rally point set.");
            }
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(g.cursorX, g.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX = g.viewX + me.x, mapY = g.viewY + mapSY;
            if (mapSY >= 0 && g.viewW > 0 && me.x < g.viewW && inBounds(mapX, mapY)) {
                g.cursorX = mapX; g.cursorY = mapY;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(mapX, mapY);
            }
        }
        goto clamp;
    }

    // Attack-move target selection.
    if (g.mode == M_ATTACK_MOVE) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        if (ch == KEY_UP)    { g.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { g.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { g.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { g.cursorX++; goto clamp; }
        auto commit = [](int tx, int ty) {
            if (g.selectedIds.size() > 1) {
                orderGroupAttackMove(tx, ty);
            } else {
                Entity* sel = findEntity(g.selectedId);
                if (sel && sel->alive && sel->owner == 0 && isUnit(sel->type)) {
                    orderMove(*sel, tx, ty); sel->attackMove = 1;
                    setStatus("Attack-moving.");
                }
            }
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(g.cursorX, g.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int mapSY = me.y - 2;
            int mapX = g.viewX + me.x, mapY = g.viewY + mapSY;
            if (mapSY >= 0 && me.x < g.viewW && inBounds(mapX, mapY)) {
                g.cursorX = mapX; g.cursorY = mapY;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(mapX, mapY);
            }
        }
        goto clamp;
    }

    // Research selection from the blacksmith.
    if (g.mode == M_RESEARCH_SELECT) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        Player& pl = g.players[0];
        Entity* bs = findEntity(g.selectedId);
        if (!bs || bs->type != E_BLACKSMITH || bs->underConstruction) {
            g.mode = M_NORMAL; return;
        }
        auto startResearch = [&](int bit, int gold, int wood, int ticks, const char* startMsg) {
            if (pl.research & bit) { setStatus("Already researched."); return; }
            if (bs->researching != 0) { setStatus("Already researching."); return; }
            if (pl.gold < gold || pl.wood < wood) { setStatus("Not enough resources!"); return; }
            pl.gold -= gold; pl.wood -= wood;
            bs->researching = bit; bs->prodProgress = 0; bs->prodTime = ticks;
            setStatus(startMsg);
        };
        // ~75 sec at 80 ms tick = 940 ticks.
        if (ch == 'i' || ch == 'I') { startResearch(R_IRON_WEAPONS, 100, 100, 940, "Researching Iron Weapons..."); g.mode = M_NORMAL; }
        else if (ch == 'c' || ch == 'C') { startResearch(R_CROSSBOWS, 80, 80, 820, "Researching Crossbows..."); g.mode = M_NORMAL; }
        return;
    }

    switch (ch) {
    case KEY_UP:    g.cursorY--; break;
    case KEY_DOWN:  g.cursorY++; break;
    case KEY_LEFT:  g.cursorX--; break;
    case KEY_RIGHT: g.cursorX++; break;

    // Fast cursor pan — 10 tiles per press. Crossing a 140x90 map takes
    // dozens of presses otherwise. Shift+arrows on most terminals.
    case KEY_SR:     g.cursorY -= 10; break;   // Shift+Up
    case KEY_SF:     g.cursorY += 10; break;   // Shift+Down
    case KEY_SLEFT:  g.cursorX -= 10; break;
    case KEY_SRIGHT: g.cursorX += 10; break;
    case KEY_PPAGE:  g.cursorY -= 10; break;   // PgUp
    case KEY_NPAGE:  g.cursorY += 10; break;   // PgDn
    case KEY_HOME:   g.cursorX -= 10; break;
    case KEY_END:    g.cursorX += 10; break;

    case ' ': {
        Entity* ent = entityAtOwner(g.cursorX, g.cursorY, 0);
        if (ent) {
            g.selectedId = ent->id;
            g.selectedIds.clear();
            setStatus(std::string("Selected: ") + STATS[ent->type].name);
        } else {
            Entity* any = entityAt(g.cursorX, g.cursorY);
            if (any && any->alive && g.map[g.cursorY][g.cursorX].visible[0]) {
                g.selectedId = any->id;
                g.selectedIds.clear();
                setStatus(std::string(any->owner==OWNER_NATURE?"Animal: ":"Enemy ") + STATS[any->type].name);
            } else {
                g.selectedId = -1;
                g.selectedIds.clear();
            }
        }
        break;
    }

    case '\n': case '\r': case KEY_ENTER: {
        if (g.selectedIds.size() > 1) {
            // Group command
            Entity* tgt = entityAt(g.cursorX, g.cursorY);
            if (tgt && tgt->alive && tgt->owner == 0 && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
                for (int id : g.selectedIds) {
                    Entity* u = findEntity(id);
                    if (u && u->alive && u->owner == 0 && isUnit(u->type) && u->type != E_CATAPULT)
                        orderGarrison(*u, tgt->id);
                }
                setStatus("Garrisoning...");
            } else if (tgt && tgt->alive && tgt->owner != 0 && g.map[g.cursorY][g.cursorX].visible[0]) {
                orderGroupAttack(tgt->id);
            } else {
                orderGroupMove(g.cursorX, g.cursorY);
            }
        } else {
            Entity* sel = findEntity(g.selectedId);
            if (!sel || sel->owner != 0 || !isUnit(sel->type)) break;
            Entity* tgt = entityAt(g.cursorX, g.cursorY);
            if (tgt && tgt->alive && tgt->owner == 0 && tgt->underConstruction && sel->type == E_PEASANT) {
                orderHelp(*sel, tgt->id);
                setStatus("Helping build...");
            } else if (tgt && tgt->alive && tgt->owner == 0 && tgt->type == E_FARM && !tgt->underConstruction && sel->type == E_PEASANT) {
                orderHelp(*sel, tgt->id);
                setStatus("Tending farm...");
            } else if (tgt && tgt->alive && tgt->owner == 0 && !tgt->underConstruction && canGarrisonIn(tgt->type) && sel->type != E_CATAPULT) {
                orderGarrison(*sel, tgt->id);
            } else if (tgt && tgt->alive && tgt->owner != 0 && g.map[g.cursorY][g.cursorX].visible[0]) {
                orderAttack(*sel, tgt->id);
                setStatus("Attacking!");
            } else if (sel->type == E_PEASANT) {
                Terrain ter = g.map[g.cursorY][g.cursorX].terrain;
                bool isW = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
                if ((ter==T_GOLD||isW) && g.map[g.cursorY][g.cursorX].resources > 0) {
                    orderGather(*sel, g.cursorX, g.cursorY);
                    setStatus(ter==T_GOLD ? "Mining gold..." : "Chopping wood...");
                } else if (ter == T_WHEAT && !tgt && canPlace(E_FARM, g.cursorX, g.cursorY, 0)) {
                    int fid = spawnEntity(E_FARM, 0, g.cursorX, g.cursorY, true);
                    orderHelp(*sel, fid);
                    setStatus("Working wheat field...");
                } else {
                    orderMove(*sel, g.cursorX, g.cursorY);
                    setStatus("Moving...");
                }
            } else if (sel->type == E_FISHING_BOAT) {
                Terrain ter = g.map[g.cursorY][g.cursorX].terrain;
                if (ter == T_FISH && g.map[g.cursorY][g.cursorX].resources > 0) {
                    orderGather(*sel, g.cursorX, g.cursorY); setStatus("Fishing...");
                } else { orderMove(*sel, g.cursorX, g.cursorY); setStatus("Moving..."); }
            } else {
                orderMove(*sel, g.cursorX, g.cursorY);
                setStatus("Moving...");
            }
        }
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
            if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK) {
                g.mode = M_TRAIN_SELECT;
                setStatus("Select unit to train...");
            } else setStatus("This building can't train.");
        } else setStatus("Select a production building!");
        break;
    }

    // Eject garrison from selected building
    case 'U': case 'u': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->alive && sel->owner == 0 && isBuilding(sel->type) && canGarrisonIn(sel->type)) {
            int n = (int)sel->garrison.size();
            if (n > 0) { ejectGarrison(*sel); setStatus(std::to_string(n) + " unit(s) ejected"); }
            else setStatus("No garrison to eject");
        }
        break;
    }

    // Rally point (production buildings) / research menu (blacksmith)
    case 'R': case 'r': {
        Entity* sel = findEntity(g.selectedId);
        if (!sel || sel->owner != 0 || !isBuilding(sel->type) || sel->underConstruction) {
            setStatus("Select a production building first.");
            break;
        }
        if (sel->type == E_BLACKSMITH) {
            g.mode = M_RESEARCH_SELECT;
            setStatus("Research: [I]ron Weapons 100g/100w  [C]rossbows 80g/80w  [Esc]");
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
            g.cursorX = pick->x; g.cursorY = pick->y;
            setStatus("Idle peasant selected");
        } else setStatus("No idle peasants");
        break;
    }

    // Gate toggle: cycle auto / locked-open / locked-closed
    case 'O': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->alive && sel->owner==0 && sel->type==E_GATE && !sel->underConstruction) {
            if (sel->gatherType == 0) {
                sel->gatherType = 1; // enter manual lock in current state
                setStatus(sel->carrying > 0 ? "Gate locked open" : "Gate locked closed");
            } else {
                sel->carrying = (sel->carrying > 0) ? 0 : 1; // toggle state
                setStatus(sel->carrying > 0 ? "Gate locked open" : "Gate locked closed");
            }
        }
        break;
    }

    // Debug: reveal entire map (Shift+S)
    case 'S': {
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            g.map[y][x].visible[0]  = true;
            g.map[y][x].explored[0] = true;
        }
        setStatus("Debug: map revealed");
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
                if (e && (e->type==E_MILITIA||e->type==E_ARCHER||e->type==E_KNIGHT||e->type==E_CATAPULT))
                    { hasMilitarySel = true; break; }
            }
        } else if (g.selectedId >= 0) {
            Entity* e = findEntity(g.selectedId);
            if (e && (e->type==E_MILITIA||e->type==E_ARCHER||e->type==E_KNIGHT||e->type==E_CATAPULT))
                hasMilitarySel = true;
        }
        if (hasMilitarySel) {
            g.mode = M_ATTACK_MOVE;
            setStatus("Attack-move: click destination. [Esc] cancel");
        } else {
            g.selectedIds.clear(); g.selectedId = -1;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != 0 || e.state == S_GARRISONED) continue;
                if (e.type==E_MILITIA||e.type==E_ARCHER||e.type==E_KNIGHT||e.type==E_CATAPULT) {
                    g.selectedIds.push_back(e.id);
                    if (g.selectedId < 0) { g.selectedId=e.id; g.cursorX=e.x; g.cursorY=e.y; }
                }
            }
            if (g.selectedIds.empty()) setStatus("No military units!");
            else setStatus(std::to_string(g.selectedIds.size()) + " military units selected");
        }
        break;
    }

    // Hold position — stop and ignore auto-aggro until explicitly ordered.
    case 'X': case 'x': {
        auto hold = [](Entity* e) {
            if (!e || !e->alive || e->owner != 0 || !isUnit(e->type)) return;
            e->state = S_IDLE; e->path.clear(); e->pathIdx = 0;
            e->attackMove = 0; e->holdPosition = 1; e->targetId = -1;
        };
        int n = 0;
        if (!g.selectedIds.empty()) {
            for (int id : g.selectedIds) { hold(findEntity(id)); n++; }
        } else if (g.selectedId >= 0) {
            hold(findEntity(g.selectedId)); n = 1;
        }
        if (n > 0) setStatus("Hold position.");
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
            g.controlGroups[idx] = g.selectedIds;
            g.groupAssignPending  = false;
            setStatus(std::string("Group ") + (char)ch + " assigned (" +
                      std::to_string(g.selectedIds.size()) + " units)");
        } else {
            if (g.controlGroups[idx].empty()) {
                setStatus(std::string("Group ") + (char)ch + " is empty");
            } else {
                g.selectedIds = g.controlGroups[idx];
                g.selectedId  = -1;
                for (int id : g.selectedIds) {
                    Entity* e = findEntity(id);
                    if (e && e->alive) { g.selectedId=e->id; g.cursorX=e->x; g.cursorY=e->y; break; }
                }
                setStatus(std::string("Group ") + (char)ch + " recalled (" +
                          std::to_string(g.selectedIds.size()) + " units)");
            }
        }
        break;
    }

    // Cycle through own units
    case '\t': {
        int sid = g.selectedId; bool found = false, past = (sid < 0);
        for (auto& e : g.entities) {
            if (!e.alive || e.owner!=0 || !isUnit(e.type) || e.state==S_GARRISONED) continue;
            if (!past) { if (e.id==sid) past=true; continue; }
            g.selectedId=e.id; g.selectedIds.clear(); g.cursorX=e.x; g.cursorY=e.y; found=true; break;
        }
        if (!found) for (auto& e : g.entities) {
            if (!e.alive || e.owner!=0 || !isUnit(e.type) || e.state==S_GARRISONED) continue;
            g.selectedId=e.id; g.selectedIds.clear(); g.cursorX=e.x; g.cursorY=e.y; break;
        }
        break;
    }

    // Home to town hall
    case 'h': {
        for (auto& e : g.entities)
            if (e.alive && e.owner==0 && (e.type==E_TOWNHALL||e.type==E_CASTLE)) {
                g.selectedId=e.id; g.selectedIds.clear(); g.cursorX=e.x+1; g.cursorY=e.y+1; break;
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
        int mapX  = g.viewX + me.x;
        int mapY  = g.viewY + mapSY;
        // Minimap click → jump viewport. Minimap sits at panelX+1..+mmW, mmY..+mmH.
        {
            int maxY2, maxX2; getmaxyx(stdscr, maxY2, maxX2); (void)maxY2;
            int panelW = 24, panelX = maxX2 - panelW;
            int mmX = panelX + 1, mmY = 1, mmW = panelW - 2;
            int mmH = std::min(g.viewH/3, 14);
            if (me.x >= mmX && me.x < mmX+mmW && me.y >= mmY && me.y < mmY+mmH) {
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED
                              | BUTTON3_CLICKED | BUTTON3_PRESSED)) {
                    int mx = (me.x - mmX) * MAP_W / mmW;
                    int my = (me.y - mmY) * MAP_H / mmH;
                    g.viewX = std::max(0, std::min(mx - g.viewW/2, MAP_W - g.viewW));
                    g.viewY = std::max(0, std::min(my - g.viewH/2, MAP_H - g.viewH));
                    g.cursorX = mx; g.cursorY = my;
                    g.dragging = false;
                }
                break;
            }
        }
        bool inMap = (mapSY >= 0 && g.viewW > 0 && me.x < g.viewW && inBounds(mapX, mapY));
        if (!inMap) { g.dragging = false; break; }

        // Hover-track the cursor, but only when the mouse actually crossed into a new map
        // cell. Without this, every stale ncurses motion event would yank the cursor back
        // to the OS mouse position, fighting keyboard arrow input. Clicks/drags still pin
        // the cursor regardless of last-cell state.
        static int lastMx = -9999, lastMy = -9999;
        bool clickEvt = (me.bstate & (BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON1_CLICKED
                                    |BUTTON1_DOUBLE_CLICKED|BUTTON3_CLICKED|BUTTON3_PRESSED)) != 0;
        if (clickEvt || mapX != lastMx || mapY != lastMy) {
            g.cursorX = mapX; g.cursorY = mapY;
            lastMx = mapX; lastMy = mapY;
        }

        if (me.bstate & BUTTON1_DOUBLE_CLICKED) {
            // Select all of clicked unit type within the current viewport.
            Entity* ent = entityAtOwner(mapX, mapY, 0);
            if (ent && isUnit(ent->type)) {
                EntityType t = ent->type;
                g.selectedIds.clear(); g.selectedId = -1;
                for (auto& e : g.entities) {
                    if (!e.alive || e.owner != 0 || e.type != t) continue;
                    if (e.state == S_GARRISONED) continue;
                    if (e.x < g.viewX || e.x >= g.viewX+g.viewW) continue;
                    if (e.y < g.viewY || e.y >= g.viewY+g.viewH) continue;
                    g.selectedIds.push_back(e.id);
                    if (g.selectedId < 0) g.selectedId = e.id;
                }
                setStatus(std::to_string(g.selectedIds.size()) + " " + STATS[t].name + "s selected");
            }
            g.dragging = false;
            break;
        }
        if (me.bstate & BUTTON1_PRESSED) {
            // Start of left-button drag/click
            g.dragging    = true;
            g.dragStartX  = mapX;
            g.dragStartY  = mapY;
        }
        else if (me.bstate & BUTTON1_RELEASED) {
            if (g.dragging) {
                g.dragging = false;
                bool moved = (std::abs(mapX - g.dragStartX) + std::abs(mapY - g.dragStartY)) > 1;
                if (moved) {
                    // Box select: all own units inside the rectangle
                    int x0 = std::min(g.dragStartX, mapX), x1 = std::max(g.dragStartX, mapX);
                    int y0 = std::min(g.dragStartY, mapY), y1 = std::max(g.dragStartY, mapY);
                    g.selectedIds.clear(); g.selectedId = -1;
                    for (auto& e : g.entities) {
                        if (!e.alive || e.owner != 0 || !isUnit(e.type)) continue;
                        if (e.state == S_GARRISONED) continue;
                        if (e.x >= x0 && e.x <= x1 && e.y >= y0 && e.y <= y1) {
                            g.selectedIds.push_back(e.id);
                            if (g.selectedId < 0) g.selectedId = e.id;
                        }
                    }
                    if (!g.selectedIds.empty())
                        setStatus(std::to_string(g.selectedIds.size()) + " units selected");
                } else {
                    // Click: select entity at cursor
                    Entity* ent = entityAtOwner(mapX, mapY, 0);
                    if (ent) {
                        g.selectedId = ent->id; g.selectedIds.clear();
                        setStatus(std::string("Selected: ") + STATS[ent->type].name);
                    } else {
                        Entity* any = entityAt(mapX, mapY);
                        if (any && any->alive && g.map[mapY][mapX].visible[0]) {
                            g.selectedId = any->id; g.selectedIds.clear();
                            setStatus(std::string(any->owner==OWNER_NATURE?"Animal: ":"Enemy ") + STATS[any->type].name);
                        } else { g.selectedId = -1; g.selectedIds.clear(); }
                    }
                }
            }
        }
        else if (me.bstate & BUTTON1_CLICKED) {
            // Terminals that report CLICKED instead of PRESSED+RELEASED
            g.dragging = false;
            Entity* ent = entityAtOwner(mapX, mapY, 0);
            if (ent) {
                g.selectedId = ent->id; g.selectedIds.clear();
                setStatus(std::string("Selected: ") + STATS[ent->type].name);
            } else {
                Entity* any = entityAt(mapX, mapY);
                if (any && any->alive && g.map[mapY][mapX].visible[0]) {
                    g.selectedId = any->id; g.selectedIds.clear();
                    setStatus(std::string(any->owner==OWNER_NATURE?"Animal: ":"Enemy ") + STATS[any->type].name);
                } else { g.selectedId = -1; g.selectedIds.clear(); }
            }
        }
        else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
            // Right click: issue command at cursor position
            g.dragging = false;
            if (g.selectedIds.size() > 1) {
                Entity* tgt = entityAt(mapX, mapY);
                if (tgt && tgt->alive && tgt->owner == 0 && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
                    for (int id : g.selectedIds) {
                        Entity* u = findEntity(id);
                        if (u && u->alive && u->owner == 0 && isUnit(u->type) && u->type != E_CATAPULT)
                            orderGarrison(*u, tgt->id);
                    }
                    setStatus("Garrisoning...");
                } else if (tgt && tgt->alive && tgt->owner != 0 && g.map[mapY][mapX].visible[0])
                    orderGroupAttack(tgt->id);
                else
                    orderGroupMove(mapX, mapY);
            } else {
                Entity* sel = findEntity(g.selectedId);
                if (!sel || sel->owner != 0 || !isUnit(sel->type)) break;
                Entity* tgt = entityAt(mapX, mapY);
                if (tgt && tgt->alive && tgt->owner == 0 && tgt->underConstruction && sel->type == E_PEASANT) {
                    orderHelp(*sel, tgt->id); setStatus("Helping build...");
                } else if (tgt && tgt->alive && tgt->owner == 0 && tgt->type == E_FARM && !tgt->underConstruction && sel->type == E_PEASANT) {
                    orderHelp(*sel, tgt->id); setStatus("Tending farm...");
                } else if (tgt && tgt->alive && tgt->owner == 0 && !tgt->underConstruction && canGarrisonIn(tgt->type) && sel->type != E_CATAPULT) {
                    orderGarrison(*sel, tgt->id);
                } else if (tgt && tgt->alive && tgt->owner != 0 && g.map[mapY][mapX].visible[0]) {
                    orderAttack(*sel, tgt->id); setStatus("Attacking!");
                } else if (sel->type == E_PEASANT) {
                    Terrain ter = g.map[mapY][mapX].terrain;
                    bool isW = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
                    if ((ter==T_GOLD||isW) && g.map[mapY][mapX].resources > 0) {
                        orderGather(*sel, mapX, mapY);
                        setStatus(ter==T_GOLD ? "Mining gold..." : "Chopping wood...");
                    } else if (ter == T_WHEAT && !tgt && canPlace(E_FARM, mapX, mapY, 0)) {
                        int fid = spawnEntity(E_FARM, 0, mapX, mapY, true);
                        orderHelp(*sel, fid);
                        setStatus("Working wheat field...");
                    } else { orderMove(*sel, mapX, mapY); setStatus("Moving..."); }
                } else if (sel->type == E_FISHING_BOAT) {
                    Terrain ter = g.map[mapY][mapX].terrain;
                    if (ter == T_FISH && g.map[mapY][mapX].resources > 0) {
                        orderGather(*sel, mapX, mapY); setStatus("Fishing...");
                    } else { orderMove(*sel, mapX, mapY); setStatus("Moving..."); }
                } else { orderMove(*sel, mapX, mapY); setStatus("Moving..."); }
            }
        }
        // All other events (pure movement): cursor already updated above
        break;
    }
    }

    clamp:
    g.cursorX = std::max(0, std::min(g.cursorX, MAP_W-1));
    g.cursorY = std::max(0, std::min(g.cursorY, MAP_H-1));
}
