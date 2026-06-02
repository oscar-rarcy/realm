#include "realm.h"
#include "ai/ai.h"
#include "core/world_index.h"

namespace {

bool validAiOwner(int owner) {
    return owner >= 0 && owner < MAX_PLAYERS;
}

} // namespace

void runAIProduction(int o, Player& p, const AIWorldView& view) {
    const int peas = view.peas, mil = view.mil, arch = view.arch, kni = view.kni;
    const int spr = view.spr, cat = view.cat, treb = view.treb;
    const int bar = view.bar, stb = view.stb;
    const int milCap = view.milCap, archCap = view.archCap;
    const int kniCap = view.kniCap;
    const AIIntel& intel = view.intel;
    WorldIndex world = buildWorldIndex(g);
    if (!validAiOwner(o)) return;
    // === MILITARY BUILDINGS ===
    if (bar == 0 && p.wood >= 150 && peas >= 2) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) aiIssueBuild(*b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && p.wood >= 150 && p.gold >= 100) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) aiIssueBuild(*b,E_BARRACKS,bx,by); }
    }
    if (aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) aiIssueBuild(*b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) aiIssueBuild(*b,E_STABLE,bx,by); }
    }

    // === RESEARCH: queue at blacksmith once it's built ===
    // Shares the canonical research service so AI now pays resources and uses
    // the same durations as the player (priority: Iron, Crossbows, Pikes, Plate, Counterweight).
    for (EntityId smithId : world.buildingsByOwner[o]) {
        Entity* smithEntity = entityById(g, world, smithId);
        if (!smithEntity || smithEntity->type != E_BLACKSMITH || smithEntity->underConstruction) continue;
        Entity& smith = *smithEntity;
        if (smith.researching != 0) break;
        const ResearchId order[] = { ResearchId::IronWeapons, ResearchId::Crossbows,
                                     ResearchId::Pikes, ResearchId::PlateHelm,
                                     ResearchId::Counterweight };
        for (ResearchId id : order) {
            int before = smith.researching;
            aiIssueResearch(smith, id);
            if (smith.researching != before) break;
        }
        break;
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needCat = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (EntityId barracksId : world.buildingsByOwner[o]) {
        Entity* barracks = entityById(g, world, barracksId);
        if (!barracks || barracks->type != E_BARRACKS || barracks->underConstruction) continue;
        Entity& br = *barracks;
        if (br.producing != E_NONE) continue;
        if (needCat && cat < 2 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { aiIssueTrain(br, E_CATAPULT); continue; }
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        if (spr < sprCap && spr < arch && p.gold >= 40 && p.food >= 20) { aiIssueTrain(br, E_SPEARMAN); continue; }
        if (arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { aiIssueTrain(br, E_ARCHER);  continue; }
        if (mil  < milCap  && p.gold >= 60 && p.food >= 20) { aiIssueTrain(br, E_MILITIA); continue; }
        if (spr < sprCap && p.gold >= 40 && p.food >= 20) { aiIssueTrain(br, E_SPEARMAN); continue; }
        if (arch < archCap && p.gold >= 70 && p.food >= 20) { aiIssueTrain(br, E_ARCHER);  continue; }
    }
    for (EntityId stableId : world.buildingsByOwner[o]) {
        Entity* stable = entityById(g, world, stableId);
        if (!stable || stable->type != E_STABLE || stable->underConstruction) continue;
        Entity& st = *stable;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) aiIssueTrain(st, E_KNIGHT);
    }
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || intel.playerArmy >= 10);
    for (EntityId castleId : world.buildingsByOwner[o]) {
        Entity* castle = entityById(g, world, castleId);
        if (!castle || castle->type != E_CASTLE || castle->underConstruction) continue;
        Entity& cs = *castle;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && p.gold >= 200 && p.wood >= 250 && p.food >= 30) aiIssueTrain(cs, E_TREBUCHET);
    }
}
