#include "realm.h"
#include <cmath>

// ============================================================
// TIME
// ============================================================
float getBrightness() { return std::max(0.0f, std::min(1.0f, sinf(g.dayPhase * M_PI))); }
Season getSeason() { return (Season)((int)g.seasonPhase % 4); }
float getSeasonProgress() { return g.seasonPhase - (int)g.seasonPhase; }
const char* getSeasonName() {
    const char* n[] = {"Spring","Summer","Autumn","Winter"};
    return n[getSeason()];
}
const char* getTimeName() {
    float b = getBrightness();
    if (b > 0.85f) return "Noon";
    if (b > 0.6f)  return "Day";
    if (b > 0.35f) return g.dayPhase < 0.5f ? "Dawn" : "Dusk";
    if (b > 0.15f) return "Twilight";
    return "Night";
}
bool isNight() { return getBrightness() < 0.3f; }
bool isDusk()  { float b = getBrightness(); return b >= 0.3f && b < 0.55f && g.dayPhase > 0.5f; }
bool isDawn()  { float b = getBrightness(); return b >= 0.3f && b < 0.55f && g.dayPhase < 0.5f; }

// ============================================================
// HELPERS
// ============================================================
int  dist(int x1,int y1,int x2,int y2)  { return std::max(std::abs(x1-x2), std::abs(y1-y2)); }
int  mdist(int x1,int y1,int x2,int y2) { return std::abs(x1-x2) + std::abs(y1-y2); }
bool inBounds(int x, int y)              { return x >= 0 && x < MAP_W && y >= 0 && y < MAP_H; }

bool isPassable(int x, int y) {
    if (!inBounds(x, y)) return false;
    Terrain t = g.map[y][x].terrain;
    // Land units can wade through shallows and reeds (slow, see moveAlongPath), but
    // deep water and fish shoals block them. Winter freezes water → T_ICE which is
    // passable everywhere as a slick. Mountains/stone/walls always block.
    return t != T_MOUNTAIN && t != T_WATER && t != T_STONE && t != T_CASTLE_WALL
        && t != T_FISH && t != T_LAVA;
}

bool isPassableWater(int x, int y) {
    if (!inBounds(x, y)) return false;
    Terrain t = g.map[y][x].terrain;
    // Boats float on open water and shallows, glide through reeds. Marsh and ice block them.
    return t == T_WATER || t == T_SHALLOWS || t == T_REEDS || t == T_FISH;
}

// Cloaking: at night or under storm, only short-range or torch-lit eyes see enemies.
bool isConcealing() { return isNight() || g.weather == W_STORM; }

static bool detectMap[MAX_PLAYERS][MAP_H][MAP_W];
static int  detectMapTick[MAX_PLAYERS] = {-1,-1,-1,-1};

// Force the detect-map cache to rebuild on its next access. Called from initGame
// so stale data from a previous match can't be served up at the new tick 0.
void resetDetectMapCache() {
    for (int p = 0; p < MAX_PLAYERS; p++) detectMapTick[p] = -1;
}

static void ensureDetectMap(int observerOwner) {
    if (observerOwner < 0 || observerOwner >= MAX_PLAYERS) return;
    if (detectMapTick[observerOwner] == g.tick) return;
    memset(detectMap[observerOwner], 0, sizeof(detectMap[observerOwner]));
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != observerOwner || e.state == S_GARRISONED) continue;
        if (e.underConstruction) continue; // unfinished walls have no eyes yet
        // Buildings with sight: tower / castle / church / TH light up a wider radius.
        bool torch = (e.type == E_TOWER || e.type == E_CASTLE
                  || e.type == E_CHURCH || e.type == E_TOWNHALL);
        int range = torch ? 7 : 3;
        auto& s = STATS[e.type];
        int cx = e.x + s.sizeW/2, cy = e.y + s.sizeH/2;
        for (int dy = -range; dy <= range; dy++) for (int dx = -range; dx <= range; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            if (dx*dx + dy*dy <= range*range) detectMap[observerOwner][ny][nx] = true;
        }
    }
    detectMapTick[observerOwner] = g.tick;
}

bool isDetectedBy(int x, int y, int observerOwner) {
    // No short-circuit on time of day: wheat crops conceal enemies in broad
    // daylight as well as night. Callers (render.cpp) already gate on whether
    // concealment applies — this just answers "is a friendly eye nearby?".
    if (observerOwner < 0 || observerOwner >= MAX_PLAYERS) return true;
    if (!inBounds(x, y)) return false;
    ensureDetectMap(observerOwner);
    return detectMap[observerOwner][y][x];
}

void setStatus(const std::string& msg) { g.statusMsg = msg; g.statusTimer = 35; }

// Food can be deposited at a Mill, Town Hall, or Castle. A Mill tracks how much
// of the player's food currently lives at its location — if the Mill is destroyed,
// that share is forfeited. TC/Castle food is treated as safe (loss of either ends the run).
static void addPlayerFood(int owner, int amount, Entity* depot) {
    g.players[owner].food += amount;
    if (depot && depot->type == E_MILL) depot->carrying += amount;
}

// Drain Mill stockpiles first so the exposed share is consumed before the safer
// reserve held at TC/Castle. Keeps mill.carrying consistent with player.food.
void spendPlayerFood(int owner, int amount) {
    Player& p = g.players[owner];
    int spent = std::min(amount, p.food);
    p.food -= spent;
    int remaining = spent;
    for (auto& e : g.entities) {
        if (remaining <= 0) break;
        if (!e.alive || e.owner != owner || e.type != E_MILL || e.underConstruction) continue;
        int take = std::min(e.carrying, remaining);
        e.carrying -= take; remaining -= take;
    }
}

Entity* findEntity(int id) {
    for (auto& e : g.entities) if (e.id == id && e.alive) return &e;
    return nullptr;
}

Entity* findDepot(Entity& e) {
    Entity* best = nullptr; int bestD = 99999;
    for (auto& o : g.entities) {
        if (!o.alive || o.owner != e.owner || o.underConstruction) continue;
        bool isBase = (o.type == E_TOWNHALL || o.type == E_CASTLE) && e.gatherType != 2;
        bool isWood = (o.type == E_LUMBER_CAMP && e.gatherType == 1);
        bool isGold = (o.type == E_MINING_CAMP && e.gatherType == 0);
        bool isFish = (o.type == E_DOCK        && e.gatherType == 2);
        // Mill accepts food deliveries from farm couriers (and berry pickers).
        bool isFood = (o.type == E_MILL        && e.gatherType == 3);
        if (isBase || isWood || isGold || isFish || isFood) {
            int d = mdist(e.x, e.y, o.x, o.y);
            if (d < bestD) { bestD = d; best = &o; }
        }
    }
    return best;
}

Entity* entityAt(int x, int y) {
    for (auto& e : g.entities) {
        if (!e.alive || e.state == S_GARRISONED) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

Entity* entityAtOwner(int x, int y, int owner) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner || e.state == S_GARRISONED) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

bool canPlace(EntityType type, int x, int y, int owner) {
    (void)owner;
    // Top-level bounds check protects every g.map read below, including the
    // farm-only terrain read that previously ran before any inBounds check.
    if (!inBounds(x, y)) return false;
    // Farms can only be sown on open ground, not in winter
    if (type == E_FARM) {
        if (getSeason() == WINTER) return false;
        Terrain t = g.map[y][x].terrain;
        if (t!=T_GRASS&&t!=T_MEADOW&&t!=T_TALL_GRASS&&t!=T_FLOWERS&&t!=T_DIRT&&t!=T_WHEAT&&t!=T_SNOW) return false;
    }
    auto& s = STATS[type];
    for (int dy = 0; dy < s.sizeH; dy++) for (int dx = 0; dx < s.sizeW; dx++) {
        int nx = x+dx, ny = y+dy;
        if (!inBounds(nx,ny) || !isPassable(nx,ny)) return false;
        Terrain ter = g.map[ny][nx].terrain;
        if (ter == T_GOLD) return false;
        // Forests are resource terrain — chop the trees before you can build here.
        if (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) return false;
        // Land buildings need solid ground — shallows, marsh, reeds and ice are
        // walkable for units but not foundations. (Docks are special: their
        // footprint must be land, plus one neighbour must be water — handled below.)
        if (ter==T_SHALLOWS||ter==T_MARSH||ter==T_REEDS||ter==T_ICE) return false;
        if (entityAt(nx,ny)) return false;
    }
    // Docks must sit on the shoreline — at least one neighbouring tile must be water.
    if (type == E_DOCK) {
        bool touchesWater = false;
        for (int dy = -1; dy <= s.sizeH && !touchesWater; dy++)
            for (int dx = -1; dx <= s.sizeW && !touchesWater; dx++) {
                if (dx >= 0 && dx < s.sizeW && dy >= 0 && dy < s.sizeH) continue;
                int nx = x+dx, ny = y+dy;
                if (inBounds(nx,ny) && isPassableWater(nx,ny)) touchesWater = true;
            }
        if (!touchesWater) return false;
    }
    return true;
}

void updateSupply(int owner) {
    int mx = 0, used = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;
        if (!e.underConstruction) mx += STATS[e.type].supplyProvided;
        used += STATS[e.type].supplyUsed;
    }
    g.players[owner].supplyMax = mx;
    g.players[owner].supply    = used;
}

// ============================================================
// PROJECTILES
// ============================================================
void spawnProjectile(int sx, int sy, int tx, int ty, char gl, int col) {
    float dx = (float)(tx-sx), dy = (float)(ty-sy);
    float len = sqrtf(dx*dx + dy*dy);
    if (len < 0.1f) return;
    Projectile p;
    p.x = (float)sx; p.y = (float)sy; p.tx = (float)tx; p.ty = (float)ty;
    p.glyph = gl; p.color = col; p.life = (int)(len/1.5f)+2; p.alive = true;
    g.projectiles.push_back(p);
}

void tickProjectiles() {
    for (auto& p : g.projectiles) {
        if (!p.alive) continue;
        float dx = p.tx-p.x, dy = p.ty-p.y, len = sqrtf(dx*dx+dy*dy);
        if (len < 1.5f || p.life <= 0) { p.alive = false; continue; }
        p.x += (dx/len)*1.5f; p.y += (dy/len)*1.5f; p.life--;
    }
    if (g.tick % 30 == 0)
        g.projectiles.erase(std::remove_if(g.projectiles.begin(), g.projectiles.end(),
            [](const Projectile& p){ return !p.alive; }), g.projectiles.end());
}

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
    // Boats also block each other since they occupy water tiles.
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
            // Forbid corner-cutting between two blocked cardinals on a diagonal step.
            if (i & 1) {
                int hx = cx+dx8[i], hy = cy;
                int vx = cx,         vy = cy+dy8[i];
                if (!pass(hx,hy) || bldMap[hy][hx]) continue;
                if (!pass(vx,vy) || bldMap[vy][vx]) continue;
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
// SPAWN / FOG
// ============================================================
int spawnEntity(EntityType type, int owner, int x, int y, bool built) {
    Entity e{};
    e.id = g.nextId++; e.type = type; e.owner = owner; e.x = x; e.y = y;
    e.maxHp = STATS[type].maxHp; e.hp = built ? e.maxHp : 1;
    e.state = S_IDLE; e.targetId = -1; e.targetX = -1; e.targetY = -1;
    e.producing = E_NONE; e.underConstruction = !built; e.alive = true;
    e.rallyX = x + STATS[type].sizeW; e.rallyY = y + STATS[type].sizeH;
    if (type == E_FISHING_BOAT) e.gatherType = 2; // fish
    if (type == E_TREBUCHET)   { e.packed = 1; e.packTicks = 0; } // spawn mobile
    g.entities.push_back(e);
    updateSupply(owner);
    return e.id;
}

void updateFog() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        for (int p = 0; p < MAX_PLAYERS; p++) g.map[y][x].visible[p] = false;
    int nightPen = isNight() ? 2 : (isDusk()||isDawn()) ? 1 : 0;
    if (getSeason() == WINTER) nightPen += 1; // blizzards eat sight
    if (g.weather == W_STORM) nightPen += 1;
    else if (g.weather == W_RAIN || g.weather == W_SNOW) nightPen += (nightPen > 0 ? 0 : 1);
    for (auto& e : g.entities) {
        if (!e.alive || e.owner >= OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (e.underConstruction) continue; // scaffold doesn't see
        int r = FOG_RADIUS - nightPen;
        if (isBuilding(e.type)) r += 2;
        if (e.type == E_TOWER)  r += 4;
        if (e.type == E_CASTLE) r += 3;
        if (e.type == E_CHURCH) r += 3;
        if (r < 3) r = 3;
        auto& s = STATS[e.type];
        int cx = e.x + s.sizeW/2, cy = e.y + s.sizeH/2;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && dx*dx+dy*dy <= r*r) {
                g.map[ny][nx].visible[e.owner]  = true;
                g.map[ny][nx].explored[e.owner] = true;
            }
        }
    }
}


// ============================================================
// MOVEMENT
// ============================================================
void moveAlongPath(Entity& e) {
    if (e.pathIdx >= (int)e.path.size()) {
        e.path.clear(); e.pathIdx = 0;
        e.stuckTicks = 0;
        if (e.state == S_MOVING) e.state = S_IDLE;
        return;
    }
    if (e.moveCd > 0) { e.moveCd--; return; }
    auto [nx, ny] = e.path[e.pathIdx];
    // Units share tiles freely; buildings block, except open gates
    Entity* blk = entityAt(nx, ny);
    if (blk && blk->id != e.id && isBuilding(blk->type)) {
        bool isOpenGate = (blk->type == E_GATE && blk->gateOpen);
        if (!isOpenGate) {
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
    Terrain ter = g.map[ny][nx].terrain;
    int spd = STATS[e.type].speed;
    if (ter==T_ROAD||ter==T_DIRT||ter==T_CASTLE_FLOOR) spd = std::max(1, spd-1);
    else if (ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW||ter==T_ICE||ter==T_ASH) spd += 1;
    else if (ter==T_MUD) spd += 2; // bogged down
    if (getSeason() == WINTER) spd = std::max(spd, STATS[e.type].speed+1);
    // Weather: rain and storm bog down movement on natural ground.
    if (g.weather != W_CLEAR && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS
            ||ter==T_MEADOW||ter==T_DIRT||ter==T_SAND||ter==T_DUNES))
        spd += (g.weather == W_STORM) ? 2 : 1;
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

// Scan explored/visible tiles for a resource matching e.gatherType; re-issue gather if found.
static bool findNearbyResource(Entity& e) {
    int bestD = 99999, bx = -1, by = -1;
    int r = FOG_RADIUS * 4;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        int nx = e.x+dx, ny = e.y+dy;
        if (!inBounds(nx,ny)) continue;
        if (e.owner < OWNER_NATURE && !g.map[ny][nx].explored[e.owner]) continue;
        if (g.map[ny][nx].resources <= 0) continue;
        Terrain t = g.map[ny][nx].terrain;
        bool match = false;
        if      (e.gatherType == 0) match = (t == T_GOLD);
        else if (e.gatherType == 1) match = (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE);
        else if (e.gatherType == 2) match = (t == T_FISH);
        else if (e.gatherType == 3) match = (t == T_BERRY);
        if (!match) continue;
        int d = mdist(e.x, e.y, nx, ny);
        if (d < bestD) { bestD = d; bx = nx; by = ny; }
    }
    if (bx >= 0) { orderGather(e, bx, by); return true; }
    return false;
}

// Finds the nearest owned TC/Castle/Tower to flee to.
static Entity* findSafeHaven(Entity& e) {
    Entity* best = nullptr; int bestD = 99999;
    for (auto& o : g.entities) {
        if (!o.alive || o.owner != e.owner || o.underConstruction) continue;
        if (o.type!=E_TOWNHALL && o.type!=E_CASTLE && o.type!=E_TOWER) continue;
        int d = dist(e.x, e.y, o.x, o.y);
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}

// ============================================================
// ENTITY TICK
// ============================================================
void tickEntity(Entity& e) {
    if (!e.alive) return;
    if (e.alertTicks > 0) e.alertTicks--;
    // Building production
    if (e.producing != E_NONE && !e.underConstruction) {
        int bonus = 0;
        for (auto& o : g.entities)
            if (o.alive && o.owner==e.owner && o.type==E_BLACKSMITH && !o.underConstruction) { bonus=1; break; }
        e.prodProgress += 1 + bonus;
        if (e.prodProgress >= e.prodTime) {
            auto& bs = STATS[e.type]; bool placed = false;
            bool produceNaval = isNaval(e.producing);
            int newId = -1;
            for (int r = 0; r <= 4 && !placed; r++)
                for (int dy = -r; dy <= bs.sizeH+r && !placed; dy++)
                    for (int dx = -r; dx <= bs.sizeW+r && !placed; dx++) {
                        int nx = e.x+dx, ny = e.y+dy;
                        if (!inBounds(nx,ny) || entityAt(nx,ny)) continue;
                        bool ok = produceNaval ? isPassableWater(nx,ny) : isPassable(nx,ny);
                        if (!ok) continue;
                        newId = spawnEntity(e.producing, e.owner, nx, ny);
                        placed = true;
                    }
            // If no spawn spot was found, keep the unit queued and retry next tick
            // instead of silently consuming it — resources were already spent.
            if (!placed) {
                e.prodProgress = e.prodTime; // stay at completion threshold
            } else {
                // Send to rally point if the building has a player-set one
                if (e.rallySet && newId >= 0) {
                    Entity* nu = findEntity(newId);
                    if (nu) orderMove(*nu, e.rallyX, e.rallyY);
                }
                EntityType justTrained = e.producing;
                e.producing = E_NONE; e.state = S_IDLE;
                if (e.owner==0) setStatus(std::string(STATS[justTrained].name) + " is ready.");
                // Pop the next queued unit straight into production.
                if (!e.queue.empty()) {
                    EntityType next = (EntityType)e.queue.front();
                    e.queue.erase(e.queue.begin());
                    e.producing = next; e.prodProgress = 0;
                    e.prodTime = STATS[next].trainTime; e.state = S_TRAINING;
                }
            }
        }
    }
    // Research progress (Blacksmith). Independent of unit production.
    if (e.researching != 0 && !e.underConstruction) {
        e.prodProgress += 1;
        if (e.prodProgress >= e.prodTime) {
            g.players[e.owner].research |= e.researching;
            int bit = e.researching;
            e.researching = 0; e.prodProgress = 0; e.prodTime = 0;
            if (e.owner == 0) {
                if      (bit == R_IRON_WEAPONS)  setStatus("Iron Weapons researched — militia/knights +2 atk!");
                else if (bit == R_CROSSBOWS)     setStatus("Crossbows researched — archers +2 range!");
                else if (bit == R_PIKES)         setStatus("Pikes researched — spearmen +1 range!");
                else if (bit == R_COUNTERWEIGHT) setStatus("Counterweight researched — trebuchets deploy faster!");
                else if (bit == R_PLATE_HELM)    setStatus("Plate Helm researched — knights take less melee damage!");
            }
        }
    }
    // Construction progress
    if (e.underConstruction) {
        bool hasBuilder = false;
        for (auto& o : g.entities) {
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
                e.hp = e.maxHp; e.underConstruction = false; updateSupply(e.owner);
                if (e.owner==0) setStatus(std::string(STATS[e.type].name) + " complete!");
                for (auto& o : g.entities) {
                    if (!o.alive || o.state!=S_BUILDING || o.targetId!=e.id) continue;
                    // For farms: keep tending — S_BUILDING handler routes to its farm branch.
                    if (e.type == E_FARM) continue;
                    // For other structures: cast around for the nearest in-progress build
                    // (any owner==o.owner site under construction) and continue helping.
                    Entity* next = nullptr; int bestD = 99999;
                    for (auto& b : g.entities) {
                        if (!b.alive || b.owner != o.owner || !b.underConstruction || !isBuilding(b.type)) continue;
                        if (b.id == e.id) continue;
                        int d = mdist(o.x, o.y, b.x, b.y);
                        if (d < bestD) { bestD = d; next = &b; }
                    }
                    if (next) orderHelp(o, next->id);
                    else o.state = S_IDLE;
                }
            }
        }
        return;
    }
    if (!isUnit(e.type)) return;

    // Trebuchet pack/unpack transition: tick the timer; while > 0 do nothing.
    if (e.type == E_TREBUCHET) {
        if (e.packTicks > 0) {
            e.packTicks--;
            if (e.packTicks == 0) {
                if (e.owner == 0)
                    setStatus(e.packed ? "Trebuchet packed." : "Trebuchet deployed.");
            }
            return;
        }
    }

    // Retreat when critically wounded: flee toward nearest TC/Castle/Tower.
    // Trebuchets don't auto-retreat — they pack and crawl, defeating the purpose.
    if (e.owner < OWNER_NATURE && e.type != E_PEASANT && e.type != E_TREBUCHET
            && !e.retreating && e.hp * 100 < e.maxHp * 15) {
        Entity* haven = findSafeHaven(e);
        if (haven) {
            e.retreating = 1; e.state = S_MOVING; e.attackMove = 0;
            e.targetX = haven->x; e.targetY = haven->y; e.targetId = haven->id;
            e.path = findPathFor(e, haven->x, haven->y); e.pathIdx = 0;
        }
    }
    // Healed enough — stop fleeing.
    if (e.retreating && e.hp * 100 >= e.maxHp * 30) e.retreating = 0;

    switch (e.state) {
    case S_IDLE:
        // Retreating unit re-paths toward safety if it ended up idle.
        if (e.retreating) {
            Entity* haven = findSafeHaven(e);
            if (haven && dist(e.x, e.y, haven->x, haven->y) > 2) {
                e.state = S_MOVING;
                e.path = findPathFor(e, haven->x, haven->y); e.pathIdx = 0;
                e.targetX = haven->x; e.targetY = haven->y;
            } else { e.retreating = 0; } // arrived — stand down
            break;
        }
        // Archers kite: back away when an enemy closes to melee range.
        if (e.type == E_ARCHER && e.owner < OWNER_NATURE) {
            Entity* en = findNearestEnemy(e, 1);
            if (en) {
                int dx = e.x - en->x, dy = e.y - en->y;
                int fleeX = std::max(0, std::min(MAP_W-1, e.x + (dx >= 0 ? 1 : -1) * 4));
                int fleeY = std::max(0, std::min(MAP_H-1, e.y + (dy >= 0 ? 1 : -1) * 4));
                if (isPassable(fleeX, fleeY)) {
                    e.state = S_MOVING; e.attackMove = 0;
                    e.path = findPathFor(e, fleeX, fleeY); e.pathIdx = 0;
                    e.targetX = fleeX; e.targetY = fleeY;
                    break;
                }
            }
        }
        // Military auto-engages anything visible within fog radius — units now
        // close in on threats they can see rather than waiting to be poked.
        // Packed trebuchets are travelling, not fighting.
        if (!e.holdPosition && !e.retreating && e.type != E_PEASANT && e.type != E_FISHING_BOAT
            && e.type != E_TRANSPORT && e.type != E_RAM && STATS[e.type].atk > 0
            && !(e.type == E_TREBUCHET && e.packed == 1)
            && e.owner != OWNER_NATURE) {
            // Melee units engage at 5 tiles; ranged at full fog radius — prevents
            // instant magnetic battles where everyone charges across the map.
            int aggroRange = isRanged(e.type) ? std::max(FOG_RADIUS, unitRange(e)+1) : 5;
            Entity* en = findNearestEnemy(e, aggroRange);
            if (en) orderAttack(e, en->id);
        }
        // Boats auto-fish when idle — find a fish shoal, gather, return to dock.
        if (e.type == E_FISHING_BOAT && (g.tick + e.id) % 12 == 0) {
            // If carrying fish but no dock when we landed here, retry now (player may have rebuilt).
            if (e.carrying > 0) {
                Entity* dep = findDepot(e);
                if (dep) {
                    e.state = S_RETURNING; e.targetId = dep->id;
                    e.targetX = dep->x; e.targetY = dep->y;
                    e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                    break;
                }
            }
            findNearbyResource(e);
        }
        break;
    case S_MOVING:
        // Attack-move: engage anything in range while marching toward the destination.
        if (e.attackMove && STATS[e.type].atk > 0) {
            Entity* en = findNearestEnemy(e, unitRange(e)+1);
            if (en) { orderAttack(e, en->id); break; }
        }
        moveAlongPath(e);
        if (e.path.empty() || e.pathIdx >= (int)e.path.size()) {
            e.state = S_IDLE; e.attackMove = 0;
        }
        break;
    case S_ATTACKING: {
        Entity* t = findEntity(e.targetId);
        if (!t || !t->alive) { e.state = S_IDLE; break; }
        // Target ducked into a building — they're untouchable, go idle. Without
        // this the attacker keeps swinging at the garrisoned position and the
        // target dies inside the safe building.
        if (t->state == S_GARRISONED) { e.state = S_IDLE; e.targetId = -1; break; }
        int d = dist(e.x, e.y, t->x, t->y);
        // Catapults need standoff — too close to arm the sling properly.
        if (e.type == E_CATAPULT && d < 2) { e.state = S_IDLE; break; }
        if (d <= unitRange(e)) {
            if (e.atkCd <= 0) {
                int rawDmg = unitAtk(e);
                int dmg = damageVs(e.type, t->type, rawDmg, t->owner);
                t->hp -= dmg;
                e.atkCd = STATS[e.type].atkSpeed;
                e.alertTicks = 12; t->alertTicks = 12;
                if (t->owner == 0 && g.attackNotifyCd == 0 && t->type != E_NONE) {
                    setStatus("Your people are under attack!");
                    g.attackNotifyCd = 200;
                }
                if (isRanged(e.type)) {
                    char pc = (e.type==E_CATAPULT) ? 'o' : '-';
                    int pcol = (e.type==E_CATAPULT) ? CP_PROJ_BOULDER : CP_PROJ_ARROW;
                    spawnProjectile(e.x, e.y, t->x, t->y, pc, pcol);
                }
                // Catapult splash: 1-tile radius around impact centre, ~1/3 of the
                // raw damage to anyone but the prime target (per-victim building
                // modifier still applies). Friendly fire is on.
                if (e.type == E_CATAPULT) {
                    auto& ts = STATS[t->type];
                    int tcx = t->x + ts.sizeW/2, tcy = t->y + ts.sizeH/2;
                    int primeId = t->id, splashRaw = rawDmg / 3;
                    for (auto& o : g.entities) {
                        if (!o.alive || o.id == primeId) continue;
                        auto& os = STATS[o.type];
                        int ox = std::max(o.x, std::min(tcx, o.x + os.sizeW - 1));
                        int oy = std::max(o.y, std::min(tcy, o.y + os.sizeH - 1));
                        if (std::abs(ox - tcx) <= 1 && std::abs(oy - tcy) <= 1) {
                            int splashDmg = damageVs(E_CATAPULT, o.type, splashRaw, o.owner);
                            o.hp -= splashDmg; o.alertTicks = 12;
                            if (o.hp <= 0) killEntity(o);
                        }
                    }
                }
                if (t->hp <= 0) {
                    bool hunted = (t->owner == OWNER_NATURE && e.owner < OWNER_NATURE);
                    if (hunted && e.type == E_PEASANT && e.carrying == 0) {
                        // Peasant hunter hauls the carcass back to a Mill/TC/Castle.
                        int food = (t->type==E_SHEEP)?80:(t->type==E_DEER)?120:(t->type==E_BOAR)?100:30;
                        e.carrying = food; e.gatherType = 3;
                        e.rallyX = -1; e.rallyY = -1; // sentinel: don't auto-resume after dropoff
                        Entity* dep = findDepot(e);
                        if (dep) {
                            e.state = S_RETURNING;
                            e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
                            e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                            if (e.owner==0) setStatus(std::string("Hunted! Hauling ") + std::to_string(food) + " food.");
                        } else {
                            e.carrying = 0; e.state = S_IDLE;
                            if (e.owner==0) setStatus("Killed game but no Mill/TC nearby — meat wasted.");
                        }
                    } else {
                        // Non-peasant kills (e.g. militia defending base) waste the carcass.
                        e.state = S_IDLE;
                    }
                    killEntity(*t);
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
                e.path = findPathFor(e, t->x, t->y); e.pathIdx = 0;
                // Target truly unreachable — drop back to idle so we don't spin
                // re-pathing every tick. Auto-aggro will pick another fight.
                if (e.path.empty()) { e.state = S_IDLE; e.targetId = -1; break; }
            }
            moveAlongPath(e);
        }
        break;
    }
    case S_GATHERING: {
        int d = dist(e.x, e.y, e.targetX, e.targetY);
        if (d <= 1) {
            Tile& tile = g.map[e.targetY][e.targetX];
            bool isW     = (tile.terrain==T_FOREST||tile.terrain==T_PINE||tile.terrain==T_PALM||tile.terrain==T_DEAD_TREE);
            bool isBerry = (tile.terrain == T_BERRY);
            bool isFishT = (tile.terrain == T_FISH);
            if ((tile.terrain==T_GOLD||isW||isBerry||isFishT) && tile.resources > 0) {
                e.gatherCd++;
                if (e.gatherCd >= GATHER_TICKS) {
                    e.gatherCd = 0;
                    int amt = std::min(GATHER_RATE, tile.resources);
                    tile.resources -= amt; e.carrying += amt;
                    if (tile.resources <= 0)
                        tile.terrain = isFishT ? T_WATER : isBerry ? T_GRASS : T_DIRT;
                    if (e.carrying >= CARRY_MAX || tile.resources <= 0) {
                        Entity* dep = findDepot(e);
                        if (dep) {
                            e.state = S_RETURNING; e.targetId = dep->id;
                            e.targetX = dep->x; e.targetY = dep->y;
                            e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                        } else e.state = S_IDLE;
                    }
                }
            } else {
                // Tile depleted (beaten to it) — seek another nearby node
                if (!findNearbyResource(e)) e.state = S_IDLE;
            }
        } else {
            moveAlongPath(e);
            if (e.path.empty() && dist(e.x,e.y,e.targetX,e.targetY) > 1) {
                e.path = findPathFor(e, e.targetX, e.targetY); e.pathIdx = 0;
                if (e.path.empty()) e.state = S_IDLE;
            }
        }
        break;
    }
    case S_RETURNING: {
        Entity* dep = findEntity(e.targetId);
        if (!dep || !dep->alive) {
            dep = findDepot(e);
            if (!dep) { e.state = S_IDLE; break; }
            e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
            e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
        }
        int d = dist(e.x, e.y, dep->x, dep->y);
        if (d <= STATS[dep->type].sizeW + 1) {
            if (e.gatherType == 0)      g.players[e.owner].gold += e.carrying;
            else if (e.gatherType == 1) g.players[e.owner].wood += e.carrying;
            else if (e.gatherType == 3) addPlayerFood(e.owner, e.carrying, dep); // farm/berry/hunt
            else                        g.players[e.owner].food += e.carrying;   // fish (dock-tracked)
            e.carrying = 0;
            // Farm courier: rallyX/Y stores the farm we came from — go back and resume tending.
            if (e.gatherType == 3 && inBounds(e.rallyX, e.rallyY)) {
                Entity* home = entityAt(e.rallyX, e.rallyY);
                if (home && home->alive && home->type == E_FARM
                    && home->owner == e.owner && !home->underConstruction) {
                    e.state = S_BUILDING; e.targetId = home->id;
                    e.targetX = home->x; e.targetY = home->y;
                    e.path = findPathFor(e, home->x, home->y); e.pathIdx = 0;
                    break;
                }
            }
            // Hunters use rallyX = -1 sentinel — no auto-resume after dropoff.
            if (!inBounds(e.rallyX, e.rallyY)) { e.state = S_IDLE; break; }
            Tile& rt = g.map[e.rallyY][e.rallyX];
            bool isW = (rt.terrain==T_FOREST||rt.terrain==T_PINE||rt.terrain==T_PALM||rt.terrain==T_DEAD_TREE);
            bool isBerry = (rt.terrain == T_BERRY);
            bool isFishT = (rt.terrain == T_FISH);
            if ((rt.terrain==T_GOLD||isW||isBerry||isFishT) && rt.resources > 0) {
                e.state = S_GATHERING; e.targetX = e.rallyX; e.targetY = e.rallyY;
                e.path = findPathFor(e, e.rallyX, e.rallyY); e.pathIdx = 0;
            } else {
                // Rally point depleted — seek another nearby node of the same type
                if (!findNearbyResource(e)) e.state = S_IDLE;
            }
        } else {
            moveAlongPath(e);
            if (e.path.empty() && dist(e.x,e.y,dep->x,dep->y) > STATS[dep->type].sizeW+1) {
                e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
            }
        }
        break;
    }
    case S_ENTERING: {
        Entity* bld = findEntity(e.targetId);
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
                if (e.owner == 0) setStatus(std::string("Garrisoned in ") + STATS[bld->type].name);
            } else {
                if (e.owner == 0) setStatus(std::string(STATS[bld->type].name) + " is full");
                e.state = S_IDLE;
            }
        } else {
            moveAlongPath(e);
            if (e.path.empty()) {
                int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
                for (int dy = -1; dy <= bh; dy++) for (int dx = -1; dx <= bw; dx++) {
                    if (dx>=0 && dx<bw && dy>=0 && dy<bh) continue;
                    int nx = bld->x+dx, ny = bld->y+dy;
                    if (inBounds(nx,ny) && isPassable(nx,ny)) {
                        int d2 = mdist(e.x, e.y, nx, ny);
                        if (d2 < bestAD) { bestAD = d2; bestAX = nx; bestAY = ny; }
                    }
                }
                e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
                if (e.path.empty()) e.state = S_IDLE;
            }
        }
        break;
    }
    case S_GARRISONED:
        break; // Stays inside; building handles its own attacks.
    case S_BUILDING: {
        Entity* bld = findEntity(e.targetId);
        if (!bld || !bld->alive) { e.state = S_IDLE; break; }
        // Tending a completed farm — stay adjacent and ferry ripe harvest to a depot
        if (!bld->underConstruction && bld->type == E_FARM) {
            int d = dist(e.x, e.y, bld->x, bld->y);
            // Pick up as soon as there is anything worth carrying.
            if (d <= 1 && bld->carrying >= 3 && e.carrying == 0) {
                int take = std::min(bld->carrying, CARRY_MAX);
                e.carrying = take; bld->carrying -= take;
                e.gatherType = 3; // food — depots: TC, Castle, Mill
                e.rallyX = bld->x; e.rallyY = bld->y; // return-to point: this farm
                Entity* dep = findDepot(e);
                if (dep) {
                    e.state = S_RETURNING;
                    e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
                    e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                } else {
                    // No depot available — drop the harvest back on the farm and keep tending
                    bld->carrying += take; e.carrying = 0;
                }
                break;
            }
            if (d > 1) {
                moveAlongPath(e);
                if (e.path.empty()) { e.path = findPathFor(e, bld->x, bld->y); e.pathIdx = 0; }
            }
            break;
        }
        if (!bld->underConstruction) { e.state = S_IDLE; break; }
        int bx2 = bld->x + STATS[bld->type].sizeW - 1, by2 = bld->y + STATS[bld->type].sizeH - 1;
        int cx = std::max(bld->x, std::min(e.x, bx2));
        int cy = std::max(bld->y, std::min(e.y, by2));
        if (dist(e.x, e.y, cx, cy) > 1) {
            moveAlongPath(e);
            if (e.path.empty()) {
                // Re-scan for the nearest free adjacent tile
                int bestAX = bld->x-1, bestAY = bld->y, bestAD = 99999;
                int bw = STATS[bld->type].sizeW, bh = STATS[bld->type].sizeH;
                for (int dy = -1; dy <= bh; dy++) for (int dx = -1; dx <= bw; dx++) {
                    if (dx>=0&&dx<bw&&dy>=0&&dy<bh) continue;
                    int nx = bld->x+dx, ny = bld->y+dy;
                    if (inBounds(nx,ny) && isPassable(nx,ny)) {
                        int d2 = mdist(e.x,e.y,nx,ny);
                        if (d2 < bestAD) { bestAD=d2; bestAX=nx; bestAY=ny; }
                    }
                }
                e.path = findPathFor(e, bestAX, bestAY); e.pathIdx = 0;
            }
        }
        break;
    }
    default: break;
    }
}

