#include "realm.h"

// Pathfinding, split out of entity.cpp: the A* search (findPath) and the
// per-tick path follower (moveAlongPath). findPathFor() stays inline in
// realm.h since combat.cpp uses it too. No behaviour change.

// ============================================================
// PATHFINDING — A* with octile distance (10 cardinal, 14 diagonal).
// Returns path to closest reachable tile if the target itself is unreachable,
// so a click always produces motion toward the destination.
// findPathFor() lives inline in realm.h since combat.cpp and entity.cpp both need it.
// ============================================================
std::vector<std::pair<int,int>> findPath(int sx, int sy, int tx, int ty, int /*maxSteps*/, bool naval) {
    if (sx == tx && sy == ty) return {};
    auto pass = [&](int x, int y) { return naval ? isPassableWater(x,y) : isPassable(x,y); };
    // If target tile is blocked, retarget to its nearest passable neighbor.
    if (!pass(tx, ty)) {
        int bestD = 9999, bx = -1, by = -1;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int nx = tx+dx, ny = ty+dy;
            if (pass(nx,ny)) { int d=mdist(sx,sy,nx,ny); if(d<bestD){bestD=d;bx=nx;by=ny;} }
        }
        if (bx < 0) return {};
        tx = bx; ty = by;
    }

    // Pre-scan buildings into a flat bool map — O(1) lookup vs O(N) entityAt per step.
    // Only buildings block paths; units (incl. boats) share tiles, see moveAlongPath.
    static bool bldMap[MAP_H][MAP_W];
    memset(bldMap, 0, sizeof(bldMap));
    for (auto& e : g.entities) {
        if (!e.alive) continue;
        if (isBuilding(e.type)) {
            if (e.type == E_GATE && e.gateOpen) continue;
            auto& s = STATS[e.type];
            for (int dy2 = 0; dy2 < s.sizeH; dy2++) for (int dx2 = 0; dx2 < s.sizeW; dx2++) {
                int bx = e.x+dx2, by = e.y+dy2;
                if (inBounds(bx,by)) bldMap[by][bx] = true;
            }
        }
    }
    bldMap[ty][tx] = false; // always allow reaching the destination

    static int       gScore[MAP_H][MAP_W];
    static unsigned  visited[MAP_H][MAP_W];  // == vgen → discovered (g+parent valid)
    static unsigned  closed [MAP_H][MAP_W];  // == vgen → expanded (final g)
    // unsigned: wraparound is well-defined; signed overflow was UB.
    static unsigned  vgen = 0;
    static std::pair<int8_t,int8_t> parent[MAP_H][MAP_W];
    static const int dx8[]   = {0,1,1,1,0,-1,-1,-1};
    static const int dy8[]   = {-1,-1,0,1,1,1,0,-1};
    static const int cost8[] = {10,14,10,14,10,14,10,14};

    auto h = [&](int x, int y) {
        int dx = std::abs(x-tx), dy = std::abs(y-ty);
        return 10 * std::max(dx,dy) + 4 * std::min(dx,dy);
    };

    vgen++;
    using Node = std::tuple<int,int,int>; // (f, x, y) — min-heap via std::greater
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    gScore[sy][sx] = 0;
    visited[sy][sx] = vgen;
    parent[sy][sx] = {0,0};
    open.push({h(sx,sy), sx, sy});

    // Track best progress toward target for the unreachable-fallback path.
    int bestH = h(sx,sy), bestX = sx, bestY = sy;
    bool found = false;
    int iter = 0;
    const int MAX_ITER = 4000;

    while (!open.empty() && iter++ < MAX_ITER) {
        auto [f, cx, cy] = open.top(); open.pop();
        if (closed[cy][cx] == vgen) continue;
        closed[cy][cx] = vgen;

        if (cx == tx && cy == ty) { found = true; break; }
        int ch = h(cx, cy);
        if (ch < bestH) { bestH = ch; bestX = cx; bestY = cy; }
        int gc = gScore[cy][cx];

        for (int i = 0; i < 8; i++) {
            int nx = cx+dx8[i], ny = cy+dy8[i];
            if (!inBounds(nx,ny)) continue;
            if (closed[ny][nx] == vgen) continue;
            if (!pass(nx,ny) || bldMap[ny][nx]) continue;
            if (!canStep(cx,cy,nx,ny,naval)) continue;  // cliffs block, ramps connect
            // Forbid corner-cutting between two blocked cardinals on a diagonal step.
            if (i & 1) {
                int hx = cx+dx8[i], hy = cy;
                int vx = cx,         vy = cy+dy8[i];
                if (!pass(hx,hy) || bldMap[hy][hx] || !canStep(cx,cy,hx,hy,naval)) continue;
                if (!pass(vx,vy) || bldMap[vy][vx] || !canStep(cx,cy,vx,vy,naval)) continue;
            }
            int ng = gc + cost8[i];
            if (visited[ny][nx] == vgen && ng >= gScore[ny][nx]) continue;
            visited[ny][nx] = vgen;
            gScore[ny][nx] = ng;
            parent[ny][nx] = {(int8_t)(cx-nx),(int8_t)(cy-ny)};
            open.push({ng + h(nx,ny), nx, ny});
        }
    }

    int cx, cy;
    if (found) { cx = tx; cy = ty; }
    else if (bestX == sx && bestY == sy) return {};  // no progress possible
    else { cx = bestX; cy = bestY; }

    std::vector<std::pair<int,int>> path;
    while (cx != sx || cy != sy) {
        path.push_back({cx,cy});
        auto [ddx,ddy] = parent[cy][cx];
        cx += ddx; cy += ddy;
    }
    std::reverse(path.begin(), path.end());
    return path;
}

// ============================================================
// MOVEMENT
// ============================================================
void moveAlongPath(Entity& e) {
    // A trebuchet only travels in wagon configuration. Deployed (or mid-
    // transition) it is anchored to the ground — every mover must respect
    // that, not just orderMove (the S_ATTACKING chase used to walk it).
    if (e.type == E_TREBUCHET && (e.packed == 0 || e.packTicks > 0)) return;
    if (e.pathIdx >= (int)e.path.size()) {
        e.path.clear(); e.pathIdx = 0;
        e.stuckTicks = 0; e.chargeSteps = 0;
        if (e.state == S_MOVING) e.state = S_IDLE;
        return;
    }
    if (e.moveCd > 0) { e.moveCd--; return; }
    auto [nx, ny] = e.path[e.pathIdx];
    // The world changes under stale paths — thaw turns ice back to water,
    // walls go up, plateau rims appear. Re-validate every step instead of
    // walking blind into terrain that no longer carries this unit.
    bool naval = isNaval(e.type);
    bool stepOk = (naval ? isPassableWater(nx, ny) : isPassable(nx, ny))
               && canStep(e.x, e.y, nx, ny, naval);
    if (!stepOk) {
        int gx = e.path.back().first, gy = e.path.back().second;
        e.path = findPathFor(e, gx, gy); e.pathIdx = 0;
        e.chargeSteps = 0;   // a turned-aside charge is no charge
        // Goal unreachable now — stand down rather than spin re-pathing.
        if (e.path.empty()) { if (e.state == S_MOVING) e.state = S_IDLE; }
        return;
    }
    // Units share tiles freely; buildings block, except open gates
    Entity* blk = entityAt(nx, ny);
    if (blk && blk->id != e.id && isBuilding(blk->type)) {
        bool isOpenGate = (blk->type == E_GATE && blk->gateOpen);
        if (!isOpenGate) {
            e.chargeSteps = 0;   // blocked — momentum lost
            // Tolerate transient blocks; only repath after several stuck ticks (staggered by id).
            // Tightened threshold (was 3+id%5) so stuck units recover quicker.
            e.stuckTicks++;
            int threshold = 2 + (e.id % 3);
            if (e.stuckTicks >= threshold) {
                e.stuckTicks = 0;
                int gx = e.path.empty() ? e.targetX : e.path.back().first;
                int gy = e.path.empty() ? e.targetY : e.path.back().second;
                // If the original goal tile is now blocked by a building, target the closest free tile around it.
                if (!isPassable(gx, gy) || (entityAt(gx,gy) && isBuilding(entityAt(gx,gy)->type))) {
                    int bestD = 99999, bx = gx, by = gy;
                    for (int r = 1; r <= 4 && bestD == 99999; r++)
                        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                            if (std::abs(dx) != r && std::abs(dy) != r) continue;
                            int qx = gx+dx, qy = gy+dy;
                            if (!inBounds(qx,qy) || !isPassable(qx,qy)) continue;
                            Entity* o = entityAt(qx,qy);
                            if (o && isBuilding(o->type)) continue;
                            int d = mdist(e.x, e.y, qx, qy);
                            if (d < bestD) { bestD = d; bx = qx; by = qy; }
                        }
                    gx = bx; gy = by;
                }
                e.path = findPathFor(e, gx, gy); e.pathIdx = 0;
            }
            return;
        }
    }
    e.x = nx; e.y = ny; e.pathIdx++;
    e.stuckTicks = 0;
    // Cavalry build a charge over consecutive strides; siege engines lose
    // their entrenchment the moment they roll; marching tires the men.
    if (e.type == E_KNIGHT || e.type == E_HUSSAR) { if (e.chargeSteps < 99) e.chargeSteps++; }
    if (e.type == E_CATAPULT) e.entrenchTicks = 0;
    if (hasMorale(e.type) && (g.tick + e.id) % 3 == 0)
        e.stamina = std::max(0, e.stamina - 1);
    Terrain ter = g.map[ny][nx].terrain;
    int spd = STATS[e.type].speed;
    // Roads (and bridges) are strictly faster than everything else. Dirt no
    // longer shares the bonus — it's the halfway stage; let traffic finish
    // the road (wear 80) to earn the speed. Makes road networks worth having.
    if (ter==T_ROAD||ter==T_BRIDGE||ter==T_CASTLE_FLOOR) spd = std::max(1, spd-1);
    else if (ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW||ter==T_ICE||ter==T_ASH) spd += 1;
    else if (ter==T_MUD) spd += 2; // bogged down
    // Forests impede everyone; cavalry takes a worse penalty since trees
    // negate the open-ground speed advantage that justifies their cost.
    else if (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) {
        spd += (e.type == E_KNIGHT) ? 2 : 1;
    }
    if (getSeason() == WINTER) spd = std::max(spd, STATS[e.type].speed+1);
    // Weather: rain and storm bog down movement on natural ground.
    bool naturalGround = (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS
            ||ter==T_MEADOW||ter==T_DIRT||ter==T_SAND||ter==T_DUNES);
    if (g.weather != W_CLEAR && naturalGround)
        spd += (g.weather == W_STORM) ? 2 : 1;
    // Mud season: spring soil swallows wheels. Siege engines crawl off-road —
    // launch your sieges in summer, or build roads first.
    if (getSeason() == SPRING
        && (e.type==E_CATAPULT || e.type==E_RAM || e.type==E_TREBUCHET)
        && (naturalGround || ter==T_MUD))
        spd += 2;
    // Winded troops drag their feet; broken men run faster than they march.
    if (hasMorale(e.type) && e.stamina < 30) spd += 1;
    if (e.state == S_ROUTING) spd = std::max(1, spd - 1);
    e.moveCd = spd;

    // Path wear — natural ground gets compacted into dirt then road by repeated traffic.
    Tile& tt = g.map[ny][nx];
    bool pavable = (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW
                 ||ter==T_DIRT||ter==T_SAND||ter==T_DUNES);
    if (pavable && tt.wear < 100) {
        tt.wear += 1;
        if (tt.wear >= 80 && ter != T_ROAD) {
            tt.terrain = T_ROAD; tt.preWinterTerrain = T_ROAD;
        } else if (tt.wear >= 40 && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW)) {
            tt.terrain = T_DIRT; tt.preWinterTerrain = T_DIRT;
        }
    }
}
