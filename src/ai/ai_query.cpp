#include "realm.h"
#include "ai/ai.h"
#include "core/world_index.h"

#include <cmath>

// ============================================================
// AI — economy + military build-out, target picking, wave dispatch
// ============================================================
static int countTypeInIndex(Game& game, const WorldIndex& world, int owner, EntityType type, bool includeUnderConstruction) {
    if (owner < 0 || owner >= MAX_PLAYERS) return 0;
    int count = 0;
    for (EntityId id : world.entitiesByOwner[owner]) {
        const Entity* entity = entityById(game, world, id);
        if (entity && entity->type == type && (includeUnderConstruction || !entity->underConstruction)) count++;
    }
    return count;
}

static bool validAiOwner(int owner) {
    return owner >= 0 && owner < MAX_PLAYERS;
}

static bool aiEnemyOwner(int owner, int candidateOwner) {
    return candidateOwner >= 0 && candidateOwner < MAX_PLAYERS && candidateOwner != owner;
}

template <typename Fn>
static void forEachEnemyEntity(Game& game, const WorldIndex& world, int owner, Fn&& fn) {
    for (int other = 0; other < MAX_PLAYERS; other++) {
        if (!aiEnemyOwner(owner, other)) continue;
        for (EntityId id : world.entitiesByOwner[other]) {
            Entity* entity = entityById(game, world, id);
            if (entity) fn(*entity);
        }
    }
}

int aiCount(AIContext& context, EntityType t) {
    return countTypeInIndex(context.ctx.game, context.ctx.world, context.owner, t, false);
}

int aiCountAll(AIContext& context, EntityType t) {
    return countTypeInIndex(context.ctx.game, context.ctx.world, context.owner, t, true);
}

Entity* aiBldg(AIContext& context, EntityType t) {
    int o = context.owner;
    if (!validAiOwner(o)) return nullptr;
    for (EntityId id : context.ctx.world.buildingsByOwner[o]) {
        Entity* entity = entityById(context.ctx.game, context.ctx.world, id);
        if (entity && entity->type == t && !entity->underConstruction) return entity;
    }
    return nullptr;
}

bool aiCanAffordEntity(AIContext& context, EntityType type) {
    if (!validAiOwner(context.owner)) return false;
    const Player& player = context.ctx.game.players[context.owner];
    const EntityStats& stats = STATS[type];
    return player.gold >= stats.costGold
        && player.wood >= stats.costWood
        && player.food >= stats.costFood;
}

// Worker selection for AI construction. Prefers idle peasants; if none exist,
// pulls one off gathering/returning so the AI doesn't deadlock at 10/10
// population because aiGather() consumed every idle worker before the build
// pass ran.
Entity* aiWorker(AIContext& context) {
    int o = context.owner;
    if (!validAiOwner(o)) return nullptr;
    for (EntityId id : context.ctx.world.unitsByOwner[o]) {
        Entity* e = entityById(context.ctx.game, context.ctx.world, id);
        if (e && isWorker(e->type) && e->state == S_IDLE && !e->underConstruction) return e;
    }
    for (EntityId id : context.ctx.world.unitsByOwner[o]) {
        Entity* e = entityById(context.ctx.game, context.ctx.world, id);
        if (e && isWorker(e->type)
            && !e->underConstruction
            && (e->state == S_GATHERING || e->state == S_RETURNING)) return e;
    }
    return nullptr;
}

Entity* aiIdlePeasant(AIContext& context) {
    int o = context.owner;
    if (!validAiOwner(o)) return nullptr;
    for (EntityId id : context.ctx.world.unitsByOwner[o]) {
        Entity* e = entityById(context.ctx.game, context.ctx.world, id);
        if (e && isWorker(e->type) && e->state == S_IDLE && !e->underConstruction) return e;
    }
    return nullptr;
}

static void considerResourceTarget(const Entity& gatherer, const std::vector<ResourceTile>& resources,
                                   int& bestD, int& bx, int& by) {
    for (const ResourceTile& resource : resources) {
        int d = mdist(gatherer.x, gatherer.y, resource.pos.x, resource.pos.y);
        if (d < bestD) {
            bestD = d;
            bx = resource.pos.x;
            by = resource.pos.y;
        }
    }
}

void aiGather(AIContext& context) {
    const int o = context.owner;
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    for (EntityId id : world.unitsByOwner[o]) {
        Entity* entity = entityById(context.ctx.game, world, id);
        if (!entity || !canGather(entity->type) || entity->state != S_IDLE) continue;
        Entity& e = *entity;
        int bestD = 9999, bx = -1, by = -1;
        if (isNaval(e.type)) {
            considerResourceTarget(e, world.resources.fish, bestD, bx, by);
        } else {
            considerResourceTarget(e, world.resources.gold, bestD, bx, by);
            considerResourceTarget(e, world.resources.wood, bestD, bx, by);
            considerResourceTarget(e, world.resources.food, bestD, bx, by);
        }
        if (bx >= 0) aiIssueGather(context, e, bx, by);
    }
}

// Build placement: try near a given centre. Prefer positions with breathing
// room from existing owned buildings so the base spreads instead of clumping.
void aiBuildSpotNear(AIContext& context, EntityType bt, int cx, int cy, int& ox, int& oy) {
    int o = context.owner;
    Game& game = context.ctx.game;
    WorldIndex& world = context.ctx.world;
    if (cx < 0 || cy < 0) {
        Entity* th = aiBldg(context, E_TOWNHALL);
        if (!th) th = aiBldg(context, E_CASTLE);
        if (!th) return;
        cx = th->x; cy = th->y;
    }
    for (int r = 4; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (realmRand(game)%(r*2+1)) - r, by = cy + (realmRand(game)%(r*2+1)) - r;
        if (!canPlace(game, world, bt, bx, by, o)) continue;
        bool tooClose = false;
        for (EntityId id : world.buildingsByOwner[o]) {
            Entity* e = entityById(game, world, id);
            if (!e) continue;
            if (dist(e->x, e->y, bx, by) < 3) { tooClose = true; break; }
        }
        if (!tooClose) { ox = bx; oy = by; return; }
    }
    for (int r = 2; r < 22; r++) for (int a = 0; a < 28; a++) {
        int bx = cx + (realmRand(game)%(r*2+1)) - r, by = cy + (realmRand(game)%(r*2+1)) - r;
        if (canPlace(game, world, bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}

void aiBuildSpot(AIContext& context, EntityType bt, int& ox, int& oy) {
    aiBuildSpotNear(context, bt, -1, -1, ox, oy);
}

void aiBuildSpotWide(AIContext& context, EntityType bt, int& ox, int& oy) {
    int o = context.owner;
    Game& game = context.ctx.game;
    WorldIndex& world = context.ctx.world;
    Entity* th = aiBldg(context, E_TOWNHALL);
    if (!th) th = aiBldg(context, E_CASTLE);
    if (!th) return;
    for (int r = 5; r < 28; r++) for (int a = 0; a < 32; a++) {
        int bx = th->x + (realmRand(game)%(r*2+1)) - r, by = th->y + (realmRand(game)%(r*2+1)) - r;
        if (canPlace(game, world, bt, bx, by, o)) { ox = bx; oy = by; return; }
    }
}

// Scan player state — used to scale production and pick targets.
AIIntel aiScout(Game& game, const WorldIndex& world, int o) {
    AIIntel x{};
    forEachEnemyEntity(game, world, o, [&](Entity& e) {
        if (e.state == S_GARRISONED) return;
        if (isWorker(e.type)) x.playerPeasants++;
        else if (e.type == E_CATAPULT || e.type == E_TREBUCHET) { x.playerCatapults++; x.playerArmy++; }
        else if (isUnit(e.type)) x.playerArmy++;
        else if (e.type == E_CASTLE)  {
            x.playerCastles++;
            if (!x.playerTownCenterId) {
                x.playerTownCenterId = e.id;
                x.playerTownCenterPos = MapPos{ e.x, e.y };
            }
        }
        else if (e.type == E_WALL)    x.playerWalls++;
        else if (e.type == E_TOWNHALL && !x.playerTownCenterId) {
            x.playerTownCenterId = e.id;
            x.playerTownCenterPos = MapPos{ e.x, e.y };
        }
    });
    return x;
}

static Entity* aiNearestEnemyBuilding(Game& game, const WorldIndex& world, int o, int x, int y) {
    Entity* best = nullptr; int bestD = 99999;
    forEachEnemyEntity(game, world, o, [&](Entity& e) {
        if (!isBuilding(e.type)) return;
        int d = dist(x, y, e.x, e.y);
        if (d < bestD) { bestD = d; best = &e; }
    });
    return best;
}

static bool findShoreTileNear(const Game& game, int cx, int cy, int searchR, int& ox, int& oy) {
    for (int r = 1; r <= searchR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            int nx = cx+dx, ny = cy+dy;
            if (!inBounds(nx,ny) || !isPassableWater(game,nx,ny)) continue;
            for (int dy2 = -1; dy2 <= 1; dy2++) for (int dx2 = -1; dx2 <= 1; dx2++) {
                if (dx2 == 0 && dy2 == 0) continue;
                int qx = nx+dx2, qy = ny+dy2;
                if (inBounds(qx,qy) && isPassable(game,qx,qy)) { ox = nx; oy = ny; return true; }
            }
        }
    }
    return false;
}

void aiTickTrebuchets(AIContext& context) {
    const int o = context.owner;
    int rng = STATS[E_TREBUCHET].range;
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    for (EntityId id : world.unitsByOwner[o]) {
        Entity* entity = entityById(context.ctx.game, world, id);
        if (!entity || entity->type != E_TREBUCHET || entity->underConstruction) continue;
        Entity& t = *entity;
        if (t.packTicks > 0) continue;
        Entity* target = aiNearestEnemyBuilding(context.ctx.game, world, o, t.x, t.y);
        if (t.packed) {
            if (!target) continue;
            if (dist(t.x, t.y, target->x, target->y) <= rng) {
                aiIssueToggleTrebuchetPacked(context, t);
            } else if (t.state == S_IDLE || t.path.empty()) {
                aiIssueMove(context, t, target->x, target->y);
            }
        } else {
            if (t.hp * 100 < t.maxHp * 25 || !target || dist(t.x, t.y, target->x, target->y) > rng) {
                aiIssueToggleTrebuchetPacked(context, t);
                continue;
            }
            if (t.state == S_IDLE || t.targetId != target->id) aiIssueAttack(context, t, target->id);
        }
    }
}

void aiTickTransports(AIContext& context) {
    const int o = context.owner;
    Entity* target = nullptr;
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    forEachEnemyEntity(context.ctx.game, world, o, [&](Entity& e) {
        if (target) return;
        if (e.type == E_TOWNHALL || e.type == E_CASTLE) target = &e;
    });
    Entity* home = aiBldg(context, E_TOWNHALL);
    if (!home) home = aiBldg(context, E_CASTLE);
    if (!target || !home) return;
    int ex=-1, ey=-1, hx=-1, hy=-1;
    if (!findShoreTileNear(context.ctx.game, target->x, target->y, 18, ex, ey)) return;
    if (!findShoreTileNear(context.ctx.game, home->x, home->y, 18, hx, hy)) return;
    for (EntityId transportId : world.unitsByOwner[o]) {
        Entity* transport = entityById(context.ctx.game, world, transportId);
        if (!transport || transport->type != E_TRANSPORT || transport->underConstruction) continue;
        Entity& t = *transport;
        bool atHome = dist(t.x, t.y, hx, hy) <= 3;
        bool atEnemy = dist(t.x, t.y, ex, ey) <= 3;
        if (t.garrison.empty()) {
            if (atHome) {
                int free = garrisonCap(E_TRANSPORT);
                Entity* peasant = aiIdlePeasant(context);
                if (peasant && mdist(peasant->x, peasant->y, t.x, t.y) <= 12) { aiIssueGarrison(context, *peasant, t.id); free--; }
                for (EntityId unitId : world.unitsByOwner[o]) {
                    if (free <= 0) break;
                    Entity* unit = entityById(context.ctx.game, world, unitId);
                    if (!unit || unit->state != S_IDLE) continue;
                    Entity& u = *unit;
                    if (!isUnit(u.type) || u.type == E_PEASANT || isNaval(u.type) || u.type == E_TREBUCHET) continue;
                    if (mdist(u.x, u.y, t.x, t.y) <= 12) { aiIssueGarrison(context, u, t.id); free--; }
                }
            } else if (t.state == S_IDLE || t.path.empty()) {
                aiIssueMove(context, t, hx, hy);
            }
        } else {
            if (atEnemy) aiIssueEjectGarrison(context, t);
            else if (t.state == S_IDLE || t.path.empty()) aiIssueMove(context, t, ex, ey);
        }
    }
}

static int aiPickTargetIndexed(Game& game, const WorldIndex& world, int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    forEachEnemyEntity(game, world, o, [&](Entity& e) {
        if (e.state == S_GARRISONED) return;
        int score = 0;
        if      (e.type == E_CATAPULT || e.type == E_TREBUCHET)     score += 270;
        else if (isWorker(e.type))                                  score += 220;
        else if (isRanged(e.type) || isSiege(e.type))               score += 180;
        else if (isUnit(e.type))                                     score += 110;
        else if (e.type == E_TOWER)                                  score += 140;
        else if (e.type == E_BARRACKS || e.type == E_STABLE)         score += 90;
        else if (e.type == E_FARM || e.type == E_MILL)               score += 70;
        else if (e.type == E_TOWNHALL || e.type == E_CASTLE)         score += 50;
        else if (isBuilding(e.type))                                 score += 25;
        int missing = e.maxHp - e.hp;
        score += missing / 3;            // finish wounded targets
        int d = dist(attacker->x, attacker->y, e.x, e.y);
        score -= d / 2;                  // prefer closer
        if (score > bestScore) { bestScore = score; best = &e; }
    });
    return best ? best->id : -1;
}

// Pick a target worth attacking from `attacker`'s position.
// Priorities: peasants (raid), wounded enemies, towers, key buildings.
int aiPickTarget(AIContext& context, Entity* attacker) {
    return aiPickTargetIndexed(context.ctx.game, context.ctx.world, context.owner, attacker);
}

static int aiPickSiegeTargetIndexed(Game& game, const WorldIndex& world, int o, Entity* attacker) {
    Entity* best = nullptr; int bestScore = -999999;
    forEachEnemyEntity(game, world, o, [&](Entity& e) {
        if (!isBuilding(e.type) && e.type != E_CATAPULT && e.type != E_TREBUCHET) return;
        int score = 0;
        if      (e.type == E_TOWNHALL || e.type == E_CASTLE) score += 300;
        else if (e.type == E_TOWER)                          score += 220;
        else if (e.type == E_BARRACKS || e.type == E_STABLE) score += 160;
        else if (e.type == E_DOCK)                           score += 130;
        else                                                 score += 60;
        score += (e.maxHp - e.hp) / 2;
        score -= dist(attacker->x, attacker->y, e.x, e.y);
        if (score > bestScore) { bestScore = score; best = &e; }
    });
    return best ? best->id : -1;
}

int aiPickSiegeTarget(AIContext& context, Entity* attacker) {
    return aiPickSiegeTargetIndexed(context.ctx.game, context.ctx.world, context.owner, attacker);
}
