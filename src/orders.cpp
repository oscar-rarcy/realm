#include "realm.h"

void orderMove(Entity& e, int tx, int ty) {
    if (e.type == E_TREBUCHET && e.packed == 0) {
        if (e.owner == 0) setStatus("Pack the trebuchet first [D].");
        return;
    }
    e.state = S_MOVING; e.targetX = tx; e.targetY = ty; e.targetId = -1;
    e.stuckTicks = 0;
    e.attackMove = 0; e.holdPosition = 0;
    e.path = findPath(e.x, e.y, tx, ty, 300, isNaval(e.type)); e.pathIdx = 0;
    if (e.path.empty() && (e.x != tx || e.y != ty)) {
        e.state = S_IDLE;
        if (e.owner == 0) setStatus("Can't reach there.");
    } else if (e.owner == 0) {
        addActionMarker(tx, ty, 'x');
    }
}

static void orderAttackMove(Entity& e, int tx, int ty) {
    orderMove(e, tx, ty);
    e.attackMove = 1;
}

void orderAttack(Entity& e, int tid) {
    Entity* t = findEntity(tid);
    if (!t) return;
    if (e.type == E_RAM && !isBuilding(t->type)) return; // rams demolish buildings only
    if (e.type == E_TREBUCHET && e.packed == 1) {
        if (e.owner == 0) setStatus("Deploy the trebuchet first [D].");
        return;
    }
    if (e.type == E_TREBUCHET && e.packTicks > 0) return;
    e.holdPosition = 0;
    e.state = S_ATTACKING; e.targetId = tid;
    if (e.owner == 0) addActionMarker(t->x, t->y, '!');
}

void orderGather(Entity& e, int tx, int ty) {
    Entity* carcass = corpseAt(tx, ty);
    if (carcass && isHarvestableCarcass(*carcass) && e.type == E_PEASANT && canGather(e.type)) {
        e.cargo.type = CR_FOOD;
        e.cargo.sourceX = tx;
        e.cargo.sourceY = ty;
        e.resourceX = tx;
        e.resourceY = ty;
        e.state = S_GATHERING; e.targetX = tx; e.targetY = ty;
        e.targetId = -1;
        if (e.owner == 0) addActionMarker(tx, ty, '+');
        int bestAX = tx, bestAY = ty, bestAD = 99999;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (dx==0 && dy==0) continue;
            int nx = tx+dx, ny = ty+dy;
            if (!inBounds(nx,ny) || !isPassable(nx,ny)) continue;
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
        e.path = findPath(e.x, e.y, bestAX, bestAY, 300, false); e.pathIdx = 0;
        e.gatherCd = 0; e.cargo.amount = 0;
        return;
    }

    Terrain ter = g.map[ty][tx].terrain;
    CargoResource resource = resourceForTerrain(ter);
    // Workers gather land resources; naval gatherers fish.
    if (canGather(e.type) && !isNaval(e.type)) {
        if (resource != CR_GOLD && resource != CR_WOOD && resource != CR_FOOD) return;
    } else if (canGather(e.type) && isNaval(e.type)) {
        if (resource != CR_FISH) return;
    } else return;
    e.cargo.type = resource;
    e.cargo.sourceX = tx;
    e.cargo.sourceY = ty;
    e.resourceX = tx;
    e.resourceY = ty;
    e.state = S_GATHERING; e.targetX = tx; e.targetY = ty;
    if (e.owner == 0) addActionMarker(tx, ty, '+');
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
    e.path = findPath(e.x, e.y, bestAX, bestAY, 300, naval); e.pathIdx = 0;
    e.gatherCd = 0; e.cargo.amount = 0;
}

void orderBuild(Entity& e, EntityType bt, int bx, int by) {
    if (!canBuild(e.type)) return;
    Player& p = g.players[e.owner];
    if (p.gold < STATS[bt].costGold || p.wood < STATS[bt].costWood) {
        if (e.owner == 0) setStatus("Not enough resources!");
        return;
    }
    if (!canPlace(bt, bx, by, e.owner)) {
        if (e.owner == 0) setStatus("Can't build there!");
        return;
    }
    p.gold -= STATS[bt].costGold; p.wood -= STATS[bt].costWood;
    int bid = spawnEntity(bt, e.owner, bx, by, false);
    if (e.owner == 0) addActionMarker(bx, by, '#');
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
    e.path = findPath(e.x, e.y, bestAX, bestAY, 300, isNaval(e.type)); e.pathIdx = 0;
}

void orderTrain(Entity& bld, EntityType ut) {
    if (!isBuilding(bld.type) || bld.underConstruction) return;
    bool allowed = false;
    if (bld.type == E_TOWNHALL) allowed = (ut == E_PEASANT);
    else if (bld.type == E_BARRACKS) allowed = (ut == E_MILITIA || ut == E_ARCHER || ut == E_SPEARMAN || ut == E_CATAPULT || ut == E_RAM);
    else if (bld.type == E_STABLE) allowed = (ut == E_KNIGHT);
    else if (bld.type == E_CASTLE) allowed = (ut == E_PEASANT || ut == E_TREBUCHET);
    else if (bld.type == E_DOCK) allowed = (ut == E_FISHING_BOAT || ut == E_WARSHIP || ut == E_TRANSPORT);
    if (!allowed) return;
    // Queue if busy; reject only when queue is full.
    if (bld.producing != E_NONE && (int)bld.queue.size() >= 5) {
        if (bld.owner==0) setStatus("Queue full!");
        return;
    }
    Player& p = g.players[bld.owner];
    if (p.gold < STATS[ut].costGold || p.wood < STATS[ut].costWood) {
        if (bld.owner==0) setStatus("Not enough resources!");
        return;
    }
    if (reservedSupply(bld.owner) + STATS[ut].supplyUsed > p.supplyMax) {
        if (bld.owner==0) setStatus("Need more houses!");
        return;
    }
    int foodCost = 0;
    if (ut==E_MILITIA||ut==E_ARCHER||ut==E_SPEARMAN) foodCost = 20;
    else if (ut==E_KNIGHT) foodCost = 40;
    else if (ut==E_CATAPULT) foodCost = 30;
    else if (ut==E_TREBUCHET) foodCost = 30;
    else if (ut==E_WARSHIP)  foodCost = 20;
    else if (ut==E_TRANSPORT) foodCost = 10;
    if (p.food < foodCost) { if (bld.owner==0) setStatus("Need more food!"); return; }
    spendPlayerFood(bld.owner, foodCost);
    p.gold -= STATS[ut].costGold; p.wood -= STATS[ut].costWood;
    if (bld.producing == E_NONE) {
        bld.producing = ut; bld.trainProgress = 0; bld.trainTime = STATS[ut].trainTime;
        bld.state = S_TRAINING;
    } else {
        bld.queue.push_back((int)ut);
        if (bld.owner == 0) setStatus("Queued.");
    }
}

static int rolePriority(EntityType t) {
    switch (t) {
        case E_KNIGHT:   return 0;
        case E_MILITIA:  return 1;
        case E_SPEARMAN: return 1;
        case E_PEASANT:  return 2;
        case E_ARCHER:   return 3;
        case E_CATAPULT: return 4;
        case E_TREBUCHET:return 4;
        default:         return 5;
    }
}

static void groupMoveCore(int tx, int ty, bool attackMove) {
    std::vector<Entity*> units;
    for (int id : g.selectedIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && e->owner == 0 && isUnit(e->type))
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

    int cx = 0, cy = 0;
    for (Entity* u : units) { cx += u->x; cy += u->y; }
    cx /= N; cy /= N;
    int dx = tx - cx, dy = ty - cy;
    bool horizontal = std::abs(dx) >= std::abs(dy);
    int sx = (dx > 0) ? 1 : (dx < 0 ? -1 : 1);
    int sy = (dy > 0) ? 1 : (dy < 0 ? -1 : 1);

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
    for (int i = 0; i < N && i < (int)slots.size(); i++) {
        if (attackMove) orderAttackMove(*units[i], slots[i].first, slots[i].second);
        else            orderMove(*units[i], slots[i].first, slots[i].second);
    }
    setStatus(attackMove ? "Attack-move in formation!" : "Group moving in formation...");
}

void orderGroupMove(int tx, int ty)        { groupMoveCore(tx, ty, false); }
void orderGroupAttackMove(int tx, int ty)  { groupMoveCore(tx, ty, true); }

void orderGroupAttack(int tid) {
    for (int id : g.selectedIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && e->owner == 0 && isUnit(e->type))
            orderAttack(*e, tid);
    }
    setStatus("Group attacking!");
}
