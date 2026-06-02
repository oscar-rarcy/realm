# Realm Asset JSON v2 Implementation Specification

**Document status:** implementation-ready migration specification  
**Target audience:** coding agent / implementation agent  
**Primary goal:** align the existing generated Realm asset JSON with the updated tile, rendering, projection, and asset-category model.

---

## 1. Executive summary

The current Realm asset JSON already contains a substantial asset catalogue and a lot of useful design intent. The implementation should not discard it. Instead, migrate the current v1 JSON shape into a clearer v2 structure that separates:

```text
asset identity
runtime/gameplay stats
rendering and projection rules
art-direction notes
season/weather variants
resource/passability behaviour
placement/footprint behaviour
animation/action metadata
source provenance
```

The target content categories are:

```text
grounds
features
decals
units
animals
buildings
projectiles
effects
user_interface
```

The important category changes are:

```text
ammunition -> projectiles
road ground -> road decal
animals remain separate from units but share actor-style implementation
features remain one-tile terrain objects
buildings may be multi-tile
```

The central rendering model is:

```text
grounds and flat decals form the board
features, buildings, units, animals, and projectiles stand or move on top of the board
UI renders outside the tile composition
```

The renderer and JSON should stop relying on free-text strings such as:

```text
top-down square source tile, projected into an isometric diamond in-app
transparent feature sprite anchored over a projected isometric map tile
transparent upright_world projectile sprite or tiny animation
```

Instead, each asset must get structured render metadata:

```json
"render": {
  "layer": "ground",
  "projection_mode": "surface_projected",
  "projection_factor": 1.0,
  "depth_bucket": "surface",
  "anchor": "tile"
}
```

The migration should be done in compatibility phases:

```text
read v1 and v2
write v2
warn on v1-only fields
remove v1 compatibility only after maps, renderer, exporters, docs, and tests are migrated
```

---

## 2. Current source inventory

The uploaded v1 JSON contains these generated groups:

| Current group | Count | Current category status | Target group |
|---|---:|---|---|
| `grounds` | 17 | Mostly correct, except `road` | `grounds` |
| `features` | 14 | Mostly correct, but needs one-tile/multi-tile ruin clarification | `features` |
| `decals` | 2 | Correct but under-specified | `decals` |
| `units` | 11 | Correct | `units` |
| `animals` | 4 | Correct as content category; should become actor-like in schema | `animals` |
| `buildings` | 16 | Correct | `buildings` |
| `ammunition` | 7 | Rename required | `projectiles` |
| `effects` | 0 | Missing | `effects` |
| `user_interface` | 0 | Missing | `user_interface` |

Current asset slugs by category:

```text
grounds:
  ash, castle_floor, dirt, dunes, grass, gravel, hills, ice, lava,
  marsh, meadow, mud, road, sand, shallows, snow, water

decals:
  flowers, tall_grass

features:
  berry, castle_gate, castle_wall, dead_tree, fish, forest, gold,
  mountain, palm, pine, reeds, ruins, stone, wheat

units:
  archer, catapult, fishing_boat, knight, militia, peasant, ram,
  spearman, transport, trebuchet, warship

animals:
  boar, deer, sheep, wolf

buildings:
  barracks, blacksmith, castle, church, dock, farm, gate, house,
  lumber_camp, market, mill, mining_camp, stable, tower, town_hall, wall

ammunition / target projectiles:
  arrow, catapult_boulder, crossbow_bolt, flaming_arrow, tower_bolt,
  trebuchet_boulder, warship_arrow_volley
```

Current generated schemas found in the v1 JSON:

```text
realm.tile_specs_index.v1
realm.terrain_sprite_spec.v1
realm.sprite_spec.v1
realm.ammunition_sprite_spec.v1
```

Current generator/source files referenced by specs include:

```text
scripts/export_tile_specs.py
scripts/export_image_generation_prompts.py
include/realm.h
src/core/entity_defs.cpp
src/core/terrain_defs.cpp
src/render/sdl/display_glyphs.cpp
docs/tileset/realm_tileset_visual_audit.md
```

These source fields must be preserved in v2. They are useful provenance and should not be dropped.

---

## 3. Design model to implement

## 3.1 Core tile composition

Each map square is a `Tile`. A tile is a layered composition:

```text
Tile
  ground              required, exactly one
  decals              optional list
  feature             optional, at most one normal terrain feature
  building reference  optional, if covered by a building footprint
  actors              optional units/animals/actor-like entities
  overlays            derived/render/UI state, not permanent tile content
```

Target conceptual runtime structure:

```json
{
  "coord": { "x": 10, "y": 12 },
  "ground": "grass",
  "decals": ["road", "flowers"],
  "feature": "forest",
  "building_instance_id": null,
  "actors": ["unit_123"],
  "weather_overlay": null,
  "derived": {
    "passability": {},
    "movement_cost": {},
    "visibility": {},
    "ownership": null
  }
}
```

Rules:

```text
Every tile must have exactly one ground.
A tile may have zero or more decals.
A tile may have zero or one normal feature.
A tile may be covered by zero or one building footprint.
Actors are not permanent tile layers; they occupy or pass through tile/world positions.
Projectiles are not tile contents; they are transient world objects.
UI is not part of the world tile composition.
```

---

## 3.2 Flat layer versus standing layer

The key split is:

```text
flat things are drawn top-down and projected into the board
standing things are drawn as upright sprites placed on top of the board
```

| Category | Example | Render behaviour |
|---|---|---|
| Ground | grass, sand, water, snow | flat board surface |
| Surface decal | road, dirt patch, blood, scorch mark | flat overlay projected with board |
| Semi-upright decal | flowers, tall grass, tufts | decal category but may visually lean toward upright |
| Feature | forest, mountain, stone, gold | upright or semi-upright object anchored to one tile |
| Building | house, barracks, castle, wall | upright sprite with building rules and footprint |
| Unit | peasant, archer, knight | upright actor sprite |
| Animal | deer, wolf, boar | upright actor sprite, separate category |
| Projectile | arrow, bolt, boulder | transient world object |
| Effect | dust, fire, splash | transient effect; projection varies by effect |
| UI | health bar, selection bracket | screen-space or world-overlay UI |

User-facing readability rule:

```text
Surface detail normally means non-interactable.
Standing object normally means potentially interactable.
```

This is a default rule, not an absolute law. Any exception must be explicit in JSON.

---

## 4. Target v2 file tree

Preferred target file tree:

```text
art/tiles/image-json/
  index.json
  grounds/
  decals/
  features/
  units/
  animals/
  buildings/
  projectiles/
  effects/
  user_interface/

assets/tiles/
  terrain/
    grounds/
    decals/
    features/
  entities/
    units/
    animals/
    buildings/
  projectiles/
  effects/
  ui/
```

A less disruptive intermediate tree is allowed:

```text
assets/tiles/terrain/<slug>
assets/tiles/entities/<slug>
assets/tiles/projectiles/<slug>
```

But the JSON `asset_type`, `render.layer`, and index groups must be correct even if runtime art paths are migrated later.

---

## 5. Target schemas

Recommended v2 schemas:

```text
realm.tile_specs_index.v2
realm.ground_spec.v2
realm.decal_spec.v2
realm.feature_spec.v2
realm.building_spec.v2
realm.actor_sprite_spec.v2
realm.projectile_sprite_spec.v2
realm.effect_spec.v1
realm.ui_asset_spec.v1
```

`realm.actor_sprite_spec.v2` should be used by both `unit` and `animal` asset types.

The implementation may alternatively use one common schema:

```text
realm.tile_asset_spec.v2
```

with validation branches by `asset_type`. Prefer separate schema files if practical, because they are easier for tools and agents to reason about.

---

## 6. Common v2 asset object

Every v2 asset spec should have this high-level shape:

```json
{
  "schema": "realm.<specific_schema>.v2",
  "asset_type": "ground",
  "id": "grass",
  "slug": "grass",
  "name": "Grass",
  "ui_name": "Grassland",
  "enum": "T_GRASS",

  "runtime": {},
  "render": {},
  "art": {},
  "gameplay": {},
  "placement": {},
  "seasonal": {},
  "weather": {},
  "variants": {},
  "states": [],
  "actions": [],
  "paths": {},
  "sources": [],

  "migration": {}
}
```

Required common fields:

| Field | Required | Notes |
|---|---:|---|
| `schema` | yes | v2 schema name |
| `asset_type` | yes | canonical category |
| `slug` | yes | stable machine ID and filename stem |
| `name` | yes | human name |
| `render` | yes | structured render/projection behaviour |
| `paths` | yes | generated/runtime asset paths |
| `sources` | yes | source provenance |

Recommended common fields:

| Field | Notes |
|---|---|
| `id` | normally same as `slug`; use for future references |
| `ui_name` | optional display name |
| `enum` | runtime enum, if one exists |
| `runtime` | runtime stats and legacy compatibility info |
| `art` | human art direction only; not renderer source of truth |
| `gameplay` | structured gameplay behaviour |
| `placement` | footprint, anchors, allowed placement |
| `seasonal` | season-specific variants |
| `weather` | weather transformations and overlays |
| `variants` | non-seasonal visual variants, autotiles, research variants |
| `states` | named visual/gameplay states |
| `actions` | animation/action definitions |
| `migration` | v1 compatibility, aliases, deprecation notes |

Authoritative-field rule:

```text
renderer reads render.*
gameplay reads gameplay.* and placement.*
season/weather systems read seasonal.* and weather.*
art.projection, art.runtime_notes, and art.required_variants are legacy/human notes only
```

---

## 7. Render object

Every asset must have a structured `render` object.

```json
"render": {
  "layer": "ground",
  "projection_mode": "surface_projected",
  "projection_factor": 1.0,
  "depth_bucket": "surface",
  "anchor": "tile",
  "depth_sort": {
    "enabled": false,
    "key": null,
    "tie_breakers": []
  }
}
```

### 7.1 `render.layer` values

Allowed values:

```text
ground
decal
feature
building
actor
projectile
effect
ui
```

Mapping:

| `asset_type` | `render.layer` |
|---|---|
| `ground` | `ground` |
| `decal` | `decal` |
| `feature` | `feature` |
| `building` | `building` |
| `unit` | `actor` |
| `animal` | `actor` |
| `projectile` | `projectile` |
| `effect` | `effect` |
| `user_interface` | `ui` |

### 7.2 `render.projection_mode` values

Allowed values:

```text
surface_projected
surface_decal
semi_upright_decal
upright_world
screen_space
```

Meanings:

| Value | Meaning |
|---|---|
| `surface_projected` | flat source art projected into board/isometric tile |
| `surface_decal` | flat transparent overlay projected with tile surface |
| `semi_upright_decal` | decal category, but visual angle leans partly upright |
| `upright_world` | sprite stands on board; image is not flattened into tile surface |
| `screen_space` | UI/panel/cursor/screen overlay |

### 7.3 `render.projection_factor`

Use a numeric factor to support future tuning:

```text
1.0 = fully board-projected / flat on ground
0.5 = semi-flat or semi-upright
0.0 = upright world sprite
```

The enum remains the primary authoring API. The factor is a renderer/control value.

Default values:

| Category | `projection_mode` | `projection_factor` |
|---|---|---:|
| ground | `surface_projected` | `1.0` |
| road decal | `surface_decal` | `1.0` |
| flowers/tall grass | `semi_upright_decal` | `0.25` to `0.5` |
| feature | `upright_world` | `0.0` |
| building | `upright_world` | `0.0` |
| unit/animal | `upright_world` | `0.0` |
| projectile | `upright_world` | `0.0` |
| UI | `screen_space` | `0.0` |

### 7.4 `render.depth_bucket` values

Allowed values:

```text
surface
surface_overlay
standing_back
building_back
actor
projectile
standing_front
building_front
effect
ui
```

Recommended render stack:

```text
1. Surface pass:
   ground
   surface decals
   weather/surface overlays

2. Depth-sorted world pass:
   semi-upright decals, if depth-sorted
   feature back layer
   building back layer
   actors: units and animals
   projectiles
   feature front layer
   building front layer
   world effects

3. UI pass:
   selection brackets
   health bars
   ownership markers
   movement previews
   command arrows
   fog of war
   cursor/tooltips/panels
```

Depth sort key for standing/world objects:

```text
primary: screen_y or tile_y after isometric projection
secondary: screen_x or tile_x
tertiary: depth_bucket priority
stable fallback: entity_id / instance_id
```

---

## 8. Ground spec

Grounds are the underlying base terrain of a tile.

Rules:

```text
Every tile has exactly one ground.
Grounds are always flat source art projected into the board.
Grounds are not interactable objects.
Grounds may affect passability, movement cost, weather, season, and biome.
Grounds should not be mixed half-and-half within one tile.
```

Target ground example:

```json
{
  "schema": "realm.ground_spec.v2",
  "asset_type": "ground",
  "id": "grass",
  "slug": "grass",
  "enum": "T_GRASS",
  "name": "Grass",
  "ui_name": "Grassland",
  "render": {
    "layer": "ground",
    "projection_mode": "surface_projected",
    "projection_factor": 1.0,
    "depth_bucket": "surface",
    "anchor": "tile",
    "depth_sort": { "enabled": false, "key": null, "tie_breakers": [] }
  },
  "gameplay": {
    "interactable": false,
    "selectable": false,
    "passability": {
      "land": "passable",
      "boat": "blocked"
    },
    "movement_cost": {
      "land": 1.0,
      "boat": null
    }
  },
  "seasonal": {
    "supports_seasons": true,
    "default_season": "summer",
    "variants": {
      "spring": { "notes": "fresh green growth" },
      "summer": { "notes": "normal/summer dry variant" },
      "autumn": { "notes": "duller autumn grass" },
      "winter": { "overlay": "snow_dusting", "notes": "mild snow overlay compatibility" }
    }
  },
  "weather": {
    "rules": [
      {
        "trigger": "winter_snow",
        "mode": "overlay_or_replace",
        "overlay": "snow_dusting",
        "replace_with": "snow",
        "needs_design_review": true
      }
    ]
  },
  "art": {
    "visual_design": "Default temperate grass, uneven small blades, no strong pattern",
    "legacy_projection": "top-down square source tile, projected into an isometric diamond in-app",
    "legacy_required_variants": "12 base variants; spring, summer dry, autumn dull, winter mild overlay compatibility",
    "legacy_runtime_notes": "Can wear into dirt/road; winter converts/overlays to snow",
    "team_color_required": false
  },
  "paths": {
    "runtime_root": "assets/tiles/terrain/grass",
    "base": "assets/tiles/terrain/grass.png"
  },
  "sources": []
}
```

### 8.1 Ground passability defaults

Initial structured mapping from current notes:

| Ground | Land | Boat | Movement | Notes |
|---|---|---|---|---|
| `grass` | passable | blocked | normal | can wear to dirt; winter snow overlay/replace |
| `meadow` | passable | blocked | normal | can host farms; winter snowed |
| `hills` | passable | blocked | slow | snow overlay |
| `water` | blocked | passable | boat normal | shoreline masks; freezes to ice |
| `shallows` | slow | passable | land slow | shoreline blend masks; freezes |
| `marsh` | slow | blocked | land slow | rain may affect movement |
| `sand` | passable | blocked | slow | shoreline wet edge; weather can slow further |
| `dunes` | passable | blocked | slow | desert identity |
| `snow` | passable | blocked | slow or normal, review | winter-converted terrain / tundra base |
| `ice` | passable | blocked | slippery/normal, review | replacement for water/shallows/marsh/reeds |
| `dirt` | passable | blocked | normal | created by traffic/buildings/depletion; can regrow grass |
| `mud` | slow | blocked | slow | created by rain/storm; dries to dirt |
| `gravel` | passable | blocked | normal or slow, review | winter overlay target |
| `lava` | blocked | blocked | blocked | volcanic blocker |
| `ash` | passable | blocked | slow | volcanic biome |
| `castle_floor` | passable | blocked | fast | mapgen ruin paving |
| `road` | legacy only | legacy only | legacy only | migrate to decal |

Balance numbers are not final unless already defined in engine constants. If no current constant exists, keep `needs_design_review: true`.

---

## 9. Decal spec

Decals are surface or semi-surface additions placed on top of grounds.

Rules:

```text
Decals are normally not interactable.
Decals may alter tile movement, visibility, status, or other modifiers.
A tile may support a list of decals.
Most art should use zero or one decal per tile unless stacking is explicitly safe.
Decals need structured projection settings.
Roads should be decals, not grounds.
```

Target decal example:

```json
{
  "schema": "realm.decal_spec.v2",
  "asset_type": "decal",
  "id": "road",
  "slug": "road",
  "enum": "D_ROAD",
  "legacy_enums": ["T_ROAD"],
  "name": "Road",
  "ui_name": "Stone Road",
  "render": {
    "layer": "decal",
    "projection_mode": "surface_decal",
    "projection_factor": 1.0,
    "depth_bucket": "surface_overlay",
    "anchor": "tile",
    "depth_sort": { "enabled": false, "key": null, "tie_breakers": [] }
  },
  "placement": {
    "allowed_grounds": ["grass", "meadow", "dirt", "sand", "snow", "castle_floor", "gravel"],
    "stacking": {
      "max_with_other_decals": 1,
      "exclusive_with": [],
      "allowed_with": ["flowers"]
    },
    "autotile": {
      "enabled": true,
      "connects_to": ["road"],
      "variants": ["straight", "corner", "t_junction", "cross", "end_cap", "worn"]
    }
  },
  "gameplay": {
    "interactable": false,
    "selectable": false,
    "movement_cost_modifier": {
      "land": -0.25
    },
    "status_modifiers": []
  },
  "weather": {
    "rules": [
      { "trigger": "traffic_wear", "mode": "create_or_strengthen" },
      { "trigger": "decay", "mode": "remove_decal", "result_ground": "dirt" }
    ]
  },
  "art": {
    "visual_design": "Packed road/cobble line, readable as movement route",
    "legacy_required_variants": "47 path autotiles; straight, corner, T, cross, ends; 6 worn variants",
    "legacy_runtime_notes": "Created by heavy traffic; decays to dirt"
  },
  "paths": {
    "runtime_root": "assets/tiles/terrain/road",
    "base": "assets/tiles/terrain/road.png"
  },
  "migration": {
    "from_asset_type": "ground",
    "from_group": "grounds",
    "map_migration": "T_ROAD -> ground + road decal"
  }
}
```

Target flowers/tall grass projection:

```json
"render": {
  "layer": "decal",
  "projection_mode": "semi_upright_decal",
  "projection_factor": 0.35,
  "depth_bucket": "surface_overlay",
  "anchor": "tile"
}
```

If semi-upright decals need to occlude actors, move them to depth-sorted world pass with:

```json
"depth_bucket": "standing_front",
"depth_sort": { "enabled": true, "key": "tile_screen_y", "tie_breakers": ["tile_screen_x", "slug"] }
```

Do not make flowers or tall grass selectable by default.

---

## 10. Feature spec

Features are terrain objects attached to a single tile.

Examples:

```text
forest, pine, palm, dead_tree, mountain, stone, reeds, gold, wheat, berry, fish, ruins, castle_wall, castle_gate
```

Rules:

```text
A normal feature has a gameplay footprint of exactly 1x1.
A feature may visually overhang its tile, but the anchor must remain tile-readable.
A feature may be a resource, blocker, concealment object, or mapgen/static object.
A feature may have back/main/front visual layers.
Features are not multi-tile buildings.
```

Target feature example:

```json
{
  "schema": "realm.feature_spec.v2",
  "asset_type": "feature",
  "id": "forest",
  "slug": "forest",
  "enum": "T_FOREST",
  "name": "Forest",
  "ui_name": "Oak Forest",
  "render": {
    "layer": "feature",
    "projection_mode": "upright_world",
    "projection_factor": 0.0,
    "depth_bucket": "standing_split",
    "anchor": "tile_center",
    "visual_overhang_allowed": true,
    "depth_sort": { "enabled": true, "key": "tile_screen_y", "tie_breakers": ["tile_screen_x", "slug"] }
  },
  "placement": {
    "footprint": { "w": 1, "h": 1 },
    "must_be_single_tile_feature": true,
    "allowed_grounds": ["grass", "meadow", "dirt"]
  },
  "feature_layers": {
    "back": { "asset_suffix": "back", "required": false, "draw_before_actor_on_same_tile": true },
    "main": { "asset_suffix": "main", "required": true },
    "front": { "asset_suffix": "front", "required": false, "draw_after_actor_on_same_tile": true }
  },
  "occlusion": {
    "mode": "actor_between_back_and_front",
    "applies_when_actor_inside_tile": true,
    "opacity_when_selected_actor_inside": 1.0
  },
  "gameplay": {
    "interactable": true,
    "selectable": false,
    "passability": { "land": "passable", "boat": "blocked" },
    "movement_cost": { "land": 1.5, "boat": null },
    "resource": {
      "type": "wood",
      "gatherable": true,
      "depletion_result": { "ground": "dirt", "feature": null }
    },
    "visibility_modifier": -1,
    "concealment": true
  },
  "seasonal": {
    "supports_seasons": true,
    "variants": {
      "autumn": { "variant_group": "autumn_leaves", "notes": "early/mid/late leaves" },
      "winter": { "overlay": "branch_snow", "notes": "winter branch snow" }
    }
  },
  "art": {
    "visual_design": "Deciduous tree canopy, trunks partly visible; oak/leafy mixed shapes",
    "legacy_required_variants": "16 tree variants; forest edge masks; autumn early/mid/late leaves; winter branch snow",
    "legacy_runtime_notes": "Gatherable wood; depleted to dirt"
  },
  "paths": {
    "runtime_root": "assets/tiles/terrain/forest",
    "base": "assets/tiles/terrain/forest.png"
  }
}
```

### 10.1 Feature front/back split

Required renderer behaviour for split features:

```text
If actor and split feature occupy the same tile:
  draw feature.back
  draw actor
  draw feature.front
```

If no split is available:

```text
draw feature.main in normal standing-object depth order
```

Recommended feature split priorities:

| Feature | Split required? | Reason |
|---|---:|---|
| `forest` | yes | unit should appear inside forest |
| `pine` | recommended | same reason as forest |
| `reeds` | optional/recommended | vertical vegetation, water/wetland concealment |
| `wheat` | optional/recommended | current render logic hides enemies; crop can obscure units |
| `berry` | optional | if units gather beside/inside bushes |
| `ruins` | optional | if actors can stand among ruins |
| `mountain` | no | likely impassable, no actor inside tile |
| `stone` | no | blocker/resource-looking terrain |
| `gold` | no | resource object; likely gathered adjacent |
| `fish` | no | water resource, actors/boats do not stand inside as land actors |

### 10.2 Ruins conflict rule

The current `ruins` spec mentions large-building ruin footprints. The v2 rule is:

```text
normal Feature = always 1x1
multi-tile destroyed building = BuildingRuinInstance or destroyed building state
one-tile decorative rubble = feature or decal
mapgen ancient wall/gate = feature only if each tile is independent
```

Migration for `ruins`:

```text
Keep T_RUINS as one-tile feature rubble.
Do not use T_RUINS to represent a multi-tile building footprint.
Represent destroyed large buildings through the building instance/state system.
If mapgen needs a large ruin, compose it from multiple one-tile ruin/castle_wall/castle_gate features or introduce a separate map_object system later.
```

---

## 11. Building spec

Buildings are standing sprites with building gameplay rules.

Rules:

```text
Buildings may be multi-tile.
Every tile covered by a building footprint points to the same BuildingInstance.
Only the building render origin draws the main sprite, unless the building is intentionally tile-composed like walls.
Gameplay footprint and visual sprite bounds are separate.
A building may visually overhang its footprint.
```

Target building example:

```json
{
  "schema": "realm.building_spec.v2",
  "asset_type": "building",
  "id": "barracks",
  "slug": "barracks",
  "enum": "E_BARRACKS",
  "name": "Barracks",
  "render": {
    "layer": "building",
    "projection_mode": "upright_world",
    "projection_factor": 0.0,
    "depth_bucket": "building",
    "anchor": "footprint_origin",
    "draw_from_origin_only": true,
    "visual_overhang_allowed": true,
    "depth_sort": { "enabled": true, "key": "footprint_screen_y", "tie_breakers": ["footprint_screen_x", "instance_id"] }
  },
  "placement": {
    "footprint": { "w": 3, "h": 2 },
    "origin": "south_west",
    "occupies_all_tiles": true,
    "blocks_placement": true,
    "requires_adjacent": [],
    "allowed_grounds": ["grass", "meadow", "dirt", "sand", "snow", "castle_floor"]
  },
  "gameplay": {
    "interactable": true,
    "selectable": true,
    "passability": { "land": "blocked", "boat": "blocked" },
    "ownership": "player",
    "construction": true,
    "resource": null,
    "garrison": null,
    "trains_units": true
  },
  "runtime": {
    "stats": {},
    "legacy_footprint": { "w": 3, "h": 2 },
    "team_color_required": true
  },
  "art": {
    "visual_design": "Long timber hall with weapon racks and practice yard",
    "team_color_slots": ["flags", "shield sign", "awning"]
  },
  "states": ["complete", "training", "construction", "ruin_footprint"],
  "actions": []
}
```

### 11.1 Current building footprints

Migrate current `runtime.footprint` into `placement.footprint`.

| Building | Current footprint |
|---|---:|
| `barracks` | 3x2 |
| `blacksmith` | 2x2 |
| `castle` | 4x4 |
| `church` | 2x2 |
| `dock` | 2x2 |
| `farm` | 1x1 |
| `gate` | 1x1 |
| `house` | 2x2 |
| `lumber_camp` | 2x2 |
| `market` | 2x2 |
| `mill` | 2x2 |
| `mining_camp` | 2x2 |
| `stable` | 3x2 |
| `tower` | 1x1 |
| `town_hall` | 3x3 |
| `wall` | 1x1 |

### 11.2 Farm classification

`farm` remains a building even though it looks like a surface/crop layer.

Reason:

```text
It is owned/interactable.
It has states: sowing, growing, ripe, tended, winter-dead/snowed, depleted/dead.
It has a gameplay role as a food generator.
It can be constructed/tended by a peasant.
It has team-colour indication.
```

Do not migrate farm to decal or feature.

### 11.3 Walls and gates

Walls and gates need connectivity metadata:

```json
"connectivity": {
  "enabled": true,
  "connects_to": ["wall", "gate"],
  "variants": ["straight", "corner", "t_junction", "cross", "end_cap"],
  "team_color_frequency": "intervals_only"
}
```

Gate state rules:

```text
closed = blocks enemies and maybe all land units depending gameplay
open = passable by allowed units
locked_open = forced open
locked_closed = forced closed
construction = blocks/partial based on existing engine rule
```

### 11.4 Dock placement

Dock has a 2x2 footprint and must touch water.

Target placement addition:

```json
"placement": {
  "footprint": { "w": 2, "h": 2 },
  "requires_adjacent": [
    { "type": "ground", "any_of": ["water", "shallows"], "relation": "edge_adjacent" }
  ],
  "allowed_grounds": ["grass", "sand", "dirt", "shallows", "water"],
  "needs_design_review": true
}
```

Exact dock footprint over land/water needs implementation review. Preserve current behaviour if already coded.

---

## 12. Units and animals as actors

Units and animals share actor implementation but remain separate content categories.

Rules:

```text
asset_type unit remains unit
asset_type animal remains animal
render.layer for both is actor
units and animals render as upright sprites
actors rest on tile centres
actors may interpolate between tile centres while moving
actors can render between feature back/front layers when occupying a split feature tile
```

Target actor object:

```json
"entity": {
  "kind": "actor",
  "actor_type": "unit",
  "rests_on_tile_center": true,
  "can_interpolate_between_tiles": true,
  "movement_domain": "land",
  "owner_model": "player",
  "occupancy": {
    "footprint": { "w": 1, "h": 1 },
    "tile_centered_rest_state": true
  }
}
```

Animal-specific example:

```json
"entity": {
  "kind": "actor",
  "actor_type": "animal",
  "rests_on_tile_center": true,
  "can_interpolate_between_tiles": true,
  "movement_domain": "land",
  "owner_model": "neutral_or_wild",
  "ai_behavior": "flee",
  "resource_on_death": "food",
  "occupancy": {
    "footprint": { "w": 1, "h": 1 },
    "tile_centered_rest_state": true
  }
}
```

Target render object for actors:

```json
"render": {
  "layer": "actor",
  "projection_mode": "upright_world",
  "projection_factor": 0.0,
  "depth_bucket": "actor",
  "anchor": "tile_center",
  "depth_sort": {
    "enabled": true,
    "key": "world_screen_y",
    "tie_breakers": ["world_screen_x", "entity_id"]
  }
}
```

### 12.1 Actor directions and mirroring

Preserve current fields, but move them under a more explicit object:

Current:

```json
"art": {
  "directions": ["front", "back"],
  "runtime_mirrors_horizontal": true
}
```

Target:

```json
"render": {
  "directions": ["front", "back"],
  "runtime_mirrors_horizontal": true
}
```

Keep a compatibility copy or mirror under `art` for one migration version if needed.

### 12.2 Team colour

Current team-colour fields should be preserved:

```text
team_color_required
team_color_slots
recommended_player_colour
player_sigil
```

Target structure:

```json
"team_color": {
  "required": true,
  "slots": ["shield face", "tabard stripe"],
  "recommended_player_colour": { "name": "blue", "hex": "#00AFFF" },
  "player_sigil": {
    "id": "player-sigil",
    "description": "white diagonal stripe running from top left to bottom right"
  }
}
```

### 12.3 Operators for siege units

Current fields:

```json
"operated_by_person": true,
"operator_contract": "Show exactly one visible human operator..."
```

Target structure:

```json
"operator": {
  "visible_operator_required": true,
  "count": 1,
  "contract": "Show exactly one visible human operator actively handling, pushing, loading, firing, bracing, or inspecting this movable machine."
}
```

Apply to:

```text
catapult
trebuchet
ram
```

Preserve current content exactly unless design changes are requested.

---

## 13. Research visual variants

The current JSON already supports research-driven visual variants on units such as archer, militia, knight, spearman, and trebuchet.

Promote these from `art.research_visual_lines` and `art.research_visual_variants` into a structured shared field.

Target:

```json
"visual_variants": {
  "research_lines": [
    {
      "id": "archer_weapon",
      "name": "archer weapon",
      "tiers": [
        {
          "id": "self_bow",
          "name": "Self Bow",
          "research": null,
          "description": "starting archer equipment"
        },
        {
          "id": "crossbow",
          "name": "Crossbows",
          "research": "Crossbows",
          "description": "upgraded archer equipment"
        }
      ]
    }
  ],
  "resolved_variants": [
    {
      "id": "self_bow",
      "name": "Self Bow",
      "research": [],
      "is_default": true
    },
    {
      "id": "crossbow",
      "name": "Crossbows",
      "research": ["Crossbows"],
      "is_default": false
    }
  ]
}
```

Action ID convention:

```text
<variant_id>__<action_id>
```

Examples:

```text
self_bow__idle
crossbow__release
basic_weapons__open_helmet__trot
iron_weapons__plate_helm__charge_strike
```

Validator requirements:

```text
Each resolved research variant has a stable slug-safe id.
Exactly one default variant exists for each mutually exclusive variant set.
Every research-gated variant lists required research by stable research name/id.
Generated action ids are deterministic.
No generated action id exceeds filesystem/path safety limits.
```

---

## 14. Actions and animation metadata

The current peasant spec has the richest action format. Treat it as the target action schema.

Target action shape:

```json
{
  "id": "walk",
  "description": "Worker walks across the map with an alternating two-step gait.",
  "frame_ms": 90,
  "loop": true,
  "hold_last": false,
  "transition_after_ms": 0,
  "family": "gait",
  "target_relation": "self_tile",
  "range_tiles": 0,
  "fit_profile": "standing",
  "phases": [
    "Walking gait with the front or near leg forward.",
    "Walking gait with the rear or far leg forward."
  ],
  "frame_durations_ms": [90, 90],
  "tool": null,
  "carry": null,
  "research_visual_variant": null,
  "source": "docs/tileset/realm_tileset_visual_audit.md"
}
```

Compatibility action shape:

```json
{
  "id": "idle",
  "description": "idle",
  "source": "docs/tileset/realm_tileset_visual_audit.md",
  "frames_recommended": 2
}
```

Migration rule:

```text
If an action has only frames_recommended, keep it valid and mark action_detail_level = basic.
If an action has frame_ms/phases/frame_durations_ms, mark action_detail_level = timed.
The validator should accept both during migration.
New or updated actions should use the richer timed schema.
```

Target metadata addition:

```json
"animation_schema": {
  "action_detail_level": "basic | timed | mixed",
  "default_frame_ms": 120,
  "default_frames_recommended": 2
}
```

---

## 15. Projectiles

`ammunition` must be renamed to `projectiles`.

Current files:

```text
art/tiles/image-json/ammunition/arrow.json
art/tiles/image-json/ammunition/catapult_boulder.json
art/tiles/image-json/ammunition/crossbow_bolt.json
art/tiles/image-json/ammunition/flaming_arrow.json
art/tiles/image-json/ammunition/tower_bolt.json
art/tiles/image-json/ammunition/trebuchet_boulder.json
art/tiles/image-json/ammunition/warship_arrow_volley.json
```

Target files:

```text
art/tiles/image-json/projectiles/arrow.json
art/tiles/image-json/projectiles/catapult_boulder.json
art/tiles/image-json/projectiles/crossbow_bolt.json
art/tiles/image-json/projectiles/flaming_arrow.json
art/tiles/image-json/projectiles/tower_bolt.json
art/tiles/image-json/projectiles/trebuchet_boulder.json
art/tiles/image-json/projectiles/warship_arrow_volley.json
```

Current schema:

```json
"schema": "realm.ammunition_sprite_spec.v1",
"asset_type": "ammunition"
```

Target schema:

```json
"schema": "realm.projectile_sprite_spec.v2",
"asset_type": "projectile"
```

Target projectile example:

```json
{
  "schema": "realm.projectile_sprite_spec.v2",
  "asset_type": "projectile",
  "id": "arrow",
  "slug": "arrow",
  "name": "Arrow",
  "render": {
    "layer": "projectile",
    "projection_mode": "upright_world",
    "projection_factor": 0.0,
    "depth_bucket": "projectile",
    "anchor": "world_position",
    "depth_sort": {
      "enabled": true,
      "key": "world_screen_y",
      "tie_breakers": ["altitude", "projectile_id"]
    }
  },
  "projectile": {
    "is_tile_content": false,
    "trajectory_type": "direct_or_arc",
    "collision_model": "point_or_small_sprite",
    "impact_effect": null,
    "source_socket": null,
    "target_socket": null
  },
  "states": ["in_flight"],
  "actions": [
    {
      "id": "in_flight",
      "description": "single arrow in flight, readable diagonal silhouette, no bow or archer",
      "frames_recommended": 1
    }
  ],
  "paths": {
    "runtime_root": "assets/tiles/projectiles/arrow",
    "manifest": "assets/tiles/projectiles/arrow/manifest.json"
  },
  "migration": {
    "from_asset_type": "ammunition",
    "from_schema": "realm.ammunition_sprite_spec.v1",
    "from_runtime_root": "assets/tiles/ammunition/arrow"
  }
}
```

### 15.1 References from units and buildings

Current references:

```json
"art": {
  "ammunition": ["arrow", "crossbow_bolt"]
}
```

Target references:

```json
"combat": {
  "projectiles": ["arrow", "crossbow_bolt"]
}
```

Migration rule:

```text
art.ammunition -> combat.projectiles
```

Compatibility:

```text
For one migration version, readers must accept art.ammunition.
Exporters must write combat.projectiles.
Validator should warn, not fail, on art.ammunition until compatibility ends.
```

Affected assets include:

```text
archer -> arrow, crossbow_bolt
catapult -> catapult_boulder
trebuchet -> trebuchet_boulder
warship -> warship_arrow_volley
tower -> tower_bolt
castle -> tower_bolt, trebuchet_boulder
```

Confirm exact references from current JSON before migration.

---

## 16. Effects

Effects are transient visual/gameplay feedback. They are not permanent tile layers.

Examples to add:

```text
impact_dust
arrow_impact
bolt_impact
boulder_impact
fire_hit
water_splash
smoke_puff
construction_dust
healing_aura
income_sparkle
selection_flash
```

Target effect spec:

```json
{
  "schema": "realm.effect_spec.v1",
  "asset_type": "effect",
  "id": "impact_dust",
  "slug": "impact_dust",
  "name": "Impact Dust",
  "render": {
    "layer": "effect",
    "projection_mode": "upright_world",
    "projection_factor": 0.0,
    "depth_bucket": "effect",
    "anchor": "world_position",
    "depth_sort": { "enabled": true, "key": "world_screen_y", "tie_breakers": ["effect_id"] }
  },
  "lifetime": {
    "duration_ms": 300,
    "loop": false,
    "remove_on_complete": true
  },
  "actions": [
    { "id": "play", "frames_recommended": 4 }
  ],
  "paths": {
    "runtime_root": "assets/tiles/effects/impact_dust",
    "manifest": "assets/tiles/effects/impact_dust/manifest.json"
  }
}
```

Effect projection can be:

```text
surface_projected    scorch appearing on ground, splash ring on water
upright_world        dust puff, smoke plume, impact flash
screen_space         UI flash, screen overlay
```

Do not store effects as tile decals unless they become persistent surface marks. A persistent scorch mark or blood stain should be a decal, not an effect.

---

## 17. User interface assets

UI assets are separate from tile composition.

Examples to add:

```text
selection_bracket
health_bar
movement_arrow
attack_preview
ownership_marker
garrison_indicator
command_cursor
fog_overlay
resource_icon_food
resource_icon_wood
resource_icon_gold
```

Target UI spec:

```json
{
  "schema": "realm.ui_asset_spec.v1",
  "asset_type": "user_interface",
  "id": "selection_bracket",
  "slug": "selection_bracket",
  "name": "Selection Bracket",
  "render": {
    "layer": "ui",
    "projection_mode": "screen_space",
    "projection_factor": 0.0,
    "depth_bucket": "ui",
    "anchor": "screen_position",
    "depth_sort": { "enabled": false, "key": null, "tie_breakers": [] }
  },
  "ui": {
    "usage": "selection",
    "scales_with_zoom": true,
    "attached_to_world_entity": true
  },
  "paths": {
    "runtime_root": "assets/tiles/ui/selection_bracket",
    "manifest": "assets/tiles/ui/selection_bracket/manifest.json"
  }
}
```

Fog of war may be world-overlay UI or surface overlay depending implementation. It should not be stored as a ground, feature, or decal asset.

---

## 18. Seasons and weather

The current JSON already contains seasonal/weather concepts in prose. Move these into structured fields.

### 18.1 Season object

```json
"seasonal": {
  "supports_seasons": true,
  "default_season": "summer",
  "variants": {
    "spring": {
      "variant_group": "spring",
      "overlay": null,
      "notes": "fresh growth"
    },
    "summer": {
      "variant_group": "summer",
      "overlay": null,
      "notes": "normal or dry summer variant"
    },
    "autumn": {
      "variant_group": "autumn",
      "overlay": null,
      "notes": "dull/dead/autumn colour variant"
    },
    "winter": {
      "variant_group": "winter",
      "overlay": "snow",
      "notes": "snowed or winter-dead variant"
    }
  }
}
```

### 18.2 Weather object

```json
"weather": {
  "supports_weather": true,
  "rules": [
    {
      "trigger": "rain",
      "mode": "overlay",
      "overlay": "wet_sheen"
    },
    {
      "trigger": "winter_freeze",
      "mode": "replace_ground",
      "from": ["water", "shallows", "marsh"],
      "to": "ice",
      "gameplay_changes": {
        "land": "passable",
        "boat": "blocked"
      }
    }
  ]
}
```

### 18.3 Snow distinction

Implement these as distinct concepts:

| Concept | Representation |
|---|---|
| Snow biome / permanent winter terrain | `ground: snow` |
| Temporary snow cover | weather overlay or decal |
| Falling snow | effect/global weather effect |
| Snow on trees/mountains/crops | seasonal variant or overlay |
| Ice over water | ground replacement / weather transform |

### 18.4 Initial extraction list

Extract structured season/weather fields from current prose:

| Asset | Current prose concept | Target structure |
|---|---|---|
| `grass` | spring/summer/autumn/winter compatibility; wears to dirt/road | `seasonal`, `weather`, `wear` |
| `meadow` | seasonal overlays; winter snowed | `seasonal` |
| `hills` | snow overlay | `seasonal.winter` |
| `water` | shoreline masks; freezes to ice | `variants.shoreline`, `weather.freeze` |
| `shallows` | shoreline blends; freezes | `variants.shoreline`, `weather.freeze` |
| `marsh` | rain movement; winter freeze possible through ice system | `weather.rain`, `weather.freeze` |
| `mud` | created by rain/storm; dries to dirt | `weather.create`, `weather.recovery` |
| `snow` | winter-converted terrain / tundra base | `seasonal`, `biome` |
| `ice` | replacement for water/shallows/marsh/reeds | `weather.replacement_source` |
| `flowers` | spring bloom; autumn dead-stalk; winter snowed | `seasonal` |
| `tall_grass` | seasonal colour overlays; winter snowed | `seasonal` |
| `forest` | autumn leaves; winter branch snow | `seasonal` |
| `pine` | snow-cap overlay; autumn yellowing | `seasonal` |
| `wheat` | growth, summer ripe, autumn spent, winter snow/dead | `seasonal`, `growth` |
| `farm` | winter-dead/snowed | `seasonal`, `states` |
| `wolf` | winter aggressive variant optional | `seasonal`, `ai_behavior` |

Do not delete legacy prose during first migration. Move it to `art.legacy_*` fields.

---

## 19. Shorelines, roads, bridges, and mixed terrain

### 19.1 Shorelines

Do not create mixed land/water base ground tiles.

Wrong model:

```text
one tile = half grass, half water
```

Correct model:

```text
one tile = grass
neighbouring tile = water
shoreline = edge variant, decal, or mask
```

Preferred implementation:

```text
Water tile checks adjacent land tiles.
Water tile chooses shore-edge variant/decal for land-facing sides.
Land tile remains readable and playable as land.
```

Alternative allowed:

```text
Land tile checks adjacent water tiles.
Land tile chooses beach/shore edge variant/decal.
```

The important rule is that base terrain remains singular.

### 19.2 Roads

Roads must migrate from ground to decal.

Map migration:

```text
old tile: T_ROAD
new tile: ground = inferred underlying ground, decals = [road]
```

If underlying ground is unknown:

```text
default ground = dirt
decals = [road]
```

If biome/old map context is known:

```text
grass road  -> ground = grass, decals = [road]
sand road   -> ground = sand, decals = [road]
snow road   -> ground = snow, decals = [road]
ruin road   -> ground = castle_floor, decals = [road]
```

Compatibility:

```text
Keep T_ROAD as a legacy load alias.
Do not emit T_ROAD as a v2 ground in new maps/spec output.
In v2 index, road belongs to decals.
```

### 19.3 Bridges

Default bridge model:

```text
ground = water
bridge = decal or semi-flat feature
movement rule = land units can cross
```

Bridge classification:

| Bridge behaviour | Category |
|---|---|
| visual/passability only | decal or feature |
| can be owned, built, damaged, selected, repaired, captured | building |
| temporary crossing effect | effect + gameplay modifier |

Do not introduce bridge as a multi-tile terrain blend.

---

## 20. Index v2

Current index schema:

```json
{
  "schema": "realm.tile_specs_index.v1",
  "generated_by": "scripts/export_tile_specs.py",
  "groups": {
    "grounds": [],
    "features": [],
    "decals": [],
    "units": [],
    "animals": [],
    "buildings": [],
    "ammunition": []
  }
}
```

Target index schema:

```json
{
  "schema": "realm.tile_specs_index.v2",
  "generated_by": "scripts/export_tile_specs.py",
  "schema_version": 2,
  "compatibility": {
    "reads_v1": true,
    "writes_v2": true,
    "legacy_groups": {
      "ammunition": "projectiles"
    }
  },
  "groups": {
    "grounds": [],
    "features": [],
    "decals": [],
    "units": [],
    "animals": [],
    "buildings": [],
    "projectiles": [],
    "effects": [],
    "user_interface": []
  }
}
```

Index group rules:

```text
Each asset appears in exactly one group.
The group must match asset_type, except units/animals both use render.layer actor.
No v2 index may contain ammunition as a primary group.
A compatibility alias may map ammunition -> projectiles for old readers.
Road must appear under decals, not grounds.
Effects and user_interface groups may start empty, but the groups must exist.
```

---

## 21. Migration plan

## Phase 0 — Safety and inventory

Create a migration audit before changing data.

Deliverable:

```text
docs/asset_migration/asset_json_v1_inventory.md
```

Audit must include:

```text
count by asset_type
count by schema
all slugs
all enums
all runtime paths
all references to ammunition
all art.projection strings
all art.required_variants strings containing season/weather/growth/autotile language
all art.runtime_notes strings containing gameplay/passability/resource language
all footprints greater than 1x1
all action schemas by field set
all current source files
```

Expected current counts:

```text
ground: 17
feature: 14
decal: 2
unit: 11
animal: 4
building: 16
ammunition: 7
```

Do not delete or rename source files in this phase.

## Phase 1 — Add schemas and validator

Add schema files:

```text
schemas/realm.tile_specs_index.v2.json
schemas/realm.ground_spec.v2.json
schemas/realm.decal_spec.v2.json
schemas/realm.feature_spec.v2.json
schemas/realm.building_spec.v2.json
schemas/realm.actor_sprite_spec.v2.json
schemas/realm.projectile_sprite_spec.v2.json
schemas/realm.effect_spec.v1.json
schemas/realm.ui_asset_spec.v1.json
```

Add validator:

```text
scripts/validate_tile_specs.py
```

Validator modes:

```text
--mode compatibility  accepts v1 and v2, warns on v1-only fields
--mode strict-v2      requires v2 fields and rejects v1-only output
```

Initial CI should run compatibility mode. Switch to strict-v2 only after migration is complete.

## Phase 2 — Add `render` object everywhere

Add structured `render` fields to every asset.

Default migration mapping:

| Current asset type | Target render |
|---|---|
| `ground` | `layer=ground`, `projection_mode=surface_projected`, `projection_factor=1.0`, `depth_bucket=surface`, `anchor=tile` |
| `decal` | `layer=decal`, `projection_mode=surface_decal` or `semi_upright_decal`, `depth_bucket=surface_overlay`, `anchor=tile` |
| `feature` | `layer=feature`, `projection_mode=upright_world`, `projection_factor=0.0`, `depth_bucket=standing_split` or `standing_back/front`, `anchor=tile_center` |
| `building` | `layer=building`, `projection_mode=upright_world`, `projection_factor=0.0`, `depth_bucket=building`, `anchor=footprint_origin` |
| `unit` | `layer=actor`, `projection_mode=upright_world`, `projection_factor=0.0`, `depth_bucket=actor`, `anchor=tile_center` |
| `animal` | `layer=actor`, `projection_mode=upright_world`, `projection_factor=0.0`, `depth_bucket=actor`, `anchor=tile_center` |
| `ammunition` | initially `layer=projectile`, `projection_mode=upright_world`, then rename asset type |

Keep old `art.projection` as `art.legacy_projection`.

## Phase 3 — Rename ammunition to projectiles

Steps:

```text
1. Create projectiles folder.
2. Move ammunition JSON files into projectiles folder or duplicate then remove after compatibility.
3. Change schema to realm.projectile_sprite_spec.v2.
4. Change asset_type to projectile.
5. Change paths.runtime_root from assets/tiles/ammunition/<slug> to assets/tiles/projectiles/<slug>.
6. Change index group from ammunition to projectiles.
7. Change references art.ammunition -> combat.projectiles.
8. Add loader alias ammunition -> projectiles.
9. Add tests proving old ammunition references still resolve during transition.
10. Remove alias only when all old maps/specs/prompts are migrated.
```

## Phase 4 — Road migration

Steps:

```text
1. Create decals/road.json.
2. Move road from grounds group to decals group in v2 index.
3. Set asset_type=decal.
4. Set render.projection_mode=surface_decal and projection_factor=1.0.
5. Add placement.autotile metadata.
6. Add movement modifier metadata.
7. Add migration alias from T_ROAD.
8. Update map loader: T_ROAD -> inferred ground + road decal.
9. Update map exporter: never emit T_ROAD as ground in v2.
10. Keep old road ground only as deprecated compatibility input.
```

Migration field:

```json
"migration": {
  "deprecated_as_ground": true,
  "from_asset_type": "ground",
  "from_enum": "T_ROAD",
  "target_asset_type": "decal",
  "target_enum": "D_ROAD",
  "fallback_ground_when_unknown": "dirt"
}
```

If changing C++ enums is too expensive immediately, allow:

```json
"enum": "T_ROAD",
"asset_type": "decal",
"id": "road"
```

In that case, `asset_type` wins over enum prefix. Add `target_enum: "D_ROAD"` for future cleanup.

## Phase 5 — Structure gameplay fields

Extract gameplay from `art.runtime_notes` and existing runtime code into `gameplay`.

Add fields for:

```text
interactable
selectable
passability
movement_cost
movement_cost_modifier
resource
depletion_result
visibility_modifier
concealment
ownership
construction
garrison
trains_units
```

Use existing engine constants where available. If not available, mark as:

```json
"needs_design_review": true
```

Do not invent final balance values without code/design confirmation.

## Phase 6 — Structure seasons/weather

Extract season/weather information from prose into:

```text
seasonal
weather
variants
```

Keep old text as:

```text
art.legacy_required_variants
art.legacy_runtime_notes
```

Required transforms:

```text
water/shallows/marsh/reeds -> ice in winter/freeze conditions
rain/storm -> mud creation where supported
mud -> dirt when dry
snow as ground vs overlay vs effect distinction
seasonal tree/crop/flower variants
```

## Phase 7 — Feature layers and occlusion

Add to features:

```text
feature_layers
occlusion
```

Implement renderer logic:

```text
feature.back before actor on same tile
actor
feature.front after actor on same tile
```

Acceptance test:

```text
Given a unit on a forest tile,
When rendered,
Then rear trees/trunks are behind the unit,
And foreground bushes/branches are in front of the unit.
```

## Phase 8 — Building placement/render anchors

Add to buildings:

```text
placement.footprint
placement.origin
render.anchor
render.draw_from_origin_only
render.visual_overhang_allowed
```

Keep `runtime.footprint` as compatibility copy for one migration version.

Special reviews:

```text
castle 4x4
town_hall 3x3
barracks/stable 3x2
dock 2x2 water-touch placement
wall/gate connectivity
farm 1x1 building despite surface-like art
```

## Phase 9 — Actors and action schema

Add to units/animals:

```text
entity.kind = actor
entity.actor_type = unit | animal
entity.rests_on_tile_center = true
entity.can_interpolate_between_tiles = true
```

Move or mirror:

```text
art.directions -> render.directions
art.runtime_mirrors_horizontal -> render.runtime_mirrors_horizontal
art.team_color_* -> team_color
art.operated_by_person/operator_contract -> operator
art.research_visual_* -> visual_variants
```

Do not remove old fields until exporters and consumers are updated.

## Phase 10 — Effects and UI

Add empty groups to index immediately:

```text
effects
user_interface
```

Then add minimum specs.

Recommended first effects:

```text
impact_dust
arrow_impact
boulder_impact
water_splash
construction_dust
healing_aura
income_sparkle
```

Recommended first UI:

```text
selection_bracket
health_bar
movement_arrow
attack_preview
ownership_marker
garrison_indicator
command_cursor
fog_overlay
```

## Phase 11 — Exporters and prompts

Update:

```text
scripts/export_tile_specs.py
scripts/export_image_generation_prompts.py
```

Exporter rules:

```text
Read engine/source constants.
Emit v2 schema.
Emit render.* for all assets.
Emit gameplay.* where source data exists.
Emit seasonal/weather from extracted rules.
Emit projectiles, not ammunition.
Emit road as decal.
Emit effects and user_interface groups.
Keep sources[] provenance.
Do not emit v1-only fields except under legacy/migration keys.
```

Image-generation prompt rules:

```text
Use render.projection_mode, not art.projection, for projection wording.
Use asset_type/render.layer for category-specific prompt constraints.
Use team_color/operator/visual_variants/action metadata where available.
Use projectile terminology.
```

## Phase 12 — Renderer and loader

Renderer must consume structured fields:

```text
render.layer
render.projection_mode
render.projection_factor
render.depth_bucket
render.anchor
feature_layers
placement.footprint
```

Map loader must support:

```text
ground + decals + feature + building reference + actors
legacy T_ROAD conversion
legacy ammunition/projectile aliases
```

Pseudo render pipeline:

```cpp
// Surface pass
for (TileCoord coord : map.draw_order_surface()) {
    const Tile& tile = map.tile(coord);
    draw_ground(tile.ground);
    draw_surface_decals(tile.decals.where(depth_bucket == surface_overlay));
    draw_weather_surface_overlays(tile.weather_overlay);
}

// World pass
std::vector<Renderable> renderables = collect_depth_sorted_world_renderables(map, entities);
std::stable_sort(renderables.begin(), renderables.end(), compare_depth_key);

for (const Renderable& r : renderables) {
    draw_renderable(r);
}

// Same-tile split feature handling should ensure:
// feature.back -> actors on tile -> feature.front

// UI pass
draw_world_ui_overlays();
draw_screen_ui();
```

---

## 22. Validator rules

Validator must check common rules:

```text
asset_type matches containing folder/group
schema matches asset_type
slug matches filename
paths are present
sources are present
render exists
render.layer is valid
render.projection_mode is valid
render.depth_bucket is valid
legacy prose fields are not used as authoritative renderer/gameplay input
```

Category-specific rules:

```text
ground:
  projection_mode must be surface_projected
  projection_factor must be 1.0
  must not be interactable
  road must not appear as v2 ground except deprecated alias

decal:
  projection_mode must be surface_decal or semi_upright_decal unless explicitly exceptional
  interactable should normally be false
  if interactable true, interaction_exception_reason required

feature:
  placement.footprint must be 1x1
  if feature_layers exists, main layer required
  resource features must specify resource.type and depletion_result or needs_design_review

building:
  placement.footprint required
  runtime.footprint, if present, must match placement.footprint during compatibility
  render.anchor must be footprint_origin or explicit override
  multi-tile buildings must specify origin

unit/animal:
  entity.kind must be actor
  render.layer must be actor
  projection_mode must be upright_world
  placement/entity footprint must be 1x1 unless explicitly supported

projectile:
  asset_type must be projectile
  schema must be projectile schema
  is_tile_content must be false
  render.anchor must be world_position
  old ammunition path is compatibility-only

effect:
  lifetime required unless marked persistent

user_interface:
  projection_mode should normally be screen_space
```

Reference rules:

```text
Every combat.projectiles entry must resolve to a projectile slug.
Every allowed_grounds entry must resolve to a ground slug.
Every decal stacking reference must resolve to a decal slug.
Every resource depletion result must resolve or be null.
Every state/action reference must be slug-safe.
Every research visual variant id must be unique within its asset.
```

Migration warnings:

```text
art.ammunition present -> warn, suggest combat.projectiles
asset_type ammunition -> warn/error depending mode
schema realm.ammunition_sprite_spec.v1 -> warn/error depending mode
art.projection without render.projection_mode -> warn/error depending mode
road in grounds group -> warn/error depending mode
```

---

## 23. Conflict resolution rules

Use these rules whenever current JSON and target design disagree.

### Rule 1 — Preserve inventory first

Do not remove assets during migration. Rename/reclassify with aliases.

Example:

```text
ammunition assets become projectiles, but the same slugs remain.
```

### Rule 2 — Target semantics win over current category labels

If an asset is in the wrong category, migrate it.

Example:

```text
road is currently a ground but becomes a decal.
```

### Rule 3 — Structured fields become authoritative

Free-text fields remain notes only.

```text
render reads render.*
gameplay reads gameplay.*
season/weather reads seasonal.* and weather.*
```

### Rule 4 — Features are one-tile unless reclassified

If an object needs a multi-tile footprint, it is not a normal feature.

Possible classifications:

```text
building
building ruin instance
map object
coordinated one-tile feature group
```

### Rule 5 — Interactable things should usually stand up

Default:

```text
decal = not selectable/interactable
feature/building/unit/animal = may be interactable
```

Exception format:

```json
"gameplay": {
  "interactable": true,
  "interaction_exception_reason": "..."
}
```

### Rule 6 — Animals stay separate but share actor implementation

Do not merge animal content into units.

```text
asset_type = animal
entity.kind = actor
entity.actor_type = animal
render.layer = actor
```

### Rule 7 — Farm remains building

Farm is a building because it is owned, interactable, stateful, and managed.

### Rule 8 — Enum prefixes do not decide category during migration

Some current terrain/decal/feature enums use `T_*`. During migration:

```text
asset_type controls category
enum is a runtime compatibility identifier
```

Example allowed temporarily:

```json
{
  "asset_type": "decal",
  "enum": "T_ROAD",
  "target_enum": "D_ROAD"
}
```

### Rule 9 — Old readers may load aliases, but exporters write target names

```text
Readers: accept v1/v2 during transition.
Exporters: write v2 only.
```

---

## 24. Required deliverables

Implementation should produce:

```text
docs/asset_json_v2_spec.md
docs/asset_migration/asset_json_v1_inventory.md
docs/asset_migration/asset_json_v1_to_v2_plan.md

schemas/realm.tile_specs_index.v2.json
schemas/realm.ground_spec.v2.json
schemas/realm.decal_spec.v2.json
schemas/realm.feature_spec.v2.json
schemas/realm.building_spec.v2.json
schemas/realm.actor_sprite_spec.v2.json
schemas/realm.projectile_sprite_spec.v2.json
schemas/realm.effect_spec.v1.json
schemas/realm.ui_asset_spec.v1.json

scripts/migrate_tile_specs_v1_to_v2.py
scripts/validate_tile_specs.py

updated scripts/export_tile_specs.py
updated scripts/export_image_generation_prompts.py
```

Optional but useful:

```text
docs/rendering/tile_render_stack.md
docs/rendering/decal_projection.md
docs/rendering/feature_split_layers.md
docs/gameplay/terrain_passability_matrix.md
```

---

## 25. Acceptance criteria

Minimum acceptance criteria:

```text
[ ] Existing v1 JSON can be loaded without data loss.
[ ] v2 JSON validates against schemas.
[ ] index.json uses schema realm.tile_specs_index.v2.
[ ] index groups include grounds, features, decals, units, animals, buildings, projectiles, effects, user_interface.
[ ] ammunition group is no longer emitted as a primary v2 group.
[ ] projectile specs replace ammunition specs.
[ ] legacy ammunition references still resolve during transition.
[ ] art.ammunition is migrated to combat.projectiles.
[ ] road exists as a decal in v2.
[ ] road is not emitted as a v2 ground.
[ ] legacy T_ROAD maps convert to ground + road decal.
[ ] all assets have render.layer, render.projection_mode, render.projection_factor, render.depth_bucket, and render.anchor.
[ ] grounds use surface_projected projection.
[ ] units, animals, buildings, features, and projectiles use upright_world unless explicitly exceptional.
[ ] decals have explicit projection behaviour.
[ ] flowers/tall_grass can use semi_upright_decal.
[ ] grounds/features/decals have structured seasonal/weather data where current prose mentions it.
[ ] feature resources specify resource/depletion or needs_design_review.
[ ] forest supports feature back/front split or has fields ready for it.
[ ] renderer can draw actor inside forest between back/front layers.
[ ] buildings have placement.footprint and render anchor.
[ ] multi-tile buildings draw from a defined origin/anchor.
[ ] animals remain asset_type animal but use entity.kind actor.
[ ] projectiles are transient world objects, not tile contents.
[ ] effects and UI are separate from tile composition.
[ ] validation can run in compatibility and strict-v2 modes.
[ ] CI includes validation.
```

Visual smoke tests:

```text
[ ] Unit on normal grass renders above ground/decals.
[ ] Unit on forest renders between forest back/front layers.
[ ] Road on grass renders as surface decal, not ground replacement.
[ ] Road on sand/snow/dirt can reuse same road decal logic.
[ ] Multi-tile barracks/castle/town_hall renders from one anchor and occupies full footprint.
[ ] Tower remains 1x1 footprint even if sprite overhangs.
[ ] Arrow/bolt/boulder projectile renders as transient world object.
[ ] Selection brackets and health bars render as UI overlays.
```

---

## 26. Recommended implementation order

Use this order to reduce breakage:

```text
1. Add inventory report.
2. Add v2 schemas and validator in compatibility mode.
3. Add render fields to all generated JSON while preserving old fields.
4. Update renderer to prefer render.* when present.
5. Rename ammunition to projectiles with loader aliases.
6. Migrate art.ammunition to combat.projectiles.
7. Move road from ground to decal with map compatibility.
8. Add structured gameplay fields for grounds/decals/features.
9. Add structured seasonal/weather fields.
10. Add feature split layers and forest render test.
11. Add building placement footprints and anchors.
12. Add actor/entity structure for units and animals.
13. Normalize research variants and action metadata.
14. Add effects and user_interface groups/specs.
15. Switch exporter to write only v2.
16. Switch validator/CI to strict-v2.
17. Remove v1 compatibility aliases only after all consumers are migrated.
```

Highest-risk changes:

```text
road ground -> decal map migration
forest split rendering / depth ordering
ammunition -> projectiles references in combat/rendering code
multi-tile building anchor/render assumptions
```

Lowest-risk changes:

```text
adding render fields
adding migration metadata
adding structured seasonal/weather fields while keeping prose
adding validation in warning mode
adding empty effects/user_interface index groups
```

---

## 27. Final target principle

The final implementation should make this true:

```text
The JSON describes what an asset is, how it renders, how it participates in gameplay,
and how it migrates from the old vocabulary, without requiring the renderer or gameplay
systems to infer category behaviour from prose strings or folder names.
```

The strongest design rule remains:

```text
Everything must remain tile-readable.
```

Grounds and flat decals create the board. Features, buildings, units, animals, and major interactable objects stand on the board. Projectiles and effects are transient world objects. UI is separate. The player should be able to tell what is surface, what is object, what is actor, and what can be interacted with.
