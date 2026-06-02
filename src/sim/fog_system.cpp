#include "realm.h"

bool isConcealing() { return isConcealing(g); }

bool isConcealing(const Game& game) { return isNight(game) || game.weather == W_STORM; }

static bool detectMap[MAX_PLAYERS][MAP_H][MAP_W];
static int  detectMapTick[MAX_PLAYERS] = {-1,-1,-1,-1};

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
    if (!isConcealing() && !isConcealingTile(x, y)) return true;
    if (observerOwner < 0 || observerOwner >= MAX_PLAYERS) return true;
    if (!inBounds(x, y)) return false;
    ensureDetectMap(observerOwner);
    return detectMap[observerOwner][y][x];
}

bool isDetectedBy(const Game& game, int x, int y, int observerOwner) {
    if (!isConcealing(game) && !isConcealingTile(game, x, y)) return true;
    if (observerOwner < 0 || observerOwner >= MAX_PLAYERS) return true;
    if (!inBounds(x, y)) return false;
    for (const auto& e : game.entities) {
        if (!e.alive || e.owner != observerOwner || e.state == S_GARRISONED) continue;
        if (e.underConstruction) continue;
        bool torch = (e.type == E_TOWER || e.type == E_CASTLE
                  || e.type == E_CHURCH || e.type == E_TOWNHALL);
        int range = torch ? 7 : 3;
        auto& s = STATS[e.type];
        int cx = e.x + s.sizeW/2, cy = e.y + s.sizeH/2;
        int dx = x - cx, dy = y - cy;
        if (dx*dx + dy*dy <= range*range) return true;
    }
    return false;
}

void updateFog() {
    updateFog(g);
}

void updateFog(Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        for (int p = 0; p < MAX_PLAYERS; p++) game.map[y][x].visible[p] = false;
    int nightPen = isNight(game) ? 2 : (isDusk(game)||isDawn(game)) ? 1 : 0;
    if (getSeason(game) == WINTER) nightPen += 1; // blizzards eat sight
    if (game.weather == W_STORM) nightPen += 1;
    else if (game.weather == W_RAIN || game.weather == W_SNOW) nightPen += (nightPen > 0 ? 0 : 1);
    for (auto& e : game.entities) {
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
                game.map[ny][nx].visible[e.owner]  = true;
                game.map[ny][nx].explored[e.owner] = true;
            }
        }
    }
}
