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

AIWorldView buildAIWorldView(int o) {
    return buildAIWorldView(o, defaultAITuning());
}

static int indexedAITypeCount(const WorldIndex& world, int owner, EntityType type, bool includeUnderConstruction) {
    if (owner < 0 || owner > MAX_PLAYERS) return 0;
    int count = 0;
    for (EntityId id : world.entitiesByOwner[owner]) {
        const Entity* entity = entityById(g, world, id);
        if (entity && entity->type == type && (includeUnderConstruction || !entity->underConstruction)) count++;
    }
    return count;
}

AIWorldView buildAIWorldView(int o, const AITuning& tuning) {
    AIWorldView view{};
    WorldIndex world = buildWorldIndex(g);
    view.peas = indexedAITypeCount(world, o, E_PEASANT, false);
    view.mil = indexedAITypeCount(world, o, E_MILITIA, false);
    view.arch = indexedAITypeCount(world, o, E_ARCHER, false);
    view.kni = indexedAITypeCount(world, o, E_KNIGHT, false);
    view.spr = indexedAITypeCount(world, o, E_SPEARMAN, false);
    view.cat = indexedAITypeCount(world, o, E_CATAPULT, true);
    view.treb = indexedAITypeCount(world, o, E_TREBUCHET, true);
    view.hous = indexedAITypeCount(world, o, E_HOUSE, true);
    view.bar = indexedAITypeCount(world, o, E_BARRACKS, false);
    view.stb = indexedAITypeCount(world, o, E_STABLE, false);
    view.intel = aiScout(o);
    view.peasCap = std::max(tuning.minPeasantCap, view.intel.playerPeasants + tuning.earlyPeasantLead);
    if (g.tick > tuning.latePeasantCapTick) view.peasCap = std::min(view.peasCap, tuning.latePeasantCap);
    if (g.tick > tuning.finalPeasantCapTick) view.peasCap = std::min(view.peasCap, tuning.finalPeasantCap);
    view.milCap = std::max(tuning.minMilitiaCap, view.intel.playerArmy + tuning.militiaLead);
    view.archCap = std::max(tuning.minArcherCap, view.intel.playerArmy/2 + 3);
    view.kniCap = std::max(tuning.minKnightCap, view.intel.playerArmy/3 + 2);
    view.towerCap = (view.intel.playerArmy >= tuning.towerThreatArmy || view.intel.playerCastles > 0) ? tuning.towerCapThreatened : tuning.towerCapPeace;
    return view;
}

void tickAIForOwner(int owner) {
    const AITuning& tuning = defaultAITuning();
    AIWorldView view = buildAIWorldView(owner, tuning);
    WorldIndex world = buildWorldIndex(g);
    GameContext gameContext{ g, world, gameEvents() };
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

void tickAI() {
    g.aiTimer++;
    if (g.aiTimer < defaultAITuning().aiThinkIntervalTicks) return;
    g.aiTimer = 0;
    for (int o = 1; o < MAX_PLAYERS; o++) {
        if (!g.players[o].alive) continue;
        tickAIForOwner(o);
    }
}
