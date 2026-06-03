# Adding Units, Buildings, And Terrain

This guide keeps common gameplay additions local to the definition and ownership files created by the refactor.

## Add A Unit Or Building

1. Add the enum value in `include/realm.h` before `E_TYPE_COUNT`.
2. Add one `STATS` entry in `src/core/entity_defs.cpp`.
3. Add unique type predicates only if the existing `EntityStats` traits cannot describe the behavior.
4. Add training or build-menu behavior in `src/commands/orders.cpp`, `src/commands/command_dispatcher.cpp`, or `src/commands/input_controller.cpp` only when the unit is actually player-commandable.
5. Add AI usage in `src/ai/ai_economy.cpp`, `src/ai/ai_production.cpp`, or `src/ai/ai_combat.cpp` only if AI should build, train, or command it.
6. Add unique animation or asset slugs in `src/core/entity_animation.cpp` and `src/render/sdl/tileset_assets.cpp` only if the default mapping is not enough.
7. Add display handling in `src/render/ascii/display_glyphs.cpp` or `src/render/sdl/display_glyphs.cpp` only if the existing visual path cannot infer it.
8. Add a focused headless test when the new entity has gameplay behavior beyond static stats.

The baseline entity data should stay in one place: `src/core/entity_defs.cpp`.

## Add Terrain

1. Add the enum value in `include/realm.h` before `TERRAIN_COUNT`.
2. Add one `TERRAIN_DEFS` entry in `src/core/terrain_defs.cpp`.
3. Put shared terrain rules in `src/core/terrain_defs.cpp`.
4. Put gathering behavior in `src/sim/gathering_system.cpp` only if the resource flow is unique.
5. Put placement rules in map-generation passes under `src/map/`.
6. Add renderer display handling only when `visualPartsForTile()` cannot classify the terrain through the definition data.
7. Add a headless test for passability, placement, resource type, and any seasonal behavior.

The baseline terrain data should stay in one place: `src/core/terrain_defs.cpp`.

## Add A Command

1. Add the command type and payload fields in `include/realm.h`.
2. Resolve context commands in `src/commands/command_resolver.cpp`.
3. Dispatch direct commands in `src/commands/command_dispatcher.cpp`.
4. Keep validated gameplay mutations in `src/orders.cpp` or the appropriate `src/sim/` system.
5. Make terminal, SDL, mobile, and web paths emit the same command instead of duplicating rules.
6. Add a headless regression test for the command result.
