#include "realm.h"
#include "ai/ai.h"
#include "core/research_service.h"
#include "core/world_index.h"

namespace {

bool validAiOwner(int owner) {
    return owner >= 0 && owner < MAX_PLAYERS;
}

} // namespace

void runAIProduction(AIContext& context) {
    const int o = context.owner;
    const AIWorldView& view = context.view;
    const int peas = view.peas, mil = view.mil, arch = view.arch, kni = view.kni;
    const int spr = view.spr, cat = view.cat, treb = view.treb;
    const int bar = view.bar, stb = view.stb;
    const int milCap = view.milCap, archCap = view.archCap;
    const int kniCap = view.kniCap;
    const AIIntel& intel = view.intel;
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    // === MILITARY BUILDINGS ===
    if (bar == 0 && aiCanAffordEntity(context, E_BARRACKS) && peas >= 2) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_BARRACKS,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && aiCanAffordEntity(context, E_BARRACKS)) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_BARRACKS,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_BARRACKS,bx,by); }
    }
    if (aiCount(context,E_BLACKSMITH) == 0 && bar > 0 && aiCanAffordEntity(context, E_BLACKSMITH)) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_BLACKSMITH,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && aiCanAffordEntity(context, E_STABLE)) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_STABLE,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_STABLE,bx,by); }
    }

    // === RESEARCH: queue at blacksmith once it's built ===
    // Shares the canonical research service so AI now pays resources and uses
    // the same durations as the player (priority: Iron, Crossbows, Pikes, Plate, Counterweight).
    EntityId researchSmithId = -1;
    ResearchId researchToQueue = ResearchId::IronWeapons;
    for (EntityId smithId : world.buildingsByOwner[o]) {
        Entity* smithEntity = entityById(context.ctx.game, world, smithId);
        if (!smithEntity || smithEntity->type != E_BLACKSMITH || smithEntity->underConstruction) continue;
        Entity& smith = *smithEntity;
        const ResearchId order[] = { ResearchId::IronWeapons, ResearchId::Crossbows,
                                     ResearchId::Pikes, ResearchId::PlateHelm,
                                     ResearchId::Counterweight };
        for (ResearchId id : order) {
            CanResearchResult allowed = canResearch(context.ctx.game, world, o, smith, id);
            if (!allowed.ok) continue;
            researchSmithId = smithId;
            researchToQueue = id;
            break;
        }
        if (researchSmithId >= 0) break;
    }
    if (researchSmithId >= 0) {
        Entity* smith = entityById(context.ctx.game, world, researchSmithId);
        if (smith) aiIssueResearch(context, *smith, researchToQueue);
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needCat = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (EntityId barracksId : world.buildingsByOwner[o]) {
        Entity* barracks = entityById(context.ctx.game, world, barracksId);
        if (!barracks || barracks->type != E_BARRACKS || barracks->underConstruction) continue;
        Entity& br = *barracks;
        if (br.producing != E_NONE) continue;
        if (needCat && cat < 2 && aiCanAffordEntity(context, E_CATAPULT)) { aiIssueTrain(context, br, E_CATAPULT); continue; }
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        if (spr < sprCap && spr < arch && aiCanAffordEntity(context, E_SPEARMAN)) { aiIssueTrain(context, br, E_SPEARMAN); continue; }
        if (arch < mil && arch < archCap && aiCanAffordEntity(context, E_ARCHER)) { aiIssueTrain(context, br, E_ARCHER);  continue; }
        if (mil  < milCap  && aiCanAffordEntity(context, E_MILITIA)) { aiIssueTrain(context, br, E_MILITIA); continue; }
        if (spr < sprCap && aiCanAffordEntity(context, E_SPEARMAN)) { aiIssueTrain(context, br, E_SPEARMAN); continue; }
        if (arch < archCap && aiCanAffordEntity(context, E_ARCHER)) { aiIssueTrain(context, br, E_ARCHER);  continue; }
    }
    for (EntityId stableId : world.buildingsByOwner[o]) {
        Entity* stable = entityById(context.ctx.game, world, stableId);
        if (!stable || stable->type != E_STABLE || stable->underConstruction) continue;
        Entity& st = *stable;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && aiCanAffordEntity(context, E_KNIGHT)) aiIssueTrain(context, st, E_KNIGHT);
    }
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || intel.playerArmy >= 10);
    for (EntityId castleId : world.buildingsByOwner[o]) {
        Entity* castle = entityById(context.ctx.game, world, castleId);
        if (!castle || castle->type != E_CASTLE || castle->underConstruction) continue;
        Entity& cs = *castle;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && aiCanAffordEntity(context, E_TREBUCHET)) aiIssueTrain(context, cs, E_TREBUCHET);
    }
}
