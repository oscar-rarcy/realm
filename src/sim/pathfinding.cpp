#include "realm.h"

std::vector<std::pair<int,int>> findPathFor(const Game& game, Entity& e, int tx, int ty) {
    return findPath(game, e.x, e.y, tx, ty, 300, isNaval(e.type));
}

std::vector<std::pair<int,int>> findPath(const Game& game, int sx, int sy, int tx, int ty, int /*maxSteps*/, bool naval) {
    if (sx == tx && sy == ty) return {};
    auto pass = [&](int x, int y) { return naval ? isPassableWater(game,x,y) : isPassable(game,x,y); };
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

    OccupancyGrid blockers{};
    buildOccupancyGrid(game, blockers, false, true);
    blockers.occupied[ty][tx] = false; // always allow reaching the destination

    static int  gScore[MAP_H][MAP_W];
    static int  visited[MAP_H][MAP_W];  // == vgen → discovered (g+parent valid)
    static int  closed [MAP_H][MAP_W];  // == vgen → expanded (final g)
    static int  vgen = 0;
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
            if (!pass(nx,ny) || isOccupied(blockers, nx, ny)) continue;
            // Forbid corner-cutting between two blocked cardinals on a diagonal step.
            if (i & 1) {
                int hx = cx+dx8[i], hy = cy;
                int vx = cx,         vy = cy+dy8[i];
                if (!pass(hx,hy) || isOccupied(blockers, hx, hy)) continue;
                if (!pass(vx,vy) || isOccupied(blockers, vx, vy)) continue;
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
