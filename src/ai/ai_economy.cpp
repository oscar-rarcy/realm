#include "realm.h"
#include "ai/ai.h"
#include "core/world_index.h"

namespace {

bool validAiOwner(int owner) {
    return owner >= 0 && owner < MAX_PLAYERS;
}

} // namespace

void runAIEconomy(AIContext& context) {
    const int o = context.owner;
    Player& p = context.ctx.game.players[o];
    const AIWorldView& view = context.view;
    const int peas = view.peas, mil = view.mil, arch = view.arch, kni = view.kni;
    const int spr = view.spr;
    const int hous = view.hous, peasCap = view.peasCap;
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    // === ECONOMY: peasants from every TH/Castle ===
    if (peas < peasCap) {
        for (EntityId id : world.buildingsByOwner[o]) {
            Entity* entity = entityById(g, world, id);
            if (!entity) continue;
            Entity& th = *entity;
            if (th.underConstruction) continue;
            if (th.type != E_TOWNHALL && th.type != E_CASTLE) continue;
            if (th.producing != E_NONE) continue;
            if (p.gold >= 50) { aiIssueTrain(context, th, E_PEASANT); break; }
        }
    }

    // === SUPPLY: keep houses ahead of training ===
    if (p.supply + 4 >= p.supplyMax && hous < 16 && p.wood >= 50) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpotWide(context,E_HOUSE,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_HOUSE,bx,by); }
    }

    if (aiCountAll(context,E_CASTLE) == 0 && mil + arch + kni + spr >= 8 && p.gold >= 100 && p.wood >= 250) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_CASTLE,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_CASTLE,bx,by); }
    }
}

void runAIDefenseInfrastructure(AIContext& context) {
    const int o = context.owner;
    Player& p = context.ctx.game.players[o];
    const AIWorldView& view = context.view;
    const int mil = view.mil, towerCap = view.towerCap;
    // === DEFENSE: towers scaled to threat ===
    if (aiCountAll(context,E_TOWER) < towerCap && mil >= 2 && p.wood >= 100 && p.gold >= 50) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_TOWER,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_TOWER,bx,by); }
    }
}

void runAIFoodEconomy(AIContext& context) {
    const int o = context.owner;
    Player& p = context.ctx.game.players[o];
    const AITuning& tuning = defaultAITuning();
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    // === FOOD: mill + farms scale up before winter ===
    if (aiCountAll(context,E_MILL) == 0 && p.wood >= 100) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_MILL,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_MILL,bx,by); }
    }
    int wantFarms = (getSeason() == AUTUMN) ? tuning.autumnFarmCount : (getSeason() == WINTER ? 0 : tuning.normalFarmCount);
    if (aiCountAll(context,E_MILL) > 0 && aiCountAll(context,E_FARM) < wantFarms && getSeason() != WINTER) {
        Entity* b = aiWorker(context);
        if (b) { int bx=-1,by=-1; aiBuildSpot(context,E_FARM,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_FARM,bx,by); }
    }
    for (EntityId farmId : world.buildingsByOwner[o]) {
        Entity* farmEntity = entityById(g, world, farmId);
        if (!farmEntity || farmEntity->type != E_FARM || farmEntity->underConstruction) continue;
        Entity& farm = *farmEntity;
        bool tended = false;
        for (EntityId unitId : world.unitsByOwner[o]) {
            Entity* u = entityById(g, world, unitId);
            if (u && u->state == S_BUILDING && u->targetId == farm.id) {
                tended = true;
                break;
            }
        }
        if (!tended) {
            Entity* tend = aiIdlePeasant(context);
            if (tend) { aiIssueContext(context, *tend, farm.x, farm.y); break; }
        }
    }
}

void runAINaval(AIContext& context) {
    const int o = context.owner;
    Player& p = context.ctx.game.players[o];
    const AIWorldView& view = context.view;
    const int peas = view.peas;
    const AITuning& tuning = defaultAITuning();
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    // === NAVAL: dock + boats if water nearby ===
    if (aiCountAll(context,E_DOCK) == 0 && p.wood >= 100 && peas >= 4) {
        Entity* th = aiBldg(context, E_TOWNHALL);
        if (th) {
            int bx=-1,by=-1; aiBuildSpotNear(context, E_DOCK, th->x, th->y, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(context);
                if (b) aiIssueBuild(context, *b, E_DOCK, bx, by);
            }
        }
    }
    for (EntityId dockId : world.buildingsByOwner[o]) {
        Entity* dock = entityById(g, world, dockId);
        if (!dock || dock->type != E_DOCK || dock->underConstruction) continue;
        Entity& dk = *dock;
        if (dk.producing != E_NONE) continue;
        // Fishing boats first for food, then a couple of warships for coastline pressure.
        if (aiCount(context,E_FISHING_BOAT) < tuning.fishingBoatCap && p.gold >= 80 && p.wood >= 50) { aiIssueTrain(context, dk, E_FISHING_BOAT); continue; }
        if (aiCount(context,E_WARSHIP) < tuning.warshipCap && p.gold >= 150 && p.wood >= 80 && p.food >= 20) { aiIssueTrain(context, dk, E_WARSHIP); continue; }
        if (g.biomeChoice == B_OCEAN && aiCount(context,E_TRANSPORT) < tuning.transportCap
            && p.gold >= 80 && p.wood >= 40 && p.food >= 10) { aiIssueTrain(context, dk, E_TRANSPORT); continue; }
    }
}

void runAIExpansion(AIContext& context) {
    const int o = context.owner;
    Player& p = context.ctx.game.players[o];
    const AIWorldView& view = context.view;
    const int peas = view.peas, mil = view.mil, arch = view.arch, kni = view.kni;
    const int spr = view.spr;
    const AIIntel& intel = view.intel;
    const AITuning& tuning = defaultAITuning();
    WorldIndex& world = context.ctx.world;
    if (!validAiOwner(o)) return;
    // === EXPANSION: forward TH halfway to the player ===
    if (aiCountAll(context,E_TOWNHALL) + aiCountAll(context,E_CASTLE) < tuning.expansionBaseCap
        && peas >= tuning.expansionPeasantMin && p.wood >= 260 && intel.playerTownCenterPos) {
        Entity* myTh = aiBldg(context, E_TOWNHALL);
        if (!myTh) myTh = aiBldg(context, E_CASTLE);
        if (myTh) {
            MapPos playerBase = *intel.playerTownCenterPos;
            int fx = (myTh->x + playerBase.x) / 2;
            int fy = (myTh->y + playerBase.y) / 2;
            int bx=-1, by=-1; aiBuildSpotNear(context, E_TOWNHALL, fx, fy, bx, by);
            if (bx >= 0) {
                Entity* b = aiWorker(context);
                if (b) aiIssueBuild(context, *b, E_TOWNHALL, bx, by);
            }
        }
    }

    // === GARRISON: pack archers into the nearest tower/TH/Castle ===
    for (EntityId buildingId : world.buildingsByOwner[o]) {
        Entity* building = entityById(g, world, buildingId);
        if (!building || building->underConstruction) continue;
        Entity& bld = *building;
        if (!canGarrisonIn(bld.type)) continue;
        if ((int)bld.garrison.size() >= garrisonCap(bld.type)) continue;
        Entity* archer = nullptr; int bestD = 99999;
        for (EntityId unitId : world.unitsByOwner[o]) {
            Entity* unit = entityById(g, world, unitId);
            if (!unit || unit->state != S_IDLE) continue;
            if (!isRanged(unit->type) || isSiege(unit->type) || isNaval(unit->type)) continue;
            int d = mdist(unit->x, unit->y, bld.x, bld.y);
            if (d < bestD) { bestD = d; archer = unit; }
        }
        if (archer) aiIssueGarrison(context, *archer, bld.id);
    }

    // === FORWARD AGGRESSION: mid-game outpost near the player base ===
    if (g.tick > tuning.expansionTick && intel.playerTownCenterPos && (mil + arch + kni + spr) >= tuning.forwardAggressionArmy) {
        Entity* home = aiBldg(context, E_TOWNHALL);
        if (!home) home = aiBldg(context, E_CASTLE);
        if (home) {
            MapPos playerBase = *intel.playerTownCenterPos;
            int fx = home->x + (playerBase.x - home->x) * tuning.forwardAggressionPercent / 100;
            int fy = home->y + (playerBase.y - home->y) * tuning.forwardAggressionPercent / 100;
            Entity* anchor = nullptr;
            for (EntityId buildingId : world.buildingsByOwner[o]) {
                Entity* e = entityById(g, world, buildingId);
                if (!e || (e->type != E_CASTLE && e->type != E_TOWNHALL)) continue;
                if (dist(e->x, e->y, home->x, home->y) < tuning.forwardHomeExclusionRadius) continue;
                if (dist(e->x, e->y, fx, fy) < tuning.forwardAnchorRadius) { anchor = e; break; }
            }
            if (!anchor && p.gold >= 100 && p.wood >= 250) {
                Entity* b = aiWorker(context);
                if (b) { int bx=-1,by=-1; aiBuildSpotNear(context,E_CASTLE,fx,fy,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_CASTLE,bx,by); }
            } else if (anchor) {
                bool hasBarr = false;
                for (EntityId buildingId : world.buildingsByOwner[o]) {
                    Entity* e = entityById(g, world, buildingId);
                    if (e && e->type == E_BARRACKS && dist(e->x,e->y,anchor->x,anchor->y) < tuning.forwardHomeExclusionRadius)
                        { hasBarr = true; break; }
                }
                if (!hasBarr && p.wood >= 150) {
                    Entity* b = aiWorker(context);
                    if (b) { int bx=-1,by=-1; aiBuildSpotNear(context,E_BARRACKS,anchor->x,anchor->y,bx,by); if(bx>=0) aiIssueBuild(context, *b,E_BARRACKS,bx,by); }
                }
            }
        }
    }

    // === COASTAL BEACHHEAD: landed peasant starts a forward Castle ===
    if (g.biomeChoice == B_OCEAN && intel.playerTownCenterPos && p.gold >= 100 && p.wood >= 250) {
        Entity* home = aiBldg(context, E_TOWNHALL);
        if (!home) home = aiBldg(context, E_CASTLE);
        MapPos playerBase = *intel.playerTownCenterPos;
        for (EntityId unitId : world.unitsByOwner[o]) {
            Entity* unit = entityById(g, world, unitId);
            if (!unit || !isWorker(unit->type) || unit->state != S_IDLE) continue;
            Entity& u = *unit;
            if (home && mdist(u.x,u.y,home->x,home->y) < tuning.beachheadHomeExclusionRadius) continue;
            if (mdist(u.x,u.y,playerBase.x,playerBase.y) > tuning.beachheadPlayerRadius) continue;
            bool hasBase = false;
            for (EntityId buildingId : world.buildingsByOwner[o]) {
                Entity* b = entityById(g, world, buildingId);
                if (b && isBuilding(b->type) && mdist(b->x,b->y,u.x,u.y) < tuning.beachheadBaseRadius)
                    { hasBase = true; break; }
            }
            if (!hasBase) { int bx=-1,by=-1; aiBuildSpotNear(context,E_CASTLE,u.x,u.y,bx,by); if(bx>=0) aiIssueBuild(context, u,E_CASTLE,bx,by); }
            break;
        }
    }
}
