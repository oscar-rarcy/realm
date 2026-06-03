#include "realm.h"

void assignBiomesAndPaintBaseTerrain(Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n1 = sampleNoise(x*0.028f, y*0.028f), n2 = sampleNoise(x*0.020f+10, y*0.020f+10);
        Biome b = B_TEMPERATE;
        if (game.biomeChoice >= 0) {
            b = (Biome)game.biomeChoice;
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
        game.map[y][x] = {T_GRASS, 0, {}, {}, b, T_GRASS, 0};
    }
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Tile& t = game.map[y][x]; int r = realmRand(game) % 100;
        switch (t.biome) {
        case B_TEMPERATE:
            if (r<5)       t.terrain = T_TALL_GRASS;
            else if (r<8)  t.terrain = T_FLOWERS;
            else if (r<10) t.terrain = T_MEADOW;
            else if (r<14) { t.terrain = T_FOREST; t.resources = 100 + realmRand(game) % 100; }
            else           t.terrain = T_GRASS;
            break;
        case B_DESERT:
            if (r<60)      t.terrain = T_SAND;
            else if (r<75) t.terrain = T_DUNES;
            else if (r<80) t.terrain = T_GRAVEL;
            else if (r<85) { t.terrain = T_PALM; t.resources = 60 + realmRand(game) % 40; }
            else           t.terrain = T_SAND;
            break;
        case B_SNOW:
            if (r<60)      t.terrain = T_SNOW;
            else if (r<75) { t.terrain = T_PINE; t.resources = 80 + realmRand(game) % 60; }
            else if (r<80) t.terrain = T_STONE;
            else           t.terrain = T_SNOW;
            break;
        case B_SWAMP:
            if (r<30)      t.terrain = T_MARSH;
            else if (r<45) t.terrain = T_REEDS;
            else if (r<55) t.terrain = T_SHALLOWS;
            else if (r<65) { t.terrain = T_DEAD_TREE; t.resources = 40 + realmRand(game) % 30; }
            else           t.terrain = T_TALL_GRASS;
            break;
        case B_FOREST:
            if (r<40)      { t.terrain = T_FOREST; t.resources = 100 + realmRand(game) % 100; }
            else if (r<55) { t.terrain = T_PINE;   t.resources = 80  + realmRand(game) % 60;  }
            else if (r<60) { t.terrain = T_BERRY;  t.resources = 50  + realmRand(game) % 40;  }
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
            else if (r<93) { t.terrain = T_FOREST; t.resources = 80 + realmRand(game)%60; }
            else           t.terrain = T_GRASS;
            break;
        }
    }
}

void addMountains(Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = sampleNoise(x*0.12f+5, y*0.12f+5);
        if (n > 0.78f) { game.map[y][x].terrain = T_MOUNTAIN; game.map[y][x].resources = 0; }
        else if (n > 0.72f && game.map[y][x].biome != B_DESERT)
            if (realmRand(game) % 3 == 0) { game.map[y][x].terrain = T_HILLS; game.map[y][x].resources = 0; }
    }
}

void addWater(Game& game) {
    for (int r = 0; r < 5; r++) {
        int rx, ry;
        if (r % 2 == 0) { rx = realmRand(game) % MAP_W; ry = 0; }
        else             { rx = 0; ry = realmRand(game) % MAP_H; }
        int len = 80 + realmRand(game) % 50;
        float angle = (realmRand(game) % 628) / 100.0f;
        std::vector<std::pair<int,int>> path;
        for (int i = 0; i < len; i++) {
            int wx = rx + (int)(cos(angle)*i), wy = ry + (int)(sin(angle)*i);
            angle += ((realmRand(game) % 100) - 50) / 200.0f;
            path.push_back({wx, wy});
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = wx+dx, ny = wy+dy;
                if (inBounds(nx,ny) && game.map[ny][nx].terrain != T_MOUNTAIN) {
                    if (dx==0 && dy==0) { game.map[ny][nx].terrain = T_WATER; game.map[ny][nx].resources = 0; }
                    else if (realmRand(game) % 3 == 0) { game.map[ny][nx].terrain = T_SHALLOWS; game.map[ny][nx].resources = 0; }
                }
            }
        }
        if ((int)path.size() > 8) {
            auto& p = path[path.size()/2 + (realmRand(game) % (path.size()/4)) - (int)path.size()/8];
            for (int dy = -2; dy <= 2; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = p.first+dx, ny = p.second+dy;
                if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_WATER) {
                    game.map[ny][nx].terrain = T_SHALLOWS;
                    game.map[ny][nx].resources = 0;
                }
            }
        }
    }
    // Lakes
    for (int l = 0; l < 9; l++) {
        int cx = 20 + realmRand(game) % (MAP_W-40), cy = 20 + realmRand(game) % (MAP_H-40), sz = 3 + realmRand(game) % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            if (dx*dx + dy*dy > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain != T_MOUNTAIN) {
                if (dx*dx + dy*dy < (sz-1)*(sz-1)) game.map[ny][nx].terrain = T_WATER;
                else if (realmRand(game) % 2 == 0) game.map[ny][nx].terrain = T_SHALLOWS;
                else game.map[ny][nx].terrain = T_REEDS;
                game.map[ny][nx].resources = 0;
            }
        }
    }
    // Open inland seas — sizeable water bodies so boats have somewhere to roam.
    for (int s = 0; s < 3; s++) {
        int cx = 30 + realmRand(game) % (MAP_W - 60);
        int cy = 25 + realmRand(game) % (MAP_H - 50);
        int sz = 7 + realmRand(game) % 4;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int r2 = dx*dx + dy*dy;
            if (r2 > sz*sz) continue;
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = game.map[ny][nx].terrain;
            if (o == T_MOUNTAIN || o == T_GOLD) continue;
            if (r2 < (sz-2)*(sz-2))      game.map[ny][nx].terrain = T_WATER;
            else if (r2 < (sz-1)*(sz-1)) game.map[ny][nx].terrain = (realmRand(game)%4==0) ? T_SHALLOWS : T_WATER;
            else                         game.map[ny][nx].terrain = (realmRand(game)%2==0) ? T_SHALLOWS : T_REEDS;
            game.map[ny][nx].resources = 0;
        }
    }
}

void addFish(Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Terrain t = game.map[y][x].terrain;
        if ((t == T_WATER || t == T_SHALLOWS) && realmRand(game) % 30 == 0) {
            game.map[y][x].terrain = T_FISH;
            game.map[y][x].resources = 80 + realmRand(game) % 70;
        }
    }
}

void addGold(Game& game) {
    for (int i = 0; i < 14; i++)
        placeGoldCluster(game, 15 + realmRand(game)%(MAP_W-30), 15 + realmRand(game)%(MAP_H-30), 3 + realmRand(game)%3);
    {
        int gx = MAP_W/3 + realmRand(game)%(MAP_W/3);
        int gy = MAP_H/3 + realmRand(game)%(MAP_H/3);
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int nx = gx+dx, ny = gy+dy;
            if (!inBounds(nx,ny)) continue;
            Terrain o = game.map[ny][nx].terrain;
            if (o == T_WATER || o == T_MOUNTAIN || o == T_SHALLOWS) continue;
            if (dx*dx + dy*dy <= 5) {
                game.map[ny][nx].terrain = T_GOLD;
                game.map[ny][nx].resources = 500 + realmRand(game) % 300;
            }
        }
    }
}

void addStone(Game& game) {
    for (int i = 0; i < 17; i++) {
        int sx = 10 + realmRand(game)%(MAP_W-20), sy = 10 + realmRand(game)%(MAP_H-20);
        for (int j = 0; j < 3; j++) {
            int nx = sx + realmRand(game)%4-2, ny = sy + realmRand(game)%4-2;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_GRASS) game.map[ny][nx].terrain = T_STONE;
        }
    }
}

void addRoads(Game& game) {
    int midX = MAP_W/2, midY = MAP_H/2;
    auto makeRoad = [&](int sx, int sy, int ex, int ey) {
        int cx = sx, cy = sy;
        while (cx != ex || cy != ey) {
            if (inBounds(cx,cy) && game.map[cy][cx].terrain != T_WATER
                && game.map[cy][cx].terrain != T_MOUNTAIN && game.map[cy][cx].terrain != T_GOLD
                && game.map[cy][cx].terrain != T_SHALLOWS)
                { game.map[cy][cx].terrain = T_ROAD; game.map[cy][cx].resources = 0; }
            if (realmRand(game)%2==0) { if(cx<ex)cx++; else if(cx>ex)cx--; }
            else              { if(cy<ey)cy++; else if(cy>ey)cy--; }
            if (realmRand(game)%5==0) { cx += (realmRand(game)%3)-1; cy += (realmRand(game)%3)-1; }
            cx = std::max(0, std::min(cx, MAP_W-1));
            cy = std::max(0, std::min(cy, MAP_H-1));
        }
    };
    // Ocean maps are mostly water — roads on water tiles look wrong; skip them.
    if (game.biomeChoice != B_OCEAN) {
        makeRoad(15,15,midX,midY);
        makeRoad(MAP_W-15,MAP_H-15,midX,midY);
        makeRoad(midX,5,midX,MAP_H-5);
        makeRoad(5,midY,MAP_W-5,midY);
    }
    if (game.biomeChoice != B_OCEAN && realmRand(game) % 2 == 0) {
        int passY = MAP_H/2 + (realmRand(game) % 20) - 10;
        int gapX  = MAP_W/4 + realmRand(game) % (MAP_W/2);
        for (int x = 5; x < MAP_W - 5; x++) {
            if (std::abs(x - gapX) < 3) continue;
            float n = sampleNoise(x*0.3f, passY*0.3f + 99);
            int thickness = 1 + (n > 0.5f ? 1 : 0);
            for (int dy = -thickness; dy <= thickness; dy++) {
                int ny = passY + dy + (int)(sampleNoise(x*0.4f, 88)*3) - 1;
                if (inBounds(x, ny) && game.map[ny][x].terrain != T_WATER
                    && game.map[ny][x].terrain != T_GOLD)
                    { game.map[ny][x].terrain = T_MOUNTAIN; game.map[ny][x].resources = 0; }
            }
        }
    }
}

void addPointsOfInterest(Game& game) {
    placeCastleRuin(game, MAP_W/2-4, MAP_H/2-4, 8);
    placeCastleRuin(game, MAP_W/4,   MAP_H/4,   6);
    placeCastleRuin(game, 3*MAP_W/4, 3*MAP_H/4, 6);
    // Ruins
    for (int i = 0; i < 22; i++) {
        int rx = 10 + realmRand(game)%(MAP_W-20), ry = 10 + realmRand(game)%(MAP_H-20);
        for (int j = 0; j < 3+realmRand(game)%4; j++) {
            int nx = rx + realmRand(game)%5-2, ny = ry + realmRand(game)%5-2;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_GRASS) game.map[ny][nx].terrain = T_RUINS;
        }
    }
}

void addFoodPatches(Game& game) {
    for (int i = 0; i < 20; i++) {
        int bx = 10 + realmRand(game)%(MAP_W-20), by = 10 + realmRand(game)%(MAP_H-20);
        Biome b = game.map[by][bx].biome;
        if (b == B_DESERT || b == B_SNOW || b == B_OCEAN) continue;
        int sz = 1 + realmRand(game) % 3;
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
    // Wheat patches
    for (int i = 0; i < 17; i++) {
        int wx = 10 + realmRand(game)%(MAP_W-20), wy = 10 + realmRand(game)%(MAP_H-20);
        if (game.map[wy][wx].biome != B_TEMPERATE) continue;
        int sz = 2 + realmRand(game) % 3;
        for (int dy = -sz; dy <= sz; dy++) for (int dx = -sz; dx <= sz; dx++) {
            int nx = wx+dx, ny = wy+dy;
            if (inBounds(nx,ny) && game.map[ny][nx].terrain == T_GRASS && realmRand(game)%2==0)
                game.map[ny][nx].terrain = T_WHEAT;
        }
    }
}

void snapshotPreWinterTerrain(Game& game) {
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        game.map[y][x].preWinterTerrain = game.map[y][x].terrain;
}
