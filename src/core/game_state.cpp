#include "realm.h"

Game g;

int normalizePlayerColorHue(int hue) {
    hue %= 360;
    if (hue < 0) hue += 360;
    return hue;
}

int playerColorHueForOwner(int humanHue, int numAIs, int owner) {
    humanHue = normalizePlayerColorHue(humanHue);
    if (owner <= 0) return humanHue;

    int activePlayers = std::max(2, std::min(MAX_PLAYERS, 1 + numAIs));
    if (owner < activePlayers) {
        int step = (int)std::lround(360.0 / activePlayers);
        return normalizePlayerColorHue(humanHue + step * owner);
    }

    int fallbackStep = (int)std::lround(360.0 / MAX_PLAYERS);
    return normalizePlayerColorHue(humanHue + fallbackStep * owner);
}

void setHumanPlayerColorHue(Game& game, int hue) {
    game.playerColorHue[0] = normalizePlayerColorHue(hue);
}

void configurePlayerColorHues(Game& game, int numAIs) {
    int humanHue = normalizePlayerColorHue(game.playerColorHue[0]);
    game.playerColorHue[0] = humanHue;
    for (int owner = 1; owner < MAX_PLAYERS; ++owner) {
        game.playerColorHue[owner] = playerColorHueForOwner(humanHue, numAIs, owner);
    }
}

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
    e.visualMoveFromX = x; e.visualMoveFromY = y;
    e.visualMoveToX = x; e.visualMoveToY = y;
    e.visualMoveStartedTick = 0;
    e.visualMoveDurationTicks = 0;
    e.visualMoveSeq = 0;
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
