#include "realm.h"
#include "core/game_events.h"
#include "core/order_service.h"
#include "core/world_index.h"

void moveAlongPath(Game& game, const WorldIndex& world, Entity& e) {
    if (e.pathIdx >= (int)e.path.size()) {
        e.path.clear(); e.pathIdx = 0;
        e.stuckTicks = 0;
        if (e.state == S_MOVING) e.state = S_IDLE;
        return;
    }
    if (e.moveCd > 0) { e.moveCd--; return; }
    auto [nx, ny] = e.path[e.pathIdx];
    if (!isNaval(e.type) && !isLandPassableWithBridges(game, world, nx, ny)) {
        e.stuckTicks++;
        return;
    }
    // Units share tiles freely; buildings block, except open gates
    Entity* blk = entityAt(game, world, nx, ny);
    if (blk && blk->id != e.id && isBuilding(blk->type)) {
        if (buildingBlocksLandMovement(*blk)) {
            // Tolerate transient blocks; only repath after several stuck ticks (staggered by id).
            e.stuckTicks++;
            int threshold = 2 + (e.id % 3);
            if (e.stuckTicks >= threshold) {
                e.stuckTicks = 0;
                int gx = e.path.empty() ? e.targetX : e.path.back().first;
                int gy = e.path.empty() ? e.targetY : e.path.back().second;
                // If the original goal tile is now blocked by a building, target the closest free tile around it.
                Entity* goalBlocker = entityAt(game, world, gx, gy);
                if (!isPassable(game, gx, gy) || (goalBlocker && isBuilding(goalBlocker->type))) {
                    int bestD = 99999, bx = gx, by = gy;
                    for (int r = 1; r <= 4 && bestD == 99999; r++)
                        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                            if (std::abs(dx) != r && std::abs(dy) != r) continue;
                            int qx = gx+dx, qy = gy+dy;
                            if (!inBounds(qx,qy) || !isPassable(game,qx,qy)) continue;
                            Entity* o = entityAt(game, world, qx, qy);
                            if (o && isBuilding(o->type)) continue;
                            int d = mdist(e.x, e.y, qx, qy);
                            if (d < bestD) { bestD = d; bx = qx; by = qy; }
                        }
                    gx = bx; gy = by;
                }
                e.path = findPath(game, e.x, e.y, gx, gy, 300, isNaval(e.type)); e.pathIdx = 0;
            }
            return;
        }
    }
    int fromX = e.x;
    int fromY = e.y;
    e.facingDx = (nx > e.x) - (nx < e.x);
    e.facingDy = (ny > e.y) - (ny < e.y);
    e.x = nx; e.y = ny; e.pathIdx++;
    e.stuckTicks = 0;
    Tile& tt = game.map[ny][nx];
    Terrain ter = tt.terrain;
    const bool roadSurface = tileHasRoadVisual(tt);
    int spd = STATS[e.type].speed;
    if (roadSurface||ter==T_DIRT||ter==T_CASTLE_FLOOR) spd = std::max(1, spd-1);
    else if (ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW||ter==T_ICE||ter==T_ASH) spd += 1;
    else if (ter==T_MUD) spd += 2; // bogged down
    spd += movementPenaltyForTile(tt);
    if ((ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) && e.type == E_KNIGHT) spd += 1;
    if (getSeason(game) == WINTER) spd = std::max(spd, STATS[e.type].speed+1);
    // Weather: rain and storm bog down movement on natural ground.
    if (game.weather != W_CLEAR && !roadSurface && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS
            ||ter==T_MEADOW||ter==T_DIRT||ter==T_SAND||ter==T_DUNES))
        spd += (game.weather == W_STORM) ? 2 : 1;
    e.moveCd = spd;
    e.visualMoveFromX = fromX;
    e.visualMoveFromY = fromY;
    e.visualMoveToX = nx;
    e.visualMoveToY = ny;
    e.visualMoveStartedTick = game.tick;
    e.visualMoveDurationTicks = std::max(1, spd + 1);
    e.visualMoveSeq++;

    // Path wear — natural ground gets compacted into dirt then road by repeated traffic.
    bool pavable = (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW
                 ||ter==T_DIRT||ter==T_SAND||ter==T_DUNES);
    if (pavable && tt.wear < 100) {
        tt.wear += 1;
        if (tt.wear >= 80) {
            promoteTileToLegacyRoad(tt);
        } else if (tt.wear >= 40 && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW)) {
            tt.terrain = T_DIRT; tt.preWinterTerrain = T_DIRT;
        }
    }
}

bool findNearbyResource(Game& game, const WorldIndex& world, EventSink& events, Entity& e) {
    if (e.cargo.type == CR_NONE) return false;
    int bestD = 99999, bx = -1, by = -1;
    int r = FOG_RADIUS * 4;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        int nx = e.x+dx, ny = e.y+dy;
        if (!inBounds(nx,ny)) continue;
        if (e.owner < OWNER_NATURE && !game.map[ny][nx].explored[e.owner]) continue;
        if (game.map[ny][nx].resources <= 0) continue;
        Terrain t = game.map[ny][nx].terrain;
        if (!terrainMatchesResource(t, e.cargo.type)) continue;
        int d = mdist(e.x, e.y, nx, ny);
        if (d < bestD) { bestD = d; bx = nx; by = ny; }
    }
    if (bx >= 0) {
        startGather(game, world, events, e.owner, Selection{ e.id, { e.id } }, { bx, by });
        return true;
    }
    return false;
}
