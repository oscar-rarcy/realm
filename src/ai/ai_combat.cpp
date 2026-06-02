#include "realm.h"
#include "ai/ai.h"
#include "core/world_index.h"

namespace {

bool aiCombatUnit(const Entity& entity, int owner) {
    return entity.alive && entity.owner == owner && entity.state == S_IDLE
        && isMilitary(entity.type) && !isWorker(entity.type)
        && !isNaval(entity.type) && entity.type != E_TREBUCHET;
}

bool aiIdleMilitaryUnit(const Entity& entity, int owner) {
    return entity.alive && entity.owner == owner && entity.state == S_IDLE
        && isUnit(entity.type) && isMilitary(entity.type);
}

template <typename Fn>
void forEachEnemyEntity(Game& game, const WorldIndex& world, int owner, Fn&& fn) {
    for (int other = 0; other < MAX_PLAYERS; other++) {
        if (other == owner) continue;
        for (EntityId id : world.entitiesByOwner[other]) {
            Entity* entity = entityById(game, world, id);
            if (entity) fn(*entity);
        }
    }
}

} // namespace

void runAIAttackAndDefense(int o, Player& p, const AIWorldView& view) {
    if (o < 0 || o > MAX_PLAYERS) return;
    WorldIndex world = buildWorldIndex(g);
    const AIIntel& intel = view.intel;
    const AITuning& tuning = defaultAITuning();
    // === ATTACK RHYTHM: send waves from idle military only ===
    if (p.aiWaveCd > 0) p.aiWaveCd--;
    int idleArmy = 0;
    Entity* anchor = nullptr;
    for (EntityId id : world.unitsByOwner[o]) {
        Entity* e = entityById(g, world, id);
        if (!e || !aiCombatUnit(*e, o)) continue;
        idleArmy++;
        if (!anchor) anchor = e;
    }
    bool lateGame = g.tick > tuning.lateGameTick;
    bool midGame = g.tick > tuning.midGameTick;
    int attackThreshold = (g.tick < tuning.attackGraceTicks)
        ? 999
        : (lateGame ? tuning.lateAttackThreshold
                    : (midGame ? tuning.midAttackThreshold : tuning.earlyAttackThreshold));
    int waveCooldown = lateGame ? tuning.lateWaveCooldown : tuning.midWaveCooldown;
    if (idleArmy >= attackThreshold && p.aiWaveCd == 0 && anchor) {
        int tid = aiPickTarget(o, anchor);
        int siegeId = aiPickSiegeTarget(o, anchor);
        if (tid < 0 && intel.playerTownCenterId) tid = *intel.playerTownCenterId;
        if (tid >= 0) {
            for (EntityId id : world.unitsByOwner[o]) {
                Entity* e = entityById(g, world, id);
                if (!e || !aiCombatUnit(*e, o)) continue;
                int myTarget = (e->type == E_CATAPULT && siegeId >= 0) ? siegeId : tid;
                Entity* target = entityById(g, world, myTarget);
                if (target) aiIssueAttackMove(*e, target->x, target->y);
                else aiIssueAttack(*e, myTarget);
            }
            p.aiWaveCd = waveCooldown;
        }
    }

    // === DEFENSE: respond to threats near any owned TH/Castle ===
    for (EntityId baseId : world.buildingsByOwner[o]) {
        Entity* base = entityById(g, world, baseId);
        if (!base || (base->type != E_TOWNHALL && base->type != E_CASTLE)) continue;
        bool handledThreat = false;
        forEachEnemyEntity(g, world, o, [&](Entity& en) {
            if (handledThreat) return;
            if (en.state == S_GARRISONED) return;
            if (dist(en.x, en.y, base->x, base->y) < tuning.defenseThreatRadius) {
                for (EntityId defenderId : world.unitsByOwner[o]) {
                    Entity* d = entityById(g, world, defenderId);
                    if (d && aiIdleMilitaryUnit(*d, o)) {
                        aiIssueAttackMove(*d, en.x, en.y);
                    }
                }
                handledThreat = true;
            }
        });
    }

    // Worker defense: if a peasant was hit, nearest idle military intercepts.
    for (EntityId workerId : world.unitsByOwner[o]) {
        Entity* worker = entityById(g, world, workerId);
        if (!worker || !isWorker(worker->type) || worker->alertTicks <= 0) continue;
        Entity* guard = nullptr; int bestD = 99999;
        for (EntityId unitId : world.unitsByOwner[o]) {
            Entity* u = entityById(g, world, unitId);
            if (!u || u->state != S_IDLE || !isMilitary(u->type) || isNaval(u->type)) continue;
            int d = mdist(u->x, u->y, worker->x, worker->y);
            if (d < bestD) { bestD = d; guard = u; }
        }
        Entity* threat = nullptr; int threatD = 99999;
        forEachEnemyEntity(g, world, o, [&](Entity& en) {
            int d = mdist(en.x,en.y,worker->x,worker->y);
            if (d < threatD) { threatD = d; threat = &en; }
        });
        if (guard && threat) aiIssueAttackMove(*guard, threat->x, threat->y);
        break;
    }

    aiTickTrebuchets(o);
    if (g.biomeChoice == B_OCEAN) aiTickTransports(o);
}
