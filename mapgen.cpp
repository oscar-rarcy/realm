#include "realm.h"

static float noiseGrid[32][32];

static void initNoise() {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            noiseGrid[y][x] = (float)(simRand() % 1000) / 1000.0f;
}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float sampleNoise(float fx, float fy) {
    int x0 = (int)fx % 31, y0 = (int)fy % 31, x1 = x0 + 1, y1 = y0 + 1;
    float tx = fx - (int)fx, ty = fy - (int)fy;
    return lerp(lerp(noiseGrid[y0][x0], noiseGrid[y0][x1], tx),
                lerp(noiseGrid[y1][x0], noiseGrid[y1][x1], tx), ty);
}

// Spawn-area prep called from main.cpp once spawn positions are chosen.
void clearStartArea(int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius+4; dy++) for (int dx = -radius; dx <= radius+4; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y) && g.map[y][x].terrain != T_GOLD) g.map[y][x].terrain = T_GRASS;
    }
    // Level a wider apron around the spawn so the starting base, its gold
    // cluster, and the first farms never end up split across a cliff.
    int baseElev = inBounds(cx,cy) ? g.map[cy][cx].elev : 0;
    int er = radius + 9;
    for (int dy = -er; dy <= er; dy++) for (int dx = -er; dx <= er; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y)) g.map[y][x].elev = baseElev;
    }
}

void placeGoldCluster(int cx, int cy, int count) {
    for (int i = 0; i < count; i++) {
        int gx = cx + (simRand()%7)-3, gy = cy + (simRand()%5)-2;
        if (inBounds(gx,gy) && g.map[gy][gx].terrain != T_WATER
            && g.map[gy][gx].terrain != T_MOUNTAIN && g.map[gy][gx].terrain != T_SHALLOWS)
            { g.map[gy][gx].terrain = T_GOLD; g.map[gy][gx].resources = 300 + simRand() % 300; }
    }
}

static void placeCastleRuin(int cx, int cy, int size) {
    for (int dy = 0; dy < size; dy++) for (int dx = 0; dx < size; dx++) {
        int x = cx + dx, y = cy + dy;
        if (!inBounds(x, y)) continue;
        bool isEdge   = (dx == 0 || dx == size-1 || dy == 0 || dy == size-1);
        bool isCorner = (dx == 0 || dx == size-1) && (dy == 0 || dy == size-1);
        bool isGate   = !isCorner && isEdge && (dx == size/2 || dy == size/2);
        if (isGate)       g.map[y][x].terrain = T_CASTLE_GATE;
        else if (isEdge)  g.map[y][x].terrain = (simRand() % 4 != 0) ? T_CASTLE_WALL : T_RUINS;
        else              g.map[y][x].terrain = T_CASTLE_FLOOR;
        g.map[y][x].resources = 0;
    }
    int corners[][2] = {{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c : corners)
        if (inBounds(c[0], c[1])) g.map[c[1]][c[0]].terrain = T_CASTLE_WALL;
    // The keep itself: a neutral, capturable shelter in the castle's heart.
    // Garrison units inside to claim it — vision and stone walls, no upkeep.
    int kx = cx + size/2 - 1, ky = cy + size/2 - 1;
    if (inBounds(kx, ky) && inBounds(kx+1, ky+1))
        spawnEntity(E_RUIN, OWNER_NATURE, kx, ky);
}

// Distance helper for Voronoi continent generation — Euclidean (round shapes).
static float edist(int x1, int y1, int x2, int y2) {
    int dx = x1-x2, dy = y1-y2;
    return std::sqrt((float)(dx*dx + dy*dy));
}

// Coastal map: 2-3 large landmasses separated by sea channels, with a few
// small islands in between. Replaces the noise-soup archipelago.
static void generateContinentMap() {
    // Pick continent seeds, spread evenly across the map.
    int n = 2 + (simRand() % 2);                       // 2 or 3 continents
    std::vector<std::pair<int,int>> seeds;
    auto jitter = [](int v, int amt) { return v + (simRand() % (2*amt + 1)) - amt; };
    if (n == 2) {
        seeds.push_back({jitter(MAP_W*1/4, 10), jitter(MAP_H/2, MAP_H/6)});
        seeds.push_back({jitter(MAP_W*3/4, 10), jitter(MAP_H/2, MAP_H/6)});
    } else {
        seeds.push_back({jitter(MAP_W*1/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W*3/4, 8), jitter(MAP_H*1/3, 6)});
        seeds.push_back({jitter(MAP_W/2,    8), jitter(MAP_H*3/4, 6)});
    }

    // Continent radius: scales with map size. ~1/4 of the shorter dimension.
    int contR = std::min(MAP_W, MAP_H) / 4 + 5;

    // Assign every tile by (noise-perturbed) distance to nearest continent seed.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float minD = 1e9f;
        for (auto& s : seeds) {
            float d = edist(x, y, s.first, s.second);
            if (d < minD) minD = d;
        }
        // Wobble the coastline with noise so continents aren't perfect circles.
        float wobble = sampleNoise(x*0.07f, y*0.07f) * 18.0f - 4.0f; // -4..+14
        float adjD = minD + wobble;

        Biome b; Terrain t;
        if      (adjD < contR - 6) { b = B_TEMPERATE; t = T_GRASS;    }
        else if (adjD < contR - 3) { b = B_TEMPERATE; t = (simRand()%3==0) ? T_SAND : T_GRASS; }
        else if (adjD < contR)     { b = B_TEMPERATE; t = T_SAND;     }    // beach
        else if (adjD < contR + 3) { b = B_OCEAN;     t = T_SHALLOWS; }
        else                       { b = B_OCEAN;     t = T_WATER;    }
        g.map[y][x] = {t, 0, {}, {}, b, t, 0, 0};
    }

    // Inland variety: scatter forest/meadow/tall grass on grass tiles.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g.map[y][x].biome != B_TEMPERATE || g.map[y][x].terrain != T_GRASS) continue;
        int r = simRand() % 100;
        if      (r < 12) { g.map[y][x].terrain = T_FOREST; g.map[y][x].resources = 100 + simRand()%100; }
        else if (r < 16) g.map[y][x].terrain = T_TALL_GRASS;
        else if (r < 19) g.map[y][x].terrain = T_FLOWERS;
        else if (r < 21) g.map[y][x].terrain = T_MEADOW;
    }

    // A few small islands scattered between continents — strategic stepping stones.
    for (int i = 0; i < 9; i++) {
        int ix = 15 + simRand()%(MAP_W-30), iy = 15 + simRand()%(MAP_H-30);
        if (g.map[iy][ix].terrain != T_WATER) continue;
        int sz = 1 + simRand() % 2;
        for (int dy = -sz-1; dy <= sz+1; dy++) for (int dx = -sz-1; dx <= sz+1; dx++) {
            int nx = ix+dx, ny = iy+dy;
            if (!inBounds(nx,ny)) continue;
            int r2 = dx*dx + dy*dy;
            if      (r2 <= sz*sz) {
                Terrain isle = (simRand()%3==0) ? T_FOREST : T_GRASS;
                g.map[ny][nx].terrain = isle;
                g.map[ny][nx].biome = B_TEMPERATE;
                if (isle == T_FOREST) g.map[ny][nx].resources = 80 + simRand()%60;
            }
            else if (r2 <= (sz+1)*(sz+1)) {
                if (g.map[ny][nx].terrain == T_WATER) g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }

    // Fish in water (slightly denser than the standard map — more naval food).
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && simRand() % 22 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + simRand() % 70;
        }
    }

    // Gold clusters on land only.
    for (int i = 0; i < 14; i++) {
        int gx = 15 + simRand()%(MAP_W-30), gy = 15 + simRand()%(MAP_H-30);
        if (g.map[gy][gx].biome == B_TEMPERATE) placeGoldCluster(gx, gy, 3 + simRand()%3);
    }

    // Berry, wheat, ruins on land.
    for (int i = 0; i < 16; i++) {
        int bx = 10 + simRand()%(MAP_W-20), by = 10 + simRand()%(MAP_H-20);
        if (g.map[by][bx].biome != B_TEMPERATE) continue;
        int sz = 1 + simRand() % 2;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && simRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + simRand() % 40;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        int wx = 10 + simRand()%(MAP_W-20), wy = 10 + simRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && simRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    for (int i = 0; i < 15; i++) {
        int rx = 10 + simRand()%(MAP_W-20), ry = 10 + simRand()%(MAP_H-20);
        if (g.map[ry][rx].biome != B_TEMPERATE) continue;
        for (int j = 0; j < 3+simRand()%4; j++) {
            int nx = rx + simRand()%5-2, ny = ry + simRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }

    // One castle ruin per continent — a landmark on each landmass.
    for (auto& s : seeds) placeCastleRuin(s.first - 3, s.second - 3, 6);

    // Snapshot for winter thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}

void generateMap() {
    initNoise();
    // Coastal maps get their own special generator with proper continents.
    if (g.biomeChoice == B_OCEAN) { generateContinentMap(); return; }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        // Two noise channels at different scales give organic biome regions.
        // n1 is the macro climate (hot/cold), n2 is the micro feature (wet/dry).
        float n1 = sampleNoise(x*0.028f,     y*0.028f);
        float n2 = sampleNoise(x*0.020f+10,  y*0.020f+10);
        Biome b = B_TEMPERATE;
        if (g.biomeChoice >= 0) {
            // Player picked a biome — make it dominant (~70%) but still inject
            // patches of other biomes so the map has variety.
            b = (Biome)g.biomeChoice;
            // ~30% chance to swap in a contrasting biome from a small patch.
            float patch = sampleNoise(x*0.06f+30, y*0.06f+30);
            if (patch > 0.78f) {
                // Pick a non-ocean contrast based on n2 for variety.
                if      (b == B_TEMPERATE) b = (n2 > 0.5f) ? B_FOREST : B_DESERT;
                else if (b == B_FOREST)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SWAMP;
                else if (b == B_DESERT)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SNOW;
                else if (b == B_SNOW)      b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
                else if (b == B_SWAMP)     b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
                // Ocean stays ocean — its identity is total water coverage.
            }
        } else {
            // Random map: latitude-banded climate. North is cold, south is
            // hot, noise wobbles the band borders so they read as organic
            // frontiers rather than ruler lines. Swamps hug the wet noise,
            // forests sit on the cool side of temperate. The map gets real
            // geography: tundra campaigns up top, desert flanks below.
            float lat = (float)y / MAP_H;                 // 0 = north, 1 = south
            float climate = lat * 0.55f + n1 * 0.45f;     // cold..hot with wobble
            if      (climate < 0.24f)          b = B_SNOW;
            else if (climate > 0.76f)          b = B_DESERT;
            else if (n2 > 0.72f)               b = B_SWAMP;
            else if (climate < 0.45f && n2 < 0.40f) b = B_FOREST;
            // remainder stays B_TEMPERATE
        }
        g.map[y][x] = {T_GRASS, 0, {}, {}, b, T_GRASS, 0, 0};
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x]; int r = simRand() % 100;
        switch (t.biome) {
        case B_TEMPERATE:
            if (r<5)       t.terrain = T_TALL_GRASS;
            else if (r<8)  t.terrain = T_FLOWERS;
            else if (r<10) t.terrain = T_MEADOW;
            else if (r<14) { t.terrain = T_FOREST; t.resources = 100 + simRand() % 100; }
            else           t.terrain = T_GRASS;
            break;
        case B_DESERT:
            if (r<60)      t.terrain = T_SAND;
            else if (r<75) t.terrain = T_DUNES;
            else if (r<80) t.terrain = T_GRAVEL;
            else if (r<85) { t.terrain = T_PALM; t.resources = 60 + simRand() % 40; }
            else           t.terrain = T_SAND;
            break;
        case B_SNOW:
            if (r<60)      t.terrain = T_SNOW;
            else if (r<75) { t.terrain = T_PINE; t.resources = 80 + simRand() % 60; }
            else if (r<80) t.terrain = T_STONE;
            else           t.terrain = T_SNOW;
            break;
        case B_SWAMP:
            if (r<30)      t.terrain = T_MARSH;
            else if (r<45) t.terrain = T_REEDS;
            else if (r<55) t.terrain = T_SHALLOWS;
            else if (r<65) { t.terrain = T_DEAD_TREE; t.resources = 40 + simRand() % 30; }
            else           t.terrain = T_TALL_GRASS;
            break;
        case B_FOREST:
            if (r<40)      { t.terrain = T_FOREST; t.resources = 100 + simRand() % 100; }
            else if (r<55) { t.terrain = T_PINE;   t.resources = 80  + simRand() % 60;  }
            else if (r<60) { t.terrain = T_BERRY;  t.resources = 50  + simRand() % 40;  }
            else if (r<65) t.terrain = T_TALL_GRASS;
            else           t.terrain = T_GRASS;
            break;
        case B_VOLCANIC:
            // Deprecated — biome remains in the enum but is never generated.
            // Falls through to a temperate-default tile for safety.
            t.terrain = T_GRASS;
            break;
        case B_OCEAN:
            // Archipelago: mostly water with scattered island terrain.
            if (r<50)      t.terrain = T_WATER;
            else if (r<65) t.terrain = T_SHALLOWS;
            else if (r<70) t.terrain = T_SAND;
            else if (r<73) t.terrain = T_REEDS;
            else if (r<80) t.terrain = T_GRASS;
            else if (r<87) t.terrain = T_TALL_GRASS;
            else if (r<93) { t.terrain = T_FOREST; t.resources = 80 + simRand()%60; }
            else           t.terrain = T_GRASS;
            break;
        }
    }
    // Mountains
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = sampleNoise(x*0.12f+5, y*0.12f+5);
        if (n > 0.78f) { g.map[y][x].terrain = T_MOUNTAIN; g.map[y][x].resources = 0; }
        else if (n > 0.72f && g.map[y][x].biome != B_DESERT)
            if (simRand() % 3 == 0) g.map[y][x].terrain = T_HILLS;
    }
    // Rivers — scaled to the larger map (5 instead of 4)
    for (int r = 0; r < 5; r++) {
        int rx, ry;
        if (r % 2 == 0) { rx = simRand() % MAP_W; ry = 0; }
        else             { rx = 0; ry = simRand() % MAP_H; }
        int len = 80 + simRand() % 50;
        float angle = (simRand() % 628) / 100.0f;
        // Track river path so we can guarantee at least one fordable crossing.
        std::vector<std::pair<int,int>> path;
        for (int i = 0; i < len; i++) {
            int wx = rx + (int)(cos(angle)*i), wy = ry + (int)(sin(angle)*i);
            angle += ((simRand() % 100) - 50) / 200.0f;
            path.push_back({wx, wy});
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = wx+dx, ny = wy+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                    if (dx==0 && dy==0) g.map[ny][nx].terrain = T_WATER;
                    else if (simRand() % 3 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                }
            }
        }
        // Guaranteed ford: pick one point along the river and clear a 2-tile-wide
        // strip of shallows — gives armies a crossable choke point.
        if ((int)path.size() > 8) {
            auto& p = path[path.size()/2 + (simRand() % (path.size()/4)) - (int)path.size()/8];
            for (int dy = -2; dy <= 2; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = p.first+dx, ny = p.second+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_WATER)
                    g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }
    // Lakes — bumped to 9 for the larger map
    for (int l = 0; l < 9; l++) {
        int cx = 20 + simRand() % (MAP_W-40), cy = 20 + simRand() % (MAP_H-40), sz = 3 + simRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            if (dx*dx + dy*dy > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                if (dx*dx + dy*dy < (sz-1)*(sz-1)) g.map[ny][nx].terrain = T_WATER;
                else if (simRand() % 2 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                else g.map[ny][nx].terrain = T_REEDS;
            }
        }
    }
    // Open inland seas — bumped to 3 for the larger map.
    for (int s = 0; s < 3; s++) {
        int cx = 30 + simRand() % (MAP_W - 60);
        int cy = 25 + simRand() % (MAP_H - 50);
        int sz = 7 + simRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_MOUNTAIN || o == T_GOLD) continue;
            if (r2 < (sz-2)*(sz-2))      g.map[ny][nx].terrain = T_WATER;
            else if (r2 < (sz-1)*(sz-1)) g.map[ny][nx].terrain = (simRand()%4==0) ? T_SHALLOWS : T_WATER;
            else                         g.map[ny][nx].terrain = (simRand()%2==0) ? T_SHALLOWS : T_REEDS;
            g.map[ny][nx].resources = 0;
        }
    }
    // Fish shoals — sparse food deposits in open water and shallows.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && simRand() % 30 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + simRand() % 70;
        }
    }
    // Gold — scattered medium deposits. Spawn-point gold is added later
    // by main.cpp once spawn positions are chosen.
    for (int i = 0; i < 14; i++)
        placeGoldCluster(15 + simRand()%(MAP_W-30), 15 + simRand()%(MAP_H-30), 3 + simRand()%3);
    // Signature gold lode: one extra-rich deposit somewhere mid-map — a
    // strategic landmark worth contesting.
    {
        int gx = MAP_W/3 + simRand()%(MAP_W/3);
        int gy = MAP_H/3 + simRand()%(MAP_H/3);
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int nx = gx+dx, ny = gy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_WATER || o == T_MOUNTAIN || o == T_SHALLOWS) continue;
            if (dx*dx + dy*dy <= 5) {
                g.map[ny][nx].terrain = T_GOLD;
                g.map[ny][nx].resources = 500 + simRand() % 300;
            }
        }
    }
    // Stone clusters
    for (int i = 0; i < 17; i++) {
        int sx = 10 + simRand()%(MAP_W-20), sy = 10 + simRand()%(MAP_H-20);
        for (int j = 0; j < 3; j++) {
            int nx = sx + simRand()%4-2, ny = sy + simRand()%4-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_STONE;
        }
    }
    // Roads
    int midX = MAP_W/2, midY = MAP_H/2;
    auto makeRoad = [&](int sx, int sy, int ex, int ey) {
        int cx = sx, cy = sy;
        while (cx != ex || cy != ey) {
            if (inBounds(cx,cy) && g.map[cy][cx].terrain != T_WATER
                && g.map[cy][cx].terrain != T_MOUNTAIN && g.map[cy][cx].terrain != T_GOLD
                && g.map[cy][cx].terrain != T_SHALLOWS)
                g.map[cy][cx].terrain = T_ROAD;
            if (simRand()%2==0) { if(cx<ex)cx++; else if(cx>ex)cx--; }
            else              { if(cy<ey)cy++; else if(cy>ey)cy--; }
            if (simRand()%5==0) { cx += (simRand()%3)-1; cy += (simRand()%3)-1; }
            cx = std::max(0, std::min(cx, MAP_W-1));
            cy = std::max(0, std::min(cy, MAP_H-1));
        }
    };
    // Ocean maps are mostly water — roads on water tiles look wrong; skip them.
    if (g.biomeChoice != B_OCEAN) {
        // Crossroads through the middle so spawn-to-spawn travel has natural paths.
        makeRoad(15,        15,         midX,    midY);
        makeRoad(MAP_W-15,  MAP_H-15,   midX,    midY);
        makeRoad(midX,      5,          midX,    MAP_H-5);
        makeRoad(5,         midY,       MAP_W-5, midY);
    }
    // Mountain pass — a horizontal wall of mountains across one band of the map,
    // with a 3-tile gap forming a strategic choke point. Skip on ocean maps.
    if (g.biomeChoice != B_OCEAN && simRand() % 2 == 0) {
        int passY = MAP_H/2 + (simRand() % 20) - 10;
        int gapX  = MAP_W/4 + simRand() % (MAP_W/2);
        for (int x = 5; x < MAP_W - 5; x++) {
            // Skip the gap and a small variation zone around it
            if (std::abs(x - gapX) < 3) continue;
            float n = sampleNoise(x*0.3f, passY*0.3f + 99);
            // Mountains in a 1-2 tile band (some scatter for organic shape)
            int thickness = 1 + (n > 0.5f ? 1 : 0);
            for (int dy = -thickness; dy <= thickness; dy++) {
                int ny = passY + dy + (int)(sampleNoise(x*0.4f, 88)*3) - 1;
                if (inBounds(x, ny) && g.map[ny][x].terrain != T_WATER
                    && g.map[ny][x].terrain != T_GOLD)
                    g.map[ny][x].terrain = T_MOUNTAIN;
            }
        }
    }
    // Castle ruins — three signature ones at thirds.
    placeCastleRuin(MAP_W/2-4, MAP_H/2-4, 8);
    placeCastleRuin(MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(3*MAP_W/4, 3*MAP_H/4, 6);
    // Smaller ruin clusters scattered everywhere
    for (int i = 0; i < 22; i++) {
        int rx = 10 + simRand()%(MAP_W-20), ry = 10 + simRand()%(MAP_H-20);
        for (int j = 0; j < 3+simRand()%4; j++) {
            int nx = rx + simRand()%5-2, ny = ry + simRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }
    // Berry patches
    for (int i = 0; i < 20; i++) {
        int bx = 10 + simRand()%(MAP_W-20), by = 10 + simRand()%(MAP_H-20);
        Biome b = g.map[by][bx].biome;
        if (b == B_DESERT || b == B_SNOW || b == B_OCEAN) continue;
        int sz = 1 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && simRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + simRand() % 40;
            }
        }
    }
    // Wheat patches
    for (int i = 0; i < 17; i++) {
        int wx = 10 + simRand()%(MAP_W-20), wy = 10 + simRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + simRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && simRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    // Great corn meadows: a few HUGE swathes of wild wheat rolling across
    // temperate plains — natural breadbaskets. Settle near one and the
    // sow-a-farm-on-wheat mechanic turns it into your kingdom's larder;
    // they're also the most flammable thing in an enemy's economy to raid.
    for (int i = 0; i < 4; i++) {
        int mx = 20 + simRand()%(MAP_W-40), my = 15 + simRand()%(MAP_H-30);
        if (g.map[my][mx].biome != B_TEMPERATE) continue;
        int rx = 6 + simRand()%5, ry = 4 + simRand()%3;   // elliptical swathe
        for (int dy = -ry; dy <= ry; dy++) for (int dx = -rx; dx <= rx; dx++) {
            int nx = mx+dx, ny = my+dy;
            if (!inBounds(nx,ny)) continue;
            float ell = (float)(dx*dx)/(rx*rx) + (float)(dy*dy)/(ry*ry);
            if (ell > 1.0f) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o!=T_GRASS && o!=T_TALL_GRASS && o!=T_FLOWERS && o!=T_MEADOW) continue;
            // Dense heart of wheat, meadow fringe.
            g.map[ny][nx].terrain = (ell < 0.65f || simRand()%3 != 0) ? T_WHEAT : T_MEADOW;
        }
    }

    // === ELEVATION: highland plateaus ===
    // A coarse noise channel raises broad swathes of land one level. The rim
    // of each plateau is a cliff — impassable, a hard wall for armies — except
    // where ramps spawn (below). Water, castle ruins and mountains stay put.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        Terrain ter = t.terrain;
        bool noLift = (ter==T_WATER||ter==T_SHALLOWS||ter==T_REEDS||ter==T_MARSH
                    || ter==T_FISH ||ter==T_ICE||ter==T_CASTLE_WALL
                    || ter==T_CASTLE_FLOOR||ter==T_CASTLE_GATE);
        float n = sampleNoise(x*0.035f+60, y*0.035f+60);
        t.elev = (!noLift && n > 0.74f) ? 1 : 0;
    }
    // Ramps: roughly a quarter of each plateau's rim becomes climbable hill
    // tiles, so every highland is reachable but defensible — armies funnel
    // through the ramps, and a tower on one owns the approach.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x];
        if (t.elev != 1) continue;
        Terrain ter = t.terrain;
        if (ter==T_GOLD||ter==T_MOUNTAIN||ter==T_STONE) continue;
        bool rim = false;
        static const int d4[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (auto& d : d4) {
            int nx = x+d[0], ny = y+d[1];
            if (inBounds(nx,ny) && g.map[ny][nx].elev == 0
                && g.map[ny][nx].terrain != T_WATER) { rim = true; break; }
        }
        if (!rim) continue;
        if (((unsigned)(x*7919 + y*6271) % 4) == 0) {
            t.terrain = T_HILLS;
            t.resources = 0;
        }
    }

    // Baseline snapshot used by the winter→spring thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}
