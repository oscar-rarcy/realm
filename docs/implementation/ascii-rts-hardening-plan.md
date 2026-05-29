# ASCII RTS hardening plan

## Goal

Harden the current ASCII RTS prototype by fixing correctness bugs, applying agreed gameplay decisions, adding repeatable tests, and documenting remaining follow-up work.

This is not a rewrite. Keep the project compact and C++/ncurses-based. Do not convert the game to ECS. Split files and clarify state only where it reduces current risk.

Relevant project files currently include:

* `main.cpp`
* `realm.h`
* `globals.cpp`
* `entity.cpp`
* `input.cpp`
* `render.cpp`
* `mapgen.cpp`
* `Makefile`
* `.gitignore`
* `realm.command`

---

## Status key

* `[ ]` Not started
* `[~]` In progress
* `[x]` Done
* `[!]` Blocked / needs decision

---

## Accepted decisions

These decisions are settled for this pass.

* [ ] AI may pull peasants from gathering/returning when construction is important.
* [ ] Forest-like terrain is non-buildable.
* [ ] Forest-like terrain remains passable for now.
* [ ] Berry bushes are gatherable food.
* [ ] Starting town halls may be free through setup/spawn logic.
* [ ] Player/AI-built town halls must have a real resource cost.
* [ ] Enemy owners 1, 2, and 3 should not render as animals.
* [ ] Do not rewrite into ECS.
* [ ] Split overloaded entity fields where practical.
* [ ] Add headless tests rather than relying only on manual ncurses playtesting.
* [ ] Add build/dependency documentation.

---

## Phase 0: Baseline

### Tasks

* [ ] Record current normal build result.
* [ ] Record current warning output.
* [ ] Record whether `<ncurses.h>` is available locally.
* [ ] Record whether the game launches locally.
* [ ] Record whether the game can run for several minutes without crashing.
* [ ] Record any obvious current runtime issues before making changes.

### Notes to fill in

```text
Baseline date:
Compiler:
Platform:
ncurses available:
Normal build command:
Normal build result:
Warnings:
Runtime smoke result:
Known baseline issues:
```

---

## Phase 1: Safety and correctness fixes

### 1.1 Add top-level bounds check to `canPlace()`

#### Problem

`canPlace()` can read `g.map[y][x]` before checking whether `x,y` are in bounds, especially for farms.

#### Required change

* [ ] Add an immediate `inBounds(x, y)` check at the top of `canPlace()`.
* [ ] Ensure every direct `g.map[y][x]` read in placement logic is protected.
* [ ] Keep existing multi-tile footprint bounds logic.

#### Tests

* [ ] `canPlace(E_FARM, -1, 0, owner)` returns false and does not crash.
* [ ] `canPlace(E_FARM, 0, -1, owner)` returns false and does not crash.
* [ ] `canPlace(E_FARM, MAP_W, 0, owner)` returns false and does not crash.
* [ ] `canPlace(E_FARM, 0, MAP_H, owner)` returns false and does not crash.
* [ ] Valid farm placement still works on allowed terrain outside winter.

---

### 1.2 Make entity-state rendering safe

#### Problem

The renderer uses a fixed state-name array that does not cover every `EntityState`, especially `S_ENTERING` and `S_GARRISONED`.

#### Required change

* [ ] Add a safe helper, e.g. `stateName(EntityState state)`.
* [ ] Use a `switch`, not unchecked array indexing.
* [ ] Cover every defined `EntityState`.
* [ ] Return `"Unknown"` for invalid/default values.
* [ ] Replace all direct state-name array indexing.

#### Tests

* [ ] Every defined `EntityState` returns non-null display text.
* [ ] Selecting an entity in `S_ENTERING` does not crash.
* [ ] Selecting an entity in `S_GARRISONED` does not crash.
* [ ] Debug/sanitizer build reports no out-of-bounds read from state display.

---

### 1.3 Fix enemy rendering for owners 2 and 3

#### Problem

The main map treats only `owner == 1` as enemy. Owners 2 and 3 can fall through into animal/default colouring.

#### Required change

* [ ] Treat any live entity with `owner > 0 && owner < MAX_PLAYERS` as enemy/faction-owned on the main map.
* [ ] Keep neutral animals classified separately.
* [ ] Make main-map logic consistent with minimap logic.
* [ ] Optional: add faction-specific colours if this is simple. Do not block this pass on it.

#### Tests

* [ ] Owner 1 unit renders/classifies as enemy.
* [ ] Owner 2 unit renders/classifies as enemy.
* [ ] Owner 3 unit renders/classifies as enemy.
* [ ] Sheep/deer/wolves still render/classify as animals.
* [ ] Neutral entities do not accidentally become enemies.

---

### 1.4 Add supply reservation for queued and in-production units

#### Problem

Training checks only current supply. Queued and in-production units can allow actual supply to exceed the cap later.

#### Required change

* [ ] Add helper for current supply used.
* [ ] Add helper for reserved supply used.
* [ ] Reserved supply must include:

  * live units
  * currently producing unit in each production building
  * queued units in each production building queue
* [ ] `orderTrain()` must check reserved supply.
* [ ] UI population forecast should use the same helper or match its logic.
* [ ] AI training should use the same rule.

#### Tests

* [ ] At 9/10 supply, queueing one 1-supply unit succeeds.
* [ ] Immediately queueing another 1-supply unit is blocked.
* [ ] Completing queued units does not push actual supply above cap.
* [ ] Cancelling/removing queued items updates reserved supply if cancellation exists.
* [ ] Unit death updates current/reserved supply correctly.
* [ ] AI cannot overqueue past supply cap.

---

### 1.5 Add real build cost for town halls

#### Problem

Town halls appear to have zero cost, but expansion logic can build them. This creates free expansion.

#### Required change

* [ ] Assign a real cost to `E_TOWNHALL`.
* [ ] Starting town halls should still be spawned directly during setup without charging resources.
* [ ] Ordered construction of town halls must check affordability.
* [ ] Ordered construction of town halls must deduct resources.
* [ ] AI expansion must respect the same cost.
* [ ] Document the chosen placeholder cost if balance is not final.

#### Tests

* [ ] Starting game still creates starting town halls even with low starting resources.
* [ ] Player cannot build a town hall without required resources.
* [ ] Player resources are deducted when ordering town hall construction.
* [ ] AI cannot build free town halls.
* [ ] AI can still expand once it has enough resources.

---

### 1.6 Audit enum-indexed arrays and unchecked lookups

#### Problem

The state-name renderer is one known case, but similar unchecked enum-indexed arrays may exist.

#### Required change

* [ ] Search for array indexing by enum values.
* [ ] Confirm each indexed array covers every enum value or has bounds checks.
* [ ] Replace fragile cases with helper functions or safe switches.

#### Tests

* [ ] Debug/sanitizer build reports no enum-indexed out-of-bounds access during headless simulation.
* [ ] Manual selection/rendering of varied entity states does not crash.

---

## Phase 2: Gameplay decisions

### 2.1 Fix AI construction deadlock

#### Problem

AI calls gathering before construction. `aiGather()` assigns all idle peasants to resources. Later construction code asks for idle peasants and finds none, so AI can stall at the opening economy and remain stuck at the population cap.

#### Required change

* [ ] Add helper such as `aiWorker(int owner, bool allowPullFromGathering)`.
* [ ] Prefer idle peasants.
* [ ] For important construction, allow pulling workers from:

  * `S_GATHERING`
  * `S_RETURNING`
  * possibly `S_MOVING` if clearly part of gathering/returning and safe to interrupt
* [ ] Do not pull peasants that are:

  * dead
  * under construction
  * garrisoned
  * already building
  * entering a building
* [ ] Use this helper for AI houses, military buildings, towers, mills, farms, and town halls.
* [ ] Ensure issuing a build order clears or overrides the old gather order safely.
* [ ] Critical construction, especially houses when supply-blocked, may pull any valid worker.
* [ ] Non-critical construction should prefer idle workers, then pull gatherers only if needed.

#### Tests

* [ ] Run a fresh headless simulation for 5,000 ticks.
* [ ] AI does not remain stuck at 10/10 population.
* [ ] AI builds at least one supply structure or otherwise increases supply cap.
* [ ] AI builds at least one military production building if resources permit.
* [ ] AI trains at least one military unit if resources permit.
* [ ] AI does not assign the same peasant to multiple construction orders in the same tick.
* [ ] Pulling a gathering/returning worker for construction does not crash.

---

### 2.2 Make forest-like terrain non-buildable but still passable

#### Problem

Forest-like terrain appears to be buildable. That makes resource terrain unclear and can erase forests unintentionally.

#### Required change

* [ ] Identify all forest-like terrain, likely including:

  * `T_FOREST`
  * `T_PINE`
  * `T_PALM`
  * `T_DEADTREE`
  * any other tree/wood terrain
* [ ] Update `canPlace()` so buildings cannot be placed on these tiles.
* [ ] Do not change `isPassable()` for these tiles in this pass unless required by existing logic.
* [ ] Keep units able to path through forest-like terrain for now.

#### Tests

* [ ] `canPlace(E_HOUSE, forestTile, owner)` returns false.
* [ ] `canPlace(E_BARRACKS, forestTile, owner)` returns false.
* [ ] `canPlace(E_FARM, forestTile, owner)` returns false.
* [ ] Units can still path through forest-like terrain.
* [ ] Map generation still produces navigable starts.

---

### 2.3 Make berries gatherable food

#### Problem

Berry terrain exists and visually reads as a food source, but peasants do not currently gather from it.

#### Required change

* [ ] Add berries to the food-gathering economy.
* [ ] Peasants ordered to berry tiles should gather food.
* [ ] Berry tiles should have finite resource value or use an existing depletion model.
* [ ] Depleted berries should become appropriate empty terrain, probably grass.
* [ ] Terrain info/UI should describe berries as food.
* [ ] AI should be able to use nearby berries as a food source.

#### Tests

* [ ] Peasant ordered to berry tile begins gathering food.
* [ ] Peasant returns food to a valid drop-off.
* [ ] Player food increases after return.
* [ ] Berry resource amount decreases.
* [ ] Depleted berries disappear or become non-resource terrain.
* [ ] AI can choose berries as a food source.
* [ ] Ordering a peasant to non-resource terrain still fails or does nothing safely.

---

### 2.4 Guarantee reasonable starting resources

#### Problem

Starting areas may not always have accessible wood, food, and gold.

#### Required change

* [ ] In map generation, after clearing or stamping start areas, guarantee starter resources near each player start:

  * accessible woodline or tree cluster
  * accessible food source, preferably berries/sheep/deer
  * accessible gold
* [ ] Ensure guaranteed resources are in bounds.
* [ ] Ensure guaranteed resources are reachable by a peasant.
* [ ] Keep placement organic enough that the map still feels procedural.

#### Tests

For each start position:

* [ ] Wood exists within a reasonable radius.
* [ ] Food exists within a reasonable radius.
* [ ] Gold exists within a reasonable radius.
* [ ] Each guaranteed source is in bounds.
* [ ] Each guaranteed source is reachable by a peasant.
* [ ] Test multiple seeds if deterministic seeding exists or can be added.

---

### 2.5 Clarify and sanity-check the food economy

#### Problem

The food loop is less obvious than wood/gold. The game has hunting, farms, wheat/farms, fishing, winter food pressure, and now berries. The player needs a coherent rule model.

#### Required change

* [ ] Review all food sources:

  * berries
  * hunting
  * farms
  * wheat/farm terrain, if distinct
  * fishing
  * any passive food systems
* [ ] Confirm what drop-off buildings accept food.
* [ ] Confirm whether farms require mills.
* [ ] Confirm whether farms require peasants to tend them.
* [ ] Confirm how winter food pressure works.
* [ ] Confirm what happens when food reaches zero.
* [ ] Keep current peasant cost unless it is clearly inconsistent.
* [ ] If peasants still cost gold, document that as intentional for now.
* [ ] Make UI/help text match actual mechanics.

#### Tests

* [ ] Each intended food source can actually produce food.
* [ ] Each unintended/decorative food-looking tile is either removed, renamed, or made functional.
* [ ] Food drop-off rules are consistent.
* [ ] Winter food behaviour is understandable and does not create immediate unavoidable failure.
* [ ] Help text matches implemented mechanics.

---

### 2.6 Add in-game help/reference screen

#### Problem

The game has many controls and mechanics, but no clear in-game reference.

#### Required change

* [ ] Add a simple help screen or overlay, probably bound to `?`.
* [ ] Keep it concise and readable in terminal dimensions.
* [ ] Include:

  * basic controls
  * selection and movement
  * attack / attack-move
  * gathering wood/gold/food
  * berries as food
  * hunting and animal food
  * farms and mills
  * fishing if usable
  * building and training
  * rally points
  * garrison/eject
  * gates/walls
  * control groups
  * seasons
  * winter food pressure
  * weather/night visibility effects
  * pause/quit controls
* [ ] Ensure help can be closed without disrupting the game.

#### Tests

* [ ] Pressing `?` opens help.
* [ ] Help screen renders within terminal bounds.
* [ ] Closing help returns to game.
* [ ] Help text does not desync from actual implemented controls.
* [ ] Help works in normal play and while units are selected.

---

## Phase 3: Architecture cleanup

### 3.1 Remove or reduce overloaded `Entity` fields

#### Problem

Several fields currently carry unrelated meanings depending on entity type or system. This makes future bugs likely.

Known examples:

* `gatherType` is used for resource type and gate lock/manual state.
* `carrying` is used for resource cargo and gate open/closed state.
* `prodProgress` / `prodTime` are used for training and research.
* `rallyX` / `rallyY` are used for production rally points and gather-return targets.

#### Required change

* [ ] Split resource-carrying fields from gate fields.
* [ ] Split gate state into explicit fields, e.g. `gateOpen`, `gateLocked`.
* [ ] Split production progress from research progress where practical.
* [ ] Split rally point from gather/drop-off target where practical.
* [ ] Initialise all new fields safely.
* [ ] Update rendering, input, orders, AI, and simulation references.

Possible direction:

```cpp
struct Entity {
    ResourceType carriedResource;
    int carriedAmount;

    int gatherTargetX;
    int gatherTargetY;
    int dropoffX;
    int dropoffY;

    int rallyX;
    int rallyY;

    EntityType producing;
    int productionProgress;
    int productionTime;

    int researchId;
    int researchProgress;
    int researchTime;

    bool gateOpen;
    bool gateLocked;
};
```

Use names that fit the existing code. Do not over-engineer.

#### Tests

* [ ] Existing wood gathering still works.
* [ ] Existing gold gathering still works.
* [ ] New berry gathering works.
* [ ] Existing gate open/close/lock behaviour still works.
* [ ] Unit training still works.
* [ ] Research still works.
* [ ] Rally points still work.
* [ ] Debug/sanitizer build reports no uninitialised field use.

---

### 3.2 Reduce reference invalidation risk from `g.entities`

#### Problem

The code relies on `g.entities.reserve(8192)` to avoid dangling references while spawning during ticks. This is fragile.

#### Required change

Pick the smallest safe approach.

Preferred:

* [ ] Add deferred spawn queue.
* [ ] Do not `push_back()` into `g.entities` while holding references/iterators during tick processing.
* [ ] Queue spawn requests and flush them after the current tick phase.

Alternative:

* [ ] Convert `g.entities` to `std::deque<Entity>` if that is safer and less invasive.

Do not perform a large rewrite unless needed.

#### Tests

* [ ] Completed unit training still spawns units.
* [ ] Projectiles still spawn.
* [ ] Wildlife/AI spawning still works if present.
* [ ] 10,000-tick headless simulation does not crash.
* [ ] Address/undefined sanitizer reports no reference invalidation issue.

---

### 3.3 Add occupancy-grid helper if practical

#### Problem

Rendering and selection can repeatedly scan all entities with `entityAt(x, y)`. This is simple but scales poorly and can create inconsistent lookup behaviour.

#### Required change

If practical:

* [ ] Add a per-tick or per-frame occupancy helper.
* [ ] Use it for render-time entity lookup.
* [ ] Use it for selection/path blocking only if safe.
* [ ] Garrisoned/dead entities should not occupy map tiles.
* [ ] Document any deferred occupancy work if too invasive.

Possible shape:

```cpp
struct OccupancyGrid {
    int entityIdAt[MAP_H][MAP_W];
};
```

#### Tests

* [ ] Occupancy grid agrees with existing `entityAt()` for known positions.
* [ ] Rendering still shows units/buildings correctly.
* [ ] Selection still selects the correct entity.
* [ ] Garrisoned entities do not occupy map tiles.
* [ ] Dead entities do not occupy map tiles.

---

### 3.4 Split `entity.cpp` if safe

#### Problem

`entity.cpp` appears to contain orders, economy, AI, combat, projectiles, construction, and simulation. It is becoming too broad.

#### Required change

Split only where safe and low-risk.

Possible files:

* `orders.cpp`
* `economy.cpp`
* `combat.cpp`
* `ai.cpp`
* `simulation.cpp`

Rules:

* [ ] Do not change behaviour purely as part of moving code.
* [ ] Keep declarations in `realm.h` or add a small internal header if needed.
* [ ] Avoid circular dependencies.
* [ ] Update Makefile source list.

#### Tests

* [ ] Normal build succeeds.
* [ ] Debug build succeeds.
* [ ] No duplicate symbol/linker errors.
* [ ] Headless tests still pass.

---

### 3.5 Keep render/input/simulation boundaries clean

#### Problem

As features accumulate, rendering and input should not become simulation owners.

#### Required change

During cleanup:

* [ ] `render.cpp` should display state, not advance simulation.
* [ ] `input.cpp` should translate input into orders, not contain large simulation rules.
* [ ] AI logic should live outside rendering/input.
* [ ] Economy/combat/order logic should not depend on ncurses.
* [ ] Headless tests should exercise simulation without rendering.
* [ ] Minor UI-only counters/status-message timers may remain near UI code if already structured that way, but avoid adding new simulation mutation to rendering.

#### Tests

* [ ] Headless build can compile simulation without ncurses rendering.
* [ ] No new simulation rule is added only to render code.
* [ ] Input-triggered actions call order functions rather than duplicating simulation rules.

---

## Phase 4: Build, docs, warnings, and tooling

### 4.1 Add build/dependency documentation

#### Required change

* [ ] Update `README.md` or create it if missing.
* [ ] Document required dependencies.
* [ ] Document normal build.
* [ ] Document debug build.
* [ ] Document test build.
* [ ] Document what to do if `<ncurses.h>` is missing.

Suggested content:

```md
## Build

Linux:

sudo apt install build-essential libncurses-dev
make

macOS:

brew install ncurses
make

If the compiler cannot find `ncurses.h`, install the ncurses development package and check include/library paths.

## Debug build

make debug

## Tests

make test
```

#### Tests

* [ ] README instructions match actual Makefile targets.
* [ ] Fresh clone instructions are sufficient for a developer with ncurses installed.
* [ ] Missing-ncurses failure mode is documented.

---

### 4.2 Add debug/sanitizer Makefile target

#### Required change

Add a debug target similar to:

```make
debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address,undefined
debug: LDFLAGS = -lncurses -fsanitize=address,undefined
debug: clean $(TARGET)
```

Adjust to current Makefile structure.

Also consider a warnings-focused target.

#### Tests

* [ ] `make clean`
* [ ] `make`
* [ ] `make debug`
* [ ] Debug build links successfully.
* [ ] If sanitizer runtime is unavailable on a platform, document that in this file.

---

### 4.3 Track and reduce compiler warnings

#### Required change

* [ ] Record existing warnings before changes.
* [ ] Do not introduce new warnings.
* [ ] Fix warnings that touch changed code.
* [ ] Prefer getting `-Wall -Wextra` clean if practical.
* [ ] If warnings remain, document each remaining warning and why it was deferred.

#### Warning log

```text
Baseline warnings:

Remaining warnings after hardening:

Deferred warnings and reasons:
```

---

## Phase 5: Headless tests

### 5.1 Add headless simulation/test mode

#### Required change

Add a way to run simulation tests without ncurses UI.

Possible approach:

* [ ] Add compile flag such as `REALM_HEADLESS`.
* [ ] Exclude ncurses-dependent rendering/input in test builds.
* [ ] Add a test executable, e.g. `tests/realm_headless_tests.cpp`.
* [ ] Add `make test`.
* [ ] Use simple `assert()` tests unless adding a framework is clearly worth it.

#### Required test groups

##### Placement bounds

* [ ] Out-of-bounds `canPlace()` calls return false.
* [ ] Out-of-bounds `canPlace()` calls do not crash.
* [ ] Valid placement still works.

##### Entity-state names

* [ ] Every `EntityState` has a safe display name.
* [ ] Invalid/default value returns `"Unknown"` or equivalent.
* [ ] No state-name lookup indexes past an array.

##### Enemy owner classification

* [ ] Owner 1 is enemy/faction-owned.
* [ ] Owner 2 is enemy/faction-owned.
* [ ] Owner 3 is enemy/faction-owned.
* [ ] Neutral animals are not enemy/faction-owned.

##### Supply reservation

* [ ] Queued units reserve population.
* [ ] In-production units reserve population.
* [ ] Queues cannot over-reserve past the cap.
* [ ] Completed units do not exceed cap.
* [ ] AI uses the same rules.

##### AI progression

* [ ] Run a fresh game for 5,000 ticks.
* [ ] AI does not remain stuck at 10/10 population.
* [ ] AI builds at least one supply structure or otherwise increases supply.
* [ ] AI builds at least one military production building if resources permit.
* [ ] AI trains at least one military unit if resources permit.
* [ ] No builder is assigned two construction jobs in the same tick.

##### Berry gathering

* [ ] Peasant gathers from berries.
* [ ] Peasant returns food to valid drop-off.
* [ ] Food increases.
* [ ] Berry resource decreases.
* [ ] Depleted berry tile becomes non-resource terrain.

##### Forest buildability/passability

* [ ] Buildings cannot be placed on forest-like terrain.
* [ ] Units can still path through forest-like terrain.

##### Town hall cost

* [ ] Starting spawn bypasses cost.
* [ ] Ordered town hall construction requires resources.
* [ ] Ordered town hall construction deducts resources.
* [ ] AI cannot build free town halls.

##### Starting resources

* [ ] Each start has reachable wood within a reasonable radius.
* [ ] Each start has reachable food within a reasonable radius.
* [ ] Each start has reachable gold within a reasonable radius.
* [ ] Run across multiple seeds if possible.

##### Long simulation invariants

* [ ] Run 10,000 ticks.
* [ ] No live entity has out-of-bounds coordinates.
* [ ] No entity has invalid state.
* [ ] No player has negative resources unless debt is intentional.
* [ ] No player’s actual supply exceeds supply cap.
* [ ] No crash under debug/sanitizer build.

---

## Phase 6: Manual ncurses smoke test

Run the actual game after automated tests pass.

### Manual checklist

* [ ] Start a new game.
* [ ] Select peasants.
* [ ] Gather wood.
* [ ] Gather gold.
* [ ] Gather berries/food.
* [ ] Build house.
* [ ] Build barracks.
* [ ] Build farm.
* [ ] Attempt to build on forest and confirm blocked.
* [ ] Confirm units can still walk through forest.
* [ ] Attempt to build town hall without enough resources and confirm blocked.
* [ ] Build town hall with enough resources and confirm resources deducted.
* [ ] Queue units up to supply cap and confirm overqueue is blocked.
* [ ] Select units in varied states and confirm state display is safe.
* [ ] Garrison and eject units.
* [ ] Use gates/walls if available.
* [ ] Use control groups.
* [ ] Scout until enemy owners 1/2/3 are visible.
* [ ] Confirm AI owners 1/2/3 do not render as animals.
* [ ] Let the game run until AI builds economy and military.
* [ ] Open and close help screen with `?`.
* [ ] Confirm minimap still works.
* [ ] Confirm side panel still works.
* [ ] Confirm bottom command panel still works.
* [ ] Confirm terrain info still works.
* [ ] Confirm fog/night/weather rendering still works.

### Manual result

```text
Manual smoke test date:
Platform:
Terminal:
Result:
Issues found:
```

---

## Definition of done

This hardening pass is complete when:

* [ ] `make clean && make` passes.
* [ ] `make debug` passes, or platform limitation is documented.
* [ ] `make test` or equivalent headless test command passes.
* [ ] Headless AI progression test proves AI no longer stalls at 10/10.
* [ ] Headless long simulation test passes.
* [ ] Manual ncurses smoke test passes.
* [ ] `README.md` documents dependencies, build, debug build, and tests.
* [ ] This progress file is updated.
* [ ] Any deferred item is explicitly listed below.
* [ ] No accepted design decision remains ambiguous in code comments or docs.

---

## Do not do in this pass

Do not:

* [ ] Rewrite into ECS.
* [ ] Redesign all pathfinding around blocking forests.
* [ ] Hide failing tests by weakening assertions.
* [ ] Treat manual playtesting as a substitute for headless tests.

---

## Deferred follow-ups

Use this section for work intentionally left after the hardening pass.

```text
Deferred item:
Reason:
Risk:
Suggested follow-up:
```

---

## Implementation report template

When this pass is complete, write a final report using this structure.

```md
# ASCII RTS hardening implementation report

## Summary

## Files changed

## Behaviour changes players will notice

## Bugs fixed

## Design decisions implemented

## Tests added

## Commands run

- `make clean && make`
- `make debug`
- `make test`

## Results

## Manual smoke test

## Remaining warnings

## Deferred follow-ups

## Notes for future work
```
