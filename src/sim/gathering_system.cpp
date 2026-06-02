#include "realm.h"

Cargo emptyCargo() {
    return {CR_NONE, 0, -1, -1};
}

int carcassFoodForAnimal(EntityType type) {
    switch (type) {
        case E_SHEEP: return 80;
        case E_DEER: return 120;
        case E_BOAR: return 100;
        case E_WOLF: return 0;
        default: return 0;
    }
}

bool isHarvestableCarcass(const Entity& e) {
    return !e.alive && e.state == S_DEAD
        && (e.type == E_DEER || e.type == E_SHEEP || e.type == E_BOAR)
        && e.deathTicks < DEATH_DECAY_TICKS
        && e.carcassFoodRemaining > 0;
}

const char* cargoResourceName(CargoResource r) {
    switch (r) {
        case CR_GOLD: return "gold";
        case CR_WOOD: return "wood";
        case CR_FOOD: return "food";
        case CR_FISH: return "fish";
        case CR_NONE: return "nothing";
    }
    return "unknown";
}

CargoResource resourceForTerrain(Terrain t) {
    return terrainDef(t).resource;
}

bool terrainMatchesResource(Terrain t, CargoResource r) {
    return resourceForTerrain(t) == r;
}
