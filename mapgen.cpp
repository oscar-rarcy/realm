#include "realm.h"

static float noiseGrid[32][32];

static void initNoise() {
    for (int y = 0; y < 32; y++)
        for (int x = 0; x < 32; x++)
            noiseGrid[y][x] = (float)(rand() % 1000) / 1000.0f;
}

static float lerp(float a, float b, float t) { return a + t * (b - a); }

static float sampleNoise(float fx, float fy) {
    int x0 = (int)fx % 31, y0 = (int)fy % 31, x1 = x0 + 1, y1 = y0 + 1;
    float tx = fx - (int)fx, ty = fy - (int)fy;
    return lerp(lerp(noiseGrid[y0][x0], noiseGrid[y0][x1], tx),
                lerp(noiseGrid[y1][x0], noiseGrid[y1][x1], tx), ty);
}

static void placeCastleRuin(int cx, int cy, int size) {
    for (int dy = 0; dy < size; dy++) for (int dx = 0; dx < size; dx++) {
        int x = cx + dx, y = cy + dy;
        if (!inBounds(x, y)) continue;
        bool isEdge   = (dx == 0 || dx == size-1 || dy == 0 || dy == size-1);
        bool isCorner = (dx == 0 || dx == size-1) && (dy == 0 || dy == size-1);
        bool isGate   = !isCorner && isEdge && (dx == size/2 || dy == size/2);
        if (isGate)       g.map[y][x].terrain = T_CASTLE_GATE;
        else if (isEdge)  g.map[y][x].terrain = (rand() % 4 != 0) ? T_CASTLE_WALL : T_RUINS;
        else              g.map[y][x].terrain = T_CASTLE_FLOOR;
        g.map[y][x].resources = 0;
    }
    int corners[][2] = {{cx,cy},{cx+size-1,cy},{cx,cy+size-1},{cx+size-1,cy+size-1}};
    for (auto& c : corners)
        if (inBounds(c[0], c[1])) g.map[c[1]][c[0]].terrain = T_CASTLE_WALL;
}

void generateMap() {
    initNoise();
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        // Larger biome patches: scale halved for more distinct identity.
        float n1 = sampleNoise(x*0.04f, y*0.04f), n2 = sampleNoise(x*0.025f+10, y*0.025f+10);
        Biome b = B_TEMPERATE;
        if (n1 > 0.7f) b = B_DESERT;
        else if (n1 < 0.25f) b = B_SNOW;
        else if (n2 > 0.7f) b = B_SWAMP;
        else if (n2 < 0.3f && n1 > 0.4f && n1 < 0.6f) b = B_FOREST;
        g.map[y][x] = {T_GRASS, 0, {false,false}, {false,false}, b};
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = g.map[y][x]; int r = rand() % 100;
        switch (t.biome) {
        case B_TEMPERATE:
            if (r<5)       t.terrain = T_TALL_GRASS;
            else if (r<8)  t.terrain = T_FLOWERS;
            else if (r<10) t.terrain = T_MEADOW;
            else if (r<14) { t.terrain = T_FOREST; t.resources = 100 + rand() % 100; }
            else           t.terrain = T_GRASS;
            break;
        case B_DESERT:
            if (r<60)      t.terrain = T_SAND;
            else if (r<75) t.terrain = T_DUNES;
            else if (r<80) t.terrain = T_GRAVEL;
            else if (r<85) { t.terrain = T_PALM; t.resources = 60 + rand() % 40; }
            else           t.terrain = T_SAND;
            break;
        case B_SNOW:
            if (r<60)      t.terrain = T_SNOW;
            else if (r<75) { t.terrain = T_PINE; t.resources = 80 + rand() % 60; }
            else if (r<80) t.terrain = T_STONE;
            else           t.terrain = T_SNOW;
            break;
        case B_SWAMP:
            if (r<30)      t.terrain = T_MARSH;
            else if (r<45) t.terrain = T_REEDS;
            else if (r<55) t.terrain = T_SHALLOWS;
            else if (r<65) { t.terrain = T_DEAD_TREE; t.resources = 40 + rand() % 30; }
            else           t.terrain = T_TALL_GRASS;
            break;
        case B_FOREST:
            if (r<40)      { t.terrain = T_FOREST; t.resources = 100 + rand() % 100; }
            else if (r<55) { t.terrain = T_PINE;   t.resources = 80  + rand() % 60;  }
            else if (r<60) t.terrain = T_BERRY;
            else if (r<65) t.terrain = T_TALL_GRASS;
            else           t.terrain = T_GRASS;
            break;
        }
    }
    // Mountains
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = sampleNoise(x*0.12f+5, y*0.12f+5);
        if (n > 0.78f) { g.map[y][x].terrain = T_MOUNTAIN; g.map[y][x].resources = 0; }
        else if (n > 0.72f && g.map[y][x].biome != B_DESERT)
            if (rand() % 3 == 0) g.map[y][x].terrain = T_HILLS;
    }
    // Rivers
    for (int r = 0; r < 4; r++) {
        int rx, ry;
        if (r % 2 == 0) { rx = rand() % MAP_W; ry = 0; }
        else             { rx = 0; ry = rand() % MAP_H; }
        int len = 60 + rand() % 40;
        float angle = (rand() % 628) / 100.0f;
        for (int i = 0; i < len; i++) {
            int wx = rx + (int)(cos(angle)*i), wy = ry + (int)(sin(angle)*i);
            angle += ((rand() % 100) - 50) / 200.0f;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = wx+dx, ny = wy+dy;
                if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                    if (dx==0 && dy==0) g.map[ny][nx].terrain = T_WATER;
                    else if (rand() % 3 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                }
            }
        }
    }
    // Lakes
    for (int l = 0; l < 6; l++) {
        int cx = 20 + rand() % (MAP_W-40), cy = 20 + rand() % (MAP_H-40), sz = 3 + rand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            if (dx*dx + dy*dy > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain != T_MOUNTAIN) {
                if (dx*dx + dy*dy < (sz-1)*(sz-1)) g.map[ny][nx].terrain = T_WATER;
                else if (rand() % 2 == 0) g.map[ny][nx].terrain = T_SHALLOWS;
                else g.map[ny][nx].terrain = T_REEDS;
            }
        }
    }
    // Open inland seas — sizeable water bodies so boats have somewhere to roam.
    for (int s = 0; s < 2; s++) {
        int cx = 30 + rand() % (MAP_W - 60);
        int cy = 25 + rand() % (MAP_H - 50);
        int sz = 7 + rand() % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = g.map[ny][nx].terrain;
            if (o == T_MOUNTAIN || o == T_GOLD) continue;
            if (r2 < (sz-2)*(sz-2))      g.map[ny][nx].terrain = T_WATER;
            else if (r2 < (sz-1)*(sz-1)) g.map[ny][nx].terrain = (rand()%4==0) ? T_SHALLOWS : T_WATER;
            else                         g.map[ny][nx].terrain = (rand()%2==0) ? T_SHALLOWS : T_REEDS;
            g.map[ny][nx].resources = 0;
        }
    }
    // Fish shoals — sparse food deposits in open water and shallows.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = g.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && rand() % 30 == 0) {
            g.map[y][x].terrain = T_FISH;
            g.map[y][x].resources = 80 + rand() % 70;
        }
    }
    // Gold
    auto placeGold = [](int cx, int cy, int count) {
        for (int i = 0; i < count; i++) {
            int gx = cx + (rand()%7)-3, gy = cy + (rand()%5)-2;
            if (inBounds(gx,gy) && g.map[gy][gx].terrain != T_WATER
                && g.map[gy][gx].terrain != T_MOUNTAIN && g.map[gy][gx].terrain != T_SHALLOWS)
                { g.map[gy][gx].terrain = T_GOLD; g.map[gy][gx].resources = 300 + rand() % 300; }
        }
    };
    placeGold(14, 12, 6); placeGold(MAP_W-16, MAP_H-14, 6);
    for (int i = 0; i < 10; i++) placeGold(15 + rand()%(MAP_W-30), 15 + rand()%(MAP_H-30), 3 + rand()%3);
    // Stone
    for (int i = 0; i < 12; i++) {
        int sx = 10 + rand()%(MAP_W-20), sy = 10 + rand()%(MAP_H-20);
        for (int j = 0; j < 3; j++) {
            int nx = sx + rand()%4-2, ny = sy + rand()%4-2;
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
            if (rand()%2==0) { if(cx<ex)cx++; else if(cx>ex)cx--; }
            else              { if(cy<ey)cy++; else if(cy>ey)cy--; }
            if (rand()%5==0) { cx += (rand()%3)-1; cy += (rand()%3)-1; }
            cx = std::max(0, std::min(cx, MAP_W-1));
            cy = std::max(0, std::min(cy, MAP_H-1));
        }
    };
    makeRoad(15,15,midX,midY); makeRoad(MAP_W-15,MAP_H-15,midX,midY); makeRoad(midX,5,midX,MAP_H-5);
    // Castle ruins
    placeCastleRuin(MAP_W/2-4, MAP_H/2-4, 8);
    placeCastleRuin(MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(3*MAP_W/4, 3*MAP_H/4, 6);
    // Ruins
    for (int i = 0; i < 15; i++) {
        int rx = 10 + rand()%(MAP_W-20), ry = 10 + rand()%(MAP_H-20);
        for (int j = 0; j < 3+rand()%4; j++) {
            int nx = rx + rand()%5-2, ny = ry + rand()%5-2;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS) g.map[ny][nx].terrain = T_RUINS;
        }
    }
    // Wheat patches
    for (int i = 0; i < 12; i++) {
        int wx = 10 + rand()%(MAP_W-20), wy = 10 + rand()%(MAP_H-20);
        if (g.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + rand() % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && g.map[ny][nx].terrain == T_GRASS && rand()%2==0)
                g.map[ny][nx].terrain = T_WHEAT;
        }
    }
    // Clear starting areas
    auto clearArea = [](int cx, int cy, int r) {
        for (int dy = -r; dy <= r+4; dy++) for (int dx = -r; dx <= r+4; dx++) {
            int x = cx+dx, y = cy+dy;
            if (inBounds(x,y) && g.map[y][x].terrain != T_GOLD) g.map[y][x].terrain = T_GRASS;
        }
    };
    clearArea(4,       4,       6); clearArea(MAP_W-11, 4,       6);
    clearArea(4,       MAP_H-11, 6); clearArea(MAP_W-11, MAP_H-11, 6);
    placeGold(14,         9,         5); placeGold(MAP_W-16, 9,         5);
    placeGold(14,         MAP_H-11,  5); placeGold(MAP_W-16, MAP_H-11,  5);

    // Baseline snapshot used by the winter→spring thaw cycle.
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        g.map[y][x].preWinterTerrain = g.map[y][x].terrain;
}
