#!/usr/bin/env python3
"""Local architecture guardrails for the Realm refactor.

This intentionally enforces only rules that have already been migrated, so it
can run in the current transitional codebase without blocking planned work.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


RULES: list[tuple[str, list[str], re.Pattern[str]]] = [
    (
        "command/domain code must emit events instead of calling UI helpers directly",
        [
            "src/core/*_service.cpp",
            "src/commands/orders.cpp",
            "src/commands/command_resolver.cpp",
            "src/commands/command_dispatcher.cpp",
        ],
        re.compile(r"\b(setStatus|addActionMarker)\s*\("),
    ),
    (
        "commands must not pack coordinates into integer payload fields",
        [
            "src/commands/*.h",
            "src/commands/*.cpp",
            "src/render/sdl/gfx_renderer.cpp",
        ],
        re.compile(r"\bgroupIndex\b|<<\s*16|&\s*0xffff"),
    ),
    (
        "AI planners must issue typed commands instead of direct order/service mutations",
        [
            "src/ai/*.cpp",
        ],
        re.compile(
            r"\border(Build|Train|Gather|Move|Attack|Garrison|Help)\s*\("
            r"|\bejectGarrison\s*\("
            r"|\bstart(Research|Move|AttackMove|Attack|Gather|Garrison)\s*\("
            r"|\bejectGarrisonService\s*\("
            r"|\b(setRallyPoint|toggleGateMode|toggleTrebuchetPacked|holdPosition|stopUnits)\s*\("
        ),
    ),
    (
        "app save/load shortcuts must dispatch typed commands instead of calling save/load directly",
        [
            "src/render/sdl/gfx_renderer.cpp",
            "src/platform/main_sdl.cpp",
            "src/platform/main_web.cpp",
        ],
        re.compile(r"\b(saveGame|loadGame)\s*\("),
    ),
    (
        "simulation systems must emit game events instead of calling UI status helpers directly",
        [
            "src/sim/*.cpp",
        ],
        re.compile(r"\b(setStatus|addActionMarker)\s*\("),
    ),
    (
        "game event handling must queue events instead of using the legacy UI sink",
        [
            "src/core/game_events.h",
            "src/core/game_events.cpp",
        ],
        re.compile(r"\bLegacyUiEventSink\b|\bevent\.player\s*>\s*0\b|\bg\s*\.\s*(statusMsg|statusTimer|actionMarkers)\b"),
    ),
    (
        "core simulation AI and map code must use explicit Game state instead of global g",
        [
            "src/ai/*.cpp",
            "src/core/*.cpp",
            "src/sim/*.cpp",
            "src/map/*.cpp",
        ],
        re.compile(r"\bg\s*\."),
    ),
    (
        "validation APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/core/validation.h",
            "src/core/validation.cpp",
        ],
        re.compile(
            r"\bvalidateGameStateIssues\s*\(\s*\)"
            r"|\bvalidateGameState\s*\(\s*std::string\s*\*"
        ),
    ),
    (
        "terrain/movement APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/core/terrain_defs.h",
            "src/core/entity_query.h",
            "src/core/validation.cpp",
            "src/sim/movement_system.cpp",
            "src/sim/fog_system.cpp",
        ],
        re.compile(
            r"\bisPassable(?:Water)?\s*\(\s*int\s+"
            r"|\bmoveAlongPath\s*\(\s*Entity&"
            r"|\bisConcealing\s*\(\s*\)"
            r"|\bisDetectedBy\s*\(\s*int\s+"
            r"|\bresetDetectMapCache\b"
            r"|\bis(Night|Dusk|Dawn)\s*\(\s*\)"
        ),
    ),
    (
        "movement/resource helpers must receive caller-provided WorldIndex",
        [
            "include/realm.h",
            "src/sim/movement_system.cpp",
        ],
        re.compile(
            r"\bmoveAlongPath\s*\(\s*Game&\s+\w+\s*,\s*Entity&"
            r"|\bfindNearbyResource\s*\(\s*Game&\s+\w+\s*,\s*Entity&"
            r"|\bbuildWorldIndex\s*\("
        ),
    ),
    (
        "time/season APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/sim/time_system.cpp",
        ],
        re.compile(r"\b(getBrightness|getSeason|getSeasonProgress|getSeasonName|getTimeName)\s*\(\s*\)"),
    ),
    (
        "simulation subsystem APIs must receive explicit Game state instead of using global tick wrappers",
        [
            "include/realm.h",
            "src/core/game_state.cpp",
            "src/sim/*.cpp",
        ],
        re.compile(
            r"\bvoid\s+(tickProjectiles|tickTowers|tickGates|tickFarms|tickMarkets|"
            r"tickChurches|tickAnimals|tickSeasons|tickThaw|tickWinter|tickPaving|"
            r"tickWeather|checkWin|updateFog|tickActionMarkers)\s*\(\s*\)"
        ),
    ),
    (
        "top-level simulation tick must receive explicit Game state",
        [
            "include/realm.h",
            "src/sim/simulation.cpp",
        ],
        re.compile(r"^\s*void\s+tickSimulationOnce\s*\(\s*\)"),
    ),
    (
        "save/load APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/sim/save_load.cpp",
        ],
        re.compile(r"^\s*bool\s+(saveGame|loadGame)\s*\(\s*const\s+std::string&"),
    ),
    (
        "entity tick APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/sim/entity_tick.cpp",
            "src/sim/production_system.cpp",
        ],
        re.compile(r"^\s*void\s+(tickEntity|tickProduction|tickResearch)\s*\(\s*(?!Game&)"),
    ),
    (
        "resource and spawn APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/core/game_state.h",
            "src/core/game_state.cpp",
            "src/core/entity_query.cpp",
            "src/sim/economy_system.cpp",
        ],
        re.compile(r"^\s*(?:void|int)\s+(addPlayerFood|spendPlayerFood|updateSupply|reservedSupply|spawnEntity)\s*\(\s*(?!Game&|const\s+Game&)"),
    ),
    (
        "RNG APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/core/rng.h",
            "src/core/rng.cpp",
        ],
        re.compile(r"^\s*(?:void|int)\s+realm(?:Srand|Rand)\s*\(\s*(?!Game&)"),
    ),
    (
        "entity query APIs must receive explicit Game and WorldIndex instead of using global wrappers",
        [
            "include/realm.h",
            "src/core/entity_query.h",
            "src/core/entity_query.cpp",
        ],
        re.compile(
            r"^\s*(?:Entity\*|bool|void)\s+"
            r"(findEntity|findDepot|entityAt|entityAtOwner|corpseAt|canPlace|buildOccupancyGrid)"
            r"\s*\(\s*(?!Game&|const\s+Game&)"
        ),
    ),
    (
        "simulation helper APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/sim/*.cpp",
        ],
        re.compile(
            r"^\s*(?:void|int|Entity\*|std::vector<.*?>)\s+"
            r"(spawnProjectile|findPath|findPathFor|findNearestEnemy|unitAtk|unitRange|"
            r"damageVs|killEntity|orderHelp|orderGarrison|findNearbyResource|ejectGarrison)"
            r"\s*\(\s*(?!const\s+Game&|Game&)"
        ),
    ),
    (
        "raw order APIs must receive explicit Game state instead of using global wrappers",
        [
            "include/realm.h",
            "src/commands/orders.cpp",
        ],
        re.compile(
            r"^\s*void\s+(orderMove|orderAttack|orderGather|orderBuild|orderBuildLine|"
            r"orderTrain|orderGroupMove|orderGroupAttack|orderGroupAttackMove)"
            r"\s*\(\s*(?!Game&)"
        ),
    ),
    (
        "dispatcher must delegate control-group state changes to the selection service",
        [
            "src/commands/command_dispatcher.cpp",
        ],
        re.compile(r"\bcontrolGroups\b"),
    ),
    (
        "core services must use indexed entity lookups instead of legacy global findEntity(id)",
        [
            "src/core/*_service.cpp",
        ],
        re.compile(r"\bfindEntity\s*\(\s*(?!game\s*,)"),
    ),
    (
        "entity animation helpers must receive explicit Game state instead of reading global g",
        [
            "src/core/entity_animation.cpp",
        ],
        re.compile(r"\bg\s*\."),
    ),
    (
        "core services must spawn entities into the supplied Game",
        [
            "src/core/*_service.cpp",
        ],
        re.compile(r"\bspawnEntity\s*\(\s*(?!game\s*,)"),
    ),
    (
        "order services must use local Game-aware single-unit order helpers",
        [
            "src/core/order_service.cpp",
        ],
        re.compile(r"\border(Move|Attack|Gather|Garrison)\s*\(\s*(?!game\s*,)"),
    ),
    (
        "input controller must dispatch commands instead of direct gameplay order/spawn calls",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\border(Build|Train|Attack|Gather|Move|Garrison)\s*\(|\bspawnEntity\s*\("),
    ),
    (
        "input controller must build an explicit GameContext instead of using legacy dispatchCommand(Game&, ...)",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bdispatchCommand\s*\(\s*g\s*,"),
    ),
    (
        "selection helpers must read selection from explicit Game state",
        [
            "src/commands/selection_controller.cpp",
        ],
        re.compile(r"\bg\s*\."),
    ),
    (
        "migrated module headers must not include the realm umbrella",
        [
            "include/entity_animation.h",
            "include/tileset_assets.h",
            "src/ai/ai.h",
            "src/commands/command.h",
            "src/commands/input_mode_controller.h",
            "src/core/build_service.h",
            "src/core/entity_defs.h",
            "src/core/game_context.h",
            "src/core/game_events.h",
            "src/core/market_service.h",
            "src/core/production_service.h",
            "src/core/research_defs.h",
            "src/core/terrain_defs.h",
            "src/core/validation.h",
            "src/core/world_index.h",
            "src/render/sdl/sdl_common.h",
            "src/render/visual_model.h",
        ],
        re.compile(r'#include\s+"realm\.h"'),
    ),
    (
        "context command resolution must create farms through typed BuildCommand/build service",
        [
            "src/commands/command_resolver.cpp",
        ],
        re.compile(r"\bspawnEntity\s*\("),
    ),
    (
        "context command resolution must produce typed commands instead of invoking orders or status directly",
        [
            "src/commands/command_resolver.cpp",
        ],
        re.compile(r"\border(Move|Attack|Gather|Garrison|Help|GroupMove|GroupAttack)\s*\(|\bemitStatusEvent\s*\("),
    ),
    (
        "context command resolution must receive caller-provided WorldIndex",
        [
            "src/commands/command.h",
            "src/commands/command_resolver.cpp",
        ],
        re.compile(r"\bresolveContextCommand\s*\(\s*const\s+Game&\s+\w+\s*,(?!\s*const\s+WorldIndex\s*&)"),
    ),
    (
        "dispatcher must not fall back to legacy context tile order helpers",
        [
            "src/commands/command_dispatcher.cpp",
        ],
        re.compile(r"\bcmdAtTile(Single|Group)\s*\("),
    ),
    (
        "command dispatch callers must provide an explicit GameContext",
        [
            "src/commands/command.h",
            "src/commands/command_dispatcher.cpp",
        ],
        re.compile(r"\bdispatchCommand\s*\(\s*Game\s*&"),
    ),
    (
        "input controller must not mutate player resources directly",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bplayers\s*\[[^\]]+\]\s*\.\s*(gold|wood|food|supply|supplyMax)\s*[+\-*/]?="),
    ),
    (
        "input controller must use selection/query services instead of scanning entity storage",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bg\s*\.\s*entities\b"),
    ),
    (
        "input controller must centralize game-mode transitions through input_mode_controller",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bg\s*\.\s*mode\s*=[^=]"),
    ),
    (
        "input controller must route status text through UI feedback helpers",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bsetStatus\s*\("),
    ),
    (
        "input controller must use view-state helpers for screen/map coordinate conversion",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bview\s*\.\s*view[XY]\s*\+|\bmapSY\b"),
    ),
    (
        "input controller must use controller/service helpers instead of direct entity lookups",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bfindEntity\s*\(|\bisBuilding\s*\(|\bisMilitary\s*\("),
    ),
    (
        "input controller must use selection services instead of direct selection state mutation",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bselectedId\s*=|\bselectedIds\s*\.|\bgroupAssignPending\s*="),
    ),
    (
        "input controller must use mode/UI helpers for pending build, help overlay, and control-group pending state",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bbuildPending\b|\bhelpOverlay\b|\bgroupAssignPending\b"),
    ),
    (
        "input controller must thread a PlayerId issuer instead of hardcoding player 0 in ownership checks",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(
            r"\b(selected\w+|selectNext\w+|selectAllMilitary|beginControlGroupAssignment|"
            r"selectionContainsMilitary|trainMenuEligibilityForSelected|utilityModeForSelectedBuilding)"
            r"\s*\(\s*g\s*,\s*(?:world\s*,\s*)?0\b"
        ),
    ),
    (
        "AI command helpers must execute through AIContext instead of dispatchCommand(g, ...)",
        [
            "src/ai/*.cpp",
        ],
        re.compile(r"\bdispatchCommand\s*\(\s*g\s*,"),
    ),
    (
        "AI tick must receive explicit Game state instead of reading global g",
        [
            "include/realm.h",
            "src/ai/ai.cpp",
            "src/sim/simulation.cpp",
        ],
        re.compile(r"\btickAI\s*\(\s*\)|\bg\s*\.\s*(aiTimer|players)\b"),
    ),
    (
        "AI planners must use context Game state for time and season queries",
        [
            "src/ai/*.cpp",
        ],
        re.compile(r"\bget(Brightness|Season|SeasonProgress|SeasonName|TimeName)\s*\(\s*\)"),
    ),
    (
        "AI planners must pass AIContext explicitly instead of using an active-context shim",
        [
            "src/ai/*.h",
            "src/ai/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\b(activeAIContext|setActiveAIContext)\b"),
    ),
    (
        "AI command helpers must require AIContext instead of no-context immediate dispatch shims",
        [
            "src/ai/*.h",
            "src/ai/*.cpp",
        ],
        re.compile(r"\baiDispatchImmediate\b|\bvoid\s+aiIssue\w+\s*\(\s*Entity&"),
    ),
    (
        "AI query/planner helpers must require AIContext or explicit Game/WorldIndex",
        [
            "src/ai/*.h",
            "src/ai/*.cpp",
        ],
        re.compile(
            r"\b(aiCount|aiCountAll|aiIdle|aiBldg|aiWorker|aiIdlePeasant|aiGather|"
            r"aiBuildSpotNear|aiBuildSpotWide|aiBuildSpot|aiScout|aiTickTrebuchets|"
            r"aiTickTransports|aiPickTarget|aiPickSiegeTarget|buildAIWorldView)"
            r"\s*\(\s*int\b"
        ),
    ),
    (
        "AI query helpers must reuse caller-provided WorldIndex instead of rebuilding global g",
        [
            "src/ai/ai_query.cpp",
        ],
        re.compile(r"\bbuildWorldIndex\s*\(\s*g\s*\)|\bGameContext\s+\w+\s*\{\s*g\s*,"),
    ),
    (
        "local save/load overloads must not swap through global g",
        [
            "src/sim/save_load.cpp",
        ],
        re.compile(r"\bstd::swap\s*\(\s*g\s*,\s*game\s*\)"),
    ),
    (
        "local map generation must not swap through global g",
        [
            "src/map/mapgen.cpp",
        ],
        re.compile(r"\bstd::swap\s*\(\s*g\s*,\s*game\s*\)"),
    ),
    (
        "map generation passes must receive Game explicitly instead of using target macros",
        [
            "src/map/mapgen.cpp",
            "src/map/mapgen_passes.cpp",
            "include/realm.h",
        ],
        re.compile(r"\bmapGenerationTarget\b|#define\s+g\b|#define\s+realmRand\b"),
    ),
    (
        "map generation callers must provide explicit Game state",
        [
            "include/realm.h",
            "src/map/mapgen.cpp",
        ],
        re.compile(r"\bgenerateMap\s*\(\s*(?:const\s+MapGenerationConfig&\s+\w+)?\s*\)"),
    ),
    (
        "legacy GameContext adapters must not return to core context headers",
        [
            "src/core/game_context.h",
        ],
        re.compile(r"\bstatic\s+WorldIndex\b|\blegacy(Game|Ui)Context\b"),
    ),
    (
        "save orchestration must delegate parsing to save_reader",
        [
            "src/sim/save_load.cpp",
        ],
        re.compile(r"\b(parseSaveStream|readIntVec|std::ifstream|REALM_SAVE)\b"),
    ),
    (
        "AI planners must use typed stance commands instead of mutating trebuchet packed fields",
        [
            "src/ai/*.cpp",
        ],
        re.compile(r"\.\s*(packed|packTicks)\s*="),
    ),
    (
        "AI planners should narrow inputs instead of blanket void suppressions",
        [
            "src/ai/*.cpp",
        ],
        re.compile(r"\(void\)\s*\w+"),
    ),
    (
        "migrated AI modules must use WorldIndex instead of scanning global entity storage",
        [
            "src/ai/ai_query.cpp",
            "src/ai/ai_combat.cpp",
            "src/ai/ai_economy.cpp",
            "src/ai/ai_production.cpp",
        ],
        re.compile(r"\bg\s*\.\s*entities\b"),
    ),
    (
        "context-based AI planner modules must use AIContext game state instead of global g",
        [
            "src/ai/ai_combat.cpp",
            "src/ai/ai_economy.cpp",
            "src/ai/ai_production.cpp",
        ],
        re.compile(r"\bg\s*\.|\bentityById\s*\(\s*g\s*,|\bforEachEnemyEntity\s*\(\s*g\s*,"),
    ),
    (
        "production tick system must use local Game-aware query/spawn/resource paths",
        [
            "src/sim/production_system.cpp",
        ],
        re.compile(
            r"\bg\s*\.\s*(entities|players)\b"
            r"|\b(findEntity|entityAt|spawnEntity)\s*\(\s*(?!game\s*,)"
        ),
    ),
    (
        "production tick system must emit owner-tagged events instead of filtering to owner 0",
        [
            "src/sim/production_system.cpp",
        ],
        re.compile(r"\bowner\s*==\s*0\b"),
    ),
    (
        "entity tick internals must use supplied Game state instead of global storage",
        [
            "src/sim/entity_tick.cpp",
        ],
        re.compile(r"\bg\s*\.\s*(entities|players|map|tick|attackNotifyCd|projectiles)\b"),
    ),
    (
        "tower tick system must use supplied Game state instead of global storage",
        [
            "src/sim/building_system.cpp",
        ],
        re.compile(r"\bg\s*\.\s*(entities|players|map|tick|projectiles)\b"),
    ),
    (
        "projectile tick system must use supplied Game state instead of global storage",
        [
            "src/sim/projectile_system.cpp",
        ],
        re.compile(r"\bg\s*\.\s*(projectiles|tick)\b"),
    ),
    (
        "win-condition system must use supplied Game state instead of global storage",
        [
            "src/sim/win_system.cpp",
        ],
        re.compile(r"\bg\s*\."),
    ),
    (
        "simulation tick orchestration must use supplied Game state instead of direct global storage",
        [
            "src/sim/simulation.cpp",
        ],
        re.compile(r"\bg\s*\."),
    ),
    (
        "focused state/save utility headers must not include the realm umbrella",
        [
            "src/core/types.h",
            "src/core/rng.h",
            "src/core/game_state.h",
            "src/core/entity_query.h",
            "src/core/game_context.h",
            "src/sim/save_reader.h",
            "src/sim/save_migration.h",
            "src/sim/save_writer.h",
        ],
        re.compile(r'#include\s+"realm\.h"'),
    ),
]


def iter_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        files.extend(ROOT.glob(pattern))
    return sorted({path for path in files if path.is_file()})


def main() -> int:
    failures: list[str] = []
    for description, patterns, regex in RULES:
        for path in iter_files(patterns):
            rel = path.relative_to(ROOT)
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                if regex.search(line):
                    failures.append(f"{rel}:{line_no}: {description}: {line.strip()}")

    for path in iter_files(["src/core/*.h", "include/core/*.h"]):
        rel = path.relative_to(ROOT)
        meaningful = []
        for line in path.read_text(encoding="utf-8").splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("//"):
                continue
            meaningful.append(stripped)
        if meaningful in (["#pragma once", '#include "realm.h"'], ['#include "realm.h"']):
            failures.append(f"{rel}:1: refactored module headers must not be empty realm.h wrappers")

    if failures:
        print("Architecture check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print("Architecture check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
