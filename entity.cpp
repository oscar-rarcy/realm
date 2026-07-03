#include "realm.h"
#include <cmath>

// ============================================================
// TIME
// ============================================================
float getBrightness() {
    // Own constant: M_PI is a POSIX extension MinGW's <cmath> hides.
    constexpr float kPi = 3.14159265358979f;
    float b = sinf(g.dayPhase * kPi);
    // Seasonal day length: midsummer nights are short, midwinter nights long.
    // Biasing brightness moves the isNight()/dusk thresholds, which stretches
    // or shrinks the dark window without touching the day-cycle clock.
    Season s = getSeason();
    if (s == SUMMER)      b += 0.12f;
    else if (s == WINTER) b -= 0.10f;
    return std::max(0.0f, std::min(1.0f, b));
}
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

// Elevation step rule: land movement between tiles of different height needs
// a ramp — a T_HILLS tile on either end bridges a one-level difference.
// Everywhere else the plateau rim is a cliff. Water is all at sea level, so
// naval movement ignores elevation entirely. Callers guarantee bounds.
bool canStep(int fx, int fy, int tx, int ty, bool naval) {
    if (naval) return true;
    int d = std::abs(g.map[fy][fx].elev - g.map[ty][tx].elev);
    if (d == 0) return true;
    if (d > 1)  return false;
    return g.map[fy][fx].terrain == T_HILLS || g.map[ty][tx].terrain == T_HILLS;
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

// 50 ticks ≈ 4 s on screen — the old 35 vanished before long lines were read.
void setStatus(const std::string& msg) {
    g.statusMsg = msg; g.statusTimer = 50;
    // Mirror into the rolling event log so a glanced-away player can catch up
    // on what just happened (dedup back-to-back repeats like chain-build hints).
    if (g.eventLog.empty() || g.eventLog.back() != msg) {
        g.eventLog.push_back(msg);
        if ((int)g.eventLog.size() > 6) g.eventLog.erase(g.eventLog.begin());
    }
}

// ============================================================
// STOCKPILES — wealth physically lives at depot buildings (DF lite).
// Player.gold/wood/food remain the cached totals every cost check reads;
// the per-depot stores say WHERE it sits. A burned depot forfeits its
// share and scatters part of it as loot; spending drains the piles
// nearest the paying site, so frontier logistics emerge on their own.
// ============================================================
bool isDepot(EntityType t) {
    return depotCapGold(t) > 0 || depotCapWood(t) > 0 || depotCapFood(t) > 0;
}
int depotCapGold(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return 400;  case E_CASTLE: return 600;
        case E_MINING_CAMP: return 250; case E_TRADING_POST: return 150;
        case E_STOCKYARD: return 300;   // three open pallet-tiles of coin
        default: return 0;
    }
}
int depotCapWood(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return 400;  case E_CASTLE: return 600;
        case E_LUMBER_CAMP: return 250;
        case E_STOCKYARD: return 300;
        default: return 0;
    }
}
int depotCapFood(EntityType t) {
    switch (t) {
        case E_TOWNHALL: return 400;  case E_CASTLE: return 600;
        case E_MILL: return 300;      case E_GRANARY: return 600;
        case E_DOCK: return 200;      case E_TAVERN: return 200;
        case E_WATERMILL: return 150; case E_STOCKYARD: return 300;
        default: return 0;
    }
}
int depotFoodSum(const Entity& e) {
    int s = 0;
    for (int k = 0; k < F_COUNT; k++) s += e.storeFood[k];
    return s;
}

// Deposit food of a given kind at a depot. Ale never counts toward the
// edible total — it's drunk, not eaten.
void addFood(int owner, int kind, int amount, Entity* depot) {
    if (kind != F_ALE) g.players[owner].food += amount;
    if (depot) depot->storeFood[kind] += amount;
}

// Eat spoilables first: berries rot before fish, fish before meat, and the
// granary grain is the winter reserve that's touched last.
void spendPlayerFood(int owner, int amount) {
    Player& p = g.players[owner];
    int spent = std::min(amount, p.food);
    p.food -= spent;
    static const int EAT_ORDER[] = {F_BERRY, F_FISH, F_MEAT, F_GRAIN};
    for (int kind : EAT_ORDER) {
        if (spent <= 0) break;
        for (auto& e : g.entities) {
            if (spent <= 0) break;
            if (!e.alive || e.owner != owner || !isBuilding(e.type)) continue;
            int take = std::min(e.storeFood[kind], spent);
            e.storeFood[kind] -= take; spent -= take;
        }
    }
}

// Pull a specific kind (e.g. brewing wants grain). Fails without spending
// if the player doesn't hold enough of that kind anywhere.
bool spendFoodKind(int owner, int kind, int amount) {
    int have = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner || !isBuilding(e.type)) continue;
        have += e.storeFood[kind];
    }
    if (have < amount) return false;
    if (kind != F_ALE) g.players[owner].food -= std::min(amount, g.players[owner].food);
    for (auto& e : g.entities) {
        if (amount <= 0) break;
        if (!e.alive || e.owner != owner || !isBuilding(e.type)) continue;
        int take = std::min(e.storeFood[kind], amount);
        e.storeFood[kind] -= take; amount -= take;
    }
    return true;
}

// Spend gold/wood: totals drop, and the piles nearest the paying site are
// the ones that empty — a drained frontier camp means longer hauls.
void drainStores(int owner, int gold, int wood, int x, int y) {
    Player& p = g.players[owner];
    p.gold -= std::min(gold, p.gold);
    p.wood -= std::min(wood, p.wood);
    std::vector<Entity*> depots;
    for (auto& e : g.entities)
        if (e.alive && e.owner == owner && isBuilding(e.type)) depots.push_back(&e);
    std::sort(depots.begin(), depots.end(), [x,y](Entity* a, Entity* b){
        int da = mdist(a->x, a->y, x, y), db = mdist(b->x, b->y, x, y);
        return da != db ? da < db : a->id < b->id;
    });
    for (Entity* d : depots) {
        if (gold <= 0 && wood <= 0) break;
        int tg = std::min(d->storeGold, gold); d->storeGold -= tg; gold -= tg;
        int tw = std::min(d->storeWood, wood); d->storeWood -= tw; wood -= tw;
    }
}

// Credit income (market trades, tolls) to the nearest depot with capacity.
void depositToNearest(int owner, int gold, int wood, int foodKind, int food, int x, int y) {
    Player& p = g.players[owner];
    p.gold += gold; p.wood += wood;
    if (food > 0 && foodKind != F_ALE) p.food += food;
    Entity* bg = nullptr; Entity* bw = nullptr; Entity* bf = nullptr;
    int dg = 99999, dw = 99999, df = 99999;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner || !isBuilding(e.type) || e.underConstruction) continue;
        int d = mdist(e.x, e.y, x, y);
        if (gold > 0 && depotCapGold(e.type) > 0 && d < dg) { dg = d; bg = &e; }
        if (wood > 0 && depotCapWood(e.type) > 0 && d < dw) { dw = d; bw = &e; }
        if (food > 0 && depotCapFood(e.type) > 0 && d < df) { df = d; bf = &e; }
    }
    if (bg) bg->storeGold += gold;
    if (bw) bw->storeWood += wood;
    if (bf) bf->storeFood[foodKind] += food;
}

Entity* findEntity(int id) {
    for (auto& e : g.entities) if (e.id == id && e.alive) return &e;
    return nullptr;
}

Entity* findDepot(Entity& e) {
    // Two passes: prefer depots with spare capacity for what we carry; if
    // every pile is full, fall back to the nearest anyway (over-cap deposits
    // are accepted — caps steer routing, they don't strand couriers).
    for (int pass = 0; pass < 2; pass++) {
        Entity* best = nullptr; int bestD = 99999;
        for (auto& o : g.entities) {
            if (!o.alive || o.owner != e.owner || o.underConstruction) continue;
            bool isBase = (o.type == E_TOWNHALL || o.type == E_CASTLE) && e.gatherType != 2;
            bool isWood = (o.type == E_LUMBER_CAMP && e.gatherType == 1);
            bool isGold = (o.type == E_MINING_CAMP && e.gatherType == 0);
            bool isFish = (o.type == E_DOCK        && e.gatherType == 2);
            // Food couriers (farm/berry/hunt) deliver to Mill, Granary, or a claimed Watermill.
            bool isFood = ((o.type == E_MILL || o.type == E_GRANARY || o.type == E_WATERMILL)
                           && e.gatherType == 3);
            if (!(isBase || isWood || isGold || isFish || isFood)) continue;
            if (pass == 0) {
                bool full = false;
                if      (e.gatherType == 0) full = o.storeGold >= depotCapGold(o.type);
                else if (e.gatherType == 1) full = o.storeWood >= depotCapWood(o.type);
                else                        full = depotFoodSum(o) >= depotCapFood(o.type);
                if (full) continue;
            }
            int d = mdist(e.x, e.y, o.x, o.y);
            if (d < bestD) { bestD = d; best = &o; }
        }
        if (best) return best;
    }
    return nullptr;
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

bool canPlace(EntityType type, int x, int y, int owner, int ignoreId) {
    (void)owner;
    // Top-level bounds check protects every g.map read below, including the
    // farm-only terrain read that previously ran before any inBounds check.
    if (!inBounds(x, y)) return false;
    // Bridges are the inverse of every other building: they REQUIRE water.
    // One tile of water/shallows/reeds, no entity on it, and at least one
    // orthogonal neighbour that's land or another bridge — so spans grow
    // from the shore outward, tile by tile.
    if (type == E_BRIDGE) {
        Terrain t = g.map[y][x].terrain;
        if (t != T_WATER && t != T_SHALLOWS && t != T_REEDS) return false;
        Entity* occ = entityAt(x, y);
        if (occ && (ignoreId < 0 || occ->id != ignoreId)) return false;
        static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& d : d4) {
            int nx2 = x+d[0], ny2 = y+d[1];
            if (!inBounds(nx2,ny2)) continue;
            Terrain nt = g.map[ny2][nx2].terrain;
            if (nt == T_BRIDGE || (isPassable(nx2,ny2)
                && nt!=T_SHALLOWS && nt!=T_REEDS && nt!=T_MARSH && nt!=T_ICE)) return true;
        }
        return false;
    }
    // Farms can only be sown on open ground, not in winter
    if (type == E_FARM) {
        if (getSeason() == WINTER) return false;
        Terrain t = g.map[y][x].terrain;
        if (t!=T_GRASS&&t!=T_MEADOW&&t!=T_TALL_GRASS&&t!=T_FLOWERS&&t!=T_DIRT&&t!=T_WHEAT&&t!=T_SNOW) return false;
    }
    auto& s = STATS[type];
    // The castle is placed as its full 7x7 compound (walls + courtyard +
    // 3x3 keep), so the whole enclosure must fit on clear, level ground.
    int fw = s.sizeW, fh = s.sizeH;
    if (type == E_CASTLE) { fw = 7; fh = 7; }
    for (int dy = 0; dy < fh; dy++) for (int dx = 0; dx < fw; dx++) {
        int nx = x+dx, ny = y+dy;
        if (!inBounds(nx,ny) || !isPassable(nx,ny)) return false;
        // Foundations need level ground — no footprint may straddle a cliff.
        if (g.map[ny][nx].elev != g.map[y][x].elev) return false;
        Terrain ter = g.map[ny][nx].terrain;
        if (ter == T_GOLD) return false;
        // Forests are resource terrain — chop the trees before you can build here.
        if (ter==T_FOREST||ter==T_PINE||ter==T_PALM||ter==T_DEAD_TREE) return false;
        // Land buildings need solid ground — shallows, marsh, reeds and ice are
        // walkable for units but not foundations. (Docks are special: their
        // footprint must be land, plus one neighbour must be water — handled below.)
        if (ter==T_SHALLOWS||ter==T_MARSH||ter==T_REEDS||ter==T_ICE) return false;
        Entity* o = entityAt(nx,ny);
        // ignoreId lets the builder peasant stand on its own foundation tile —
        // it'll path off via orderBuild's adjacent-tile pick next tick.
        if (o && o->id != ignoreId) return false;
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
// SPAWN / FOG
// ============================================================
int spawnEntity(EntityType type, int owner, int x, int y, bool built) {
    Entity e{};
    e.id = g.nextId++; e.type = type; e.owner = owner; e.x = x; e.y = y;
    e.maxHp = STATS[type].maxHp;
    // Stone construction: with a working stonemason, defensive works go up
    // in dressed stone — double HP for walls, gates, and towers.
    if ((type==E_WALL || type==E_GATE || type==E_TOWER) && owner < MAX_PLAYERS) {
        for (auto& m : g.entities)
            if (m.alive && m.owner == owner && m.type == E_STONEMASON && !m.underConstruction)
                { e.maxHp *= 2; break; }
        // Hillfolk build defence in dry-stone from the start (+50%, stacks).
        if (g.players[owner].civ == CIV_HILLFOLK) e.maxHp = e.maxHp * 3 / 2;
    }
    // Horse Breeding: cavalry musters with deeper wind and heavier stock.
    if ((type==E_KNIGHT || type==E_HUSSAR) && owner < MAX_PLAYERS
        && (g.players[owner].research & R_HORSE_BREEDING)) e.maxHp += 15;
    e.hp = built ? e.maxHp : 1;
    // Combat feel: full heart and wind on muster (docs/combat-feel-proposals.md).
    e.morale = 100; e.stamina = 100;
    e.routTicks = 0; e.chargeSteps = 0; e.kills = 0;
    e.prisoner = 0; e.origOwner = -1; e.captureTicks = 0; e.entrenchTicks = 0;
    e.state = S_IDLE; e.targetId = -1; e.targetX = -1; e.targetY = -1;
    e.producing = E_NONE; e.underConstruction = !built; e.alive = true;
    e.rallyX = x + STATS[type].sizeW; e.rallyY = y + STATS[type].sizeH;
    if (type == E_FISHING_BOAT) e.gatherType = 2; // fish
    if (type == E_TREBUCHET)   { e.packed = 1; e.packTicks = 0; } // spawn mobile
    g.entities.push_back(e);
    updateSupply(owner);
    return e.id;
}

// Does a cliff (or any tile higher than the viewer) sit on the line between the
// viewer and a higher target tile? If so the viewer is standing under a cliff
// looking up, and the ground above it stays hidden. Walks the Bresenham line
// between the two points, ignoring both endpoints (the viewer's own tile and
// the target). Only the near lip of a plateau — the first raised tile on the
// ray — is ever revealed from below; the surface behind it is occluded.
static bool sightBlockedByCliff(int x0, int y0, int x1, int y1, int viewerElev) {
    int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy, x = x0, y = y0;
    while (true) {
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x += sx; }
        if (e2 <= dx) { err += dx; y += sy; }
        if (x == x1 && y == y1) return false;          // reached target, clear line
        if (inBounds(x, y) && g.map[y][x].elev > viewerElev) return true;
    }
}

void updateFog() {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        for (int p = 0; p < MAX_PLAYERS; p++) g.map[y][x].visible[p] = false;
    int nightPen = isNight() ? 2 : (isDusk()||isDawn()) ? 1 : 0;
    if (getSeason() == WINTER) nightPen += 1; // blizzards eat sight
    // Weather always impedes sight, stacking with night and winter — a rainstorm
    // at night is genuinely dark. Storm is worse than rain/snow.
    if      (g.weather == W_STORM)                          nightPen += 2;
    else if (g.weather == W_RAIN || g.weather == W_SNOW)    nightPen += 1;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner >= OWNER_NATURE) continue;
        if (e.state == S_GARRISONED) continue;
        if (e.underConstruction) continue; // scaffold doesn't see
        int r = FOG_RADIUS - nightPen;
        if (isBuilding(e.type)) r += 2;
        if (e.type == E_TOWER)  r += 4;
        if (e.type == E_CASTLE) r += 3;
        if (e.type == E_CHURCH) r += 3;
        // A deployed trebuchet's spotters out-see every tower and castle —
        // it has to: its throw (range 12) reaches beyond their sight.
        if (e.type == E_TREBUCHET) r += (e.packed == 0 ? 8 : 2);
        // High ground: units on a plateau see further.
        if (!isBuilding(e.type) && g.map[e.y][e.x].elev > 0) r += 2;
        // Standing stones: a sentry on the old monolith sees the whole vale.
        if (!isBuilding(e.type) && g.map[e.y][e.x].terrain == T_MONOLITH) r += 6;
        if (r < 3) r = 3;
        auto& s = STATS[e.type];
        int cx = e.x + s.sizeW/2, cy = e.y + s.sizeH/2;
        int viewerElev = inBounds(cx,cy) ? g.map[cy][cx].elev : 0;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny) || dx*dx+dy*dy > r*r) continue;
            // Under a cliff looking up: higher ground hidden behind the cliff
            // edge stays fogged. Same/lower ground in radius is always seen, so
            // this only ever trims the plateau you can't actually see onto.
            if (g.map[ny][nx].elev > viewerElev
                && sightBlockedByCliff(cx, cy, nx, ny, viewerElev)) continue;
            g.map[ny][nx].visible[e.owner]  = true;
            g.map[ny][nx].explored[e.owner] = true;
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

// A unit whose morale has shattered drops its orders and bolts for the
// nearest friendly stronghold, deaf to commands until it rallies (1.1).
static void beginRout(Entity& e) {
    e.state = S_ROUTING; e.routTicks = 80;
    e.attackMove = 0; e.holdPosition = 0; e.retreating = 0; e.targetId = -1;
    Entity* haven = findSafeHaven(e);
    if (haven) {
        e.path = findPathFor(e, haven->x, haven->y); e.pathIdx = 0;
        e.targetX = haven->x; e.targetY = haven->y;
    } else { e.path.clear(); e.pathIdx = 0; }
    if (e.owner == g.localPlayer)      setStatus("Your men are breaking — rally them!");
    // Tally routs per side in a short window for the "broke their line" flash.
    if (e.owner < MAX_PLAYERS) {
        if (g.tick - g.routFlashTick > 20 || e.owner != g.routFlashOwner) {
            g.routFlashTick = g.tick; g.routFlashOwner = e.owner; g.routFlashCount = 0;
        }
        if (++g.routFlashCount == 3 && e.owner != g.localPlayer) setStatus("You broke their line!");
    }
}

// A broken man run down by the enemy is taken, not killed: he changes hands
// as an inert prisoner, to be ransomed or freed later (tickPrisoners) (3.3).
static void capture(Entity& e) {
    Entity* captor = nullptr;
    for (auto& o : g.entities) {
        if (!o.alive || o.owner == e.owner || o.owner >= OWNER_NATURE) continue;
        if (!isUnit(o.type) || isRanged(o.type) || o.prisoner) continue;
        if (dist(e.x, e.y, o.x, o.y) <= 1) { captor = &o; break; }
    }
    if (!captor) { e.captureTicks = 0; return; }
    int oldOwner = e.owner;
    e.prisoner = 1; e.origOwner = oldOwner; e.owner = captor->owner;
    e.state = S_IDLE; e.routTicks = 0; e.captureTicks = 0; e.morale = 0;
    e.path.clear(); e.pathIdx = 0; e.targetId = -1; e.holdPosition = 0;
    updateSupply(oldOwner); updateSupply(e.owner);
    if (captor->owner == g.localPlayer) setStatus("Prisoner taken!");
    else if (oldOwner == 0) setStatus("One of your soldiers was captured!");
}

// ============================================================
// ENTITY TICK
// ============================================================
// Is (x,y) a resource tile a peasant can work right now?
static bool isGatherTile(int x, int y) {
    if (!inBounds(x, y) || g.map[y][x].resources <= 0) return false;
    Terrain t = g.map[y][x].terrain;
    return t==T_GOLD || t==T_BERRY || t==T_FOREST || t==T_PINE || t==T_PALM || t==T_DEAD_TREE;
}

// Send an idle builder to the nearest matching resource within `rad` of a
// just-finished camp/mill (wantType: 0 gold, 1 wood, 3 berries). "In the
// vicinity" only — we don't march them across the map.
static bool autoHarvestNear(Entity& e, int ox, int oy, int wantType, int rad) {
    int bestD = rad + 1, bx = -1, by = -1;
    for (int dy = -rad; dy <= rad; dy++) for (int dx = -rad; dx <= rad; dx++) {
        int x = ox+dx, y = oy+dy;
        if (!inBounds(x, y) || g.map[y][x].resources <= 0) continue;
        Terrain t = g.map[y][x].terrain;
        bool match = (wantType==0 && t==T_GOLD)
                  || (wantType==1 && (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE))
                  || (wantType==3 && t==T_BERRY);
        if (!match) continue;
        int d = dist(e.x, e.y, x, y);
        if (d < bestD) { bestD = d; bx = x; by = y; }
    }
    if (bx < 0) return false;
    orderGather(e, bx, by);
    return e.state == S_GATHERING;
}

void tickEntity(Entity& e) {
    if (!e.alive) return;
    if (e.alertTicks > 0) e.alertTicks--;
    if (e.aleTicks > 0) e.aleTicks--;
    // Tavern feast cooldown (atkCd is unused on non-attacking buildings).
    if (e.type == E_TAVERN && e.atkCd > 0) e.atkCd--;
    // Prisoners are inert: they only shuffle along the march path tickPrisoners
    // set for them (toward the captor's hold), and otherwise do nothing.
    if (e.prisoner) {
        if (e.state == S_MOVING) moveAlongPath(e);
        return;
    }
    // --- Combat feel: morale, stamina, entrenchment (docs/combat-feel) ---
    if (isUnit(e.type) && e.owner < OWNER_NATURE) {
        // Catapults entrench by standing still; rolling resets the dig-in.
        if (e.type == E_CATAPULT) {
            if (e.state == S_MOVING) e.entrenchTicks = 0;
            else if (e.entrenchTicks < 100000) e.entrenchTicks++;
        }
        // Stamina recovers at rest; it's spent moving (moveAlongPath) and fighting.
        if ((e.state == S_IDLE || e.state == S_GARRISONED) && e.alertTicks == 0
                && (g.tick + e.id) % 4 == 0)
            e.stamina = std::min(100, e.stamina + 1);
        if (hasMorale(e.type)) {
            // Morale recovers out of combat — faster near a town/castle or a banner.
            if (e.state != S_ROUTING && (g.tick + e.id) % 8 == 0) {
                int regen = (e.alertTicks == 0) ? 2 : 0;
                for (auto& o : g.entities) {
                    if (!o.alive || o.owner != e.owner) continue;
                    if ((o.type==E_TOWNHALL||o.type==E_CASTLE) && !o.underConstruction
                            && dist(e.x,e.y,o.x,o.y) <= 8) { regen += 2; break; }
                }
                for (auto& o : g.entities) {   // a veteran banner steadies the line
                    if (!o.alive || o.owner != e.owner || o.type != E_MILITIA || o.kills < 3) continue;
                    if (o.state == S_GARRISONED) continue;
                    if (dist(e.x,e.y,o.x,o.y) <= 3) { regen += 2; break; }
                }
                if (regen) e.morale = std::min(100, e.morale + regen);
            }
            // Outnumbered locally in a fight: nerves fray.
            if (e.alertTicks > 0 && (g.tick + e.id) % 10 == 0) {
                int friends = 0, foes = 0;
                for (auto& o : g.entities) {
                    if (!o.alive || !isUnit(o.type) || o.state == S_GARRISONED) continue;
                    if (dist(e.x,e.y,o.x,o.y) > 5) continue;
                    if (o.owner == e.owner) friends++;
                    else if (o.owner < OWNER_NATURE) foes++;
                }
                if (foes > friends + 1) e.morale = std::max(0, e.morale - (foes-friends));
            }
            // Broken: drop everything and run.
            if (e.morale <= 0 && e.state != S_ROUTING) beginRout(e);
        }
    }
    // Pop the next queued waypoint when the unit goes idle.
    // Patrol mode rotates the waypoint to the back of the queue so the unit loops.
    if (e.state == S_IDLE && !e.waypoints.empty() && isUnit(e.type) && !isNaval(e.type)
            && e.holdPosition == 0 && e.retreating == 0) {
        auto wp = e.waypoints.front();
        e.waypoints.erase(e.waypoints.begin());
        if (e.patrolMode) e.waypoints.push_back(wp);
        // Command queue: a queued waypoint that lands on a resource is a
        // GATHER order (e.g. "build here, then go mine that gold"); patrol
        // waypoints always just move.
        if (e.type == E_PEASANT && !e.patrolMode && isGatherTile(wp.first, wp.second))
            orderGather(e, wp.first, wp.second);
        else
            orderMove(e, wp.first, wp.second);
    }
    // Building production
    if (e.producing != E_NONE && !e.underConstruction) {
        int bonus = 0;
        for (auto& o : g.entities)
            if (o.alive && o.owner==e.owner && o.type==E_BLACKSMITH && !o.underConstruction) { bonus=1; break; }
        e.prodProgress += 1 + bonus;
        if (e.prodProgress >= e.prodTime) {
          // Pop-cap muster gate: a finished unit waits at the threshold until a
          // house raises the supply ceiling — same hold-and-retry idea as the
          // "no free tile to spawn on" case just below.
          Player& sp = g.players[e.owner];
          if (sp.supply + STATS[e.producing].supplyUsed > sp.supplyMax) {
            e.prodProgress = e.prodTime;
            if (e.owner == g.localPlayer && (g.tick + e.id) % 100 == 0)
                setStatus("Trained unit waiting on supply — build more houses!");
          } else {
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
                // Send to rally point if the building has a player-set one.
                // A rally placed on a resource makes new peasants go HARVEST it
                // (assign workers straight from the Town Hall).
                if (e.rallySet && newId >= 0) {
                    Entity* nu = findEntity(newId);
                    if (nu) {
                        if (nu->type == E_PEASANT && isGatherTile(e.rallyX, e.rallyY))
                            orderGather(*nu, e.rallyX, e.rallyY);
                        else
                            orderMove(*nu, e.rallyX, e.rallyY);
                    }
                }
                EntityType justTrained = e.producing;
                e.producing = E_NONE; e.state = S_IDLE;
                if (e.owner==g.localPlayer) setStatus(std::string(STATS[justTrained].name) + " is ready.");
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
    }
    // Research progress (Blacksmith). Independent of unit production.
    if (e.researching != 0 && !e.underConstruction) {
        e.prodProgress += 1;
        if (e.prodProgress >= e.prodTime) {
            int bit = e.researching;
            e.researching = 0; e.prodProgress = 0; e.prodTime = 0;
            if (bit == R_ERA_ADVANCE) {
                // The bells ring: a new era. Everyone on the map hears of it.
                Player& p = g.players[e.owner];
                if (p.era < ERA_STRONGHOLD) p.era++;
                if (e.owner == g.localPlayer)
                    setStatus(std::string("You enter the ") + eraName(p.era) + " era!");
                else
                    setStatus(std::string("The ") + CIVS[p.civ].name + " (P" + std::to_string(e.owner + 1)
                              + ") enter the " + eraName(p.era) + " era!");
            } else {
                g.players[e.owner].research |= bit;
                if (e.owner == g.localPlayer) {
                    int n = 0; const ResearchDef* tbl = researchTable(n);
                    for (int i = 0; i < n; i++)
                        if (tbl[i].bit == bit)
                            setStatus(std::string(tbl[i].name) + " researched — " + tbl[i].effect + "!");
                }
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
                if (e.owner==g.localPlayer) setStatus(std::string(STATS[e.type].name) + " complete!");
                // A finished bridge becomes terrain: the scaffold entity goes
                // away and the tile itself turns into a fast, land-passable
                // span (T_BRIDGE survives winter — it isn't water anymore).
                if (e.type == E_BRIDGE) {
                    g.map[e.y][e.x].terrain = T_BRIDGE;
                    g.map[e.y][e.x].preWinterTerrain = T_BRIDGE;
                    g.map[e.y][e.x].resources = 0;
                    e.alive = false; e.state = S_DEAD;
                    return;
                }
                // Resource depots send their builder straight to work the
                // resource they were raised to serve (if any is in the vicinity).
                int harvestType = (e.type==E_LUMBER_CAMP) ? 1
                                : (e.type==E_MINING_CAMP) ? 0
                                : (e.type==E_MILL)        ? 3 : -1;
                for (auto& o : g.entities) {
                    if (!o.alive || o.state!=S_BUILDING || o.targetId!=e.id) continue;
                    // For farms: keep tending — S_BUILDING handler routes to its farm branch.
                    if (e.type == E_FARM) continue;
                    if (harvestType >= 0 && autoHarvestNear(o, e.x, e.y, harvestType, 12)) continue;
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
                if (e.owner == g.localPlayer)
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
        // Anti-bunching: idle units don't linger stacked on one tile. The
        // lowest-id occupant keeps the spot; everyone else steps to a free
        // neighbouring tile. Throttled and deterministic (pure sim state).
        if (e.owner < OWNER_NATURE && !e.holdPosition
            && !(e.type == E_TREBUCHET && e.packed == 0)
            && (g.tick + e.id) % 3 == 0) {
            bool mustYield = false;
            for (auto& o : g.entities) {
                if (!o.alive || o.id == e.id || o.x != e.x || o.y != e.y) continue;
                if (!isUnit(o.type) || o.state == S_GARRISONED) continue;
                if (isNaval(o.type) != isNaval(e.type)) continue;
                if (o.state == S_IDLE && o.id < e.id) { mustYield = true; break; }
            }
            if (mustYield) {
                static const int dx8s[] = {1,-1,0,0,1,1,-1,-1};
                static const int dy8s[] = {0,0,1,-1,1,-1,1,-1};
                bool nav = isNaval(e.type);
                for (int i = 0; i < 8; i++) {
                    int nx = e.x + dx8s[i], ny = e.y + dy8s[i];
                    if (!inBounds(nx, ny)) continue;
                    bool ok = nav ? isPassableWater(nx, ny) : isPassable(nx, ny);
                    if (!ok || !canStep(e.x, e.y, nx, ny, nav) || entityAt(nx, ny)) continue;
                    e.state = S_MOVING; e.targetX = nx; e.targetY = ny;
                    e.path.clear(); e.path.push_back({nx, ny}); e.pathIdx = 0;
                    break;
                }
                if (e.state == S_MOVING) break;
            }
        }
        // Plunder: an idle land unit standing on or beside scattered loot
        // scoops a load and hauls it to its own nearest depot. Raiders ride
        // home laden; kill the carrier and the loot drops again.
        if (e.owner < OWNER_NATURE && !isNaval(e.type) && e.carrying == 0
            && e.type != E_TREBUCHET && (g.tick + e.id) % 5 == 0) {
            static const int d5x[] = {0,1,-1,0,0}, d5y[] = {0,0,0,1,-1};
            for (int i = 0; i < 5; i++) {
                int lx = e.x + d5x[i], ly = e.y + d5y[i];
                if (!inBounds(lx, ly)) continue;
                Tile& lt = g.map[ly][lx];
                int take = 0;
                if      (lt.lootGold > 0) { take = std::min(CARRY_MAX, lt.lootGold); lt.lootGold -= take; e.gatherType = 0; }
                else if (lt.lootWood > 0) { take = std::min(CARRY_MAX, lt.lootWood); lt.lootWood -= take; e.gatherType = 1; }
                else if (lt.lootFood > 0) { take = std::min(CARRY_MAX, lt.lootFood); lt.lootFood -= take; e.gatherType = 3; e.foodKind = F_GRAIN; }
                if (take == 0) continue;
                e.carrying = take;
                e.rallyX = -1; e.rallyY = -1;   // no auto-resume after dropoff
                Entity* dep = findDepot(e);
                if (dep) {
                    e.state = S_RETURNING; e.targetId = dep->id;
                    e.targetX = dep->x; e.targetY = dep->y;
                    e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                    if (e.owner == g.localPlayer) setStatus("Plunder! Hauling stolen stores home.");
                }
                break;
            }
            if (e.state != S_IDLE) break;
        }
        // Village life: on dark evenings idle peasants drift toward the
        // tavern's light. Pure flavour — they're doing nothing anyway.
        if (e.type == E_PEASANT && isNight() && (g.tick + e.id) % 60 == 0) {
            for (auto& t : g.entities) {
                if (!t.alive || t.owner != e.owner || t.type != E_TAVERN || t.underConstruction) continue;
                int d = dist(e.x, e.y, t.x, t.y);
                if (d > 3 && d <= 24) { orderMove(e, t.x, t.y); break; }
            }
            if (e.state != S_IDLE) break;
        }
        // Monks mend: an idle monk heals the most-wounded adjacent friendly
        // unit 1 hp every 8 ticks. Focus the monk to stop the mending.
        if (e.type == E_MONK && e.owner < OWNER_NATURE && (g.tick + e.id) % 8 == 0) {
            Entity* worst = nullptr;
            for (auto& o : g.entities) {
                if (!o.alive || o.owner != e.owner || o.id == e.id) continue;
                if (!isUnit(o.type) || o.state == S_GARRISONED) continue;
                if (o.hp >= o.maxHp || dist(e.x, e.y, o.x, o.y) > 1) continue;
                if (!worst || o.hp * worst->maxHp < worst->hp * o.maxHp) worst = &o;
            }
            if (worst) worst->hp = std::min(worst->maxHp, worst->hp + 1);
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
    case S_ROUTING: {
        if (e.routTicks > 0) e.routTicks--;
        // Cornered while broken: an adjacent enemy footman can take him captive.
        bool cornered = false;
        for (auto& o : g.entities) {
            if (!o.alive || o.owner == e.owner || o.owner >= OWNER_NATURE) continue;
            if (!isUnit(o.type) || isRanged(o.type) || o.prisoner) continue;
            if (dist(e.x,e.y,o.x,o.y) <= 1) { cornered = true; break; }
        }
        if (cornered) {
            if (++e.captureTicks >= 40) { capture(e); break; }
        } else if (e.captureTicks > 0) e.captureTicks--;
        // Rally on reaching safety or once the panic passes.
        Entity* haven = findSafeHaven(e);
        bool safe = haven && dist(e.x,e.y,haven->x,haven->y) <= 3;
        if (e.routTicks == 0 || safe) {
            e.state = S_IDLE; e.morale = std::max(e.morale, 40);
            e.captureTicks = 0; e.path.clear(); e.pathIdx = 0;
            break;
        }
        if (e.path.empty() || e.pathIdx >= (int)e.path.size()) {
            if (haven) { e.path = findPathFor(e, haven->x, haven->y); e.pathIdx = 0; }
        }
        moveAlongPath(e);   // routers get +1 speed inside moveAlongPath
        break;
    }
    case S_ATTACKING: {
        Entity* t = findEntity(e.targetId);
        if (!t || !t->alive) { e.state = S_IDLE; break; }
        // Target ducked into a building — they're untouchable, go idle. Without
        // this the attacker keeps swinging at the garrisoned position and the
        // target dies inside the safe building.
        if (t->state == S_GARRISONED) { e.state = S_IDLE; e.targetId = -1; break; }
        int d = dist(e.x, e.y, t->x, t->y);
        // Sapper at the wall: light the fuse. 120 Crush to the building, a
        // 30-damage blast to anything adjacent (friend or foe), sapper dies.
        if (e.type == E_SAPPER && d <= 1) {
            t->hp -= shieldBuilding(*t, 120);
            int blastX = e.x, blastY = e.y;
            for (auto& o : g.entities) {
                if (!o.alive || o.id == e.id || o.id == t->id) continue;
                auto& os = STATS[o.type];
                int ox = std::max(o.x, std::min(blastX, o.x + os.sizeW - 1));
                int oy = std::max(o.y, std::min(blastY, o.y + os.sizeH - 1));
                if (std::abs(ox - blastX) <= 1 && std::abs(oy - blastY) <= 1) {
                    o.hp -= shieldBuilding(o, damageVs(E_SAPPER, o.type, 30, o.owner));
                    o.alertTicks = 12;
                    if (o.hp <= 0) killEntity(o);
                }
            }
            if (t->owner == g.localPlayer && g.attackNotifyCd == 0) {
                setStatus("A sapper charge rocks your walls!");
                g.attackNotifyCd = 200;
            }
            killEntity(e);
            if (t->hp <= 0 && t->alive) killEntity(*t);
            break;
        }
        // Catapults need standoff — too close to arm the sling properly.
        if (e.type == E_CATAPULT && d < 2) { e.state = S_IDLE; break; }
        if (d <= unitRange(e)) {
            if (e.atkCd <= 0) {
                int rawDmg = unitAtk(e);
                int dmg = damageVs(e.type, t->type, rawDmg, t->owner);
                // High ground: ranged fire strikes harder downhill (+50%) and
                // loses force shooting up a cliff face (-25%). Melee unaffected.
                if (isRanged(e.type)) {
                    int de = g.map[e.y][e.x].elev - g.map[t->y][t->x].elev;
                    if      (de > 0) dmg = dmg * 3 / 2;
                    else if (de < 0) dmg = std::max(1, dmg * 3 / 4);
                }
                // Cavalry charge: several unbroken strides into the foe land a
                // crushing first blow — but a braced spearman eats the charge,
                // and buildings don't flinch.
                bool charged = false;
                if ((e.type==E_KNIGHT || e.type==E_HUSSAR) && e.chargeSteps >= 4
                        && t->type != E_SPEARMAN && !isBuilding(t->type)) {
                    dmg *= 2; charged = true;
                }
                // Winded attackers hit softer (stamina also bites in unitAtk).
                dmg = shieldBuilding(*t, dmg);
                t->hp -= dmg;
                e.atkCd = STATS[e.type].atkSpeed * COMBAT_PACE / 100;
                e.chargeSteps = 0;   // the charge (if any) is spent on this blow
                if (hasMorale(e.type)) e.stamina = std::max(0, e.stamina - 2);
                e.alertTicks = 12; t->alertTicks = 12;
                // A wound shakes resolve; a charge shakes it harder.
                if (hasMorale(t->type)) {
                    int md = 2 + dmg * 15 / std::max(1, t->maxHp);
                    if (charged) md += 10;
                    t->morale = std::max(0, t->morale - md);
                }
                // Charge knockback + stun: shove the target back a free tile.
                if (charged) {
                    t->atkCd = std::max(t->atkCd, STATS[t->type].atkSpeed * COMBAT_PACE / 100);
                    int kx = t->x + ((t->x>e.x)-(t->x<e.x));
                    int ky = t->y + ((t->y>e.y)-(t->y<e.y));
                    if (inBounds(kx,ky) && isPassable(kx,ky) && !entityAt(kx,ky)
                            && canStep(t->x,t->y,kx,ky,false)) {
                        t->x = kx; t->y = ky; t->path.clear(); t->pathIdx = 0;
                    }
                }
                if (t->owner == g.localPlayer && g.attackNotifyCd == 0 && t->type != E_NONE) {
                    setStatus("Your people are under attack!");
                    g.attackNotifyCd = 200;
                }
                if (isRanged(e.type)) {
                    // Siege engines hurl boulders; everything else looses bolts/arrows.
                    char pc = (e.type==E_CATAPULT) ? 'o'
                            : (e.type==E_TREBUCHET) ? 'O' : '-';
                    int pcol = (e.type==E_CATAPULT || e.type==E_TREBUCHET)
                             ? CP_PROJ_BOULDER : CP_PROJ_ARROW;
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
                            int splashDmg = shieldBuilding(o, damageVs(E_CATAPULT, o.type, splashRaw, o.owner));
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
                        e.carrying = food; e.gatherType = 3; e.foodKind = F_MEAT;
                        e.rallyX = -1; e.rallyY = -1; // sentinel: don't auto-resume after dropoff
                        Entity* dep = findDepot(e);
                        if (dep) {
                            e.state = S_RETURNING;
                            e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
                            e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                            if (e.owner==g.localPlayer) setStatus(std::string("Hunted! Hauling ") + std::to_string(food) + " food.");
                        } else {
                            e.carrying = 0; e.state = S_IDLE;
                            if (e.owner==g.localPlayer) setStatus("Killed game but no Mill/TC nearby — meat wasted.");
                        }
                    } else {
                        // Non-peasant kills (e.g. militia defending base) waste the carcass.
                        e.state = S_IDLE;
                    }
                    // Felling an enemy soldier bloods the attacker — 3 kills
                    // make a militia a veteran banner (hasMorale / unitAtk).
                    if (isUnit(t->type) && t->owner < OWNER_NATURE && hasMorale(t->type)
                            && e.owner < OWNER_NATURE)
                        e.kills++;
                    killEntity(*t);
                }
            } else e.atkCd--;
        } else {
            // A deployed trebuchet is anchored — it cannot walk after a target
            // that left its throw. Stand down; pack (D) to reposition.
            if (e.type == E_TREBUCHET) {
                e.state = S_IDLE; e.targetId = -1;
                if (e.owner == g.localPlayer) setStatus("Target out of range — pack the trebuchet (D) to move.");
                break;
            }
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
                    int rate = GATHER_RATE;
                    if (e.owner < MAX_PLAYERS) {
                        int civ = g.players[e.owner].civ;
                        if (civ == CIV_HILLFOLK   && tile.terrain == T_GOLD) rate = rate * 5 / 4;
                        if (civ == CIV_FENLANDERS && isFishT)                rate = rate * 5 / 4;
                    }
                    int amt = std::min(rate, tile.resources);
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
    case S_RAIDING: {
        Entity* yard = findEntity(e.targetId);
        if (!yard || !yard->alive || yard->owner == e.owner || yard->underConstruction) {
            e.state = S_IDLE; break;
        }
        int edible = 0;
        for (int k = 0; k < F_COUNT; k++) if (k != F_ALE) edible += yard->storeFood[k];
        if (yard->storeGold + yard->storeWood + edible <= 0) { e.state = S_IDLE; break; }
        int d = dist(e.x, e.y, yard->x, yard->y);
        if (getenv("REALM_AI_DEBUG") && (g.tick % 50 == 0))
            fprintf(stderr, "[raid] unit %d at (%d,%d) yard (%d,%d) d=%d\n", e.id, e.x, e.y, yard->x, yard->y, d);
        if (d <= STATS[yard->type].sizeW) {
            // At the piles: grab the tallest one and leg it. The victim's
            // realm totals shrink NOW — the goods only join the raider's
            // treasury if the courier run home survives (see S_RETURNING;
            // a dead raider scatters it as loot like any other courier).
            Player& victim = g.players[yard->owner];
            const int RAID_CARRY = 30;
            int kind = 0, amt = yard->storeGold;              // 0 gold, 1 wood, 2 food
            if (yard->storeWood > amt) { kind = 1; amt = yard->storeWood; }
            if (edible          > amt) { kind = 2; amt = edible; }
            int take = std::min(RAID_CARRY, amt);
            if (kind == 0)      { yard->storeGold -= take; victim.gold -= std::min(take, victim.gold); e.gatherType = 0; }
            else if (kind == 1) { yard->storeWood -= take; victim.wood -= std::min(take, victim.wood); e.gatherType = 1; }
            else {
                int left = take;
                for (int k = 0; k < F_COUNT && left > 0; k++) {
                    if (k == F_ALE) continue;
                    int c = std::min(left, yard->storeFood[k]);
                    yard->storeFood[k] -= c; left -= c;
                }
                victim.food -= std::min(take, victim.food);
                e.gatherType = 3; e.foodKind = F_GRAIN;
            }
            e.carrying = take;
            if (e.owner <= MAX_PLAYERS) g.statRaids[e.owner]++;
            e.rallyX = -1; e.rallyY = -1;                     // no auto-resume after dropoff
            e.state = S_RETURNING; e.targetId = -1;           // S_RETURNING finds our depot
            if (yard->owner == g.localPlayer && g.attackNotifyCd == 0) {
                setStatus("Raiders are plundering your Stockyard!");
                g.attackNotifyCd = 100;
            } else if (e.owner == g.localPlayer) {
                setStatus(std::string("Plundered ") + std::to_string(take)
                          + (kind==0 ? " gold" : kind==1 ? " wood" : " food") + " — run for home!");
            }
        } else {
            moveAlongPath(e);
            if (e.path.empty() && (g.tick + e.id) % 10 == 0) {
                e.path = findPathFor(e, yard->x, yard->y); e.pathIdx = 0;
                if (e.path.empty()) e.state = S_IDLE;
            }
        }
        break;
    }
    case S_RETURNING: {
        // Wagon haul: load at a depot when empty, unload when laden. Cargo
        // leaves the realm's totals while on the road — a killed wagon
        // scatters it as loot (see killEntity), an ambushed supply line.
        if (e.type == E_WAGON) {
            Entity* dep = findEntity(e.targetId);
            if (!dep || !dep->alive || dep->underConstruction || !isBuilding(dep->type)
                || dep->owner != e.owner) { e.state = S_IDLE; break; }
            int d = dist(e.x, e.y, dep->x, dep->y);
            if (d <= STATS[dep->type].sizeW + 1) {
                Player& p = g.players[e.owner];
                if (e.carrying == 0) {
                    // Load the largest pile at this depot (ale travels too).
                    int bestAmt = dep->storeGold, bestKind = -2;     // -2 gold, -1 wood, >=0 food kind
                    if (dep->storeWood > bestAmt) { bestAmt = dep->storeWood; bestKind = -1; }
                    for (int k = 0; k < F_COUNT; k++)
                        if (dep->storeFood[k] > bestAmt) { bestAmt = dep->storeFood[k]; bestKind = k; }
                    int take = std::min(bestAmt, WAGON_CAP);
                    if (take > 0) {
                        if (bestKind == -2)      { dep->storeGold -= take; p.gold -= std::min(take, p.gold); e.gatherType = 0; }
                        else if (bestKind == -1) { dep->storeWood -= take; p.wood -= std::min(take, p.wood); e.gatherType = 1; }
                        else {
                            dep->storeFood[bestKind] -= take;
                            if (bestKind != F_ALE) p.food -= std::min(take, p.food);
                            e.gatherType = 3; e.foodKind = bestKind;
                        }
                        e.carrying = take;
                        if (e.owner == g.localPlayer) setStatus("Wagon loaded — right-click another depot to deliver.");
                    } else if (e.owner == g.localPlayer) setStatus("Nothing stored here to load.");
                } else {
                    if      (e.gatherType == 0) { p.gold += e.carrying; dep->storeGold += e.carrying; }
                    else if (e.gatherType == 1) { p.wood += e.carrying; dep->storeWood += e.carrying; }
                    else                        addFood(e.owner, e.foodKind, e.carrying, dep);
                    e.carrying = 0;
                    if (e.owner == g.localPlayer) setStatus("Wagon unloaded.");
                }
                e.state = S_IDLE;
            } else {
                moveAlongPath(e);
                if (e.path.empty() && (g.tick + e.id) % 10 == 0) {
                    e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
                }
            }
            break;
        }
        Entity* dep = findEntity(e.targetId);
        if (!dep || !dep->alive) {
            dep = findDepot(e);
            if (!dep) { e.state = S_IDLE; break; }
            e.targetId = dep->id; e.targetX = dep->x; e.targetY = dep->y;
            e.path = findPathFor(e, dep->x, dep->y); e.pathIdx = 0;
        }
        int d = dist(e.x, e.y, dep->x, dep->y);
        if (d <= STATS[dep->type].sizeW + 1) {
            if      (e.gatherType == 0) { g.players[e.owner].gold += e.carrying; dep->storeGold += e.carrying; }
            else if (e.gatherType == 1) { g.players[e.owner].wood += e.carrying; dep->storeWood += e.carrying; }
            else if (e.gatherType == 3) addFood(e.owner, e.foodKind, e.carrying, dep); // farm/berry/hunt
            else                        addFood(e.owner, F_FISH, e.carrying, dep);
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
        // Claimable neutrals (ruins, watermills, posts, shrines) accept anyone
        // while neutral or already theirs. Everything else is own-only.
        bool ruinOk = bld && isClaimable(bld->type)
                   && (bld->owner == OWNER_NATURE || bld->owner == e.owner);
        if (!bld || !bld->alive || bld->underConstruction || !canGarrisonIn(bld->type)
            || (bld->owner != e.owner && !ruinOk)) {
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
                if (e.owner == g.localPlayer) setStatus(std::string("Garrisoned in ") + STATS[bld->type].name);
                // First unit into a neutral claimable claims it; it reverts
                // when the last one leaves.
                if (isClaimable(bld->type) && bld->owner != e.owner) {
                    bld->owner = e.owner;
                    if (e.owner == g.localPlayer) setStatus(std::string(STATS[bld->type].name) + " claimed — your banner flies here.");
                }
            } else {
                if (e.owner == g.localPlayer) setStatus(std::string(STATS[bld->type].name) + " is full");
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
                e.gatherType = 3; e.foodKind = F_GRAIN; // wheat harvest
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
                if (e.path.empty() && (g.tick + e.id) % 10 == 0) {
                    e.path = findPathFor(e, bld->x, bld->y); e.pathIdx = 0;
                }
            }
            break;
        }
        if (!bld->underConstruction) {
            // The building just finished: send the builder straight to work if
            // it raised a resource depot near a matching resource — lumber camp
            // → wood, mining camp → gold, mill → nearby berries.
            int bt = bld->type, ox = bld->x, oy = bld->y;
            bool tasked = (bt==E_LUMBER_CAMP) ? autoHarvestNear(e, ox, oy, 1, 12)
                        : (bt==E_MINING_CAMP) ? autoHarvestNear(e, ox, oy, 0, 12)
                        : (bt==E_MILL)        ? autoHarvestNear(e, ox, oy, 3, 12)
                        : false;
            if (!tasked) e.state = S_IDLE;
            break;
        }
        int bx2 = bld->x + STATS[bld->type].sizeW - 1, by2 = bld->y + STATS[bld->type].sizeH - 1;
        int cx = std::max(bld->x, std::min(e.x, bx2));
        int cy = std::max(bld->y, std::min(e.y, by2));
        if (dist(e.x, e.y, cx, cy) > 1) {
            moveAlongPath(e);
            // Re-path at most every 10 ticks (staggered by id): an unreachable
            // site used to trigger a full A* every tick per stuck builder —
            // pure CPU burn that never went anywhere.
            if (e.path.empty() && (g.tick + e.id) % 10 == 0) {
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

