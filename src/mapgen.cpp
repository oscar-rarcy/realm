#include "realm.h"

static float noiseGrid[32][32];

static void initNoise() {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            noiseGrid[y][x] = (float)(realmRand() % 1000) / 1000.0f;
}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float sampleNoise(float fx, float fy) {
    int x0 = (int)fx % 31, y0 = (int)fy % 31, x1 = x0 + 1, y1 = y0 + 1;
    float tx = fx - (int)fx, ty = fy - (int)fy;
    return lerp(lerp(noiseGrid[y0][x0], noiseGrid[y0][x1], tx),
                lerp(noiseGrid[y1][x0], noiseGrid[y1][x1], tx), ty);
}

void clearStartArea(int cx, int cy, int radius) {
    for (int dy = -radius; dy <= radius+4; dy++) for (int dx = -radius; dx <= radius+4; dx++) {
        int x = cx+dx, y = cy+dy;
        if (inBounds(x,y) && g.map[y][x].terrain != T_GOLD) {
            g.map[y][x].terrain = T_GRASS;
            g.map[y][x].resources = 0;
            g.map[y][x].preWinterTerrain = T_GRASS;
        }
    }
}

void placeGoldCluster(int cx, int cy, int count) {
    for (int i = 0; i < count; i++) {
        int gx = cx + (realmRand()%7)-3, gy = cy + (realmRand()%5)-2;
        if (inBounds(gx,gy) && g.map[gy][gx].terrain != T_WATER
            && g.map[gy][gx].terrain != T_MOUNTAIN && g.map[gy][gx].terrain != T_SHALLOWS) {
            g.map[gy][gx].terrain = T_GOLD;
            g.map[gy][gx].resources = 300 + realmRand() % 300;
            g.map[gy][gx].preWinterTerrain = T_GOLD;
        }
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
        else if (isEdge)  g.map[y][x].terrain = (realmRand() % 4 != 0) ? T_CASTLE_WALL : T_RUINS;
        else              g.map[y][x].terrain = T_CASTLE_FLOOR;
        g.map[y][x].resources = 0;
    }
    int corners[][2] = {{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c : corners)
        if (inBounds(c[0], c[1])) g.map[c[1]][c[0]].terrain = T_CASTLE_WALL;
}

static float edist(int x1, int y1, int x2, int y2) {
    int dx = x1-x2, dy = y1-y2;
    return std::sqrt((float)(dx*dx + dy*dy));
}

static void generateContinentMap() {
    int n = 2 + (realmRand() % 2);
    std::vector<std::pair<int,int>> seeds;
    auto jitter = [](int v, int amt) { return v + (realmRand() % (2*amt + 1)) - amt; };
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
        float wobble = sampleNoise(x*0.07f, y*0.07f) * 18.0f - 4.0f;
        float adjD = minD + wobble;

        Biome b; Terrain t;
        if      (adjD < contR - 6) { b = B_TEMPERATE; t = T_GRASS;    }
        else if (adjD < contR - 3) { b = B_TEMPERATE; t = (realmRand()%3==0) ? T_SAND : T_GRASS; }
        else if (adjD < contR)     { b = B_TEMPERATE; t = T_SAND;     }
        else if (adjD < contR + 3) { b = B_OCEAN;     t = T_SHALLOWS; }
        else                       { b = B_OCEAN;     t = T_WATER;    }
        g.map[y][x] = {t, 0, {}, {}, b, t, 0};
    }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g.map[y][x].biome != B_TEMPERATE || g.map[y][x].terrain != T_GRASS) continue;
        int r = realmRand() % 100;
        if      (r < 12) { g.map[y][x].terrain = T_FOREST; g.map[y][x].resources = 100 + realmRand()%100; }
        else if (r < 16) g.map[y][x].terrain = T_TALL_GRASS;
        else if (r < 19) g.map[y][x].terrain = T_FLOWERS;
        else if (r < 21) g.map[y][x].terrain = T_MEADOW;
    }

    for (int i = 0; i < 9; i++) {
        int ix = 15 + realmRand()%(MAP_W-30), iy = 15 + realmRand()%(MAP_H-30);
        if (g.map[iy][ix].terrain != T_WATER) continue;
        int sz = 1 + realmRand() % 2;
        for (int dy = -sz-1; dy <= sz+1; dy++) for (int dx = -sz-1; dx <= sz+1; dx++) {
            int nx = ix+dx, ny = iy+dy;
            if (!inBounds(nx,ny)) continue;
            int r2 = dx*dx + dy*dy;
            if (r2 <= sz*sz) {
                Terrain isle = (realmRand()%3==0) ? T_FOREST : T_GRASS;
                g.map[ny][nx].terrain = isle;
                g.map[ny][nx].biome = B_TEMPERATE;
                if (isle == T_FOREST) g.map[ny][nx].resources = 80 + realmRand()%60;
            } else if (r2 <= (sz+1)*(sz+1)) {
                if (g.map[ny][nx].terrain == T_WATER) g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && realmRand() % 22 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + realmRand() % 70;
        }
    }
    for (int i = 0; i < 14; i++) {
        int gx = 15 + realmRand()%(MAP_W-30), gy = 15 + realmRand()%(MAP_H-30);
        if (g.map[gy][gx].biome == B_TEMPERATE) placeGoldCluster(gx, gy, 3 + realmRand()%3);
    }
    for (int i = 0; i < 16; i++) {
        int bx = 10 + realmRand()%(MAP_W-20), by = 10 + realmRand()%(MAP_H-20);
        if (g.map[by][bx].biome != B_TEMPERATE) continue;
        int sz = 1 + realmRand() % 2;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && realmRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + realmRand() % 40;
            }
        }
    }
    for (int i = 0; i < 12; i++) {
        int wx = 10 + realmRand()%(MAP_W-20), wy = 10 + realmRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + realmRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && realmRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    for (int i = 0; i < 15; i++) {
        int rx = 10 + realmRand()%(MAP_W-20), ry = 10 + realmRand()%(MAP_H-20);
        if (g.map[ry][rx].biome != B_TEMPERATE) continue;
        for (int j = 0; j < 3+realmRand()%4; j++) {
            int nx = rx + realmRand()%5-2, ny = ry + realmRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }
    for (auto& s : seeds) placeCastleRuin(s.first - 3, s.second - 3, 6);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}

void generateMap() {
    initNoise();
    if (g.biomeChoice == B_OCEAN) { generateContinentMap(); return; }

    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n1 = sampleNoise(x*0.028f, y*0.028f), n2 = sampleNoise(x*0.020f+10, y*0.020f+10);
        Biome b = B_TEMPERATE;
        if (g.biomeChoice >= 0) {
            b = (Biome)g.biomeChoice;
            float patch = sampleNoise(x*0.06f+30, y*0.06f+30);
            if (patch > 0.78f) {
                if      (b == B_TEMPERATE) b = (n2 > 0.5f) ? B_FOREST : B_DESERT;
                else if (b == B_FOREST)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SWAMP;
                else if (b == B_DESERT)    b = (n2 > 0.5f) ? B_TEMPERATE : B_SNOW;
                else if (b == B_SNOW)      b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
                else if (b == B_SWAMP)     b = (n2 > 0.5f) ? B_FOREST : B_TEMPERATE;
            }
        } else {
            if      (n1 > 0.68f) b = B_DESERT;
            else if (n1 < 0.26f) b = B_SNOW;
            else if (n2 > 0.70f) b = B_SWAMP;
            else if (n2 < 0.30f && n1 > 0.38f && n1 < 0.62f) b = B_FOREST;
        }
        g.map[y][x] = {T_GRASS, 0, {}, {}, b, T_GRASS, 0};
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x]; int r = realmRand() % 100;
        switch (t.biome) {
        case B_TEMPERATE:
            if (r<5)       t.terrain = T_TALL_GRASS;
            else if (r<8)  t.terrain = T_FLOWERS;
            else if (r<10) t.terrain = T_MEADOW;
            else if (r<14) { t.terrain = T_FOREST; t.resources = 100 + realmRand() % 100; }
            else           t.terrain = T_GRASS;
            break;
        case B_DESERT:
            if (r<60)      t.terrain = T_SAND;
            else if (r<75) t.terrain = T_DUNES;
            else if (r<80) t.terrain = T_GRAVEL;
            else if (r<85) { t.terrain = T_PALM; t.resources = 60 + realmRand() % 40; }
            else           t.terrain = T_SAND;
            break;
        case B_SNOW:
            if (r<60)      t.terrain = T_SNOW;
            else if (r<75) { t.terrain = T_PINE; t.resources = 80 + realmRand() % 60; }
            else if (r<80) t.terrain = T_STONE;
            else           t.terrain = T_SNOW;
            break;
        case B_SWAMP:
            if (r<30)      t.terrain = T_MARSH;
            else if (r<45) t.terrain = T_REEDS;
            else if (r<55) t.terrain = T_SHALLOWS;
            else if (r<65) { t.terrain = T_DEAD_TREE; t.resources = 40 + realmRand() % 30; }
            else           t.terrain = T_TALL_GRASS;
            break;
        case B_FOREST:
            if (r<40)      { t.terrain = T_FOREST; t.resources = 100 + realmRand() % 100; }
            else if (r<55) { t.terrain = T_PINE;   t.resources = 80  + realmRand() % 60;  }
            else if (r<60) { t.terrain = T_BERRY;  t.resources = 50  + realmRand() % 40;  }
            else if (r<65) t.terrain = T_TALL_GRASS;
            else           t.terrain = T_GRASS;
            break;
        case B_VOLCANIC:
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
            else if (r<93) { t.terrain = T_FOREST; t.resources = 80 + realmRand()%60; }
            else           t.terrain = T_GRASS;
            break;
        }
    }
    // Mountains
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = sampleNoise(x*0.12f+5, y*0.12f+5);
        if (n > 0.78f) { g.map[y][x].terrain = T_MOUNTAIN; g.map[y][x].resources = 0; }
        else if (n > 0.72f && g.map[y][x].biome != B_DESERT)
            if (realmRand() % 3 == 0) g.map[y][x].terrain = T_HILLS;
    }
    // Rivers
    for (int r = 0; r < 5; r++) {
        int rx, ry;
        if (r % 2 == 0) { rx = realmRand() % MAP_W; ry = 0; }
        else             { rx = 0; ry = realmRand() % MAP_H; }
        int len = 80 + realmRand() % 50;
        float angle = (realmRand() % 628) / 100.0f;
        std::vector<std::pair<int,int>> path;
        for (int i = 0; i < len; i++) {
            int wx = rx + (int)(cos(angle)*i), wy = ry + (int)(sin(angle)*i);
            angle += ((realmRand() % 100) - 50) / 200.0f;
            path.push_back({wx, wy});
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = wx+dx, ny = wy+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                    if (dx==0 && dy==0) g.map[ny][nx].terrain = T_WATER;
                    else if (realmRand() % 3 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                }
            }
        }
        if ((int)path.size() > 8) {
            auto& p = path[path.size()/2 + (realmRand() % (path.size()/4)) - (int)path.size()/8];
            for (int dy = -2; dy <= 2; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = p.first+dx, ny = p.second+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_WATER)
                    g.map[ny][nx].terrain = T_SHALLOWS;
            }
        }
    }
    // Lakes
    for (int l = 0; l < 9; l++) {
        int cx = 20 + realmRand() % (MAP_W-40), cy = 20 + realmRand() % (MAP_H-40), sz = 3 + realmRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            if (dx*dx + dy*dy > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                if (dx*dx + dy*dy < (sz-1)*(sz-1)) g.map[ny][nx].terrain = T_WATER;
                else if (realmRand() % 2 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                else g.map[ny][nx].terrain = T_REEDS;
            }
        }
    }
    // Open inland seas — sizeable water bodies so boats have somewhere to roam.
    for (int s = 0; s < 3; s++) {
        int cx = 30 + realmRand() % (MAP_W - 60);
        int cy = 25 + realmRand() % (MAP_H - 50);
        int sz = 7 + realmRand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_MOUNTAIN || o == T_GOLD) continue;
            if (r2 < (sz-2)*(sz-2))      g.map[ny][nx].terrain = T_WATER;
            else if (r2 < (sz-1)*(sz-1)) g.map[ny][nx].terrain = (realmRand()%4==0) ? T_SHALLOWS : T_WATER;
            else                         g.map[ny][nx].terrain = (realmRand()%2==0) ? T_SHALLOWS : T_REEDS;
            g.map[ny][nx].resources = 0;
        }
    }
    // Fish shoals — sparse food deposits in open water and shallows.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && realmRand() % 30 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + realmRand() % 70;
        }
    }
    // Gold
    for (int i = 0; i < 14; i++)
        placeGoldCluster(15 + realmRand()%(MAP_W-30), 15 + realmRand()%(MAP_H-30), 3 + realmRand()%3);
    {
        int gx = MAP_W/3 + realmRand()%(MAP_W/3);
        int gy = MAP_H/3 + realmRand()%(MAP_H/3);
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int nx = gx+dx, ny = gy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_WATER || o == T_MOUNTAIN || o == T_SHALLOWS) continue;
            if (dx*dx + dy*dy <= 5) {
                g.map[ny][nx].terrain = T_GOLD;
                g.map[ny][nx].resources = 500 + realmRand() % 300;
            }
        }
    }
    // Stone
    for (int i = 0; i < 17; i++) {
        int sx = 10 + realmRand()%(MAP_W-20), sy = 10 + realmRand()%(MAP_H-20);
        for (int j = 0; j < 3; j++) {
            int nx = sx + realmRand()%4-2, ny = sy + realmRand()%4-2;
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
            if (realmRand()%2==0) { if(cx<ex)cx++; else if(cx>ex)cx--; }
            else              { if(cy<ey)cy++; else if(cy>ey)cy--; }
            if (realmRand()%5==0) { cx += (realmRand()%3)-1; cy += (realmRand()%3)-1; }
            cx = std::max(0, std::min(cx, MAP_W-1));
            cy = std::max(0, std::min(cy, MAP_H-1));
        }
    };
    // Ocean maps are mostly water — roads on water tiles look wrong; skip them.
    if (g.biomeChoice != B_OCEAN) {
        makeRoad(15,15,midX,midY);
        makeRoad(MAP_W-15,MAP_H-15,midX,midY);
        makeRoad(midX,5,midX,MAP_H-5);
        makeRoad(5,midY,MAP_W-5,midY);
    }
    if (g.biomeChoice != B_OCEAN && realmRand() % 2 == 0) {
        int passY = MAP_H/2 + (realmRand() % 20) - 10;
        int gapX  = MAP_W/4 + realmRand() % (MAP_W/2);
        for (int x = 5; x < MAP_W - 5; x++) {
            if (std::abs(x - gapX) < 3) continue;
            float n = sampleNoise(x*0.3f, passY*0.3f + 99);
            int thickness = 1 + (n > 0.5f ? 1 : 0);
            for (int dy = -thickness; dy <= thickness; dy++) {
                int ny = passY + dy + (int)(sampleNoise(x*0.4f, 88)*3) - 1;
                if (inBounds(x, ny) && g.map[ny][x].terrain != T_WATER
                    && g.map[ny][x].terrain != T_GOLD)
                    g.map[ny][x].terrain = T_MOUNTAIN;
            }
        }
    }
    // Castle ruins
    placeCastleRuin(MAP_W/2-4, MAP_H/2-4, 8);
    placeCastleRuin(MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(3*MAP_W/4, 3*MAP_H/4, 6);
    // Ruins
    for (int i = 0; i < 22; i++) {
        int rx = 10 + realmRand()%(MAP_W-20), ry = 10 + realmRand()%(MAP_H-20);
        for (int j = 0; j < 3+realmRand()%4; j++) {
            int nx = rx + realmRand()%5-2, ny = ry + realmRand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }
    // Berry patches — scattered clusters in temperate/forest/swamp biomes so any
    // map (even one with biome forced via splash) has wild food to forage.
    for (int i = 0; i < 20; i++) {
        int bx = 10 + realmRand()%(MAP_W-20), by = 10 + realmRand()%(MAP_H-20);
        Biome b = g.map[by][bx].biome;
        if (b == B_DESERT || b == B_SNOW || b == B_OCEAN) continue;
        int sz = 1 + realmRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = bx+dx, ny = by+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if ((o==T_GRASS||o==T_TALL_GRASS||o==T_MEADOW) && realmRand()%3 != 0) {
                g.map[ny][nx].terrain = T_BERRY;
                g.map[ny][nx].resources = 50 + realmRand() % 40;
            }
        }
    }
    // Wheat patches
    for (int i = 0; i < 17; i++) {
        int wx = 10 + realmRand()%(MAP_W-20), wy = 10 + realmRand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + realmRand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && realmRand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    // Baseline snapshot used by the winter→spring thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}
