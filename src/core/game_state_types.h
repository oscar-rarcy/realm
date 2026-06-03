#pragma once

#include "core/game_types.h"

#include <deque>
#include <utility>
#include <vector>

struct Projectile {
    ProjectileType type = PT_ARROW;
    float x, y, tx, ty;
    char glyph;
    int color, life;
    bool alive;
};

struct Cargo {
    CargoResource type;
    int amount;
    int sourceX, sourceY;
};

struct Tile {
    Terrain terrain;
    int resources;
    bool visible[MAX_PLAYERS], explored[MAX_PLAYERS];
    Biome biome;
    Terrain preWinterTerrain;
    int wear;
};

struct Entity {
    int id;
    EntityType type;
    int owner, x, y, hp, maxHp;
    EntityState state;
    int targetId, targetX, targetY;
    std::vector<std::pair<int, int>> path;
    int pathIdx;
    int moveCd, atkCd, gatherCd;
    Cargo cargo;
    EntityType producing;
    int trainProgress, trainTime;
    int researchProgress, researchTime;
    bool underConstruction, alive;
    int rallyX, rallyY;
    int resourceX, resourceY;
    int storedFood;
    int stuckTicks;
    int alertTicks;
    int deathTicks;
    int carcassFoodRemaining;
    int carcassFoodMax;
    int rallySet;
    int researching;
    int attackMove;
    int holdPosition;
    int facingDx;
    int facingDy;
    bool gateOpen;
    bool gateLocked;
    int convertTicks;
    int retreating;
    int packed;
    int packTicks;
    std::vector<int> queue;
    std::vector<int> garrison;
    std::vector<std::pair<int, int>> waypoints;
    bool patrolMode;
};

struct Player {
    int gold, wood, food, supply, supplyMax;
    bool alive;
    int research;
    int aiWaveCd;
};

struct LocalGameState {
    int selectedId = -1;
    std::vector<int> selectedIds;
    bool groupAssignPending = false;
    EntityType buildPending = E_NONE;
    bool diagnostics = false;
    bool helpOverlay = false;
};

struct Game {
    Tile map[MAP_H][MAP_W];
    std::deque<Entity> entities;
    std::vector<Projectile> projectiles;
    int nextId;
    Player players[MAX_PLAYERS + 1];
    int tick;
    GameMode mode;
    std::vector<int> controlGroupsByOwner[MAX_PLAYERS][9];
    LocalGameState local;
    int winner, aiTimer, farmTimer, animalTimer;
    float dayPhase, seasonPhase;
    int prevSeason;
    int prevTimePhase;
    int attackNotifyCd;
    int weather;
    int weatherTimer;
    int biomeChoice;
    bool returnToMenu;
    unsigned seed;
    int startupAIs;
    int humanCorner;
    int matchNumber;
    unsigned rngState;
};
