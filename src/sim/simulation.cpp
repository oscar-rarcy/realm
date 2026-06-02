#include "realm.h"
#include "core/world_index.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <utility>
#include <vector>

static void tickSimulationOnceInternal(Game& game, bool runAI);

static bool validateOrRecoverGameState(Game& game, const char* phase) {
    std::vector<ValidationIssue> issues = validateGameStateIssues(game);
    if (issues.empty()) return true;
    bool hasHardError = false;
    for (const ValidationIssue& issue : issues)
        if (issue.severity == ValidationSeverity::Error) hasHardError = true;

    static int lastLoggedTick = -1000000;
    if (game.tick - lastLoggedTick >= 100) {
        std::cerr << "realm: " << (hasHardError ? "hard" : "recoverable")
                  << " validation error phase=" << phase
                  << " tick=" << game.tick
                  << " seed=" << game.seed
                  << " error=\"" << issues.front().message << "\"\n";
        lastLoggedTick = game.tick;
    }
    if (hasHardError) {
#ifdef REALM_DEBUG_ASSERTS
        assert(false && "hard invalid game state");
#endif
        return false;
    }

    RecoveryResult recovery = recoverGameState(game, issues);
    bool ok = recovery.recovered;
#ifdef REALM_DEBUG_ASSERTS
    assert(ok && "invalid game state");
#endif
    if (!ok) {
        std::cerr << "realm: unrecovered validation error phase=" << phase
                  << " tick=" << game.tick
                  << " seed=" << game.seed
                  << " error=\"" << (recovery.remainingIssues.empty() ? std::string{} : recovery.remainingIssues.front().message) << "\"\n";
    }
    return ok;
}

void tickSimulationOnce() {
    tickSimulationOnceInternal(g, true);
}

static void pruneDeadReferenceList(Game& game, const WorldIndex& world, std::vector<int>& ids) {
    ids.erase(std::remove_if(ids.begin(), ids.end(),
        [&](int id){ return findEntity(game, world, id) == nullptr; }), ids.end());
}

static void tickSimulationOnceInternal(Game& game, bool runAI) {
    validateOrRecoverGameState(game, "pre-tick");
    game.tick++;
    if (game.statusTimer > 0) game.statusTimer--;
    game.dayPhase += 1.0f / DAY_LENGTH;
    if (game.dayPhase >= 1.0f) game.dayPhase -= 1.0f;
    game.seasonPhase += 1.0f / SEASON_LENGTH;
    if (game.seasonPhase >= 4.0f) game.seasonPhase -= 4.0f;
    for (int i = 0; i < (int)game.entities.size(); i++) tickEntity(game, game.entities[i]);
    tickSeasons(game); tickThaw(game); tickWinter(game);
    tickWeather(game); tickPaving(game);
    tickTowers(game); tickGates(game); tickProjectiles(game); tickFarms(game); tickMarkets(game);
    tickChurches(game); tickAnimals(game);
    if (runAI) tickAI();
    tickActionMarkers(game); updateFog(game);
    for (auto& e : game.entities) {
        if (!e.alive && e.state == S_DEAD && e.deathTicks < CORPSE_REMOVE_TICKS)
            e.deathTicks++;
    }
    WorldIndex pruneWorld = buildWorldIndex(game);
    pruneDeadReferenceList(game, pruneWorld, game.selectedIds);
    for (int i = 0; i < 9; i++) pruneDeadReferenceList(game, pruneWorld, game.controlGroups[i]);
    for (int p = 0; p < MAX_PLAYERS; p++)
        for (int i = 0; i < 9; i++)
            pruneDeadReferenceList(game, pruneWorld, game.controlGroupsByOwner[p][i]);
    if (game.selectedId >= 0) {
        if (!findEntity(game, pruneWorld, game.selectedId)) game.selectedId = -1;
    }
    if (game.tick % 100 == 0) {
        game.entities.erase(std::remove_if(game.entities.begin(), game.entities.end(),
            [](const Entity& e){
                if (e.alive || e.state != S_DEAD) return false;
                if (!isUnit(e.type) || isBuilding(e.type)) return true;
                return e.deathTicks >= CORPSE_REMOVE_TICKS;
            }), game.entities.end());
        for (int p = 0; p < MAX_PLAYERS; p++) updateSupply(game, p);
        checkWin(game);
    }
    validateOrRecoverGameState(game, "post-tick");
}

void tickSimulationOnce(Game& game) {
    tickSimulationOnceInternal(game, false);
}
