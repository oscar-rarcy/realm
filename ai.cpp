#include "realm.h"

// ============================================================
// AI — economy + military build-out, target picking, wave dispatch
// ============================================================
int     aiCount(int o, EntityType t)    { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)c++; return c; }
int     aiCountAll(int o, EntityType t) { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t)c++;                    return c; }
Entity* aiIdle(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&e.state==S_IDLE&&!e.underConstruction)return &e; return nullptr; }
Entity* aiBldg(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)return &e; return nullptr; }

// Worker selection for AI construction. Prefers idle peasants; if none exist,
// pulls one off gathering/returning so the AI doesn't deadlock.
static Entity* aiWorker(int o) {
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && e.type == E_PEASANT
            && e.state == S_IDLE && !e.underConstruction) return &e;
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && e.type == E_PEASANT
            && !e.underConstruction
            && (e.state == S_GATHERING || e.state == S_RETURNING)) return &e;
    return nullptr;
}

// Idle peasant only — used where we don't want to interrupt gathering.
static Entity* aiIdlePeasant(int o) {
    for (auto& e : g.entities)
        if (e.alive && e.owner == o && e.type == E_PEASANT
            && e.state == S_IDLE && !e.underConstruction) return &e;
    return nullptr;
}

void aiGather(int o) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner!=o || e.type!=E_PEASANT || e.state!=S_IDLE) continue;
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

// Build placement: try near a given centre. Wider, more attempts.
static void aiBuildSpotNear(int o, EntityType bt, int cx, int cy, int& ox, int& oy) {
    if (cx < 0 || cy < 0) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (!th) th = aiBldg(o, E_CASTLE);
        if (!th) return;
        cx = th->x; cy = th->y;
    }
    for (int r = 2; r < 18; r++) for (int a = 0; a < 24; a++) {
        int bx = cx + (rand()%(r*2+1)) - r, by = cy + (rand()%(r*2+1)) - r;
        if (canPlace(bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}
void aiBuildSpot(int o, EntityType bt, int& ox, int& oy) { aiBuildSpotNear(o, bt, -1, -1, ox, oy); }

// Wide placement: starts searching further out — good for supply buildings
// that would be blocked by the dense inner base cluster.
static void aiBuildSpotWide(int o, EntityType bt, int& ox, int& oy) {
    Entity* th = aiBldg(o, E_TOWNHALL);
    if (!th) th = aiBldg(o, E_CASTLE);
    if (!th) return;
    for (int r = 5; r < 28; r++) for (int a = 0; a < 32; a++) {
        int bx = th->x + (rand()%(r*2+1)) - r, by = th->y + (rand()%(r*2+1)) - r;
        if (canPlace(bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}

// Scan all opponents — used to scale production and pick targets.
struct AIIntel { int playerArmy; int playerCastles; int playerWalls; int playerPeasants; int playerCatapults; Entity* playerTH; };
static AIIntel aiScout(int o) {
    AIIntel x{0,0,0,0,0,nullptr};
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if      (e.type == E_PEASANT)  x.playerPeasants++;
        else if (e.type == E_CATAPULT) { x.playerCatapults++; x.playerArmy++; }
        else if (isUnit(e.type))       x.playerArmy++;
        else if (e.type == E_CASTLE)   { x.playerCastles++; if (!x.playerTH) x.playerTH = &e; }
        else if (e.type == E_WALL)     x.playerWalls++;
        else if (e.type == E_TOWNHALL && !x.playerTH) x.playerTH = &e;
    }
    return x;
}

// Pick the best target for a wave originating from `attacker`.
// Priority adapts to game phase: early = raid workers, late = destroy HQ.
static int aiPickTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    bool lateGame = g.tick > 6000;
    bool midGame  = g.tick > 2500;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        int score = 0;
        if      (e.type == E_CATAPULT)                               score += 270; // neutralise siege threat first
        else if (e.type == E_WARSHIP)                                score += 220;
        else if (e.type == E_ARCHER)                                 score += 200;
        else if (e.type == E_PEASANT)                                score += lateGame ? 60 : 220; // raid workers early; ignore late
        else if (isUnit(e.type))                                     score += 130;
        else if (e.type == E_TOWER)                                  score += 160;
        else if (e.type == E_TOWNHALL || e.type == E_CASTLE)         score += lateGame ? 250 : (midGame ? 120 : 50);
        else if (e.type == E_BARRACKS || e.type == E_STABLE)         score += 100;
        else if (e.type == E_DOCK)                                   score += 80;
        else if (e.type == E_FARM || e.type == E_MILL)               score += 65;
        else if (isBuilding(e.type))                                 score += 30;
        // Focus-fire bonus: heavily wounded targets are almost dead, finish them.
        int missing = e.maxHp - e.hp;
        score += missing / 2;
        // Proximity strongly preferred — don't split the army chasing something far away.
        int d = dist(attacker->x, attacker->y, e.x, e.y);
        score -= d * 2 / 3;
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best ? best->id : -1;
}

// Pick a building target for catapults (they deal 1.5x to buildings).
static int aiPickSiegeTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (!isBuilding(e.type) && e.type != E_CATAPULT) continue;
        int score = 0;
        if      (e.type == E_TOWNHALL || e.type == E_CASTLE) score += 300;
        else if (e.type == E_TOWER)                          score += 220;
        else if (e.type == E_BARRACKS || e.type == E_STABLE) score += 160;
        else if (e.type == E_DOCK)                           score += 130;
        else                                                 score += 60;
        int missing = e.maxHp - e.hp;
        score += missing / 2;
        int d = dist(attacker->x, attacker->y, e.x, e.y);
        score -= d;
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best ? best->id : -1;
}

static void tickAIForOwner(int o) {
    Player& p = g.players[o];

    int peas = aiCount(o,E_PEASANT), mil = aiCount(o,E_MILITIA);
    int arch = aiCount(o,E_ARCHER),  kni = aiCount(o,E_KNIGHT);
    int cat  = aiCountAll(o,E_CATAPULT);
    int hous = aiCountAll(o,E_HOUSE), bar = aiCount(o,E_BARRACKS), stb = aiCount(o,E_STABLE);

    AIIntel intel = aiScout(o);

    aiGather(o);

    // Caps scale with what opponents have fielded — match and exceed.
    int peasCap = std::max(12, intel.playerPeasants + 4);
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
    // Use wide placement so houses sprawl outside the dense inner base cluster.
    if (p.supply + 4 >= p.supplyMax && hous < 16 && p.wood >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpotWide(o,E_HOUSE,bx,by); if(bx>=0) orderBuild(*b,E_HOUSE,bx,by); }
    }

    // === LATE-GAME UPGRADE: Castle ===
    // Castle (100g+250w, 4x4) replaces or supplements the TH — more HP, more supply,
    // and gives the AI a hardened anchor for late-game sieges.
    if (aiCountAll(o,E_CASTLE) == 0 && mil + kni >= 8 && p.gold >= 100 && p.wood >= 250) {
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
        if (smith.researching != 0) break; // already in progress
        if (!(p.research & R_IRON_WEAPONS))  { smith.researching = R_IRON_WEAPONS; smith.prodProgress = 0; smith.prodTime = 350; break; }
        if (!(p.research & R_CROSSBOWS))     { smith.researching = R_CROSSBOWS;    smith.prodProgress = 0; smith.prodTime = 350; break; }
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needSiege = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (needSiege && cat < 3 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { orderTrain(br, E_CATAPULT); continue; }
        // Alternate militia and archers for a balanced field force.
        if (arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
        if (mil < milCap               && p.gold >= 60 && p.food >= 20) { orderTrain(br, E_MILITIA); continue; }
        if (arch < archCap             && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) orderTrain(st, E_KNIGHT);
    }

    // === DEFENSE: towers scaled to threat ===
    if (aiCountAll(o,E_TOWER) < towerCap && mil >= 2 && p.wood >= 100 && p.gold >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_TOWER,bx,by); if(bx>=0) orderBuild(*b,E_TOWER,bx,by); }
    }

    // === FOOD: mill + farms scale up; tend untended farms ===
    if (aiCountAll(o,E_MILL) == 0 && p.wood >= 100) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MILL,bx,by); if(bx>=0) orderBuild(*b,E_MILL,bx,by); }
    }
    int wantFarms = (getSeason() == AUTUMN) ? 8 : (getSeason() == WINTER ? 0 : 5);
    if (aiCountAll(o,E_MILL) > 0 && aiCountAll(o,E_FARM) < wantFarms && getSeason() != WINTER) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FARM,bx,by); if(bx>=0) orderBuild(*b,E_FARM,bx,by); }
    }
    // Assign idle peasants to tend any untended farm (one assignment per tick).
    for (auto& farm : g.entities) {
        if (!farm.alive || farm.owner != o || farm.type != E_FARM || farm.underConstruction) continue;
        bool tended = false;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o) continue;
            if (u.state == S_BUILDING && u.targetId == farm.id) { tended = true; break; }
        }
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
        if (aiCount(o,E_FISHING_BOAT) < 3 && p.gold >= 80 && p.wood >= 50) { orderTrain(dk, E_FISHING_BOAT); continue; }
        if (aiCount(o,E_WARSHIP) < 2 && p.gold >= 150 && p.wood >= 80 && p.food >= 20) { orderTrain(dk, E_WARSHIP); continue; }
    }

    // === EXPANSION: forward TH halfway to the nearest opponent ===
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

    // === GARRISON: pack archers into towers and TH/Castle only.
    // Cap at 2 per tower, 3 per TH/Castle. Don't absorb every archer —
    // the field needs a ranged component too.
    int totalArmy = mil + arch + kni + cat;
    for (auto& bld : g.entities) {
        if (!bld.alive || bld.owner != o || bld.underConstruction) continue;
        if (bld.type != E_TOWER && bld.type != E_TOWNHALL && bld.type != E_CASTLE) continue;
        int garCap = (bld.type == E_TOWER) ? 2 : 3;
        if ((int)bld.garrison.size() >= garCap) continue;
        // Only garrison when we have enough archers that some can stay in the field.
        if (arch < 3 || totalArmy - arch < 2) continue;
        Entity* archer = nullptr; int bestD = 99999;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
            if (u.type != E_ARCHER) continue;
            int d = mdist(u.x, u.y, bld.x, bld.y);
            if (d < bestD) { bestD = d; archer = &u; }
        }
        if (archer) orderGarrison(*archer, bld.id);
    }

    // === ATTACK RHYTHM ===
    int army = mil + arch + kni + cat;
    if (p.aiWaveCd > 0) p.aiWaveCd--;

    // Grace period before first attack. Threshold and send-fraction scale with game age.
    const int graceTicks = 600;
    bool lateGame = g.tick > 6000;
    bool midGame  = g.tick > 2500;
    int attackThreshold = (g.tick < graceTicks) ? 999 : (midGame ? 3 : 5);
    int waveCooldown    = lateGame ? 2 : 4; // AI ticks between re-orders
    // Send 70% early, 85% mid, 95% late — always keep a token home guard.
    int sendPct = lateGame ? 95 : (midGame ? 85 : 70);
    int sendCap = std::max(3, army * sendPct / 100);

    if (army >= attackThreshold && p.aiWaveCd == 0) {
        Entity* anchor = nullptr;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != o) continue;
            if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT) continue;
            if (e.state != S_IDLE) continue;
            anchor = &e; break;
        }
        if (anchor) {
            int tid    = aiPickTarget(o, anchor);
            int siegeId = aiPickSiegeTarget(o, anchor);
            // Fallback: march on known enemy HQ.
            if (tid < 0 && intel.playerTH) tid = intel.playerTH->id;
            if (tid >= 0) {
                int sent = 0;
                for (auto& e : g.entities) {
                    if (sent >= sendCap) break;
                    if (!e.alive || e.owner != o) continue;
                    if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT) continue;
                    if (e.state == S_IDLE) {
                        // Catapults seek siege targets; everyone else takes the general target.
                        int myTarget = (e.type == E_CATAPULT && siegeId >= 0) ? siegeId : tid;
                        e.attackMove = 1; // engage opportunistically on the march
                        orderAttack(e, myTarget);
                        sent++;
                    }
                }
                p.aiWaveCd = waveCooldown;
            }
        }
    }

    // === DEFENSE: counter any enemy that strolls near our base ===
    for (auto& base : g.entities) {
        if (!base.alive || base.owner != o) continue;
        if (base.type != E_TOWNHALL && base.type != E_CASTLE) continue;
        for (auto& en : g.entities) {
            if (!en.alive || en.owner == o || en.owner == OWNER_NATURE) continue;
            if (en.state == S_GARRISONED) continue;
            if (dist(en.x, en.y, base.x, base.y) < 22) {
                for (auto& d : g.entities) {
                    if (!d.alive || d.owner != o) continue;
                    if (!isUnit(d.type) || d.type == E_PEASANT || d.type == E_FISHING_BOAT) continue;
                    if (d.state == S_IDLE) { d.attackMove = 1; orderAttack(d, en.id); }
                }
                break;
            }
        }
        break; // respond to the first threatened base; one response per tick
    }

    // === WORKER DEFENSE: if an enemy is near any AI peasant, send nearest idle military ===
    for (auto& worker : g.entities) {
        if (!worker.alive || worker.owner != o || worker.type != E_PEASANT) continue;
        if (worker.alertTicks <= 0) continue; // only if under attack
        // Find nearest idle military unit to intercept.
        Entity* guard = nullptr; int bestD = 99999;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
            if (u.type == E_PEASANT || u.type == E_FISHING_BOAT || !isUnit(u.type)) continue;
            int d = mdist(u.x, u.y, worker.x, worker.y);
            if (d < bestD) { bestD = d; guard = &u; }
        }
        // Find the attacker closest to this worker.
        Entity* threat = nullptr; int threatD = 99999;
        for (auto& en : g.entities) {
            if (!en.alive || en.owner == o || en.owner == OWNER_NATURE) continue;
            int d = mdist(en.x, en.y, worker.x, worker.y);
            if (d < threatD) { threatD = d; threat = &en; }
        }
        if (guard && threat) { guard->attackMove = 1; orderAttack(*guard, threat->id); }
        break; // one escort response per AI tick
    }
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
