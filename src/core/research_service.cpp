#include "research_service.h"

CanResearchResult canResearch(const Game& game, int player, const Entity& building, ResearchId id) {
    const ResearchDef* def = researchDef(id);
    if (!def) return {false, "Unknown research."};
    if (building.type != def->requiredBuilding) return {false, "Wrong building."};
    if (building.underConstruction) return {false, "Building not complete."};
    if (building.owner != player) return {false, "Not your building."};

    const Player& p = game.players[player];
    if (p.research & def->bit) return {false, "Already researched."};
    if (building.researching != 0) return {false, "Already researching."};

    if (def->requiredOwnedBuilding != E_NONE) {
        bool has = false;
        for (const auto& e : game.entities) {
            if (e.alive && e.owner == player && e.type == def->requiredOwnedBuilding
                && !e.underConstruction) { has = true; break; }
        }
        if (!has) return {false, "Requires a Castle."};
    }

    if (p.gold < def->costGold || p.wood < def->costWood)
        return {false, "Not enough resources!"};

    return {true, nullptr};
}

bool startResearch(Game& game, int player, int buildingId, ResearchId id) {
    Entity* building = findEntity(buildingId);
    if (!building) return false;

    CanResearchResult result = canResearch(game, player, *building, id);
    if (!result.ok) {
        if (player == 0) setStatus(result.reason);
        return false;
    }

    const ResearchDef* def = researchDef(id);
    Player& p = game.players[player];
    p.gold -= def->costGold;
    p.wood -= def->costWood;
    building->researching = def->bit;
    building->researchProgress = 0;
    building->researchTime = def->ticks;
    if (player == 0) setStatus(def->startMessage);
    return true;
}
