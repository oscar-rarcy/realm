#include "realm.h"
#include "core/game_events.h"
#include "core/order_service.h"
#include "core/world_index.h"

// ============================================================
// ENTITY TICK
// ============================================================
static void tickAlert(Entity& e) {
    if (e.alertTicks > 0) e.alertTicks--;
}





static Entity* findEntityForTick(Game& game, const WorldIndex& world, int id) {
    return findEntity(game, world, id);
}

static Entity* entityAtForTick(Game& game, const WorldIndex& world, int x, int y) {
    return entityAt(game, world, x, y);
}

static Entity* corpseAtForTick(Game& game, const WorldIndex& world, int x, int y) {
    return corpseAt(game, world, x, y);
}

static Entity* findDepotForTick(Game& game, const WorldIndex& world, Entity& e) {
    return findDepot(game, world, e);
}

namespace {

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

} // namespace

static void orderAttackForTick(Game& game, const WorldIndex& world, EventSink& events, Entity& e, int targetId) {
    startAttack(game, world, events, e.owner, Selection{ e.id, { e.id } }, targetId);
}

static void orderGatherForTick(Game& game, const WorldIndex& world, EventSink& events, Entity& e, int x, int y) {
    startGather(game, world, events, e.owner, Selection{ e.id, { e.id } }, { x, y });
}

static void orderHelpForTick(Game& game, const WorldIndex& world, EventSink& events, Entity& e, int buildingId) {
    startHelp(game, world, events, e.owner, Selection{ e.id, { e.id } }, buildingId);
}

static bool tickQueuedWaypoint(Game& game, const WorldIndex& world, EventSink& events, Entity& e) {
    if (e.state != S_IDLE || e.waypoints.empty() || !isUnit(e.type) || isNaval(e.type)) return false;
    if (e.holdPosition != 0 || e.retreating != 0) return false;
    MapPos target{ e.waypoints.front().first, e.waypoints.front().second };
    e.waypoints.erase(e.waypoints.begin());
    if (!inBounds(target.x, target.y)) return false;
    if (e.patrolMode) e.waypoints.push_back({ target.x, target.y });
    if (e.type == E_TREBUCHET && e.packed == 0) return false;
    e.state = S_MOVING;
    e.targetX = target.x;
    e.targetY = target.y;
    e.targetId = -1;
    e.stuckTicks = 0;
    e.attackMove = 0;
    e.holdPosition = 0;
    e.path = findPath(game, world, e.x, e.y, target.x, target.y, 300, isNaval(e.type));
    e.pathIdx = 0;
    if (e.path.empty() && (e.x != target.x || e.y != target.y)) {
        e.state = S_IDLE;
        return false;
    }
    events.emit({ GameEventType::ActionMarker, e.owner, -1, target, "", 'x' });
    return true;
}

static bool tickConstruction(Game& game, const WorldIndex& world, EventSink& events, Entity& e) {
    if (e.underConstruction) {
        bool hasBuilder = false;
        for (auto& o : game.entities) {
            if (!o.alive || o.owner!=e.owner || o.state!=S_BUILDING || o.targetId!=e.id) continue;
            // Require adjacency to the building footprint, not just proximity to its origin
            int bx2 = e.x + STATS[e.type].sizeW - 1, by2 = e.y + STATS[e.type].sizeH - 1;
            int cx = std::max(e.x, std::min(o.x, bx2));
            int cy = std::max(e.y, std::min(o.y, by2));
            if (dist(o.x, o.y, cx, cy) <= 1) hasBuilder = true;
        }
        if (hasBuilder) {
            e.hp += 2;
            if (e.hp >= e.maxHp) {
                e.hp = e.maxHp; e.underConstruction = false; updateSupply(game, e.owner);
                if (e.owner >= 0 && e.owner < OWNER_NATURE) emitStatus(events, e.owner, std::string(STATS[e.type].name) + " complete!", GameEventType::EntitySpawned);
                for (auto& o : game.entities) {
                    if (!o.alive || o.state!=S_BUILDING || o.targetId!=e.id) continue;
                    // For farms: keep tending — S_BUILDING handler routes to its farm branch.
                    if (e.type == E_FARM) continue;
                    // For other structures: cast around for the nearest in-progress build
                    // (any owner==o.owner site under construction) and continue helping.
                    Entity* next = nullptr; int bestD = 99999;
                    for (auto& b : game.entities) {
                        if (!b.alive || b.owner != o.owner || !b.underConstruction || !isBuilding(b.type)) continue;
                        if (b.id == e.id) continue;
                        int d = mdist(o.x, o.y, b.x, b.y);
                        if (d < bestD) { bestD = d; next = &b; }
                    }
                    if (next) orderHelpForTick(game, world, events, o, next->id);
                    else o.state = S_IDLE;
                }
            }
        }
        return true;
    }
    return false;
}

static bool tickPackedTrebuchet(EventSink& events, Entity& e) {
    if (e.type == E_TREBUCHET && e.packTicks > 0) {
        e.packTicks--;
        if (e.packTicks == 0 && e.owner >= 0 && e.owner < OWNER_NATURE)
            emitStatus(events, e.owner, e.packed ? "Trebuchet packed." : "Trebuchet deployed.", GameEventType::UnitOrdered);
        return true;
    }
    return false;
}

static void tickRetreat(Game& game, const WorldIndex& world, EventSink& events, Entity& e) {
    if (e.retreating > 0) {
        e.retreating--;
        if (e.hp * 100 >= e.maxHp * 30) e.retreating = 0;
    } else if (e.owner < OWNER_NATURE && isMilitary(e.type) && e.hp > 0
            && e.hp * 100 < e.maxHp * 15 && e.state != S_GARRISONED) {
        Entity* safe = nullptr;
        int bestD = 99999;
        for (auto& b : game.entities) {
            if (!b.alive || b.owner != e.owner || b.underConstruction) continue;
            if (b.type != E_TOWNHALL && b.type != E_CASTLE && b.type != E_TOWER) continue;
            int d = mdist(e.x, e.y, b.x, b.y);
            if (d < bestD) { bestD = d; safe = &b; }
        }
        if (safe) {
            e.retreating = 120;
            startMove(game, world, events, e.owner, Selection{ e.id, { e.id } }, { safe->x, safe->y });
        }
    }
}

static void tickUnitState(Game& game, WorldIndex& world, EventSink& events, Entity& e) {
    switch (e.state) {
    case S_IDLE:
        // Military auto-engages anything visible within fog radius — units now
        // close in on threats they can see rather than waiting to be poked.
        if (!e.retreating && !e.holdPosition && isMilitary(e.type) && canAttack(e.type) && e.type != E_RAM
            && !(e.type == E_TREBUCHET && e.packed == 1)
            && e.owner != OWNER_NATURE) {
            // Melee units engage at 5 tiles; ranged at full fog radius — prevents
            // instant magnetic battles where everyone charges across the map.
            int aggroRange = isRanged(e.type) ? std::max(FOG_RADIUS, unitRange(game, e)+1) : 5;
            Entity* en = findNearestEnemy(game, e, aggroRange);
            if (en) orderAttackForTick(game, world, events, e, en->id);
        }
        // Boats auto-fish when idle — find a fish shoal, gather, return to dock.
        if (e.type == E_FISHING_BOAT && (game.tick + e.id) % 12 == 0) {
            // If carrying fish but no dock when we landed here, retry now (player may have rebuilt).
            if (e.cargo.amount > 0) {
                Entity* dep = findDepotForTick(game, world, e);
                if (dep) {
                    e.state = S_RETURNING; e.targetId = dep->id;
                    e.targetX = dep->x; e.targetY = dep->y;
                    e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
                    break;
                }
            }
            findNearbyResource(game, world, events, e);
        }
        break;
    case S_MOVING:
        // Attack-move: engage anything in range while marching toward the destination.
        if (e.attackMove && STATS[e.type].atk > 0 && !(e.type == E_TREBUCHET && e.packed == 1)) {
            Entity* en = findNearestEnemy(game, e, unitRange(game, e)+1);
            if (en) { orderAttackForTick(game, world, events, e, en->id); break; }
        }
        moveAlongPath(game, world, e);
        if (e.path.empty() || e.pathIdx >= (int)e.path.size()) {
            e.state = S_IDLE; e.attackMove = 0;
        }
        break;
    case S_ATTACKING: {
        Entity* t = findEntityForTick(game, world, e.targetId);
        if (!t || !t->alive) { e.state = S_IDLE; break; }
        // Target ducked into a building — they're untouchable, go idle. Without
        // this the attacker keeps swinging at the garrisoned position and the
        // target dies inside the safe building.
        if (t->state == S_GARRISONED) { e.state = S_IDLE; e.targetId = -1; break; }
        int d = dist(e.x, e.y, t->x, t->y);
        if (e.type == E_TREBUCHET && (e.packed == 1 || e.packTicks > 0)) { e.state = S_IDLE; break; }
        // Catapults need standoff — too close to arm the sling properly.
        if (e.type == E_CATAPULT && d < 2) { e.state = S_IDLE; break; }
        if (d <= unitRange(game, e)) {
            if (e.atkCd <= 0) {
                int rawDmg = unitAtk(game, e);
                int dmg = damageVs(game, e.type, t->type, rawDmg, t->owner);
                t->hp -= dmg;
                e.atkCd = STATS[e.type].atkSpeed;
                e.alertTicks = 12; t->alertTicks = 12;
                if (t->owner >= 0 && t->owner < OWNER_NATURE && game.attackNotifyCd == 0 && t->type != E_NONE) {
                    emitStatus(events, t->owner, "Your people are under attack!");
                    game.attackNotifyCd = 200;
                }
                if (isRanged(e.type)) {
                    char pc = (e.type==E_CATAPULT || e.type==E_TREBUCHET) ? 'o' : '-';
                    int pcol = (e.type==E_CATAPULT || e.type==E_TREBUCHET) ? CP_PROJ_BOULDER : CP_PROJ_ARROW;
                    spawnProjectile(game, e.x, e.y, t->x, t->y, pc, pcol);
                }
                // Catapult splash: 1-tile radius around impact centre, ~1/3 of the
                // raw damage to anyone but the prime target (per-victim building
                // modifier still applies). Friendly fire is on.
                if (e.type == E_CATAPULT) {
                    auto& ts = STATS[t->type];
                    int tcx = t->x + ts.sizeW/2, tcy = t->y + ts.sizeH/2;
                    int primeId = t->id, splashRaw = rawDmg / 3;
                    bool worldDirty = false;
                    for (auto& o : game.entities) {
                        if (!o.alive || o.id == primeId) continue;
                        auto& os = STATS[o.type];
                        int ox = std::max(o.x, std::min(tcx, o.x + os.sizeW - 1));
                        int oy = std::max(o.y, std::min(tcy, o.y + os.sizeH - 1));
                        if (std::abs(ox - tcx) <= 1 && std::abs(oy - tcy) <= 1) {
                            int splashDmg = damageVs(game, E_CATAPULT, o.type, splashRaw, o.owner);
                            o.hp -= splashDmg; o.alertTicks = 12;
                            if (o.hp <= 0) {
                                killEntity(game, events, o);
                                worldDirty = true;
                            }
                        }
                    }
                    if (worldDirty) world = buildWorldIndex(game);
                }
                if (t->hp <= 0) {
                    bool startCarcassHarvest = t->owner == OWNER_NATURE && e.owner < OWNER_NATURE
                        && e.type == E_PEASANT
                        && (t->type == E_DEER || t->type == E_SHEEP || t->type == E_BOAR);
                    int cx = t->x, cy = t->y;
                    killEntity(game, events, *t);
                    world = buildWorldIndex(game);
                    Entity* carcass = corpseAtForTick(game, world, cx, cy);
                    if (startCarcassHarvest && carcass && isHarvestableCarcass(*carcass)) {
                        orderGatherForTick(game, world, events, e, cx, cy);
                        if (e.owner >= 0 && e.owner < OWNER_NATURE) emitStatus(events, e.owner, "Harvesting carcass.", GameEventType::UnitOrdered);
                    } else if (e.cargo.amount <= 0) {
                        e.state = S_IDLE;
                    }
                }
            } else e.atkCd--;
        } else {
            // Re-path if our path is exhausted OR target wandered away from its end.
            bool stale = e.path.empty() || e.pathIdx >= (int)e.path.size();
            if (!stale) {
                auto end = e.path.back();
                if (dist(end.first, end.second, t->x, t->y) > 2) stale = true;
            }
            if (stale) {
                e.path = findPathFor(game, e, t->x, t->y); e.pathIdx = 0;
                if (e.path.empty()) { e.state = S_IDLE; e.targetId = -1; break; }
            }
            moveAlongPath(game, world, e);
        }
        break;
    }
    case S_GATHERING: {
        int d = dist(e.x, e.y, e.targetX, e.targetY);
        if (d <= 1) {
            Entity* carcass = corpseAtForTick(game, world, e.targetX, e.targetY);
            if (carcass && e.type == E_PEASANT && e.cargo.type == CR_FOOD) {
                if (isHarvestableCarcass(*carcass)) {
                    e.gatherCd++;
                    if (e.gatherCd >= GATHER_TICKS) {
                        e.gatherCd = 0;
                        int carryLeft = std::max(0, CARRY_MAX - e.cargo.amount);
                        int amt = std::min(GATHER_RATE, std::min(carcass->carcassFoodRemaining, carryLeft));
                        carcass->carcassFoodRemaining -= amt;
                        e.cargo.amount += amt;
                        if (e.cargo.amount >= CARRY_MAX || carcass->carcassFoodRemaining <= 0) {
                            Entity* dep = findDepotForTick(game, world, e);
                            if (dep && e.cargo.amount > 0) {
                                e.state = S_RETURNING; e.targetId = dep->id;
                                e.targetX = dep->x; e.targetY = dep->y;
                                e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
                            } else {
                                if (e.owner >= 0 && e.owner < OWNER_NATURE && e.cargo.amount > 0) emitStatus(events, e.owner, "No food drop-off available.", GameEventType::CommandRejected);
                                e.cargo = emptyCargo();
                                e.state = S_IDLE;
                            }
                        }
                    }
                } else {
                    Entity* dep = e.cargo.amount > 0 ? findDepotForTick(game, world, e) : nullptr;
                    if (dep) {
                        e.state = S_RETURNING; e.targetId = dep->id;
                        e.targetX = dep->x; e.targetY = dep->y;
                        e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
                    } else {
                        e.cargo = emptyCargo();
                        e.state = S_IDLE;
                    }
                }
                break;
            }
            Tile& tile = game.map[e.targetY][e.targetX];
            if (terrainHasDirectGatherResource(tile.terrain) && tile.resources > 0) {
                e.gatherCd++;
                if (e.gatherCd >= GATHER_TICKS) {
                    e.gatherCd = 0;
                    int amt = std::min(GATHER_RATE, tile.resources);
                    tile.resources -= amt; e.cargo.amount += amt;
                    if (tile.resources <= 0)
                        tile.terrain = depletedTerrainForResource(tile.terrain);
                    if (e.cargo.amount >= CARRY_MAX || tile.resources <= 0) {
                        Entity* dep = findDepotForTick(game, world, e);
                        if (dep) {
                            e.state = S_RETURNING; e.targetId = dep->id;
                            e.targetX = dep->x; e.targetY = dep->y;
                            e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
                        } else e.state = S_IDLE;
                    }
                }
            } else {
                // Tile depleted (beaten to it) — seek another nearby node
                if (!findNearbyResource(game, world, events, e)) e.state = S_IDLE;
            }
        } else {
            moveAlongPath(game, world, e);
            if (e.path.empty() && dist(e.x,e.y,e.targetX,e.targetY) > 1) {
                e.path = findPathFor(game, e, e.targetX, e.targetY); e.pathIdx = 0;
                if (e.path.empty()) e.state = S_IDLE;
            }
        }
        break;
    }
    case S_RETURNING: {
        Entity* dep = findEntityForTick(game, world, e.targetId);
        if (!dep || !dep->alive) {
            dep = findDepotForTick(game, world, e);
            if (!dep) { e.state = S_IDLE; break; }
            e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
            e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
        }
        int d = dist(e.x, e.y, dep->x, dep->y);
        if (d <= STATS[dep->type].sizeW + 1) {
            if (e.cargo.type == CR_GOLD)      game.players[e.owner].gold += e.cargo.amount;
            else if (e.cargo.type == CR_WOOD) game.players[e.owner].wood += e.cargo.amount;
            else                              addPlayerFood(game, e.owner, e.cargo.amount, dep);
            e.cargo.amount = 0;
            // Farm courier: cargo source stores the farm we came from; return and resume tending.
            if (e.cargo.type == CR_FOOD && inBounds(e.cargo.sourceX, e.cargo.sourceY)) {
                Entity* home = entityAtForTick(game, world, e.cargo.sourceX, e.cargo.sourceY);
                if (home && home->alive && home->type == E_FARM
                    && home->owner == e.owner && !home->underConstruction) {
                    e.state = S_BUILDING; e.targetId = home->id;
                    e.targetX = home->x; e.targetY = home->y;
                    e.path = findPathFor(game, e, home->x, home->y); e.pathIdx = 0;
                    break;
                }
            }
            if (!inBounds(e.resourceX, e.resourceY)) { e.state = S_IDLE; break; }
            Entity* carcass = corpseAtForTick(game, world, e.resourceX, e.resourceY);
            if (carcass && isHarvestableCarcass(*carcass)) {
                orderGatherForTick(game, world, events, e, e.resourceX, e.resourceY);
                break;
            }
            Tile& rt = game.map[e.resourceY][e.resourceX];
            if (terrainHasDirectGatherResource(rt.terrain) && rt.resources > 0) {
                e.state = S_GATHERING; e.targetX = e.resourceX; e.targetY = e.resourceY;
                e.path = findPathFor(game, e, e.resourceX, e.resourceY); e.pathIdx = 0;
            } else {
                // Rally point depleted — seek another nearby node of the same type
                if (!findNearbyResource(game, world, events, e)) e.state = S_IDLE;
            }
        } else {
            moveAlongPath(game, world, e);
            if (e.path.empty() && dist(e.x,e.y,dep->x,dep->y) > STATS[dep->type].sizeW+1) {
                e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
            }
        }
        break;
    }
    case S_ENTERING: {
        Entity* bld = findEntityForTick(game, world, e.targetId);
        if (!bld || !bld->alive || bld->underConstruction || !canGarrisonIn(bld->type) || bld->owner != e.owner) {
            e.state = S_IDLE; break;
        }
        int bw = STATS[bld->type].sizeW, bh = STATS[bld->type].sizeH;
        int bx2 = bld->x + bw - 1, by2 = bld->y + bh - 1;
        int cx = std::max(bld->x, std::min(e.x, bx2));
        int cy = std::max(bld->y, std::min(e.y, by2));
        if (dist(e.x, e.y, cx, cy) <= 1) {
            if ((int)bld->garrison.size() < garrisonCap(bld->type)) {
                bld->garrison.push_back(e.id);
                e.state = S_GARRISONED;
                e.x = bld->x; e.y = bld->y;
                e.path.clear(); e.pathIdx = 0; e.stuckTicks = 0;
                if (e.owner >= 0 && e.owner < OWNER_NATURE) emitStatus(events, e.owner, std::string("Garrisoned in ") + STATS[bld->type].name, GameEventType::GarrisonChanged);
            } else {
                if (e.owner >= 0 && e.owner < OWNER_NATURE) emitStatus(events, e.owner, std::string(STATS[bld->type].name) + " is full", GameEventType::CommandRejected);
                e.state = S_IDLE;
            }
        } else {
            moveAlongPath(game, world, e);
            if (e.path.empty()) {
                int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
                for (int dy = -1; dy <= bh; dy++) for (int dx = -1; dx <= bw; dx++) {
                    if (dx>=0 && dx<bw && dy>=0 && dy<bh) continue;
                    int nx = bld->x+dx, ny = bld->y+dy;
                    if (inBounds(nx,ny) && isPassable(game,nx,ny)) {
                        int d2 = mdist(e.x, e.y, nx, ny);
                        if (d2 < bestAD) { bestAD = d2; bestAX = nx; bestAY = ny; }
                    }
                }
                e.path = findPathFor(game, e, bestAX, bestAY); e.pathIdx = 0;
                if (e.path.empty()) e.state = S_IDLE;
            }
        }
        break;
    }
    case S_GARRISONED:
        break; // Stays inside; building handles its own attacks.
    case S_BUILDING: {
        Entity* bld = findEntityForTick(game, world, e.targetId);
        if (!bld || !bld->alive) { e.state = S_IDLE; break; }
        // Tending a completed farm — stay adjacent and ferry ripe harvest to a depot
        if (!bld->underConstruction && bld->type == E_FARM) {
            int d = dist(e.x, e.y, bld->x, bld->y);
            // Pick up ripe wheat once enough has accumulated to make the trip worthwhile
            if (d <= 1 && bld->storedFood >= 3 && e.cargo.amount == 0) {
                int take = std::min(bld->storedFood, CARRY_MAX);
                e.cargo = {CR_FOOD, take, bld->x, bld->y};
                e.resourceX = bld->x; e.resourceY = bld->y;
                bld->storedFood -= take;
                Entity* dep = findDepotForTick(game, world, e);
                if (dep) {
                    e.state = S_RETURNING;
                    e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
                    e.path = findPathFor(game, e, dep->x, dep->y); e.pathIdx = 0;
                } else {
                    // No depot available — drop the harvest back on the farm and keep tending
                    bld->storedFood += take; e.cargo.amount = 0;
                }
                break;
            }
            if (d > 1) {
                moveAlongPath(game, world, e);
                if (e.path.empty()) { e.path = findPathFor(game, e, bld->x, bld->y); e.pathIdx = 0; }
            }
            break;
        }
        if (!bld->underConstruction) { e.state = S_IDLE; break; }
        int bx2 = bld->x + STATS[bld->type].sizeW - 1, by2 = bld->y + STATS[bld->type].sizeH - 1;
        int cx = std::max(bld->x, std::min(e.x, bx2));
        int cy = std::max(bld->y, std::min(e.y, by2));
        if (dist(e.x, e.y, cx, cy) > 1) {
            moveAlongPath(game, world, e);
            if (e.path.empty()) {
                // Re-scan for the nearest free adjacent tile
                int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
                int bw = STATS[bld->type].sizeW, bh = STATS[bld->type].sizeH;
                for (int dy = -1; dy <= bh; dy++) for (int dx = -1; dx <= bw; dx++) {
                    if (dx>=0&&dx<bw&&dy>=0&&dy<bh) continue;
                    int nx = bld->x+dx, ny = bld->y+dy;
                    if (inBounds(nx,ny) && isPassable(game,nx,ny)) {
                        int d2 = mdist(e.x,e.y,nx,ny);
                        if (d2 < bestAD) { bestAD=d2; bestAX=nx; bestAY=ny; }
                    }
                }
                e.path = findPathFor(game, e, bestAX, bestAY); e.pathIdx = 0;
            }
        }
        break;
    }
    default: break;
    }
}

void tickEntity(Game& game, WorldIndex& world, EventSink& events, Entity& e) {
    if (!e.alive) return;
    tickAlert(e);
    tickProduction(game, world, events, e);
    tickResearch(game, events, e);
    if (tickConstruction(game, world, events, e)) return;
    if (!isUnit(e.type)) return;
    if (tickQueuedWaypoint(game, world, events, e)) return;
    if (tickPackedTrebuchet(events, e)) return;
    tickRetreat(game, world, events, e);
    tickUnitState(game, world, events, e);
}
