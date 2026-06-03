#include "realm.h"

Game g;

int spawnEntity(Game& game, EntityType type, int owner, int x, int y, bool built) {
    Entity e{};
    e.id = game.nextId++; e.type = type; e.owner = owner; e.x = x; e.y = y;
    e.maxHp = STATS[type].maxHp; e.hp = built ? e.maxHp : 1;
    e.state = S_IDLE; e.targetId = -1; e.targetX = -1; e.targetY = -1;
    e.producing = E_NONE; e.underConstruction = !built; e.alive = true;
    e.rallyX = x + STATS[type].sizeW; e.rallyY = y + STATS[type].sizeH;
    e.resourceX = -1; e.resourceY = -1;
    e.deathTicks = 0;
    e.carcassFoodMax = carcassFoodForAnimal(type);
    e.carcassFoodRemaining = e.carcassFoodMax;
    e.facingDx = 1; e.facingDy = 0;
    e.convertTicks = 0;
    e.retreating = 0;
    e.packed = (type == E_TREBUCHET) ? 1 : 0;
    e.packTicks = 0;
    e.cargo = emptyCargo();
    if (type == E_FISHING_BOAT) e.cargo.type = CR_FISH;
    game.entities.push_back(e);
    updateSupply(game, owner);
    return e.id;
}
void tickActionMarkers(Game& game) {
    for (auto& m : game.actionMarkers) if (m.ticks > 0) m.ticks--;
    game.actionMarkers.erase(std::remove_if(game.actionMarkers.begin(), game.actionMarkers.end(),
        [](const ActionMarker& m){ return m.ticks <= 0; }), game.actionMarkers.end());
}
