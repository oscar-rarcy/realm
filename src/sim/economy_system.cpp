#include "realm.h"

void updateSupply(Game& game, int owner) {
    int mx = 0, used = 0;
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != owner) continue;
        if (!e.underConstruction) mx += STATS[e.type].supplyProvided;
        used += STATS[e.type].supplyUsed;
    }
    game.players[owner].supplyMax = mx;
    game.players[owner].supply    = used;
}

// Supply already in use plus everything currently producing or queued. Used by
// orderTrain so a flurry of queued units can't push live supply over the cap
// once they all spawn.
int reservedSupply(const Game& game, int owner) {
    int used = 0;
    for (auto& e : game.entities) {
        if (!e.alive || e.owner != owner) continue;
        used += STATS[e.type].supplyUsed;
        if (isBuilding(e.type) && !e.underConstruction) {
            if (e.producing != E_NONE) used += STATS[e.producing].supplyUsed;
            for (int q : e.queue) used += STATS[(EntityType)q].supplyUsed;
        }
    }
    return used;
}
