#include "realm.h"
#include <chrono>

// ============================================================
// SELECTION HELPERS — keep selectedId (leader) and selectedIds (group) in sync.
// Shift+click and shift+drag use these so single-and-multi selection stay
// indistinguishable to the rest of the codebase.
// ============================================================
static bool selectionContains(int id) {
    if (id < 0) return false;
    if (g.selectedId == id) return true;
    for (int s : g.selectedIds) if (s == id) return true;
    return false;
}

static void addToSelection(int id) {
    if (id < 0 || selectionContains(id)) return;
    // Promote any existing single-selection into the group list first.
    if (g.selectedIds.empty() && g.selectedId >= 0) {
        g.selectedIds.push_back(g.selectedId);
    }
    g.selectedIds.push_back(id);
    if (g.selectedId < 0) g.selectedId = id;
}

static void removeFromSelection(int id) {
    if (id < 0) return;
    if (!g.selectedIds.empty()) {
        g.selectedIds.erase(std::remove(g.selectedIds.begin(), g.selectedIds.end(), id),
                            g.selectedIds.end());
        if (g.selectedIds.size() == 1) {
            g.selectedId = g.selectedIds.front();
            g.selectedIds.clear();
        } else if (g.selectedIds.empty()) {
            g.selectedId = -1;
        } else if (g.selectedId == id) {
            g.selectedId = g.selectedIds.front();
        }
    } else if (g.selectedId == id) {
        g.selectedId = -1;
    }
}

// ============================================================
// COMMAND EMISSION — input never mutates the sim directly. Every verb
// below resolves into a Command pushed onto the queue; the tick applies
// it (commands.cpp). Selection/camera stay local — a Command carries
// the explicit unit ids it acts on.
// ============================================================
static void pushCmd(int type, std::vector<int> units, int x = 0, int y = 0,
                    int target = -1, int arg = 0, int x2 = 0, int y2 = 0) {
    Command c;
    c.type = type; c.player = 0;
    c.units = std::move(units);
    c.x = x; c.y = y; c.x2 = x2; c.y2 = y2;
    c.target = target; c.arg = arg;
    pushCommand(c);
}

// Current selection as a unit-id list (own live units only).
static std::vector<int> selectedUnitIds() {
    std::vector<int> ids;
    auto add = [&](int id) {
        Entity* u = findEntity(id);
        if (u && u->alive && u->owner == 0 && isUnit(u->type)) ids.push_back(id);
    };
    if (!g.selectedIds.empty()) for (int id : g.selectedIds) add(id);
    else if (g.selectedId >= 0) add(g.selectedId);
    return ids;
}

// Single right-click / Enter command from a selected player unit onto (x,y).
// Picks the right verb based on what's at the tile: help-build / tend-farm /
// garrison / attack / gather / sow-farm / fish / move. Used by both the
// keyboard Enter handler and the mouse right-click handler so the two paths
// can never drift out of sync.
static void cmdAtTileSingle(Entity* sel, int x, int y) {
    if (!sel || sel->owner != 0 || !isUnit(sel->type)) return;
    Entity* tgt = entityAt(x, y);
    bool visible = g.map[y][x].visible[0];

    // Wagon: right-click a friendly building to load (if empty) or unload.
    if (sel->type == E_WAGON) {
        if (tgt && tgt->alive && tgt->owner == 0 && isBuilding(tgt->type) && !tgt->underConstruction) {
            pushCmd(CMD_HAUL, {sel->id}, 0, 0, tgt->id);
            setStatus(sel->carrying ? "Wagon delivering..." : "Wagon heading to load...");
            return;
        }
        pushCmd(CMD_MOVE, {sel->id}, x, y); setStatus("Moving..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == 0 && tgt->underConstruction && sel->type == E_PEASANT) {
        pushCmd(CMD_HELP, {sel->id}, 0, 0, tgt->id); setStatus("Helping build..."); return;
    }
    // Derelict village house: repair it to claim it.
    if (tgt && tgt->alive && tgt->owner == OWNER_NATURE && tgt->type == E_HOUSE
        && tgt->underConstruction && sel->type == E_PEASANT) {
        pushCmd(CMD_HELP, {sel->id}, 0, 0, tgt->id); setStatus("Restoring the old house..."); return;
    }
    if (tgt && tgt->alive && tgt->owner == 0 && tgt->type == E_FARM
        && !tgt->underConstruction && sel->type == E_PEASANT) {
        pushCmd(CMD_HELP, {sel->id}, 0, 0, tgt->id); setStatus("Tending farm..."); return;
    }
    bool ruinClaim = tgt && isClaimable(tgt->type) && tgt->owner == OWNER_NATURE;
    if (tgt && tgt->alive && (tgt->owner == 0 || ruinClaim) && !tgt->underConstruction
        && canGarrisonIn(tgt->type) && sel->type != E_CATAPULT) {
        pushCmd(CMD_GARRISON, {sel->id}, 0, 0, tgt->id);
        if (ruinClaim) setStatus(std::string("Claiming the ") + STATS[tgt->type].name + "...");
        return;
    }
    if (tgt && tgt->alive && tgt->owner != 0 && visible) {
        pushCmd(CMD_ATTACK, {sel->id}, 0, 0, tgt->id); setStatus("Attacking!"); return;
    }
    if (sel->type == E_PEASANT) {
        Terrain ter = g.map[y][x].terrain;
        bool isW = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
        if ((ter==T_GOLD||isW||ter==T_BERRY) && g.map[y][x].resources > 0) {
            pushCmd(CMD_GATHER, {sel->id}, x, y);
            setStatus(ter==T_GOLD ? "Mining gold..."
                    : ter==T_BERRY ? "Picking berries..." : "Chopping wood...");
            return;
        }
        if (ter == T_WHEAT && !tgt && canPlace(E_FARM, x, y, 0)) {
            pushCmd(CMD_SOW_FARM, {sel->id}, x, y);
            setStatus("Working wheat field...");
            return;
        }
        pushCmd(CMD_MOVE, {sel->id}, x, y); setStatus("Moving..."); return;
    }
    if (sel->type == E_FISHING_BOAT) {
        Terrain ter = g.map[y][x].terrain;
        if (ter == T_FISH && g.map[y][x].resources > 0) {
            pushCmd(CMD_GATHER, {sel->id}, x, y); setStatus("Fishing..."); return;
        }
        pushCmd(CMD_MOVE, {sel->id}, x, y); setStatus("Moving..."); return;
    }
    pushCmd(CMD_MOVE, {sel->id}, x, y); setStatus("Moving...");
}

// Group right-click / Enter command — applies to the whole selection.
// Same path for keyboard and mouse.
static void cmdAtTileGroup(int x, int y) {
    Entity* tgt = entityAt(x, y);
    bool visible = g.map[y][x].visible[0];
    if (tgt && tgt->alive
        && (tgt->owner == 0 || (isClaimable(tgt->type) && tgt->owner == OWNER_NATURE))
        && !tgt->underConstruction && canGarrisonIn(tgt->type)) {
        // Garrison the soldiers, but never sweep the workers in: a keep/TC has
        // a big footprint that sits in your base, and a stray move-click on it
        // used to march every peasant into the walls "for no reason". Peasants
        // take the move instead (shelter them deliberately one at a time, or
        // let the wounded-flee logic tuck them away under fire).
        std::vector<int> troops, workers;
        for (int id : selectedUnitIds()) {
            Entity* u = findEntity(id);
            if (!u) continue;
            (u->type == E_PEASANT ? workers : troops).push_back(id);
        }
        if (!troops.empty()) {
            pushCmd(CMD_GARRISON, std::move(troops), 0, 0, tgt->id);
            setStatus("Garrisoning...");
        }
        if (!workers.empty()) pushCmd(CMD_MOVE, std::move(workers), x, y);
        return;
    }
    if (tgt && tgt->alive && tgt->owner != 0 && visible) {
        pushCmd(CMD_ATTACK, selectedUnitIds(), 0, 0, tgt->id); return;
    }
    pushCmd(CMD_MOVE, selectedUnitIds(), x, y);
}

void handleInput(int ch) {
    if (ch == ERR) return;
    // Who owns the cursor decides whether render may auto-pan to it
    // (keyboard: yes; mouse: never — see renderMap). One place, all modes.
    if (ch == KEY_MOUSE) g.cursorByMouse = true;
    else if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT
          || ch == KEY_SR || ch == KEY_SF || ch == KEY_SLEFT || ch == KEY_SRIGHT
          || ch == KEY_PPAGE || ch == KEY_NPAGE || ch == KEY_HOME || ch == KEY_END
          || ch == '\t' || ch == 'h' || ch == '.' || ch == ',')
        g.cursorByMouse = false;
    if (ch == 'q' || ch == 'Q') {
        if (g.mode == M_GAME_OVER) { g.returnToMenu = true; return; }
        // Mid-game quit needs a confirming second press — one stray key
        // shouldn't end a long session. Returns to the main menu (the splash
        // owns app exit); the match is abandoned, not saved.
        static int qArmedTick = -1;
        if (qArmedTick >= 0 && qArmedTick <= g.tick && g.tick - qArmedTick < 40) {
            qArmedTick = -1;
            g.returnToMenu = true;
            return;
        }
        qArmedTick = g.tick;
        setStatus("Press Q again to abandon the match and return to the menu (no save!).");
        return;
    }
    if ((ch=='\n'||ch==KEY_ENTER||ch=='\r') && g.mode==M_GAME_OVER) {
        g.returnToMenu = true; return;
    }
    if ((ch=='p'||ch=='P') && (g.mode==M_NORMAL||g.mode==M_PAUSED)) {
        g.mode = (g.mode==M_PAUSED) ? M_NORMAL : M_PAUSED; return;
    }
    // Help overlay: '?' opens (game pauses underneath); any key closes.
    if (g.mode == M_HELP) { if (ch != ERR && ch != KEY_MOUSE) g.mode = M_NORMAL; return; }
    if (ch == '?' && g.mode == M_NORMAL) { g.mode = M_HELP; return; }

    // From the pause screen, S / L / Enter open the visual Save/Load menu.
    if (g.mode == M_PAUSED &&
        (ch=='s'||ch=='S'||ch=='l'||ch=='L'||ch=='\n'||ch=='\r'||ch==KEY_ENTER)) {
        g.saveSlotSel = 0; g.mode = M_SAVELOAD; return;
    }

    // Visual Save/Load menu: pick a slot (arrows or click), then act on it.
    if (g.mode == M_SAVELOAD) {
        if (ch == 27 || ch=='p' || ch=='P') { g.mode = M_NORMAL; return; }
        if (ch == KEY_UP)   { g.saveSlotSel = (g.saveSlotSel + NUM_SAVE_SLOTS - 1) % NUM_SAVE_SLOTS; return; }
        if (ch == KEY_DOWN) { g.saveSlotSel = (g.saveSlotSel + 1) % NUM_SAVE_SLOTS; return; }
        int slot = g.saveSlotSel + 1;
        char path[64]; saveSlotPath(slot, path, sizeof(path));
        if (ch=='s' || ch=='S') {
            if (saveGame(path)) setStatus(std::string("Saved to slot ") + std::to_string(slot) + ".");
            else                setStatus("Save failed! (disk full?)");
            return;
        }
        if (ch=='d' || ch=='D') {
            SaveSlotInfo info;
            if (peekSave(path, info)) { remove(path); setStatus(std::string("Deleted slot ") + std::to_string(slot) + "."); }
            else setStatus("Slot is already empty.");
            return;
        }
        if (ch=='\n' || ch=='\r' || ch==KEY_ENTER || ch=='l' || ch=='L') {
            if (replayPlaying()) { setStatus("Can't load a save during replay playback."); return; }
            SaveSlotInfo info;
            if (!peekSave(path, info)) { setStatus("That slot is empty — nothing to load."); return; }
            if (loadGame(path)) {
                replayStopRecording();   // loaded state no longer reproduces from the seed
                g.mode = M_NORMAL;
                setStatus(std::string("Loaded slot ") + std::to_string(slot) + ".");
            } else setStatus("Load failed — wrong version or corrupt.");
            return;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) == OK && g.slMenuRowH > 0
                && me.x >= g.slMenuX && me.x < g.slMenuX + g.slMenuW) {
                int row = (me.y - g.slMenuRowY0) / g.slMenuRowH;
                if (row >= 0 && row < NUM_SAVE_SLOTS) g.saveSlotSel = row;  // click selects; Enter loads
            }
            return;
        }
        return;
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
        case 'r': case 'R': tb = E_BRIDGE;      break;
        case 'y': case 'Y': tb = E_GRANARY;     break;
        case 'v': case 'V': tb = E_TAVERN;      break;
        case 'o': case 'O': tb = E_WELL;        break;
        case 'e': case 'E': tb = E_MANOR;       break;
        case 'u': case 'U': tb = E_STONEMASON;  break;
        case 27: g.mode = M_NORMAL; return;
        default: return;
        }
        if (tb != E_NONE) {
            // Two-step placement: pick the type, then move the cursor with a
            // footprint preview and Enter to commit. Wall handled separately above.
            g.buildPending = tb;
            g.mode = M_BUILD_PLACE;
            setStatus(std::string("Place ") + STATS[tb].name + " — arrows/mouse to position, Enter to build, Esc to cancel");
        }
        return;
    }

    // Build placement mode: cursor moves freely with a ghost footprint preview.
    if (g.mode == M_BUILD_PLACE) {
        Entity* sel = findEntity(g.selectedId);
        if (!sel || sel->type != E_PEASANT || sel->owner != 0) {
            g.mode = M_NORMAL; g.buildPending = E_NONE; return;
        }
        if (ch == 27) {
            g.mode = M_NORMAL; g.buildPending = E_NONE;
            setStatus("Build cancelled."); return;
        }
        if (ch == KEY_UP)    { g.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { g.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { g.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { g.cursorX++; goto clamp; }
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            // Emit the build command; the tick validates cost/placement and
            // sets any failure status. Either way, exit place mode.
            pushCmd(CMD_BUILD, {sel->id}, g.cursorX, g.cursorY, -1, g.buildPending);
            g.mode = M_NORMAL; g.buildPending = E_NONE;
            return;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
            int mapSY = me.y - 2;
            int mapSX = me.x / tileW;
            int mapX  = g.viewX + mapSX;
            int mapY  = g.viewY + mapSY;
            bool inMap = (mapSY >= 0 && mapSY < g.viewH && g.viewW > 0 && me.x < g.viewW * tileW && inBounds(mapX, mapY));
            if (inMap) { g.cursorX = mapX; g.cursorY = mapY; }
            if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED)) {
                if (inMap) {
                    pushCmd(CMD_BUILD, {sel->id}, mapX, mapY, -1, g.buildPending);
                    // Hold Shift to lay down a chain of the same structure (e.g.
                    // a row of houses) without reopening the build menu each time.
                    if (me.bstate & BUTTON_SHIFT) {
                        setStatus(std::string("Place another ") + STATS[g.buildPending].name
                                  + " — release Shift / right-click / Esc to stop");
                    } else {
                        g.mode = M_NORMAL; g.buildPending = E_NONE;
                    }
                }
            } else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
                g.mode = M_NORMAL; g.buildPending = E_NONE;
                setStatus("Build cancelled.");
            }
            goto clamp;
        }
        return;
    }

    // Patrol target picker.
    if (g.mode == M_PATROL_SET) {
        if (ch == 27) { g.mode = M_NORMAL; setStatus("Patrol cancelled."); return; }
        int tx = -1, ty = -1;
        if (ch == KEY_UP)    { g.cursorY--; goto clamp; }
        if (ch == KEY_DOWN)  { g.cursorY++; goto clamp; }
        if (ch == KEY_LEFT)  { g.cursorX--; goto clamp; }
        if (ch == KEY_RIGHT) { g.cursorX++; goto clamp; }
        if (ch == ' ' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            tx = g.cursorX; ty = g.cursorY;
        } else if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
            int mapSY = me.y - 2;
            int mapSX = me.x / tileW;
            int mapX  = g.viewX + mapSX;
            int mapY  = g.viewY + mapSY;
            bool inMap = (mapSY >= 0 && mapSY < g.viewH && g.viewW > 0 && me.x < g.viewW * tileW && inBounds(mapX, mapY));
            if (!inMap) goto clamp;
            g.cursorX = mapX; g.cursorY = mapY;
            if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED | BUTTON3_PRESSED)) {
                tx = mapX; ty = mapY;
            } else goto clamp;
        } else return;

        if (tx < 0 || ty < 0) goto clamp;
        {
            std::vector<int> ids = selectedUnitIds();
            // Land units only — count for the status, the sim re-filters anyway.
            int started = 0;
            for (int id : ids) { Entity* u = findEntity(id); if (u && !isNaval(u->type) && !(u->x==tx && u->y==ty)) started++; }
            pushCmd(CMD_PATROL, std::move(ids), tx, ty);
            g.mode = M_NORMAL;
            if (started > 0) setStatus(std::to_string(started) + " unit(s) on patrol");
            else             setStatus("No valid units for patrol.");
        }
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
            else if (ch=='x'||ch=='X') tt = E_CROSSBOWMAN;
            else if (ch=='p'||ch=='P') tt = E_SAPPER;
            else if (ch=='c'||ch=='C') tt = E_CATAPULT;
            else if (ch=='r'||ch=='R') tt = E_RAM;
        }
        else if (sel->type == E_STABLE) {
            if      (ch=='k'||ch=='K') tt = E_KNIGHT;
            else if (ch=='h'||ch=='H') tt = E_HUSSAR;
        }
        else if (sel->type == E_CHURCH) { if (ch=='m'||ch=='M') tt = E_MONK; }
        else if (sel->type == E_MILL || sel->type == E_GRANARY) { if (ch=='w'||ch=='W') tt = E_WAGON; }
        else if (sel->type == E_CASTLE) { if (ch=='t'||ch=='T') tt = E_TREBUCHET; }
        else if (sel->type == E_DOCK)   {
            if      (ch=='b'||ch=='B') tt = E_FISHING_BOAT;
            else if (ch=='w'||ch=='W') tt = E_WARSHIP;
            else if (ch=='t'||ch=='T') tt = E_TRANSPORT;
        }
        if (tt != E_NONE) { pushCmd(CMD_TRAIN, {}, 0, 0, sel->id, tt); g.mode = M_NORMAL; }
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
                if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT)
                    pushCmd(CMD_BUILD_WALL, {sel->id}, g.wallDragX, g.wallDragY,
                            -1, 0, g.cursorX, g.cursorY);
                g.dragging = false; g.mode = M_NORMAL;
            }
            goto clamp;
        }
        if (ch == KEY_MOUSE) {
            MEVENT me;
            if (getmouse(&me) != OK) goto clamp;
            int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
            int mapSY = me.y - 2;
            int mapSX = me.x / tileW;
            int mapX  = g.viewX + mapSX;
            int mapY  = g.viewY + mapSY;
            bool inMap = (mapSY>=0 && mapSY<g.viewH && g.viewW>0 && me.x<g.viewW*tileW && inBounds(mapX,mapY));
            if (!inMap) goto clamp;
            g.cursorX = mapX; g.cursorY = mapY;
            if (me.bstate & BUTTON1_PRESSED) {
                g.dragging = true;
                g.wallDragX = mapX; g.wallDragY = mapY;
            } else if (me.bstate & (BUTTON1_RELEASED | BUTTON1_CLICKED)) {
                if (g.dragging || (me.bstate & BUTTON1_CLICKED)) {
                    Entity* sel = findEntity(g.selectedId);
                    if (sel && sel->alive && sel->owner==0 && sel->type==E_PEASANT)
                        pushCmd(CMD_BUILD_WALL, {sel->id}, g.wallDragX, g.wallDragY,
                                -1, 0, mapX, mapY);
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
            if (sel && sel->alive && sel->owner == 0)
                pushCmd(CMD_RALLY, {}, tx, ty, sel->id);
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(g.cursorX, g.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
            int mapSY = me.y - 2;
            int mapSX = me.x / tileW;
            int mapX = g.viewX + mapSX, mapY = g.viewY + mapSY;
            if (mapSY >= 0 && mapSY < g.viewH && g.viewW > 0 && me.x < g.viewW * tileW && inBounds(mapX, mapY)) {
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
            std::vector<int> ids = selectedUnitIds();
            if (!ids.empty()) {
                bool group = ids.size() > 1;
                pushCmd(CMD_ATTACK_MOVE, std::move(ids), tx, ty);
                if (!group) setStatus("Attack-moving.");
            }
            g.mode = M_NORMAL;
        };
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) { commit(g.cursorX, g.cursorY); goto clamp; }
        if (ch == KEY_MOUSE) {
            MEVENT me; if (getmouse(&me) != OK) goto clamp;
            int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
            int mapSY = me.y - 2;
            int mapSX = me.x / tileW;
            int mapX = g.viewX + mapSX, mapY = g.viewY + mapSY;
            if (mapSY >= 0 && mapSY < g.viewH && me.x < g.viewW * tileW && inBounds(mapX, mapY)) {
                g.cursorX = mapX; g.cursorY = mapY;
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_RELEASED | BUTTON3_CLICKED)) commit(mapX, mapY);
            }
        }
        goto clamp;
    }

    // Market trade menu (markets and claimed trading posts).
    if (g.mode == M_MARKET_TRADE) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        Entity* mkt = findEntity(g.selectedId);
        if (!mkt || (mkt->type != E_MARKET && mkt->type != E_TRADING_POST)
            || mkt->underConstruction) { g.mode = M_NORMAL; return; }
        // Rates + statuses live in the CMD_TRADE table in commands.cpp.
        int tradeIdx = -1;
        if      (ch == 'g' || ch == 'G') tradeIdx = 0;
        else if (ch == 'w' || ch == 'W') tradeIdx = 1;
        else if (ch == 'f' || ch == 'F') tradeIdx = 2;
        else if (ch == 'v' || ch == 'V') tradeIdx = 3;
        if (tradeIdx >= 0) pushCmd(CMD_TRADE, {}, 0, 0, mkt->id, tradeIdx);
        return;
    }

    // Research selection from the blacksmith.
    if (g.mode == M_RESEARCH_SELECT) {
        if (ch == 27) { g.mode = M_NORMAL; return; }
        Entity* bs = findEntity(g.selectedId);
        if (!bs || bs->type != E_BLACKSMITH || bs->underConstruction) {
            g.mode = M_NORMAL; return;
        }
        // Costs/durations + statuses live in the CMD_RESEARCH table in commands.cpp.
        int bit = 0;
        if      (ch == 'i' || ch == 'I') bit = R_IRON_WEAPONS;
        else if (ch == 'c' || ch == 'C') bit = R_CROSSBOWS;
        else if (ch == 'p' || ch == 'P') bit = R_PIKES;
        else if (ch == 'w' || ch == 'W') bit = R_COUNTERWEIGHT;
        else if (ch == 'h' || ch == 'H') bit = R_PLATE_HELM;
        if (bit) { pushCmd(CMD_RESEARCH, {}, 0, 0, bs->id, bit); g.mode = M_NORMAL; }
        return;
    }

    switch (ch) {
    // Save / load. F5-F8 = save slots 1-4, F9-F12 = load slots 1-4.
    // Slot 1 is the quicksave.
    case KEY_F(5): case KEY_F(6): case KEY_F(7): case KEY_F(8): {
        int slot = (ch - KEY_F(5)) + 1;
        char path[64]; snprintf(path, sizeof(path), "realm-slot%d.sav", slot);
        if (saveGame(path)) setStatus(std::string("Saved to slot ") + std::to_string(slot) + ".");
        else                setStatus("Save failed! (disk full?)");
        break;
    }
    case KEY_F(9): case KEY_F(10): case KEY_F(11): case KEY_F(12): {
        if (replayPlaying()) { setStatus("Can't load a save during replay playback."); break; }
        int slot = (ch - KEY_F(9)) + 1;
        char path[64]; snprintf(path, sizeof(path), "realm-slot%d.sav", slot);
        if (loadGame(path)) {
            // The recording's command stream no longer reproduces this state
            // from the seed — stop rather than write a lying replay.
            replayStopRecording();
            setStatus(std::string("Loaded slot ") + std::to_string(slot) + ".");
        }
        else setStatus("Load failed — no save, wrong version, or corrupt.");
        break;
    }

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
        // A direct command cancels waypoints/patrol — the sim does that when
        // it applies the command, so nothing to clear here.
        if (g.selectedIds.size() > 1) cmdAtTileGroup(g.cursorX, g.cursorY);
        else                          cmdAtTileSingle(findEntity(g.selectedId), g.cursorX, g.cursorY);
        break;
    }

    // Patrol: enter targeting mode; next click sets patrol target. Selected
    // units bounce between their current position and that target indefinitely.
    case 'Z': case 'z': {
        bool hasUnit = false;
        if (!g.selectedIds.empty()) { for (int id : g.selectedIds) { Entity* u = findEntity(id); if (u && u->alive && u->owner==0 && isUnit(u->type) && !isNaval(u->type)) { hasUnit = true; break; } } }
        else if (g.selectedId >= 0) { Entity* u = findEntity(g.selectedId); if (u && u->alive && u->owner==0 && isUnit(u->type) && !isNaval(u->type)) hasUnit = true; }
        if (!hasUnit) { setStatus("Select land units to patrol."); break; }
        g.mode = M_PATROL_SET;
        setStatus("Patrol: click target — units bounce between current position and target. [Esc] cancel");
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
            if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK||sel->type==E_CASTLE||sel->type==E_CHURCH||sel->type==E_MILL||sel->type==E_GRANARY) {
                g.mode = M_TRAIN_SELECT;
                setStatus("Select unit to train...");
            } else setStatus("This building can't train.");
        } else setStatus("Select a production building!");
        break;
    }

    // Trebuchet pack / deploy toggle
    case 'D': case 'd': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->alive && sel->owner == 0 && sel->type == E_TREBUCHET)
            pushCmd(CMD_PACK, {sel->id});   // statuses set on apply
        else setStatus("Select a trebuchet to pack/deploy.");
        break;
    }

    // Eject garrison from selected building or transport
    case 'U': case 'u': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->alive && sel->owner == 0 && canGarrisonIn(sel->type)) {
            int n = (int)sel->garrison.size();
            if (n > 0) { pushCmd(CMD_UNGARRISON, {}, 0, 0, sel->id); setStatus(std::to_string(n) + " unit(s) ejected"); }
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
        if (sel->type == E_MARKET || sel->type == E_TRADING_POST) {
            g.mode = M_MARKET_TRADE;
            setStatus("Trade (40→30): [G]old→Wood  [W]ood→Gold  [F]ood←Gold  [V]ictuals→Gold  [Esc]");
        } else if (sel->type == E_TAVERN) {
            pushCmd(CMD_FEAST, {}, 0, 0, sel->id);   // validation + statuses on apply
        } else if (sel->type == E_BLACKSMITH) {
            g.mode = M_RESEARCH_SELECT;
            setStatus("Research: [I]ron 100/100 [C]rossbows 80/80 [P]ikes 100/100 [W]eight 120/150 [H]elm 120/100 [Esc]");
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

    // Gate toggle: cycle auto -> locked-open -> locked-closed -> auto
    case 'O': {
        Entity* sel = findEntity(g.selectedId);
        if (sel && sel->alive && sel->owner==0 && sel->type==E_GATE && !sel->underConstruction)
            pushCmd(CMD_GATE, {}, 0, 0, sel->id);   // status reflects the new state, set on apply
        break;
    }

    // Debug: reveal entire map (Shift+S). Goes through the funnel because
    // explored[] feeds sim decisions — a local poke would desync replays.
    case 'S': {
        pushCmd(CMD_REVEAL, {});
        break;
    }

    // Stop: cancel orders/waypoints/patrol and stand ground. Unlike hold
    // position ('x'), auto-aggro stays on — the StarCraft/AoE 'S' stop.
    // (Capital 'S' remains the debug map reveal.)
    case 's': {
        std::vector<int> ids = selectedUnitIds();
        if (!ids.empty()) { pushCmd(CMD_STOP, std::move(ids)); setStatus("Stop."); }
        break;
    }

    // 'a' is contextual: attack-move with military selected, otherwise
    // select all military. 'A' (Shift+A) ALWAYS (re)selects all military —
    // so after an attack-move you can grab the whole army back in one key.
    case 'A': case 'a': {
        auto isMilType = [](EntityType t) {
            return t==E_MILITIA||t==E_ARCHER||t==E_KNIGHT||t==E_SPEARMAN
                || t==E_CATAPULT||t==E_TREBUCHET||t==E_RAM
                || t==E_CROSSBOWMAN||t==E_HUSSAR||t==E_MONK||t==E_SAPPER;
        };
        bool hasMilitarySel = false;
        if (ch == 'a') {
            if (!g.selectedIds.empty()) {
                for (int id : g.selectedIds) {
                    Entity* e = findEntity(id);
                    if (e && isMilType(e->type)) { hasMilitarySel = true; break; }
                }
            } else if (g.selectedId >= 0) {
                Entity* e = findEntity(g.selectedId);
                if (e && isMilType(e->type)) hasMilitarySel = true;
            }
        }
        if (hasMilitarySel) {
            g.mode = M_ATTACK_MOVE;
            setStatus("Attack-move: click destination. [Esc] cancel");
        } else {
            g.selectedIds.clear(); g.selectedId = -1;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != 0 || e.state == S_GARRISONED) continue;
                if (isMilType(e.type)) {
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
        std::vector<int> ids = selectedUnitIds();
        if (!ids.empty()) { pushCmd(CMD_HOLD, std::move(ids)); setStatus("Hold position."); }
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
        int tileW = (displayMode == DM_EMOJI) ? 2 : 1;
        int mapSY = me.y - 2;
        int mapSX = me.x / tileW;
        int mapX  = g.viewX + mapSX;
        int mapY  = g.viewY + mapSY;
        // Minimap click → jump viewport; click-and-drag scrubs it (AoE-style).
        // Minimap sits at panelX+1..+mmW, mmY..+mmH.
        {
            static bool mmScrub = false;
            bool wasScrub = mmScrub;
            if (me.bstate & BUTTON1_RELEASED) mmScrub = false;
            int maxY2, maxX2; getmaxyx(stdscr, maxY2, maxX2); (void)maxY2;
            int panelW = 24, panelX = maxX2 - panelW;
            int mmX = panelX + 1, mmY = 1, mmW = panelW - 2;
            int mmH = std::min(g.viewH/3, 14);
            if (me.x >= mmX && me.x < mmX+mmW && me.y >= mmY && me.y < mmY+mmH) {
                bool buttoned = me.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED | BUTTON1_RELEASED
                                          | BUTTON3_CLICKED | BUTTON3_PRESSED);
                if (me.bstate & (BUTTON1_CLICKED | BUTTON1_PRESSED)) mmScrub = true;
                if (buttoned || (wasScrub && (me.bstate & REPORT_MOUSE_POSITION))) {
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
        bool clickEvt = (me.bstate & (BUTTON1_PRESSED|BUTTON1_RELEASED|BUTTON1_CLICKED
                                    |BUTTON1_DOUBLE_CLICKED|BUTTON3_CLICKED|BUTTON3_PRESSED)) != 0;

        // Edge scrolling: pointer against the WINDOW border pans the viewport.
        // Runs before the in-map check so the bottom bars / HUD edges scroll too.
        // Time-throttled to one tile per ~70ms regardless of event rate —
        // trackpads stream motion events at device rate, and the old
        // step-per-event logic scrolled 30+ tiles/sec the instant the pointer
        // brushed the margin (the map flew, the cursor tile looked possessed).
        // The shim feeds synthetic position reports while the pointer parks at
        // an edge, so scrolling continues without wiggling.
        if (!clickEvt) {
            int maxY2, maxX2; getmaxyx(stdscr, maxY2, maxX2);
            static std::chrono::steady_clock::time_point lastEdgeScroll{};
            auto nowT = std::chrono::steady_clock::now();
            if (nowT - lastEdgeScroll >= std::chrono::milliseconds(70)) {
                int dx = 0, dy = 0;
                if (me.x <= 1)              dx = -1;
                else if (me.x >= maxX2 - 2) dx =  1;
                if (me.y <= 1)              dy = -1;
                else if (me.y >= maxY2 - 2) dy =  1;
                if (dx || dy) {
                    lastEdgeScroll = nowT;
                    g.viewX = std::max(0, std::min(g.viewX + dx, MAP_W - g.viewW));
                    g.viewY = std::max(0, std::min(g.viewY + dy, MAP_H - g.viewH));
                    // The view just moved: re-derive the map tile under the
                    // pointer or the cursor gets pinned with stale view coords.
                    mapX = g.viewX + mapSX;
                    mapY = g.viewY + mapSY;
                }
            }
        }

        // In-map = inside the VISIBLE viewport. mapSY < viewH matters: without
        // it, hovering the bottom status/hotkey bars counted as map (mapY was
        // still inBounds) and dragged the cursor to tiles below the screen.
        bool inMap = (mapSY >= 0 && mapSY < g.viewH && g.viewW > 0
                      && me.x < g.viewW * tileW && inBounds(mapX, mapY));
        if (!inMap) { g.dragging = false; break; }

        // Hover-track the cursor, but only when the mouse actually crossed into a new map
        // cell. Without this, every stale ncurses motion event would yank the cursor back
        // to the OS mouse position, fighting keyboard arrow input. Clicks/drags still pin
        // the cursor regardless of last-cell state.
        static int lastMx = -9999, lastMy = -9999;
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
        bool shift = (me.bstate & BUTTON_SHIFT) != 0;

        // Shared logic for non-drag clicks: shift toggles membership, plain click replaces.
        auto handleClick = [&](){
            Entity* ent = entityAtOwner(mapX, mapY, 0);
            if (shift && ent && isUnit(ent->type)) {
                if (selectionContains(ent->id)) { removeFromSelection(ent->id); setStatus("Removed from selection"); }
                else                            { addToSelection(ent->id);      setStatus("Added to selection"); }
                return;
            }
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
        };

        // Drag-box commit: shift unions with current selection; otherwise replaces.
        // When the box would pull in both military and peasants, peasants are dropped
        // (unless shift held) so dragging across your base doesn't yank workers off jobs.
        auto handleDragBox = [&](int x0, int y0, int x1, int y1){
            std::vector<int> hits;
            bool sawMilitary = false, sawPeasant = false;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != 0 || !isUnit(e.type)) continue;
                if (e.state == S_GARRISONED) continue;
                if (e.x < x0 || e.x > x1 || e.y < y0 || e.y > y1) continue;
                if (e.type == E_PEASANT) sawPeasant = true; else sawMilitary = true;
                hits.push_back(e.id);
            }
            bool filterPeasants = (!shift && sawMilitary && sawPeasant);
            if (!shift) { g.selectedIds.clear(); g.selectedId = -1; }
            for (int id : hits) {
                Entity* e = findEntity(id);
                if (!e) continue;
                if (filterPeasants && e->type == E_PEASANT) continue;
                addToSelection(id);
            }
            int n = (int)g.selectedIds.size();
            if (n == 0 && g.selectedId >= 0) n = 1;
            if (n > 0) setStatus(std::to_string(n) + " units selected");
        };

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
                    int x0 = std::min(g.dragStartX, mapX), x1 = std::max(g.dragStartX, mapX);
                    int y0 = std::min(g.dragStartY, mapY), y1 = std::max(g.dragStartY, mapY);
                    handleDragBox(x0, y0, x1, y1);
                } else {
                    handleClick();
                }
            }
        }
        else if (me.bstate & BUTTON1_CLICKED) {
            // Terminals that report CLICKED instead of PRESSED+RELEASED
            g.dragging = false;
            handleClick();
        }
        else if (me.bstate & (BUTTON3_CLICKED | BUTTON3_PRESSED)) {
            // Right click: issue command at cursor position.
            // Shift+RClick appends a waypoint to every selected unit's queue
            // without disturbing their current order; a plain RClick clears any
            // existing waypoints/patrol so the new command is honoured immediately.
            g.dragging = false;
            if (shift) {
                std::vector<int> ids = selectedUnitIds();
                if (!ids.empty()) {
                    setStatus("Waypoint queued (" + std::to_string(ids.size()) + " units)");
                    pushCmd(CMD_WAYPOINT, std::move(ids), mapX, mapY);
                }
            } else {
                // Plain right-click: the applied command clears waypoints/patrol.
                if (g.selectedIds.size() > 1) cmdAtTileGroup(mapX, mapY);
                else                          cmdAtTileSingle(findEntity(g.selectedId), mapX, mapY);
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
