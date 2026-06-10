#include "realm.h"

// ============================================================
// UI RENDER — top bar, side panel, minimap, menus, status line.
// World/terrain rendering lives in render.cpp.
// ============================================================

// Safe entity-state display name. EntityState has more values than the old
// hard-coded array covered (notably S_ENTERING and S_GARRISONED), so any
// non-peasant selected mid-board would index past the array.
static const char* stateName(EntityState s) {
    switch (s) {
        case S_IDLE:       return "Idle";
        case S_MOVING:     return "Moving";
        case S_ATTACKING:  return "Attacking";
        case S_GATHERING:  return "Gathering";
        case S_BUILDING:   return "Building";
        case S_TRAINING:   return "Training";
        case S_RETURNING:  return "Returning";
        case S_DEAD:       return "Dead";
        case S_ENTERING:   return "Boarding";
        case S_GARRISONED: return "Garrisoned";
    }
    return "Unknown";
}

void renderUI() {
    int maxY, maxX; getmaxyx(stdscr, maxY, maxX);
    Player& p = g.players[0]; int panelW = 24, panelX = maxX - panelW;

    // Top bar
    attron(COLOR_PAIR(CP_UI_BAR)|A_BOLD); mvhline(0, 0, ' ', maxX);
    mvprintw(0, 1, " REALM "); attroff(A_BOLD);
    int idleCount = 0, idleBldg = 0, popForecast = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != 0) continue;
        if (e.type == E_PEASANT && e.state == S_IDLE) idleCount++;
        if (isBuilding(e.type) && !e.underConstruction) {
            bool producer = (e.type==E_TOWNHALL||e.type==E_BARRACKS||e.type==E_STABLE||e.type==E_DOCK);
            if (producer && e.producing == E_NONE && e.queue.empty()) idleBldg++;
            if (e.producing != E_NONE) popForecast += STATS[e.producing].supplyUsed;
            for (int qt : e.queue) popForecast += STATS[(EntityType)qt].supplyUsed;
        }
    }
    mvprintw(0, 9, "Gold:%-5d Wood:%-5d Food:%-5d Pop:%d/%d(+%d) Idle:%d/%d",
             p.gold, p.wood, p.food, p.supply, p.supplyMax, popForecast, idleCount, idleBldg);

    int iconX = maxX - 22;
    if (getBrightness() > 0.5f) {
        attron(COLOR_PAIR(CP_SUN)|A_BOLD);
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "☀️" : "*");
        attroff(COLOR_PAIR(CP_SUN)|A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_MOON));
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "🌙" : "o");
        attroff(COLOR_PAIR(CP_MOON));
    }
    attron(COLOR_PAIR(CP_UI_BAR));
    const char* wn = (g.weather == W_STORM) ? "Storm" : (g.weather == W_RAIN) ? "Rain " : (g.weather == W_SNOW) ? "Snow " : "Clear";
    mvprintw(0, iconX+1, " %-5s %-6s %s", getTimeName(), getSeasonName(), wn);
    attroff(COLOR_PAIR(CP_UI_BAR));

    // Terrain info bar
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(1, 0, '-', g.viewW); attroff(COLOR_PAIR(CP_UI_DIM));
    if (inBounds(g.cursorX, g.cursorY) && g.map[g.cursorY][g.cursorX].explored[0]) {
        Tile& ct = g.map[g.cursorY][g.cursorX];
        const char* bn[] = {"Temperate","Desert","Tundra","Swamp","Woodland","Volcanic","Ocean"};
        const char* tn[] = {"Grassland","Tall Grass","Wildflowers","Meadow","Oak Forest","Pine Forest",
            "Palm Grove","Dead Tree","Mountain","Rolling Hills","Stone","Deep Water","Shallows",
            "Marshland","Reed Bed","Gold Deposit","Sandy Ground","Sand Dunes","Snow Cover","Frozen Ice",
            "Bare Earth","Stone Road","Mud","Wheat Field","Berry Bush","Fish Shoal","Ancient Ruins","Gravel",
            "Lava Fissure","Volcanic Ash",
            "Castle Wall","Castle Floor","Castle Gate"};
        attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(1, 1, "%-16s", tn[ct.terrain]); attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(1, 18, "[%s]", bn[ct.biome]); attroff(COLOR_PAIR(CP_UI_DIM));
        if (ct.resources > 0) { attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(1, 30, "Res:%d", ct.resources); attroff(COLOR_PAIR(CP_UI_HIGH)); }
    }

    // Panel separator
    for (int y = 0; y < maxY; y++) { attron(COLOR_PAIR(CP_UI_DIM)); mvaddch(y, panelX-1, '|'); attroff(COLOR_PAIR(CP_UI_DIM)); }

    // Minimap
    attron(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD); mvprintw(0, panelX+1, "Map"); attroff(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);
    int mmW = panelW-2, mmH = std::min(g.viewH/3, 14), mmY = 1;
    for (int my = 0; my < mmH; my++) for (int mx = 0; mx < mmW; mx++) {
        int mapX = mx*MAP_W/mmW, mapY = my*MAP_H/mmH;
        char mch = ' '; int mcp = CP_FOG;
        if (g.map[mapY][mapX].explored[0]) {
            Terrain t = g.map[mapY][mapX].terrain;
            if (t==T_WATER||t==T_SHALLOWS)              { mch='~'; mcp=CP_MM_WATER;  }
            else if (t==T_MOUNTAIN||t==T_STONE)          { mch='^'; mcp=CP_MM_MTN;   }
            else if (t==T_FOREST||t==T_PINE||t==T_PALM)  { mch='.'; mcp=CP_MM_FOREST;}
            else if (t==T_GOLD)                           { mch='$'; mcp=CP_MM_GOLD;  }
            else if (t==T_SAND||t==T_DUNES)               { mch='.'; mcp=CP_MM_SAND;  }
            else if (t==T_SNOW||t==T_ICE)                 { mch='.'; mcp=CP_MM_SNOW;  }
            else if (t==T_CASTLE_WALL||t==T_CASTLE_GATE)  { mch='#'; mcp=CP_MM_CASTLE;}
            else { mch='.'; mcp=CP_FOG; }
        }
        if (g.map[mapY][mapX].visible[0]) {
            Entity* ent = entityAt(mapX, mapY);
            // Hide cloaked enemies from the minimap as well.
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && isConcealing() && !isDetectedBy(mapX, mapY, 0)) ent = nullptr;
            if (ent && ent->alive) {
                // Mirror main-map crop/cloaking on the minimap.
                bool mmInCrop = !isBuilding(ent->type) && g.map[mapY][mapX].terrain == T_WHEAT;
                if (ent->owner != 0 && ent->owner < MAX_PLAYERS
                    && (isConcealing() || mmInCrop) && !isDetectedBy(mapX, mapY, 0))
                    ent = nullptr;
            }
            if (ent && ent->alive) {
                mch = isBuilding(ent->type) ? '#' : '*';
                if      (ent->owner == 0)            mcp = CP_MM_PLAYER;
                else if (ent->owner < MAX_PLAYERS)   mcp = CP_MM_ENEMY;
                else                                  mcp = CP_MM_ANIMAL;
            }
        }
        attron(COLOR_PAIR(mcp)); mvaddch(mmY+my, panelX+1+mx, mch); attroff(COLOR_PAIR(mcp));
    }

    // Selection info panel
    int iy = mmY + mmH + 1;
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(iy-1, panelX, '-', panelW); attroff(COLOR_PAIR(CP_UI_DIM));

    if (g.selectedIds.size() > 1) {
        // Multi-unit group summary
        int counts[6] = {0};
        for (int sid : g.selectedIds) {
            Entity* e = findEntity(sid); if (!e || !e->alive) continue;
            switch (e->type) {
            case E_PEASANT:  counts[0]++; break; case E_MILITIA:  counts[1]++; break;
            case E_ARCHER:   counts[2]++; break; case E_KNIGHT:   counts[3]++; break;
            case E_CATAPULT: counts[4]++; break; default: counts[5]++; break;
            }
        }
        attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        mvprintw(iy++, panelX+1, "Group: %d units", (int)g.selectedIds.size());
        attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_TEXT));
        // Use the entity glyph/emoji for each unit type in the group summary.
        if (counts[0]) mvprintw(iy++, panelX+1, "  %s x%d Peasant",  getEntityEmoji(E_PEASANT),  counts[0]);
        if (counts[1]) mvprintw(iy++, panelX+1, "  %s x%d Militia",  getEntityEmoji(E_MILITIA),  counts[1]);
        if (counts[2]) mvprintw(iy++, panelX+1, "  %s x%d Archer",   getEntityEmoji(E_ARCHER),   counts[2]);
        if (counts[3]) mvprintw(iy++, panelX+1, "  %s x%d Knight",   getEntityEmoji(E_KNIGHT),   counts[3]);
        if (counts[4]) mvprintw(iy++, panelX+1, "  %s x%d Catapult", getEntityEmoji(E_CATAPULT), counts[4]);
        if (counts[5]) mvprintw(iy++, panelX+1, "  + x%d Other",    counts[5]);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        iy++;
        attron(COLOR_PAIR(CP_UI_ACCENT));
        mvprintw(iy++, panelX+1, "[Enter] Move/Attack");
        mvprintw(iy++, panelX+1, "[G] Assign to group");
        mvprintw(iy++, panelX+1, "[A] Select all mil.");
        mvprintw(iy++, panelX+1, "[1-9] Groups");
        attroff(COLOR_PAIR(CP_UI_ACCENT));
    } else {
        Entity* sel = findEntity(g.selectedId);
        if (sel) {
            auto& st = STATS[sel->type];
            int nc = (sel->owner == 0) ? CP_PLAYER : CP_ENEMY;
            attron(COLOR_PAIR(nc)|A_BOLD); mvprintw(iy++, panelX+1, "%-20s", st.name); attroff(COLOR_PAIR(nc)|A_BOLD);
            int barW = panelW-4, filled = sel->hp * barW / std::max(1, sel->maxHp);
            int pct = sel->hp * 100 / std::max(1, sel->maxHp);
            int hc = (pct>60) ? CP_HP_GREEN : (pct>30) ? CP_HP_YELLOW : CP_HP_RED;
            mvprintw(iy, panelX+1, "HP");
            for (int i = 0; i < barW; i++) {
                int c = (i < filled) ? hc : CP_FOG;
                attron(COLOR_PAIR(c)); mvaddch(iy, panelX+3+i, (i<filled)?'|':'-'); attroff(COLOR_PAIR(c));
            }
            iy++;
            attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(iy++, panelX+1, "%d / %d", sel->hp, sel->maxHp); attroff(COLOR_PAIR(CP_UI_TEXT));
            if (isUnit(sel->type)) {
                attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(iy++, panelX+1, "ATK %-3d  RNG %-2d", st.atk, st.range); attroff(COLOR_PAIR(CP_UI_TEXT));
                std::string stDesc;
                if (sel->type == E_PEASANT) {
                    switch (sel->state) {
                    case S_IDLE:      stDesc = "Idle"; break;
                    case S_MOVING:    stDesc = "Moving"; break;
                    case S_ATTACKING: stDesc = "Fighting"; break;
                    case S_GATHERING:
                        if      (sel->gatherType == 0) stDesc = "Mining gold";
                        else if (sel->gatherType == 1) stDesc = "Chopping wood";
                        else                           stDesc = "Picking berries";
                        break;
                    case S_BUILDING:  { Entity* b = findEntity(sel->targetId);
                                        if (b && !b->underConstruction && b->type==E_FARM)
                                            stDesc = "Tending farm";
                                        else
                                            stDesc = b ? (std::string("Building ") + STATS[b->type].name) : "Building";
                                        break; }
                    case S_RETURNING:
                        if      (sel->gatherType == 0) stDesc = "Carrying gold";
                        else if (sel->gatherType == 1) stDesc = "Carrying wood";
                        else                           stDesc = "Carrying food";
                        break;
                    default:          stDesc = "Idle"; break;
                    }
                } else {
                    stDesc = stateName(sel->state);
                }
                attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(iy++, panelX+1, "%s", stDesc.c_str()); attroff(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->carrying > 0) {
                    const char* what = (sel->gatherType==0) ? "gold"
                                     : (sel->gatherType==1) ? "wood" : "food";
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Carrying: %d %s", sel->carrying, what);
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                }
                if (sel->type == E_TREBUCHET && sel->owner == 0) {
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    if      (sel->packTicks > 0) mvprintw(iy++, panelX+1, "%s... %d", sel->packed?"Packing":"Deploying", sel->packTicks);
                    else if (sel->packed)        mvprintw(iy++, panelX+1, "Packed (D to deploy)");
                    else                          mvprintw(iy++, panelX+1, "Deployed (D to pack)");
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                }
                // Transport cargo display + unload hint
                if (sel->type == E_TRANSPORT && sel->owner == 0) {
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Cargo: %d/%d", (int)sel->garrison.size(), garrisonCap(E_TRANSPORT));
                    attroff(COLOR_PAIR(CP_UI_HIGH));
                    attron(COLOR_PAIR(CP_UI_ACCENT));
                    mvprintw(iy++, panelX+1, "[U] Unload");
                    attroff(COLOR_PAIR(CP_UI_ACCENT));
                }
            }
            if (sel->producing != E_NONE) {
                iy++;
                int pp = sel->prodProgress * 100 / std::max(1, sel->prodTime);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Training: %s", STATS[sel->producing].name);
                int pb = panelW-4, pf = pp*pb/100;
                for (int i = 0; i < pb; i++) { int c=(i<pf)?CP_UI_HIGH:CP_FOG; attron(COLOR_PAIR(c)); mvaddch(iy, panelX+1+i, (i<pf)?'=':'-'); attroff(COLOR_PAIR(c)); }
                iy++; mvprintw(iy++, panelX+1, "%d%%", pp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            if (!sel->queue.empty()) {
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Queue: %d", (int)sel->queue.size());
                int n = std::min((int)sel->queue.size(), panelW-4);
                int qStep = (displayMode == DM_EMOJI) ? 2 : 1;
                for (int i = 0; i < n; i++) {
                    if (displayMode == DM_ASCII)
                        mvaddch(iy, panelX+1+i*qStep, STATS[(EntityType)sel->queue[i]].glyph);
                    else
                        mvprintw(iy, panelX+1+i*qStep, "%s", getEntityEmoji(sel->queue[i]));
                }
                iy++;
                attroff(COLOR_PAIR(CP_UI_DIM));
            }
            if (sel->researching != 0) {
                iy++;
                int pp = sel->prodProgress * 100 / std::max(1, sel->prodTime);
                const char* rn = (sel->researching == R_IRON_WEAPONS)  ? "Iron Weapons" :
                                 (sel->researching == R_CROSSBOWS)    ? "Crossbows"    :
                                 (sel->researching == R_PIKES)        ? "Pikes"        :
                                 (sel->researching == R_COUNTERWEIGHT)? "Counterweight":
                                 (sel->researching == R_PLATE_HELM)   ? "Plate Helm"   : "Research";
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Researching: %s", rn);
                int pb = panelW-4, pf = pp*pb/100;
                for (int i = 0; i < pb; i++) { int c=(i<pf)?CP_UI_HIGH:CP_FOG; attron(COLOR_PAIR(c)); mvaddch(iy, panelX+1+i, (i<pf)?'=':'-'); attroff(COLOR_PAIR(c)); }
                iy++; mvprintw(iy++, panelX+1, "%d%%", pp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            if (sel->underConstruction) {
                int bp = sel->hp * 100 / std::max(1, sel->maxHp);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Building: %d%%", bp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            iy++;
            if (sel->owner == 0) {
                attron(COLOR_PAIR(CP_UI_DIM)); mvhline(iy-1, panelX, '-', panelW); attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->type == E_PEASANT) { mvprintw(iy++, panelX+1, "[B] Build"); mvprintw(iy++, panelX+1, "[Enter] Move/Gather"); }
                else if (isUnit(sel->type)) mvprintw(iy++, panelX+1, "[Enter] Move/Attack");
                else if (isBuilding(sel->type) && !sel->underConstruction) {
                    if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK) mvprintw(iy++, panelX+1, "[T] Train");
                    if (sel->type==E_DOCK)        mvprintw(iy++, panelX+1, "Fish drop-off");
                    if (sel->type==E_BLACKSMITH) mvprintw(iy++, panelX+1, "Speeds training");
                    if (sel->type==E_CHURCH)     mvprintw(iy++, panelX+1, "Heals nearby +Vision");
                    if (sel->type==E_MARKET)     mvprintw(iy++, panelX+1, "Passive gold income");
                    if (sel->type==E_FARM)        { mvprintw(iy++, panelX+1, "Generates food");
                                                     mvprintw(iy++, panelX+1, "Assign peasant to tend");
                                                     mvprintw(iy++, panelX+1, "Ripe: %d / 20", sel->carrying); }
                    if (sel->type==E_LUMBER_CAMP) mvprintw(iy++, panelX+1, "Wood drop-off");
                    if (sel->type==E_MINING_CAMP) mvprintw(iy++, panelX+1, "Gold drop-off");
                    if (sel->type==E_MILL)        { mvprintw(iy++, panelX+1, "Enables harvesting");
                                                     mvprintw(iy++, panelX+1, "Stored: %d food", sel->carrying);
                                                     mvprintw(iy++, panelX+1, "(lost if destroyed)"); }
                    if (sel->type==E_GATE) {
                        mvprintw(iy++, panelX+1, sel->gateOpen ? "State: Open" : "State: Closed");
                        mvprintw(iy++, panelX+1, sel->gateLocked ? "Mode: Locked" : "Mode: Auto");
                        mvprintw(iy++, panelX+1, "[O] Toggle/Lock");
                    }
                    if (sel->type==E_CASTLE)     mvprintw(iy++, panelX+1, "+15 Supply, 350 HP");
                    if (canGarrisonIn(sel->type)) {
                        mvprintw(iy++, panelX+1, "Garrison: %d/%d",
                                 (int)sel->garrison.size(), garrisonCap(sel->type));
                        mvprintw(iy++, panelX+1, "[U] Eject all");
                    }
                }
                attroff(COLOR_PAIR(CP_UI_ACCENT));
            }
        } else {
            attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy, panelX+1, "No selection"); attroff(COLOR_PAIR(CP_UI_DIM));
            iy += 2;
            if (displayMode == DM_ASCII) {
                attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend (ASCII) --"); attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_UI_TEXT));
                mvprintw(iy++, panelX+1, "$ Gold   T Oak");
                mvprintw(iy++, panelX+1, "^ Mtn    Y Pine");
                mvprintw(iy++, panelX+1, "~ Water  n Hills");
                mvprintw(iy++, panelX+1, ": Berry  %% Wheat");
                mvprintw(iy++, panelX+1, "# Castle & Ruins");
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                attron(COLOR_PAIR(CP_OWN_P0));
                mvprintw(iy++, panelX+1, "p Peasant  m Militia");
                mvprintw(iy++, panelX+1, "a Archer   k Knight");
                mvprintw(iy++, panelX+1, "c Catapult");
                attroff(COLOR_PAIR(CP_OWN_P0)); iy++;
                attron(COLOR_PAIR(CP_DEER));
                mvprintw(iy++, panelX+1, "d Deer  s Sheep");
                mvprintw(iy++, panelX+1, "w Wolf  o Boar");
                attroff(COLOR_PAIR(CP_DEER));
            } else {
                // Emoji legend: resources/useful things get emojis; decorative
                // terrain stays symbolic on biome-coloured backgrounds.
                attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend (Emoji) --"); attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_UI_TEXT));
                mvprintw(iy,   panelX+1,  "🪙 Gold");
                mvprintw(iy++, panelX+10, "🌳 Wood");
                mvprintw(iy,   panelX+1,  "🫐 Berry");
                mvprintw(iy++, panelX+10, "🌾 Wheat");
                mvprintw(iy,   panelX+1,  "≈ Water");
                mvprintw(iy++, panelX+10, "▲ Mtn");
                mvprintw(iy,   panelX+1,  "· Grass");
                mvprintw(iy++, panelX+10, "⌂ Ruins");
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
                mvprintw(iy,   panelX+1,  "🧍‍♂️ Peas");
                mvprintw(iy++, panelX+12, "🤺 Mil");
                mvprintw(iy,   panelX+1,  "🏹 Arch");
                mvprintw(iy++, panelX+12, "🐎 Cav");
                mvprintw(iy++, panelX+1,  "🛞 Catapult");
                attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD); iy++;
                attron(COLOR_PAIR(CP_UI_TEXT));
                mvprintw(iy,   panelX+1,  "🦌 Deer");
                mvprintw(iy++, panelX+10, "🐑 Sheep");
                mvprintw(iy,   panelX+1,  "🐺 Wolf");
                mvprintw(iy++, panelX+10, "🐗 Boar");
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Bg=biome; units=owner");
                attroff(COLOR_PAIR(CP_UI_DIM));
                attron(COLOR_PAIR(CP_OWN_P0)); mvprintw(iy, panelX+1, "You"); attroff(COLOR_PAIR(CP_OWN_P0));
                attron(COLOR_PAIR(CP_OWN_P1)); mvprintw(iy, panelX+5, "P2");  attroff(COLOR_PAIR(CP_OWN_P1));
                attron(COLOR_PAIR(CP_OWN_P2)); mvprintw(iy, panelX+8, "P3");  attroff(COLOR_PAIR(CP_OWN_P2));
                attron(COLOR_PAIR(CP_OWN_P3)); mvprintw(iy, panelX+11,"P4");  attroff(COLOR_PAIR(CP_OWN_P3));
                iy++;
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Sel=reversed bg");
                attroff(COLOR_PAIR(CP_UI_DIM));
            }
        }
    }

    // Bottom bars
    int botY2 = maxY-2, botY1 = maxY-1;
    attron(COLOR_PAIR(CP_UI_BAR)); mvhline(botY2, 0, ' ', maxX);
    if (g.mode == M_BUILD_SELECT)
        mvprintw(botY2, 1, " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [Esc] ");
    else if (g.mode == M_BUILD_PLACE) {
        const char* name = (g.buildPending != E_NONE) ? STATS[g.buildPending].name : "building";
        mvprintw(botY2, 1, " PLACE %s: Arrows/Mouse to position, [Enter]/Click to build, [Esc]/RClick to cancel ", name);
    }
    else if (g.mode == M_TRAIN_SELECT) {
        Entity* s2 = findEntity(g.selectedId);
        if (s2) {
            if (s2->type==E_TOWNHALL)  mvprintw(botY2, 1, " TRAIN: [P]easant(50g) [Esc] ");
            else if (s2->type==E_BARRACKS) mvprintw(botY2, 1, " TRAIN: [M]ilitia(60g) [A]rcher(70g) [S]pearman(40g) [C]atapult(150g+40w) [R]am(70g+80w) [Esc] ");
            else if (s2->type==E_STABLE)   mvprintw(botY2, 1, " TRAIN: [K]night(120g) [Esc] ");
            else if (s2->type==E_DOCK)     mvprintw(botY2, 1, " TRAIN: [B]oat(80g+50w) [W]arship(150g+80w) [T]ransport(80g+40w) [Esc] ");
            else if (s2->type==E_CASTLE)   mvprintw(botY2, 1, " TRAIN: [T]rebuchet(200g+250w) [Esc] ");
        }
    } else if (g.mode == M_WALL_DRAG) {
        if (g.dragging)
            mvprintw(botY2, 1, " WALL: Drag to cursor position — release to place  [Esc] Cancel ");
        else
            mvprintw(botY2, 1, " WALL: Click and drag to draw wall line  [Esc] Cancel ");
    } else if (g.mode == M_PAUSED) {
        attron(A_BOLD); mvprintw(botY2, 1, " PAUSED - Press [P] to resume "); attroff(A_BOLD);
    } else if (g.mode == M_GAME_OVER) {
        attron(A_BOLD);
        if (g.winner==0) mvprintw(botY2, 1, " VICTORY! The realm is yours. [Enter] New game  [Q] Quit ");
        else             mvprintw(botY2, 1, " DEFEAT! Your kingdom has fallen. [Enter] New game  [Q] Quit ");
        attroff(A_BOLD);
    } else if (g.groupAssignPending) {
        attron(A_BOLD); mvprintw(botY2, 1, " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel "); attroff(A_BOLD);
    } else if (g.mode == M_PATROL_SET) {
        mvprintw(botY2, 1, " PATROL: Move cursor + Enter (or click) to set target. [Esc] cancel ");
    } else {
        mvprintw(botY2, 1, " Spc:Sel  Enter:Cmd  Shift+RClick:Waypoint  Z:Patrol  B:Build  T:Train  A:All Mil  G:Group  P:Pause  Q:Quit ");
    }
    attroff(COLOR_PAIR(CP_UI_BAR));

    mvhline(botY1, 0, ' ', maxX);
    if (g.statusTimer > 0) {
        attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        mvprintw(botY1, 1, ">> %s", g.statusMsg.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        g.statusTimer--;
    }
    attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(botY1, maxX-12, "(%d,%d)", g.cursorX, g.cursorY); attroff(COLOR_PAIR(CP_UI_DIM));
}
