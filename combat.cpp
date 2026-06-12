#include "realm.h"
#include <cmath>

// ============================================================
// COMBAT HELPERS (research-aware stats, damage modifiers)
// ============================================================
int unitAtk(const Entity& e) {
    int a = STATS[e.type].atk;
    int r = g.players[e.owner].research;
    if ((e.type == E_MILITIA || e.type == E_KNIGHT) && (r & R_IRON_WEAPONS)) a += 2;
    // Shield wall: militia fight harder when shoulder-to-shoulder.
    if (e.type == E_MILITIA) {
        int allies = 0;
        for (const auto& o : g.entities) {
            if (!o.alive || o.id == e.id || o.owner != e.owner || o.type != E_MILITIA) continue;
            if (dist(e.x, e.y, o.x, o.y) <= 1) { allies++; if (allies >= 2) break; }
        }
        a += allies;
    }
    return a;
}
// ============================================================
// DAMAGE GRAMMAR — Brood-War-style damage type × armour class.
// Composition beats raw numbers: every relationship lives in one table.
// ============================================================
static bool isSiege(EntityType t) { return t==E_CATAPULT||t==E_RAM||t==E_WARSHIP||t==E_TREBUCHET||t==E_SAPPER; }

ArmorClass armorClassOf(EntityType t) {
    if (isBuilding(t)) return ARM_SIEGE;
    switch (t) {
        case E_KNIGHT: case E_CROSSBOWMAN: case E_RAM: case E_WARSHIP:
            return ARM_ARMORED;
        case E_CATAPULT: case E_TREBUCHET:
            return ARM_SIEGE;
        default:
            return ARM_LIGHT;   // peasant, militia, archer, spearman, hussar, monk, sapper, boats, animals
    }
}

DamageType damageTypeOf(EntityType t) {
    switch (t) {
        case E_ARCHER:   case E_WARSHIP:                              return DMG_PIERCE;
        case E_SPEARMAN: case E_CROSSBOWMAN:                          return DMG_THRUST;
        case E_CATAPULT: case E_TREBUCHET: case E_RAM: case E_SAPPER: return DMG_CRUSH;
        default:                                                       return DMG_SLASH;
    }
}

// % of raw damage dealt: rows = DamageType, cols = Light / Armored / Siege.
// Pierce shreds light but pings off plate; Thrust punches armour; Crush
// breaks stone and engines but swings wide of nimble men.
static const int DMG_TABLE[4][3] = {
    {100, 100,  75},   // Slash
    {100,  60,  40},   // Pierce
    { 75, 150,  50},   // Thrust
    { 75, 100, 150},   // Crush
};

int damageVs(EntityType attacker, EntityType target, int rawDmg, int targetOwner) {
    // Walls and gates require siege to breach — swords bounce off stone.
    if ((target==E_WALL||target==E_GATE) && !isSiege(attacker)) return 0;
    // Braced spears gut warhorses. Flat bonus on top of the Thrust multiplier
    // so 2 Spearmen (80g) still reliably beat 1 Knight (120g).
    if (attacker==E_SPEARMAN && (target==E_KNIGHT||target==E_HUSSAR)) rawDmg += 8;
    // Buildings keep bespoke siege multipliers — the table covers field combat.
    if (isBuilding(target)) {
        if (attacker == E_TREBUCHET) return rawDmg * 3;       // city-killer
        if (attacker == E_RAM)       return rawDmg * 2;       // built for doors
        if (attacker == E_CATAPULT)  return (rawDmg * 3) / 2;
        return std::max(1, rawDmg / 2);                       // 0.5x, floor 1
    }
    // Trebuchet vs personnel: a wall-breaker, not a man-killer.
    if (attacker == E_TREBUCHET) rawDmg = std::max(1, rawDmg / 4);
    int dmg = rawDmg * DMG_TABLE[damageTypeOf(attacker)][armorClassOf(target)] / 100;
    // Plate Helm research: knights shrug 2 points off cuts and arrows.
    if (target==E_KNIGHT && targetOwner >= 0 && targetOwner <= MAX_PLAYERS
            && (g.players[targetOwner].research & R_PLATE_HELM)) {
        DamageType dt = damageTypeOf(attacker);
        if (dt == DMG_SLASH || dt == DMG_PIERCE) dmg -= 2;
    }
    return std::max(1, dmg);
}
int unitRange(const Entity& e) {
    int rng = STATS[e.type].range;
    int r = g.players[e.owner].research;
    if (e.type == E_ARCHER && (r & R_CROSSBOWS)) rng += 2;
    if (e.type == E_SPEARMAN && (r & R_PIKES))   rng += 1;
    return rng;
}

Entity* findNearestEnemy(Entity& e, int range) {
    Entity* best = nullptr; int bestD = range + 1;
    bool concealing = isConcealing();
    for (auto& o : g.entities) {
        if (!o.alive || o.owner == e.owner) continue;
        if (o.state == S_GARRISONED) continue;
        // Non-nature units do not auto-attack nature entities (deer/wolf/sheep)
        if (e.owner != OWNER_NATURE && o.owner == OWNER_NATURE) continue;
        // Cloaking: enemy hidden if (a) night/storm and not close-detected, or
        // (b) standing in wheat and not close-detected. Buildings can't hide.
        bool inCrop = !isBuilding(o.type) && g.map[o.y][o.x].terrain == T_WHEAT;
        if ((concealing || inCrop) && o.owner != OWNER_NATURE
                && e.owner < MAX_PLAYERS
                && !isDetectedBy(o.x, o.y, e.owner)) continue;
        int d = dist(e.x, e.y, o.x, o.y);
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}

// ============================================================
// ORDERS
// ============================================================
void orderMove(Entity& e, int tx, int ty) {
    // Deployed trebuchet is rooted — must be packed (press P) before moving.
    if (e.type == E_TREBUCHET && e.packed == 0) {
        if (e.owner == 0) setStatus("Pack trebuchet first (D).");
        return;
    }
    if (e.type == E_TREBUCHET && e.packTicks > 0) {
        if (e.owner == 0) setStatus("Trebuchet is mid-deploy.");
        return;
    }
    // Warn if the clicked tile is impassable for this unit type.
    bool targetOk = isNaval(e.type) ? isPassableWater(tx, ty) : isPassable(tx, ty);
    if (!targetOk && e.owner == 0) setStatus("Can't move there.");
    e.state = S_MOVING; e.targetX = tx; e.targetY = ty; e.targetId = -1;
    e.stuckTicks = 0;
    e.attackMove = 0; e.holdPosition = 0; e.retreating = 0;
    e.path = findPathFor(e, tx, ty); e.pathIdx = 0;
    if (e.path.empty() && (e.x != tx || e.y != ty)) {
        e.state = S_IDLE;
        if (e.owner == 0) setStatus("Can't reach there.");
    }
}

// Attack-move: walk toward (tx,ty) but engage anything along the way.
static void orderAttackMove(Entity& e, int tx, int ty) {
    orderMove(e, tx, ty);
    e.attackMove = 1;
}

void orderAttack(Entity& e, int tid) {
    Entity* t = findEntity(tid);
    if (!t) return;
    // Rams demolish buildings only; sappers are walking petards — same rule.
    if ((e.type == E_RAM || e.type == E_SAPPER) && !isBuilding(t->type)) return;
    if (e.type == E_TREBUCHET && e.packed == 1) {
        if (e.owner == 0) setStatus("Deploy trebuchet first (D).");
        return;
    }
    e.holdPosition = 0; e.retreating = 0;
    e.state = S_ATTACKING; e.targetId = tid;
}

void orderGather(Entity& e, int tx, int ty) {
    Terrain ter = g.map[ty][tx].terrain;
    bool isW     = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
    bool isBerry = (ter == T_BERRY);
    bool isFishT = (ter == T_FISH);
    // Peasants gather wood/gold/berries; boats fish.
    if (e.type == E_PEASANT) {
        if (ter != T_GOLD && !isW && !isBerry) return;
        e.gatherType = (ter == T_GOLD) ? 0 : isBerry ? 3 : 1;
    } else if (e.type == E_FISHING_BOAT) {
        if (!isFishT) return;
        e.gatherType = 2;
    } else return;
    e.state = S_GATHERING; e.targetX = tx; e.targetY = ty;
    // Path to nearest adjacent passable tile so units don't block each other on the same node
    bool naval = isNaval(e.type);
    int bestAX = tx, bestAY = ty, bestAD = 99999;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx==0 && dy==0) continue;
        int nx = tx+dx, ny = ty+dy;
        if (!inBounds(nx,ny)) continue;
        bool ok = naval ? isPassableWater(nx,ny) : isPassable(nx,ny);
        if (!ok) continue;
        int d = mdist(e.x, e.y, nx, ny);
        if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
    }
    e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
    e.gatherCd = 0; e.carrying = 0;
    e.rallyX = tx; e.rallyY = ty;
}

void orderBuild(Entity& e, EntityType bt, int bx, int by) {
    if (e.type != E_PEASANT) return;
    Player& p = g.players[e.owner];
    if (p.gold < STATS[bt].costGold || p.wood < STATS[bt].costWood) {
        if (e.owner == 0) setStatus("Not enough resources!"); return;
    }
    if (!canPlace(bt, bx, by, e.owner, e.id)) {
        if (e.owner == 0) setStatus("Can't build there!"); return;
    }
    p.gold -= STATS[bt].costGold; p.wood -= STATS[bt].costWood;
    int bid = spawnEntity(bt, e.owner, bx, by, false);
    e.state = S_BUILDING; e.targetId = bid; e.targetX = bx; e.targetY = by;
    // Pick nearest passable tile adjacent to the building footprint
    int bldW = STATS[bt].sizeW, bldH = STATS[bt].sizeH;
    int bestAX = bx-1, bestAY = by, bestAD = 99999;
    for (int dy = -1; dy <= bldH; dy++) for (int dx = -1; dx <= bldW; dx++) {
        if (dx>=0 && dx<bldW && dy>=0 && dy<bldH) continue;
        int nx = bx+dx, ny = by+dy;
        if (inBounds(nx,ny) && isPassable(nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
    // Unreachable site (boxed-in builder, spot across water/walls): the order
    // used to fail silently — the peasant just stood there re-pathing forever,
    // which reads as a dead unit. Say so immediately instead.
    if (e.path.empty() && mdist(e.x, e.y, bestAX, bestAY) > 1 && e.owner == 0)
        setStatus("Builder can't reach the site!");
}

// Supply already in use plus everything currently producing or queued. Used by
// orderTrain so a flurry of queued units can't push live supply over the cap
// once they all spawn.
static int reservedSupply(int owner) {
    int used = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;
        used += STATS[e.type].supplyUsed;
        if (isBuilding(e.type) && !e.underConstruction) {
            if (e.producing != E_NONE) used += STATS[e.producing].supplyUsed;
            for (int q : e.queue) used += STATS[(EntityType)q].supplyUsed;
        }
    }
    return used;
}

// Food component of a unit's training cost. One source of truth — the
// train menus (ui.cpp) print from here, orderTrain charges from here.
int trainFoodCost(EntityType ut) {
    switch (ut) {
        case E_MILITIA: case E_ARCHER: case E_CROSSBOWMAN:
        case E_HUSSAR:  case E_WARSHIP:                    return 20;
        case E_KNIGHT:                                     return 40;
        case E_CATAPULT:                                   return 30;
        case E_TRANSPORT:                                  return 10;
        default:                                           return 0;
    }
}

void orderTrain(Entity& bld, EntityType ut) {
    if (!isBuilding(bld.type) || bld.underConstruction) return;
    // Queue if busy; reject only when queue is full.
    if (bld.producing != E_NONE && (int)bld.queue.size() >= 5) {
        if (bld.owner==0) setStatus("Queue full!"); return;
    }
    Player& p = g.players[bld.owner];
    // Workshop units need a working forge: no Blacksmith, no crossbows/petards.
    if (ut==E_CROSSBOWMAN || ut==E_SAPPER) {
        bool smith = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==bld.owner && e.type==E_BLACKSMITH && !e.underConstruction) { smith = true; break; }
        if (!smith) { if (bld.owner==0) setStatus("Requires a Blacksmith!"); return; }
    }
    if (p.gold < STATS[ut].costGold || p.wood < STATS[ut].costWood) {
        if (bld.owner==0) setStatus("Not enough resources!"); return;
    }
    if (reservedSupply(bld.owner) + STATS[ut].supplyUsed > p.supplyMax) {
        if (bld.owner==0) setStatus("Need more houses!"); return;
    }
    int foodCost = trainFoodCost(ut);
    if (p.food < foodCost) { if (bld.owner==0) setStatus("Need more food!"); return; }
    spendPlayerFood(bld.owner, foodCost);
    p.gold -= STATS[ut].costGold; p.wood -= STATS[ut].costWood;
    if (bld.producing == E_NONE) {
        bld.producing = ut; bld.prodProgress = 0; bld.prodTime = STATS[ut].trainTime;
        bld.state = S_TRAINING;
    } else {
        bld.queue.push_back((int)ut);
        if (bld.owner == 0) setStatus("Queued.");
    }
}

// Lower priority = closer to the front of the formation (toward the destination).
static int rolePriority(EntityType t) {
    switch (t) {
        case E_KNIGHT: case E_HUSSAR:        return 0;
        case E_MILITIA: case E_SPEARMAN:     return 1;
        case E_PEASANT:                      return 2;
        case E_ARCHER: case E_CROSSBOWMAN:   return 3;
        case E_CATAPULT: case E_SAPPER:      return 4;
        case E_MONK:                         return 5;
        default:                             return 6;
    }
}

// Group move with role-aware formation:
// melee occupy the rows facing the target, ranged hang back.
// If attackMove is true, all units engage opportunistically en route.
// Takes explicit unit ids — selection is UI state and never reaches the sim
// (the command funnel resolves "what's selected" before a Command is issued).
static void groupMoveCore(const std::vector<int>& unitIds, int tx, int ty, bool attackMove) {
    std::vector<Entity*> units;
    for (int id : unitIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && isUnit(e->type))
            units.push_back(e);
    }
    if (units.empty()) return;
    std::sort(units.begin(), units.end(), [](Entity* a, Entity* b) {
        return rolePriority(a->type) < rolePriority(b->type);
    });
    int N = (int)units.size();
    int cols = std::max(1, (int)ceil(sqrt((double)N)));
    int rows = (N + cols - 1) / cols;
    int half = (cols - 1) / 2;

    // Approach direction: from group centroid toward target.
    int cx = 0, cy = 0;
    for (Entity* u : units) { cx += u->x; cy += u->y; }
    cx /= N; cy /= N;
    int dx = tx - cx, dy = ty - cy;
    bool horizontal = std::abs(dx) >= std::abs(dy);
    int sx = (dx > 0) ? 1 : (dx < 0 ? -1 : 1);
    int sy = (dy > 0) ? 1 : (dy < 0 ? -1 : 1);

    // Generate slots: row 0 = the front (at the target), receding back toward approach.
    std::vector<std::pair<int,int>> slots;
    slots.reserve(N);
    for (int r = 0; r < rows && (int)slots.size() < N; r++) {
        for (int c = 0; c < cols && (int)slots.size() < N; c++) {
            int slotX, slotY;
            if (horizontal) { slotX = tx - sx*r; slotY = ty + (c - half); }
            else            { slotX = tx + (c - half); slotY = ty - sy*r; }
            slotX = std::max(0, std::min(slotX, MAP_W-1));
            slotY = std::max(0, std::min(slotY, MAP_H-1));
            slots.push_back({slotX, slotY});
        }
    }
    // Assign role-sorted units to ordered slots: front-line goes first.
    for (int i = 0; i < N && i < (int)slots.size(); i++) {
        if (attackMove) orderAttackMove(*units[i], slots[i].first, slots[i].second);
        else            orderMove(*units[i], slots[i].first, slots[i].second);
    }
    if (units[0]->owner == 0)
        setStatus(attackMove ? "Attack-move in formation!" : "Group moving in formation...");
}

void orderGroupMove(const std::vector<int>& unitIds, int tx, int ty)       { groupMoveCore(unitIds, tx, ty, false); }
void orderGroupAttackMove(const std::vector<int>& unitIds, int tx, int ty) { groupMoveCore(unitIds, tx, ty, true); }

void orderGroupAttack(const std::vector<int>& unitIds, int tid) {
    bool any0 = false;
    for (int id : unitIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && isUnit(e->type)) {
            orderAttack(*e, tid);
            if (e->owner == 0) any0 = true;
        }
    }
    if (any0) setStatus("Group attacking!");
}

// ============================================================
// GARRISON
// ============================================================
bool canGarrisonIn(EntityType bt) {
    return bt==E_TOWER || bt==E_TOWNHALL || bt==E_CASTLE || bt==E_HOUSE
        || bt==E_TRANSPORT || bt==E_RUIN;
}
int garrisonCap(EntityType bt) {
    switch (bt) {
        case E_TOWER:     return 3;
        case E_HOUSE:     return 4;
        case E_TOWNHALL:  return 6;
        case E_CASTLE:    return 10;
        case E_TRANSPORT: return 4;
        case E_RUIN:      return 6;   // a sheltering shell of old walls
        default:          return 0;
    }
}

void ejectGarrison(Entity& bld) {
    if (bld.garrison.empty()) return;
    int bw = STATS[bld.type].sizeW, bh = STATS[bld.type].sizeH;
    std::vector<std::pair<int,int>> spots;
    static bool taken[MAP_H][MAP_W];
    memset(taken, 0, sizeof(taken));
    for (int r = 1; r <= 6 && spots.size() < bld.garrison.size(); r++)
        for (int dy = -r; dy <= bh-1+r && spots.size() < bld.garrison.size(); dy++)
            for (int dx = -r; dx <= bw-1+r && spots.size() < bld.garrison.size(); dx++) {
                if (dx>=0 && dx<bw && dy>=0 && dy<bh) continue;
                int nx = bld.x+dx, ny = bld.y+dy;
                if (!inBounds(nx,ny) || taken[ny][nx]) continue;
                if (!isPassable(nx,ny) || entityAt(nx,ny)) continue;
                taken[ny][nx] = true;
                spots.push_back({nx,ny});
            }
    size_t si = 0;
    for (int uid : bld.garrison) {
        Entity* u = findEntity(uid);
        if (!u || !u->alive) continue;
        if (si < spots.size()) {
            u->x = spots[si].first; u->y = spots[si].second;
            u->state = S_IDLE; u->path.clear(); u->pathIdx = 0;
            u->stuckTicks = 0; u->targetId = -1;
            si++;
        } else {
            u->alive = false; u->state = S_DEAD;
        }
    }
    bld.garrison.clear();
    updateSupply(bld.owner);
    // An emptied ruined keep goes back to being nobody's — next claimant wins.
    if (bld.type == E_RUIN && bld.alive) bld.owner = OWNER_NATURE;
}

// Centralized death handler: marks dead, ejects garrison, ruins terrain, updates supply.
void killEntity(Entity& t) {
    if (!t.alive) return;
    t.alive = false; t.state = S_DEAD;
    // Mill destruction forfeits the food stored locally at it — the only food depot
    // that's "exposed" (TC/Castle food is treated as safe; their death ends the run anyway).
    if (t.type == E_MILL && t.carrying > 0 && t.owner >= 0 && t.owner < MAX_PLAYERS) {
        int loss = std::min(t.carrying, g.players[t.owner].food);
        g.players[t.owner].food -= loss;
        if (t.owner == 0 && loss > 0)
            setStatus(std::string("Mill destroyed! Lost ") + std::to_string(loss) + " food.");
        t.carrying = 0;
    }
    // Anything that can hold a garrison (buildings + transports) drops its cargo on death.
    if (canGarrisonIn(t.type)) ejectGarrison(t);
    if (isBuilding(t.type)) {
        // Large buildings leave a permanent scar — the floor goes to ruins.
        auto& s = STATS[t.type];
        if (s.sizeW * s.sizeH >= 4 && t.type != E_FARM) {
            for (int dy = 0; dy < s.sizeH; dy++) for (int dx = 0; dx < s.sizeW; dx++) {
                int nx = t.x+dx, ny = t.y+dy;
                if (inBounds(nx,ny)) {
                    g.map[ny][nx].terrain = T_RUINS;
                    g.map[ny][nx].preWinterTerrain = T_RUINS;
                    g.map[ny][nx].wear = 0;
                }
            }
        }
    }
    updateSupply(t.owner);
}

void orderGarrison(Entity& e, int buildingId) {
    Entity* bld = findEntity(buildingId);
    if (!bld || !bld->alive || bld->underConstruction) return;
    // Neutral ruined keeps accept anyone — garrisoning one captures it.
    bool ruinOk = (bld->type == E_RUIN && bld->owner == OWNER_NATURE);
    if (bld->owner != e.owner && !ruinOk) return;
    if (!canGarrisonIn(bld->type)) return;
    if (!isUnit(e.type) || e.type == E_CATAPULT) return;
    // Naval units can't board buildings or each other.
    if (isNaval(e.type)) return;
    if ((int)bld->garrison.size() >= garrisonCap(bld->type)) {
        if (e.owner == 0) setStatus(std::string(STATS[bld->type].name) + " is full");
        return;
    }
    e.state = S_ENTERING; e.targetId = buildingId;
    e.targetX = bld->x; e.targetY = bld->y;
    e.stuckTicks = 0;
    int bw = STATS[bld->type].sizeW, bh = STATS[bld->type].sizeH;
    int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
    for (int dy = -1; dy <= bh; dy++) for (int dx = -1; dx <= bw; dx++) {
        if (dx>=0 && dx<bw && dy>=0 && dy<bh) continue;
        int nx = bld->x+dx, ny = bld->y+dy;
        if (inBounds(nx,ny) && isPassable(nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
}

void orderHelp(Entity& e, int buildingId) {
    if (e.type != E_PEASANT) return;
    Entity* bld = findEntity(buildingId);
    if (!bld || !bld->alive) return;
    // Allow tending a completed farm; otherwise only work on buildings under construction
    if (!bld->underConstruction && bld->type != E_FARM) return;
    e.state = S_BUILDING; e.targetId = buildingId;
    e.targetX = bld->x; e.targetY = bld->y;
    int bldW = STATS[bld->type].sizeW, bldH = STATS[bld->type].sizeH;
    int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
    for (int dy = -1; dy <= bldH; dy++) for (int dx = -1; dx <= bldW; dx++) {
        if (dx>=0 && dx<bldW && dy>=0 && dy<bldH) continue;
        int nx = bld->x+dx, ny = bld->y+dy;
        if (inBounds(nx,ny) && isPassable(nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
}
