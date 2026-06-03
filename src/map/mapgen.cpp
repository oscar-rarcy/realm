#include "realm.h"

#include <iostream>
#include <queue>
#include <utility>

MapNoise initMapNoise(Game& game) {
    MapNoise noise;
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            noise.samples[y][x] = (float)(realmRand(game) % 1000) / 1000.0f;
    return noise;
}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

float sampleNoise(const MapNoise& noise, float fx, float fy) {
    int x0 = (int)fx % 31, y0 = (int)fy % 31, x1 = x0 + 1, y1 = y0 + 1;
    float tx = fx - (int)fx, ty = fy - (int)fy;
    return lerp(lerp(noise.samples[y0][x0], noise.samples[y0][x1], tx),
                lerp(noise.samples[y1][x0], noise.samples[y1][x1], tx), ty);
}

void clearStartArea(Game& game, int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius+4; dy++) for (int dx = -radius; dx <= radius+4; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y) && game.map[y][x].terrain != T_GOLD) {
            game.map[y][x].terrain = T_GRASS;
            game.map[y][x].resources = 0;
            game.map[y][x].preWinterTerrain = T_GRASS;
        }
    }
}

void placeGoldCluster(Game& game, int cx, int cy, int count) {
    for (int i = 0; i < count; i++) {
        int gx = cx + (realmRand(game)%7)-3, gy = cy + (realmRand(game)%5)-2;
        if (inBounds(gx,gy) && game.map[gy][gx].terrain != T_WATER
            && game.map[gy][gx].terrain != T_MOUNTAIN && game.map[gy][gx].terrain != T_SHALLOWS) {
            game.map[gy][gx].terrain = T_GOLD;
            game.map[gy][gx].resources = 300 + realmRand(game) % 300;
            game.map[gy][gx].preWinterTerrain = T_GOLD;
        }
    }
}

void placeCastleRuin(Game& game, int cx, int cy, int size) {
    for (int dy = 0; dy < size; dy++) for (int dx = 0; dx < size; dx++) {
        int x = cx + dx, y = cy + dy;
        if (!inBounds(x, y)) continue;
        bool isEdge   = (dx == 0 || dx == size-1 || dy == 0 || dy == size-1);
        bool isCorner = (dx == 0 || dx == size-1) && (dy == 0 || dy == size-1);
        bool isGate   = !isCorner && isEdge && (dx == size/2 || dy == size/2);
        if (isGate)       game.map[y][x].terrain = T_CASTLE_GATE;
        else if (isEdge)  game.map[y][x].terrain = (realmRand(game) % 4 != 0) ? T_CASTLE_WALL : T_RUINS;
        else              game.map[y][x].terrain = T_CASTLE_FLOOR;
        game.map[y][x].resources = 0;
    }
    int corners[][2] = {{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c : corners)
        if (inBounds(c[0], c[1])) game.map[c[1]][c[0]].terrain = T_CASTLE_WALL;
}

static float edist(int x1, int y1, int x2, int y2) {
    int dx = x1-x2, dy = y1-y2;
    return std::sqrt((float)(dx*dx + dy*dy));
}

static bool mapLandPassable(const Game& game, int x, int y) {
    if (!inBounds(x, y)) return false;
    return terrainDef(game.map[y][x].terrain).passableLand;
}

static bool validStartingFoundationTerrain(Terrain terrain) {
    return terrainDef(terrain).buildable;
}

static bool hasReachableStartingResources(const Game& game, int sx, int sy) {
    bool seen[MAP_H][MAP_W] = {};
    std::queue<std::pair<int, int>> open;
    if (!mapLandPassable(game, sx, sy)) return false;
    seen[sy][sx] = true;
    open.push({ sx, sy });
    bool wood = false, food = false, gold = false;
    const int dx[4] = { 1, -1, 0, 0 };
    const int dy[4] = { 0, 0, 1, -1 };
    while (!open.empty()) {
        auto [x, y] = open.front();
        open.pop();
        const Tile& tile = game.map[y][x];
        if (tile.resources > 0) {
            CargoResource resource = resourceForTerrain(tile.terrain);
            wood = wood || resource == CR_WOOD;
            food = food || resource == CR_FOOD;
            gold = gold || resource == CR_GOLD;
        }
        if (wood && food && gold) return true;
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!inBounds(nx, ny) || seen[ny][nx] || !mapLandPassable(game, nx, ny)) continue;
            seen[ny][nx] = true;
            open.push({ nx, ny });
        }
    }
    return false;
}

static bool hasViableDockShoreline(const Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain water = game.map[y][x].terrain;
        if (!terrainDef(water).passableWater) continue;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            if (dx == 0 && dy == 0) continue;
            int nx = x + dx, ny = y + dy;
            if (mapLandPassable(game, nx, ny) && validStartingFoundationTerrain(game.map[ny][nx].terrain))
                return true;
        }
    }
    return false;
}

bool validateMapInvariants(const Game& game, std::string* error) {
    auto fail = [&](const std::string& message) {
        if (error) *error = message;
        return false;
    };
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        const Tile& tile = game.map[y][x];
        if (tile.terrain < T_GRASS || tile.terrain > T_CASTLE_GATE)
            return fail("map tile terrain outside valid range");
        if (tile.preWinterTerrain < T_GRASS || tile.preWinterTerrain > T_CASTLE_GATE)
            return fail("map tile pre-winter terrain outside valid range");
        if (tile.biome < B_TEMPERATE || tile.biome > B_OCEAN)
            return fail("map tile biome outside valid range");
        if (tile.resources < 0)
            return fail("map tile resources below zero");
        if (tile.resources > 0 && resourceForTerrain(tile.terrain) == CR_NONE)
            return fail("map tile has resources on non-resource terrain");
    }
    int waterishTiles = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain terrain = game.map[y][x].terrain;
        if (terrainDef(terrain).passableWater) waterishTiles++;
    }
    if (waterishTiles > (MAP_W * MAP_H) / 4 && !hasViableDockShoreline(game))
        return fail("ocean map has no viable dock shoreline");

    for (const Entity& entity : game.entities) {
        if (!entity.alive || entity.type != E_TOWNHALL || entity.owner < 0 || entity.owner >= MAX_PLAYERS) continue;
        const EntityStats& stats = STATS[entity.type];
        for (int dy = 0; dy < stats.sizeH; dy++) for (int dx = 0; dx < stats.sizeW; dx++) {
            int x = entity.x + dx, y = entity.y + dy;
            if (!inBounds(x, y)) return fail("starting Town Hall outside map bounds");
            if (!validStartingFoundationTerrain(game.map[y][x].terrain))
                return fail("starting Town Hall on invalid terrain");
        }
        if (!hasReachableStartingResources(game, entity.x, entity.y))
            return fail("starting Town Hall lacks reachable wood, food, and gold");
    }
    return true;
}

static void generateContinentMap(Game& game, const MapNoise& noise) {
    int n = 2 + (realmRand(game) % 2);
    std::vector<std::pair<int,int>> seeds;
    auto jitter = [&game](int v, int amt) { return v + (realmRand(game) % (2*amt + 1)) - amt; };
    if (n == 2) {
        seeds.push_back({jitter(MAP_W*1/4, 10), jitter(MAP_H/2, MAP_H/6)});
        seeds.push_back({jitter(MAP_W*3/4, 10), jitter(MAP_H/2, MAP_H/6)});
    } else {
        seeds.push_back({jitter(MAP_W*1/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W*3/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W/2,    8), jitter(MAP_H*3/4, 6)});
    }

    int contR = std::min(MAP_W, MAP_H) / 4 + 5;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float minD = 1e9f;
        for (auto& s : seeds) {
            float d = edist(x, y, s.first, s.second);
            if (d < minD) minD = d;
        }
        float wobble = sampleNoise(noise, x*0.07f, y*0.07f) * 18.0f - 4.0f;
        float adjD = minD + wobble;

        Biome b; Terrain t;
        if      (adjD < contR - 6) { b = B_TEMPERATE; t = T_GRASS;    }
        else if (adjD < contR - 3) { b = B_TEMPERATE; t = (realmRand(game)%3==0) ? T_SAND : T_GRASS; }
        else if (adjD < contR)     { b = B_TEMPERATE; t = T_SAND;     }
        else if (adjD < contR + 3) { b = B_OCEAN;     t = T_SHALLOWS; }
        else                       { b = B_OCEAN;     t = T_WATER;    }
        game.map[y][x] = {t, 0, {}, {}, b, t, 0};
    }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (game.map[y][x].biome != B_TEMPERATE || game.map[y][x].terrain != T_GRASS) continue;
        int r = realmRand(game) % 100;
        if      (r < 12) { game.map[y][x].terrain = T_FOREST; game.map[y][x].resources = 100 + realmRand(game)%100; }
        else if (r < 16) game.map[y][x].terrain = T_TALL_GRASS;
        else if (r < 19) game.map[y][x].terrain = T_FLOWERS;
        else if (r < 21) game.map[y][x].terrain = T_MEADOW;
    }

    for (int i = 0; i < 9; i++) {
        int ix = 15 + realmRand(game)%(MAP_W-30), iy = 15 + realmRand(game)%(MAP_H-30);
        if (game.map[iy][ix].terrain != T_WATER) continue;
        int sz = 1 + realmRand(game) % 2;
        for (int dy = -sz-1; dy <= sz+1; dy++) for (int dx = -sz-1; dx <= sz+1; dx++) {
            int nx = ix+dx, ny = iy+dy;
            if (!inBounds(nx,ny)) continue;
            int r2 = dx*dx + dy*dy;
            if (r2 <= sz*sz) {
                Terrain isle = (realmRand(game)%3==0) ? T_FOREST : T_GRASS;
                game.map[ny][nx].terrain = isle;
                game.map[ny][nx].biome = B_TEMPERATE;
                if (isle == T_FOREST) game.map[ny][nx].resources = 80 + realmRand(game)%60;
            } else if (r2 <= (sz+1)*(sz+1)) {
                if (game.map[ny][nx].terrain == T_WATER) game.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = game.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && realmRand(game) % 22 == 0) {
            game.map[y][x].terrain = T_FISH;
            game.map[y][x].resources = 80 + realmRand(game) % 70;
        }
    }
    for (int i = 0; i < 14; i++) {
        int gx = 15 + realmRand(game)%(MAP_W-30), gy = 15 + realmRand(game)%(MAP_H-30);
        if (game.map[gy][gx].biome == B_TEMPERATE) placeGoldCluster(game, gx, gy, 3 + realmRand(game)%3);
    }
    for (int i = 0; i < 16; i++) {
        int bx = 10 + realmRand(game)%(MAP_W-20), by = 10 + realmRand(game)%(MAP_H-20);
        if (game.map[by][bx].biome != B_TEMPERATE) continue;
        int sz = 1 + realmRand(game) % 2;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = game.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && realmRand(game)%3 != 0) {
                game.map[ny][nx].terrain = T_BERRY;
                game.map[ny][nx].resources = 50 + realmRand(game) % 40;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        int wx = 10 + realmRand(game)%(MAP_W-20), wy = 10 + realmRand(game)%(MAP_H-20);
        if (game.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + realmRand(game) % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_GRASS && realmRand(game)%2==0)
                game.map[ny][nx].terrain = T_WHEAT;
        }
    }
    for (int i = 0; i < 15; i++) {
        int rx = 10 + realmRand(game)%(MAP_W-20), ry = 10 + realmRand(game)%(MAP_H-20);
        if (game.map[ry][rx].biome != B_TEMPERATE) continue;
        for (int j = 0; j < 3+realmRand(game)%4; j++) {
            int nx = rx + realmRand(game)%5-2, ny = ry + realmRand(game)%5-2;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_GRASS) game.map[ny][nx].terrain = T_RUINS;
        }
    }
    for (auto& s : seeds) placeCastleRuin(game, s.first - 3, s.second - 3, 6);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        game.map[y][x].preWinterTerrain = game.map[y][x].terrain;
}

MapGenerationConfig currentMapGenerationConfig(const Game& game) {
    return { game.biomeChoice };
}

void generateMap(Game& game, const MapGenerationConfig& config) {
    game.biomeChoice = (config.biomeChoice >= -1 && config.biomeChoice <= B_OCEAN)
        ? config.biomeChoice
        : -1;
    const MapNoise noise = initMapNoise(game);
    if (game.biomeChoice == B_OCEAN) {
        generateContinentMap(game, noise);
    } else {
        assignBiomesAndPaintBaseTerrain(game, noise);
        addMountains(game, noise);
        addWater(game);
        addFish(game);
        addGold(game);
        addStone(game);
        addRoads(game, noise);
        addPointsOfInterest(game);
        addFoodPatches(game);
        snapshotPreWinterTerrain(game);
    }
    std::string error;
    if (!validateMapInvariants(game, &error))
        std::cerr << "realm: map invariant failed: " << error << "\n";
}
