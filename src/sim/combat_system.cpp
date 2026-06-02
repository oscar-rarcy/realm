#include "realm.h"

// ============================================================
// Research-aware combat stats.
int unitAtk(const Entity& e) {
    return unitAtk(g, e);
}

int unitAtk(const Game& game, const Entity& e) {
    int a = STATS[e.type].atk;
    int r = game.players[e.owner].research;
    if ((e.type == E_MILITIA || e.type == E_KNIGHT) && (r & R_IRON_WEAPONS)) a += 2;
    if (e.type == E_MILITIA) {
        int adj = 0;
        for (auto& ally : game.entities) {
            if (!ally.alive || ally.id == e.id || ally.owner != e.owner || ally.type != E_MILITIA) continue;
            if (dist(e.x, e.y, ally.x, ally.y) <= 1 && ++adj >= 2) break;
        }
        a += adj;
    }
    return a;
}
// Building-damage multiplier: catapults are siege specialists, everyone else
// is bad at chewing through walls. Returns the damage actually applied.
int damageVs(EntityType attacker, EntityType target, int rawDmg, int targetOwner) {
    return damageVs(g, attacker, target, rawDmg, targetOwner);
}

int damageVs(const Game& game, EntityType attacker, EntityType target, int rawDmg, int targetOwner) {
    if ((target == E_WALL || target == E_GATE) && !isSiege(attacker)) return 0;
    if (attacker == E_SPEARMAN && target == E_KNIGHT) rawDmg += 14;
    if (target == E_KNIGHT && attacker != E_SPEARMAN && attacker != E_CATAPULT
        && attacker != E_TREBUCHET && attacker != E_WARSHIP && attacker != E_RAM) {
        bool plateHelm = targetOwner >= 0 && targetOwner < MAX_PLAYERS
            && (game.players[targetOwner].research & R_PLATE_HELM);
        rawDmg = (rawDmg * (plateHelm ? 60 : 75)) / 100;
        rawDmg = std::max(1, rawDmg);
    }
    if (!isBuilding(target)) {
        if (attacker == E_TREBUCHET) return std::max(1, rawDmg / 4);
        return rawDmg;
    }
    if (attacker == E_TREBUCHET) return rawDmg * 3;
    if (attacker == E_CATAPULT) return (rawDmg * 3) / 2; // 1.5x
    return std::max(1, rawDmg / 2);                       // 0.5x, floor 1
}
int unitRange(const Entity& e) {
    return unitRange(g, e);
}

int unitRange(const Game& game, const Entity& e) {
    int rng = STATS[e.type].range;
    int r = game.players[e.owner].research;
    if (e.type == E_ARCHER && (r & R_CROSSBOWS)) rng += 2;
    if (e.type == E_SPEARMAN && (r & R_PIKES)) rng += 1;
    return rng;
}

Entity* findNearestEnemy(Entity& e, int range) {
    return findNearestEnemy(g, e, range);
}

Entity* findNearestEnemy(Game& game, Entity& e, int range) {
    Entity* best = nullptr; int bestD = range + 1;
    bool concealing = isConcealing(game);
    for (auto& o : game.entities) {
        if (!o.alive || o.owner == e.owner) continue;
        if (o.state == S_GARRISONED) continue;
        // Non-nature units do not auto-attack nature entities (deer/wolf/sheep)
        if (e.owner != OWNER_NATURE && o.owner == OWNER_NATURE) continue;
        // Cloaking: if it's night/storm and no friendly eye is near the target, can't engage.
        if (concealing && o.owner != OWNER_NATURE && e.owner < MAX_PLAYERS
            && !isDetectedBy(game, o.x, o.y, e.owner)) continue;
        int d = dist(e.x, e.y, o.x, o.y);
        if (isConcealingTile(game, o.x, o.y) && d > 2) continue;
        if (d < bestD) { bestD = d; best = &o; }
    }
    return best;
}
