#include "realm.h"
#include "ai/ai.h"
#include "core/research_service.h"

void runAIProduction(int o, Player& p, const AIWorldView& view) {
    const int peas = view.peas, mil = view.mil, arch = view.arch, kni = view.kni;
    const int spr = view.spr, cat = view.cat, treb = view.treb;
    const int hous = view.hous, bar = view.bar, stb = view.stb;
    const int peasCap = view.peasCap, milCap = view.milCap, archCap = view.archCap;
    const int kniCap = view.kniCap, towerCap = view.towerCap;
    const AIIntel& intel = view.intel;
    (void)peas; (void)mil; (void)arch; (void)kni; (void)spr; (void)cat; (void)treb;
    (void)hous; (void)bar; (void)stb; (void)peasCap; (void)milCap; (void)archCap;
    (void)kniCap; (void)towerCap; (void)intel;
    // === MILITARY BUILDINGS ===
    if (bar == 0 && p.wood >= 150 && peas >= 2) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (bar == 1 && peas >= 6 && p.wood >= 150 && p.gold >= 100) {
        // Second barracks doubles training throughput.
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BARRACKS,bx,by); if(bx>=0) orderBuild(*b,E_BARRACKS,bx,by); }
    }
    if (aiCount(o,E_BLACKSMITH) == 0 && bar > 0 && p.wood >= 120) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_BLACKSMITH,bx,by); if(bx>=0) orderBuild(*b,E_BLACKSMITH,bx,by); }
    }
    if (stb == 0 && mil >= 3 && p.wood >= 200) {
        Entity* b = aiWorker(o);
        if (b) { int bx=-1,by=-1; aiBuildSpot(o,E_STABLE,bx,by); if(bx>=0) orderBuild(*b,E_STABLE,bx,by); }
    }

    // === RESEARCH: queue at blacksmith once it's built ===
    // Shares the canonical research service so AI now pays resources and uses
    // the same durations as the player (priority: Iron, Crossbows, Pikes, Plate, Counterweight).
    for (auto& smith : g.entities) {
        if (!smith.alive || smith.owner != o || smith.type != E_BLACKSMITH || smith.underConstruction) continue;
        if (smith.researching != 0) break;
        const ResearchId order[] = { ResearchId::IronWeapons, ResearchId::Crossbows,
                                     ResearchId::Pikes, ResearchId::PlateHelm,
                                     ResearchId::Counterweight };
        for (ResearchId id : order) {
            if (startResearch(g, o, smith.id, id)) break;
        }
        break;
    }

    // === MILITARY UNITS — train at every barracks/stable in parallel ===
    bool needCat = (intel.playerCastles > 0 || intel.playerWalls > 6 || intel.playerCatapults > 0);
    for (auto& br : g.entities) {
        if (!br.alive || br.owner != o || br.type != E_BARRACKS || br.underConstruction) continue;
        if (br.producing != E_NONE) continue;
        if (needCat && cat < 2 && p.gold >= 150 && p.wood >= 40 && p.food >= 30) { orderTrain(br, E_CATAPULT); continue; }
        int sprCap = std::max(4, intel.playerArmy/3 + 2);
        if (spr < sprCap && spr < arch && p.gold >= 40 && p.food >= 20) { orderTrain(br, E_SPEARMAN); continue; }
        if (arch < mil && arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
        if (mil  < milCap  && p.gold >= 60 && p.food >= 20) { orderTrain(br, E_MILITIA); continue; }
        if (spr < sprCap && p.gold >= 40 && p.food >= 20) { orderTrain(br, E_SPEARMAN); continue; }
        if (arch < archCap && p.gold >= 70 && p.food >= 20) { orderTrain(br, E_ARCHER);  continue; }
    }
    for (auto& st : g.entities) {
        if (!st.alive || st.owner != o || st.type != E_STABLE || st.underConstruction) continue;
        if (st.producing != E_NONE) continue;
        if (kni < kniCap && p.gold >= 120 && p.food >= 40) orderTrain(st, E_KNIGHT);
    }
    bool wantTreb = (intel.playerCastles > 0 || intel.playerWalls > 8 || intel.playerArmy >= 10);
    for (auto& cs : g.entities) {
        if (!cs.alive || cs.owner != o || cs.type != E_CASTLE || cs.underConstruction) continue;
        if (cs.producing != E_NONE) continue;
        if (wantTreb && treb < 2 && p.gold >= 200 && p.wood >= 250 && p.food >= 30) orderTrain(cs, E_TREBUCHET);
    }
}
