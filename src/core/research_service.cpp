#include "research_service.h"
#include "core/entity_query.h"
#include "core/game_events.h"
#include "core/world_index.h"

CanResearchResult canResearch(const Game& game, int player, const Entity& building, ResearchId id) {
    WorldIndex world = buildWorldIndex(game);
    return canResearch(game, world, player, building, id);
}

CanResearchResult canResearch(const Game& game, const WorldIndex& world, int player, const Entity& building, ResearchId id) {
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
        if (player >= 0 && player <= MAX_PLAYERS) {
            for (EntityId id : world.buildingsByOwner[player]) {
                const Entity* e = entityById(game, world, id);
                if (e && e->type == def->requiredOwnedBuilding && !e->underConstruction) {
                    has = true;
                    break;
                }
            }
        }
        if (!has) return {false, "Requires a Castle."};
    }

    if (p.gold < def->costGold || p.wood < def->costWood)
        return {false, "Not enough resources!"};

    return {true, nullptr};
}

bool startResearch(Game& game, int player, int buildingId, ResearchId id) {
    return startResearchService(game, player, buildingId, id).ok;
}

ServiceResult startResearchService(Game& game, int player, int buildingId, ResearchId id) {
    WorldIndex world = buildWorldIndex(game);
    return startResearchService(game, world, player, buildingId, id);
}

ServiceResult startResearchService(Game& game, const WorldIndex& world, int player, int buildingId, ResearchId id) {
    Entity* building = findEntity(game, world, buildingId);
    if (!building) return { false, "Research building not found." };

    CanResearchResult result = canResearch(game, world, player, *building, id);
    if (!result.ok) {
        emitStatusEvent(player, result.reason, GameEventType::CommandRejected);
        return { false, result.reason };
    }

    const ResearchDef* def = researchDef(id);
    Player& p = game.players[player];
    p.gold -= def->costGold;
    p.wood -= def->costWood;
    building->researching = def->bit;
    building->researchProgress = 0;
    building->researchTime = def->ticks;
    emitStatusEvent(player, def->startMessage, GameEventType::ResearchStarted);
    return { true, nullptr };
}
