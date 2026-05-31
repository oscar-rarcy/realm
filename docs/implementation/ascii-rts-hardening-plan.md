# Realm hardening plan

Last updated: 2026-05-31

Current baseline commit: `63eda5a Build Windows GUI renderer and clean project layout`

## Goal

Harden the current Realm RTS prototype by keeping the existing compact C++ codebase reliable, testable, and easier to work on. This is not an ECS rewrite.

The project is no longer root-file-only or ncurses-only. It now has a Windows-first SDL2 GUI build and a retained terminal/ncurses frontend for Linux/macOS/WSL.

## Current project layout

```text
src/        C++ implementation files
include/    Project headers
docs/       Design notes, renderer notes, manual test plan, implementation plans
scripts/    Convenience launch/build scripts
build/      Generated object files and logs, ignored by git
bin/        Generated executables and Windows runtime DLLs, ignored by git
```

Important files:

* `Makefile`
* `README.md`
* `include/realm.h`
* `include/display.h`
* `include/gfx_renderer.h`
* `src/main.cpp`
* `src/main_gfx.cpp`
* `src/globals.cpp`
* `src/entity.cpp`
* `src/orders.cpp`
* `src/simulation.cpp`
* `src/ai.cpp`
* `src/input.cpp`
* `src/render.cpp`
* `src/gfx_renderer.cpp`
* `src/mapgen.cpp`
* `src/display.cpp`
* `docs/gfx-renderer.md`
* `docs/tests/manual-test-plan.md`
* `scripts/windows-build-and-run-gui.bat`

## Status key

* `[ ]` Not started
* `[~]` Partially done or needs verification
* `[x]` Done
* `[!]` Blocked or needs a decision

## Current verified baseline

These results are from the current Windows/MSYS2 UCRT64 environment plus WSL Ubuntu where noted.

```text
Baseline date: 2026-05-31
Platform: Windows, MSYS2 UCRT64
Primary build command:
  mingw32-make gfx
Clean build command previously verified:
  mingw32-make clean && mingw32-make gfx
Windows output:
  bin/realm.exe
Runtime DLL placement:
  bin/*.dll copied by Makefile
Runtime smoke command:
  REALM_SMOKE_TEST=1 bin/realm.exe
Runtime smoke result:
  exit code 0, realm-run.log reaches "realm: main screen ready"
Match smoke command:
  REALM_SMOKE_TEST=match REALM_SEED=2468 REALM_HUMAN_CORNER=1 REALM_BIOME=0 bin/realm.exe
Match smoke result:
  exit code 0, realm-run.log reaches "realm: match smoke complete tick=60"
Compiler warnings:
  Windows GUI build is currently clean under -Wall -Wextra
Terminal/ncurses build:
  WSL Ubuntu `make clean && make terminal`, exit code 0, no -Wall/-Wextra warnings
Debug/sanitizer build:
  Windows debug target implemented and verified; native Windows sanitizer documented as unsupported
  WSL Ubuntu `make sanitize`, exit code 0 with ASan/UBSan and REALM_TEST_LONG_TICKS=2000
Headless tests:
  implemented via mingw32-make test, exit code 0; default long simulation is 10,000 ticks
```

## Implemented in 2026-05-31 hardening pass

* [x] Cleared entities, projectiles, action markers, selections, groups, and transient match state on every new match while preserving vector reservation.
* [x] Added deterministic startup through `REALM_SEED`, `REALM_HUMAN_CORNER`, `REALM_BIOME`, `initGameWithSeed()`, and match-start logging.
* [x] Added occupied-start hostile-wildlife exclusion and corrected sheep clusters to occupied starts only.
* [x] Added guaranteed nearby wood and berry resources around starts.
* [x] Added a headless test target covering placement bounds, state names, traits, command bindings, reset, deterministic startup, supply reservation, town hall cost, start safety, save/load, and AI progression.
* [x] Added entity trait helpers for worker/gather/build/military/ranged/naval/siege/wildlife/dropoff/training categories.
* [x] Added explicit cargo/resource state and removed `gatherType` / `carrying` overloads.
* [x] Split training progress from research progress.
* [x] Added a shared occupancy grid helper used by placement and pathfinding.
* [x] Switched `g.entities` to `std::deque` so appending spawned entities does not invalidate active entity references.
* [x] Added save/load to `realm-save.txt`.
* [x] Added SDL and terminal diagnostics toggles.
* [x] Added recoverable validation/logging for stale cursor, selection, control group, target, marker, and projectile state, with stricter debug assertions in `make debug`.
* [x] Added SDL and terminal cursor-tile HUD details.
* [x] Added a shared SDL/terminal help overlay on `?`.
* [x] Kept train mode open after queueing so repeated unit keys queue more units.
* [x] Added temporary visual command markers.
* [x] Added debug, sanitizer-policy, test, and packaging Makefile targets/docs.
* [x] Split player order/group command logic from `src/entity.cpp` into `src/orders.cpp`.
* [x] Split simulation tick and validation recovery logic from `src/main.cpp` into `src/simulation.cpp`.
* [x] Added deterministic SDL match smoke mode with `REALM_SMOKE_TEST=match`.
* [x] Centralized death-reference cleanup so normal combat no longer produces stale-target validation repairs.

## Player feedback incorporated

Source: informal player feedback reviewed on 2026-05-31.

The feedback was used as a signal to find real issues, not as a direct implementation spec. Accepted hardening items from that review:

* [x] Fix match reset: starting a second game in the same process must clear old entities and projectiles.
* [x] Add a two-games-in-one-process regression or smoke test.
* [x] Add occupied-start safety checks so hostile wildlife and enemy-owned units cannot begin too close to an active starting base.
* [x] Re-test early "enemy militia near town hall" reports after reset is fixed. Initial spawn currently creates town halls and peasants only.
* [x] Improve SDL HUD cursor-tile information: show terrain, biome, resource, and visible unit/object stack for the square under the cursor, especially when nothing is selected.
* [x] Improve training flow so repeated unit-key presses can queue more units without accidentally pausing immediately after the first peasant.
* [x] Expand the visible legend/help for owner colours, neutral animals, resources, landmarks, red alert markers, danger markers, resign, and exit.
* [~] Tune early boar/sheep behaviour only after reset and start-safety fixes are verified.
* [x] Add a lightweight visual command marker for issued tasks.
* [x] Add deterministic seed/startup controls so spawn and balance reports are reproducible.
* [x] Add save/resume support for debugging and longer play sessions.

Feedback items not accepted as immediate implementation work:

* Mouse and keyboard already drive the same game cursor in SDL; only terminal-specific friction needs repro.
* Boars are already supposed to target units rather than buildings; any building attack report needs reproduction.
* Enemy peasants should already use owner-specific colours; add regression/manual checks before changing the palette.

## Accepted decisions and current status

These are the decisions from the earlier review, updated to match the current code.

* [x] AI may pull peasants from gathering/returning when construction is important.
  * Implemented in `src/ai.cpp` via `aiWorker()`.
* [x] Forest-like terrain is non-buildable.
  * Implemented in `canPlace()` for `T_FOREST`, `T_PINE`, `T_PALM`, and `T_DEAD_TREE`.
* [x] Forest-like terrain remains passable for now.
  * `isPassable()` does not block forest terrain.
* [x] Berry bushes are gatherable food.
  * `orderGather()`, `S_GATHERING`, `S_RETURNING`, AI gathering, mapgen, and rendering all know about `T_BERRY`.
* [x] Starting town halls may be free through setup/spawn logic.
  * `initGame()` still spawns starting town halls directly.
* [x] Player/AI-built town halls must have a real resource cost.
  * `E_TOWNHALL` has cost `200g/150w` in `src/globals.cpp`.
* [x] Enemy owners 1, 2, and 3 should not render as animals.
  * Terminal renderer has owner colour pairs for players 0-3.
  * SDL renderer also treats non-nature owners as faction-owned.
* [x] Do not rewrite into ECS.
  * Current structure remains a compact procedural C++ game.
* [x] Split overloaded entity fields where practical.
  * `gateOpen`, `gateLocked`, cargo/resource state, farm stored food, production rally, training progress, and research progress are now explicit fields.
* [~] Replace repeated hard-coded entity-type checks with explicit capabilities/traits where practical.
* [x] Add headless tests rather than relying only on manual playtesting.
* [x] Add build/dependency documentation.
  * Windows, GUI, terminal basics, project layout, and smoke test are documented.
  * Debug/test build docs now exist.

## Completed since the original plan

* [x] Reorganized source into `src/`, headers into `include/`, scripts into `scripts/`, and reference notes into `docs/reference/`.
* [x] Updated `.gitignore` so generated `bin/`, `build/`, object files, DLLs, executables, and logs do not clutter git.
* [x] Added `README.md`.
* [x] Added SDL2/SDL_ttf graphical renderer.
* [x] Added native Windows/MSYS2 UCRT64 GUI build path.
* [x] Changed native Windows output to `bin/realm.exe`.
* [x] Made Windows build copy required runtime DLLs beside `bin/realm.exe`.
* [x] Added `REALM_SMOKE_TEST=1` runtime smoke path.
* [x] Added `realm-run.log` startup milestones.
* [x] Fixed SDL `SDL_main` link issue by including SDL main handling in the GUI entrypoint.
* [x] Fixed the previous warning set:
  * misleading indentation in `orderBuild()` and `orderTrain()`
  * invalid `case '='+128` in `display.cpp`
  * misleading indentation in `gfx_renderer.cpp`
* [x] Added SDL isometric/top-down projection switch.
* [x] Added middle-button drag panning in SDL.
* [x] Changed in-match controls so `Q` resigns/returns to menu and `X` exits the app.
* [x] Kept `R` for rally/research to avoid conflicting gameplay bindings.

## Phase 1: Safety and correctness

### 1.1 Placement bounds

Status: `[x]`

Implemented:

* [x] `canPlace()` has a top-level `inBounds(x, y)` guard.
* [x] Farm terrain reads are protected by that guard.
* [x] Multi-tile footprint checks still validate every footprint tile.

Tests:

* [x] Headless tests cover out-of-bounds farm and building placement.

### 1.2 Safe entity-state rendering

Status: `[x]`

Implemented:

* [x] Terminal renderer uses `stateName(EntityState)` switch.
* [x] SDL renderer uses `stateName(EntityState)` switch.
* [x] `S_ENTERING` and `S_GARRISONED` are covered.
* [x] Invalid/default values return `"Unknown"`.

Tests:

* [x] Headless tests cover all valid `EntityState` names plus an invalid-state fallback.

### 1.3 Enemy owner rendering

Status: `[x]` for implementation, `[~]` for automated tests

Implemented:

* [x] Terminal owner colour pairs exist for owners 0, 1, 2, and 3.
* [x] Owner colours are separate from neutral animal colours.
* [x] SDL renderer treats player slots separately from `OWNER_NATURE`.

Tests:

* [x] Trait tests cover neutral hostile wildlife classification separately from military/player categories.
* [~] Renderer colour classification is still primarily covered by manual SDL/terminal checks rather than golden-frame tests.

### 1.4 Supply reservation

Status: `[x]`

Implemented:

* [x] Added `reservedSupply(int owner)` in `src/entity.cpp`.
* [x] Includes live units, currently-producing units, and queued units.
* [x] `orderTrain()` checks reserved supply.
* [x] AI uses `orderTrain()`, so it goes through the same cap rule.

Tests:

* [x] Headless tests queue over the cap and assert reserved supply never exceeds `supplyMax`.
* [x] UI population uses the same `reservedSupply()` forecast path.

### 1.5 Town hall cost

Status: `[x]` for implementation and core tests, `[~]` for final balance

Implemented:

* [x] `E_TOWNHALL` now costs `200g/150w`.
* [x] Starting town halls are still spawned directly in setup.
* [x] Ordered town hall construction uses normal affordability/deduction logic.
* [x] AI expansion uses `orderBuild()`, so it pays the same cost.

Tests:

* [x] Headless tests confirm starting town halls spawn through setup and ordered town halls deduct cost.
* [~] Final balance cost remains deferred until longer play sessions.

### 1.6 Enum-indexed lookup audit

Status: `[x]`

Done:

* [x] Known unsafe state-name lookup was replaced.
* [x] Debug and test targets now exist.
* [x] `make debug` builds the headless tests with debug symbols, libstdc++ assertions, and `REALM_DEBUG_ASSERTS`.

Still needed:

* [x] Search/audit all enum-indexed arrays touched in this pass; unsafe state-name indexing was replaced, and remaining fixed arrays are either range-controlled or visual lookup tables.
* [x] Native Windows sanitizer remains intentionally disabled; WSL Ubuntu `make sanitize` is verified.

### 1.7 Match reset between games

Status: `[x]`

Problem:

* `initGame()` resets counters, selections, players, and map state, but the match-scoped entity/projectile containers are only reserved, not cleared.
* Starting a new game in the same process can therefore leave old units, buildings, animals, and projectiles in the next match.
* This likely explains playtest reports of unfair spawns, enemy units near the player town hall, or immediate danger after playing multiple games in a row.

Required change:

* [x] Clear `g.entities` at the start of `initGame()`.
* [x] Clear `g.projectiles` at the start of `initGame()`.
* [x] Reset any other match-scoped transient state discovered while making this change.
* [x] Keep capacity reservation if it is still useful for reference-invalidation mitigation.

Tests:

* [x] Start a game, record live entity count and owner/type summary.
* [x] Start a second game in the same process.
* [x] Confirm the second game contains only the expected initial entities plus freshly generated wildlife/resources.
* [x] Confirm no stale projectiles survive the restart.
* [x] Confirm `nextId` and entity IDs are coherent after restart.

## Phase 2: Gameplay decisions

### 2.1 AI construction deadlock

Status: `[x]`

Implemented:

* [x] `aiWorker()` prefers idle peasants.
* [x] If none are idle, it pulls peasants from `S_GATHERING` or `S_RETURNING`.
* [x] AI houses, military buildings, towers, farms, docks, castles, and town halls use this worker path.

Tests:

* [x] Headless tests run a deterministic 5,000 tick AI progression scenario.
* [x] Tests assert the AI increases supply capacity or starts production-building progress instead of stalling at 10/10.

### 2.2 Forest non-buildable, still passable

Status: `[x]`

Implemented:

* [x] `canPlace()` rejects `T_FOREST`, `T_PINE`, `T_PALM`, and `T_DEAD_TREE`.
* [x] `isPassable()` still allows those terrain types.

Tests:

* [x] Headless tests assert forest terrain is passable but non-buildable.

### 2.3 Berries as food

Status: `[x]`

Implemented:

* [x] Berries appear in map generation.
* [x] Peasants can gather berries.
* [x] Berries return food.
* [x] Berry resources deplete and become grass.
* [x] AI can choose berries as a resource.
* [x] Terminal and SDL renderers show berry terrain.

Tests:

* [x] Headless tests cover berry gathering, food delivery, and depletion to grass.

### 2.4 Starting resources

Status: `[x]`

Implemented:

* [x] Starting areas are cleared.
* [x] Gold is placed near each corner start.
* [x] Sheep spawn near each corner.
* [x] Wild deer, boar, wolves, berries, wheat, and forests are generated.

Tests:

* [x] `initGameWithSeed()` provides deterministic repeated mapgen checks.
* [x] Occupied starts receive nearby guaranteed wood and berries.
* [x] Headless tests assert reachable wood, food, and gold across deterministic seed ranges.

### 2.5 Food economy clarity

Status: `[x]` for current mechanics/docs, `[~]` for final balance

Currently implemented food sources:

* [x] Berries
* [x] Hunting animals
* [x] Farms and farm tending
* [x] Wheat/farm interactions
* [x] Fishing
* [x] Winter food pressure

Done:

* [x] README and in-game help document food sources, drop-off behavior, farm/mill tending, and winter starvation.
* [x] SDL and terminal help use the same command/food/winter notes.
* [~] Peasant gold cost remains a balance decision after longer play sessions.

### 2.6 In-game help/reference screen

Status: `[x]`

Done:

* [x] `?` toggles a help overlay.
* [x] The shared command table feeds SDL and terminal help text.
* [x] Help includes selection, command/move/attack/gather, build, train, rally/research, groups, diagnostics, save/load, pause, resign, exit, cancel, SDL zoom/pan/projection, food, winter, owner colours, neutral animals, and combat alerts.

### 2.7 Starting safety and early danger

Status: `[x]`

Problem:

* Player feedback reported peasants spawning near boars, enemy units, or enemy bases and dying almost immediately.
* The strongest confirmed cause is the match-reset bug in Phase 1.7.
* A separate likely issue remains: wildlife is generated after starts and boars/wolves do not have an explicit no-spawn radius around occupied starts.

Required change:

* [x] Fix Phase 1.7 before tuning balance.
* [x] Add an occupied-start exclusion radius for hostile wildlife, especially boars and wolves.
* [x] Ensure enemy-owned initial entities cannot be placed close to the human start except through intended corner placement.
* [x] Re-test reports of early enemy militia after reset; initial spawn should remain town hall plus peasants only.
* [x] Add a danger/readability pass only after invalid early danger has been removed.

Tests:

* [x] Across deterministic seeds, assert no boars or wolves spawn within the chosen safety radius of occupied starts.
* [x] Across deterministic seeds, assert enemy initial units/buildings are only at valid occupied starts.
* [x] Run two games in one process and repeat the same safety checks on the second game.

### 2.8 Early animal balance

Status: `[x]`

Problem:

* Boars can kill early peasants too reliably if they are close to a start.
* Sheep may move/flee fast enough that new players find them awkward to interact with.

Required order:

* [x] First fix match reset.
* [x] Then add start exclusion for hostile wildlife.
* [~] Then tune boar aggression, attack, alert radius, or speed only if playtests still show unfair early losses.
* [~] Tune sheep movement/flee behaviour only after core safety issues are fixed and manual playtests still show friction.

Do not change yet:

* [x] Do not nerf boars solely from reports that may have been caused by stale entities.
* [x] Do not change boar building targeting without a reproduction; boars currently target units.

### 2.9 Deterministic startup and reproducible scenarios

Status: `[x]`

Problem:

* Spawn, mapgen, wildlife, and balance bugs are hard to act on when a playtest report cannot be reproduced.
* Current startup uses random choices for map generation and starting corner.

Required change:

* [x] Add a deterministic seed input for map generation and simulation startup.
* [x] Allow debug launch options for AI count, biome, human starting corner, and possibly projection/renderer.
* [x] Log the seed, biome, AI count, starting corner, and build/version information at match start.
* [x] Make playtest reports actionable by asking for or recording the seed and startup options.
* [x] Keep normal "random game" behaviour as the default.

Tests:

* [x] Same seed/options produce the same starting map and initial entity summary.
* [x] Different seeds still produce varied maps.
* [x] Seed and startup options are recorded in `realm-run.log`.

## Phase 3: Architecture cleanup

### 3.1 Overloaded `Entity` fields

Status: `[x]`

Done:

* [x] Added explicit `gateOpen`.
* [x] Added explicit `gateLocked`.
* [x] Updated gate rendering, passability, toggling, and auto-open logic.
* [x] Replaced `gatherType` with explicit `CargoResource` state.
* [x] Replaced unit `carrying` with explicit `Cargo` amount/type/source.
* [x] Replaced farm `carrying` reuse with `storedFood`.
* [x] Replaced resource/farm return reuse of `rallyX` / `rallyY` with `resourceX` / `resourceY` and cargo source coordinates.
* [x] Replaced shared `prodProgress` / `prodTime` with separate training and research progress fields.
* [x] `rallyX` / `rallyY` now cover production rally points only.

Next step:

* [x] Introduce a small resource/cargo representation before touching more systems.

### 3.2 Entity capabilities and traits

Status: `[~]`

Problem:

* The simulation often asks "is this exact type?" where it should ask "can this entity do this?" or "does this entity belong to this category?"
* Examples to reduce over time:
  * `type == E_PEASANT` should usually become a capability such as `canGather`, `canBuild`, or `isWorker`.
  * `type == E_MILITIA || type == E_ARCHER || ...` should usually become traits such as `isMilitary`, `isInfantry`, `isRanged`, `isSiege`, or `canAttack`.
  * `type == E_WOLF || type == E_BOAR` style checks should usually become traits such as `isWildAnimal`, `isCarnivore`, `isHostileWildlife`, or `attacksUnits`.
  * Building checks should use traits such as `isBuilding`, `trainsUnits`, `isDropoff`, `storesUnits`, or `isDefense`.
* This matters because adding a new unit/building currently requires hunting through scattered type lists, which is easy to miss.

Required change:

* [x] Add a compact capability/trait representation to the entity definition data, probably near `EntityStats`.
* [x] Prefer named helper functions such as `canGather(type)`, `isInfantry(type)`, `isHostileWildlife(type)`, and `isDropoff(type)` over scattered conditionals.
* [x] Start with traits that remove real duplication in AI, input/orders, combat targeting, rendering classification, and HUD display.
* [ ] Keep exact-type checks only where the specific type genuinely matters, such as a town hall having a specific training menu.
* [x] Do this incrementally; do not turn it into a large data-driven rewrite before tests exist.

Tests:

* [x] Worker capability tests cover peasants.
* [x] Combat category tests cover infantry, ranged, siege, ships, and hostile wildlife.
* [~] Owner/render classification tests distinguish neutral animals from enemy categories through traits, but renderer golden-frame tests are still absent.
* [~] Adding a new military unit is easier through trait data; some exact-type checks remain for specific menus, building behavior, and animal-specific behavior.

### 3.3 Reference invalidation risk from `g.entities`

Status: `[x]`

Current state:

* `g.entities` is now a `std::deque`, so appending spawned entities during a tick does not invalidate active entity references/pointers.
* A 10,000 tick deterministic AI simulation now runs through the headless harness.

Done:

* [x] Add deferred spawn queue, or switch container strategy if safer.
* [x] Keep long simulation coverage to catch regressions.

Follow-up:

* [x] Extend the long simulation target beyond the original 5,000 tick AI progression run.

### 3.4 Occupancy-grid helper

Status: `[x]`

Current state:

* [x] Pathfinding pre-scans buildings into a flat boolean map for faster blocked-tile checks.
* [x] Shared `OccupancyGrid` helper exists for placement and pathfinding.
* [x] The helper keeps garrisoned/dead entities out of occupancy.

Follow-up:

* [~] Consider using the helper in renderer tile stack summaries if render/selection occupancy logic grows further.

### 3.5 Split `entity.cpp`

Status: `[x]`

Current state:

* [x] AI code is now in `src/ai.cpp`.
* [x] Player order/group command logic is now in `src/orders.cpp`.
* [x] Simulation tick and validation recovery logic is now in `src/simulation.cpp`.
* [ ] `src/entity.cpp` still contains economy, combat, construction, projectiles, weather/seasons, garrisoning, and win checks.

Still needed:

* [x] Split only after tests exist, or when touching a subsystem for functional work.
* [ ] Candidate files: `economy.cpp`, `combat.cpp`.

### 3.6 Render/input/simulation boundaries

Status: `[~]`

Improved:

* [x] SDL frontend calls shared command/select helpers in `src/input.cpp`.
* [x] SDL renderer has its own frontend file and does not require ncurses.
* [x] `tickSimulationOnce()` centralizes the simulation tick for SDL, terminal, and headless tests.
* [x] `tickSimulationOnce()` now lives outside frontend startup code in `src/simulation.cpp`.
* [x] Headless simulation builds without ncurses or SDL.

Still needed:

* [ ] Avoid adding more simulation mutation to render code.
* [ ] Move shared state-display helpers if both frontends continue duplicating them.

### 3.7 Central command and keybinding registry

Status: `[~]`

Problem:

* Controls, HUD hints, menu text, help overlays, SDL input, and terminal input can drift from each other.
* Recent changes such as `Q` resigning and `X` exiting are easy to document in one renderer and miss in another.

Required change:

* [x] Add a shared command/keybinding definition table for gameplay commands.
* [x] Use the shared table for help text and renderer help overlays.
* [x] Include command id, display label, key(s), allowed modes, and short help text.
* [x] Keep renderer-only inputs, such as mouse-wheel zoom or middle-button pan, clearly scoped but still documented from a shared source.
* [x] Add a simple check that every documented gameplay command has an input binding.

Tests:

* [x] Help/control text includes resign, exit, pause, train, build, rally, attack-move, groups, help, save/load, diagnostics, renderer-only controls, and escape/cancel.
* [x] SDL and terminal help overlays agree for shared commands because they read the same table.
* [x] Updating a binding in the table updates the displayed help text.

### 3.8 Runtime diagnostics and debug overlay

Status: `[x]`

Problem:

* Reports about stuck modes, odd selections, stale entities, or confusing spawns require inspecting internal state.
* Logs now prove splash startup and deterministic match-smoke startup, and diagnostics expose match-level state during debugging.

Required change:

* [x] Add a debug diagnostics mode or overlay that can show cursor tile, selected entity id/type/state/order/target, current mode, entity count, projectile count, seed, AI count, biome, and tick.
* [x] Add log milestones for match start, resign/return-to-menu, match restart, save, load, and recoverable validation errors.
* [x] Keep diagnostics out of the normal player-facing UI unless explicitly enabled.
* [x] Make diagnostics available in SDL first; terminal can follow if useful.

Tests:

* [x] Headless reset tests and match-start logs cover entity/projectile counts across second-game startup.
* [x] SDL and terminal diagnostics identify selected entity state and current command mode.
* [x] Match-start logs include seed, AI count, human corner, biome, entity count, and projectile count.

## Phase 4: Build, docs, warnings, tooling

### 4.1 Build/dependency documentation

Status: `[x]`

Done:

* [x] Root `README.md` exists.
* [x] Windows/MSYS2 dependencies documented.
* [x] Windows build and runtime smoke documented.
* [x] Unix-like GUI/terminal commands documented.
* [x] `docs/gfx-renderer.md` documents renderer-specific behavior.

Done:

* [x] README documents debug and test targets.
* [x] README and renderer docs document deterministic match smoke mode.
* [x] README and renderer docs document native Windows ncurses limitations and Unix-like terminal commands.
* [x] Makefile help reports the native Windows terminal limitation.

### 4.2 Debug/sanitizer Makefile target

Status: `[x]`

Still needed:

* [x] Add debug target.
* [x] Add sanitizer flags where available.
* [x] Account for platform-specific sanitizer availability on Windows/MSYS2.
* [x] Confirm terminal and/or headless debug link path.

### 4.3 Warning tracking

Status: `[x]`

Current warning log:

```text
Baseline warnings from the previous Windows GUI build:
- entity.cpp misleading indentation in orderBuild/orderTrain
- display.cpp switch case outside char range
- gfx_renderer.cpp misleading indentation

Current Windows GUI build:
- clean under -Wall -Wextra

Current WSL Ubuntu terminal build:
- clean under -Wall -Wextra

Current WSL Ubuntu sanitizer target:
- ASan/UBSan headless tests pass with REALM_TEST_LONG_TICKS=2000
```

Done:

* [x] Re-run terminal/ncurses build on WSL/Linux/macOS.
* [x] Record Windows debug findings once target exists.
* [x] Record sanitizer findings on WSL/Linux/macOS.

### 4.4 Windows packaging and release checklist

Status: `[x]` after final verification on Windows/MSYS2

Problem:

* The project now has a Windows-first GUI executable with runtime DLL dependencies.
* Build outputs are ignored, but there is no repeatable release/package checklist yet.

Required change:

* [x] Add a documented packaging checklist for `bin/realm.exe`, required DLLs, README, and any runtime assets.
* [x] Consider a packaging target that creates a clean zip from `bin/` after a successful build.
* [x] Verify packaged builds run outside the repo root.
* [x] Include `REALM_SMOKE_TEST=1` and `REALM_SMOKE_TEST=match` as packaging verification paths.
* [x] Document where logs are written and whether they are packaged or generated at runtime.

Tests:

* [x] Clean Windows GUI build succeeds.
* [x] Packaged output contains `realm.exe` and all required DLLs.
* [x] Packaged `realm.exe` launches and reaches the main screen smoke milestone.
* [x] Packaged `realm.exe` starts a deterministic match and reaches the match smoke milestone.
* [x] Generated logs and temporary files remain ignored by git.

### 4.5 Crash/assertion policy

Status: `[x]` for implemented policy and recoverable tests, `[~]` for broader invariant catalog

Problem:

* Debug builds should catch impossible state early, but release builds should avoid hard-crashing on recoverable bad state.

Required change:

* [x] Add debug assertions for invalid map coordinates, invalid entity ids, impossible entity states, stale selected units, and invalid orders.
* [x] Prefer recoverable validation/logging in release builds where bad input can be ignored safely.
* [~] Keep unrecoverable impossible entity/map state fatal in debug; release currently logs unrecovered validation failures and continues where possible.
* [x] Route repeated recoverable errors to throttled diagnostics/logging without flooding logs.

Tests:

* [x] Debug/headless tests run with `REALM_DEBUG_ASSERTS` and validation coverage.
* [x] Release/headless path handles invalid selection, control group, and cursor inputs without crashing.
* [x] Logs include phase, tick, seed, and validation error for recoverable errors.

## Phase 5: Tests

### 5.1 Automated headless tests

Status: `[x]`

This is now the highest-leverage next step.

Required first test harness:

* [x] Compile game logic without ncurses/SDL.
* [x] Exclude terminal render/input and SDL renderer.
* [x] Add a test executable, for example `tests/realm_headless_tests.cpp`.
* [x] Add `make test`.
* [x] Start with simple `assert()` tests unless a framework becomes useful.

Initial test groups:

* [x] Placement bounds.
* [x] Match reset between consecutive games.
* [x] Deterministic seed/startup reproducibility.
* [x] Mapgen invariants across many seeds.
* [x] Forest buildability/passability.
* [x] State-name coverage.
* [x] Enemy owner classification.
* [x] Supply reservation.
* [x] Town hall cost.
* [x] Berry gathering and depletion.
* [x] AI progression for 5,000 ticks.
* [x] Starting resource reachability.
* [x] Starting safety and hostile-wildlife exclusion.
* [x] Save/load round-trip.
* [x] Long simulation invariants currently run for 10,000 AI ticks.

### 5.2 Existing runtime smoke

Status: `[x]`

Implemented:

* [x] `REALM_SMOKE_TEST=1` exits after the SDL splash renders.
* [x] Success writes `realm: main screen ready` to `realm-run.log`.
* [x] Verified on Windows with exit code 0.
* [x] `REALM_SMOKE_TEST=match` starts a deterministic match, runs 60 simulation ticks, renders SDL frames, exits 0, and writes `realm: match smoke complete tick=60`.

Limitations:

* [x] Match smoke now starts a match.
* [x] Match smoke now exercises simulation, AI startup, and SDL rendering beyond the main screen.
* [~] It still does not synthesize interactive keyboard or mouse input.

### 5.3 Mapgen invariant tests

Status: `[x]`

Required checks:

* [x] Occupied starts are not boxed in.
* [x] Occupied starts have reachable wood, food, and gold.
* [x] Hostile wildlife does not spawn inside the start-safety radius.
* [x] Enemy starts are separated by a minimum distance through fixed corner starts.
* [x] Generated maps retain enough passable area for scouting and combat.
* [x] Resource and landmark placement does not overwrite starting town halls or peasants.
* [x] Tests run across deterministic seed ranges; failing assertions are tied to the current seed loop.

### 5.4 Save and resume

Status: `[x]` for initial debug-friendly save/load

Problem:

* Save/resume is useful for players, but also important for debugging rare bugs because a bad state can be captured and replayed.

Required change:

* [x] Define a versioned save format for match state.
* [x] Save enough state to resume exactly: map tiles/resources, entities, projectiles, players, AI timers, current tick, random seed/state if needed, selected/cursor/view state, current mode where safe, season/weather/day state, and victory state.
* [x] Prefer a readable/debuggable format at first unless file size becomes a real problem.
* [x] Add save and load commands behind explicit keys/menu actions.
* [x] On load, validate file version and reject incompatible/corrupt saves with a clear error.
* [x] Ensure save files do not get committed accidentally unless they are intentional fixtures.

Tests:

* [x] Save immediately after match start, load, and compare entity/player/map summary.
* [x] Save after combat/economy/training, load, and continue ticking without crash.
* [~] Save, exit process, restart, load, and resume remains a manual path; exact in-process save/resume is automated.
* [x] Invalid/corrupt save files fail cleanly.
* [x] A deterministic save fixture path is generated under `build/` during tests and proves exact resume across future ticks.

## Phase 6: Player-facing UX and manual smoke testing

### 6.1 HUD and cursor-tile information

Status: `[x]`

Required change:

* [x] When nothing is selected, show information for the square under the cursor instead of only "No selection".
* [x] SDL HUD should include cursor tile terrain, biome, resource, and visible unit/object stack.
* [x] Terminal and SDL should have equivalent cursor-tile semantics even if the layout differs.
* [x] If units are selected, keep selected-unit details prominent but still expose cursor-tile context where practical.
* [x] Keep resources at the top and controls/help at the bottom where the renderer layout supports it.

### 6.2 Training and command-mode controls

Status: `[x]`

Required change:

* [x] Decide whether train mode stays open after queueing a unit.
* [x] If train mode stays open, ensure repeated `P` queues peasants from a town hall.
* [x] Avoid accidental pause immediately after queueing one peasant.
* [x] Confirm `Esc` cancels build, train, wall, rally, attack-move, research, and any new command modes.
* [x] Update all control text after any keybinding change.

### 6.3 Legend, colour, and danger readability

Status: `[~]`

Required change:

* [x] Add or expand a clear colour key for player units, enemy owners, neutral animals, resources, and landmarks.
* [x] Explain pulsing red `!` combat alerts and any danger markers.
* [x] Verify enemy peasants do not use the same visual treatment as player peasants in SDL and terminal.
* [ ] Review sheep colour after owner-colour regression checks exist.
* [ ] Make resources and landmarks more visually distinct where the current glyph/colour is ambiguous.

### 6.4 Visual action markers

Status: `[x]`

Required change:

* [x] Add a lightweight marker when assigning a move, gather, attack, build, or rally task.
* [x] Keep the marker temporary and renderer-friendly.
* [x] Ensure it does not obscure selection, danger, or combat alerts.

### 6.5 Input and UI scale testing

Status: `[~]`

Required change:

* [x] Add manual checks for small windows, large windows, high-DPI displays, and text clipping.
* [ ] Run the small/large/high-DPI checks on physical displays.
* [ ] Verify HUD panels do not overlap the map or each other.
* [ ] Verify buttons/control text remain legible and do not overflow.
* [ ] Verify cursor, selected units, drag boxes, action markers, and danger markers remain visible at different zoom levels.
* [ ] Verify mouse and keyboard cursor movement remain coherent after resize/zoom/projection changes.

### 6.6 Manual smoke checklist

Current manual checklist lives in:

* `docs/tests/manual-test-plan.md`

Update needed:

* [x] Add SDL-specific checks for:
  * middle-button drag panning
  * mouse-wheel zoom
  * top-down/isometric toggle
  * `Q` resigns to menu during a match
  * `X` exits app
  * Windows launch from `bin/realm.exe`
  * cursor-tile HUD shows terrain, biome, resource, and entities
  * repeated peasant queueing does not accidentally pause
  * red alert/danger markers are explained
  * owner colours distinguish player, enemies, and neutral entities
  * deterministic seed/options are visible in logs or diagnostics
  * save/resume round-trip works from a running match
  * diagnostics overlay/logs show selected entity, mode, cursor tile, and entity counts
  * small/large/high-DPI window layouts do not clip important UI
* [x] Keep ncurses checks separate from SDL checks.

Manual ncurses result:

```text
Manual smoke test date:
Platform:
Terminal:
Result:
Issues found:
```

Manual SDL result:

```text
Manual smoke test date:
Platform:
Renderer:
Result:
Issues found:
```

## Recommended next work order

Do these before more gameplay changes:

1. Fix match reset between consecutive games.
2. Add a two-games-in-one-process regression or smoke test.
3. Add a deterministic seed or test hook for mapgen.
4. Add occupied-start safety checks for hostile wildlife and enemy initial entities.
5. Add a headless test harness and `make test`.
6. Add placement, state-name, supply reservation, town hall cost, berry, reset, and start-safety tests.
7. Add an AI progression test.
8. Add debug/sanitizer target.
9. Re-run terminal/ncurses build after the project layout change.
10. Update manual test plan with SDL-specific controls and player-feedback checks.
11. Add logging of seed/startup options so manual reports are reproducible.

Then continue gameplay hardening:

1. Guarantee reachable starting wood/food/gold for every start.
2. Improve HUD cursor-tile information and selected/tile entity display.
3. Improve training-mode flow and pause/training key UX.
4. Add in-game help overlay.
5. Clarify/document food economy rules.
6. Add legend/danger-marker explanations.
7. Add visual command markers.
8. Tune boars/sheep only after reset and spawn-safety checks are verified.
9. Add entity capability/trait helpers and replace the highest-risk hard-coded type lists.
10. Add a central command/keybinding registry.
11. Add runtime diagnostics overlay/logging.
12. Add save/resume with a versioned debug-friendly save format.
13. Add packaging/release checklist.
14. Add input/UI scale testing.
15. Continue reducing overloaded `Entity` fields.
16. Address `g.entities` reference invalidation risk.
17. Consider occupancy grid only after tests exist.

## Definition of done for the next hardening pass

The next hardening pass is complete when:

* [x] `mingw32-make clean && mingw32-make gfx` passes on Windows.
* [x] `REALM_SMOKE_TEST=1 bin/realm.exe` exits 0 and logs `realm: main screen ready`.
* [x] Terminal build is verified on a Unix-like environment, or limitation is documented.
* [x] `make test` exists and passes.
* [x] Debug/sanitizer target exists or platform limitation is documented.
* [x] Starting a second game in the same process does not retain stale entities or projectiles.
* [x] Deterministic seed/options can reproduce a starting map and are logged.
* [x] AI progression test proves AI no longer stalls at 10/10.
* [x] Start-safety checks pass across deterministic seeds.
* [x] Save/resume round-trip works and has at least one regression fixture path.
* [x] Long simulation test passes.
* [x] Runtime diagnostics can show match seed, mode, cursor tile, selected entity state, entity count, and projectile count.
* [~] Manual SDL smoke checklist is updated and run.
* [~] Manual ncurses smoke checklist is updated and run if terminal support remains in scope.
* [x] README/docs match actual commands.
* [x] This plan is updated again with results.

## Do not do in this pass

* [x] Do not rewrite into ECS.
* [x] Do not redesign all pathfinding around blocking forests.
* [x] Do not hide failing tests by weakening assertions.
* [x] Do not treat manual playtesting as a substitute for headless tests.
* [x] Do not make a large `entity.cpp` split before there is test coverage for moved behavior.

## Deferred follow-ups

```text
Deferred item: Full balance pass.
Reason: Town hall cost and food economy are now functional enough to test, but not final.
Risk: AI/player pacing may feel uneven.
Suggested follow-up: Balance after deterministic tests and manual play sessions.

Deferred item: Full renderer parity.
Reason: SDL and terminal frontends now share simulation but not all UI affordances.
Risk: Controls/help can drift.
Suggested follow-up: Add shared control/help text source or renderer-specific manual checks.
```
