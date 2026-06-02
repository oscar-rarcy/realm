#include "realm.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

static void pruneInvalidEntityIds(std::vector<int>& ids) {
    ids.erase(std::remove_if(ids.begin(), ids.end(),
        [](int id){ return id <= 0 || id >= g.nextId || findEntity(id) == nullptr; }),
        ids.end());
}

static void recoverCommonGameStateIssues() {
    g.cursorX = std::max(0, std::min(g.cursorX, MAP_W - 1));
    g.cursorY = std::max(0, std::min(g.cursorY, MAP_H - 1));

    if (g.selectedId < 0 || g.selectedId >= g.nextId || findEntity(g.selectedId) == nullptr)
        g.selectedId = -1;
    pruneInvalidEntityIds(g.selectedIds);
    for (int i = 0; i < 9; i++) pruneInvalidEntityIds(g.controlGroups[i]);

    for (auto& e : g.entities) {
        if (e.targetId < -1 || e.targetId >= g.nextId || (e.targetId > 0 && findEntity(e.targetId) == nullptr)) {
            e.targetId = -1;
            if (e.state == S_ATTACKING || e.state == S_BUILDING || e.state == S_RETURNING || e.state == S_ENTERING)
                e.state = S_IDLE;
        }
        if (e.targetX != -1 && e.targetY != -1 && !inBounds(e.targetX, e.targetY)) {
            e.targetX = e.x;
            e.targetY = e.y;
            e.path.clear();
            e.pathIdx = 0;
            if (e.state == S_MOVING || e.state == S_GATHERING) e.state = S_IDLE;
        }
        if (e.pathIdx < 0 || e.pathIdx > (int)e.path.size()) e.pathIdx = 0;
        e.path.erase(std::remove_if(e.path.begin(), e.path.end(),
            [](const std::pair<int,int>& p){ return !inBounds(p.first, p.second); }),
            e.path.end());
        pruneInvalidEntityIds(e.garrison);
    }

    g.actionMarkers.erase(std::remove_if(g.actionMarkers.begin(), g.actionMarkers.end(),
        [](const ActionMarker& m){ return !inBounds(m.x, m.y) || m.ticks < 0; }),
        g.actionMarkers.end());
    for (auto& p : g.projectiles) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.tx) || !std::isfinite(p.ty) || p.life < 0) {
            p.alive = false;
            p.life = 0;
        }
    }
}

static bool validateOrRecoverGameState(const char* phase) {
    std::string error;
    if (validateGameState(&error)) return true;

    static int lastLoggedTick = -1000000;
    if (g.tick - lastLoggedTick >= 100) {
        std::cerr << "realm: recoverable validation error phase=" << phase
                  << " tick=" << g.tick
                  << " seed=" << g.seed
                  << " error=\"" << error << "\"\n";
        lastLoggedTick = g.tick;
    }

    recoverCommonGameStateIssues();
    std::string after;
    bool ok = validateGameState(&after);
#ifdef REALM_DEBUG_ASSERTS
    assert(ok && "invalid game state");
#endif
    if (!ok) {
        std::cerr << "realm: unrecovered validation error phase=" << phase
                  << " tick=" << g.tick
                  << " seed=" << g.seed
                  << " error=\"" << after << "\"\n";
    }
    return ok;
}

void tickSimulationOnce() {
    validateOrRecoverGameState("pre-tick");
    g.tick++;
    g.dayPhase += 1.0f / DAY_LENGTH;
    if (g.dayPhase >= 1.0f) g.dayPhase -= 1.0f;
    g.seasonPhase += 1.0f / SEASON_LENGTH;
    if (g.seasonPhase >= 4.0f) g.seasonPhase -= 4.0f;
    for (int i = 0; i < (int)g.entities.size(); i++) tickEntity(g.entities[i]);
    tickSeasons(); tickThaw(); tickWinter();
    tickWeather(); tickPaving();
    tickTowers(); tickGates(); tickProjectiles(); tickFarms(); tickMarkets();
    tickChurches(); tickAnimals(); tickAI(); tickActionMarkers(); updateFog();
    for (auto& e : g.entities) {
        if (!e.alive && e.state == S_DEAD && e.deathTicks < CORPSE_REMOVE_TICKS)
            e.deathTicks++;
    }
    auto pruneDead = [](std::vector<int>& v) {
        v.erase(std::remove_if(v.begin(), v.end(),
            [](int id){ return findEntity(id) == nullptr; }), v.end());
    };
    pruneDead(g.selectedIds);
    for (int i = 0; i < 9; i++) pruneDead(g.controlGroups[i]);
    if (g.selectedId >= 0 && !findEntity(g.selectedId)) g.selectedId = -1;
    if (g.tick % 100 == 0) {
        g.entities.erase(std::remove_if(g.entities.begin(), g.entities.end(),
            [](const Entity& e){
                if (e.alive || e.state != S_DEAD) return false;
                if (!isUnit(e.type) || isBuilding(e.type)) return true;
                return e.deathTicks >= CORPSE_REMOVE_TICKS;
            }), g.entities.end());
        for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(p);
        checkWin();
    }
    validateOrRecoverGameState("post-tick");
}
