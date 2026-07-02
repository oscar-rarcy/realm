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

// May this AI make that, era- and civ-wise? Checked before shopping so the
// build order adapts (a Fenlander AI never wants a stable; a Hamlet-era AI
// doesn't burn workers trying to place a castle).
static bool aiCan(int o, EntityType t) { return makeGate(o, t) == 0; }

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

// Has owner `o` ever laid eyes on this entity's tile? The AI's knowledge of
// its enemies is limited to what its own units have scouted — no more
// omniscient counting. (explored[] is honest: it only spreads with vision.)
static bool aiKnows(int o, const Entity& e) {
    return inBounds(e.x, e.y) && g.map[e.y][e.x].explored[o];
}

// Scan all opponents — used to scale production and pick targets.
// FAIR: only counts what this AI has actually scouted (explored tiles).
struct AIIntel { int playerArmy; int playerCastles; int playerWalls; int playerPeasants; int playerCatapults; int playerCavalry; Entity* playerTH; };
static AIIntel aiScout(int o) {
    AIIntel x{0,0,0,0,0,0,nullptr};
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (!aiKnows(o, e)) continue;
        if      (e.type == E_PEASANT)  x.playerPeasants++;
        else if (e.type == E_CATAPULT) { x.playerCatapults++; x.playerArmy++; }
        else if (e.type == E_KNIGHT || e.type == E_HUSSAR) { x.playerCavalry++; x.playerArmy++; }
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
        if (!isBuilding(e.type) || !aiKnows(o, e)) continue;
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
        if (!aiKnows(o, e)) continue;
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

// SCOUTING — with fair intel the AI has to go and LOOK. Until it has found
// an enemy hall, it periodically sends one fast unit riding at an unexplored
// quarter of the map; afterwards, an occasional sweep keeps intel fresh.
static void aiScoutMission(const struct AICtx& cx);

// Pick the best target for a wave originating from `attacker`.
// Priority adapts to game phase: early = raid workers, late = destroy HQ.
static int aiPickTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    bool lateGame = g.tick > 6000;
    bool midGame  = g.tick > 2500;
    Season season = getSeason();
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (!aiKnows(o, e)) continue;      // can't march on what it hasn't found
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
        // Fat depots are worth burning: weight targets by what's stored there.
        if (isBuilding(e.type))
            score += (e.storeGold + e.storeWood + depotFoodSum(e)) / 4;
        // Autumn is the season to strike the harvest: granaries, mills,
        // stockyards and taverns all smell of winter stores.
        if (season == AUTUMN && (e.type==E_GRANARY || e.type==E_MILL
                              || e.type==E_STOCKYARD || e.type==E_TAVERN)) score += 120;
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
        if (!aiKnows(o, e)) continue;
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

// Per-owner AI snapshot: counts/caps frozen once per tick so the phases below
// all see the same numbers (the original monolithic tick read them once too).
// Live resources (p.gold/wood/food/supply) are read fresh from g.players[o].
struct AICtx {
    int o, diff;
    int peas, mil, arch, kni, spr, xbow, hus, cat, treb, hous, bar, stb;
    AIIntel intel;
    int peasCap, milCap, archCap, kniCap, towerCap;
};

// Phase 1 — economy buildings, supply, military buildings, research, training.
static void aiProduceAndTrain(const AICtx& cx) {
    const int o = cx.o; Player& p = g.players[o]; const int diff = cx.diff;
    const AIIntel& intel = cx.intel;
    const int peas=cx.peas, mil=cx.mil, arch=cx.arch, kni=cx.kni, spr=cx.spr,
              xbow=cx.xbow, hus=cx.hus, cat=cx.cat, treb=cx.treb, hous=cx.hous,
              bar=cx.bar, stb=cx.stb, peasCap=cx.peasCap, milCap=cx.milCap,
              archCap=cx.archCap, kniCap=cx.kniCap;
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

    // === ERA ADVANCE — the spine of the AI's game plan ===
    // Builders push the ladder early; Warlords wait for an army; Raiders
    // sit in the middle. All personas advance eventually — stalling in the
    // Hamlet era is how you lose to a knight push.
    {
        int persona = p.aiPersona;
        int wantPeas = (persona == 2) ? 6 : (persona == 3) ? 9 : 7;
        int f, gld, w, ticks;
        bool advancing = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner == o && e.researching == R_ERA_ADVANCE) { advancing = true; break; }
        if (!advancing && eraUpCost(p.era, f, gld, w, ticks)
            && peas >= wantPeas
            && p.food >= f + 40 && p.gold >= gld && p.wood >= w) {
            Entity* hall = aiBldg(o, E_TOWNHALL);
            if (!hall) hall = aiBldg(o, E_CASTLE);
            if (hall && hall->researching == 0) {
                Command c; c.type = CMD_ERA_UP; c.target = hall->id; aiCmd(c, o);
            }
        }
    }

    // === LATE-GAME UPGRADE: Castle ===
    // Castle (100g+250w, 4x4) replaces or supplements the TH — more HP, more supply,
    // and gives the AI a hardened anchor for late-game sieges.
    if (aiCan(o,E_CASTLE) && aiCountAll(o,E_CASTLE) == 0 && mil + kni >= 8 && p.gold >= STATS[E_CASTLE].costGold && p.wood >= STATS[E_CASTLE].costWood) {
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
    if (aiCan(o,E_BLACKSMITH) && aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) aiBuildAt(o,*b,E_BLACKSMITH,bx,by); }
    }
    if (aiCan(o,E_STABLE) && stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) aiBuildAt(o,*b,E_STABLE,bx,by); }
    }

    // === STOCKYARD: the AI hoards too — and its open piles are raidable,
    // which is exactly the kind of target the raid squads live for. ===
    if (aiCan(o,E_STOCKYARD) && aiCountAll(o,E_STOCKYARD) == 0 && peas >= 6 && p.wood >= 100) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STOCKYARD,bx,by); if(bx>=0) aiBuildAt(o,*b,E_STOCKYARD,bx,by); }
    }

    // === RESEARCH: shop the shared table at every research building ===
    // (Free and difficulty-time-scaled — the AI's standing handicap/cheat.)
    // Counterweight waits for a castle; everything else goes in table order.
    {
        int nRes = 0; const ResearchDef* tbl = researchTable(nRes);
        for (auto& shop : g.entities) {
            if (!shop.alive || shop.owner != o || shop.underConstruction) continue;
            if (shop.researching != 0) continue;
            if (shop.type != E_BLACKSMITH && shop.type != E_MILL
                && shop.type != E_STABLE && shop.type != E_STONEMASON) continue;
            for (int i = 0; i < nRes; i++) {
                const ResearchDef& r = tbl[i];
                if (r.building != shop.type) continue;
                if (p.era < r.era) continue;
                if (p.research & r.bit) continue;
                if (r.bit == R_COUNTERWEIGHT && aiCountAll(o,E_CASTLE) == 0) continue;
                shop.researching = r.bit; shop.prodProgress = 0;
                shop.prodTime = r.ticks * ((diff==2) ? 10 : (diff==1) ? 16 : 24) / 24;
                break;
            }
        }
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needSiege = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (aiCan(o,E_CATAPULT) && needSiege && cat < 3 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { aiTrain(o, br, E_CATAPULT); continue; }
        // Crossbowmen answer enemy cavalry: thrust punches plate. Needs a smith.
        if (aiCan(o,E_CROSSBOWMAN) && aiBldg(o, E_BLACKSMITH) && intel.playerCavalry >= 2
            && xbow < std::max(2, intel.playerCavalry)
            && p.gold >= 70 && p.wood >= 30 && p.food >= 20) { aiTrain(o, br, E_CROSSBOWMAN); continue; }
        // Spearmen counter the player's cavalry — train them in response to knights.
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        bool canSpear = aiCan(o,E_SPEARMAN), canArch = aiCan(o,E_ARCHER);
        bool needSpears = (canSpear && spr < sprCap && p.gold >= 40 && p.food >= 20);
        if (needSpears && spr < arch) { aiTrain(o, br, E_SPEARMAN); continue; }
        // Alternate militia and archers for a balanced field force.
        if (canArch && arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { aiTrain(o, br, E_ARCHER);  continue; }
        if (mil < milCap               && p.gold >= 60 && p.food >= 20) { aiTrain(o, br, E_MILITIA); continue; }
        if (needSpears                                                  ) { aiTrain(o, br, E_SPEARMAN); continue; }
        if (canArch && arch < archCap  && p.gold >= 70 && p.food >= 20) { aiTrain(o, br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        int husCap = (p.aiPersona == 1) ? 4 + diff : 2 + diff;   // Raiders love light horse
        if      (aiCan(o,E_KNIGHT) && kni < kniCap && p.gold >= 120 && p.food >= 40) aiTrain(o, st, E_KNIGHT);
        // Hussars raid the player's economy — Raiders keep a whole stable of them.
        else if (aiCan(o,E_HUSSAR) && (kni >= 2 || p.aiPersona == 1)
                 && hus < husCap && p.gold >= 80 && p.food >= 20) aiTrain(o, st, E_HUSSAR);
    }
    // Castles produce trebuchets — siege specialists, 1-2 max, only when sieging.
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || (intel.playerArmy >= 10));
    for (auto& cs : g.entities) {
        if (!cs.alive || cs.owner != o || cs.type != E_CASTLE || cs.underConstruction) continue;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && p.gold >= 200 && p.wood >= 250 && p.food >= 40)
            aiTrain(o, cs, E_TREBUCHET);
    }
}

// Phase 2 — defensive towers, food economy (mill/granary/farms), naval.
static void aiInfraAndNaval(const AICtx& cx) {
    const int o = cx.o; Player& p = g.players[o];
    const int peas=cx.peas, mil=cx.mil, towerCap=cx.towerCap;
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
    // Granary once the economy is rolling: deep larder + halved winter hunger.
    if (aiCountAll(o,E_MILL) > 0 && aiCountAll(o,E_GRANARY) == 0
        && g.tick > 4000 && p.wood >= 80) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_GRANARY,bx,by); if(bx>=0) aiBuildAt(o,*b,E_GRANARY,bx,by); }
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
        if (g.layoutChoice == L_ISLANDS && aiCount(o,E_TRANSPORT) < 1
                && p.gold >= 80 && p.wood >= 40 && p.food >= 10) {
            aiTrain(o, dk, E_TRANSPORT); continue;
        }
    }
}

// Phase 3 — expansion town halls, forward aggression, coastal beachheads.
static void aiExpand(const AICtx& cx) {
    const int o = cx.o; Player& p = g.players[o];
    const AIIntel& intel = cx.intel;
    const int peas=cx.peas, mil=cx.mil, arch=cx.arch, kni=cx.kni, spr=cx.spr, towerCap=cx.towerCap;
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
            if (!fwdAnchor && p.gold >= STATS[E_CASTLE].costGold && p.wood >= STATS[E_CASTLE].costWood) {
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
    if (g.layoutChoice == L_ISLANDS && intel.playerTH && p.gold >= STATS[E_CASTLE].costGold && p.wood >= STATS[E_CASTLE].costWood) {
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
}

// Phase 4 — garrisoning, attack waves, base/worker defence, siege & transports.
static void aiCommandArmy(const AICtx& cx) {
    const int o = cx.o; Player& p = g.players[o]; const int diff = cx.diff;
    const AIIntel& intel = cx.intel;
    const int mil=cx.mil, arch=cx.arch, kni=cx.kni, cat=cx.cat;
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

    // Grace period before first attack. Threshold scales with game age —
    // then persona, season and hour of day bend it. A Raider prowls at
    // night; everyone digs in for winter; summer is campaign season.
    const int graceTicks = (diff==2) ? 1500 : (diff==1) ? 2250 : 3200; // ~2/3/4.5 min
    bool lateGame = g.tick > 12000;
    bool midGame  = g.tick > 6000;
    int attackThreshold = (g.tick < graceTicks) ? 999 : (lateGame ? 4 : (midGame ? 5 : 7)) + (2-diff)*2;
    int waveCooldown    = (lateGame ? 6 : 10) + (2-diff)*3; // AI ticks between wave re-orders
    if (g.tick >= graceTicks) {
        switch (p.aiPersona) {
            case 1: attackThreshold -= 1; break;                       // Raider: restless
            case 2: attackThreshold += 1; break;                       // Builder: patient
            case 3: attackThreshold += 2; waveCooldown += 4; break;    // Warlord: fewer, bigger blows
        }
        Season ss = getSeason();
        if      (ss == WINTER) attackThreshold += 3;   // the land itself defends
        else if (ss == SUMMER) attackThreshold -= 1;   // dry roads, long days
        if (isNight()) attackThreshold += (p.aiPersona == 1) ? -2 : 2; // Raiders own the dark
        attackThreshold = std::max(3, attackThreshold);
    }

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

    // === PLUNDER SQUADS: small, fast, and after your goods, not your walls ===
    // Stockyards get robbed (CMD_RAID hauls the piles home); otherwise the
    // squad harasses scouted gatherers and farms. Raiders do this constantly,
    // Builders now and then, Warlords barely bother. Autumn sharpens everyone.
    if (p.aiRaidCd > 0) p.aiRaidCd--;
    if (g.tick >= graceTicks/2 && p.aiRaidCd == 0) {
        int interval = (p.aiPersona == 1) ? 45 : (p.aiPersona == 2) ? 110 : 160;
        if (getSeason() == AUTUMN) interval = interval * 2 / 3;
        // Gather a small fast squad: hussars first, then militia/spearmen.
        std::vector<Entity*> squad;
        for (int pass = 0; pass < 2 && (int)squad.size() < 3; pass++)
            for (auto& u : g.entities) {
                if ((int)squad.size() >= 3) break;
                if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
                if (pass == 0 && u.type != E_HUSSAR) continue;
                if (pass == 1 && u.type != E_MILITIA && u.type != E_SPEARMAN) continue;
                squad.push_back(&u);
            }
        if (getenv("REALM_AI_DEBUG"))
            fprintf(stderr, "[ai%d] raid check t=%d squad=%d cd=%d\n", o, g.tick, (int)squad.size(), p.aiRaidCd);
        if ((int)squad.size() >= 2) {
            // Best loot first: a scouted enemy stockyard with goods in it.
            Entity* yard = nullptr; int yD = 99999;
            Entity* prey = nullptr; int pD = 99999;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner == o || e.owner >= MAX_PLAYERS) continue;
                if (!aiKnows(o, e)) continue;
                int d = mdist(squad[0]->x, squad[0]->y, e.x, e.y);
                if (e.type == E_STOCKYARD && !e.underConstruction
                    && (e.storeGold + e.storeWood + depotFoodSum(e)) > 40 && d < yD) { yD = d; yard = &e; }
                if ((e.type == E_PEASANT || e.type == E_FARM || e.type == E_MILL) && d < pD) { pD = d; prey = &e; }
            }
            if (getenv("REALM_AI_DEBUG"))
                fprintf(stderr, "[ai%d] raid targets t=%d yard=%d prey=%d\n", o, g.tick, yard?yard->id:-1, prey?prey->id:-1);
            if (yard) {
                Command c; c.type = CMD_RAID; c.target = yard->id;
                for (auto* u : squad) c.units.push_back(u->id);
                aiCmd(c, o);
                p.aiRaidCd = interval;
            } else if (prey) {
                Command c; c.type = CMD_ATTACK_MOVE; c.x = prey->x; c.y = prey->y;
                for (auto* u : squad) c.units.push_back(u->id);
                aiCmd(c, o);
                p.aiRaidCd = interval;
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
    if (g.layoutChoice == L_ISLANDS) aiTickTransports(o);
}

// Ride out and LOOK. Until the enemy hall is found, this fires often; after
// that, an occasional sweep refreshes the picture. One rider per mission.
static void aiScoutMission(const AICtx& cx) {
    const int o = cx.o;
    bool blind = (cx.intel.playerTH == nullptr);
    // Cadence: blind = every ~40 AI ticks; informed = every ~400.
    static_assert(MAX_PLAYERS <= 8, "scout phase packing");
    int phase = (g.tick / 12) % (blind ? 40 : 400);
    if (phase != o) return;                      // stagger per seat, deterministic
    // Pick the rider: hussar > militia > spearman > (blind only) a peasant.
    Entity* rider = nullptr;
    for (EntityType t : {E_HUSSAR, E_MILITIA, E_SPEARMAN}) {
        rider = aiIdle(o, t);
        if (rider) break;
    }
    if (!rider && blind && g.tick > 600) rider = aiIdlePeasant(o);
    if (!rider) return;
    // Aim at an unexplored spot far from home — quarter the map and probe.
    Entity* home = aiBldg(o, E_TOWNHALL);
    if (!home) home = aiBldg(o, E_CASTLE);
    int bx = -1, by = -1, bestScore = -1;
    for (int tries = 0; tries < 12; tries++) {
        int x = 8 + simRand() % (MAP_W - 16), y = 8 + simRand() % (MAP_H - 16);
        if (!isPassable(x, y)) continue;
        int score = g.map[y][x].explored[o] ? 0 : 100;
        if (home) score += mdist(home->x, home->y, x, y) / 4;
        if (score > bestScore) { bestScore = score; bx = x; by = y; }
    }
    if (bx >= 0) aiMove(o, *rider, bx, by);
}

static void tickAIForOwner(int o) {
    const int diff = g.difficulty;
    AICtx cx;
    cx.o = o; cx.diff = diff;
    cx.peas = aiCount(o,E_PEASANT);     cx.mil  = aiCount(o,E_MILITIA);
    cx.arch = aiCount(o,E_ARCHER);      cx.kni  = aiCount(o,E_KNIGHT);
    cx.spr  = aiCount(o,E_SPEARMAN);
    cx.xbow = aiCount(o,E_CROSSBOWMAN); cx.hus  = aiCount(o,E_HUSSAR);
    cx.cat  = aiCountAll(o,E_CATAPULT); cx.treb = aiCountAll(o,E_TREBUCHET);
    cx.hous = aiCountAll(o,E_HOUSE);    cx.bar  = aiCount(o,E_BARRACKS); cx.stb = aiCount(o,E_STABLE);
    cx.intel = aiScout(o);
    aiGather(o);

    // Caps scale with what opponents have fielded — match and exceed.
    int peasCap = std::max(12, cx.intel.playerPeasants + 4);
    if (g.tick > 9000)  peasCap = std::min(peasCap, 18);
    if (g.tick > 15000) peasCap = std::min(peasCap, 14);
    int capPad = (diff==2) ? 4 : (diff==1) ? 2 : 0;
    cx.peasCap  = peasCap;
    cx.milCap   = std::max(8, cx.intel.playerArmy + capPad);
    cx.archCap  = std::max(6, cx.intel.playerArmy/2 + 1 + diff);
    cx.kniCap   = std::max(4, cx.intel.playerArmy/3 + diff);
    cx.towerCap = (cx.intel.playerArmy >= 6 || cx.intel.playerCastles > 0) ? 4 : 2;

    // Persona flavour on the caps: Builders boom, Warlords mass, Raiders run lean.
    if (g.players[o].aiPersona == 2) cx.peasCap += 4;
    if (g.players[o].aiPersona == 3) cx.milCap  += 4;

    aiScoutMission(cx);
    aiProduceAndTrain(cx);
    aiInfraAndNaval(cx);
    aiExpand(cx);
    aiCommandArmy(cx);
}

void tickAI() {
    g.aiTimer++;
    if (g.aiTimer < 12) return;
    g.aiTimer = 0;
    for (int o = 0; o < MAX_PLAYERS; o++) {
        if ((g.humanMask >> o) & 1) continue;   // human-driven seat (local or remote)
        if (!g.players[o].alive) continue;
        tickAIForOwner(o);
    }
}
