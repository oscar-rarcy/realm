# Refactor Roadmap (condensed)

The full, authoritative plan lives in `docs/implementation/refactor-plan.md`. This file is the
short working checklist and records the established conventions/safety net.

## Safety net (Phase 0)
- Build + tests: `make test` (Linux/macOS/WSL) or `mingw32-make test` (Windows/MSYS2 UCRT64).
- Deterministic harness: `initGameWithSeed(numAI, seed, humanCorner)` in
  `tests/realm_headless_tests.cpp`; covers spawn, resources, commands, ticking, save/load,
  `validateGameState()`, and mapgen invariants across seeds.
- Warnings: `-Wall -Wextra` always on (see `Makefile`).

## Architectural rules
- Platform code reads devices. Input code creates intents/commands. Command/domain code applies
  player + AI actions. Simulation advances time. Render observes. AI plans (does not mutate
  directly). Save serializes + migrates.
- No layer mutates global `g` except through its assigned boundary.

## Input policy (Phase 1.1)
- `X` = hold position during gameplay.
- `Q` = resign / return to main menu.
- `X` exits the application only on the game-over screen; otherwise exit via the menu.

## Progress
- [x] Phase 0 safety net (pre-existing harness) + roadmap.
- [x] Phase 1.1 `x`/`X` input conflict.
- [x] Phase 1.2 wall-line build ownership/cost/validation.
- [x] Phase 1.3 / 3.3 shared research service (player + AI, AI now pays/uses canonical durations).
- [x] Phase 3.1 production food cost in `EntityStats` (`costFood`), `orderTrain` switch removed.
- [x] Phase 3.1/3.2 production and build services (`startTraining`, `startBuild`, `startBuildLine`).
- [x] Phase 3.4 market trade service (`MarketTrade` command + rate table).
- [x] Phase 2.1/2.2/2.3 (partial) explicit command payloads for implemented commands, no
      box-select coordinate packing, group/context execution uses command selection instead of
      current global selection, dispatcher switch has no silent default.
- [x] Phase 4 (initial) `GameEvent`/`EventSink`/`LegacyUiEventSink`; command/domain services and
      order helpers emit events instead of direct `setStatus()` / `addActionMarker()` calls.
- [x] Phase 5 (initial) `EntityId`/`PlayerId`, `GameContext`, `UiContext`, command dispatcher
      accepts `GameContext&` with a legacy `Game&` wrapper.
- [x] Phase 6 (initial) `WorldIndex` with id/owner/tile/occupancy/resource indexes and parity tests.
- [x] Phase 8 (initial) save version constant + supported-version gate + migration hook; v8 saves
      migrate into current v9 format.
- [x] Phase 14 (initial) `scripts/check_architecture.py` + `make architecture-check` for migrated
      command/domain boundaries.
- [ ] Remaining structural work: finish input-intent split, route save/load and more UI actions
      through commands, thread event sinks instead of using the global legacy sink, migrate hot
      query paths and AI to `WorldIndex`, split AI combat/economy/planning, fully separate save
      parse/migrate/hydrate/validate, consolidate render model consumers, replace empty wrapper
      headers, remove legacy wrappers/global `g` use from migrated layers, and expand architecture
      checks as each boundary is migrated.

## Notes for continuation
- Domain services live under `src/core/` (auto-globbed by both `Makefile` and
  `scripts/build-web.sh`); adding a new top-level `src/` dir requires editing both build files.
- The `Makefile` does NOT track header dependencies: after editing any header, run
  `mingw32-make clean` before rebuilding to avoid stale-object struct-layout mismatches.
- Commit only specific files; the tree has many unrelated staged migration changes.
