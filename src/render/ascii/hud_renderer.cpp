#include "realm.h"
#include "view_state.h"
#include "input_keys.h"

void renderUI(const WorldIndex& world) {
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
    if (getBrightness(g) > 0.5f) {
        attron(COLOR_PAIR(CP_SUN)|A_BOLD);
        // Emoji mode: ✦ (U+2726 BLACK FOUR POINTED STAR, width-1) for sun.
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "\xe2\x9c\xa6" : "*");
        attroff(COLOR_PAIR(CP_SUN)|A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_MOON));
        // Emoji mode: ◐ (U+25D0 CIRCLE WITH LEFT HALF BLACK, width-1) for moon.
        mvprintw(0, iconX, (displayMode == DM_EMOJI) ? "\xe2\x97\x90" : "o");
        attroff(COLOR_PAIR(CP_MOON));
    }
    attron(COLOR_PAIR(CP_UI_BAR));
    const char* wn = (g.weather == W_STORM) ? "Storm" : (g.weather == W_RAIN) ? "Rain " : (g.weather == W_SNOW) ? "Snow " : "Clear";
    mvprintw(0, iconX+1, " %-5s %-6s %s", getTimeName(g), getSeasonName(g), wn);
    attroff(COLOR_PAIR(CP_UI_BAR));

    // Terrain info bar
    attron(COLOR_PAIR(CP_UI_DIM)); mvhline(1, 0, '-', view.viewW); attroff(COLOR_PAIR(CP_UI_DIM));
    if (inBounds(view.cursorX, view.cursorY) && g.map[view.cursorY][view.cursorX].explored[0]) {
        Tile& ct = g.map[view.cursorY][view.cursorX];
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
    int mmW = panelW-2, mmH = std::min(view.viewH/3, 14), mmY = 1;
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
            Entity* ent = entityAt(g, world, mapX, mapY);
            // Hide cloaked enemies from the minimap as well.
            if (ent && ent->alive && ent->owner != 0 && ent->owner < MAX_PLAYERS
                && isConcealing(g) && !isDetectedBy(g, mapX, mapY, 0)) ent = nullptr;
            if (ent && ent->alive) {
                // Mirror main-map crop/cloaking on the minimap.
                bool mmInCrop = !isBuilding(ent->type) && g.map[mapY][mapX].terrain == T_WHEAT;
                if (ent->owner != 0 && ent->owner < MAX_PLAYERS
                    && (isConcealing(g) || mmInCrop) && !isDetectedBy(g, mapX, mapY, 0))
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
    if (inBounds(view.cursorX, view.cursorY)) {
        const Tile& ct = g.map[view.cursorY][view.cursorX];
        attron(COLOR_PAIR(CP_UI_TEXT));
        mvprintw(iy++, panelX+1, "Tile: %.14s", terrainName(ct.terrain));
        mvprintw(iy++, panelX+1, "Biome: %.13s", biomeName(ct.biome));
        if (ct.resources > 0) mvprintw(iy++, panelX+1, "Resource: %d", ct.resources);
        int stack = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.state == S_GARRISONED) continue;
            auto& st = STATS[e.type];
            bool covers = st.isBuilding
                ? (view.cursorX>=e.x && view.cursorX<e.x+st.sizeW && view.cursorY>=e.y && view.cursorY<e.y+st.sizeH)
                : (view.cursorX==e.x && view.cursorY==e.y);
            if (!covers) continue;
            if (stack == 0) mvprintw(iy++, panelX+1, "Stack:");
            if (stack < 3) mvprintw(iy++, panelX+2, "%.16s", st.name);
            stack++;
        }
        if (stack == 0) mvprintw(iy++, panelX+1, "Stack: empty");
        else if (stack > 3) mvprintw(iy++, panelX+2, "+%d more", stack - 3);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        if (g.local.diagnostics) {
            attron(COLOR_PAIR(CP_UI_HIGH));
            mvprintw(iy++, panelX+1, "Diag T%d M:%s", g.tick, modeName(g.mode));
            mvprintw(iy++, panelX+1, "Ent:%d Proj:%d", (int)g.entities.size(), (int)g.projectiles.size());
            mvprintw(iy++, panelX+1, "Seed:%u AI:%d", g.seed, g.startupAIs);
            attroff(COLOR_PAIR(CP_UI_HIGH));
        }
        iy++;
    }

    if (g.local.selectedIds.size() > 1) {
        // Multi-unit group summary
        int counts[8] = {0};
        for (int sid : g.local.selectedIds) {
            Entity* e = findEntity(g, world, sid); if (!e || !e->alive) continue;
            switch (e->type) {
            case E_PEASANT:  counts[0]++; break; case E_MILITIA:  counts[1]++; break;
            case E_ARCHER:   counts[2]++; break; case E_KNIGHT:   counts[3]++; break;
            case E_SPEARMAN: counts[4]++; break; case E_CATAPULT: counts[5]++; break;
            case E_TREBUCHET: counts[6]++; break; default: counts[7]++; break;
            }
        }
        attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        mvprintw(iy++, panelX+1, "Group: %d units", (int)g.local.selectedIds.size());
        attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
        attron(COLOR_PAIR(CP_UI_TEXT));
        // Use the entity glyph/emoji for each unit type in the group summary.
        if (counts[0]) mvprintw(iy++, panelX+1, "  %s x%d Peasant",  getEntityEmoji(E_PEASANT),  counts[0]);
        if (counts[1]) mvprintw(iy++, panelX+1, "  %s x%d Militia",  getEntityEmoji(E_MILITIA),  counts[1]);
        if (counts[2]) mvprintw(iy++, panelX+1, "  %s x%d Archer",   getEntityEmoji(E_ARCHER),   counts[2]);
        if (counts[3]) mvprintw(iy++, panelX+1, "  %s x%d Knight",   getEntityEmoji(E_KNIGHT),   counts[3]);
        if (counts[4]) mvprintw(iy++, panelX+1, "  %s x%d Spearman", getEntityEmoji(E_SPEARMAN), counts[4]);
        if (counts[5]) mvprintw(iy++, panelX+1, "  %s x%d Catapult", getEntityEmoji(E_CATAPULT), counts[5]);
        if (counts[6]) mvprintw(iy++, panelX+1, "  %s x%d Trebuchet", getEntityEmoji(E_TREBUCHET), counts[6]);
        if (counts[7]) mvprintw(iy++, panelX+1, "  + x%d Other",    counts[7]);
        attroff(COLOR_PAIR(CP_UI_TEXT));
        iy++;
        attron(COLOR_PAIR(CP_UI_ACCENT));
        mvprintw(iy++, panelX+1, "[Enter] Move/Attack");
        mvprintw(iy++, panelX+1, "[G] Assign to group");
        mvprintw(iy++, panelX+1, "[A] Select all mil.");
        mvprintw(iy++, panelX+1, "[1-9] Groups");
        attroff(COLOR_PAIR(CP_UI_ACCENT));
    } else {
        Entity* sel = findEntity(g, world, g.local.selectedId);
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
                        if      (sel->cargo.type == CR_GOLD) stDesc = "Mining gold";
                        else if (sel->cargo.type == CR_WOOD) stDesc = "Chopping wood";
                        else if (sel->cargo.type == CR_FISH) stDesc = "Fishing";
                        else                                stDesc = "Picking berries";
                        break;
                    case S_BUILDING:  { Entity* b = findEntity(g, world, sel->targetId);
                                        if (b && !b->underConstruction && b->type==E_FARM)
                                            stDesc = "Tending farm";
                                        else
                                            stDesc = b ? (std::string("Building ") + STATS[b->type].name) : "Building";
                                        break; }
                    case S_RETURNING:
                        if      (sel->cargo.type == CR_GOLD) stDesc = "Carrying gold";
                        else if (sel->cargo.type == CR_WOOD) stDesc = "Carrying wood";
                        else if (sel->cargo.type == CR_FISH) stDesc = "Carrying fish";
                        else                                stDesc = "Carrying food";
                        break;
                    default:          stDesc = "Idle"; break;
                    }
                } else {
                    stDesc = stateName(sel->state);
                }
                attron(COLOR_PAIR(CP_UI_ACCENT)); mvprintw(iy++, panelX+1, "%s", stDesc.c_str()); attroff(COLOR_PAIR(CP_UI_ACCENT));
                if (sel->cargo.amount > 0) {
                    const char* what = cargoResourceName(sel->cargo.type);
                    attron(COLOR_PAIR(CP_UI_HIGH));
                    mvprintw(iy++, panelX+1, "Carrying: %d %s", sel->cargo.amount, what);
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
                int pp = sel->trainProgress * 100 / std::max(1, sel->trainTime);
                attron(COLOR_PAIR(CP_UI_HIGH)); mvprintw(iy++, panelX+1, "Training: %s", STATS[sel->producing].name);
                int pb = panelW-4, pf = pp*pb/100;
                for (int i = 0; i < pb; i++) { int c=(i<pf)?CP_UI_HIGH:CP_FOG; attron(COLOR_PAIR(c)); mvaddch(iy, panelX+1+i, (i<pf)?'=':'-'); attroff(COLOR_PAIR(c)); }
                iy++; mvprintw(iy++, panelX+1, "%d%%", pp); attroff(COLOR_PAIR(CP_UI_HIGH));
            }
            if (!sel->queue.empty()) {
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Queue: %d", (int)sel->queue.size());
                int n = std::min((int)sel->queue.size(), panelW-4);
                for (int i = 0; i < n; i++) {
                    // Each queue slot is one map-cell-width apart regardless of mode.
                    if (displayMode == DM_ASCII)
                        mvaddch(iy, panelX+1+i, STATS[(EntityType)sel->queue[i]].glyph);
                    else
                        mvprintw(iy, panelX+1+i, "%s", getEntityEmoji(sel->queue[i]));
                }
                iy++;
                attroff(COLOR_PAIR(CP_UI_DIM));
            }
            if (sel->researching != 0) {
                iy++;
                int pp = sel->researchProgress * 100 / std::max(1, sel->researchTime);
                const char* rn = (sel->researching == R_IRON_WEAPONS) ? "Iron Weapons" :
                                 (sel->researching == R_CROSSBOWS)   ? "Crossbows"    :
                                 (sel->researching == R_PIKES)       ? "Pikes"        :
                                 (sel->researching == R_COUNTERWEIGHT) ? "Counterweight" :
                                 (sel->researching == R_PLATE_HELM)  ? "Plate Helm"   : "Research";
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
                    if (sel->type==E_TOWNHALL||sel->type==E_BARRACKS||sel->type==E_STABLE||sel->type==E_DOCK||sel->type==E_CASTLE) mvprintw(iy++, panelX+1, "[T] Train");
                    if (sel->type==E_DOCK)        mvprintw(iy++, panelX+1, "Fish drop-off");
                    if (sel->type==E_BLACKSMITH) mvprintw(iy++, panelX+1, "Speeds training");
                    if (sel->type==E_CHURCH)     mvprintw(iy++, panelX+1, "Heals nearby +Vision");
                    if (sel->type==E_MARKET)     { mvprintw(iy++, panelX+1, "Passive gold income");
                                                     mvprintw(iy++, panelX+1, "[R] Trade"); }
                    if (sel->type==E_FARM)        { mvprintw(iy++, panelX+1, "Generates food");
                                                     mvprintw(iy++, panelX+1, "Assign peasant to tend");
                                                     mvprintw(iy++, panelX+1, "Ripe: %d / 20", sel->storedFood); }
                    if (sel->type==E_LUMBER_CAMP) mvprintw(iy++, panelX+1, "Wood drop-off");
                    if (sel->type==E_MINING_CAMP) mvprintw(iy++, panelX+1, "Gold drop-off");
                    if (sel->type==E_MILL)        { mvprintw(iy++, panelX+1, "Enables harvesting");
                                                     mvprintw(iy++, panelX+1, "Stored: %d food", sel->storedFood);
                                                     mvprintw(iy++, panelX+1, "(lost if destroyed)"); }
                    if (sel->type==E_GATE) {
                        mvprintw(iy++, panelX+1, sel->gateOpen ? "State: Open" : "State: Closed");
                        mvprintw(iy++, panelX+1, sel->gateLocked ? "Mode: Locked" : "Mode: Auto");
                        mvprintw(iy++, panelX+1, "[O] Toggle/Lock");
                    }
                    if (sel->type==E_CASTLE)     { mvprintw(iy++, panelX+1, "+15 Supply, 350 HP");
                                                     mvprintw(iy++, panelX+1, "[T] Peasants/Trebuchets"); }
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
                // Emoji legend: show Unicode symbols + ownership colour key.
                attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(iy++, panelX+1, "-- Legend (Emoji) --"); attroff(COLOR_PAIR(CP_UI_DIM));
                // Terrain (two per row, explicit x for column 2)
                attron(COLOR_PAIR(CP_UI_TEXT));
                // Use mvprintw at explicit positions so multi-byte UTF-8 doesn't
                // misalign ncurses' internal cursor for the right-column label.
                mvprintw(iy,   panelX+1,  "\xe2\x99\xa6 Gold");     // ♦
                mvprintw(iy++, panelX+10, "\xe2\x99\xa3 Oak");      // ♣
                mvprintw(iy,   panelX+1,  "\xe2\x96\xb2 Mtn");      // ▲
                mvprintw(iy++, panelX+10, "\xe2\x86\x91 Pine");      // ↑
                mvprintw(iy,   panelX+1,  "\xe2\x89\x88 Water");     // ≈
                mvprintw(iy++, panelX+10, "\xe2\x88\xa9 Hills");     // ∩
                mvprintw(iy,   panelX+1,  "\xe2\x88\xb7 Berry");     // ∷
                mvprintw(iy++, panelX+10, "\xc2\xa7 Wheat");         // §
                mvprintw(iy,   panelX+1,  "\xe2\x96\xa0 Wall");      // ■
                mvprintw(iy++, panelX+10, "\xc2\xb6 Ruins");         // ¶
                attroff(COLOR_PAIR(CP_UI_TEXT)); iy++;
                // Military units with player-colour background
                attron(COLOR_PAIR(CP_OWN_P0)|A_BOLD);
                mvprintw(iy,   panelX+1,  "\xe2\x99\x9f Peasant");   // ♟
                mvprintw(iy++, panelX+12, "\xe2\x99\x99 Militia");   // ♙
                mvprintw(iy,   panelX+1,  "\xe2\x99\x97 Archer");    // ♝
                mvprintw(iy++, panelX+12, "\xe2\x99\x9e Knight");    // ♞
                mvprintw(iy++, panelX+1,  "\xe2\x8a\x99 Catapult");  // ⊙
                attroff(COLOR_PAIR(CP_OWN_P0)|A_BOLD); iy++;
                // Animals (neutral — no ownership background)
                attron(COLOR_PAIR(CP_DEER));
                mvprintw(iy,   panelX+1,  "\xe2\x96\xb7 Deer");      // ▷
                mvprintw(iy++, panelX+10, "\xe2\x97\x8c Sheep");     // ◌
                mvprintw(iy,   panelX+1,  "\xe2\x97\x81 Wolf");      // ◁
                mvprintw(iy++, panelX+10, "\xe2\x97\x8f Boar");      // ●
                attroff(COLOR_PAIR(CP_DEER)); iy++;
                // Ownership colour key
                attron(COLOR_PAIR(CP_UI_DIM));
                mvprintw(iy++, panelX+1, "Bg=owner colour:");
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
        mvprintw(botY2, 1, " BUILD: [H]ouse [B]arracks [S]table [T]ower [F]arm [W]all [G]ate [A]rmory [C]hurch [M]arket [K]Castle [L]umber [N]mine [I]mill [D]ock [J]wood bridge [V]stone bridge [Esc] ");
    else if (g.mode == M_BUILD_PLACE) {
        const char* name = (g.local.buildPending != E_NONE) ? STATS[g.local.buildPending].name : "building";
        mvprintw(botY2, 1, " PLACE %s: Arrows/Mouse, [Enter]/Click to build, [Esc]/RClick cancel ", name);
    }
    else if (g.mode == M_TRAIN_SELECT) {
        Entity* s2 = findEntity(g, world, g.local.selectedId);
        if (s2) {
            if (s2->type==E_TOWNHALL)  mvprintw(botY2, 1, " TRAIN: [P]easant(50g), repeat keys to queue [Esc] ");
            else if (s2->type==E_BARRACKS) mvprintw(botY2, 1, " TRAIN: [M]ilitia [A]rcher [S]pearman [C]atapult [R]am, repeat keys to queue [Esc] ");
            else if (s2->type==E_STABLE)   mvprintw(botY2, 1, " TRAIN: [K]night, repeat keys to queue [Esc] ");
            else if (s2->type==E_DOCK)     mvprintw(botY2, 1, " TRAIN: [B]oat [W]arship [T]ransport, repeat keys to queue [Esc] ");
            else if (s2->type==E_CASTLE)   mvprintw(botY2, 1, " TRAIN: [P]easant [T]rebuchet, repeat keys to queue [Esc] ");
        }
    } else if (g.mode == M_MARKET_TRADE) {
        mvprintw(botY2, 1, " TRADE: [G]40g->30w [W]40w->30g [F]50g->30f [V]40f->30g [Esc] ");
    } else if (g.mode == M_WALL_DRAG) {
        if (view.dragging)
            mvprintw(botY2, 1, " WALL: Drag to cursor position — release to place  [Esc] Cancel ");
        else
            mvprintw(botY2, 1, " WALL: Click and drag to draw wall line  [Esc] Cancel ");
    } else if (g.mode == M_PAUSED) {
        attron(A_BOLD); mvprintw(botY2, 1, " PAUSED - Press [P] to resume "); attroff(A_BOLD);
    } else if (g.mode == M_GAME_OVER) {
        attron(A_BOLD);
        if (g.winner==0) mvprintw(botY2, 1, " VICTORY! The realm is yours. [Enter/Q] Main menu  [X] Exit ");
        else             mvprintw(botY2, 1, " DEFEAT! Your kingdom has fallen. [Enter/Q] Main menu  [X] Exit ");
        attroff(A_BOLD);
    } else if (g.local.groupAssignPending) {
        attron(A_BOLD); mvprintw(botY2, 1, " GROUP ASSIGN: Press [1]-[9] to assign selection to group, [Esc] to cancel "); attroff(A_BOLD);
    } else if (g.mode == M_PATROL_SET) {
        mvprintw(botY2, 1, " PATROL: Move cursor + Enter or click target. [Esc] cancel ");
    } else {
        mvprintw(botY2, 1, " Arrows:Move Spc:Select Enter:Cmd Shift+RClick:Waypoint Z:Patrol B:Build T:Train ?:Help V:Save Q:Resign ");
    }
    attroff(COLOR_PAIR(CP_UI_BAR));

    mvhline(botY1, 0, ' ', maxX);
    if (ui.statusTimer > 0) {
        attron(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
        mvprintw(botY1, 1, ">> %s", ui.statusMsg.c_str());
        attroff(COLOR_PAIR(CP_UI_HIGH)|A_BOLD);
    }
    attron(COLOR_PAIR(CP_UI_DIM)); mvprintw(botY1, maxX-12, "(%d,%d)", view.cursorX, view.cursorY); attroff(COLOR_PAIR(CP_UI_DIM));
}


