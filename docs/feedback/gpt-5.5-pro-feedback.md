# Feedback report: ASCII RTS “Realm”

## Scope

I reviewed the uploaded C++/ncurses project files: `main.cpp`, `realm.h`, `globals.cpp`, `entity.cpp`, `input.cpp`, `render.cpp`, `mapgen.cpp`, `Makefile`, `.gitignore`, and `realm.command`.

I also attempted to build the project. The real build could not complete in this container because `<ncurses.h>` is not installed here. To check the C++ itself, I used a minimal local ncurses stub and compiled all source files; that passed with warnings. I also ran a stubbed 5,000-tick headless simulation of the game logic.

---

## Executive assessment

This is a strong prototype. It is not just an ASCII toy renderer; it already has the core shape of a small RTS: procedural map generation, fog of war, resource collection, construction, unit training, combat, ranged projectiles, garrisoning, gates, control groups, minimap, seasons, weather, wildlife, day/night visibility, and AI opponents.

The best part is that several atmospheric systems also affect mechanics. Winter changes terrain and food pressure; storms and night affect visibility; roads and mud affect movement; wildlife reacts to units. That gives the game a distinctive identity beyond “Age of Empires in ncurses.”

The biggest current problems are AI progression, a few unsafe/out-of-bounds cases, and some field overloading that will make future features harder to maintain. The project is at the point where a short hardening pass would pay off more than adding new features.

---

## Major strengths

### 1. The game has a coherent RTS loop

The current loop is recognizable and playable in structure:

* Start with a town hall and peasants.
* Gather gold and wood.
* Build military/economic structures.
* Train units.
* Scout through fog.
* Fight enemies and wildlife.
* Survive seasonal and weather pressure.

The control set also covers many expected RTS actions: selection, group movement, attack, attack-move, control groups, rally points, garrisoning, ejecting, wall dragging, gates, and research.

### 2. Strong environmental identity

The seasonal and weather systems are a highlight. They are not purely cosmetic:

* Night/storm conditions affect detection.
* Winter freezes water and creates food pressure.
* Rain/storm creates mud and slows movement.
* Roads emerge from repeated path wear.
* Spring thaw restores terrain gradually.

This gives the project a clear identity: a survival/settlement RTS in a living ASCII world.

### 3. Good amount of UI for a terminal game

The side panel, minimap, top resource bar, bottom command hints, selection details, training progress, queue display, and terrain info bar are all valuable. The UI is dense, but it is already doing the right jobs.

### 4. Sensible separation by file

The project is still globally stateful, but the broad file split is reasonable:

* `mapgen.cpp`: world generation.
* `entity.cpp`: simulation, orders, combat, AI.
* `input.cpp`: controls.
* `render.cpp`: display.
* `globals.cpp` / `realm.h`: shared data and declarations.

This is enough structure for a compact game, though some internals now need refinement.

---

## Highest-priority issues

### 1. AI gets stuck at the economic opening

In a 5,000-tick headless simulation, each AI reached 10/10 population but built no houses, barracks, stables, or military units. The count after 5,000 ticks was effectively:

```text
AI peasants: 10
AI houses:   0
AI barracks: 0
AI military: 0
```

The likely cause is in `tickAIForOwner()` and `aiGather()`.

`tickAIForOwner()` calls `aiGather(o)` before trying to build houses, barracks, towers, mills, and other structures. `aiGather()` assigns every idle peasant to gathering. Later, the builder logic asks for `aiIdle(o, E_PEASANT)`, but there are no idle peasants left.

Relevant areas:

* `entity.cpp`, around `aiGather()`, lines 1469–1483.
* `entity.cpp`, `tickAIForOwner()`, around lines 1551–1574.

Recommended fix:

* Add an `aiWorker()` function that can select a peasant from idle, gathering, or returning states.
* Alternatively, reserve one or two peasants as builders before calling `aiGather()`.
* Do not let `aiGather()` consume every idle peasant unconditionally.

Example direction:

```cpp
Entity* aiWorker(int owner) {
    // Prefer idle.
    for (auto& e : g.entities)
        if (e.alive && e.owner == owner && e.type == E_PEASANT &&
            e.state == S_IDLE && !e.underConstruction)
            return &e;

    // Then allow pulling a gatherer if the AI needs construction.
    for (auto& e : g.entities)
        if (e.alive && e.owner == owner && e.type == E_PEASANT &&
            (e.state == S_GATHERING || e.state == S_RETURNING))
            return &e;

    return nullptr;
}
```

Then use `aiWorker()` for supply/military/economy construction instead of `aiIdle()`.

---

### 2. `canPlace()` can read outside the map for farms

In `entity.cpp`, `canPlace()` reads `g.map[y][x]` before checking bounds when the building type is `E_FARM`:

```cpp
if (type == E_FARM) {
    if (getSeason() == WINTER) return false;
    Terrain t = g.map[y][x].terrain;
    ...
}
```

This is unsafe if `x` or `y` are outside the map. AI building placement can generate negative or out-of-range candidate positions near corners, so this is a real risk.

Relevant area: `entity.cpp`, lines 128–135.

Recommended fix:

```cpp
bool canPlace(EntityType type, int x, int y, int owner) {
    (void)owner;
    if (!inBounds(x, y)) return false;

    if (type == E_FARM) {
        if (getSeason() == WINTER) return false;
        Terrain t = g.map[y][x].terrain;
        ...
    }

    ...
}
```

The later footprint loop still handles multi-tile buildings extending beyond the map.

---

### 3. Rendering can index past the state-name array

In `render.cpp`, non-peasant unit state display uses:

```cpp
const char* sn[] = {"Idle","Moving","Attacking","Gathering","Building","Training","Returning","Dead"};
stDesc = sn[sel->state];
```

But `EntityState` has more than those eight states:

```cpp
S_IDLE, S_MOVING, S_ATTACKING, S_GATHERING,
S_BUILDING, S_TRAINING, S_RETURNING, S_DEAD,
S_ENTERING, S_GARRISONED
```

If a selected archer/militia/knight is entering a building, `sel->state == S_ENTERING`, so this can read out of bounds.

Relevant area: `render.cpp`, lines 641–644.

Recommended fix: replace the array lookup with a `switch` or include every state and bounds-check it.

```cpp
static const char* stateName(EntityState s) {
    switch (s) {
    case S_IDLE:       return "Idle";
    case S_MOVING:     return "Moving";
    case S_ATTACKING:  return "Attacking";
    case S_GATHERING:  return "Gathering";
    case S_BUILDING:   return "Building";
    case S_TRAINING:   return "Training";
    case S_RETURNING:  return "Returning";
    case S_DEAD:       return "Dead";
    case S_ENTERING:   return "Entering";
    case S_GARRISONED: return "Garrisoned";
    default:           return "Unknown";
    }
}
```

---

### 4. Enemy owners 2 and 3 render like animals

In `render.cpp`, entity color selection only treats `owner == 1` as an enemy:

```cpp
else if (ent->owner == 1) cp = night ? CP_ENEMY_NIGHT : CP_ENEMY;
else if (ent->type == E_WOLF) cp = CP_WOLF;
else if (ent->type == E_SHEEP) cp = CP_SHEEP;
else cp = CP_DEER;
```

Since the game has up to three AI opponents, owners 2 and 3 fall through to animal coloring. This will make two AI factions visually misleading on the main map.

Relevant area: `render.cpp`, lines 408–413.

Recommended fix:

```cpp
else if (ent->owner > 0 && ent->owner < MAX_PLAYERS)
    cp = night ? CP_ENEMY_NIGHT : CP_ENEMY;
else if (ent->type == E_WOLF)
    cp = CP_WOLF;
else if (ent->type == E_SHEEP)
    cp = CP_SHEEP;
else
    cp = CP_DEER;
```

The minimap already uses the broader `owner < MAX_PLAYERS` logic, so this would make the main map consistent.

---

### 5. Supply is not reserved for queued or in-production units

`orderTrain()` checks only current supply:

```cpp
if (p.supply + STATS[ut].supplyUsed > p.supplyMax) ...
```

This means a player can queue multiple units while current supply is still below the cap. When they complete, actual supply can exceed the cap. The UI already calculates `Pop:+forecast`, so the code recognizes pending population conceptually, but training validation does not use it.

Relevant area: `entity.cpp`, lines 466–484.

Recommended fix: add a helper that includes existing units, current production, and queues:

```cpp
int reservedSupply(int owner) {
    int used = 0;
    for (auto& e : g.entities) {
        if (!e.alive || e.owner != owner) continue;

        used += STATS[e.type].supplyUsed;

        if (isBuilding(e.type)) {
            if (e.producing != E_NONE)
                used += STATS[e.producing].supplyUsed;

            for (int q : e.queue)
                used += STATS[(EntityType)q].supplyUsed;
        }
    }
    return used;
}
```

Then use:

```cpp
if (reservedSupply(bld.owner) + STATS[ut].supplyUsed > p.supplyMax) ...
```

---

## Gameplay/design feedback

### Economy clarity needs work

Right now, the food economy is less intuitive than gold/wood. Peasants can chop and mine, but food comes from hunting, farms, wheat/farms, and fishing. Berry bushes exist as terrain, but they do not appear to be gatherable.

This may confuse players because `T_BERRY` looks like a food source but `orderGather()` ignores it.

Recommended changes:

* Either make berry bushes gatherable food deposits, or make them purely decorative with a different name.
* Add a `?` help screen explaining:

  * Peasants gather wood/gold.
  * Animals provide food when killed.
  * Farms require a mill and a tending peasant.
  * Boats gather fish.
* Consider making peasants cost food instead of gold, unless the gold-based economy is intentional.

### Starting-resource fairness should be made explicit

Starting areas are cleared, and gold is placed near corners, but nearby wood is not obviously guaranteed. Since forests can be procedurally sparse or cleared away near spawn, some starts may have awkward early wood access.

Recommended changes:

* Guarantee a small woodline near each starting town hall.
* Guarantee a hunt/food source near each start.
* Keep the random map, but stamp a symmetric “starting kit” around each corner.

### Forest passability/buildability should be decided intentionally

Currently, `isPassable()` does not block forest, pine, palm, or dead tree terrain. `canPlace()` also only blocks gold, water, mountains, stone, fish, etc. This means players may be able to walk through and build on wood-resource tiles.

That may be intentional for a lightweight terminal RTS, but if you want forest to function like RTS terrain, it should block movement and construction until chopped. At minimum, buildings should probably not be placeable on resource-bearing forest tiles unless you explicitly clear the tile.

### AI expansion uses free town halls

`E_TOWNHALL` has zero build cost in `STATS`, but AI expansion logic can call `orderBuild(*b, E_TOWNHALL, ...)`. If the AI eventually starts building correctly, this gives it free expansion town halls.

Recommended options:

* Give `E_TOWNHALL` a real cost.
* Prevent AI from building town halls and use castles instead.
* Add a separate `E_OUTPOST` or `E_KEEP` if you want cheaper expansion.

---

## Code architecture feedback

### Avoid overloading entity fields for unrelated meanings

Several fields currently do double or triple duty:

* `gatherType` is resource type, but also gate manual-lock state.
* `carrying` is resource cargo, but also gate open/closed state.
* `prodProgress` / `prodTime` are used for both training and research.
* `rallyX` / `rallyY` are used for production rally points and gather-return targets.

This works in a small prototype, but it will become a source of hidden bugs.

Recommended direction:

```cpp
struct Entity {
    ...
    int resourceKind;
    int carriedAmount;

    bool gateLocked;
    bool gateOpen;

    EntityType producing;
    int productionProgress;
    int productionTime;

    int researchBit;
    int researchProgress;
    int researchTime;

    int rallyX, rallyY;
    int gatherX, gatherY;
};
```

This will make debugging much easier.

### Global state is acceptable for now, but simulation/render/input should become cleaner

`Game g` is simple and practical, but many functions mutate it freely. As the game grows, this makes bugs harder to isolate.

A good next step would be to keep `Game g`, but separate responsibilities more clearly:

* `simulation.cpp`: tick logic.
* `orders.cpp`: player/AI orders.
* `ai.cpp`: AI.
* `combat.cpp`: attacking, projectiles, death.
* `economy.cpp`: gathering, building, training.
* `render.cpp`: no simulation mutation except maybe UI status decrement.
* `input.cpp`: translates input into orders only.

### Replace render-time `entityAt()` scanning with an occupancy grid

`renderMap()` calls `entityAt(mx, my)` for each visible map tile. `entityAt()` scans the full entity vector. This is fine for a small number of entities, but it becomes expensive as the map fills.

Recommended improvement:

* Build a per-frame or per-tick occupancy array:

  ```cpp
  int occ[MAP_H][MAP_W]; // entity id or -1
  ```
* Fill it once from live entities.
* Use `occ[y][x]` in render, selection, and movement checks.

This would also make click selection and minimap rendering more predictable.

### Do not rely on `vector::reserve()` for reference safety long-term

`main.cpp` reserves `g.entities.reserve(8192)` and comments that this avoids dangling references while spawning inside `tickEntity()`. That is a reasonable emergency fix, but it leaves a hidden limit.

Better options:

* Queue spawns until after the current tick.
* Store entities in `std::deque<Entity>` for more stable references.
* Store IDs and resolve them after mutations.
* Use indices carefully and avoid holding references across `push_back()`.

---

## Build and tooling feedback

### Add dependency notes

The Makefile is simple, but the project needs an install note for ncurses.

Suggested `README.md` section:

```md
## Build

Linux:
sudo apt install build-essential libncurses-dev
make

macOS:
brew install ncurses
make
```

On macOS/Homebrew, you may need include/library paths depending on environment.

### Add debug and sanitizer targets

Add a debug target:

```make
debug: CXXFLAGS = -std=c++17 -g -O0 -Wall -Wextra -fsanitize=address,undefined
debug: LDFLAGS = -lncurses -fsanitize=address,undefined
debug: clean $(TARGET)
```

This would catch issues like the farm `canPlace()` out-of-bounds and render state-name indexing.

### Add a headless simulation test

A small headless harness would be very useful. The game logic can already tick without real curses if rendering is skipped. Add a compile flag such as `REALM_HEADLESS` and a test that runs 5,000 ticks, checking invariants:

* No entity has invalid coordinates.
* Supply never exceeds supply cap unless intentionally allowed.
* AI builds at least one house/barracks within N ticks.
* No dead entity remains selected.
* No entity state is outside enum range.

---

## Suggested priority roadmap

### Immediate fixes

1. Add bounds check at the top of `canPlace()`.
2. Fix non-peasant state rendering with a safe `stateName()` function.
3. Fix enemy coloring for owners 2 and 3.
4. Fix AI worker selection so AIs actually build houses and military.
5. Make queued units reserve supply.

### Next gameplay pass

1. Guarantee starting wood/food/gold.
2. Decide whether forests should block movement/building.
3. Make berry bushes gatherable or remove them as apparent food.
4. Add an in-game help screen.
5. Tune food economy so the player understands how to avoid winter starvation.

### Next architecture pass

1. Split overloaded entity fields.
2. Add an occupancy grid.
3. Move AI into its own file.
4. Add debug/sanitizer Makefile target.
5. Add headless simulation tests.

---

## Overall verdict

This is a substantial prototype with a clear identity. The strongest aspect is the combination of RTS mechanics with a dynamic ASCII world: seasons, weather, fog, wildlife, roads, mud, freezing, and thawing all give the game personality.

The main development risk is not lack of features; it is systems complexity growing around a single overloaded `Entity` struct and global state. Fixing AI progression, unsafe indexing, supply reservation, and field overloading would make the project much more stable and easier to extend.
