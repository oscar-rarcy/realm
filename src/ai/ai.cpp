#include "realm.h"
#include "ai/ai.h"
#include "core/game_events.h"
#include "core/world_index.h"

#include <algorithm>
#include <vector>

const AITuning& defaultAITuning() {
    static const AITuning tuning{};
    return tuning;
}

static int indexedAITypeCount(Game& game, const WorldIndex& world, int owner, EntityType type, bool includeUnderConstruction) {
    if (owner < 0 || owner >= MAX_PLAYERS) return 0;
    int count = 0;
    for (EntityId id : world.entitiesByOwner[owner]) {
        const Entity* entity = entityById(game, world, id);
        if (entity && entity->type == type && (includeUnderConstruction || !entity->underConstruction)) count++;
    }
    return count;
}

AIWorldView buildAIWorldView(Game& game, const WorldIndex& world, int o, const AITuning& tuning) {
    AIWorldView view{};
    view.peas = indexedAITypeCount(game, world, o, E_PEASANT, false);
    view.mil = indexedAITypeCount(game, world, o, E_MILITIA, false);
    view.arch = indexedAITypeCount(game, world, o, E_ARCHER, false);
    view.kni = indexedAITypeCount(game, world, o, E_KNIGHT, false);
    view.spr = indexedAITypeCount(game, world, o, E_SPEARMAN, false);
    view.cat = indexedAITypeCount(game, world, o, E_CATAPULT, true);
    view.treb = indexedAITypeCount(game, world, o, E_TREBUCHET, true);
    view.hous = indexedAITypeCount(game, world, o, E_HOUSE, true);
    view.bar = indexedAITypeCount(game, world, o, E_BARRACKS, false);
    view.stb = indexedAITypeCount(game, world, o, E_STABLE, false);
    view.intel = aiScout(game, world, o);
    view.peasCap = std::max(tuning.minPeasantCap, view.intel.playerPeasants + tuning.earlyPeasantLead);
    if (game.tick > tuning.latePeasantCapTick) view.peasCap = std::min(view.peasCap, tuning.latePeasantCap);
    if (game.tick > tuning.finalPeasantCapTick) view.peasCap = std::min(view.peasCap, tuning.finalPeasantCap);
    view.milCap = std::max(tuning.minMilitiaCap, view.intel.playerArmy + tuning.militiaLead);
    view.archCap = std::max(tuning.minArcherCap, view.intel.playerArmy/2 + 3);
    view.kniCap = std::max(tuning.minKnightCap, view.intel.playerArmy/3 + 2);
    view.towerCap = (view.intel.playerArmy >= tuning.towerThreatArmy || view.intel.playerCastles > 0) ? tuning.towerCapThreatened : tuning.towerCapPeace;
    return view;
}

void tickAIForOwner(Game& game, int owner) {
    const AITuning& tuning = defaultAITuning();
    WorldIndex world = buildWorldIndex(game);
    AIWorldView view = buildAIWorldView(game, world, owner, tuning);
    GameContext gameContext{ game, world, gameEvents() };
    AIContext aiContext{ owner, gameContext, view, tuning, {}, {} };

    aiGather(aiContext);
    runAIEconomy(aiContext);
    runAIProduction(aiContext);
    runAIDefenseInfrastructure(aiContext);
    runAIFoodEconomy(aiContext);
    runAINaval(aiContext);
    runAIExpansion(aiContext);
    runAIAttackAndDefense(aiContext);
    executeAICommands(aiContext);
}

void tickAI(Game& game) {
    game.aiTimer++;
    if (game.aiTimer < defaultAITuning().aiThinkIntervalTicks) return;
    game.aiTimer = 0;
    for (int o = 1; o < MAX_PLAYERS; o++) {
        if (!game.players[o].alive) continue;
        tickAIForOwner(game, o);
    }
}
