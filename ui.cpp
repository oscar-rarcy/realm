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
        case S_ROUTING:    return "ROUTING!";
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
        // Indexed by Terrain — keep in lockstep with the enum in realm.h.
        // (T_BRIDGE was missing here once: hovering a bridge read past the array.)
        const char* tn[] = {"Grassland","Tall Grass","Wildflowers","Meadow","Oak Forest","Pine Forest",
            "Palm Grove","Dead Tree","Mountain","Hills (ramp)","Stone","Deep Water","Shallows",
            "Marshland","Reed Bed","Gold Deposit","Sandy Ground","Sand Dunes","Snow Cover","Frozen Ice",
            "Bare Earth","Stone Road","Mud","Wheat Field","Berry Bush","Fish Shoal","Ancient Ruins","Gravel",
            "Lava Fissure","Volcanic Ash",
            "Castle Wall","Castle Floor","Castle Gate","Stone Bridge","Standing Stones"};
        static_assert(sizeof(tn)/sizeof(tn[0]) == (size_t)T_MONOLITH + 1,
            "terrain name table must cover every Terrain value");
        attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(1, 1, "%-16s", tn[ct.terrain]); attroff(COLOR_PAIR(CP_UI_TEXT));
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(1, 18, "[%s]", bn[ct.biome]); attroff(COLOR_PAIR(CP_UI_DIM));
        if (ct.resources > 0) { attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(1, 30, "Res:%d", ct.resources); attroff(COLOR_PAIR(CP_UI_HIGH)); }
        if (ct.elev > 0) { attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(1, 40, "Highland"); attroff(COLOR_PAIR(CP_UI_ACCENT)); }
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

    // One row of a build/train menu: key, name, and the live cost from STATS.
    auto menuRow = [&](int& row, char key, EntityType t, const char* req) {
        attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(row, panelX+1, "[%c]", key); attroff(COLOR_PAIR(CP_UI_HIGH));
        attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(row, panelX+4, "%-11.11s", STATS[t].name); attroff(COLOR_PAIR(CP_UI_TEXT));
        char cost[16] = ""; int cl = 0;
        if (STATS[t].costGold) cl += snprintf(cost+cl, sizeof(cost)-cl, "%dg", STATS[t].costGold);
        if (STATS[t].costWood) cl += snprintf(cost+cl, sizeof(cost)-cl, "%s%dw", cl?" ":"", STATS[t].costWood);
        int fc = isBuilding(t) ? 0 : trainFoodCost(t);
        if (fc)                cl += snprintf(cost+cl, sizeof(cost)-cl, "%s%df", cl?" ":"", fc);
        if (!cl) snprintf(cost, sizeof(cost), "free");
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(row, panelX+15, "%-8s", cost); attroff(COLOR_PAIR(CP_UI_DIM));
        if (req && *req) { attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(row, panelX+15+7, "%s", req); attroff(COLOR_PAIR(CP_UI_ACCENT)); }
        row++;
    };

    if (g.mode == M_BUILD_SELECT) {
        // Keys mirror the M_BUILD_SELECT switch in input.cpp — keep in sync.
        attron(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD); mvprintw(iy++, panelX+1, "BUILD"); attroff(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);
        menuRow(iy, 'H', E_HOUSE, "");      menuRow(iy, 'B', E_BARRACKS, "");
        menuRow(iy, 'S', E_STABLE, "");     menuRow(iy, 'T', E_TOWER, "");
        menuRow(iy, 'F', E_FARM, "");       menuRow(iy, 'W', E_WALL, "");
        menuRow(iy, 'G', E_GATE, "");       menuRow(iy, 'A', E_BLACKSMITH, "");
        menuRow(iy, 'C', E_CHURCH, "");     menuRow(iy, 'M', E_MARKET, "");
        menuRow(iy, 'K', E_CASTLE, "");     menuRow(iy, 'L', E_LUMBER_CAMP, "");
        menuRow(iy, 'N', E_MINING_CAMP, "");menuRow(iy, 'I', E_MILL, "");
        menuRow(iy, 'D', E_DOCK, "");       menuRow(iy, 'R', E_BRIDGE, "");
        menuRow(iy, 'Y', E_GRANARY, "");    menuRow(iy, 'V', E_TAVERN, "");
        menuRow(iy, 'O', E_WELL, "");       menuRow(iy, 'E', E_MANOR, "");
        menuRow(iy, 'U', E_STONEMASON, "");
        iy++;
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "[Esc] cancel"); attroff(COLOR_PAIR(CP_UI_DIM));
    } else if (g.mode == M_TRAIN_SELECT) {
        Entity* b = findEntity(g.selectedId);
        attron(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD); mvprintw(iy++, panelX+1, "TRAIN"); attroff(COLOR_PAIR(CP_UI_ACCENT)|A_BOLD);
        if (b) switch (b->type) {
            case E_TOWNHALL: menuRow(iy, 'P', E_PEASANT, ""); break;
            case E_BARRACKS:
                menuRow(iy, 'M', E_MILITIA, "");     menuRow(iy, 'A', E_ARCHER, "");
                menuRow(iy, 'S', E_SPEARMAN, "");    menuRow(iy, 'X', E_CROSSBOWMAN, "+smith");
                menuRow(iy, 'P', E_SAPPER, "+smith");menuRow(iy, 'C', E_CATAPULT, "");
                menuRow(iy, 'R', E_RAM, "");
                break;
            case E_STABLE: menuRow(iy, 'K', E_KNIGHT, ""); menuRow(iy, 'H', E_HUSSAR, ""); break;
            case E_CHURCH: menuRow(iy, 'M', E_MONK, ""); break;
            case E_MILL: case E_GRANARY: menuRow(iy, 'W', E_WAGON, ""); break;
            case E_CASTLE: menuRow(iy, 'T', E_TREBUCHET, ""); break;
            case E_DOCK:
                menuRow(iy, 'B', E_FISHING_BOAT, ""); menuRow(iy, 'W', E_WARSHIP, "");
                menuRow(iy, 'T', E_TRANSPORT, "");
                break;
            default: break;
        }
        iy++;
        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "[Esc] cancel"); attroff(COLOR_PAIR(CP_UI_DIM));
    } else if (g.selectedIds.size() > 1) {
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
                // Live values: research, shield-wall and ale all show here.
                attron(COLOR_PAIR(CP_UI_TEXT)); mvprintw(iy++, panelX+1, "ATK %-3d  RNG %-2d", unitAtk(*sel), unitRange(*sel)); attroff(COLOR_PAIR(CP_UI_TEXT));
                static const char* armorName[] = {"Light","Armored","Siege"};
                static const char* dmgName[]   = {"Slash","Pierce","Thrust","Crush"};
                attron(COLOR_PAIR(CP_UI_DIM));
                if (STATS[sel->type].atk > 0)
                    mvprintw(iy++, panelX+1, "%s / deals %s", armorName[armorClassOf(sel->type)], dmgName[damageTypeOf(sel->type)]);
                else
                    mvprintw(iy++, panelX+1, "%s armour", armorName[armorClassOf(sel->type)]);
                attroff(COLOR_PAIR(CP_UI_DIM));
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
                // Combat feel: a soldier's morale/stamina, veterancy, captivity.
                if (hasMorale(sel->type)) {
                    int mc = sel->morale >= 60 ? CP_HP_GREEN : (sel->morale >= 25 ? CP_HP_YELLOW : CP_HP_RED);
                    attron(COLOR_PAIR(mc));
                    mvprintw(iy++, panelX+1, "Morale %-3d Stam %-3d", sel->morale, sel->stamina);
                    attroff(COLOR_PAIR(mc));
                    if (sel->type == E_MILITIA && sel->kills >= 3) {
                        attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Veteran banner (%d kills)", sel->kills); attroff(COLOR_PAIR(CP_UI_HIGH));
                    } else if (sel->kills > 0) {
                        attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "Kills: %d", sel->kills); attroff(COLOR_PAIR(CP_UI_DIM));
                    }
                }
                if (sel->prisoner) {
                    attron(COLOR_PAIR(CP_HP_RED)|A_BOLD); mvprintw(iy++, panelX+1, "PRISONER"); attroff(COLOR_PAIR(CP_HP_RED)|A_BOLD);
                }
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
                    if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK||sel->type==E_CHURCH) mvprintw(iy++, panelX+1, "[T] Train");
                    if (sel->type==E_DOCK)        mvprintw(iy++, panelX+1, "Fish drop-off");
                    if (sel->type==E_BLACKSMITH) mvprintw(iy++, panelX+1, "Speeds training");
                    if (sel->type==E_CHURCH)     mvprintw(iy++, panelX+1, "Heals nearby +Vision");
                    if (sel->type==E_MARKET)     mvprintw(iy++, panelX+1, "Passive gold income");
                    if (sel->type==E_FARM)        { mvprintw(iy++, panelX+1, "Generates food");
                                                     mvprintw(iy++, panelX+1, "Assign peasant to tend");
                                                     mvprintw(iy++, panelX+1, "Ripe: %d / 20", sel->carrying); }
                    if (sel->type==E_LUMBER_CAMP) mvprintw(iy++, panelX+1, "Wood drop-off");
                    if (sel->type==E_MINING_CAMP) mvprintw(iy++, panelX+1, "Gold drop-off");
                    if (sel->type==E_MILL)        mvprintw(iy++, panelX+1, "Boosts farms; food store");
                    if (sel->type==E_GRANARY)     { mvprintw(iy++, panelX+1, "Deep larder (600)");
                                                     mvprintw(iy++, panelX+1, "Halves winter hunger"); }
                    if (sel->type==E_TAVERN)      { mvprintw(iy++, panelX+1, "Brews grain into ale");
                                                     mvprintw(iy++, panelX+1, "Ale-warms passing troops");
                                                     if (sel->atkCd > 0) mvprintw(iy++, panelX+1, "[R] Feast in %ds", sel->atkCd*8/100);
                                                     else                mvprintw(iy++, panelX+1, "[R] Feast (10 ale)"); }
                    if (sel->type==E_WELL)        { mvprintw(iy++, panelX+1, "Peasants heal nearby");
                                                     mvprintw(iy++, panelX+1, "Shields close buildings"); }
                    if (sel->type==E_MANOR)       mvprintw(iy++, panelX+1, "+10 supply, farm tax");
                    if (sel->type==E_STONEMASON)  { mvprintw(iy++, panelX+1, "Stone walls (2x HP)");
                                                     mvprintw(iy++, panelX+1, "Repairs from stone: %d", sel->carrying); }
                    if (sel->type==E_WATERMILL)   mvprintw(iy++, panelX+1, "Half-rate mill (claimed)");
                    if (sel->type==E_TRADING_POST) mvprintw(iy++, panelX+1, "Road toll, [R] trade");
                    if (sel->type==E_SHRINE)      mvprintw(iy++, panelX+1, "Heals the faithful nearby");
                    // Stockpile readout — anything stored here burns with the building.
                    if (isDepot(sel->type) && !sel->underConstruction) {
                        if (sel->storeGold || sel->storeWood)
                            mvprintw(iy++, panelX+1, "Stored: %dg %dw", sel->storeGold, sel->storeWood);
                        static const char* fk[] = {"Grain","Meat","Fish","Berry","Ale"};
                        for (int k = 0; k < F_COUNT; k++)
                            if (sel->storeFood[k] > 0)
                                mvprintw(iy++, panelX+1, " %s: %d", fk[k], sel->storeFood[k]);
                        if (sel->storeGold || sel->storeWood || depotFoodSum(*sel))
                            mvprintw(iy++, panelX+1, "(scatters if destroyed)");
                    }
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
        mvprintw(botY2, 1, " BUILD: press a key from the panel on the right. [Esc] cancel ");
    else if (g.mode == M_BUILD_PLACE) {
        const char* name = (g.buildPending != E_NONE) ? STATS[g.buildPending].name : "building";
        mvprintw(botY2, 1, " PLACE %s: Arrows/Mouse to position, [Enter]/Click to build, [Esc]/RClick to cancel ", name);
    }
    else if (g.mode == M_TRAIN_SELECT) {
        mvprintw(botY2, 1, " TRAIN: press a key from the panel on the right. [Esc] cancel ");
    } else if (g.mode == M_WALL_DRAG) {
        if (g.dragging)
            mvprintw(botY2, 1, " WALL: Drag to cursor position — release to place  [Esc] Cancel ");
        else
            mvprintw(botY2, 1, " WALL: Click and drag to draw wall line  [Esc] Cancel ");
    } else if (g.mode == M_PAUSED) {
        attron(A_BOLD); mvprintw(botY2, 1, " PAUSED — [S] Save / Load game    [P] resume "); attroff(A_BOLD);
    } else if (g.mode == M_SAVELOAD) {
        mvprintw(botY2, 1, " SAVE / LOAD — Up/Down or click a slot   [Enter] Load   [S] Save   [D] Delete   [Esc] Back ");
    } else if (g.mode == M_GAME_OVER) {
        attron(A_BOLD);
        if (g.winner==0) mvprintw(botY2, 1, " VICTORY! The realm is yours. [Enter] New game  [Q] Quit ");
        else             mvprintw(botY2, 1, " DEFEAT! Your kingdom has fallen. [Enter] New game  [Q] Quit ");
        attroff(A_BOLD);
    } else if (g.groupAssignPending) {
        attron(A_BOLD); mvprintw(botY2, 1, " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel "); attroff(A_BOLD);
    } else if (g.mode == M_PATROL_SET) {
        mvprintw(botY2, 1, " PATROL: Move cursor + Enter (or click) to set target. [Esc] cancel ");
    } else if (g.mode == M_HELP) {
        mvprintw(botY2, 1, " HELP — press any key to return ");
    } else {
        // Contextual hints: lead with what the current selection can DO.
        Entity* cs = findEntity(g.selectedId);
        bool group = g.selectedIds.size() > 1;
        if (group) {
            mvprintw(botY2, 1, " Enter:Move/Attack  a:Attack-move  X:Hold  s:Stop  Z:Patrol  G:Group  Shift+A:All Mil  ?:Help ");
        } else if (cs && cs->owner == 0 && cs->type == E_PEASANT) {
            mvprintw(botY2, 1, " B:Build  Enter:Move/Gather/Repair  Shift+RClick:Waypoint  ,:Next idle  ?:Help  Q:Menu ");
        } else if (cs && cs->owner == 0 && cs->type == E_WAGON) {
            mvprintw(botY2, 1, " Enter/RClick on a depot: load or deliver cargo  ?:Help  Q:Menu ");
        } else if (cs && cs->owner == 0 && cs->type == E_TREBUCHET) {
            mvprintw(botY2, 1, " D:Pack/Deploy  Enter:Move(packed)/Attack(deployed)  X:Hold  ?:Help ");
        } else if (cs && cs->owner == 0 && isUnit(cs->type)) {
            mvprintw(botY2, 1, " Enter:Move/Attack  a:Attack-move  X:Hold  Z:Patrol  G:Group  Shift+A:All Mil  ?:Help ");
        } else if (cs && cs->owner == 0 && isBuilding(cs->type) && !cs->underConstruction) {
            bool trains = (cs->type==E_TOWNHALL||cs->type==E_BARRACKS||cs->type==E_STABLE
                        || cs->type==E_DOCK||cs->type==E_CASTLE||cs->type==E_CHURCH
                        || cs->type==E_MILL||cs->type==E_GRANARY);
            bool rallies = trains || cs->type==E_BLACKSMITH || cs->type==E_MARKET
                        || cs->type==E_TAVERN || cs->type==E_TRADING_POST;
            char line[120] = " ";
            if (trains) strncat(line, "T:Train  ", sizeof(line)-strlen(line)-1);
            if (cs->type==E_BLACKSMITH) strncat(line, "R:Research  ", sizeof(line)-strlen(line)-1);
            else if (cs->type==E_MARKET||cs->type==E_TRADING_POST) strncat(line, "R:Trade  ", sizeof(line)-strlen(line)-1);
            else if (cs->type==E_TAVERN) strncat(line, "R:Feast  ", sizeof(line)-strlen(line)-1);
            else if (rallies) strncat(line, "R:Rally  ", sizeof(line)-strlen(line)-1);
            if (cs->type==E_GATE) strncat(line, "O:Open/Close  ", sizeof(line)-strlen(line)-1);
            if (canGarrisonIn(cs->type)) strncat(line, "U:Eject  ", sizeof(line)-strlen(line)-1);
            strncat(line, "?:Help  Q:Menu ", sizeof(line)-strlen(line)-1);
            mvprintw(botY2, 1, "%s", line);
        } else {
            mvprintw(botY2, 1, " Spc:Sel  Enter:Cmd  B:Build  T:Train  A:All Mil  X:Hold  Z:Patrol  G:Group  P:Pause  ?:Help  Q:Menu ");
        }
    }
    attroff(COLOR_PAIR(CP_UI_BAR));

    // ---- Help sheet: full reference, drawn over the map; sim is paused ----
    if (g.mode == M_HELP) {
        int hw = 74, hh = 26;
        int hx = std::max(0, (maxX - panelW - hw) / 2);
        int hy = std::max(1, (maxY - hh) / 2);
        attron(COLOR_PAIR(CP_UI_BAR));
        for (int r = 0; r < hh; r++) mvhline(hy+r, hx, ' ', hw);
        attron(A_BOLD); mvprintw(hy+1, hx+2, "REALM — COMMAND REFERENCE"); attroff(A_BOLD);
        int r = hy + 3, c1 = hx + 2, c2 = hx + 38;
        mvprintw(r,   c1, "SELECTION");
        mvprintw(r+1, c1, " Space/LClick   select");
        mvprintw(r+2, c1, " Drag           box-select");
        mvprintw(r+3, c1, " Dbl-click      all of type on screen");
        mvprintw(r+4, c1, " Shift+click    add/remove");
        mvprintw(r+5, c1, " Tab / ,        cycle units / idle peasant");
        mvprintw(r+6, c1, " 1-9 / G        recall / assign group");
        mvprintw(r+8, c1, "ORDERS");
        mvprintw(r+9, c1, " Enter/RClick   move attack gather tend");
        mvprintw(r+10,c1, " Shift+RClick   queue waypoint");
        mvprintw(r+11,c1, " A              attack-move (mil. selected)");
        mvprintw(r+12,c1, " s / X          stop / hold position");
        mvprintw(r+13,c1, " Z              patrol to click");
        mvprintw(r+14,c1, " D              pack/deploy trebuchet");
        mvprintw(r+15,c1, " U / O          eject garrison / gate mode");
        mvprintw(r,   c2, "PRODUCTION");
        mvprintw(r+1, c2, " B   build menu (peasant)");
        mvprintw(r+2, c2, " T   train menu (building)");
        mvprintw(r+3, c2, " R   rally / research / trade");
        mvprintw(r+5, c2, "CAMERA");
        mvprintw(r+6, c2, " Arrows, Shift+arrows fast");
        mvprintw(r+7, c2, " Mouse edge-scroll, minimap");
        mvprintw(r+8, c2, " h   jump to town hall");
        mvprintw(r+10,c2, "GAME");
        mvprintw(r+11,c2, " P        pause + Save/Load menu");
        mvprintw(r+12,c2, " F5-F8    quick-save slots 1-4");
        mvprintw(r+13,c2, " F9-F12   quick-load slots 1-4");
        mvprintw(r+14,c2, " Q Q      abandon to menu");
        mvprintw(r+15,c2, " Shift+S  reveal map (debug)");
        mvprintw(hy+hh-2, c1, "Terrain: ramps 'n' climb cliffs '#'. High ground: +sight, +ranged dmg.");
        attroff(COLOR_PAIR(CP_UI_BAR));
    }

    // ---- Visual Save / Load menu: slot cards with in-game date + timestamp ----
    if (g.mode == M_SAVELOAD) {
        const int rowH = 3, hw = 60, hh = 6 + NUM_SAVE_SLOTS * rowH;
        int hx = std::max(0, (maxX - panelW - hw) / 2);
        int hy = std::max(1, (maxY - hh) / 2);
        attron(COLOR_PAIR(CP_UI_BAR));
        for (int rr = 0; rr < hh; rr++) mvhline(hy+rr, hx, ' ', hw);
        attron(A_BOLD); mvprintw(hy+1, hx+2, "SAVE  /  LOAD"); attroff(A_BOLD);
        int rowY0 = hy + 3;
        // Stash geometry so input.cpp can hit-test mouse clicks onto slot rows.
        g.slMenuX = hx; g.slMenuW = hw; g.slMenuRowY0 = rowY0; g.slMenuRowH = rowH;
        static const char* seasons[] = {"Spring","Summer","Autumn","Winter"};
        for (int s = 0; s < NUM_SAVE_SLOTS; s++) {
            int ry = rowY0 + s * rowH;
            bool sel = (s == g.saveSlotSel);
            char path[64]; saveSlotPath(s+1, path, sizeof(path));
            SaveSlotInfo info; bool used = peekSave(path, info);
            if (sel) attron(A_REVERSE);
            mvprintw(ry, hx+2, " %s Slot %d%s",
                     sel ? ">" : " ", s+1, (s==0 ? "  (quicksave)" : ""));
            if (used) {
                char when[40] = "";
                time_t t = (time_t)info.saveTime;
                struct tm* lt = localtime(&t);
                if (lt) strftime(when, sizeof(when), "%b %d  %H:%M", lt);
                mvprintw(ry+1, hx+6, "Year %d, %-6s          saved %s",
                         info.year, seasons[info.season & 3], when);
            } else {
                mvprintw(ry+1, hx+6, "- empty -");
            }
            if (sel) attroff(A_REVERSE);
        }
        mvprintw(hy+hh-2, hx+2, "Up/Down or click: pick   Enter: Load   S: Save   D: Delete   Esc: Back");
        attroff(COLOR_PAIR(CP_UI_BAR));
    }

    mvhline(botY1, 0, ' ', maxX);
    if (g.statusTimer > 0) {
        attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        mvprintw(botY1, 1, ">> %s", g.statusMsg.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        g.statusTimer--;
    }
    attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(botY1, maxX-12, "(%d,%d)", g.cursorX, g.cursorY); attroff(COLOR_PAIR(CP_UI_DIM));
}
