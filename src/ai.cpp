#include "realm.h"

#include <cmath>

// ============================================================
// AI — economy + military build-out, target picking, wave dispatch
// ============================================================
int     aiCount(int o, EntityType t)    { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)c++; return c; }
int     aiCountAll(int o, EntityType t) { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t)c++;                    return c; }
Entity* aiIdle(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&e.state==S_IDLE&&!e.underConstruction)return &e; return nullptr; }
Entity* aiBldg(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)return &e; return nullptr; }

// Worker selection for AI construction. Prefers idle peasants; if none exist,
// pulls one off gathering/returning so the AI doesn't deadlock at 10/10
// population because aiGather() consumed every idle worker before the build
// pass ran.
static Entity* aiWorker(int o) {
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && isWorker(e.type)
            && e.state == S_IDLE && !e.underConstruction) return &e;
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && isWorker(e.type)
            && !e.underConstruction
            && (e.state == S_GATHERING || e.state == S_RETURNING)) return &e;
    return nullptr;
}

static Entity* aiIdlePeasant(int o) {
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && isWorker(e.type)
            && e.state == S_IDLE && !e.underConstruction) return &e;
    return nullptr;
}

void aiGather(int o) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner!=o || !canGather(e.type) || e.state!=S_IDLE) continue;
        int bestD = 9999, bx = -1, by = -1;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Terrain t = g.map[y][x].terrain;
            bool isR = (t==T_GOLD||t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE||t==T_BERRY);
            if (isR && g.map[y][x].resources > 0) {
                int d = mdist(e.x, e.y, x, y);
                if (d < bestD) { bestD=d; bx=x; by=y; }
            }
        }
        if (bx >= 0) orderGather(e, bx, by);
    }
}

// Build placement: try near a given centre. Prefer positions with breathing
// room from existing owned buildings so the base spreads instead of clumping.
static void aiBuildSpotNear(int o, EntityType bt, int cx, int cy, int& ox, int& oy) {
    if (cx < 0 || cy < 0) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (!th) th = aiBldg(o, E_CASTLE);
        if (!th) return;
        cx = th->x; cy = th->y;
    }
    for (int r = 4; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (realmRand()%(r*2+1)) - r, by = cy + (realmRand()%(r*2+1)) - r;
        if (!canPlace(bt, bx, by, o)) continue;
        bool tooClose = false;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != o || !isBuilding(e.type)) continue;
            if (dist(e.x, e.y, bx, by) < 3) { tooClose = true; break; }
        }
        if (!tooClose) { ox = bx; oy = by; return; }
    }
    for (int r = 2; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (realmRand()%(r*2+1)) - r, by = cy + (realmRand()%(r*2+1)) - r;
        if (canPlace(bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}
void aiBuildSpot(int o, EntityType bt, int& ox, int& oy) { aiBuildSpotNear(o, bt, -1, -1, ox, oy); }

static void aiBuildSpotWide(int o, EntityType bt, int& ox, int& oy) {
    Entity* th = aiBldg(o, E_TOWNHALL);
    if (!th) th = aiBldg(o, E_CASTLE);
    if (!th) return;
    for (int r = 5; r < 28; r++) for (int a = 0; a < 32; a++) {
        int bx = th->x + (realmRand()%(r*2+1)) - r, by = th->y + (realmRand()%(r*2+1)) - r;
        if (canPlace(bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}

// Scan player state — used to scale production and pick targets.
struct AIIntel { int playerArmy; int playerCastles; int playerWalls; int playerPeasants; int playerCatapults; Entity* playerTH; };
static AIIntel aiScout(int o) {
    AIIntel x{0,0,0,0,0,nullptr};
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (isWorker(e.type)) x.playerPeasants++;
        else if (e.type == E_CATAPULT || e.type == E_TREBUCHET) { x.playerCatapults++; x.playerArmy++; }
        else if (isUnit(e.type)) x.playerArmy++;
        else if (e.type == E_CASTLE)  { x.playerCastles++; if (!x.playerTH) x.playerTH = &e; }
        else if (e.type == E_WALL)    x.playerWalls++;
        else if (e.type == E_TOWNHALL && !x.playerTH) x.playerTH = &e;
    }
    return x;
}

static Entity* aiNearestEnemyBuilding(int o, int x, int y) {
    Entity* best = nullptr; int bestD = 99999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (!isBuilding(e.type)) continue;
        int d = dist(x, y, e.x, e.y);
        if (d < bestD) { bestD = d; best = &e; }
    }
    return best;
}

static bool findShoreTileNear(int cx, int cy, int searchR, int& ox, int& oy) {
    for (int r = 1; r <= searchR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny) || !isPassableWater(nx,ny)) continue;
            for (int dy2 = -1; dy2 <= 1; dy2++) for (int dx2 = -1; dx2 <= 1; dx2++) {
                if (dx2 == 0 && dy2 == 0) continue;
                int qx = nx+dx2, qy = ny+dy2;
                if (inBounds(qx,qy) && isPassable(qx,qy)) { ox = nx; oy = ny; return true; }
            }
        }
    }
    return false;
}

static void aiTickTrebuchets(int o) {
    int rng = STATS[E_TREBUCHET].range;
    for (auto& t : g.entities) {
        if (!t.alive || t.owner != o || t.type != E_TREBUCHET || t.underConstruction) continue;
        if (t.packTicks > 0) continue;
        int transition = (g.players[o].research & R_COUNTERWEIGHT) ? 25 : 40;
        Entity* target = aiNearestEnemyBuilding(o, t.x, t.y);
        if (t.packed) {
            if (!target) continue;
            if (dist(t.x, t.y, target->x, target->y) <= rng) {
                t.packed = 0; t.packTicks = transition;
                t.state = S_IDLE; t.path.clear(); t.pathIdx = 0;
            } else if (t.state == S_IDLE || t.path.empty()) {
                orderMove(t, target->x, target->y);
            }
        } else {
            if (t.hp * 100 < t.maxHp * 25 || !target || dist(t.x, t.y, target->x, target->y) > rng) {
                t.packed = 1; t.packTicks = transition;
                t.state = S_IDLE; t.targetId = -1;
                continue;
            }
            if (t.state == S_IDLE || t.targetId != target->id) orderAttack(t, target->id);
        }
    }
}

static void aiTickTransports(int o) {
    Entity* target = nullptr;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.type == E_TOWNHALL || e.type == E_CASTLE) { target = &e; break; }
    }
    Entity* home = aiBldg(o, E_TOWNHALL);
    if (!home) home = aiBldg(o, E_CASTLE);
    if (!target || !home) return;
    int ex=-1, ey=-1, hx=-1, hy=-1;
    if (!findShoreTileNear(target->x, target->y, 18, ex, ey)) return;
    if (!findShoreTileNear(home->x, home->y, 18, hx, hy)) return;
    for (auto& t : g.entities) {
        if (!t.alive || t.owner != o || t.type != E_TRANSPORT || t.underConstruction) continue;
        bool atHome = dist(t.x, t.y, hx, hy) <= 3;
        bool atEnemy = dist(t.x, t.y, ex, ey) <= 3;
        if (t.garrison.empty()) {
            if (atHome) {
                int free = garrisonCap(E_TRANSPORT);
                Entity* peasant = aiIdlePeasant(o);
                if (peasant && mdist(peasant->x, peasant->y, t.x, t.y) <= 12) { orderGarrison(*peasant, t.id); free--; }
                for (auto& u : g.entities) {
                    if (free <= 0) break;
                    if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
                    if (!isUnit(u.type) || u.type == E_PEASANT || isNaval(u.type) || u.type == E_TREBUCHET) continue;
                    if (mdist(u.x, u.y, t.x, t.y) <= 12) { orderGarrison(u, t.id); free--; }
                }
            } else if (t.state == S_IDLE || t.path.empty()) {
                orderMove(t, hx, hy);
            }
        } else {
            if (atEnemy) ejectGarrison(t);
            else if (t.state == S_IDLE || t.path.empty()) orderMove(t, ex, ey);
        }
    }
}

// Pick a target worth attacking from `attacker`'s position.
// Priorities: peasants (raid), wounded enemies, towers, key buildings.
static int aiPickTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        int score = 0;
        if      (e.type == E_CATAPULT || e.type == E_TREBUCHET)     score += 270;
        else if (isWorker(e.type))                                  score += 220;
        else if (isRanged(e.type) || isSiege(e.type))               score += 180;
        else if (isUnit(e.type))                                     score += 110;
        else if (e.type == E_TOWER)                                  score += 140;
        else if (e.type == E_BARRACKS || e.type == E_STABLE)         score += 90;
        else if (e.type == E_FARM || e.type == E_MILL)               score += 70;
        else if (e.type == E_TOWNHALL || e.type == E_CASTLE)         score += 50;
        else if (isBuilding(e.type))                                 score += 25;
        int missing = e.maxHp - e.hp;
        score += missing / 3;            // finish wounded targets
        int d = dist(attacker->x, attacker->y, e.x, e.y);
        score -= d / 2;                  // prefer closer
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best ? best->id : -1;
}

static int aiPickSiegeTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (!isBuilding(e.type) && e.type != E_CATAPULT && e.type != E_TREBUCHET) continue;
        int score = 0;
        if      (e.type == E_TOWNHALL || e.type == E_CASTLE) score += 300;
        else if (e.type == E_TOWER)                          score += 220;
        else if (e.type == E_BARRACKS || e.type == E_STABLE) score += 160;
        else if (e.type == E_DOCK)                           score += 130;
        else                                                 score += 60;
        score += (e.maxHp - e.hp) / 2;
        score -= dist(attacker->x, attacker->y, e.x, e.y);
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best ? best->id : -1;
}

static void tickAIForOwner(int o) {
    Player& p = g.players[o];

    int peas = aiCount(o,E_PEASANT), mil = aiCount(o,E_MILITIA);
    int arch = aiCount(o,E_ARCHER),  kni = aiCount(o,E_KNIGHT);
    int spr  = aiCount(o,E_SPEARMAN);
    int cat  = aiCountAll(o,E_CATAPULT);
    int treb = aiCountAll(o,E_TREBUCHET);
    int hous = aiCountAll(o,E_HOUSE), bar = aiCount(o,E_BARRACKS), stb = aiCount(o,E_STABLE);

    AIIntel intel = aiScout(o);

    aiGather(o);

    // Caps scale with what the player has fielded — match and exceed.
    int peasCap = std::max(12, intel.playerPeasants + 4);
    if (g.tick > 9000) peasCap = std::min(peasCap, 18);
    if (g.tick > 15000) peasCap = std::min(peasCap, 14);
    int milCap  = std::max(8,  intel.playerArmy + 4);
    int archCap = std::max(6,  intel.playerArmy/2 + 3);
    int kniCap  = std::max(4,  intel.playerArmy/3 + 2);
    int towerCap= (intel.playerArmy >= 6 || intel.playerCastles > 0) ? 4 : 2;

    // === ECONOMY: peasants from every TH/Castle ===
    if (peas < peasCap) {
        for (auto& th : g.entities) {
            if (!th.alive || th.owner != o || th.underConstruction) continue;
            if (th.type != E_TOWNHALL && th.type != E_CASTLE) continue;
            if (th.producing != E_NONE) continue;
            if (p.gold >= 50) { orderTrain(th, E_PEASANT); break; }
        }
    }

    // === SUPPLY: keep houses ahead of training ===
    if (p.supply + 4 >= p.supplyMax && hous < 16 && p.wood >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpotWide(o,E_HOUSE,bx,by); if(bx>=0) orderBuild(*b,E_HOUSE,bx,by); }
    }

    if (aiCountAll(o,E_CASTLE) == 0 && mil + arch + kni + spr >= 8 && p.gold >= 100 && p.wood >= 250) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_CASTLE,bx,by); if(bx>=0) orderBuild(*b,E_CASTLE,bx,by); }
    }

    // === MILITARY BUILDINGS ===
    if (bar == 0 && p.wood >= 150 && peas >= 2) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && p.wood >= 150 && p.gold >= 100) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) orderBuild(*b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) orderBuild(*b,E_STABLE,bx,by); }
    }

    // === RESEARCH: queue at blacksmith once it's built ===
    for (auto& smith : g.entities) {
        if (!smith.alive || smith.owner != o || smith.type != E_BLACKSMITH || smith.underConstruction) continue;
        if (smith.researching != 0) break;
        if      (!(p.research & R_IRON_WEAPONS))  { smith.researching = R_IRON_WEAPONS;  smith.researchProgress=0; smith.researchTime=350; break; }
        else if (!(p.research & R_CROSSBOWS))     { smith.researching = R_CROSSBOWS;     smith.researchProgress=0; smith.researchTime=350; break; }
        else if (!(p.research & R_PIKES))         { smith.researching = R_PIKES;         smith.researchProgress=0; smith.researchTime=350; break; }
        else if (!(p.research & R_PLATE_HELM))    { smith.researching = R_PLATE_HELM;    smith.researchProgress=0; smith.researchTime=400; break; }
        else if (!(p.research & R_COUNTERWEIGHT) && aiCountAll(o,E_CASTLE) > 0)
            { smith.researching = R_COUNTERWEIGHT; smith.researchProgress=0; smith.researchTime=400; break; }
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needCat = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (needCat && cat < 2 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { orderTrain(br, E_CATAPULT); continue; }
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        if (spr < sprCap && spr < arch && p.gold >= 40 && p.food >= 20) { orderTrain(br, E_SPEARMAN); continue; }
        if (arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
        if (mil  < milCap  && p.gold >= 60 && p.food >= 20) { orderTrain(br, E_MILITIA); continue; }
        if (spr < sprCap && p.gold >= 40 && p.food >= 20) { orderTrain(br, E_SPEARMAN); continue; }
        if (arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) orderTrain(st, E_KNIGHT);
    }
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || intel.playerArmy >= 10);
    for (auto& cs : g.entities) {
        if (!cs.alive || cs.owner != o || cs.type != E_CASTLE || cs.underConstruction) continue;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && p.gold >= 200 && p.wood >= 250 && p.food >= 30) orderTrain(cs, E_TREBUCHET);
    }

    // === DEFENSE: towers scaled to threat ===
    if (aiCountAll(o,E_TOWER) < towerCap && mil >= 2 && p.wood >= 100 && p.gold >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_TOWER,bx,by); if(bx>=0) orderBuild(*b,E_TOWER,bx,by); }
    }

    // === FOOD: mill + farms scale up before winter ===
    if (aiCountAll(o,E_MILL) == 0 && p.wood >= 100) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MILL,bx,by); if(bx>=0) orderBuild(*b,E_MILL,bx,by); }
    }
    int wantFarms = (getSeason() == AUTUMN) ? 8 : (getSeason() == WINTER ? 0 : 5);
    if (aiCountAll(o,E_MILL) > 0 && aiCountAll(o,E_FARM) < wantFarms && getSeason() != WINTER) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FARM,bx,by); if(bx>=0) orderBuild(*b,E_FARM,bx,by); }
    }
    for (auto& farm : g.entities) {
        if (!farm.alive || farm.owner != o || farm.type != E_FARM || farm.underConstruction) continue;
        bool tended = false;
        for (auto& u : g.entities)
            if (u.alive && u.owner == o && u.state == S_BUILDING && u.targetId == farm.id) { tended = true; break; }
        if (!tended) {
            Entity* tend = aiIdlePeasant(o);
            if (tend) { orderHelp(*tend, farm.id); break; }
        }
    }

    // === NAVAL: dock + boats if water nearby ===
    if (aiCountAll(o,E_DOCK) == 0 && p.wood >= 100 && peas >= 4) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (th) {
            int bx=-1,by=-1; aiBuildSpotNear(o, E_DOCK, th->x, th->y, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(o);
                if (b) orderBuild(*b, E_DOCK, bx, by);
            }
        }
    }
    for (auto& dk : g.entities) {
        if (!dk.alive || dk.owner != o || dk.type != E_DOCK || dk.underConstruction) continue;
        if (dk.producing != E_NONE) continue;
        // Fishing boats first for food, then a couple of warships for coastline pressure.
        if (aiCount(o,E_FISHING_BOAT) < 3 && p.gold >= 80 && p.wood >= 50) { orderTrain(dk, E_FISHING_BOAT); continue; }
        if (aiCount(o,E_WARSHIP) < 2 && p.gold >= 150 && p.wood >= 80 && p.food >= 20) { orderTrain(dk, E_WARSHIP); continue; }
        if (g.biomeChoice == B_OCEAN && aiCount(o,E_TRANSPORT) < 1
            && p.gold >= 80 && p.wood >= 40 && p.food >= 10) { orderTrain(dk, E_TRANSPORT); continue; }
    }

    // === EXPANSION: forward TH halfway to the player ===
    if (aiCountAll(o,E_TOWNHALL) + aiCountAll(o,E_CASTLE) < 2
        && peas >= 9 && p.wood >= 260 && intel.playerTH) {
        Entity* myTh = aiBldg(o, E_TOWNHALL);
        if (!myTh) myTh = aiBldg(o, E_CASTLE);
        if (myTh) {
            int fx = (myTh->x + intel.playerTH->x) / 2;
            int fy = (myTh->y + intel.playerTH->y) / 2;
            int bx=-1, by=-1; aiBuildSpotNear(o, E_TOWNHALL, fx, fy, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(o);
                if (b) orderBuild(*b, E_TOWNHALL, bx, by);
            }
        }
    }

    // === GARRISON: pack archers into the nearest tower/TH/Castle ===
    for (auto& bld : g.entities) {
        if (!bld.alive || bld.owner != o || bld.underConstruction) continue;
        if (!canGarrisonIn(bld.type)) continue;
        if ((int)bld.garrison.size() >= garrisonCap(bld.type)) continue;
        Entity* archer = nullptr; int bestD = 99999;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
            if (!isRanged(u.type) || isSiege(u.type) || isNaval(u.type)) continue;
            int d = mdist(u.x, u.y, bld.x, bld.y);
            if (d < bestD) { bestD = d; archer = &u; }
        }
        if (archer) orderGarrison(*archer, bld.id);
    }

    // === FORWARD AGGRESSION: mid-game outpost near the player base ===
    if (g.tick > 7000 && intel.playerTH && (mil + arch + kni + spr) >= 10) {
        Entity* home = aiBldg(o, E_TOWNHALL);
        if (!home) home = aiBldg(o, E_CASTLE);
        if (home) {
            int fx = home->x + (intel.playerTH->x - home->x) * 65 / 100;
            int fy = home->y + (intel.playerTH->y - home->y) * 65 / 100;
            Entity* anchor = nullptr;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != o) continue;
                if (e.type != E_CASTLE && e.type != E_TOWNHALL) continue;
                if (dist(e.x, e.y, home->x, home->y) < 10) continue;
                if (dist(e.x, e.y, fx, fy) < 18) { anchor = &e; break; }
            }
            if (!anchor && p.gold >= 100 && p.wood >= 250) {
                Entity* b = aiWorker(o);
                if (b) { int bx=-1,by=-1; aiBuildSpotNear(o,E_CASTLE,fx,fy,bx,by); if(bx>=0) orderBuild(*b,E_CASTLE,bx,by); }
            } else if (anchor) {
                bool hasBarr = false;
                for (auto& e : g.entities)
                    if (e.alive && e.owner == o && e.type == E_BARRACKS && dist(e.x,e.y,anchor->x,anchor->y) < 10)
                        { hasBarr = true; break; }
                if (!hasBarr && p.wood >= 150) {
                    Entity* b = aiWorker(o);
                    if (b) { int bx=-1,by=-1; aiBuildSpotNear(o,E_BARRACKS,anchor->x,anchor->y,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
                }
            }
        }
    }

    // === COASTAL BEACHHEAD: landed peasant starts a forward Castle ===
    if (g.biomeChoice == B_OCEAN && intel.playerTH && p.gold >= 100 && p.wood >= 250) {
        Entity* home = aiBldg(o, E_TOWNHALL);
        if (!home) home = aiBldg(o, E_CASTLE);
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || !isWorker(u.type) || u.state != S_IDLE) continue;
            if (home && mdist(u.x,u.y,home->x,home->y) < 30) continue;
            if (mdist(u.x,u.y,intel.playerTH->x,intel.playerTH->y) > 30) continue;
            bool hasBase = false;
            for (auto& b : g.entities)
                if (b.alive && b.owner == o && isBuilding(b.type) && mdist(b.x,b.y,u.x,u.y) < 12)
                    { hasBase = true; break; }
            if (!hasBase) { int bx=-1,by=-1; aiBuildSpotNear(o,E_CASTLE,u.x,u.y,bx,by); if(bx>=0) orderBuild(u,E_CASTLE,bx,by); }
            break;
        }
    }

    // === ATTACK RHYTHM: send waves from idle military only ===
    if (p.aiWaveCd > 0) p.aiWaveCd--;
    int idleArmy = 0;
    Entity* anchor = nullptr;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != o || e.state != S_IDLE) continue;
        if (!isMilitary(e.type) || isWorker(e.type) || isNaval(e.type) || e.type == E_TREBUCHET) continue;
        idleArmy++;
        if (!anchor) anchor = &e;
    }
    const int graceTicks = 1500;
    bool lateGame = g.tick > 12000;
    bool midGame = g.tick > 6000;
    int attackThreshold = (g.tick < graceTicks) ? 999 : (lateGame ? 4 : (midGame ? 5 : 7));
    int waveCooldown = lateGame ? 6 : 10;
    if (idleArmy >= attackThreshold && p.aiWaveCd == 0 && anchor) {
        int tid = aiPickTarget(o, anchor);
        int siegeId = aiPickSiegeTarget(o, anchor);
        if (tid < 0 && intel.playerTH) tid = intel.playerTH->id;
        if (tid >= 0) {
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != o || e.state != S_IDLE) continue;
                if (!isMilitary(e.type) || isWorker(e.type) || isNaval(e.type) || e.type == E_TREBUCHET) continue;
                int myTarget = (e.type == E_CATAPULT && siegeId >= 0) ? siegeId : tid;
                e.attackMove = 1;
                orderAttack(e, myTarget);
            }
            p.aiWaveCd = waveCooldown;
        }
    }

    // === DEFENSE: respond to threats near any owned TH/Castle ===
    for (auto& base : g.entities) {
        if (!base.alive || base.owner != o) continue;
        if (base.type != E_TOWNHALL && base.type != E_CASTLE) continue;
        for (auto& en : g.entities) {
            if (!en.alive || en.owner == o || en.owner == OWNER_NATURE) continue;
            if (en.state == S_GARRISONED) continue;
            if (dist(en.x, en.y, base.x, base.y) < 22) {
                for (auto& d : g.entities)
                    if (d.alive && d.owner == o && isUnit(d.type)
                        && isMilitary(d.type) && d.state == S_IDLE) {
                        d.attackMove = 1;
                        orderAttack(d, en.id);
                    }
                break;
            }
        }
    }

    // Worker defense: if a peasant was hit, nearest idle military intercepts.
    for (auto& worker : g.entities) {
        if (!worker.alive || worker.owner != o || !isWorker(worker.type) || worker.alertTicks <= 0) continue;
        Entity* guard = nullptr; int bestD = 99999;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.state != S_IDLE || !isMilitary(u.type) || isNaval(u.type)) continue;
            int d = mdist(u.x, u.y, worker.x, worker.y);
            if (d < bestD) { bestD = d; guard = &u; }
        }
        Entity* threat = nullptr; int threatD = 99999;
        for (auto& en : g.entities) {
            if (!en.alive || en.owner == o || en.owner == OWNER_NATURE) continue;
            int d = mdist(en.x,en.y,worker.x,worker.y);
            if (d < threatD) { threatD = d; threat = &en; }
        }
        if (guard && threat) { guard->attackMove = 1; orderAttack(*guard, threat->id); }
        break;
    }

    aiTickTrebuchets(o);
    if (g.biomeChoice == B_OCEAN) aiTickTransports(o);
}

void tickAI() {
    g.aiTimer++;
    if (g.aiTimer < 12) return;
    g.aiTimer = 0;
    for (int o = 1; o < MAX_PLAYERS; o++) {
        if (!g.players[o].alive) continue;
        tickAIForOwner(o);
    }
}
