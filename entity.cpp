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
    // Rivers freeze solid in mid-to-late winter and become walkable
    if (t == T_WATER && getSeason() == WINTER && getSeasonProgress() > 0.25f) return true;
    return t != T_MOUNTAIN && t != T_WATER && t != T_ICE && t != T_STONE && t != T_CASTLE_WALL;
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
        bool isBase = (o.type == E_TOWNHALL || o.type == E_CASTLE);
        bool isWood = (o.type == E_LUMBER_CAMP && e.gatherType == 1);
        bool isGold = (o.type == E_MINING_CAMP && e.gatherType == 0);
        if (isBase || isWood || isGold) {
            int d = mdist(e.x, e.y, o.x, o.y);
            if (d < bestD) { bestD = d; best = &o; }
        }
    }
    return best;
}

Entity* entityAt(int x, int y) {
    for (auto& e : g.entities) {
        if (!e.alive) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

Entity* entityAtOwner(int x, int y, int owner) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;
        auto& s = STATS[e.type];
        if (s.isBuilding) { if (x>=e.x && x<e.x+s.sizeW && y>=e.y && y<e.y+s.sizeH) return &e; }
        else if (e.x == x && e.y == y) return &e;
    }
    return nullptr;
}

bool canPlace(EntityType type, int x, int y, int owner) {
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
        if (g.map[ny][nx].terrain == T_GOLD) return false;
        if (entityAt(nx,ny)) return false;
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
// PATHFINDING
// ============================================================
std::vector<std::pair<int,int>> findPath(int sx, int sy, int tx, int ty, int maxSteps) {
    if (sx == tx && sy == ty) return {};
    if (!isPassable(tx, ty)) {
        int bestD = 9999, bx = -1, by = -1;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            int nx = tx+dx, ny = ty+dy;
            if (isPassable(nx,ny)) { int d=mdist(sx,sy,nx,ny); if(d<bestD){bestD=d;bx=nx;by=ny;} }
        }
        if (bx < 0) return {};  // surrounded by impassable terrain, give up
        tx = bx; ty = by;
    }
    static int visited[MAP_H][MAP_W]; static int vgen = 0;
    struct Node { int x, y, px, py; };
    static Node hist[MAP_W*MAP_H];
    static const int dx8[] = {0,1,1,1,0,-1,-1,-1};
    static const int dy8[] = {-1,-1,0,1,1,1,0,-1};

    // Two passes: first avoids idle units (routes around clusters), second ignores them (fallback)
    for (int pass = 0; pass < 2; pass++) {
        bool avoidIdle = (pass == 0);
        vgen++; int hc = 0; bool found = false; int fi = -1;
        std::queue<Node> q;
        q.push({sx, sy, -1, -1}); visited[sy][sx] = vgen;
        while (!q.empty() && hc < maxSteps*4) {
            Node cur = q.front(); q.pop();
            int idx = hc; hist[hc++] = cur;
            if (cur.x == tx && cur.y == ty) { found = true; fi = idx; break; }
            for (int i = 0; i < 8; i++) {
                int nx = cur.x+dx8[i], ny = cur.y+dy8[i];
                if (!inBounds(nx,ny) || visited[ny][nx] == vgen) continue;
                if (!isPassable(nx,ny)) continue;
                Entity* occ = entityAt(nx,ny);
                if (occ && isBuilding(occ->type) && !(nx==tx && ny==ty)) continue;
                // Pass 0: treat idle non-building units as soft obstacles — route around them
                if (avoidIdle && occ && !isBuilding(occ->type) && occ->state==S_IDLE
                    && !(nx==tx && ny==ty)) continue;
                visited[ny][nx] = vgen;
                q.push({nx, ny, cur.x, cur.y});
            }
        }
        if (!found) continue; // retry without the idle-avoidance constraint
        std::vector<std::pair<int,int>> path;
        int cx = hist[fi].x, cy = hist[fi].y;
        path.push_back({cx, cy});
        int px = hist[fi].px, py = hist[fi].py;
        while (px != -1) {
            path.push_back({px, py});
            for (int i = fi-1; i >= 0; i--) {
                if (hist[i].x == px && hist[i].y == py) { px=hist[i].px; py=hist[i].py; fi=i; break; }
            }
            if ((int)path.size() > 600) break;
        }
        std::reverse(path.begin(), path.end());
        if (!path.empty()) path.erase(path.begin());
        return path;
    }
    return {};
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
    g.entities.push_back(e);
    updateSupply(owner);
    return e.id;
}

void updateFog() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        g.map[y][x].visible[0] = false;
        g.map[y][x].visible[1] = false;
    }
    int nightPen = isNight() ? 2 : (isDusk()||isDawn()) ? 1 : 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner >= OWNER_NATURE) continue;
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
Entity* findNearestEnemy(Entity& e, int range) {
    Entity* best = nullptr; int bestD = range + 1;
    for (auto& o : g.entities) {
        if (!o.alive || o.owner == e.owner) continue;
        // Non-nature units do not auto-attack nature entities (deer/wolf/sheep)
        if (e.owner != OWNER_NATURE && o.owner == OWNER_NATURE) continue;
        int d = dist(e.x, e.y, o.x, o.y);
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}

void orderMove(Entity& e, int tx, int ty) {
    e.state = S_MOVING; e.targetX = tx; e.targetY = ty; e.targetId = -1;
    e.path = findPath(e.x, e.y, tx, ty); e.pathIdx = 0;
    // If path is empty but we're not already there, scan expanding rings for a reachable nearby tile
    if (e.path.empty() && (e.x != tx || e.y != ty)) {
        for (int r = 1; r <= 3 && e.path.empty(); r++)
            for (int dy = -r; dy <= r && e.path.empty(); dy++)
                for (int dx = -r; dx <= r && e.path.empty(); dx++) {
                    if (std::abs(dx)!=r && std::abs(dy)!=r) continue;
                    int nx=tx+dx, ny=ty+dy;
                    if (!inBounds(nx,ny)||!isPassable(nx,ny)) continue;
                    Entity* occ = entityAt(nx,ny);
                    if (occ && occ->state==S_IDLE) continue;
                    e.path = findPath(e.x, e.y, nx, ny);
                }
    }
}

void orderAttack(Entity& e, int tid) {
    Entity* t = findEntity(tid);
    if (!t) return;
    e.state = S_ATTACKING; e.targetId = tid;
}

void orderGather(Entity& e, int tx, int ty) {
    if (e.type != E_PEASANT) return;
    Terrain ter = g.map[ty][tx].terrain;
    bool isW = (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE);
    if (ter != T_GOLD && !isW) return;
    e.state = S_GATHERING; e.targetX = tx; e.targetY = ty;
    e.gatherType = (ter == T_GOLD) ? 0 : 1;
    // Path to nearest adjacent passable tile so multiple peasants don't block each other on same resource
    int bestAX = tx, bestAY = ty, bestAD = 99999;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx==0 && dy==0) continue;
        int nx = tx+dx, ny = ty+dy;
        if (inBounds(nx,ny) && isPassable(nx,ny)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestAD) { bestAD = d; bestAX = nx; bestAY = ny; }
        }
    }
    e.path = findPath(e.x, e.y, bestAX, bestAY); e.pathIdx = 0;
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
    e.path = findPath(e.x, e.y, bestAX, bestAY); e.pathIdx = 0;
}

void orderTrain(Entity& bld, EntityType ut) {
    if (!isBuilding(bld.type) || bld.underConstruction) return;
    if (bld.producing != E_NONE) { if (bld.owner==0) setStatus("Already training!"); return; }
    Player& p = g.players[bld.owner];
    if (p.gold < STATS[ut].costGold || p.wood < STATS[ut].costWood) {
        if (bld.owner==0) setStatus("Not enough resources!"); return;
    }
    if (p.supply + STATS[ut].supplyUsed > p.supplyMax) {
        if (bld.owner==0) setStatus("Need more houses!"); return;
    }
    int foodCost = 0;
    if (ut==E_MILITIA||ut==E_ARCHER) foodCost = 20;
    else if (ut==E_KNIGHT) foodCost = 40;
    else if (ut==E_CATAPULT) foodCost = 30;
    if (p.food < foodCost) { if (bld.owner==0) setStatus("Need more food!"); return; }
    p.food -= foodCost;
    p.gold -= STATS[ut].costGold; p.wood -= STATS[ut].costWood;
    bld.producing = ut; bld.prodProgress = 0; bld.prodTime = STATS[ut].trainTime;
    bld.state = S_TRAINING;
}

void orderGroupMove(int tx, int ty) {
    std::vector<Entity*> units;
    for (int id : g.selectedIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && e->owner == 0 && isUnit(e->type))
            units.push_back(e);
    }
    if (units.empty()) return;
    int N = (int)units.size();
    int cols = std::max(1, (int)ceil(sqrt((double)N)));
    int rows  = (N + cols - 1) / cols;
    int offsetX = (cols - 1) / 2;
    int offsetY = (rows - 1) / 2;
    std::vector<std::pair<int,int>> slots;
    for (int r = 0; r < rows && (int)slots.size() < N; r++) {
        for (int c = 0; c < cols && (int)slots.size() < N; c++) {
            int sx = std::max(0, std::min(tx - offsetX + c, MAP_W-1));
            int sy = std::max(0, std::min(ty - offsetY + r, MAP_H-1));
            slots.push_back({sx, sy});
        }
    }
    std::vector<bool> taken(slots.size(), false);
    for (Entity* u : units) {
        int best = -1, bestD = 9999;
        for (int i = 0; i < (int)slots.size(); i++) {
            if (taken[i]) continue;
            int d = mdist(u->x, u->y, slots[i].first, slots[i].second);
            if (d < bestD) { bestD = d; best = i; }
        }
        if (best >= 0) { taken[best] = true; orderMove(*u, slots[best].first, slots[best].second); }
    }
    setStatus("Group moving in formation...");
}

void orderGroupAttack(int tid) {
    for (int id : g.selectedIds) {
        Entity* e = findEntity(id);
        if (e && e->alive && e->owner == 0 && isUnit(e->type))
            orderAttack(*e, tid);
    }
    setStatus("Group attacking!");
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
    e.path = findPath(e.x, e.y, bestAX, bestAY); e.pathIdx = 0;
}

// ============================================================
// MOVEMENT
// ============================================================
void moveAlongPath(Entity& e) {
    if (e.pathIdx >= (int)e.path.size()) {
        e.path.clear(); e.pathIdx = 0;
        if (e.state == S_MOVING) e.state = S_IDLE;
        return;
    }
    if (e.moveCd > 0) { e.moveCd--; return; }
    auto [nx, ny] = e.path[e.pathIdx];
    Entity* blk = entityAt(nx, ny);
    if (blk && blk->id != e.id) {
        e.stuckTicks++;
        e.moveCd = 1 + (e.id % 2); // short staggered wait before retry
        // Force repath after a few blocked attempts; stagger per entity to prevent thrash
        if (e.stuckTicks >= 3 + (e.id % 3) && !e.path.empty()
            && (e.state==S_MOVING||e.state==S_GATHERING||e.state==S_BUILDING)) {
            auto dest = e.path.back();
            e.path = findPath(e.x, e.y, dest.first, dest.second);
            e.pathIdx = 0;
            e.stuckTicks = 0;
        }
        return;
    }
    e.stuckTicks = 0;
    e.x = nx; e.y = ny; e.pathIdx++;
    Terrain ter = g.map[ny][nx].terrain;
    int spd = STATS[e.type].speed;
    if (ter==T_ROAD||ter==T_DIRT||ter==T_CASTLE_FLOOR) spd = std::max(1, spd-1);
    else if (ter==T_MARSH||ter==T_SHALLOWS||ter==T_SAND||ter==T_SNOW) spd += 1;
    if (getSeason() == WINTER) spd = std::max(spd, STATS[e.type].speed+1);
    e.moveCd = spd;
}

// Scan explored/visible tiles for a resource matching e.gatherType; re-issue gather if found.
static bool findNearbyResource(Entity& e) {
    bool wantGold = (e.gatherType == 0);
    int bestD = 99999, bx = -1, by = -1;
    int r = FOG_RADIUS * 4;
    for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        int nx = e.x+dx, ny = e.y+dy;
        if (!inBounds(nx,ny)) continue;
        if (e.owner < OWNER_NATURE && !g.map[ny][nx].explored[e.owner]) continue;
        if (g.map[ny][nx].resources <= 0) continue;
        Terrain t = g.map[ny][nx].terrain;
        bool isGold = (t == T_GOLD);
        bool isWood = (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE);
        if ((wantGold && isGold) || (!wantGold && isWood)) {
            int d = mdist(e.x, e.y, nx, ny);
            if (d < bestD) { bestD = d; bx = nx; by = ny; }
        }
    }
    if (bx >= 0) { orderGather(e, bx, by); return true; }
    return false;
}

// ============================================================
// ENTITY TICK
// ============================================================
void tickEntity(Entity& e) {
    if (!e.alive) return;
    // Building production
    if (e.producing != E_NONE && !e.underConstruction) {
        int bonus = 0;
        for (auto& o : g.entities)
            if (o.alive && o.owner==e.owner && o.type==E_BLACKSMITH && !o.underConstruction) { bonus=1; break; }
        e.prodProgress += 1 + bonus;
        if (e.prodProgress >= e.prodTime) {
            auto& bs = STATS[e.type]; bool placed = false;
            for (int r = 0; r <= 3 && !placed; r++)
                for (int dy = -r; dy <= bs.sizeH+r && !placed; dy++)
                    for (int dx = -r; dx <= bs.sizeW+r && !placed; dx++) {
                        int nx = e.x+dx, ny = e.y+dy;
                        if (!inBounds(nx,ny)||!isPassable(nx,ny)||entityAt(nx,ny)) continue;
                        spawnEntity(e.producing, e.owner, nx, ny);
                        placed = true;
                    }
            e.producing = E_NONE; e.state = S_IDLE;
            if (e.owner==0 && placed) setStatus("Training complete!");
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
                for (auto& o : g.entities)
                    if (o.alive && o.state==S_BUILDING && o.targetId==e.id) o.state = S_IDLE;
            }
        }
        return;
    }
    if (!isUnit(e.type)) return;

    switch (e.state) {
    case S_IDLE:
        if (e.type != E_PEASANT && STATS[e.type].atk > 0) {
            Entity* en = findNearestEnemy(e, STATS[e.type].range+1);
            if (en) orderAttack(e, en->id);
        }
        break;
    case S_MOVING:
        moveAlongPath(e);
        if (e.path.empty() || e.pathIdx >= (int)e.path.size()) e.state = S_IDLE;
        break;
    case S_ATTACKING: {
        Entity* t = findEntity(e.targetId);
        if (!t || !t->alive) { e.state = S_IDLE; break; }
        int d = dist(e.x, e.y, t->x, t->y);
        if (d <= STATS[e.type].range) {
            if (e.atkCd <= 0) {
                t->hp -= STATS[e.type].atk;
                e.atkCd = STATS[e.type].atkSpeed;
                if (isRanged(e.type)) {
                    char pc = (e.type==E_CATAPULT) ? 'o' : '-';
                    int pcol = (e.type==E_CATAPULT) ? CP_PROJ_BOULDER : CP_PROJ_ARROW;
                    spawnProjectile(e.x, e.y, t->x, t->y, pc, pcol);
                }
                if (t->hp <= 0) {
                    t->alive=false; t->state=S_DEAD; e.state=S_IDLE; updateSupply(t->owner);
                    if (t->owner == OWNER_NATURE && e.owner < OWNER_NATURE) {
                        int food = (t->type==E_SHEEP)?80:(t->type==E_DEER)?120:30;
                        g.players[e.owner].food += food;
                        if (e.owner==0) setStatus(std::string("Got ") + std::to_string(food) + " food!");
                    }
                }
            } else e.atkCd--;
        } else {
            if (e.path.empty() || e.pathIdx >= (int)e.path.size()) {
                e.path = findPath(e.x, e.y, t->x, t->y); e.pathIdx = 0;
            }
            moveAlongPath(e);
        }
        break;
    }
    case S_GATHERING: {
        int d = dist(e.x, e.y, e.targetX, e.targetY);
        if (d <= 1) {
            Tile& tile = g.map[e.targetY][e.targetX];
            bool isW = (tile.terrain==T_FOREST||tile.terrain==T_PINE||tile.terrain==T_PALM||tile.terrain==T_DEAD_TREE);
            if ((tile.terrain==T_GOLD||isW) && tile.resources > 0) {
                e.gatherCd++;
                if (e.gatherCd >= GATHER_TICKS) {
                    e.gatherCd = 0;
                    int amt = std::min(GATHER_RATE, tile.resources);
                    tile.resources -= amt; e.carrying += amt;
                    if (tile.resources <= 0) tile.terrain = T_DIRT;
                    if (e.carrying >= CARRY_MAX || tile.resources <= 0) {
                        Entity* dep = findDepot(e);
                        if (dep) {
                            e.state = S_RETURNING; e.targetId = dep->id;
                            e.targetX = dep->x; e.targetY = dep->y;
                            e.path = findPath(e.x, e.y, dep->x, dep->y); e.pathIdx = 0;
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
                e.path = findPath(e.x, e.y, e.targetX, e.targetY); e.pathIdx = 0;
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
            e.path = findPath(e.x, e.y, dep->x, dep->y); e.pathIdx = 0;
        }
        int d = dist(e.x, e.y, dep->x, dep->y);
        if (d <= STATS[dep->type].sizeW + 1) {
            if (e.gatherType == 0) g.players[e.owner].gold += e.carrying;
            else                   g.players[e.owner].wood += e.carrying;
            e.carrying = 0;
            Tile& rt = g.map[e.rallyY][e.rallyX];
            bool isW = (rt.terrain==T_FOREST||rt.terrain==T_PINE||rt.terrain==T_PALM||rt.terrain==T_DEAD_TREE);
            if ((rt.terrain==T_GOLD||isW) && rt.resources > 0) {
                e.state = S_GATHERING; e.targetX = e.rallyX; e.targetY = e.rallyY;
                e.path = findPath(e.x, e.y, e.rallyX, e.rallyY); e.pathIdx = 0;
            } else {
                // Rally point depleted — seek another nearby node of the same type
                if (!findNearbyResource(e)) e.state = S_IDLE;
            }
        } else {
            moveAlongPath(e);
            if (e.path.empty() && dist(e.x,e.y,dep->x,dep->y) > STATS[dep->type].sizeW+1) {
                e.path = findPath(e.x, e.y, dep->x, dep->y); e.pathIdx = 0;
            }
        }
        break;
    }
    case S_BUILDING: {
        Entity* bld = findEntity(e.targetId);
        if (!bld || !bld->alive) { e.state = S_IDLE; break; }
        // Tending a completed farm — stay adjacent
        if (!bld->underConstruction && bld->type == E_FARM) {
            if (dist(e.x, e.y, bld->x, bld->y) > 1) {
                moveAlongPath(e);
                if (e.path.empty()) { e.path = findPath(e.x, e.y, bld->x, bld->y); e.pathIdx = 0; }
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
                e.path = findPath(e.x, e.y, bestAX, bestAY); e.pathIdx = 0;
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
    for (auto& e : g.entities) {
        if (!e.alive || e.underConstruction || e.type != E_TOWER) continue;
        Entity* en = findNearestEnemy(e, STATS[E_TOWER].range);
        if (en) {
            if (e.atkCd <= 0) {
                en->hp -= STATS[E_TOWER].atk; e.atkCd = STATS[E_TOWER].atkSpeed;
                spawnProjectile(e.x, e.y, en->x, en->y, '*', CP_PROJ_TOWER);
                if (en->hp <= 0) { en->alive=false; en->state=S_DEAD; updateSupply(en->owner); }
            } else e.atkCd--;
        }
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
    for (int p = 0; p < 2; p++) {
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
    for (int p = 0; p < 2; p++) {
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
                    if (dist(e.x, e.y, o.x, o.y) <= 4) {
                        int fx = std::max(1, std::min(e.x + (e.x-o.x)*4, MAP_W-2));
                        int fy = std::max(1, std::min(e.y + (e.y-o.y)*4, MAP_H-2));
                        if (isPassable(fx, fy)) orderMove(e, fx, fy);
                        break;
                    }
                }
            }
        }

        // Wolves flee settlements and only hunt isolated units far from buildings
        if (e.type == E_WOLF) {
            bool nearSettlement = false;
            int fleeX = -1, fleeY = -1;
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
            if (nearSettlement) {
                if (e.state == S_ATTACKING) { e.state = S_IDLE; e.path.clear(); }
                if (e.state == S_IDLE && fleeX >= 0 && isPassable(fleeX, fleeY))
                    orderMove(e, fleeX, fleeY);
            } else if (e.state==S_IDLE || (e.state==S_MOVING && e.path.empty())) {
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner==OWNER_NATURE || !isUnit(o.type)) continue;
                    if (dist(e.x, e.y, o.x, o.y) <= 3) { orderAttack(e, o.id); break; }
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
    for (int p = 0; p < 2; p++) {
        bool has = false;
        for (auto& e : g.entities)
            if (e.alive && e.owner==p && (e.type==E_TOWNHALL||e.type==E_CASTLE)) has = true;
        if (!has) { g.players[p].alive=false; g.winner=1-p; g.mode=M_GAME_OVER; }
    }
}

// ============================================================
// AI
// ============================================================
int     aiCount(int o, EntityType t)    { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)c++; return c; }
int     aiCountAll(int o, EntityType t) { int c=0; for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t)c++;                    return c; }
Entity* aiIdle(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&e.state==S_IDLE&&!e.underConstruction)return &e; return nullptr; }
Entity* aiBldg(int o, EntityType t)     { for(auto& e:g.entities) if(e.alive&&e.owner==o&&e.type==t&&!e.underConstruction)return &e; return nullptr; }

void aiGather(int o) {
    for (auto& e : g.entities) {
        if (!e.alive || e.owner!=o || e.type!=E_PEASANT || e.state!=S_IDLE) continue;
        int bestD = 9999, bx = -1, by = -1;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Terrain t = g.map[y][x].terrain;
            bool isR = (t==T_GOLD||t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE);
            if (isR && g.map[y][x].resources > 0) {
                int d = mdist(e.x, e.y, x, y);
                if (d < bestD) { bestD=d; bx=x; by=y; }
            }
        }
        if (bx >= 0) orderGather(e, bx, by);
    }
}

void aiBuildSpot(int o, EntityType bt, int& ox, int& oy) {
    Entity* th = aiBldg(o, E_TOWNHALL);
    if (!th) th = aiBldg(o, E_CASTLE);
    if (!th) return;
    for (int r = 3; r < 15; r++) for (int a = 0; a < 20; a++) {
        int bx = th->x + (rand()%(r*2+1))-r, by = th->y + (rand()%(r*2+1))-r;
        if (canPlace(bt, bx, by, o)) { ox=bx; oy=by; return; }
    }
}

void tickAI() {
    g.aiTimer++;
    if (g.aiTimer < 15) return;
    g.aiTimer = 0;
    int o = 1; Player& p = g.players[o];
    int peas = aiCount(o,E_PEASANT), mil = aiCount(o,E_MILITIA);
    int arch = aiCount(o,E_ARCHER),  kni = aiCount(o,E_KNIGHT);
    int hous = aiCountAll(o,E_HOUSE), bar = aiCount(o,E_BARRACKS), stb = aiCount(o,E_STABLE);

    aiGather(o);

    // Keep training peasants up to a higher cap
    if (peas < 8) {
        Entity* th = aiBldg(o, E_TOWNHALL);
        if (th && th->producing==E_NONE && p.gold>=50) orderTrain(*th, E_PEASANT);
    }
    // Build houses proactively for supply room
    if (p.supply+3 >= p.supplyMax && hous<8 && p.wood>=50) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_HOUSE,bx,by); if(bx>=0) orderBuild(*b,E_HOUSE,bx,by); }
    }
    // Barracks as soon as we have 2 peasants
    if (bar==0 && p.wood>=150 && peas>=2) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    // Train military aggressively — more militia, more archers
    if (bar > 0) {
        Entity* br = aiBldg(o, E_BARRACKS);
        if (br && br->producing==E_NONE) {
            if (mil<8 && p.gold>=60) orderTrain(*br, E_MILITIA);
            else if (arch<4 && p.gold>=70) orderTrain(*br, E_ARCHER);
        }
    }
    // Blacksmith before stable (combat bonus matters)
    if (aiCount(o,E_BLACKSMITH)==0 && bar>0 && p.wood>=120) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) orderBuild(*b,E_BLACKSMITH,bx,by); }
    }
    if (stb==0 && mil>=4 && p.wood>=200) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) orderBuild(*b,E_STABLE,bx,by); }
    }
    if (stb > 0) {
        Entity* st = aiBldg(o, E_STABLE);
        if (st && st->producing==E_NONE && kni<4 && p.gold>=120) orderTrain(*st, E_KNIGHT);
    }
    if (aiCountAll(o,E_TOWER)<2 && mil>=2 && p.wood>=100 && p.gold>=50) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_TOWER,bx,by); if(bx>=0) orderBuild(*b,E_TOWER,bx,by); }
    }
    if (aiCountAll(o,E_MILL)==0 && p.wood>=100 && bar>0) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_MILL,bx,by); if(bx>=0) orderBuild(*b,E_MILL,bx,by); }
    }
    if (aiCountAll(o,E_MILL)>0 && aiCountAll(o,E_FARM)<3 && getSeason()!=WINTER) {
        Entity* b = aiIdle(o, E_PEASANT); if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_FARM,bx,by); if(bx>=0) orderBuild(*b,E_FARM,bx,by); }
    }

    // Attack with just 3 units — don't stockpile, keep pressure on
    int army = mil + arch + kni;
    if (army >= 3) {
        Entity* pt = nullptr;
        for (auto& e : g.entities) if (e.alive && e.owner==0) {
            if (e.type==E_TOWNHALL||e.type==E_CASTLE) { pt=&e; break; }
            pt = &e;
        }
        if (pt) for (auto& e : g.entities)
            if (e.alive && e.owner==o && isUnit(e.type) && e.type!=E_PEASANT && e.state==S_IDLE)
                orderAttack(e, pt->id);
    }

    // Defend: respond to any enemy unit within 20 tiles of base
    Entity* th = aiBldg(o, E_TOWNHALL);
    if (!th) th = aiBldg(o, E_CASTLE);
    if (th) {
        for (auto& en : g.entities) {
            if (!en.alive || en.owner==o || en.owner==OWNER_NATURE) continue;
            if (dist(en.x,en.y,th->x,th->y) < 20) {
                for (auto& d : g.entities)
                    if (d.alive && d.owner==o && isUnit(d.type) && d.type!=E_PEASANT && d.state==S_IDLE)
                        orderAttack(d, en.id);
                break;
            }
        }
    }
}
