#include "realm.h"
#include "core/game_events.h"
#include "core/entity_query.h"
#include "core/world_index.h"

namespace {

void emitStatus(EventSink& events, int player, const std::string& message, GameEventType type = GameEventType::StatusMessage) {
    events.emit({ type, player, -1, { -1, -1 }, message, 0 });
}

} // namespace

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
void killEntity(Game& game, EventSink& events, Entity& t) {
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
            emitStatus(events, t.owner, std::string("Mill destroyed! Lost ") + std::to_string(loss) + " food.", GameEventType::ResourcesChanged);
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
