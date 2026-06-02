# Realm Animation and State Mechanics Audit

This audit compares the current game mechanics against the generated image prompts and animation/state design. It focuses on mechanics that need distinct art states, animation states, or effect overlays.

Lighting is intentionally excluded as a generated-art requirement. Dawn, dusk, night, storm darkness, and fog dimming should be renderer colour/effect work unless a mechanic adds a visible object, such as building torches.

Canonical visual layer architecture and terminology: `docs/tileset/realm_visual_asset_architecture.md`.

Sprites are optional overrides during the migration. The current renderer must continue to use procedural ground plus emoji/symbol/ASCII fallbacks for missing assets; peasant idle is the only production sprite lane that should be assumed to exist right now.

## Sources Checked

- `include/realm.h`: terrain, entity, season, weather, cargo, research, entity state, tile, entity, projectile, and marker fields.
- `src/entity.cpp`: movement, combat, gathering, production, research, construction, garrison, death, corpses, farms, weather, seasons, paving, animals, win condition.
- `src/orders.cpp`: move, attack, gather, build, train, formation move, attack-move, action markers.
- `src/input.cpp`: player commands, build/train/research/rally/attack-move/gate commands.
- `src/render.cpp` and `src/gfx_renderer.cpp`: current visual proxies, overlays, projectiles, health bars, selection, lab state controls.
- `src/entity_animation.cpp`: current code-derived entity action specs.
- `docs/tileset/realm_tileset_visual_audit.md` and `art/tiles/image-spec`.

## Executive Summary

The main missing mechanics are not basic unit idle/walk/death states. The gaps are in cross-cutting systems:

- No generated prompt category exists for `effects_ui`: arrows, bolts, boulders, catapult impact/splash, weather particles, command markers, range rings, selection, rally markers, and build previews.
- Building construction is usually collapsed to one `construction` state even though the design calls for staged construction and the runtime uses HP progress.
- Damaged building states are inconsistent. Some buildings have `damaged`; many can lose HP but do not have a damaged visual state in the generated prompt.
- Town Hall and Castle can produce peasants in game paths, but their generated states do not consistently include a training/production state.
- Transport garrison/cargo mechanics are under-specified: the prompt has load/unload and cargo full, but not empty/partial/full cargo or loaded/unloaded states.
- Research upgrades are only partly covered: Archer has an optional crossbow variant, but Iron Weapons has no visible upgraded militia/knight weapon state. The target design requires visible upgraded variants or overlays.
- Snow-biome `T_SNOW` has a special seasonal thaw/green cycle in the renderer, but the generated snow prompt mostly covers winter snow and thaw, not the full snow-biome summer/autumn look.
- Building settlement haloes/path creep exist as a mechanic, but there is no generated overlay set for dirt/cobble/crates/paths around completed buildings.
- Animal carcass depletion states are in the prompt design, but the current runtime does not yet have partial carcass harvest state; peasant animal kills currently grant food immediately. Wolf carcass depletion states are visual-only and must not enable wolf harvesting in gameplay.

## Current Coverage That Looks Good

These mechanics are adequately represented or should stay as renderer/UI effects:

| Mechanic | Coverage | Notes |
|---|---|---|
| Peasant economy actions | Covered | Code-derived peasant actions include chop wood, mine gold, gather berries, hoe soil, gather wheat, build, carry wood/gold/berries/wheat/meat, gather meat, club attack, death. |
| Unit directions | Covered | Front/back source directions plus runtime horizontal mirroring match the current direction contract. |
| Unit death and decay | Covered for prompts and runtime concept | Units/animals/siege/naval have dead/decayed or carcass states in prompt generation; runtime uses `deathTicks`. |
| Terrain seasons/weather/depletion | Mostly covered | The generated terrain prompts now include season/weather/depletion states where material identity changes. |
| Resource depletion for terrain | Covered in prompts | Berries/wheat/forest/pine use 4 seasons x 4 depletion. Gold/palm/dead tree/fish use depletion states. |
| Rain/storm material reactions | Mostly covered | Mud, road, dirt, water, shallows, marsh, reeds, lava steam, and completed buildings get two-frame reaction states. |
| Gate open/closed | Covered | Generated gate states include open/closed and locked variants. Runtime passability uses open/closed; locked is mostly a UI/command mode. |
| Garrisoned buildings | Partly covered | Town Hall, House, Tower, Castle have garrison states in docs/prompts. |
| Church healing | Covered as building/effect idea | Church prompt includes healing aura; this can be a building state or overlay. |
| Market income | Covered as building/effect idea | Market prompt includes income sparkle. |
| Fog, explored tiles, night/storm concealment | Renderer-only | Do not generate separate unit sprite states for this. Terrain-feature concealment needs feature occluder layers. |
| Health bars, selection, group selection, range rings | Overlay/UI | Should be effects/UI assets, not entity sprite states. |
| Training/research progress bars and queues | UI | The state can exist on the building, but progress percentage should be UI. |

## Missing Or Weak Mechanics

### 1. Effects and Tactical Overlay Sheet

No generated Markdown prompt currently covers the effect/UI assets, even though the game uses them directly.

Required effect states:

- Arrow projectile: archer attack.
- Tower/garrison bolt: tower and garrisoned building fire.
- Catapult boulder projectile.
- Catapult impact/splash: 1-tile radius impact effect; currently documented, but not prompt-exported.
- Combat alert marker: recent combat pulse.
- Action markers: move `x`, attack `!`, gather `+`, build `#`.
- Weather particles: rain, storm rain, snow.
- Selection marker, group selection marker, drag selection box, range-ring dot.
- Rally marker and build/wall preview marker.

Recommendation: add a generated `effects-ui` prompt group. Keep these as overlays; do not duplicate them in every unit/building prompt.

### 2. Building Construction Stages

Runtime construction uses HP progress and the terminal renderer pulses a scaffold glyph. The visual design says construction should have stages, but generated building prompts often contain just `construction`.

Recommended building construction states:

- `construction_0`: foundation/stakes/scaffold.
- `construction_1`: half-built walls/frame.
- `construction_2`: roof/top/detail nearly complete.
- `complete`.

Applies to all buildable buildings and walls/gates/docks. For 1x1 wall/gate, fewer states may be acceptable, but at least `construction` plus `complete` should remain explicit.

### 3. Damaged Building States

The game has HP for every building and combat can damage all buildings. Current prompts do not consistently include a damaged state for every building.

Missing or weak damaged coverage:

- House: no explicit damaged state.
- Barracks: no explicit damaged state.
- Stable: no explicit damaged state.
- Farm: has depleted/dead and winter-dead, but not damage-specific.
- Blacksmith: no explicit damaged state.
- Church: no explicit damaged state.
- Market: no explicit damaged state.
- Wall/Gate: structural join states exist, but damaged/broken join variants are not explicit.
- Lumber Camp, Mining Camp, Mill, Dock: no explicit damaged state.

Recommendation: generate `damaged` for every building. For walls/gates, add `damaged_straight`, `damaged_corner`, or a generic cracked overlay if full damaged autotiles are too expensive.

### 4. Large Building Ruins Versus Dead Building Sprites

When a large building dies, runtime converts its footprint to `T_RUINS`; dead buildings are pruned quickly. This means large-building destruction should be represented as terrain/footprint art, not a long-lived dead building sprite.

Coverage is partial: many building prompts have `ruin_footprint`, but the terrain prompt for `T_RUINS` is still generic ruins rather than per-building footprints.

Recommendation:

- Keep `ruin_footprint` in building prompts for area >= 4 buildings.
- Add a terrain/building-footprint prompt pass for large ruin footprints: Town Hall 3x3, House 2x2, Barracks 3x2, Stable 3x2, Blacksmith 2x2, Church 2x2, Market 2x2, Castle 4x4, Lumber Camp 2x2, Mining Camp 2x2, Mill 2x2, Dock 2x2.
- Do not create persistent dead-building animation states unless the runtime changes.

### 5. Production Buildings

Training exists for Town Hall, Barracks, Stable, Dock, and in AI paths Castle can act as a peasant-producing base. Current prompt coverage is uneven.

Missing or weak production states:

- Town Hall: should include `training` or `training_peasant`.
- Castle: should include `training_peasant` if AI/current mechanics continue using it as a peasant base.
- Barracks/Stable/Dock: already have training states.
- Queue length and exact progress should remain UI, not sprite states.

Recommendation: add a generic `training` completed-building state to all buildings that can produce units in any code path.

### 6. Blacksmith Research and Unit Upgrade Visuals

Research mechanics:

- `R_IRON_WEAPONS`: militia/knights gain attack.
- `R_CROSSBOWS`: archers gain range.
- Blacksmith `researching` is already present as a building state.

Missing/weak art decisions:

- Archer has an optional upgraded crossbow variant, but it is one slot rather than a consistent upgraded action set.
- Militia and Knight have no `iron_weapons` or upgraded weapon/armour variant.
- Completed research icons are UI, but if upgrades are meant to be visible, units need upgraded variants.

Decision: tech upgrades should be visible in-world. The first implementation can use equipment overlays, but the asset model must support full upgraded action variants where needed.

- Archer: add `crossbow_idle`, `crossbow_walk`, `crossbow_aim`, `crossbow_release`, `crossbow_reload`, or define the crossbow as a hybrid equipment overlay that can be promoted to full action variants.
- Militia/Knight: add subtle `iron_weapons` variants or overlays for idle, movement, and attack readability.
- Prompt/export metadata should label research variants as `overlay_only`, `full_action_variant`, or `hybrid`.

### 7. Transport Cargo and Garrison Amount

Transport can garrison up to 4 units. It is hidden/loaded state, not just movement.

Current prompt has `load/unload` and `cargo full indicator`, but the game state has a count and units disappear into the transport.

Recommendation:

- Add `empty`, `loaded_partial`, `loaded_full`, `load_unload` states for Transport.
- Keep individual passenger identities out of the sprite; this should be a visible cargo/cover/flag/weight cue.

### 8. Garrison Firing States

Towers always fire. Town Hall, House, and Castle fire only when garrisoned. Transport holds units but does not fire.

Current coverage:

- Tower has `firing` and `garrisoned`.
- Town Hall, House, Castle have `garrisoned`, but not a clear `garrison_firing` state.

Recommendation:

- Add `garrison_firing` for Town Hall, House, and Castle, or make the tower/garrison bolt plus alert marker the only visible firing indicator.
- If using only projectiles, document that building bodies do not change while firing.

### 9. Settlement Halo and Path Wear

Completed buildings create ground wear around them; repeated movement compacts natural ground to dirt then road; roads decay back to dirt; dirt can regrow grass.

Terrain prompts cover dirt/road/grass states, but not the building halo layer itself.

Recommendation:

- Add overlay assets for settlement halo: dirt scuffs, compacted paths, cobble patches, crates/barrels/log piles, farm tracks.
- Treat haloes as mergeable overlays around completed buildings rather than baked into every building sprite.

### 10. Snow Biome Seasonal Cycle

The renderer gives native `T_SNOW` in `B_SNOW` a special seasonal cycle: summer becomes mostly bare/dry grass, spring thaws, autumn refreezes, winter stays full snow.

Current snow prompt focuses on winter snow, snowfall, packed/deep snow, slush, and dirty edges.

Recommendation: add snow-biome-specific states:

- `tundra_spring_thaw`
- `tundra_summer_bare_ground`
- `tundra_autumn_refreeze`
- `tundra_winter_snow`

This is distinct from generic `T_SNOW` as a winter replacement terrain.

### 11. Animal Carcass Depletion Runtime Gap

Prompts now include animal carcass depletion states:

- dead unharvested
- partly harvested
- mostly harvested
- depleted skeleton

The runtime does not currently track partial animal-carcass harvesting. Peasant-killed food animals grant food immediately and then the animal is dead. Wolves are hostile wildlife and not a normal harvest target.

Decision:

- Deer, sheep, and boar should become harvestable carcasses.
- Wolves should still get the same visual carcass depletion states, but wolf carcass harvesting remains disabled in gameplay.

### 12. Corpse Removal Timing

Unit/animal corpses persist until `CORPSE_REMOVE_TICKS`; buildings do not. The prompt design has dead/decayed states but does not say whether decayed remains eventually vanish.

Recommendation:

- Add a note to unit/animal prompts: final decayed state is held until corpse cleanup.
- Do not generate a separate "removed" state; that is absence of entity.

### 13. Concealing Terrain Features

Enemy units inside `T_WHEAT` can be hidden unless detected nearby. Reeds, forests, and pines should also become concealing features. This is a game mechanic, but it should not require separate unit states.

Recommendation:

- Ensure wheat art supports concealment visually: tall enough to plausibly hide small units, but not so tall it hides own units in normal view.
- Add front/back feature occluder layers for reeds, forests, and pines so units can appear behind vegetation.
- Forests and pines should be passable with movement penalty, partial line-of-sight reduction, and hidden wildlife risk.
- Reeds should partially conceal and can reduce visibility through marsh/shallows.
- Use renderer visibility/tint/alpha for actual concealment.

### 14. Unit Hold Position and Attack-Move

Hold position and attack-move are command/AI states, not distinct body animations. Hold position currently uses normal idle; attack-move uses move until enemies are engaged.

Recommendation:

- Keep these as command overlays/status icons, not body sprite states.
- If a visual is needed, add a small stance/command marker overlay rather than duplicating unit animations.

### 15. Formations, Stacking, and Group Selection

Group movement uses formation slots. Multiple military units on one tile are indicated in ASCII by uppercase glyph. This is not a sprite state for a single unit.

Recommendation:

- Use selection/group overlays and optional stack-count UI.
- Do not generate separate stacked-unit sprites.

## Recommended Exporter Changes

1. Add an `effects-ui` Markdown prompt group.
2. Expand building states mechanically:
   - split `construction` into 0/1/2 where appropriate;
   - add `damaged` to all buildings;
   - add `training` to Town Hall and Castle;
   - add `garrison_firing` where building garrison fire should visibly change the building.
3. Add Transport cargo/garrison amount states.
4. Add snow-biome seasonal states for `T_SNOW`.
5. Add settlement halo overlay prompts.
6. Add unit research upgrade variants/overlays:
   - `iron_weapons` for militia/knight, likely overlay first and full upgraded action variants later;
   - `crossbow` for archer, likely overlay first and full upgraded ranged action variants later.
7. Add animal carcass runtime:
   - deer, sheep, and boar become harvestable carcasses;
   - wolves keep matching carcass visuals but remain non-harvestable in gameplay.
8. Add concealing feature support for reeds, forests, and pines, including feature occluder layers and line-of-sight/movement effects.

## Mechanics That Do Not Need Generated Sprite States

- Day/night/dawn/dusk brightness.
- Fog of war and explored/unexplored dimming.
- Storm darkness and concealment tint.
- Exact HP bars.
- Exact training/research progress percentage.
- Queue length.
- Control group assignment.
- Pause/game-over/resign/save/load/diagnostics.
- Cursor panning, minimap navigation, zoom, and mouse selection.
- Pathfinding, stuck recovery, and formation math.
- Supply reservation and resource affordability.

These should remain renderer/UI logic unless the game later wants bespoke icons for them.
