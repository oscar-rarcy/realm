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
        && t != T_FISH;
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
    if (!isConcealing()) return true;
    if (observerOwner < 0 || observerOwner >= MAX_PLAYERS) return true;
    if (!inBounds(x, y)) return false;
    ensureDetectMap(observerOwner);
    return detectMap[observerOwner][y][x];
}

void setStatus(const std::string& msg) { g.statusMsg = msg; g.statusTimer = 35; }

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
        if (isBase || isWood || isGold || isFish) {
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
        if (t!=T_GRASS&&t!=T_MEADOW&&t!=T_TALL_GRASS&&t!=T_FLOWERS&&t!=T_DIRT&&t!=T_WHEAT) return false;
    }
    auto& s = STATS[type];
    for (int dy = 0; dy < s.sizeH; dy++) for (int dx = 0; dx < s.sizeW; dx++) {
        int nx = x+dx, ny = y+dy;
        if (!inBounds(nx,ny) || !isPassable(nx,ny)) return false;
        Terrain ter = g.map[ny][nx].terrain;
        if (ter == T_GOLD) return false;
        // Forests are resource terrain — chop the trees before you can build here.
        if (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) return false;
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
// ============================================================
static inline std::vector<std::pair<int,int>> findPathFor(Entity& e, int tx, int ty) {
    return findPath(e.x, e.y, tx, ty, 300, isNaval(e.type));
}

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
            if (e.type == E_GATE && e.carrying > 0) continue;
            auto& s = STATS[e.type];
            for (int dy2 = 0; dy2 < s.sizeH; dy2++) for (int dx2 = 0; dx2 < s.sizeW; dx2++) {
                int bx = e.x+dx2, by = e.y+dy2;
                if (inBounds(bx,by)) bldMap[by][bx] = true;
            }
        }
    }
    bldMap[ty][tx] = false; // always allow reaching the destination

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
    else if (g.weather == W_RAIN) nightPen += (nightPen > 0 ? 0 : 1); // mild rain dimming by day
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
// COMBAT / ORDERS
// ============================================================
// Research-aware combat stats.
static int unitAtk(const Entity& e) {
    int a = STATS[e.type].atk;
    int r = g.players[e.owner].research;
    if ((e.type == E_MILITIA || e.type == E_KNIGHT) && (r & R_IRON_WEAPONS)) a += 2;
    return a;
}
static int unitRange(const Entity& e) {
    int rng = STATS[e.type].range;
    int r = g.players[e.owner].research;
    if (e.type == E_ARCHER && (r & R_CROSSBOWS)) rng += 2;
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
        // Cloaking: if it's night/storm and no friendly eye is near the target, can't engage.
        if (concealing && o.owner != OWNER_NATURE && e.owner < MAX_PLAYERS
            && !isDetectedBy(o.x, o.y, e.owner)) continue;
        int d = dist(e.x, e.y, o.x, o.y);
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}

void orderMove(Entity& e, int tx, int ty) {
    e.state = S_MOVING; e.targetX = tx; e.targetY = ty; e.targetId = -1;
    e.stuckTicks = 0;
    e.attackMove = 0; e.holdPosition = 0;
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
    e.holdPosition = 0;
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
    if (!canPlace(bt, bx, by, e.owner)) {
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
}

void orderTrain(Entity& bld, EntityType ut) {
    if (!isBuilding(bld.type) || bld.underConstruction) return;
    // Queue if busy; reject only when queue is full.
    if (bld.producing != E_NONE && (int)bld.queue.size() >= 5) {
        if (bld.owner==0) setStatus("Queue full!"); return;
    }
    Player& p = g.players[bld.owner];
    if (p.gold < STATS[ut].costGold || p.wood < STATS[ut].costWood) {
        if (bld.owner==0) setStatus("Not enough resources!"); return;
    }
    if (reservedSupply(bld.owner) + STATS[ut].supplyUsed > p.supplyMax) {
        if (bld.owner==0) setStatus("Need more houses!"); return;
    }
    int foodCost = 0;
    if (ut==E_MILITIA||ut==E_ARCHER) foodCost = 20;
    else if (ut==E_KNIGHT) foodCost = 40;
    else if (ut==E_CATAPULT) foodCost = 30;
    else if (ut==E_WARSHIP)  foodCost = 20;
    else if (ut==E_TRANSPORT) foodCost = 10;
    if (p.food < foodCost) { if (bld.owner==0) setStatus("Need more food!"); return; }
    p.food -= foodCost;
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
        case E_KNIGHT:   return 0;
        case E_MILITIA:  return 1;
        case E_PEASANT:  return 2;
        case E_ARCHER:   return 3;
        case E_CATAPULT: return 4;
        default:         return 5;
    }
}

// Group move with role-aware formation:
// melee occupy the rows facing the target, ranged hang back.
// If attackMove is true, all units engage opportunistically en route.
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

// ============================================================
// GARRISON
// ============================================================
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
}

// Centralized death handler: marks dead, ejects garrison, ruins terrain, updates supply.
static void killEntity(Entity& t) {
    if (!t.alive) return;
    t.alive = false; t.state = S_DEAD;
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
    if (bld->owner != e.owner) return;
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
        bool isOpenGate = (blk->type == E_GATE && blk->carrying > 0);
        if (!isOpenGate) {
            // Tolerate transient blocks; only repath after several stuck ticks (staggered by id).
            e.stuckTicks++;
            int threshold = 3 + (e.id % 5);
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
    else if (ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW||ter==T_ICE) spd += 1;
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
                e.producing = E_NONE; e.state = S_IDLE;
                if (e.owner==0) { setStatus("Training complete!"); beep(); }
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
                if (bit == R_IRON_WEAPONS) setStatus("Iron Weapons researched — militia/knights +2 atk!");
                else if (bit == R_CROSSBOWS) setStatus("Crossbows researched — archers +2 range!");
                beep();
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
                if (e.owner==0) { setStatus(std::string(STATS[e.type].name) + " complete!"); beep(); }
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

    switch (e.state) {
    case S_IDLE:
        if (!e.holdPosition && e.type != E_PEASANT && e.type != E_FISHING_BOAT && STATS[e.type].atk > 0) {
            Entity* en = findNearestEnemy(e, unitRange(e)+1);
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
        int d = dist(e.x, e.y, t->x, t->y);
        if (d <= unitRange(e)) {
            if (e.atkCd <= 0) {
                int dmg = unitAtk(e);
                t->hp -= dmg;
                e.atkCd = STATS[e.type].atkSpeed;
                e.alertTicks = 12; t->alertTicks = 12;
                if (isRanged(e.type)) {
                    char pc = (e.type==E_CATAPULT) ? 'o' : '-';
                    int pcol = (e.type==E_CATAPULT) ? CP_PROJ_BOULDER : CP_PROJ_ARROW;
                    spawnProjectile(e.x, e.y, t->x, t->y, pc, pcol);
                }
                // Catapult splash: 1-tile radius around impact centre, half damage to
                // anyone but the prime target. Friendly fire is on — siege is messy.
                if (e.type == E_CATAPULT) {
                    auto& ts = STATS[t->type];
                    int tcx = t->x + ts.sizeW/2, tcy = t->y + ts.sizeH/2;
                    int primeId = t->id, splash = dmg / 2;
                    for (auto& o : g.entities) {
                        if (!o.alive || o.id == primeId) continue;
                        auto& os = STATS[o.type];
                        // Use footprint-overlap-ish check: nearest point of o's footprint to impact centre.
                        int ox = std::max(o.x, std::min(tcx, o.x + os.sizeW - 1));
                        int oy = std::max(o.y, std::min(tcy, o.y + os.sizeH - 1));
                        if (std::abs(ox - tcx) <= 1 && std::abs(oy - tcy) <= 1) {
                            o.hp -= splash; o.alertTicks = 12;
                            if (o.hp <= 0) killEntity(o);
                        }
                    }
                }
                if (t->hp <= 0) {
                    if (t->owner == OWNER_NATURE && e.owner < OWNER_NATURE) {
                        int food = (t->type==E_SHEEP)?80:(t->type==E_DEER)?120:30;
                        g.players[e.owner].food += food;
                        if (e.owner==0) setStatus(std::string("Got ") + std::to_string(food) + " food!");
                    }
                    killEntity(*t);
                    e.state = S_IDLE;
                }
            } else e.atkCd--;
        } else {
            // Re-path if our path is exhausted OR target wandered away from its end.
            bool stale = e.path.empty() || e.pathIdx >= (int)e.path.size();
            if (!stale) {
                auto end = e.path.back();
                if (dist(end.first, end.second, t->x, t->y) > 2) stale = true;
            }
            if (stale) { e.path = findPathFor(e, t->x, t->y); e.pathIdx = 0; }
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
            else                        g.players[e.owner].food += e.carrying;
            e.carrying = 0;
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
        // Tending a completed farm — stay adjacent
        if (!bld->underConstruction && bld->type == E_FARM) {
            if (dist(e.x, e.y, bld->x, bld->y) > 1) {
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

// ============================================================
// PASSIVE BUILDING TICKS
// ============================================================
void tickTowers() {
    // Towers always defend. Town Hall/Castle/House defend only when garrisoned.
    // Garrisoned archers add ranged punch; militia/knights add a smaller bonus.
    for (auto& e : g.entities) {
        if (!e.alive || e.underConstruction) continue;
        if (!isBuilding(e.type)) continue;
        bool isTower    = (e.type == E_TOWER);
        bool canGarrAtk = canGarrisonIn(e.type) && !e.garrison.empty();
        if (!isTower && !canGarrAtk) continue;

        int archers = 0, fighters = 0;
        for (int uid : e.garrison) {
            Entity* u = findEntity(uid);
            if (!u || !u->alive) continue;
            if (u->type == E_ARCHER) archers++;
            else if (u->type == E_MILITIA || u->type == E_KNIGHT) fighters++;
        }
        int atk = STATS[e.type].atk + archers*5 + fighters*2;
        int rng = STATS[e.type].range;
        if (e.type == E_TOWNHALL && canGarrAtk) rng = std::max(rng, 6);
        if (e.type == E_CASTLE   && canGarrAtk) rng = std::max(rng, 8);
        if (e.type == E_HOUSE    && canGarrAtk) rng = std::max(rng, 4);
        if (rng <= 0 || atk <= 0) { if (e.atkCd > 0) e.atkCd--; continue; }

        int sx = e.x + STATS[e.type].sizeW/2;
        int sy = e.y + STATS[e.type].sizeH/2;
        // Build a temporary anchor entity for range checks (use e directly — its x,y is top-left, close enough)
        Entity* en = findNearestEnemy(e, rng);
        if (en) {
            if (e.atkCd <= 0) {
                en->hp -= atk; e.atkCd = isTower ? STATS[E_TOWER].atkSpeed : 9;
                en->alertTicks = 12;
                spawnProjectile(sx, sy, en->x, en->y, '*', CP_PROJ_TOWER);
                if (en->hp <= 0) killEntity(*en);
            } else e.atkCd--;
        } else if (e.atkCd > 0) e.atkCd--;
    }
}

void tickGates() {
    for (auto& gate : g.entities) {
        if (!gate.alive || gate.type != E_GATE || gate.underConstruction) continue;
        if (gate.gatherType == 1) continue; // manually locked — don't auto-toggle
        bool allyNear = false;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != gate.owner || isBuilding(u.type)) continue;
            if (dist(u.x, u.y, gate.x, gate.y) <= 2) { allyNear = true; break; }
        }
        gate.carrying = allyNear ? 1 : 0;
    }
}

void tickFarms() {
    g.farmTimer++;
    if (g.farmTimer < 40) return;
    g.farmTimer = 0;

    // Wheat dies at the onset of winter
    if (getSeason() == WINTER) {
        for (auto& e : g.entities)
            if (e.alive && e.type == E_FARM) { e.alive = false; e.state = S_DEAD; }
        return;
    }

    int bonus = (getSeason() == SUMMER) ? 1 : 0;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        bool hasMill = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && e.type==E_MILL && !e.underConstruction) { hasMill=true; break; }
        if (!hasMill) continue;

        for (auto& farm : g.entities) {
            if (!farm.alive || farm.type!=E_FARM || farm.owner!=p || farm.underConstruction) continue;
            // Any adjacent peasant (explicitly tending or just idle nearby) counts
            bool tended = false;
            for (auto& u : g.entities) {
                if (!u.alive || u.owner!=p || u.type!=E_PEASANT) continue;
                if (dist(u.x, u.y, farm.x, farm.y) <= 1) { tended=true; break; }
            }
            if (tended) g.players[p].food += 3 + bonus;
        }
    }
}

void tickMarkets() {
    if (g.tick % 50 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        int m = 0;
        for (auto& e : g.entities) if (e.alive && e.owner==p && e.type==E_MARKET && !e.underConstruction) m++;
        g.players[p].gold += m * 5;
    }
}

void tickChurches() {
    if (g.tick % 20 != 0) return;
    for (auto& e : g.entities) {
        if (!e.alive || e.type!=E_CHURCH || e.underConstruction) continue;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner!=e.owner || !isUnit(u.type)) continue;
            if (dist(u.x,u.y,e.x,e.y) <= 3 && u.hp < u.maxHp)
                u.hp = std::min(u.maxHp, u.hp + 1);
        }
    }
}

// ============================================================
// SEASONS — winter transformation, spring thaw, hunger
// ============================================================
static void applyWinter() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        t.preWinterTerrain = t.terrain;
        switch (t.terrain) {
            case T_GRASS: case T_TALL_GRASS: case T_FLOWERS: case T_MEADOW:
            case T_DIRT:  case T_ROAD:       case T_GRAVEL:  case T_RUINS:
            case T_SAND:  case T_DUNES:      case T_WHEAT:   case T_BERRY:
            case T_MUD:   case T_CASTLE_FLOOR:
                t.terrain = T_SNOW; break;
            case T_WATER: case T_SHALLOWS: case T_MARSH: case T_REEDS:
                t.terrain = T_ICE; break;
            default: break; // forests, hills, mountains, gold, walls, stone keep their look
        }
    }
    // Cull a chunk of wildlife — the herd is thinned by the cold.
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;
        if (e.type != E_DEER && e.type != E_SHEEP) continue;
        if (rand() % 100 < 35) killEntity(e);
    }
    if (g.players[0].alive) setStatus("Winter falls. The land freezes over.");
}

void tickSeasons() {
    int s = (int)getSeason();
    if (s != g.prevSeason) {
        if (s == WINTER) applyWinter();
        if (s == SPRING && g.prevSeason == WINTER && g.players[0].alive)
            setStatus("Spring stirs. The thaw begins.");
        g.prevSeason = s;
    }
}

void tickThaw() {
    if (g.tick % 5 != 0) return;
    if (getSeason() != SPRING) return;
    float progress = getSeasonProgress();
    // Patchy melt completes by ~40% of spring — earlier sessions felt snowy way too
    // deep into the season. Tiles thaw faster, world greens up quickly.
    int threshold = std::max(0, (int)(progress * 2600.0f));
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.terrain != T_SNOW && t.terrain != T_ICE) continue;
        if (t.preWinterTerrain == t.terrain) continue;
        unsigned h = ((unsigned)x * 73856093u) ^ ((unsigned)y * 19349663u);
        if ((int)(h & 0x3ff) < threshold) t.terrain = t.preWinterTerrain;
    }
}

void tickWinter() {
    if (getSeason() != WINTER) return;
    if (g.tick % 100 != 0) return;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!g.players[p].alive) continue;
        int unitCount = 0;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
            unitCount++;
        }
        if (unitCount == 0) continue;
        Player& pl = g.players[p];
        int drain = unitCount; // 1 food per unit per 100 ticks
        if (pl.food >= drain) {
            pl.food -= drain;
        } else {
            int starve = drain - pl.food;
            pl.food = 0;
            // Damage `starve` random units. If any die, they're gone.
            int hits = 0;
            for (auto& e : g.entities) {
                if (!e.alive || e.owner != p || !isUnit(e.type)) continue;
                e.hp -= 3;
                if (e.hp <= 0) killEntity(e);
                if (++hits >= starve) break;
            }
            if (p == 0) setStatus("Starvation! Units are losing health.");
        }
    }
}

// ============================================================
// PAVING — building creep + path wear + decay
// ============================================================
void tickPaving() {
    // Buildings emit creep into adjacent natural ground.
    if (g.tick % 100 == 0) {
        for (auto& e : g.entities) {
            if (!e.alive || !isBuilding(e.type) || e.underConstruction) continue;
            auto& s = STATS[e.type];
            for (int dy = -3; dy <= s.sizeH+2; dy++) for (int dx = -3; dx <= s.sizeW+2; dx++) {
                if (dx >= 0 && dx < s.sizeW && dy >= 0 && dy < s.sizeH) continue;
                int nx = e.x+dx, ny = e.y+dy;
                if (!inBounds(nx,ny)) continue;
                int ringDist = std::max(std::max(0, -dx), std::max(0, dx-s.sizeW+1))
                             + std::max(std::max(0, -dy), std::max(0, dy-s.sizeH+1));
                if (ringDist > 3) continue;
                Tile& t = g.map[ny][nx];
                Terrain ter = t.terrain;
                if (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW
                 || ter==T_SAND ||ter==T_DUNES) {
                    int gain = (ringDist <= 1) ? 5 : (ringDist == 2) ? 3 : 1;
                    if (t.wear < 80) t.wear += gain;
                    // Lower threshold so visible haloes appear within ~50 seconds.
                    if (t.wear >= 30 && (ter==T_GRASS||ter==T_TALL_GRASS||ter==T_FLOWERS||ter==T_MEADOW)) {
                        t.terrain = T_DIRT; t.preWinterTerrain = T_DIRT;
                    }
                }
            }
        }
    }
    // Decay: unused paving gradually returns to nature.
    if (g.tick % 250 == 0) {
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Tile& t = g.map[y][x];
            if (t.wear > 0) t.wear--;
            if (t.wear == 0 && t.terrain == T_ROAD) {
                t.terrain = T_DIRT; t.preWinterTerrain = T_DIRT;
            }
            // Dirt slowly regrows — patches of grass return after long disuse
            if (t.wear == 0 && t.terrain == T_DIRT && (rand() % 500) == 0) {
                t.terrain = T_GRASS; t.preWinterTerrain = T_GRASS;
            }
        }
    }
}

// ============================================================
// WEATHER
// ============================================================
void tickWeather() {
    // Mud creation/drying happens on its own cadence regardless of state changes.
    if (g.tick % 50 == 0) {
        if (g.weather == W_RAIN || g.weather == W_STORM) {
            int hits = (g.weather == W_STORM) ? 60 : 30;
            for (int i = 0; i < hits; i++) {
                int x = rand() % MAP_W, y = rand() % MAP_H;
                Tile& t = g.map[y][x];
                if (t.terrain == T_GRASS || t.terrain == T_MEADOW
                 || t.terrain == T_DIRT  || t.terrain == T_TALL_GRASS) {
                    t.terrain = T_MUD;
                }
            }
        } else {
            // Drying — mud reverts to dirt over time once skies clear.
            for (int i = 0; i < 40; i++) {
                int x = rand() % MAP_W, y = rand() % MAP_H;
                Tile& t = g.map[y][x];
                if (t.terrain == T_MUD) t.terrain = T_DIRT;
            }
        }
    }
    if (g.weatherTimer > 0) { g.weatherTimer--; return; }
    // Roll for transition. Season biases the result.
    int roll = rand() % 100;
    Season s = getSeason();
    int rainBias = (s == AUTUMN) ? 50 : (s == SPRING) ? 35 : (s == WINTER) ? 20 : 25;
    int stormBias = (s == AUTUMN) ? 15 : 8;
    if (g.weather == W_CLEAR) {
        if (roll < stormBias)              g.weather = W_STORM;
        else if (roll < rainBias)          g.weather = W_RAIN;
        g.weatherTimer = 400 + rand() % 800; // ~30s-100s
    } else {
        // Rain/storm linger then break.
        if (roll < 60) g.weather = W_CLEAR;
        else if (g.weather == W_RAIN && roll < 75) g.weather = W_STORM;
        else if (g.weather == W_STORM && roll < 80) g.weather = W_RAIN;
        g.weatherTimer = 300 + rand() % 600;
    }
    if (g.players[0].alive) {
        if (g.weather == W_RAIN)  setStatus("Rain begins.");
        else if (g.weather == W_STORM) setStatus("A storm rolls in!");
        else                       setStatus("The skies clear.");
    }
}

// ============================================================
// ANIMALS
// ============================================================
void tickAnimals() {
    static int atick = 0; atick++;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != OWNER_NATURE) continue;

        // Deer and sheep flee from nearby player units
        if (e.type == E_DEER || e.type == E_SHEEP) {
            if (e.state != S_MOVING || e.path.empty()) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 4) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(fx, fy)) orderMove(e, fx, fy);
                        break;
                    }
                }
            }
        }

        // Wolves: usually flee settlements and hunt only isolated units.
        // Winter strips their caution: they ignore settlements and hunt at longer range.
        if (e.type == E_WOLF) {
            bool winter = (getSeason() == WINTER);
            int huntRange = winter ? 6 : 3;
            bool nearSettlement = false;
            int fleeX = -1, fleeY = -1;
            if (!winter) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner == OWNER_NATURE || !isBuilding(o.type)) continue;
                    int d = dist(e.x, e.y, o.x, o.y);
                    if (d <= 15) {
                        nearSettlement = true;
                        fleeX = std::max(1, std::min(e.x + (e.x - o.x)*4, MAP_W-2));
                        fleeY = std::max(1, std::min(e.y + (e.y - o.y)*4, MAP_H-2));
                        break;
                    }
                }
            }
            if (nearSettlement) {
                if (e.state == S_ATTACKING) { e.state = S_IDLE; e.path.clear(); }
                if (e.state == S_IDLE && fleeX >= 0 && isPassable(fleeX, fleeY))
                    orderMove(e, fleeX, fleeY);
            } else if (e.state==S_IDLE || (e.state==S_MOVING && e.path.empty())) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= huntRange) { orderAttack(e, o.id); break; }
                }
            }
        }

        // Random wander when idle
        if (e.state == S_IDLE && atick % (35 + (e.id%25)) == 0) {
            int wx = e.x + (rand()%9)-4, wy = e.y + (rand()%9)-4;
            wx = std::max(1, std::min(wx, MAP_W-2));
            wy = std::max(1, std::min(wy, MAP_H-2));
            if (isPassable(wx, wy)) orderMove(e, wx, wy);
        }
    }
}

// ============================================================
// WIN CONDITION
// ============================================================
void checkWin() {
    int aliveCount = 0; int lastAlive = -1;
    for (int p = 0; p < MAX_PLAYERS; p++) {
        if (!g.players[p].alive) continue;
        bool hasBase = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && (e.type==E_TOWNHALL||e.type==E_CASTLE)) { hasBase=true; break; }
        if (!hasBase) g.players[p].alive = false;
        else { aliveCount++; lastAlive = p; }
    }
    if (aliveCount <= 1) { g.winner = lastAlive; g.mode = M_GAME_OVER; }
}

// ============================================================
// AI
// ============================================================
int     aiCount(int o, EntityType t)    { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)c++; return c; }
int     aiCountAll(int o, EntityType t) { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t)c++;                    return c; }
Entity* aiIdle(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&e.state==S_IDLE&&!e.underConstruction)return &e; return nullptr; }

// Worker selection for AI construction. Prefers idle peasants; if none exist,
// pulls one off gathering/returning so the AI doesn't deadlock at 10/10
// population because aiGather() consumed every idle worker before the build
// pass ran.
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
Entity* aiBldg(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)return &e; return nullptr; }

void aiGather(int o) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner!=o || e.type!=E_PEASANT || e.state!=S_IDLE) continue;
        int bestD = 9999, bx = -1, by = -1;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Terrain t = g.map[y][x].terrain;
            bool isR = (t==T_GOLD||t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE||t==T_BERRY);
            if (isR && g.map[y][x].resources > 0) {
                int d = mdist(e.x, e.y, x, y);
                if (d < bestD) { bestD=d; bx=x; by=y; }
            }
        }
        if (bx >= 0) orderGather(e, bx, by);
    }
}

// Build placement: try near a given centre (random TH-relative if cx<0). Wider, more attempts.
static void aiBuildSpotNear(int o, EntityType bt, int cx, int cy, int& ox, int& oy) {
    if (cx < 0 || cy < 0) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (!th) th = aiBldg(o, E_CASTLE);
        if (!th) return;
        cx = th->x; cy = th->y;
    }
    for (int r = 2; r < 18; r++) for (int a = 0; a < 24; a++) {
        int bx = cx + (rand()%(r*2+1)) - r, by = cy + (rand()%(r*2+1)) - r;
        if (canPlace(bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}
void aiBuildSpot(int o, EntityType bt, int& ox, int& oy) { aiBuildSpotNear(o, bt, -1, -1, ox, oy); }

// Scan player state — used to scale production and pick targets.
struct AIIntel { int playerArmy; int playerCastles; int playerWalls; int playerPeasants; Entity* playerTH; };
static AIIntel aiScout(int o) {
    AIIntel x{0,0,0,0,nullptr};
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (e.type == E_PEASANT) x.playerPeasants++;
        else if (isUnit(e.type)) x.playerArmy++;
        else if (e.type == E_CASTLE)  { x.playerCastles++; if (!x.playerTH) x.playerTH = &e; }
        else if (e.type == E_WALL)    x.playerWalls++;
        else if (e.type == E_TOWNHALL && !x.playerTH) x.playerTH = &e;
    }
    return x;
}

// Pick a target worth attacking from `attacker`'s position.
// Priorities: peasants (raid), wounded enemies, towers, key buildings.
static int aiPickTarget(int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner == o || e.owner == OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        int score = 0;
        if      (e.type == E_PEASANT)                                score += 220;
        else if (e.type == E_ARCHER || e.type == E_CATAPULT)         score += 180;
        else if (isUnit(e.type))                                     score += 110;
        else if (e.type == E_TOWER)                                  score += 140;
        else if (e.type == E_BARRACKS || e.type == E_STABLE)         score += 90;
        else if (e.type == E_FARM || e.type == E_MILL)               score += 70;
        else if (e.type == E_TOWNHALL || e.type == E_CASTLE)         score += 50;
        else if (isBuilding(e.type))                                 score += 25;
        int missing = e.maxHp - e.hp;
        score += missing / 3;            // finish wounded targets
        int d = dist(attacker->x, attacker->y, e.x, e.y);
        score -= d / 2;                  // prefer closer
        if (score > bestScore) { bestScore = score; best = &e; }
    }
    return best ? best->id : -1;
}

static void tickAIForOwner(int o) {
    Player& p = g.players[o];

    int peas = aiCount(o,E_PEASANT), mil = aiCount(o,E_MILITIA);
    int arch = aiCount(o,E_ARCHER),  kni = aiCount(o,E_KNIGHT);
    int cat  = aiCountAll(o,E_CATAPULT);
    int hous = aiCountAll(o,E_HOUSE), bar = aiCount(o,E_BARRACKS), stb = aiCount(o,E_STABLE);

    AIIntel intel = aiScout(o);

    aiGather(o);

    // Caps scale with what the player has fielded — match and exceed.
    int peasCap = std::max(12, intel.playerPeasants + 4);
    int milCap  = std::max(8,  intel.playerArmy + 4);
    int archCap = std::max(6,  intel.playerArmy/2 + 3);
    int kniCap  = std::max(4,  intel.playerArmy/3 + 2);
    int towerCap= (intel.playerArmy >= 6 || intel.playerCastles > 0) ? 4 : 2;

    // === ECONOMY: peasants from every TH/Castle ===
    if (peas < peasCap) {
        for (auto& th : g.entities) {
            if (!th.alive || th.owner != o || th.underConstruction) continue;
            if (th.type != E_TOWNHALL && th.type != E_CASTLE) continue;
            if (th.producing != E_NONE) continue;
            if (p.gold >= 50) { orderTrain(th, E_PEASANT); break; }
        }
    }

    // === SUPPLY: keep houses ahead of training ===
    if (p.supply + 4 >= p.supplyMax && hous < 12 && p.wood >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_HOUSE,bx,by); if(bx>=0) orderBuild(*b,E_HOUSE,bx,by); }
    }

    // === MILITARY BUILDINGS ===
    if (bar == 0 && p.wood >= 150 && peas >= 2) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && p.wood >= 150 && p.gold >= 100) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) orderBuild(*b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) orderBuild(*b,E_STABLE,bx,by); }
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needCat = (intel.playerCastles > 0 || intel.playerWalls > 6);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (needCat && cat < 2 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { orderTrain(br, E_CATAPULT); continue; }
        if (mil  < milCap  && p.gold >= 60 && p.food >= 20) { orderTrain(br, E_MILITIA); continue; }
        if (arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) orderTrain(st, E_KNIGHT);
    }

    // === DEFENSE: towers scaled to threat ===
    if (aiCountAll(o,E_TOWER) < towerCap && mil >= 2 && p.wood >= 100 && p.gold >= 50) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_TOWER,bx,by); if(bx>=0) orderBuild(*b,E_TOWER,bx,by); }
    }

    // === FOOD: mill + farms scale up before winter ===
    if (aiCountAll(o,E_MILL) == 0 && p.wood >= 100) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MILL,bx,by); if(bx>=0) orderBuild(*b,E_MILL,bx,by); }
    }
    int wantFarms = (getSeason() == AUTUMN) ? 8 : 4;
    if (aiCountAll(o,E_MILL) > 0 && aiCountAll(o,E_FARM) < wantFarms && getSeason() != WINTER) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FARM,bx,by); if(bx>=0) orderBuild(*b,E_FARM,bx,by); }
    }

    // === NAVAL: dock + boats if water nearby ===
    if (aiCountAll(o,E_DOCK) == 0 && p.wood >= 100 && peas >= 4) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (th) {
            int bx=-1,by=-1; aiBuildSpotNear(o, E_DOCK, th->x, th->y, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(o);
                if (b) orderBuild(*b, E_DOCK, bx, by);
            }
        }
    }
    for (auto& dk : g.entities) {
        if (!dk.alive || dk.owner != o || dk.type != E_DOCK || dk.underConstruction) continue;
        if (dk.producing != E_NONE) continue;
        // Fishing boats first for food, then a couple of warships for coastline pressure.
        if (aiCount(o,E_FISHING_BOAT) < 3 && p.gold >= 80 && p.wood >= 50) { orderTrain(dk, E_FISHING_BOAT); continue; }
        if (aiCount(o,E_WARSHIP) < 2 && p.gold >= 150 && p.wood >= 80 && p.food >= 20) { orderTrain(dk, E_WARSHIP); continue; }
    }

    // === EXPANSION: forward TH halfway to the player ===
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
                if (b) orderBuild(*b, E_TOWNHALL, bx, by);
            }
        }
    }

    // === GARRISON: pack archers into the nearest tower/TH/Castle ===
    for (auto& bld : g.entities) {
        if (!bld.alive || bld.owner != o || bld.underConstruction) continue;
        if (!canGarrisonIn(bld.type)) continue;
        if ((int)bld.garrison.size() >= garrisonCap(bld.type)) continue;
        Entity* archer = nullptr; int bestD = 99999;
        for (auto& u : g.entities) {
            if (!u.alive || u.owner != o || u.state != S_IDLE) continue;
            if (u.type != E_ARCHER) continue;
            int d = mdist(u.x, u.y, bld.x, bld.y);
            if (d < bestD) { bestD = d; archer = &u; }
        }
        if (archer) orderGarrison(*archer, bld.id);
    }

    // === ATTACK RHYTHM: send waves, smart targets ===
    int army = mil + arch + kni + cat;
    if (p.aiWaveCd > 0) p.aiWaveCd--;
    int attackThreshold = 3;
    if (army >= attackThreshold && p.aiWaveCd == 0) {
        Entity* anchor = nullptr;
        for (auto& e : g.entities) {
            if (!e.alive || e.owner != o) continue;
            if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT) continue;
            if (e.state != S_IDLE) continue;
            anchor = &e; break;
        }
        if (anchor) {
            int tid = aiPickTarget(o, anchor);
            // Fallback: if intel found a TH, march on it directly (hunt the player out)
            if (tid < 0 && intel.playerTH) tid = intel.playerTH->id;
            if (tid >= 0) {
                for (auto& e : g.entities) {
                    if (!e.alive || e.owner != o) continue;
                    if (!isUnit(e.type) || e.type == E_PEASANT || e.type == E_FISHING_BOAT) continue;
                    if (e.state == S_IDLE) orderAttack(e, tid);
                }
                p.aiWaveCd = 5; // re-pick target every ~5 AI ticks
            }
        }
    }

    // === DEFENSE: respond to threats near any owned TH/Castle ===
    for (auto& base : g.entities) {
        if (!base.alive || base.owner != o) continue;
        if (base.type != E_TOWNHALL && base.type != E_CASTLE) continue;
        for (auto& en : g.entities) {
            if (!en.alive || en.owner == o || en.owner == OWNER_NATURE) continue;
            if (en.state == S_GARRISONED) continue;
            if (dist(en.x, en.y, base.x, base.y) < 22) {
                for (auto& d : g.entities)
                    if (d.alive && d.owner == o && isUnit(d.type)
                        && d.type != E_PEASANT && d.type != E_FISHING_BOAT && d.state == S_IDLE)
                        orderAttack(d, en.id);
                break;
            }
        }
    }
}

void tickAI() {
    g.aiTimer++;
    if (g.aiTimer < 12) return;
    g.aiTimer = 0;
    for (int o = 1; o < MAX_PLAYERS; o++) {
        if (!g.players[o].alive) continue;
        tickAIForOwner(o);
    }
}
