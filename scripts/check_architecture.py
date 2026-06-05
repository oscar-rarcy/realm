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
        "command domain implementations must include focused headers instead of the realm umbrella",
        [
            "src/commands/command_dispatcher.cpp",
            "src/commands/command_resolver.cpp",
            "src/commands/input_mode_controller.cpp",
            "src/commands/selection_controller.cpp",
        ],
        re.compile(r'#include\s+"realm\.h"'),
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
        "AI planner modules must use AIContext tuning instead of pulling defaults",
        [
            "src/ai/ai_*.cpp",
        ],
        re.compile(r"\bdefaultAITuning\s*\("),
    ),
    (
        "AI owner tick declarations must receive explicit EventSink and tuning",
        [
            "src/ai/ai.h",
            "src/ai/ai.cpp",
        ],
        re.compile(r"\btickAIForOwner\s*\(\s*Game&\s+\w+\s*,\s*int\b"),
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
        "simulation and AI domain code must emit through explicit EventSink instead of global event helpers",
        [
            "src/ai/*.cpp",
            "src/sim/*.cpp",
        ],
        re.compile(r"\bemit(StatusEvent|ActionMarkerEvent|GameEvent)\s*\("),
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
        "UI presentation status and action markers must live in UiState instead of Game",
        [
            "src/core/*.cpp",
            "src/sim/*.cpp",
            "src/render/*.cpp",
            "src/render/ascii/*.cpp",
            "src/render/sdl/*.cpp",
            "src/platform/*.cpp",
        ],
        re.compile(r"\b(?:g|game)\s*\.\s*(statusMsg|statusTimer|actionMarkers)\b|\bflushGameEventsToUi\s*\(\s*(?:g|game)\b"),
    ),
    (
        "Game state must not declare UI-only status or action marker fields",
        [
            "include/realm.h",
        ],
        re.compile(r"\bstatusMsg\b|\bstatusTimer\b|\bactionMarkers\b"),
    ),
    (
        "core services must emit through explicit EventSink instead of global event helpers",
        [
            "src/core/build_service.cpp",
            "src/core/market_service.cpp",
            "src/core/order_service.cpp",
            "src/core/production_service.cpp",
            "src/core/research_service.cpp",
        ],
        re.compile(r"\bemit(StatusEvent|ActionMarkerEvent|GameEvent)\s*\("),
    ),
    (
        "command services must emit through GameContext or explicit EventSink instead of global event helpers",
        [
            "src/commands/*.cpp",
        ],
        re.compile(r"\bemit(StatusEvent|ActionMarkerEvent|GameEvent)\s*\("),
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
        "core passability and placement must use terrain definitions instead of hardcoded terrain lists",
        [
            "src/core/validation.cpp",
            "src/core/entity_query.cpp",
        ],
        re.compile(r"T_MOUNTAIN|T_WATER|T_STONE|T_CASTLE_WALL|T_FISH|T_LAVA|T_GOLD|T_FOREST|T_PINE|T_PALM|T_DEAD_TREE|T_SHALLOWS|T_MARSH|T_REEDS|T_ICE"),
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
        "event-producing simulation APIs must receive explicit EventSink",
        [
            "include/realm.h",
            "src/ai/ai.cpp",
            "src/sim/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(
            r"\bkillEntity\s*\(\s*Game&\s+\w+\s*,\s*Entity&"
            r"|\bfindNearbyResource\s*\(\s*Game&\s+\w+\s*,\s*const\s+WorldIndex&\s+\w+\s*,\s*Entity&"
            r"|\btick(Entity|Production|Research|Towers|Farms|Churches|Animals|Seasons|Winter|Weather|AI)\s*\(\s*Game&\s+\w+\s*(?:,\s*Entity&|\)|,\s*bool)"
            r"|\btickSimulationOnce\s*\(\s*Game&\s+\w+\s*(?:\)|,\s*bool)"
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
        "legacy occupancy-grid builders must not coexist with WorldIndex occupancy layers",
        [
            "include/realm.h",
            "src/core/entity_query.h",
            "src/core/entity_query.cpp",
            "src/core/game_types.h",
            "src/core/world_index.h",
            "src/sim/pathfinding.cpp",
            "tests/realm_headless_tests.cpp",
        ],
        re.compile(r"\bOccupancyGrid\b|\bbuildOccupancyGrid\b"),
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
        "raw single-unit order APIs must not coexist with the order service",
        [
            "include/realm.h",
            "src/commands/orders.cpp",
        ],
        re.compile(r"\border(Move|Attack|Gather|Help|Garrison)\s*\("),
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
        "research validation must receive caller-provided WorldIndex state",
        [
            "src/core/research_service.h",
            "src/core/research_service.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\bcanResearch\s*\(\s*(?:const\s+)?Game&\s+\w+\s*,\s*int\b|\bcanResearch\s*\(\s*g\s*,\s*0\s*,"),
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
        "domain services must expose explicit WorldIndex service entry points only",
        [
            "src/core/build_service.h",
            "src/core/build_service.cpp",
            "src/core/production_service.h",
            "src/core/production_service.cpp",
            "src/core/research_service.h",
            "src/core/research_service.cpp",
            "src/core/market_service.h",
            "src/core/market_service.cpp",
        ],
        re.compile(
            r"\bcanStartBuild\s*\(\s*const\s+Game&\s+\w+\s*,\s*int\b"
            r"|\b(startBuild|startBuildLine|startTraining|startResearch|executeTrade)\s*\("
            r"|\b(startTrainingService|startResearchService|executeTradeService)\s*\(\s*Game&\s+\w+\s*,\s*int\b"
        ),
    ),
    (
        "order service must own grouped move and attack command mutation",
        [
            "include/realm.h",
            "src/commands/orders.cpp",
        ],
        re.compile(r"\borderGroup(Move|Attack|AttackMove)\s*\("),
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
        "selection command APIs must receive explicit WorldIndex and PlayerId",
        [
            "src/commands/command.h",
            "src/commands/selection_controller.cpp",
        ],
        re.compile(
            r"\b(selectAtTile|selectAllOfTypeInView)\s*\(\s*Game&\s+\w+\s*,\s*int\b"
            r"|\bboxSelect\s*\(\s*Game&\s+\w+\s*,\s*(?:PlayerId\s+\w+\s*,\s*)?int\b"
        ),
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
            "src/render/render_model.h",
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
        "commands must not maintain a parallel CommandType enum beside CommandPayload",
        [
            "src/commands/command.h",
            "src/commands/command_dispatcher.cpp",
            "src/ai/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\bCommandType\b|\b\.type\s*\(\s*\)"),
    ),
    (
        "input training hotkey helpers must make ProductionRule the obvious source of truth",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\btrainMenuSelection\b|\bTrainMenuOption\b"),
    ),
    (
        "gameplay help metadata must not look like executable input bindings",
        [
            "src/commands/input_intent.h",
            "src/commands/input_intent.cpp",
            "src/render/**/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\bCommandBinding\b|\bgameplayCommands\s*\("),
    ),
    (
        "UI status helpers must be named as UI-boundary events, not domain status shims",
        [
            "src/**/*.h",
            "src/**/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\bsetStatus\s*\(|\binputStatus\s*\(|\bemitStatusEvent\s*\("),
    ),
    (
        "renderers must use TerrainDefinition for base terrain ASCII glyphs",
        [
            "src/render/**/*.h",
            "src/render/**/*.cpp",
        ],
        re.compile(r"\bterrainAscii\s*\("),
    ),
    (
        "AI passes must use injected AITuning instead of defaultAITuning",
        [
            "src/ai/ai_*.cpp",
        ],
        re.compile(r"\bdefaultAITuning\s*\("),
    ),
    (
        "input intents must not keep stale context enum values outside the typed command path",
        [
            "src/commands/input_intent.h",
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bInputIntent::Context\b|\bContext,\s*$"),
    ),
    (
        "research menu metadata must live in ResearchDef instead of input-local tables",
        [
            "src/commands/input_controller.cpp",
            "src/core/research_defs.h",
            "src/core/research_defs.cpp",
        ],
        re.compile(r"\bResearchMenuOption\b|Research:\s*\[I\]"),
    ),
    (
        "action markers must use explicit GameEvent emission instead of global marker shims",
        [
            "src/core/game_events.h",
            "src/core/game_events.cpp",
            "src/**/*.cpp",
            "tests/*.cpp",
        ],
        re.compile(r"\baddActionMarker\s*\(|\bemitActionMarkerEvent\s*\("),
    ),
    (
        "local app/render/input command runners must use the shared command runner",
        [
            "src/commands/input_controller.cpp",
            "src/render/sdl/gfx_renderer.cpp",
            "src/platform/main_sdl.cpp",
            "src/platform/main_web.cpp",
        ],
        re.compile(r"\bGameContext\s+\w+\s*\{|\bdispatchCommand\s*\(|\bdispatchContextCommandForLocalGame\b"),
    ),
    (
        "renderer/platform code must not add bespoke command dispatch wrappers",
        [
            "src/render/sdl/gfx_renderer.cpp",
            "src/platform/main_sdl.cpp",
            "src/platform/main_web.cpp",
        ],
        re.compile(r"\bdispatch(Gfx|Platform)(Context)?Command\b"),
    ),
    (
        "entity visual fallbacks must use render/entity_visual_defs instead of duplicate tables",
        [
            "src/render/display_model.cpp",
            "src/render/sdl/display_glyphs.cpp",
            "src/render/sdl/tileset_assets.cpp",
        ],
        re.compile(r"\bENTITY_EMOJI\b|case\s+E_[A-Z0-9_]+\s*:\s*return\s+u8|lowerSlug\s*\(\s*STATS\s*\["),
    ),
    (
        "SDL HUD shortcut labels must come from gameplayHelpBindings",
        [
            "src/render/sdl/hud_renderer.cpp",
        ],
        re.compile(r"F5-F8:Save|F9-F12:Load|B:Build|T:Train|Q:Resign|X:Hold"),
    ),
    (
        "startup placement must not rebuild WorldIndex for each entity-at check",
        [
            "src/platform/game_init.cpp",
        ],
        re.compile(r"\bstartupEntityAt\b|\bbuildWorldIndex\s*\("),
    ),
    (
        "input controller must not add bespoke command dispatch wrappers",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bdispatchInput\w*\b|\bdispatch(Build|Train|Tile|Simple|Slot)Command\b"),
    ),
    (
        "SDL renderer query helpers must not reintroduce legacy sdl* query names or hidden state",
        [
            "src/render/sdl/*.h",
            "src/render/sdl/*.cpp",
        ],
        re.compile(
            r"\bactiveRenderModel\b"
            r"|\bsdl(FindEntity|EntityAt|CorpseAt|CanPlace)\s*\("
        ),
    ),
    (
        "mobile HUD helpers must receive caller-provided WorldIndex state",
        [
            "src/render/sdl/sdl_hud.h",
            "src/render/sdl/mobile_hud.cpp",
            "src/render/sdl/gfx_renderer.cpp",
        ],
        re.compile(
            r"\b(mobileHudButtons|mobileHasSelectedWorker|mobileHasSelectedMilitary|"
            r"mobileSelectionSummary|primaryOwnedSelection|mobileStopSelection)\s*\(\s*\)"
        ),
    ),
    (
        "SDL terminal frame helpers must receive caller-provided WorldIndex state",
        [
            "src/render/sdl/sdl_terminal.h",
            "src/render/sdl/terminal_frame_renderer.cpp",
            "src/render/sdl/gfx_renderer.cpp",
        ],
        re.compile(
            r"\bbuildAsciiTerminalFrame\s*\(\s*\)"
            r"|\bdrawAsciiTerminalFrame\s*\(\s*(?:true|false)\s*\)"
            r"|\bdrawAsciiMobileFrame\s*\(\s*(?:true|false)\s*\)"
        ),
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
        "input controller must not reintroduce legacy input status shims",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bsetStatus\s*\(|\bemitStatusEvent\s*\(|\binputStatus\s*\("),
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
        "input train menu must use production rules instead of a duplicated trainable-unit table",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bTrainMenuOption\b|static\s+constexpr\s+.*E_(?:TOWNHALL|BARRACKS|STABLE|CASTLE|DOCK).*E_"),
    ),
    (
        "input build menu must use build rules instead of a duplicated building table",
        [
            "src/commands/input_controller.cpp",
        ],
        re.compile(r"\bBuildMenuOption\b|static\s+constexpr\s+.*E_(?:HOUSE|BARRACKS|STABLE|TOWER|FARM|WALL|BLACKSMITH|CHURCH|MARKET|CASTLE|DOCK)"),
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
        "AI planner passes must use injected AITuning instead of default tuning",
        [
            "src/ai/ai_combat.cpp",
            "src/ai/ai_economy.cpp",
            "src/ai/ai_production.cpp",
            "src/ai/ai_query.cpp",
        ],
        re.compile(r"\bdefaultAITuning\s*\("),
    ),
    (
        "AI research planning must treat aiIssueResearch as queued, not immediate mutation",
        [
            "src/ai/ai_production.cpp",
        ],
        re.compile(r"\bresearching\b|\bresearching\s*!=\s*before\b|\bbefore\s*=\s*\w+\.researching\b"),
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
        "map generation config must be read from explicit Game state",
        [
            "include/realm.h",
            "src/map/mapgen.cpp",
        ],
        re.compile(r"\bcurrentMapGenerationConfig\s*\(\s*\)"),
    ),
    (
        "map generation noise must be explicit state passed through generation passes",
        [
            "include/realm.h",
            "src/map/mapgen.cpp",
            "src/map/mapgen_passes.cpp",
        ],
        re.compile(
            r"\bstatic\s+float\s+noiseGrid\b"
            r"|\binitNoise\s*\("
            r"|\bsampleNoise\s*\(\s*(?!const\s+MapNoise&|[A-Za-z_]\w*\s*,)"
            r"|\b(assignBiomesAndPaintBaseTerrain|addMountains|addRoads)\s*\(\s*Game&\s+\w+\s*\)"
        ),
    ),
    (
        "gameplay help bindings must live with input intent mapping",
        [
            "src/core/entity_defs.cpp",
        ],
        re.compile(r"\bCommandBinding\b|\bgameplayCommands\s*\("),
    ),
    (
        "legacy GameContext adapters must not return to core context headers",
        [
            "src/core/game_context.h",
        ],
        re.compile(r"\bstatic\s+WorldIndex\b|\blegacy(Game|Ui)Context\b|\bUiContext\b"),
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
    (
        "WorldIndex build-count instrumentation must be test-gated",
        [
            "src/core/world_index.h",
            "src/core/world_index.cpp",
        ],
        re.compile(r"worldIndexBuildCount|resetWorldIndexBuildCount|g_worldIndexBuildCount"),
    ),
]


def iter_files(patterns: list[str]) -> list[Path]:
    files: list[Path] = []
    for pattern in patterns:
        files.extend(ROOT.glob(pattern))
    return sorted({path for path in files if path.is_file()})


APPROVED_GLOBAL_WORLD_INDEX_BUILDERS = {
    # Input/render boundaries may still adapt global app state into explicit
    # WorldIndex snapshots. Domain code must use caller-provided Game state.
    "src/commands/input_controller.cpp",
    "src/platform/main_web.cpp",
    "src/render/ascii/ncurses_renderer.cpp",
    "src/render/sdl/gfx_renderer.cpp",
    "src/render/sdl/tileset_lab.cpp",
}

LEGACY_RENDERER_DIRECT_G_READS = {
    # Transitional renderer modules not yet fully backed by RenderModel. New
    # renderer files should not add direct g.map/g.entities/selection reads.
    "src/render/ascii/display_glyphs.cpp",
    "src/render/ascii/hud_renderer.cpp",
    "src/render/ascii/map_renderer.cpp",
    "src/render/sdl/display_glyphs.cpp",
    "src/render/sdl/gfx_renderer.cpp",
    "src/render/sdl/hud_renderer.cpp",
    "src/render/sdl/map_renderer.cpp",
    "src/render/sdl/mobile_hud.cpp",
    "src/render/sdl/sdl_context.cpp",
    "src/render/sdl/terminal_frame_renderer.cpp",
    "src/render/sdl/tileset_lab.cpp",
}


def code_part(line: str) -> str:
    return line.split("//", 1)[0]


def add_custom_failures(failures: list[str]) -> None:
    if (ROOT / "src/render/visual_model.cpp").exists():
        failures.append(
            "src/render/visual_model.cpp: RenderModel code lives in render_model.cpp; display glyph code belongs in display_model.cpp"
        )

    raw_order_call = re.compile(r"\border(?:Move|Attack|Gather|Group(?:Move|Attack|AttackMove)?)\s*\(")
    global_world_index = re.compile(r"\bbuildWorldIndex\s*\(\s*g\s*\)")
    extern_game = re.compile(r"\bextern\s+Game\s+g\b")
    realm_include = re.compile(r'#include\s+"realm\.h"')
    domain_global_event = re.compile(
        r"\b(setStatus|addActionMarker|emitStatusEvent|emitActionMarkerEvent|emitGameEvent)\s*\("
    )
    no_context_index_overload = re.compile(
        r"\b(canStartBuild|startBuild|startBuildLine|startTraining|startResearch|executeTrade|"
        r"startTrainingService|startResearchService|executeTradeService)\b\s*\("
        r"\s*(?:const\s+)?Game&\s+\w+\s*,\s+(?!(?:const\s+)?WorldIndex&)"
    )
    renderer_direct_g = re.compile(r"\bg\s*\.\s*(map|entities|selectedId|selectedIds)\b")
    duplicate_dispatch_wrapper = re.compile(
        r"\bdispatch(?:Input|Gfx|Platform)\w*Command\s*\(|\bGameContext\s+\w+\s*\{|\bdispatchCommand\s*\("
    )
    ai_hardcoded_cost_gate = re.compile(
        r"\b(?:p|player)\s*\.\s*(gold|wood|food)\s*[<>]=\s*\d+"
        r"|\bplayers\s*\[[^\]]+\]\s*\.\s*(gold|wood|food)\s*[<>]=\s*\d+"
    )
    input_menu_table = re.compile(
        r"\b(BuildMenuOption|TrainMenuOption|ResearchMenuOption)\b"
        r"|static\s+(?:constexpr\s+)?(?:const\s+)?(?:EntityType|ResearchId)\s+\w+\s*\["
    )

    for path in iter_files(["src/**/*.h", "src/**/*.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        text = path.read_text(encoding="utf-8").splitlines()
        for line_no, line in enumerate(text, 1):
            code = code_part(line)
            if rel != "src/core/order_service.cpp" and raw_order_call.search(code):
                failures.append(
                    f"{rel}:{line_no}: raw orderMove/orderAttack/orderGather/orderGroup calls are only allowed inside order_service: {line.strip()}"
                )
            if global_world_index.search(code) and rel not in APPROVED_GLOBAL_WORLD_INDEX_BUILDERS:
                failures.append(
                    f"{rel}:{line_no}: buildWorldIndex(g) is only allowed at approved app/render transition points: {line.strip()}"
                )

    for path in iter_files(["src/core/**/*", "src/ai/**/*", "src/commands/**/*", "src/map/**/*", "src/sim/**/*"]):
        if path.suffix not in {".h", ".cpp"}:
            continue
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if extern_game.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: extern Game g is not allowed in domain modules: {line.strip()}"
                )

    for path in iter_files([
        "include/**/*.h",
        "src/core/**/*.h",
        "src/ai/**/*.h",
        "src/commands/**/*.h",
        "src/map/**/*.h",
        "src/sim/**/*.h",
    ]):
        rel = path.relative_to(ROOT).as_posix()
        if rel == "include/realm.h":
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if realm_include.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: public/domain headers must not include the realm umbrella: {line.strip()}"
                )

    for path in iter_files([
        "src/core/*_service.*",
        "src/commands/*.cpp",
        "src/ai/*.cpp",
        "src/map/*.cpp",
        "src/sim/*.cpp",
    ]):
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if domain_global_event.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: domain/services must emit through EventSink/GameContext, not global UI event helpers: {line.strip()}"
                )

    for path in iter_files(["src/core/*_service.h", "src/core/*_service.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if no_context_index_overload.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: no-context service overloads must not rebuild WorldIndex internally: {line.strip()}"
                )

    for path in iter_files(["src/render/**/*.h", "src/render/**/*.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        if rel == "src/render/render_model.cpp" or rel in LEGACY_RENDERER_DIRECT_G_READS:
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if renderer_direct_g.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: renderers must use RenderModel instead of direct g.map/g.entities/selection reads: {line.strip()}"
                )

    for path in iter_files(["src/commands/*.cpp", "src/render/**/*.cpp", "src/platform/*.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        if rel in {"src/commands/command_runner.cpp", "src/commands/command_dispatcher.cpp"}:
            continue
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if duplicate_dispatch_wrapper.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: app command dispatch must go through command_runner, not duplicate wrappers/GameContext builders: {line.strip()}"
                )

    for path in iter_files(["src/ai/*.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if ai_hardcoded_cost_gate.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: AI resource gates must use AITuning or canonical rule/STATS helpers: {line.strip()}"
                )

    for path in iter_files(["src/commands/*.h", "src/commands/*.cpp"]):
        rel = path.relative_to(ROOT).as_posix()
        for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
            if input_menu_table.search(code_part(line)):
                failures.append(
                    f"{rel}:{line_no}: input train/research/build menus must use rule metadata, not hardcoded local tables: {line.strip()}"
                )


def main() -> int:
    failures: list[str] = []
    for description, patterns, regex in RULES:
        for path in iter_files(patterns):
            rel = path.relative_to(ROOT)
            test_gated = False
            for line_no, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                stripped = line.strip()
                if stripped == "#ifdef REALM_ENABLE_WORLD_INDEX_STATS":
                    test_gated = True
                    continue
                if stripped == "#endif" and test_gated:
                    test_gated = False
                    continue
                if "WorldIndex build-count instrumentation must be test-gated" in description and test_gated:
                    continue
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

    add_custom_failures(failures)

    if failures:
        print("Architecture check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print("Architecture check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
