# Realm Visual Asset Architecture

This is the canonical design note for Realm's generated art, runtime visual states, and tile/sprite terminology.

It exists because the old `Terrain` enum mixes several different visual concepts into one field. A tile such as `T_GRASS` is a floor material. A tile such as `T_BERRY` is really a normal biome floor plus a berry-bush object. A tile such as `T_MOUNTAIN` is a rocky floor plus a large blocking upright feature. The game can keep the legacy enum while migrating, but generated art and future runtime code should use the explicit layer model below.

Lighting-only changes are not generated art. Dawn, dusk, night, fog darkness, storm darkness, and selection dimming should be renderer colour/effect work unless a mechanic adds or removes a visible object such as torches, damage, cargo, snow buildup, or resource depletion.

## Current State

The current runtime has:

- `Tile::terrain`: one legacy `Terrain` value.
- `Tile::resources`: resource amount for gatherable terrain.
- `Tile::biome`: biome context.
- `Tile::preWinterTerrain`: winter restore snapshot.
- `Tile::wear`: traffic and settlement creep amount.

The SDL renderer already behaves like a layered renderer even though the data model is not explicit:

1. It draws a projected isometric ground diamond.
2. It applies procedural terrain texture and weather/season colour.
3. It draws an upright terrain glyph or object marker for many resource/object terrains.
4. It draws units, buildings, overlays, projectiles, fog, UI markers, and health bars above that.

The target architecture formalizes this split so generated prompts, map generation, save data, and rendering all describe the same thing.

## Current Sprite Status and Fallback Contract

Sprites are not generally implemented yet. The current game should be treated as a symbol/emoji-first renderer with optional sprite overrides.

Current status:

- Peasant idle is the only production sprite lane that should be assumed to exist.
- Most units, animals, buildings, terrain features, decals, effects, and non-idle peasant actions still render through the existing emoji/symbol/ASCII fallback path.
- Terrain image tiles are not the active baseline yet; the renderer still relies on procedural ground diamonds plus terrain glyphs/object markers.

Fallback is a required runtime contract:

- Every entity, terrain feature, terrain ground, decal, overlay, and effect must have a readable fallback symbol, emoji, or procedural rendering.
- Missing PNGs, incomplete manifests, missing team masks, missing directions, missing states, and missing frames must not produce blank map cells.
- Fallbacks must preserve gameplay readability: owner/team, selected unit, HP/damage, death/decay, resource type, blockers, fog/explored state, and action markers must still be understandable.
- Sprite support is additive. Adding one sprite must not require every related state to be complete before the game remains playable.
- The renderer should log or export missing-asset information for production tracking, but missing art should not be a runtime failure in normal play.

Fallback order:

1. Use a valid loaded sprite/frame if the requested asset exists.
2. Use a same-entity or same-feature symbolic fallback if the exact action/state/direction is missing.
3. Use the current emoji/symbol fallback if available.
4. Use the existing one-character ASCII glyph as the final fallback.

This means the visual architecture describes where sprites will go, not a requirement that the sprite set already exists.

## Terminology

Use these words consistently in code, docs, and prompt exports.

| Term | Meaning | Generated as | Gameplay |
|---|---|---|---|
| Ground | Required floor material of every map cell. | Top-down square tile. The app projects it into an isometric diamond. | Movement cost, passability, biome/season/weather material response. |
| Feature | Optional gameplay object attached to a cell. | Transparent sprite, usually upright; can overhang its cell. | Resources, blockers, concealment, harvestable objects, gates/ruins, mountain peaks. |
| Decal | Optional non-gameplay or very-low-gameplay mark on the ground. | Transparent low/flat overlay. | Visual variation, flowers, scuffs, puddles, settlement clutter, path creep. |
| Overlay | Temporary or derived visual layer. | Transparent overlay or procedural renderer effect. | Snow cover, rain splashes, build previews, range rings, selection, command markers. |
| Entity | Unit, animal, ship, siege engine, or building. | Transparent sprite or sliced building footprint. | HP, owner, team colour, actions, construction, garrison, production. |
| Effect | Short-lived tactical/UI/weather visual. | Separate effects/UI sheets. | Projectiles, impacts, weather particles, alerts, rally markers. |
| Colour effect | Renderer transform. | Not generated as base art. | Time-of-day tint, fog, concealment, storm dimming, team colour remap. |
| Composite terrain | Legacy authoring/runtime concept that maps to ground plus optional feature/decals. | Not a final asset type. | Transitional bridge from current `Terrain`. |

The most important rule: every map square has exactly one `Ground`; everything else is optional layered content.

## Draw Order

Target draw order:

1. Ground source tile, authored as a top-down square and projected by the app into the isometric diamond.
2. Ground transition masks and autotile edges.
3. Ground decals: flowers, tufts, stones, puddles, path scuffs, settlement halo pieces.
4. Feature shadow.
5. Feature back sprite: the part that should appear behind units, such as trunks, distant reeds, or mountain base.
6. Feature state overlay: depletion, snow cap, wetness, frost, broken/cracked state.
7. Entity/building shadow.
8. Entity/building base sprite.
9. Team-colour mask layer for owned entities/buildings/ships.
10. Entity/building state overlay: construction, damage, garrison, training, research, cargo, lit torches.
11. Feature front/occluder sprite: foliage, foreground reeds, canopy edges, and any visual part that should cover units standing inside or behind the feature.
12. Effects/UI: projectiles, impacts, weather particles, command markers, selection, range, build previews.
13. Fog/explored/night/storm colour effects.

## Target Tile Data Model

The long-term shape should be:

```cpp
enum GroundType {
    G_GRASS, G_MEADOW, G_DIRT, G_ROAD, G_MUD, G_SAND, G_DUNES,
    G_SNOW, G_TUNDRA, G_ICE, G_WATER, G_SHALLOWS, G_MARSH,
    G_GRAVEL, G_ASH, G_LAVA, G_HILLS, G_ROCKY, G_CASTLE_FLOOR
};

enum FeatureType {
    F_NONE,
    F_FOREST, F_PINE, F_PALM, F_DEAD_TREE,
    F_BERRY_BUSH, F_WHEAT_CROP, F_FISH_SHOAL,
    F_GOLD_DEPOSIT, F_STONE_BOULDERS, F_MOUNTAIN_PEAK,
    F_REEDS, F_RUINS, F_CASTLE_WALL, F_CASTLE_GATE
};

enum FeatureState {
    FS_DEFAULT,
    FS_FULL, FS_MOSTLY_FULL, FS_MOSTLY_EMPTY, FS_DEPLETED,
    FS_OPEN, FS_CLOSED, FS_LOCKED,
    FS_DAMAGED, FS_BROKEN
};

enum FeatureTrait : uint32_t {
    FT_BLOCKS_MOVEMENT      = 1u << 0,
    FT_SLOWS_MOVEMENT       = 1u << 1,
    FT_CONCEALS_UNITS       = 1u << 2,
    FT_REDUCES_LINE_OF_SIGHT= 1u << 3,
    FT_CAN_HIDE_WILDLIFE    = 1u << 4,
    FT_HARVESTABLE          = 1u << 5
};

struct Tile {
    GroundType ground;
    FeatureType feature;
    int featureResources;
    FeatureState featureState;
    Biome biome;
    GroundType preWinterGround;
    FeatureType preWinterFeature;
    int wear;
    // Later: small fixed decal list or decal bitset.
};
```

Do not jump straight to this save-format change. First add a bridge function such as `visualPartsForTerrain(Tile)` that converts the current `Terrain` enum into this shape. That lets the renderer, prompt exporter, and tests become layer-aware before save migration.

`FeatureTrait` can be derived from `FeatureType` during the bridge phase. It does not need to be stored per tile until there is a gameplay reason for local feature variation.

## Legacy Terrain Mapping

The migration bridge should map current `Terrain` values as follows.

| Legacy terrain | Ground | Feature | Decal/overlay notes |
|---|---|---|---|
| `T_GRASS` | `G_GRASS` | none | Base biome ground. |
| `T_TALL_GRASS` | `G_GRASS` or `G_MEADOW` | none initially | Tall-grass clumps are decals unless promoted to a concealing feature later. |
| `T_FLOWERS` | `G_GRASS` | none | Flower patches are decals. |
| `T_MEADOW` | `G_MEADOW` | none | Seasonal ground variants. |
| `T_FOREST` | biome default ground | `F_FOREST` | Harvestable wood, passable with movement penalty, conceals units/wildlife, partially reduces line of sight. |
| `T_PINE` | biome default ground or `G_TUNDRA` in snow biome | `F_PINE` | Harvestable wood, passable with movement penalty, conceals units/wildlife, partially reduces line of sight. |
| `T_PALM` | `G_SAND` | `F_PALM` | Harvestable wood, desert/oasis feature. |
| `T_DEAD_TREE` | biome default ground or `G_ASH` | `F_DEAD_TREE` | Harvestable wood, dead upright tree. |
| `T_MOUNTAIN` | `G_ROCKY` | `F_MOUNTAIN_PEAK` | Blocking tall feature that may overhang neighbouring tiles. |
| `T_HILLS` | `G_HILLS` | none | Ground material with slope/ridge detail; not a tall sprite by default. |
| `T_STONE` | `G_ROCKY` or biome default ground | `F_STONE_BOULDERS` | Blocking boulder feature. |
| `T_WATER` | `G_WATER` | none | Animated/weather-reactive ground. |
| `T_SHALLOWS` | `G_SHALLOWS` | none | Animated/weather-reactive ground. |
| `T_MARSH` | `G_MARSH` | none | Weather-reactive ground; puddles are overlays. |
| `T_REEDS` | `G_MARSH` or `G_SHALLOWS` | `F_REEDS` | Concealing wetland feature; draw units partly behind reeds and reduce visibility through the tile. |
| `T_GOLD` | `G_ROCKY` or `G_DIRT` | `F_GOLD_DEPOSIT` | Harvestable resource feature. |
| `T_SAND` | `G_SAND` | none | Mostly season-invariant. |
| `T_DUNES` | `G_DUNES` | none | Ground relief, not upright feature. |
| `T_SNOW` | `G_SNOW` or `G_TUNDRA` | none | Snow biome floor or fully snow-covered replacement terrain. |
| `T_ICE` | `G_ICE` | none | Frozen water/ground material. |
| `T_DIRT` | `G_DIRT` | none | Wear/path/settlement ground. |
| `T_ROAD` | `G_ROAD` | none | Wear/path ground. |
| `T_MUD` | `G_MUD` | none | Weather-reactive ground. |
| `T_WHEAT` | `G_DIRT` or field ground | `F_WHEAT_CROP` | Harvestable crop feature; depletion becomes stubble. |
| `T_BERRY` | biome default ground, normally `G_GRASS` | `F_BERRY_BUSH` | Harvestable berry-bush feature; depletion can leave empty bush or grass. |
| `T_FISH` | `G_WATER` or `G_SHALLOWS` | `F_FISH_SHOAL` | Harvestable water feature/decal. |
| `T_RUINS` | `G_GRAVEL` or `G_DIRT` | `F_RUINS` | Ruin object/footprint, with per-building ruin footprints later. |
| `T_GRAVEL` | `G_GRAVEL` | none | Ground material. |
| `T_LAVA` | `G_LAVA` | none | Animated glowing ground plus steam/weather overlays. |
| `T_ASH` | `G_ASH` | none | Volcanic ground. |
| `T_CASTLE_WALL` | `G_CASTLE_FLOOR` or `G_GRAVEL` | `F_CASTLE_WALL` | Blocking structure feature, not a normal building entity. |
| `T_CASTLE_FLOOR` | `G_CASTLE_FLOOR` | none | Ground paving. |
| `T_CASTLE_GATE` | `G_CASTLE_FLOOR` | `F_CASTLE_GATE` | Open/closed/locked feature state. |

Decision: hills are ground, mountains are ground plus feature. This matches the visual distinction the user described and avoids asking the image generator to make a single terrain tile that is both a floor and a tall object.

## Asset Generation Rules

### Ground

Ground prompts must say:

- Generate a top-down square tile.
- Do not draw an isometric diamond.
- Make it tileable or at least edge-compatible.
- Include season/weather/material states only when the actual material identity changes.

Examples: grass, meadow, dirt, road, mud, sand, dunes, snow, tundra, ice, water, shallows, marsh, gravel, ash, lava, hills, rocky ground, castle floor.

### Feature

Feature prompts must say:

- Generate a transparent-background sprite anchored to the tile centre.
- The sprite may be upright and may overhang the logical tile.
- For concealing features, generate or define front/occluder coverage separately from the back/contact part so units can appear behind foliage or reeds.
- Include resource depletion, open/closed, damaged, snowcap, wetness, and seasonal states where gameplay or silhouette changes.
- Do not include the full ground tile unless the feature needs a small contact patch or shadow.

Examples: berry bush, tree cluster, pine cluster, palm, dead tree, mountain peak, boulders, ore deposit, fish shoal, wheat crop, reeds, ruins, castle wall/gate.

## Concealing Features

Some features are not just decorative sprites. They alter visibility and drawing order.

Decision: reeds, forests, and pine forests are concealing features. Units standing in or behind them should appear partly behind the feature sprite, and line of sight through those tiles should be reduced. Concealment works both ways: wild animals in forest/reeds can be hidden from the player, and player units can also be harder to see.

Initial concealing feature set:

- `F_FOREST`
- `F_PINE`
- `F_REEDS`

Likely later candidates:

- `F_WHEAT_CROP`, for hiding small units in standing crops.
- promoted tall-grass features, if tall grass becomes more than visual variation.
- dense swamp vegetation variants.

Rendering rule:

- Draw feature back/contact art before entities.
- Draw units/buildings normally.
- Draw feature front/occluder art after entities.
- Apply visibility/tint/alpha effects from concealment after the layer stack, not by creating duplicate unit sprites.

Gameplay rule:

- Forest and pine should be passable with a movement penalty, not hard blockers.
- Reeds should partially conceal and may also slow movement if the underlying ground is marsh/shallows.
- Concealing features should reduce or filter line of sight rather than behaving exactly like walls.
- Hidden wildlife risk belongs to concealing features, especially forests/pines and dense reeds.

Prompt/export rule:

- Concealing feature prompts should mention `back` and `front_occluder` layers.
- If the art tool can only produce one image, use the same sprite plus an occlusion mask until split-layer art exists.

### Decal

Decal prompts must say:

- Generate a transparent low/flat overlay.
- The decal should sit on the ground and not imply an independent blocking object.
- It can be randomly selected or derived from wear/building proximity.

Examples: flowers, grass tufts, small stones, puddles, path scuffs, wheel ruts, settlement dirt, cobble patches, crates, barrels, log piles, farm tracks.

### Effects/UI

Effects need their own export group. They should not be mixed into unit, building, or terrain prompts.

Required families:

- Projectiles: arrow, tower/garrison bolt, warship shot, catapult boulder.
- Impacts: melee hit spark, arrow hit, boulder impact, boulder water splash, building hit dust.
- Weather particles: rain frame 1/2, storm rain frame 1/2, snowfall frame 1/2.
- Command markers: move, attack, gather, build, rally, attack-move, hold-position.
- Tactical overlays: selection, group selection, range-ring dot, build preview valid/invalid, wall preview, garrison indicator.
- Research/production UI: queued unit marker, research active marker, completed research icon treatment.

Effects should define `projection` explicitly:

- `tile_overlay`: drawn flat over a tile.
- `upright_world`: sprite anchored in world space.
- `screen_ui`: drawn in UI coordinates.

## State Coverage Decisions

### Units

Keep current action-based unit states. All human units need:

- living actions from runtime animation specs;
- `dead`;
- `decayed`.

For humans, `decayed` means skeleton remains with non-organic equipment still visible. Armour, weapons, bows, shields, and tools do not decompose.

For siege/naval units, `dead` and `decayed` mean destroyed wreck and weathered wreckage.

### Animals and Carcasses

Animal prompt states should be:

- `idle`
- `walk`
- species attack/flee states where applicable
- `dead_unharvested`
- `partly_harvested`
- `mostly_harvested`
- `depleted_skeleton`

Runtime should be changed so food animals become harvestable carcasses instead of always granting all food immediately on kill. The proposed runtime fields are:

```cpp
int carcassFoodRemaining;
int carcassFoodMax;
```

Derived visual state for harvestable food animals:

- `dead_unharvested`: 76-100% carcass food remaining.
- `partly_harvested`: 36-75% remaining.
- `mostly_harvested`: 1-35% remaining.
- `depleted_skeleton`: 0 remaining or death decay threshold passed.

Deer, sheep, and boar should have carcass food. Wolves should still get the same visual carcass state set so animal sheets stay consistent, but wolf carcass harvesting is not enabled in gameplay. Runtime should set wolf `carcassFoodMax` and `carcassFoodRemaining` to 0 unless a later design explicitly adds pelt/meat.

### Buildings

All buildable buildings need consistent states:

- `construction_0_foundation`
- `construction_1_frame`
- `construction_2_nearly_complete`
- `complete`
- `damaged`
- `snow_light`
- `snow_heavy`
- `rain_frame_1`
- `rain_frame_2`
- `night_lit` only if the building visibly adds torches/windows, not for simple darkness.

Construction is driven by HP/progress:

- 0-33%: foundation
- 34-66%: frame
- 67-99%: nearly complete
- 100%: complete

Damage is driven by HP:

- 51-100%: normal complete state.
- 1-50%: damaged state.
- 0%: destroyed; large buildings become ruin footprint terrain/feature.

Do not create a full damaged variant of every seasonal/weather state initially. Use base building state plus weather/snow overlays, then add bespoke combinations only where they materially improve readability.

### Building Production and Research

Production buildings:

- Town Hall: `training_peasant`.
- Barracks: `training_infantry`.
- Stable: `training_cavalry`.
- Dock: `training_ship`.
- Castle: `training_peasant` if current AI/base mechanics keep using Castle as a peasant-producing fallback.

Research:

- Blacksmith gets `researching_iron_weapons` and `researching_crossbows`.
- Completed research should primarily be UI/effect state, not a permanent building change.
- Visible unit upgrades should be represented in-world on the affected units.

Upgrade variant decision:

- `iron_weapons`: subtle upgraded weapon/metal treatment for militia and knight actions.
- `crossbows`: subtle crossbow/equipment treatment for archer actions.

The first implementation can use equipment overlays to avoid multiplying every action sheet immediately, but the target asset model should support full upgraded unit action variants where needed. Prompt/export metadata should be able to say whether a research variant is `overlay_only`, `full_action_variant`, or `hybrid`.

### Transport and Garrison

Transport has cargo/garrison states:

- `empty`
- `loaded_partial`
- `loaded_full`
- `load_unload`
- normal movement/idle/death/wreck states

Passenger identities should not be drawn. Use weight, covered cargo, flags, or silhouette cues.

Town Hall, House, Tower, and Castle:

- `garrisoned`
- `garrison_firing` only if the building body visibly changes while firing.

Projectiles are still separate effects. Queue length and progress bars are UI, not sprite states.

### Terrain Seasons, Weather, and Depletion

Ground states are material states, not animation frames. Use only the states needed for visible mechanic changes:

- Seasonal ground where season changes material or vegetation.
- Rain/storm two-frame overlays only where wetness/splash/steam matters.
- Snow/frost overlays where snow materially changes the surface.
- Depletion states for harvestable features.

The 16-cell sheet cap remains:

- If a prompt requires more than 16 states, split into multiple 4x4 images.
- Do not include runtime paths, manifests, source file paths, or frame recommendations in image-generation prompts.

### Snow Biome

Decision: keep snow as both a biome floor concept and a possible replacement/coverage ground.

- `B_SNOW` uses tundra/snow ground states:
  - `tundra_spring_thaw`
  - `tundra_summer_bare_ground`
  - `tundra_autumn_refreeze`
  - `tundra_winter_snow`
- Generic snow cover remains a weather/season overlay on other grounds.
- `G_SNOW` is used when a tile is so fully snow-covered that the original ground is visually replaced.

This means a snow biome has a real floor. It is not just a global tint.

### Settlement Halo and Path Creep

Settlement halo/path creep should be decals and overlays derived from `Tile::wear` and nearby completed buildings. Do not bake them into building sprites.

Required decal families:

- dirt scuffs
- packed path marks
- cobble patches
- wheel ruts
- yard clutter
- crates/barrels
- log piles
- farm tracks
- muddy footprints
- snow-trampled path marks

Runtime derivation:

- Low wear: sparse scuffs.
- Medium wear: dirt path fragments.
- High wear: road/cobble/path creep.
- Building adjacency can add themed decals: logs near lumber camps, ore bins near mining camps, sacks near mills, barrels near docks/markets.

This keeps the map looking settled without requiring a unique ground tile for every building adjacency combination.

## Exporter Architecture

Final game-loadable tileset assets live under `assets/tiles/`. `assets/tiles/entities/<entity>/manifest.json` and its referenced `*_base.png` / `*_teammask.png` files are the runtime contract.

## Runtime Placement Contract

Any runtime asset that is not a full tile-space ground should declare how it is positioned. The normal rule is:

```text
source-pixel anchor -> projected world or screen anchor -> derived draw rectangle
```

Callers should not guess sprite rectangles. They should provide the semantic anchor point, such as a tile centre, footprint origin, projectile world position, or screen UI point. The renderer derives the draw rectangle from the manifest placement and selected frame.

Actor-like manifests use a top-level `placement` block:

```json
{
  "projection": "upright_world",
  "anchor_kind": "feet",
  "source_size": [48, 48],
  "anchor": [24, 39],
  "scale_policy": "entity_tile_zoom_1_55",
  "footprint": [1, 1],
  "depth": "entity"
}
```

Frame entries may override `anchor` only when that frame genuinely differs. `final_bbox` and `anchor_offset` are QA metadata, not placement instructions.

Accepted upright assets must pass the generic placement verifier:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-placement `
  --manifest assets\tiles\entities\<slug>\manifest.json `
  --review-out build\tileset-review\<slug>-placement
```

The review output includes a bullseye placement sheet that proves the manifest anchor lands on the intended tile/world anchor.

Generated image-planning and production-workspace artifacts live under:

- `art/tiles/image-spec`: human-readable Markdown image specifications and generation prompts.
- `art/tiles/image-json`: lower-level JSON specs for debugging, auditing, and tooling.

The prompt exporter should output these top-level groups under `art/tiles/image-spec`:

- `grounds`
- `features`
- `decals`
- `units`
- `animals`
- `buildings`
- `effects-ui`

Each Markdown prompt should include only image-generation-relevant fields:

- name
- source role
- visual design
- layer category
- projection
- footprint or anchor
- team colour required
- team colour slots
- directions
- states/actions
- grid layout
- prompt text

It should omit:

- runtime root paths
- manifests
- source files
- slug-only technical names unless needed as labels
- frame recommendations when only one generated image per state is required

Ground prompt example:

```text
Generate sprites for grass ground. This is a top-down square source tile that the app will project into an isometric diamond. Team colour is not required. Generate 4 states in a 2 by 2 grid: spring, summer, autumn, winter.
```

Feature prompt example:

```text
Generate sprites for berry bush feature. This is a transparent upright sprite anchored to the tile centre over normal biome ground. Team colour is not required. Generate 16 states in a 4 by 4 grid: spring_full, spring_mostly_full, ... winter_depleted.
```

Building prompt example:

```text
Generate sprites for Town Hall. This is a player-owned building with team colour mask slots on banners, cloth trim, roof markers, or shields. Team colour is required. Generate state sheets for construction, complete, damaged, garrisoned, garrison_firing, training_peasant, rain, snow, and night_lit.
```

Effects prompt example:

```text
Generate Realm effects UI sprites. These are transparent overlays, not terrain or entity sprites. Include arrows, bolts, boulders, impact puffs, weather particles, command markers, selection rings, rally marker, build preview valid/invalid, and research/production markers.
```

## Runtime Migration Plan

### Phase 1: Documentation and Export Contract

- Treat this document as the visual architecture source of truth.
- Update the prompt exporter to emit ground/feature/decal/effects groups.
- Update terrain prompts so ground is top-down square art, not isometric diamond art.
- Keep the JSON spec exporter in `art/tiles/image-json` as a lower-level/debug export if useful, but do not make it the image-generation source of truth.

### Phase 2: Visual Bridge

- Add a `VisualTileParts` helper that maps current `Tile` to ground, feature, feature state, and decals.
- Use it in prompt export and renderer decisions.
- Keep `Tile::terrain` in saves during this phase.
- Add tests for critical mappings: berries, forest, hills, mountain, stone, fish, ruins, snow biome, castle wall/gate.

### Phase 3: Renderer Asset Support

- Add ground tile image loading and projection/caching.
- Add feature sprite loading, anchor metadata, shadow handling, overhang support, and front/back occluder layer support.
- Add decal loading and deterministic tile variant choice.
- Add effects/UI asset loading.
- Keep procedural/emoji/symbol/ASCII fallbacks as a permanent safety path, not just until the asset set is complete.

### Phase 4: Runtime Mechanics

- Add animal carcass food fields and peasant carcass harvesting.
- Add building construction stage selection from HP/progress.
- Add consistent damaged building visual state from HP.
- Add transport loaded state from garrison count.
- Add production/research building state selection from queue/research fields.
- Add settlement halo/path creep decal selection from `wear` and building proximity.
- Add concealing feature gameplay for reeds, forests, and pines: movement penalty, partial line-of-sight reduction, unit occlusion, and hidden wildlife risk.

### Phase 5: Save Model Migration

- Add explicit `ground`, `feature`, and `featureResources` fields to `Tile`.
- Load old saves by mapping legacy `Terrain` through `VisualTileParts`.
- Save new format with explicit visual/gameplay layers.
- Keep compatibility code until old saves are no longer relevant.

## Resolved Decisions

These were open questions in the first version of this architecture and are now pinned.

1. Wolves get matching dead/harvested/skeleton sprite states for visual consistency, but wolf carcass harvesting is disabled in gameplay.
2. Reeds are a concealing feature, not just a decal. Forests and pines are also concealing/passable features with movement penalty, partial line-of-sight reduction, and hidden wildlife risk.
3. Research upgrades should eventually have visible unit variants. The first runtime implementation may use subtle equipment overlays, but the asset architecture must support full upgraded action variants.

No current unresolved visual-architecture questions block the next implementation phase.

## Completion Tests

These are the tests that should pass before calling the visual-asset migration complete. They are deliberately written as observable checks rather than only code-structure goals.

### Export Tests

- Prompt export produces `grounds`, `features`, `decals`, `units`, `animals`, `buildings`, and `effects-ui` groups.
- Ground prompts say top-down square source tile, not isometric diamond source art.
- Feature prompts say transparent anchored sprite and, for concealing features, include `back` and `front_occluder` layers.
- Animal prompts include carcass states for deer, sheep, boar, and wolf.
- Wolf prompt explicitly says wolf carcass harvesting is not enabled in gameplay.
- Unit research prompts include visible upgrade variants or overlays for `iron_weapons` and `crossbows`.
- No generated image prompt includes runtime roots, manifests, source file paths, or frame recommendations for one-frame state generation.
- Any prompt with more than 16 states is split into multiple 4x4 sheets.

Suggested command:

```powershell
python -m py_compile scripts\export_image_generation_prompts.py scripts\export_tile_specs.py
python scripts\export_image_generation_prompts.py --clean
python scripts\export_tile_specs.py --clean
```

### Fallback Rendering Tests

- With no production sprites installed, a normal game renders readable procedural ground plus emoji/symbol/ASCII units, animals, buildings, resources, projectiles, markers, fog, and UI.
- With only peasant idle sprites installed, peasant idle uses sprites and every other peasant action falls back to the current glyph/symbol path.
- If a manifest exists but one action, direction, frame, base PNG, or team-mask PNG is missing, the affected object falls back visibly instead of drawing blank.
- Terrain features without sprites still render their current fallback symbols or emoji.
- Effects without sprites still render their current projectile/weather/marker fallback symbols.
- Missing-asset logging/reporting identifies absent art without failing normal gameplay.

Suggested checks:

```powershell
mingw32-make test
mingw32-make gui
bin\realm.exe --dump-missing-tileset-assets
```

If `--dump-missing-tileset-assets` does not exist yet, add it or keep using the renderer's existing missing-tile manifest output until there is a deterministic CLI check.

### Runtime Mechanics Tests

- Dead humans show `dead`, then `decayed`; decayed humans retain weapons, armour, bows, tools, or other non-organic equipment.
- Deer, sheep, and boar become harvestable carcasses with four depletion visual states.
- Wolf death can show the same visual carcass/depletion states, but peasants cannot harvest wolf carcasses and wolf carcass food remains zero.
- Buildings select construction stages from progress/HP: foundation, frame, nearly complete, complete.
- All buildings can show damaged state below the chosen HP threshold.
- Town Hall and Castle can show training state when producing peasants.
- Transport shows empty, partially loaded, and full states from garrison count.
- Blacksmith research state is visible while researching; completed research can update unit upgrade variants or overlays.
- Settlement halo/path creep decals appear from wear/building proximity and do not require unique baked building sprites.
- Reeds, forests, and pines conceal units with front/back occluder rendering, reduce visibility through the tile, and apply movement penalties where intended.
- Concealment works both ways: hostile wildlife in concealing features can be hidden from the player, and player units can also be obscured.

### Visual QA Tests

- Ground art stays top-down before projection and aligns cleanly after in-app isometric projection.
- Feature sprites anchor to the tile centre and can overhang without jumping or resizing the logical footprint.
- Front/occluder feature layers actually cover units only where vegetation should cover them.
- Team-colour masks apply to owned units/buildings/ships and never use transparency as the team-colour signal.
- Sprite fallback and sprite rendering can coexist on the same screen without scale or anchor mismatch that breaks gameplay readability.
- Generated review sheets include contact sheet, isometric placement view, alpha/mask checks, and anchor/bounding-box overlay for any accepted production asset.

Suggested production-asset check for the currently implemented peasant idle lane:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-peasant-idle `
  --manifest assets\tiles\entities\peasant\manifest.json `
  --review-out build\tileset-review\peasant-idle
```

## Implementation Validation Status - 2026-06-01

Implemented validation covered the exporter split, visual tile bridge, fallback-safe renderer hooks, animal carcass runtime, building/transport/research visual-state selectors, concealing feature movement/acquisition behavior, missing-asset reporting, SDL smoke rendering, and the peasant idle production lane.

Passing checks:

```powershell
python -m py_compile scripts\export_image_generation_prompts.py scripts\export_tile_specs.py
python scripts\export_image_generation_prompts.py --clean
python scripts\export_tile_specs.py --clean
python <prompt contract assertions>
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH; C:\msys64\ucrt64\bin\mingw32-make.exe test
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH; .\bin\realm.exe --dump-missing-tileset-assets
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-peasant-idle --manifest assets\tiles\entities\peasant\manifest.json --review-out build\tileset-review\peasant-idle
$env:PATH = 'C:\msys64\ucrt64\bin;' + $env:PATH; $env:REALM_SMOKE_TEST='match'; .\bin\realm.exe
```

Known build-script blocker:

- `C:\msys64\ucrt64\bin\mingw32-make.exe gui` links `bin\realm.exe`, but the Makefile `copy-windows-runtime` recipe fails in this PowerShell/MSYS2 invocation with `dll was unexpected at this time.` The direct `bin/realm.exe` target is up to date, and the linked binary passed the missing-asset dump and SDL smoke checks.
