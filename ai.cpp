#include "realm.h"

// ============================================================
// AI — economy + military build-out, target picking, wave dispatch
// ============================================================

// All AI *orders* flow through the same command funnel as player input:
// one validation path, and the sim sees identical machinery whoever issued
// the order. Applied immediately — the AI runs inside the sim, so there is
// nothing to queue and nothing to record (replays re-derive AI behaviour
// from the seed). Direct field writes below (research cheats, trebuchet
// pack micro-state, attack rhythm) are sim-internal behaviour, not orders.
static void aiCmd(Command c, int o)                        { c.player = o; applyCommand(c); }
static void aiMove(int o, Entity& u, int x, int y)         { Command c; c.type=CMD_MOVE;     c.units={u.id}; c.x=x; c.y=y; aiCmd(c,o); }
static void aiAttackCmd(int o, Entity& u, int tid, bool am){ Command c; c.type=CMD_ATTACK;   c.units={u.id}; c.target=tid; c.arg=am?1:0; aiCmd(c,o); }
static void aiGatherAt(int o, Entity& u, int x, int y)     { Command c; c.type=CMD_GATHER;   c.units={u.id}; c.x=x; c.y=y; aiCmd(c,o); }
static void aiBuildAt(int o, Entity& u, EntityType bt, int x, int y) { Command c; c.type=CMD_BUILD; c.units={u.id}; c.x=x; c.y=y; c.arg=bt; aiCmd(c,o); }
static void aiTrain(int o, Entity& b, EntityType ut)       { Command c; c.type=CMD_TRAIN;    c.target=b.id; c.arg=ut; aiCmd(c,o); }
static void aiHelp(int o, Entity& u, int bid)              { Command c; c.type=CMD_HELP;     c.units={u.id}; c.target=bid; aiCmd(c,o); }
static void aiGarrisonIn(int o, Entity& u, int bid)        { Command c; c.type=CMD_GARRISON; c.units={u.id}; c.target=bid; aiCmd(c,o); }
static void aiEject(int o, Entity& t)                      { Command c; c.type=CMD_UNGARRISON; c.target=t.id; aiCmd(c,o); }
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
    // Collect resource tiles once, then match peasants against the list —
    // avoids a full map scan per idle peasant.
    std::vector<std::pair<int,int>> spots;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        bool isR = (t==T_GOLD||t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE||t==T_BERRY);
        if (isR && g.map[y][x].resources > 0) spots.push_back({x, y});
    }
    if (spots.empty()) return;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner!=o || e.type!=E_PEASANT || e.state!=S_IDLE) continue;
        int bestD = 9999, bx = -1, by = -1;
        for (auto& [x, y] : spots) {
            int d = mdist(e.x, e.y, x, y);
            if (d < bestD) { bestD=d; bx=x; by=y; }
        }
        if (bx >= 0) aiGatherAt(o, e, bx, by);
    }
}

// Build placement: try near a given centre. Prefer positions with some breathing
// room from existing buildings so the base spreads out rather than clustering.
static void aiBuildSpotNear(int o, EntityType bt, int cx, int cy, int& ox, int& oy) {
    if (cx < 0 || cy < 0) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (!th) th = aiBldg(o, E_CASTLE);
        if (!th) return;
        cx = th->x; cy = th->y;
    }
    // First pass: spaced placement — skip tiles within 3 of any owned building.
    for (int r = 4; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (simRand()%(r*2+1)) - r, by = cy + (simRand()%(r*2+1)) - r;
        if (!canPlace(bt, bx, by, o)) continue;
        bool tooClose = false;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != o || !isBuilding(e.type)) continue;
            if (dist(e.x, e.y, bx, by) < 3) { tooClose = true; break; }
        }
        if (!tooClose) { ox = bx; oy = by; return; }
    }
    // Fallback: accept any valid spot if spacing can't be maintained.
    for (int r = 2; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (simRand()%(r*2+1)) - r, by = cy + (simRand()%(r*2+1)) - r;
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
        int bx = th->x + (simRand()%(r*2+1)) - r, by = th->y + (simRand()%(r*2+1)) - r;
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

// Find the nearest enemy building to a trebuchet, for siege positioning.
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

// Finds a water tile near a position that's adjacent to passable land.
// Used as ferry endpoints on each coast.
static bool findShoreTileNear(int cx, int cy, int searchR, int& ox, int& oy) {
    for (int r = 1; r <= searchR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            if (!isPassableWater(nx,ny)) continue;
            // Adjacent passable land?
            for (int dy2 = -1; dy2 <= 1; dy2++) for (int dx2 = -1; dx2 <= 1; dx2++) {
                if (dx2 == 0 && dy2 == 0) continue;
                int qx = nx+dx2, qy = ny+dy2;
                if (inBounds(qx,qy) && isPassable(qx,qy)) { ox = nx; oy = ny; return true; }
            }
        }
    }
    return false;
}

// Per-tick AI transport behaviour. Amphibious assault lifecycle:
//   empty -> sail to home shore -> load idle military -> sail to enemy shore
//   -> eject troops -> empty -> repeat.
static void aiTickTransports(int o) {
    Entity* home = nullptr;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != o) continue;
        if (e.type != E_TOWNHALL && e.type != E_CASTLE) continue;
        home = &e; break;
    }
    if (!home) return;
    // Pick assault target: enemy TC/Castle nearest our own base.
    Entity* target = nullptr;
    int bestD = 99999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.type != E_TOWNHALL && e.type != E_CASTLE) continue;
        int d = mdist(home->x, home->y, e.x, e.y);
        if (d < bestD) { bestD = d; target = &e; }
    }
    if (!target) return;

    int enemyShoreX = -1, enemyShoreY = -1;
    if (!findShoreTileNear(target->x, target->y, 18, enemyShoreX, enemyShoreY)) return;
    int homeShoreX = -1, homeShoreY = -1;
    if (!findShoreTileNear(home->x, home->y, 18, homeShoreX, homeShoreY)) return;

    for (auto& t : g.entities) {
        if (!t.alive || t.owner != o || t.type != E_TRANSPORT || t.underConstruction) continue;

        bool atHomeShore  = dist(t.x, t.y, homeShoreX,  homeShoreY)  <= 3;
        bool atEnemyShore = dist(t.x, t.y, enemyShoreX, enemyShoreY) <= 3;

        if (t.garrison.empty()) {
            // Empty: head home to pick up troops.
            if (atEnemyShore) {
                // Just delivered — clear leftover orders so we'll sail home.
                t.state = S_IDLE; t.path.clear();
            }
            if (atHomeShore) {
                // First slot: a peasant colonist so the AI can build a forward
                // base on the enemy shore. Remaining slots: idle military.
                int free = garrisonCap(E_TRANSPORT) - (int)t.garrison.size();
                bool hasPeasant = false;
                for (int gid : t.garrison) {
                    Entity* g_e = findEntity(gid);
                    if (g_e && g_e->type == E_PEASANT) { hasPeasant = true; break; }
                }
                if (!hasPeasant && free > 0) {
                    Entity* nearestPeas = nullptr; int bestPD = 99999;
                    for (auto& u : g.entities) {
                        if (!u.alive || u.owner != o || u.type != E_PEASANT) continue;
                        if (u.state != S_IDLE) continue;
                        int d = mdist(u.x, u.y, t.x, t.y);
                        if (d > 12) continue;
                        if (d < bestPD) { bestPD = d; nearestPeas = &u; }
                    }
                    if (nearestPeas) { aiGarrisonIn(o, *nearestPeas, t.id); free--; }
                }
                // Then load nearby idle military.
                for (auto& u : g.entities) {
                    if (free <= 0) break;
                    if (!u.alive || u.owner != o) continue;
                    if (!isUnit(u.type) || u.type == E_PEASANT
                            || u.type == E_FISHING_BOAT || u.type == E_TREBUCHET) continue;
                    if (isNaval(u.type)) continue;
                    if (u.state != S_IDLE) continue;
                    if (mdist(u.x, u.y, t.x, t.y) > 12) continue;
                    aiGarrisonIn(o, u, t.id);
                    free--;
                }
            } else if (t.state == S_IDLE || t.path.empty()) {
                aiMove(o, t, homeShoreX, homeShoreY);
            }
        } else {
            // Loaded: sail to enemy shore and disembark.
            if (atEnemyShore) {
                aiEject(o, t);
            } else if (t.state == S_IDLE || t.path.empty()) {
                aiMove(o, t, enemyShoreX, enemyShoreY);
            }
        }
    }
}

// Per-tick AI trebuchet management: pack/deploy/attack lifecycle.
static void aiTickTrebuchets(int o) {
    int rng = STATS[E_TREBUCHET].range; // 12
    for (auto& t : g.entities) {
        if (!t.alive || t.owner != o || t.type != E_TREBUCHET || t.underConstruction) continue;
        if (t.packTicks > 0) continue; // mid-transition; wait
        int pT = (g.players[o].research & R_COUNTERWEIGHT) ? 25 : 40;
        Entity* tgt = aiNearestEnemyBuilding(o, t.x, t.y);
        if (t.packed == 1) {
            // Mobile: roll toward the nearest enemy building, deploy when in range.
            if (!tgt) continue;
            int d = dist(t.x, t.y, tgt->x, tgt->y);
            if (d <= rng) {
                t.packed = 0; t.packTicks = pT;
                t.state = S_IDLE; t.path.clear(); t.pathIdx = 0;
            } else if (t.state == S_IDLE) {
                // March toward the target. Stop 2 tiles short of max range to give space to deploy.
                int stopAt = rng - 1;
                int sx = tgt->x, sy = tgt->y;
                int dx = sx - t.x, dy = sy - t.y;
                float L = std::sqrt((float)(dx*dx + dy*dy));
                if (L < 1) continue;
                int gx = t.x + (int)(dx * (1.0f - stopAt / L));
                int gy = t.y + (int)(dy * (1.0f - stopAt / L));
                gx = std::max(0, std::min(MAP_W-1, gx));
                gy = std::max(0, std::min(MAP_H-1, gy));
                aiMove(o, t, gx, gy);
            }
        } else {
            // Deployed: find a target in range to attack. If under heavy fire, pack and run.
            if (t.hp * 100 < t.maxHp * 25) {
                t.packed = 1; t.packTicks = pT;
                t.state = S_IDLE; t.targetId = -1;
                continue;
            }
            if (!tgt || dist(t.x, t.y, tgt->x, tgt->y) > rng) {
                // No targets in range — pack up and move closer.
                t.packed = 1; t.packTicks = pT;
                t.state = S_IDLE; t.targetId = -1;
                continue;
            }
            if (t.state == S_IDLE || t.targetId != tgt->id) {
                aiAttackCmd(o, t, tgt->id, false);
            }
        }
    }
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
    int spr  = aiCount(o,E_SPEARMAN);
    int cat  = aiCountAll(o,E_CATAPULT);
    int treb = aiCountAll(o,E_TREBUCHET);
    int hous = aiCountAll(o,E_HOUSE), bar = aiCount(o,E_BARRACKS), stb = aiCount(o,E_STABLE);

    AIIntel intel = aiScout(o);

    aiGather(o);

    // Caps scale with what opponents have fielded — match and exceed.
    // Peasant cap softens late-game: once economy is set, more peasants is wasted supply.
    int peasCap = std::max(12, intel.playerPeasants + 4);
    if (g.tick > 9000) peasCap = std::min(peasCap, 18);   // late-game hard cap
    if (g.tick > 15000) peasCap = std::min(peasCap, 14);  // end-game even tighter
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
            if (p.gold >= 50) { aiTrain(o, th, E_PEASANT); break; }
        }
    }

    // === SUPPLY: keep houses ahead of training ===
    // Use wide placement so houses sprawl outside the dense inner base cluster.
    if (p.supply + 4 >= p.supplyMax && hous < 16 && p.wood >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpotWide(o,E_HOUSE,bx,by); if(bx>=0) aiBuildAt(o,*b,E_HOUSE,bx,by); }
    }

    // === LATE-GAME UPGRADE: Castle ===
    // Castle (100g+250w, 4x4) replaces or supplements the TH — more HP, more supply,
    // and gives the AI a hardened anchor for late-game sieges.
    if (aiCountAll(o,E_CASTLE) == 0 && mil + kni >= 8 && p.gold >= 100 && p.wood >= 250) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_CASTLE,bx,by); if(bx>=0) aiBuildAt(o,*b,E_CASTLE,bx,by); }
    }

    // === MILITARY BUILDINGS ===
    if (bar == 0 && p.wood >= 150 && peas >= 2) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) aiBuildAt(o,*b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && p.wood >= 150 && p.gold >= 100) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) aiBuildAt(o,*b,E_BARRACKS,bx,by); }
    }
    if (aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) aiBuildAt(o,*b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) aiBuildAt(o,*b,E_STABLE,bx,by); }
    }

    // === RESEARCH: queue at blacksmith once it's built ===
    // Order: Iron → Crossbows → Pikes → Plate Helm → Counterweight.
    // Counterweight last since it only matters once we're fielding trebuchets.
    for (auto& smith : g.entities) {
        if (!smith.alive || smith.owner != o || smith.type != E_BLACKSMITH || smith.underConstruction) continue;
        if (smith.researching != 0) break;
        if      (!(p.research & R_IRON_WEAPONS))  { smith.researching = R_IRON_WEAPONS;  smith.prodProgress=0; smith.prodTime=350; break; }
        else if (!(p.research & R_CROSSBOWS))     { smith.researching = R_CROSSBOWS;     smith.prodProgress=0; smith.prodTime=350; break; }
        else if (!(p.research & R_PIKES))         { smith.researching = R_PIKES;         smith.prodProgress=0; smith.prodTime=350; break; }
        else if (!(p.research & R_PLATE_HELM))    { smith.researching = R_PLATE_HELM;    smith.prodProgress=0; smith.prodTime=400; break; }
        else if (!(p.research & R_COUNTERWEIGHT) && aiCountAll(o,E_CASTLE) > 0) { smith.researching = R_COUNTERWEIGHT; smith.prodProgress=0; smith.prodTime=400; break; }
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needSiege = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (needSiege && cat < 3 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { aiTrain(o, br, E_CATAPULT); continue; }
        // Spearmen counter the player's cavalry — train them in response to knights.
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        bool needSpears = (spr < sprCap && p.gold >= 40 && p.food >= 20);
        if (needSpears && spr < arch) { aiTrain(o, br, E_SPEARMAN); continue; }
        // Alternate militia and archers for a balanced field force.
        if (arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { aiTrain(o, br, E_ARCHER);  continue; }
        if (mil < milCap               && p.gold >= 60 && p.food >= 20) { aiTrain(o, br, E_MILITIA); continue; }
        if (needSpears                                                  ) { aiTrain(o, br, E_SPEARMAN); continue; }
        if (arch < archCap             && p.gold >= 70 && p.food >= 20) { aiTrain(o, br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) aiTrain(o, st, E_KNIGHT);
    }
    // Castles produce trebuchets — siege specialists, 1-2 max, only when sieging.
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || (intel.playerArmy >= 10));
    for (auto& cs : g.entities) {
        if (!cs.alive || cs.owner != o || cs.type != E_CASTLE || cs.underConstruction) continue;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && p.gold >= 200 && p.wood >= 250 && p.food >= 40)
            aiTrain(o, cs, E_TREBUCHET);
    }

    // === DEFENSE: towers scaled to threat ===
    if (aiCountAll(o,E_TOWER) < towerCap && mil >= 2 && p.wood >= 100 && p.gold >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_TOWER,bx,by); if(bx>=0) aiBuildAt(o,*b,E_TOWER,bx,by); }
    }

    // === FOOD: mill + farms scale up; tend untended farms ===
    if (aiCountAll(o,E_MILL) == 0 && p.wood >= 100) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MILL,bx,by); if(bx>=0) aiBuildAt(o,*b,E_MILL,bx,by); }
    }
    int wantFarms = (getSeason() == AUTUMN) ? 8 : (getSeason() == WINTER ? 0 : 5);
    if (aiCountAll(o,E_MILL) > 0 && aiCountAll(o,E_FARM) < wantFarms && getSeason() != WINTER) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FARM,bx,by); if(bx>=0) aiBuildAt(o,*b,E_FARM,bx,by); }
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
            if (tend) { aiHelp(o, *tend, farm.id); break; }
        }
    }

    // === NAVAL: dock + boats if water nearby ===
    if (aiCountAll(o,E_DOCK) == 0 && p.wood >= 100 && peas >= 4) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (th) {
            int bx=-1,by=-1; aiBuildSpotNear(o, E_DOCK, th->x, th->y, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(o);
                if (b) aiBuildAt(o, *b, E_DOCK, bx, by);
            }
        }
    }
    for (auto& dk : g.entities) {
        if (!dk.alive || dk.owner != o || dk.type != E_DOCK || dk.underConstruction) continue;
        if (dk.producing != E_NONE) continue;
        if (aiCount(o,E_FISHING_BOAT) < 3 && p.gold >= 80 && p.wood >= 50) { aiTrain(o, dk, E_FISHING_BOAT); continue; }
        if (aiCount(o,E_WARSHIP) < 2 && p.gold >= 150 && p.wood >= 80 && p.food >= 20) { aiTrain(o, dk, E_WARSHIP); continue; }
        // Coastal maps: build a transport for amphibious assault on enemies on other islands.
        if (g.biomeChoice == B_OCEAN && aiCount(o,E_TRANSPORT) < 1
                && p.gold >= 80 && p.wood >= 40 && p.food >= 10) {
            aiTrain(o, dk, E_TRANSPORT); continue;
        }
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
                if (b) aiBuildAt(o, *b, E_TOWNHALL, bx, by);
            }
        }
    }

    // === FORWARD AGGRESSION: outpost near the player base ===
    // Once mid-game and the AI has a real force, plant a Castle ~65% of the
    // way to the enemy HQ and a Barracks beside it, so trained units spawn
    // at their doorstep instead of marching across the whole map.
    if (g.tick > 7000 && intel.playerTH && (mil + arch + kni + spr) >= 10) {
        Entity* home = aiBldg(o, E_TOWNHALL);
        if (!home) home = aiBldg(o, E_CASTLE);
        if (home) {
            int fx = home->x + (intel.playerTH->x - home->x) * 65 / 100;
            int fy = home->y + (intel.playerTH->y - home->y) * 65 / 100;
            // Already have a forward anchor near this position?
            Entity* fwdAnchor = nullptr;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != o) continue;
                if (e.type != E_CASTLE && e.type != E_TOWNHALL) continue;
                if (dist(e.x, e.y, home->x, home->y) < 10) continue; // skip home base
                if (dist(e.x, e.y, fx, fy) < 18) { fwdAnchor = &e; break; }
            }
            // Plant the forward Castle.
            if (!fwdAnchor && p.gold >= 100 && p.wood >= 250) {
                int bx=-1, by=-1; aiBuildSpotNear(o, E_CASTLE, fx, fy, bx, by);
                if (bx >= 0) {
                    Entity* b = aiWorker(o);
                    if (b) aiBuildAt(o, *b, E_CASTLE, bx, by);
                }
            }
            // Forward Barracks beside an existing forward anchor.
            if (fwdAnchor && p.wood >= 150) {
                bool hasBarr = false;
                for (auto& e : g.entities) {
                    if (!e.alive || e.owner != o || e.type != E_BARRACKS) continue;
                    if (dist(e.x, e.y, fwdAnchor->x, fwdAnchor->y) < 10) { hasBarr = true; break; }
                }
                if (!hasBarr) {
                    int bx=-1, by=-1; aiBuildSpotNear(o, E_BARRACKS, fwdAnchor->x, fwdAnchor->y, bx, by);
                    if (bx >= 0) {
                        Entity* b = aiWorker(o);
                        if (b) aiBuildAt(o, *b, E_BARRACKS, bx, by);
                    }
                }
            }
            // Forward Tower for vision + harassment, once Barracks is up.
            if (fwdAnchor && aiCountAll(o,E_TOWER) < towerCap + 1 && p.wood >= 100 && p.gold >= 50) {
                bool hasTwr = false;
                for (auto& e : g.entities) {
                    if (!e.alive || e.owner != o || e.type != E_TOWER) continue;
                    if (dist(e.x, e.y, fwdAnchor->x, fwdAnchor->y) < 8) { hasTwr = true; break; }
                }
                if (!hasTwr) {
                    int bx=-1, by=-1; aiBuildSpotNear(o, E_TOWER, fwdAnchor->x, fwdAnchor->y, bx, by);
                    if (bx >= 0) {
                        Entity* b = aiWorker(o);
                        if (b) aiBuildAt(o, *b, E_TOWER, bx, by);
                    }
                }
            }
        }
    }

    // === COASTAL BEACHHEAD: any peasant landed near the enemy starts a forward base.
    // A peasant marooned across the sea is the AI's signal to colonise — build a
    // Castle near them so trained units spawn on the enemy island.
    if (g.biomeChoice == B_OCEAN && intel.playerTH && p.gold >= 100 && p.wood >= 250) {
        Entity* myHome = aiBldg(o, E_TOWNHALL);
        if (!myHome) myHome = aiBldg(o, E_CASTLE);
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.type != E_PEASANT) continue;
            if (u.state != S_IDLE) continue;
            // Peasant is "far from home" (across the sea) and "near the enemy".
            int distHome  = myHome ? mdist(u.x, u.y, myHome->x, myHome->y) : 999;
            int distEnemy = mdist(u.x, u.y, intel.playerTH->x, intel.playerTH->y);
            if (distHome < 30) continue;       // still on home island
            if (distEnemy > 30) continue;      // not actually close to enemy
            // Existing forward base around here?
            bool hasFwd = false;
            for (auto& b : g.entities) {
                if (!b.alive || b.owner != o) continue;
                if (b.type != E_CASTLE && b.type != E_TOWNHALL && b.type != E_BARRACKS) continue;
                if (mdist(b.x, b.y, u.x, u.y) < 12) { hasFwd = true; break; }
            }
            if (hasFwd) continue;
            int bx=-1, by=-1; aiBuildSpotNear(o, E_CASTLE, u.x, u.y, bx, by);
            if (bx >= 0) { aiBuildAt(o, u, E_CASTLE, bx, by); break; }
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
        if (archer) aiGarrisonIn(o, *archer, bld.id);
    }

    // === ATTACK RHYTHM ===
    if (p.aiWaveCd > 0) p.aiWaveCd--;

    // Count *idle* military — the units actually available to send. Counting total
    // army (including units still deployed at the enemy base from prior waves)
    // tricks the threshold into firing without enough reinforcements at home,
    // causing the AI to dribble out single units after the first big push.
    int idleArmy = 0;
    Entity* anchor = nullptr;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != o) continue;
        if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT || e.type == E_TREBUCHET) continue;
        if (e.state != S_IDLE) continue;
        idleArmy++;
        if (!anchor) anchor = &e;
    }

    // Grace period before first attack. Threshold scales with game age.
    const int graceTicks = 1500;          // ~2 minutes of setup time
    bool lateGame = g.tick > 12000;
    bool midGame  = g.tick > 6000;
    int attackThreshold = (g.tick < graceTicks) ? 999 : (lateGame ? 4 : (midGame ? 5 : 7));
    int waveCooldown    = lateGame ? 6 : 10; // AI ticks between wave re-orders

    if (idleArmy >= attackThreshold && p.aiWaveCd == 0 && anchor) {
        int tid    = aiPickTarget(o, anchor);
        int siegeId = aiPickSiegeTarget(o, anchor);
        // Fallback: march on known enemy HQ.
        if (tid < 0 && intel.playerTH) tid = intel.playerTH->id;
        if (tid >= 0) {
            // Send the whole idle pool — anything that survived a previous wave is
            // already at the enemy base. Keep no home guard from the idle stack;
            // home defence comes from towers + the defence block below.
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != o) continue;
                if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT || e.type == E_TREBUCHET) continue;
                if (e.state == S_IDLE) {
                    int myTarget = (e.type == E_CATAPULT && siegeId >= 0) ? siegeId : tid;
                    aiAttackCmd(o, e, myTarget, true);
                }
            }
            p.aiWaveCd = waveCooldown;
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
                    if (d.state == S_IDLE) aiAttackCmd(o, d, en.id, true);
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
        if (guard && threat) aiAttackCmd(o, *guard, threat->id, true);
        break; // one escort response per AI tick
    }

    // Trebuchet management — pack/march/deploy/attack lifecycle.
    aiTickTrebuchets(o);

    // Coastal maps: AI transports ferry troops across the sea.
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
