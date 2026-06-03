#include "realm.h"
#include "core/game_events.h"
#include "core/entity_query.h"
#include "core/world_index.h"

bool canGarrisonIn(EntityType bt) {
    return bt==E_TOWER || bt==E_TOWNHALL || bt==E_CASTLE || bt==E_HOUSE || bt==E_TRANSPORT;
}
int garrisonCap(EntityType bt) {
    switch (bt) {
        case E_TOWER:     return 3;
        case E_HOUSE:     return 4;
        case E_TOWNHALL:  return 6;
        case E_CASTLE:    return 10;
        case E_TRANSPORT: return 4;
        default:          return 0;
    }
}

static void clearReferencesToEntity(Game& game, int id) {
    if (game.selectedId == id) game.selectedId = -1;
    game.selectedIds.erase(std::remove(game.selectedIds.begin(), game.selectedIds.end(), id), game.selectedIds.end());
    for (auto& group : game.controlGroups)
        group.erase(std::remove(group.begin(), group.end(), id), group.end());
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (auto& group : game.controlGroupsByOwner[p])
            group.erase(std::remove(group.begin(), group.end(), id), group.end());

    for (auto& e : game.entities) {
        if (e.targetId == id) {
            e.targetId = -1;
            e.path.clear();
            e.pathIdx = 0;
            if (e.state == S_ATTACKING || e.state == S_BUILDING || e.state == S_RETURNING || e.state == S_ENTERING)
                e.state = S_IDLE;
        }
        e.garrison.erase(std::remove(e.garrison.begin(), e.garrison.end(), id), e.garrison.end());
    }
}

void ejectGarrison(Game& game, Entity& bld) {
    if (bld.garrison.empty()) return;
    int bw = STATS[bld.type].sizeW, bh = STATS[bld.type].sizeH;
    std::vector<std::pair<int,int>> spots;
    static bool taken[MAP_H][MAP_W];
    memset(taken, 0, sizeof(taken));
    WorldIndex world = buildWorldIndex(game);
    for (int r = 1; r <= 6 && spots.size() < bld.garrison.size(); r++)
        for (int dy = -r; dy <= bh-1+r && spots.size() < bld.garrison.size(); dy++)
            for (int dx = -r; dx <= bw-1+r && spots.size() < bld.garrison.size(); dx++) {
                if (dx>=0 && dx<bw && dy>=0 && dy<bh) continue;
                int nx = bld.x+dx, ny = bld.y+dy;
                if (!inBounds(nx,ny) || taken[ny][nx]) continue;
                if (!isPassable(game,nx,ny) || entityAt(game, world, nx, ny)) continue;
                taken[ny][nx] = true;
                spots.push_back({nx,ny});
            }
    size_t si = 0;
    for (int uid : bld.garrison) {
        Entity* u = findEntity(game, world, uid);
        if (!u || !u->alive) continue;
        if (si < spots.size()) {
            u->x = spots[si].first; u->y = spots[si].second;
            u->state = S_IDLE; u->path.clear(); u->pathIdx = 0;
            u->stuckTicks = 0; u->targetId = -1;
            si++;
        } else {
            u->alive = false; u->state = S_DEAD; u->hp = 0; u->deathTicks = 0;
            u->targetId = -1; u->targetX = -1; u->targetY = -1;
            u->path.clear(); u->pathIdx = 0;
            clearReferencesToEntity(game, u->id);
        }
    }
    bld.garrison.clear();
    updateSupply(game, bld.owner);
}

// Centralized death handler: marks dead, ejects garrison, ruins terrain, updates supply.
void killEntity(Game& game, Entity& t) {
    if (!t.alive) return;
    int id = t.id;
    t.alive = false; t.state = S_DEAD; t.hp = 0; t.deathTicks = 0;
    t.targetId = -1; t.targetX = -1; t.targetY = -1;
    t.path.clear(); t.pathIdx = 0;
    clearReferencesToEntity(game, id);
    if (t.type == E_MILL && t.storedFood > 0 && t.owner >= 0 && t.owner < MAX_PLAYERS) {
        int loss = std::min(t.storedFood, game.players[t.owner].food);
        game.players[t.owner].food -= loss;
        t.storedFood = 0;
        if (loss > 0)
            emitStatusEvent(t.owner, std::string("Mill destroyed! Lost ") + std::to_string(loss) + " food.", GameEventType::ResourcesChanged);
    }
    // Anything that can hold a garrison (buildings + transports) drops its cargo on death.
    if (canGarrisonIn(t.type)) ejectGarrison(game, t);
    if (isBuilding(t.type)) {
        // Large buildings leave a permanent scar — the floor goes to ruins.
        auto& s = STATS[t.type];
        if (s.sizeW * s.sizeH >= 4 && t.type != E_FARM) {
            for (int dy = 0; dy < s.sizeH; dy++) for (int dx = 0; dx < s.sizeW; dx++) {
                int nx = t.x+dx, ny = t.y+dy;
                if (inBounds(nx,ny)) {
                    game.map[ny][nx].terrain = T_RUINS;
                    game.map[ny][nx].preWinterTerrain = T_RUINS;
                    game.map[ny][nx].wear = 0;
                }
            }
        }
    }
    updateSupply(game, t.owner);
}

void orderGarrison(Game& game, const WorldIndex& world, Entity& e, int buildingId) {
    Entity* bld = findEntity(game, world, buildingId);
    if (!bld || !bld->alive || bld->underConstruction) return;
    if (bld->owner != e.owner) return;
    if (!canGarrisonIn(bld->type)) return;
    if (!isUnit(e.type) || isSiege(e.type)) return;
    // Naval units can't board buildings or each other.
    if (isNaval(e.type)) return;
    if ((int)bld->garrison.size() >= garrisonCap(bld->type)) {
        if (e.owner == 0) emitStatusEvent(e.owner, std::string(STATS[bld->type].name) + " is full", GameEventType::CommandRejected);
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
        if (inBounds(nx,ny) && isPassable(game,nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPath(game, e.x, e.y, bestAX, bestAY, 300, isNaval(e.type)); e.pathIdx = 0;
}

void orderHelp(Game& game, const WorldIndex& world, Entity& e, int buildingId) {
    if (!canBuild(e.type)) return;
    Entity* bld = findEntity(game, world, buildingId);
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
        if (inBounds(nx,ny) && isPassable(game,nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPath(game, e.x, e.y, bestAX, bestAY, 300, isNaval(e.type)); e.pathIdx = 0;
}
