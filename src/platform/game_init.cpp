#include "realm.h"
#include "view_state.h"

#include <iostream>

static void placeStartResources(int thX, int thY) {
    for (int i = 0; i < 7; i++) {
        int x = std::max(1, std::min(thX + 6 + i % 3, MAP_W - 2));
        int y = std::max(1, std::min(thY + 1 + i / 3, MAP_H - 2));
        if (!entityAt(x, y)) {
            g.map[y][x].terrain = T_FOREST;
            g.map[y][x].resources = 120;
            g.map[y][x].preWinterTerrain = T_FOREST;
        }
    }
    for (int i = 0; i < 5; i++) {
        int x = std::max(1, std::min(thX + 1 + i, MAP_W - 2));
        int y = std::max(1, std::min(thY + 7, MAP_H - 2));
        if (!entityAt(x, y)) {
            g.map[y][x].terrain = T_BERRY;
            g.map[y][x].resources = 70;
            g.map[y][x].preWinterTerrain = T_BERRY;
        }
    }
}

void initGame(int numAIs) {
    unsigned seedFallback = (unsigned)time(nullptr);
    unsigned seed = envUnsigned("REALM_SEED", seedFallback);
    int humanCorner = envInt("REALM_HUMAN_CORNER", -1);
    int forcedBiome = envInt("REALM_BIOME", g.biomeChoice);
    if (forcedBiome >= -1 && forcedBiome <= B_OCEAN) g.biomeChoice = forcedBiome;
    initGameWithSeed(numAIs, seed, humanCorner);
}

void initGameWithSeed(int numAIs, unsigned seed, int humanCorner) {
    initGameWithSeed(numAIs, seed, humanCorner, currentMapGenerationConfig());
}

void initGameWithSeed(int numAIs, unsigned seed, int humanCorner, const MapGenerationConfig& mapConfig) {
    realmSrand(seed);
    // `g.entities` is a deque so spawnEntity() can append during a tick without
    // invalidating the Entity references and pointers held by simulation code.
    int matchNumber = g.matchNumber + 1;
    g.entities.clear();
    g.projectiles.clear();
    g.actionMarkers.clear();
    g.projectiles.reserve(256);
    g.nextId = 1; g.tick = 0; g.mode = M_NORMAL;
    g.selectedId = -1; g.selectedIds.clear(); g.groupAssignPending = false;
    view.dragging = false; view.dragStartX = 0; view.dragStartY = 0;
    for (int i = 0; i < 9; i++) g.controlGroups[i].clear();
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int i = 0; i < 9; i++)
            g.controlGroupsByOwner[p][i].clear();
    g.winner = -1; g.aiTimer = 0; g.farmTimer = 0; g.animalTimer = 0;
    g.statusMsg.clear(); g.statusTimer = 0;
    g.buildPending = E_NONE; view.wallDragX = 0; view.wallDragY = 0;
    g.dayPhase = 0.25f; g.seasonPhase = 0.0f; g.prevSeason = -1;
    g.prevTimePhase = 0; g.attackNotifyCd = 0;
    g.weather = W_CLEAR; g.weatherTimer = 0;
    g.returnToMenu = false;
    g.seed = seed;
    g.startupAIs = numAIs;
    g.humanCorner = -1;
    g.matchNumber = matchNumber;
    g.diagnostics = std::getenv("REALM_DIAGNOSTICS") != nullptr;
    g.helpOverlay = false;
    // biomeChoice is set by showSplash before initGame is called; don't reset it here
    // unless the caller provides an explicit map-generation config.
    g.biomeChoice = (mapConfig.biomeChoice >= -1 && mapConfig.biomeChoice <= B_OCEAN)
        ? mapConfig.biomeChoice
        : -1;
    for (int p = 0; p < MAX_PLAYERS; p++)
        g.players[p] = {300, 200, 100, 0, 0, true, 0, 0};
    g.players[OWNER_NATURE] = {0, 0, 0, 0, 0, true, 0, 0};

    generateMap(mapConfig);

    struct Spawn { int thX, thY; };
    const int needed = std::min(MAX_PLAYERS, 1 + numAIs);
    const int minSpawnDist = std::min(MAP_W, MAP_H) * 2 / 3;
    const int edge = 12;

    auto scoreSpawn = [](int cx, int cy) -> int {
        for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) return -1;
            Terrain t = g.map[y][x].terrain;
            if (t==T_WATER||t==T_MOUNTAIN||t==T_LAVA||t==T_SHALLOWS||t==T_GOLD) return -1;
        }
        int score = 100;
        int grass = 0;
        for (int dy = -4; dy <= 4; dy++) for (int dx = -4; dx <= 4; dx++) {
            int x = cx+dx, y = cy+dy;
            if (!inBounds(x,y)) continue;
            Terrain t = g.map[y][x].terrain;
            if (t==T_GRASS||t==T_MEADOW||t==T_DIRT||t==T_TALL_GRASS) grass++;
        }
        score += grass;
        bool hasWood = false;
        for (int dy = -10; dy <= 10 && !hasWood; dy++)
            for (int dx = -10; dx <= 10 && !hasWood; dx++) {
                int x = cx+dx, y = cy+dy;
                if (!inBounds(x,y)) continue;
                Terrain t = g.map[y][x].terrain;
                if (t==T_FOREST||t==T_PINE||t==T_PALM||t==T_DEAD_TREE) hasWood = true;
            }
        if (hasWood) score += 40; else score -= 30;
        return score;
    };

    struct Cand { int x, y, score; };
    std::vector<Cand> candidates;
    candidates.reserve(260);
    for (int i = 0; i < 260; i++) {
        int cx = edge + realmRand() % (MAP_W - 2*edge);
        int cy = edge + realmRand() % (MAP_H - 2*edge);
        int s = scoreSpawn(cx, cy);
        if (s > 0) candidates.push_back({cx, cy, s});
    }
    std::sort(candidates.begin(), candidates.end(),
              [](const Cand& a, const Cand& b){ return a.score > b.score; });

    std::vector<Spawn> spawns;
    for (auto& c : candidates) {
        if ((int)spawns.size() >= needed) break;
        bool ok = true;
        for (auto& s : spawns) {
            if (dist(c.x, c.y, s.thX, s.thY) < minSpawnDist) { ok = false; break; }
        }
        if (ok) spawns.push_back({c.x, c.y});
    }
    if ((int)spawns.size() < needed) {
        int relaxed = minSpawnDist / 2;
        for (auto& c : candidates) {
            if ((int)spawns.size() >= needed) break;
            bool duplicate = false;
            for (auto& s : spawns) if (s.thX==c.x && s.thY==c.y) { duplicate = true; break; }
            if (duplicate) continue;
            bool ok = true;
            for (auto& s : spawns) if (dist(c.x, c.y, s.thX, s.thY) < relaxed) { ok = false; break; }
            if (ok) spawns.push_back({c.x, c.y});
        }
    }
    const int cornerAnchors[4][2] = {
        {5, 5}, {MAP_W-9, 5}, {5, MAP_H-9}, {MAP_W-9, MAP_H-9}
    };
    if (spawns.empty()) {
        for (int i = 0; i < needed; i++) spawns.push_back({cornerAnchors[i][0], cornerAnchors[i][1]});
    }
    if (humanCorner < 0 || humanCorner >= 4) {
        humanCorner = realmRand() % 4;
        if (spawns.size() > 1) std::swap(spawns[0], spawns[realmRand() % spawns.size()]);
    } else if (spawns.size() > 1) {
        int best = 0;
        for (int i = 1; i < (int)spawns.size(); i++) {
            if (dist(spawns[i].thX, spawns[i].thY, cornerAnchors[humanCorner][0], cornerAnchors[humanCorner][1])
                < dist(spawns[best].thX, spawns[best].thY, cornerAnchors[humanCorner][0], cornerAnchors[humanCorner][1]))
                best = i;
        }
        std::swap(spawns[0], spawns[best]);
    }
    g.humanCorner = humanCorner;

    bool spawned[MAX_PLAYERS] = {false};
    for (int i = 0; i < (int)spawns.size() && i <= numAIs; i++) {
        int owner = (i == 0) ? 0 : i;
        if (owner >= MAX_PLAYERS) break;
        spawned[owner] = true;
        clearStartArea(spawns[i].thX - 2, spawns[i].thY - 2, 6);
        placeGoldCluster(spawns[i].thX + 9, spawns[i].thY + 4, 5);
        placeStartResources(spawns[i].thX, spawns[i].thY);
        spawnEntity(E_TOWNHALL, owner, spawns[i].thX, spawns[i].thY);
        for (int j = 0; j < 4; j++)
            spawnEntity(E_PEASANT, owner, spawns[i].thX + 4 + j, spawns[i].thY + 4);
    }
    // Mark any non-spawned slots dead so checkWin doesn't wait on them.
    for (int p = 1; p < MAX_PLAYERS; p++) if (!spawned[p]) g.players[p].alive = false;
    for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);

    view.cursorX = spawns[0].thX + 2; view.cursorY = spawns[0].thY + 2;
    view.viewX = std::max(0, spawns[0].thX - 10); view.viewY = std::max(0, spawns[0].thY - 5);

    auto farFromAnyBase = [](int ax, int ay, int radius) {
        for (auto& e : g.entities) {
            if (!e.alive) continue;
            if (e.type != E_TOWNHALL && e.type != E_CASTLE) continue;
            if (std::abs(ax - e.x) <= radius && std::abs(ay - e.y) <= radius) return false;
        }
        return true;
    };

    // Wild deer in herds of 3-6, each herd anchored to a random open spot.
    {
        int total = 0;
        for (int h = 0; h < 10 && total < 42; h++) {
            int hx = -1, hy = -1;
            for (int t = 0; t < 300 && hx < 0; t++) {
                int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && farFromAnyBase(ax, ay, 14))
                    { hx=ax; hy=ay; }
            }
            if (hx < 0) continue;
            int herdSize = 3 + realmRand()%4;
            for (int i = 0, t = 0; i < herdSize && t < 100; t++) {
                int ax = hx+(realmRand()%9)-4, ay = hy+(realmRand()%9)-4;
                ax = std::max(1, std::min(ax, MAP_W-2));
                ay = std::max(1, std::min(ay, MAP_H-2));
                Terrain tr = g.map[ay][ax].terrain;
                if ((tr==T_GRASS||tr==T_MEADOW||tr==T_TALL_GRASS||tr==T_FOREST)
                    && !entityAt(ax,ay) && farFromAnyBase(ax, ay, 10))
                    { spawnEntity(E_DEER, OWNER_NATURE, ax, ay); i++; total++; }
            }
        }
    }
    // Wolves in forested areas
    for (int i = 0, t = 0; i < 7 && t < 600; t++) {
        int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_WOLF, OWNER_NATURE, ax, ay); i++; }
    }
    // Boars in temperate woodland and forest biomes
    for (int i = 0, t = 0; i < 18 && t < 800; t++) {
        int ax = 10 + realmRand()%(MAP_W-20), ay = 10 + realmRand()%(MAP_H-20);
        Terrain tr = g.map[ay][ax].terrain;
        Biome  b  = g.map[ay][ax].biome;
        if ((tr==T_FOREST||tr==T_PINE||tr==T_TALL_GRASS||tr==T_GRASS)
            && (b==B_TEMPERATE||b==B_FOREST) && !entityAt(ax,ay)
            && farFromAnyBase(ax, ay, 16))
            { spawnEntity(E_BOAR, OWNER_NATURE, ax, ay); i++; }
    }
    // Domestic sheep near each player's town hall (one cluster per chosen spawn)
    for (int i = 0; i < (int)spawns.size() && i <= numAIs; i++) {
        int bx = spawns[i].thX + 4, by = spawns[i].thY + 4;
        for (int i = 0, t = 0; i < 4 && t < 200; t++) {
            int ax = bx+(realmRand()%7)-3, ay = by+(realmRand()%7)-3;
            ax = std::max(1, std::min(ax, MAP_W-2)); ay = std::max(1, std::min(ay, MAP_H-2));
            if (isPassable(ax,ay) && !entityAt(ax,ay)) { spawnEntity(E_SHEEP, OWNER_NATURE, ax, ay); i++; }
        }
    }

    updateFog();
    resetDetectMapCache();
    std::cerr << "realm: match start"
              << " match=" << g.matchNumber
              << " seed=" << g.seed
              << " ai=" << g.startupAIs
              << " humanCorner=" << g.humanCorner
              << " biome=" << g.biomeChoice
              << " entities=" << g.entities.size()
              << " projectiles=" << g.projectiles.size() << "\n";
}
