# Realm Tile / Asset JSON v2 Plan

**Document status:** implementation-ready migration plan aligned to the current C++ runtime
**Audience:** coding agent / implementation agent
**Primary goal:** migrate the asset JSON and renderer toward a layered tile model without losing compatibility with the current monolithic `Tile`, legacy `Terrain`, existing saves, or current runtime asset lookups.

---

## 1. Executive summary

The design direction is still correct:

```text
tile = one ground + zero or more decals + optional feature + optional building coverage + actors + overlays
```

But the current code is **not there yet**. Today:

- `Tile` is still monolithic and stores `Terrain terrain`, `resources`, visibility/exploration arrays, `Biome biome`, `Terrain preWinterTerrain`, and `int wear`.
- `Game` still stores `Tile map[MAP_H][MAP_W]`.
- `Projectile` is still a simple glyph-and-colour runtime object (`x`, `y`, `tx`, `ty`, `glyph`, `color`, `life`, `alive`).
- `RenderModel` still exposes `tiles`, `entities`, and `actionMarkers`, and `TileRenderInfo` still carries a single `Terrain terrain`.

So the plan must treat the migration as a **two-track change**:

1. **JSON / asset-schema migration**
2. **C++ runtime migration**

The key bridge already exists in code:

```text
VisualTileParts
visualPartsForTile()
visualPartsForTerrain()
```

This bridge should be the first source of truth for converting legacy `Terrain` into v2 `ground` / `feature` / `decal` data.

---

## 2. Current code reality that the plan must respect

### 2.1 Tile is still monolithic

Current runtime `Tile`:

```cpp
struct Tile {
    Terrain terrain;
    int resources;
    bool visible[MAX_PLAYERS], explored[MAX_PLAYERS];
    Biome biome;
    Terrain preWinterTerrain;
    int wear;
};
```

Current runtime migration rule:

```text
Keep Terrain authoritative in the short term.
Make VisualTileParts the official visual adapter.
Do not require the renderer, saves, or mapgen to consume a split Tile layout until the runtime migration phase is complete.
```

Target long-term runtime structure:

```cpp
struct Tile {
    GroundType ground;
    std::vector<VisualDecalType> decals;
    FeatureType feature;
    FeatureState featureState;
    int featureResources;
    Biome biome;
    Terrain legacyTerrain;        // temporary compatibility during migration
    Terrain preWinterTerrain;     // or migrated equivalent
    int wear;
    bool visible[MAX_PLAYERS], explored[MAX_PLAYERS];
};
```

That target is valid, but it is a later C++ phase, not the starting point.

### 2.2 VisualTileParts is the current bridge and must be central

Current code already exposes:

- `GroundType`
- `FeatureType`
- `FeatureState`
- `VisualDecalType`
- `VisualTileParts { ground, feature, featureState, featureResources, featureTraits, decals }`

Current bridge rules:

```text
Use visualPartsForTile(tile) as the default adapter from legacy Tile/Terrain to v2 visuals.
Use visualPartsForTerrain(terrain, biome, resources, wear, gateOpen, gateLocked) when explicit gate state is needed.
Compare terrainDef(terrain) + VisualTileParts against generated v2 JSON during migration.
```

Important current detail:

```text
visualPartsForTerrain(...) has default gateOpen=false and gateLocked=false parameters.
visualPartsForTile(tile) currently calls it without gate state, so castle-gate open/locked visuals are only available when a caller passes those flags explicitly.
```

That is a verified current behaviour, not a contradiction.

### 2.3 Projectiles are not asset-backed yet

Current runtime `Projectile`:

```cpp
struct Projectile {
    float x, y, tx, ty;
    char glyph;
    int color, life;
    bool alive;
};
```

Current gameplay code only knows glyph/colour/lifetime. It does **not** yet know a projectile slug such as `arrow` or `tower_bolt`.

### 2.4 RenderModel does not yet represent the v2 render stack

Current `RenderModel` contains:

```text
tiles
entities
actionMarkers
```

Current `TileRenderInfo` still carries:

```text
x, y, terrain, visible, explored
```

It does **not** yet carry:

```text
ground
decals
feature layers
projectiles
effects
UI/world overlay assets
depth buckets / sort keys
projection mode
```

### 2.5 Save format is still legacy-terrain based

Current save constants:

```text
REALM_SAVE_VERSION = 11
REALM_MIN_SUPPORTED_SAVE_VERSION = 8
```

Current save/read paths still serialize:

- legacy `Tile.terrain`
- `Tile.preWinterTerrain`
- `Tile.wear`
- glyph/colour projectiles

Map migration and save migration are therefore separate tasks.

---

## 3. Code-backed visual inventory to preserve

The v2 inventory must be generated from current code, not from older hand-written assumptions.

### 3.1 Ground inventory

Current `GroundType` values:

```text
grass
meadow
dirt
road
mud
sand
dunes
snow
tundra
ice
water
shallows
marsh
gravel
ash
lava
hills
rocky
castle_floor
```

Important correction:

```text
rocky and tundra already exist in code and must be present in v2 generation/runtime asset coverage.
```

Recommended rule:

```text
Generate/runtime-support rocky and tundra as first-class grounds.
Only alias them temporarily if the runtime path lookup is also updated to make the alias explicit.
```

### 3.2 Feature inventory

Current `FeatureType` values:

```text
forest
pine
palm
dead_tree
berry_bush
wheat_crop
fish_shoal
gold_deposit
stone_boulders
mountain_peak
reeds
ruins
castle_wall
castle_gate
```

Current `FeatureState` values:

```text
default
full
mostly_full
mostly_empty
depleted
open
closed
locked
damaged
broken
```

### 3.3 Decal inventory

Current `VisualDecalType` values:

```text
flowers
tall_grass
scuffs
packed_path
cobble_patch
wheel_ruts
yard_clutter
crates_barrels
log_piles
farm_tracks
muddy_footprints
snow_trampled_path
```

Important correction:

```text
Current runtime decals are not just flowers and tall_grass.
Wear already produces scuffs, packed_path, cobble_patch, wheel_ruts,
muddy_footprints, and snow_trampled_path.
```

Migration requirement:

```text
The minimum v2 decal inventory must include every current VisualDecalType value.
Do not ship a v2 plan that omits wear-generated decals.
```

### 3.4 Existing typed visual states that should go straight into v2

The plan should use the existing runtime state enums directly:

- `FeatureState`
- `BuildingVisualState`
- `AnimalCarcassVisualState`
- `TransportVisualState`

These are better than inventing generic state names and trying to map later.

### 3.5 Effects and UI seed inventory

The first `effects` and `user_interface` inventory should be seeded from `dumpMissingTilesetAssets()`, which currently checks for:

```text
Projectile-style:
  arrow_projectile
  tower_bolt_projectile
  warship_shot_projectile
  catapult_boulder_projectile

Hit / impact / world effects:
  melee_hit_spark
  arrow_hit
  boulder_impact
  boulder_water_splash
  building_hit_dust

Weather frames:
  rain_frame_1
  rain_frame_2
  storm_rain_frame_1
  storm_rain_frame_2
  snowfall_frame_1
  snowfall_frame_2

World/UI markers:
  move_marker
  attack_marker
  gather_marker
  build_marker
  rally_marker
  attack_move_marker
  hold_position_marker
  selection_ring
  group_selection_ring
  range_ring_dot
  build_preview_valid
  build_preview_invalid
  wall_preview
  garrison_indicator
  queued_unit_marker
  research_active_marker
  completed_research_icon_treatment
```

Do not invent the first effects/UI inventory by example alone when the runtime already has a missing-assets checklist.

---

## 4. Current runtime asset paths

The plan must either keep the current runtime path layout initially or explicitly schedule the code changes needed to replace it.

### 4.1 Current paths expected by code

Current runtime lookups:

| Asset kind | Current runtime path |
|---|---|
| ground | `assets/tiles/grounds/<ground>.png` |
| feature | `assets/tiles/features/<feature>/manifest.json` |
| decal | `assets/tiles/decals/<decal>.png` |
| entity frames | `assets/tiles/entities/<slug>/<action>/<direction>/frame_00_base.png` |
| entity team mask | `assets/tiles/entities/<slug>/<action>/<direction>/frame_00_teammask.png` |
| current effect/UI audit path | `assets/tiles/effects-ui/<name>.png` |

Important correction:

```text
The current runtime does not look under:
assets/tiles/terrain/grounds
assets/tiles/terrain/decals
assets/tiles/terrain/features
assets/tiles/entities/units
assets/tiles/entities/animals
assets/tiles/entities/buildings
```

### 4.2 Recommended path strategy

**Recommended first step:** keep runtime paths unchanged while migrating JSON structure.

That means:

```text
JSON groups may become grounds / decals / features / units / animals / buildings / projectiles / effects / user_interface.
But runtime path fields should still point at the current on-disk layout until the loader/renderer code is updated.
```

If a full runtime path reorganisation is chosen later, update at least:

- `src/render/sdl/display_glyphs.cpp`
- `src/sim/save_load.cpp` (`dumpMissingTilesetAssets`)
- any new terrain/decal/feature/projectile/effect asset loaders
- relevant docs and exporters

Important current limitation:

```text
src/render/sdl/tileset_assets.cpp only loads entity animation frames.
It is not yet a general-purpose ground/feature/decal/projectile/effect loader.
```

---

## 5. Gameplay and metadata extraction rules

### 5.1 Placement rules must come from code first

Do not hand-author placement metadata from intuition when code already defines it.

Initial extraction sources:

- `canPlace()`
- `terrainDef()`
- entity size/footprint from `STATS`

Current verified placement rules:

```text
Farm:
  - cannot be placed in winter
  - allowed on grass, meadow, tall_grass, flowers, dirt, wheat, snow

Dock:
  - still uses the normal building footprint rules first
  - footprint tiles must be land-passable and buildable
  - additionally requires at least one adjacent water-passable tile
```

Important correction:

```text
Do not describe dock as currently placeable on water/shallows tiles unless the runtime rules are changed.
Current code requires the dock footprint itself to pass normal land-building checks.
```

### 5.2 Movement metadata must match current integer cooldown semantics

Current movement behaviour is based on integer speed / cooldown changes, not fractional movement costs.

Current examples:

```text
road / dirt / castle_floor: faster by reducing move cooldown by 1
marsh / shallows / sand / snow / ice / ash: slower by adding 1
mud: slower by adding 2
forest-like concealment features: extra penalty via feature traits, plus knight forest penalty
rain / storm: extra slowdown on certain natural grounds
winter: movement clamp to at least base speed + 1
```

Plan rule:

```text
Use descriptive or integer movement modifiers first.
Do not treat fractional values such as -0.25 movement cost as authoritative unless gameplay is intentionally redesigned.
```

### 5.3 Road migration affects gameplay systems, not just file formats

Current `T_ROAD` is still gameplay terrain:

- movement speed uses it directly
- path wear mutates terrain into `T_DIRT` then `T_ROAD`
- winter stores/restores it through `preWinterTerrain`
- mapgen creates it directly
- ASCII/SDL fallbacks render it directly as terrain

So road migration must explicitly cover:

- `src/sim/movement_system.cpp`
- `src/sim/season_system.cpp`
- `src/map/mapgen_passes.cpp`
- `src/core/terrain_defs.cpp`
- `src/render/ascii/display_glyphs.cpp`
- `src/render/sdl/display_glyphs.cpp`
- save/load and map import/export paths
- placement/buildability rules if road stops being a terrain type

Do **not** remove `T_ROAD` until all of those replacements exist.

---

## 6. Target end state

### 6.1 Target conceptual world model

```text
Tile
  ground              required, exactly one
  decals              optional list
  feature             optional
  building reference  optional
  actors              not stored as permanent tile layers
  overlays            derived render/UI/weather state

Projectile
  runtime projectile type / slug
  world position / target
  renderable asset lookup
  ASCII fallback glyph + colour
  impact effect mapping
```

### 6.2 Target RenderModel v2

Target renderer migration:

```text
TileRenderInfo:
  replace legacy Terrain-only field with VisualTileParts or explicit ground/decals/feature fields

Add:
  ProjectileRenderInfo
  EffectRenderInfo
  UiOverlayRenderInfo or an explicit decision to keep UI procedural
  depth bucket / sort-key generation
  projection metadata needed by the renderer
```

### 6.3 Target feature split support

The end-state render stack can still be:

```text
surface pass:
  ground
  flat decals

world pass:
  feature.back
  building.back
  actors
  projectiles
  effects
  feature.front
  building.front

ui pass:
  world-space UI overlays
  screen-space UI
```

But that is not the current behaviour. Right now the code only has a symbolic concealment occluder for some features. True front/back feature art needs new renderer and asset-path support.

---

## 7. Migration phases

## Phase 0 - Inventory and validator baseline

Create a code-backed migration inventory before changing schemas.

Primary inputs:

- `terrainDef()`
- `visualPartsForTile()`
- `visualPartsForTerrain()`
- state enums in `game_types.h`
- `dumpMissingTilesetAssets()`
- `canPlace()`
- movement / weather / season systems

Deliverables:

- v2 inventory generated from code, not assumed counts from older JSON exports
- validator that can compare generated JSON against runtime inventories

Validator minimum checks:

```text
all GroundType values represented
all FeatureType values represented
all VisualDecalType values represented
all typed visual states represented
paths reflect the chosen runtime strategy
legacy Terrain-to-VisualTileParts mappings stay covered
```

## Phase 1 - JSON v2 structure without breaking runtime

Goals:

```text
add groups: grounds, decals, features, units, animals, buildings, projectiles, effects, user_interface
rename ammunition -> projectiles in JSON/exporters
keep runtime compatibility fields where needed
```

Key rule:

```text
JSON v2 can move ahead of the runtime, but it must declare current runtime paths and legacy identifiers until the C++ migration catches up.
```

## Phase 2 - Runtime C++ bridge

This is the missing phase that makes the rest of the plan realistic.

Goals:

1. Treat `visualPartsForTile()` as the official compatibility adapter from legacy `Terrain` to v2 `ground` / `feature` / `decal`.
2. Keep `Terrain` authoritative temporarily.
3. Make exporters and validators compare `terrainDef()` + `VisualTileParts` against JSON v2.
4. Add tests for every current bridge mapping.

Files already central to this phase:

- `src/core/terrain_defs.cpp`
- `src/core/game_types.h`
- `src/core/game_state_types.h`
- `include/realm.h`
- `tests/realm_headless_tests.cpp`

Acceptance notes:

```text
do not change Tile layout yet
do not change save format yet
do not remove legacy Terrain enums yet
```

## Phase 3 - Road migration

Goals:

```text
road becomes a decal in v2 JSON
T_ROAD stays as a legacy runtime/input compatibility value until runtime systems are migrated
```

Required work:

1. Create v2 road decal metadata.
2. Keep `T_ROAD` as legacy input.
3. Define how underlying ground is inferred when migrating old road terrain.
4. Update gameplay/runtime systems before removing road-as-terrain assumptions.

Files to review/update during implementation:

- `src/sim/movement_system.cpp`
- `src/sim/season_system.cpp`
- `src/map/mapgen_passes.cpp`
- `src/core/terrain_defs.cpp`
- `src/render/ascii/display_glyphs.cpp`
- `src/render/sdl/display_glyphs.cpp`
- save/map import/export code

Compatibility rule:

```text
Do not remove T_ROAD until movement, wear, seasonal restore/decay, mapgen, save/load, and renderer fallbacks all have replacement logic.
```

## Phase 4 - Save format migration

This is separate from map migration.

Trigger:

```text
Any change to Tile layout, authoritative ground/decal/feature storage, or projectile identity requires a save-version bump.
```

Required files:

- `src/sim/save_schema.h`
- `src/sim/save_writer.cpp`
- `src/sim/save_reader.cpp`
- `src/sim/save_migration.cpp`

Migration coverage must explicitly include legacy meanings for:

```text
T_ROAD
T_FLOWERS
T_TALL_GRASS
T_WHEAT
T_FISH
T_RUINS
T_CASTLE_GATE
wear
preWinterTerrain
legacy glyph/colour projectiles
```

Versioning rule:

```text
If Tile or Projectile persistence changes, bump REALM_SAVE_VERSION and add a migration path from older saves instead of silently repurposing old fields.
```

## Phase 5 - RenderModel v2 and renderer migration

Goals:

```text
teach RenderModel about the actual layered render stack
stop expecting the renderer to infer everything from Terrain alone
```

Required work:

1. Change `TileRenderInfo` to carry `VisualTileParts` or equivalent split fields.
2. Add `ProjectileRenderInfo`.
3. Add `EffectRenderInfo`.
4. Decide whether UI stays procedural for now or gets explicit `UiOverlayRenderInfo`.
5. Add depth-bucket / sort-key generation.

Key files:

- `src/render/render_model.h`
- `src/render/render_model.cpp`
- `src/render/sdl/map_renderer.cpp`
- `src/render/ascii/map_renderer.cpp`

Important current limitation:

```text
Current RenderModel has no place for projectiles or effect/UI assets.
Those fields must exist before render.layer / depth_bucket metadata can be consumed meaningfully.
```

## Phase 6 - Projectile runtime bridge

Goals:

```text
rename JSON ammunition -> projectiles
add a runtime way to know which projectile a live projectile actually is
keep ASCII glyph/colour fallback working
```

Target additions:

```cpp
enum ProjectileType { ... };

struct Projectile {
    ProjectileType type;   // or stable slug/id
    float x, y, tx, ty;
    char glyph;
    int color, life;
    bool alive;
};
```

Runtime work:

1. Add projectile type or slug.
2. Add projectile asset lookup.
3. Preserve glyph/colour fallback for ASCII mode.
4. Add save/load compatibility.
5. Add RenderModel support.
6. Map projectile types to impact effects.

Key files:

- `src/sim/projectile_system.cpp`
- `src/core/game_state_types.h`
- `src/sim/save_reader.cpp`
- `src/sim/save_writer.cpp`
- `src/render/render_model.h`
- renderers that currently draw projectiles procedurally

## Phase 7 - Feature split and occlusion

Current state:

```text
forest / pine / reeds concealment is currently represented by a symbolic occluder,
not true back/front sprite splitting.
```

Required end-state work:

1. Add feature-layer metadata (`back`, `main`, `front`) where needed.
2. Add art-path support for split feature assets.
3. Update render ordering for same-tile actor/feature composition.

Key files:

- `src/render/sdl/display_glyphs.cpp`
- `src/render/sdl/map_renderer.cpp`
- feature manifests / exporter output

Rule:

```text
Do not describe feature split as already implemented.
It is a planned renderer/art-path upgrade.
```

## Phase 8 - Placement and gameplay extraction

Goals:

```text
generate placement.allowed_grounds, requires_adjacent, passability, and movement semantics from code first
avoid hand-authored drift
```

Primary extraction sources:

- `canPlace()`
- `terrainDef()`
- movement system
- weather system
- season system

Initial examples to encode from current code:

```text
farm winter restriction
farm allowed terrain list
dock adjacency-to-water rule
ground buildable/passable data from terrainDef()
integer speed adjustments from movement_system.cpp
mud creation/drying from weather_system.cpp
wear/road decay and winter restore from season_system.cpp
```

## Phase 9 - Effects and user-interface seeding

Goals:

```text
create the first effects and user_interface inventories from dumpMissingTilesetAssets()
```

Short-term path rule:

```text
If runtime still expects assets/tiles/effects-ui/<name>.png, keep that as the runtime path until a dedicated loader/path migration exists.
```

This avoids inventing a `projectiles/`, `effects/`, and `ui/` runtime path layout that the code does not yet load.

## Phase 10 - Optional runtime path migration

Only do this after the bridge, renderer, and loader work exists.

If choosing a new runtime tree, update:

- terrain/decal/feature loaders
- projectile/effect/UI loaders
- missing-assets reporting
- exporter/runtime path fields
- docs

Do not imply that the runtime already uses the new tree before those code changes land.

## Phase 11 - Remove compatibility

Only after:

```text
RenderModel v2 exists
projectiles have runtime identity
save migration is in place
road no longer requires legacy Terrain gameplay semantics
runtime asset paths are final
exporters and validators are writing the same model the runtime consumes
```

Then:

- remove v1-only JSON compatibility
- remove `ammunition` aliases
- deprecate legacy `T_ROAD` output
- optionally migrate `Tile` storage away from monolithic `Terrain`

---

## 8. Path and schema decisions to lock in now

### 8.1 Official short-term source of truth

Lock this in:

```text
Short-term authoritative gameplay data:
  Terrain + terrainDef()

Short-term authoritative visual adapter:
  VisualTileParts via visualPartsForTile() / visualPartsForTerrain()

Long-term target:
  split Tile storage + RenderModel v2 + asset-backed projectile/effect/UI rendering
```

### 8.2 Official road rule

Lock this in:

```text
road belongs to decals in v2 JSON
T_ROAD remains a compatibility terrain until runtime systems migrate
```

### 8.3 Official placement rule

Lock this in:

```text
placement metadata is initially extracted from code, not guessed
```

### 8.4 Official path rule

Lock this in:

```text
Either keep current runtime paths for now,
or list every code file that must change before claiming a new runtime path tree.
```

---

## 9. Acceptance criteria

Minimum acceptance criteria for the plan and its implementation:

```text
[ ] The plan explicitly states that the current runtime Tile is still monolithic.
[ ] The plan treats VisualTileParts as the central compatibility bridge.
[ ] The v2 inventory includes every current GroundType, including tundra and rocky.
[ ] The v2 inventory includes every current VisualDecalType, including wear-generated decals.
[ ] The plan uses FeatureState, BuildingVisualState, AnimalCarcassVisualState, and TransportVisualState directly.
[ ] The plan separates map migration from save migration.
[ ] The plan requires a REALM_SAVE_VERSION bump before changing persisted Tile/Projectile layout.
[ ] The plan adds a projectile runtime bridge instead of assuming JSON projectiles are automatically usable.
[ ] The plan adds a RenderModel v2 migration before expecting render.layer/depth_bucket to drive runtime rendering.
[ ] The plan describes current feature occlusion as symbolic and schedules true back/front split support separately.
[ ] The plan derives placement and movement metadata from code first.
[ ] The plan treats road migration as a gameplay/runtime migration, not only a loader/exporter change.
[ ] The plan either keeps current runtime asset paths or explicitly schedules the code changes needed to replace them.
[ ] The plan seeds effects and user_interface from dumpMissingTilesetAssets().
```

---

## 10. Practical implementation order

Recommended execution order:

1. Generate v2 inventory from current code and validators.
2. Make `VisualTileParts` the explicit JSON/export bridge.
3. Add missing ground/decal/state coverage.
4. Add RenderModel v2 fields.
5. Add projectile runtime identity.
6. Add save migration.
7. Migrate road gameplay/runtime handling.
8. Add feature split art/runtime support.
9. Revisit runtime path reorganisation only after the renderer and loaders can consume it.

This keeps the plan aligned with the actual codebase instead of pretending the end-state data model already exists.
