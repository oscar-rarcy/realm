# Effects UI Sprites Image Generation Prompt

Generate Realm image sheets for **effects UI sprites**.

## Art Brief

- Asset group: effects-ui
- Asset id: effects-ui
- Visual design: clean readable small-RTS effects that remain legible over terrain and units
- Projection: mixed transparent overlays; each item declares tile_overlay, upright_world, or screen_ui
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `melee_hit_spark`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one sprite for each listed state or variant**. There are 28 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Effects and UI items are separate transparent overlays, not terrain, unit, animal, or building sprites.
tile_overlay items sit on the map, upright_world items face the camera, and screen_ui items are drawn in interface space.
Use enough contrast and alpha separation that effects remain readable over grass, snow, water, lava, buildings, and units.
Selection, range, preview, and command-marker items should align to the tile centre and avoid filled backgrounds.

### Sheet 1 of 2

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `melee_hit_spark` - upright_world melee hit spark
- row 1, column 2: `arrow_hit` - upright_world arrow impact
- row 1, column 3: `boulder_impact` - upright_world boulder dust impact
- row 1, column 4: `boulder_water_splash` - upright_world boulder water splash
- row 2, column 1: `building_hit_dust` - upright_world building hit dust
- row 2, column 2: `rain_frame_1` - tile_overlay rain splash frame 1
- row 2, column 3: `rain_frame_2` - tile_overlay rain splash frame 2
- row 2, column 4: `storm_rain_frame_1` - tile_overlay storm rain frame 1
- row 3, column 1: `storm_rain_frame_2` - tile_overlay storm rain frame 2
- row 3, column 2: `snowfall_frame_1` - tile_overlay snowfall frame 1
- row 3, column 3: `snowfall_frame_2` - tile_overlay snowfall frame 2
- row 3, column 4: `move_marker` - tile_overlay move command marker
- row 4, column 1: `attack_marker` - tile_overlay attack command marker
- row 4, column 2: `gather_marker` - tile_overlay gather command marker
- row 4, column 3: `build_marker` - tile_overlay build command marker
- row 4, column 4: `rally_marker` - tile_overlay rally marker

### Sheet 2 of 2

Use a **4 by 4** grid for this sheet.
Leave unused cells empty.

- row 1, column 1: `attack_move_marker` - tile_overlay attack-move marker
- row 1, column 2: `hold_position_marker` - screen_ui hold-position marker
- row 1, column 3: `selection_ring` - tile_overlay selection
- row 1, column 4: `group_selection_ring` - tile_overlay group selection
- row 2, column 1: `range_ring_dot` - tile_overlay range-ring dot
- row 2, column 2: `build_preview_valid` - tile_overlay valid build preview
- row 2, column 3: `build_preview_invalid` - tile_overlay invalid build preview
- row 2, column 4: `wall_preview` - tile_overlay wall preview
- row 3, column 1: `garrison_indicator` - screen_ui garrison indicator
- row 3, column 2: `queued_unit_marker` - screen_ui queued unit marker
- row 3, column 3: `research_active_marker` - screen_ui active research marker
- row 3, column 4: `completed_research_icon_treatment` - screen_ui completed research icon treatment

## Production Follow-Up

- Final production effect/UI art should be exported as one standalone square image per accepted item.
- Use transparent background or a flat #ff00ff magenta key background, with the effect centred and readable over both light and dark terrain.
- Keep rings and command markers centred on the tile anchor; keep screen UI markers compact and readable at small scale.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm effects UI sprites image sheets. Use transparent overlay sprites; do not include terrain, units, buildings, text labels, numbers, watermarks, or cropped artwork. Create one sprite for each of the 28 listed states or variants. The default state is melee_hit_spark.  Since there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Sheet 1 of 2: 4 by 4 grid
  - row 1, column 1: upright_world melee hit spark
  - row 1, column 2: upright_world arrow impact
  - row 1, column 3: upright_world boulder dust impact
  - row 1, column 4: upright_world boulder water splash
  - row 2, column 1: upright_world building hit dust
  - row 2, column 2: tile_overlay rain splash frame 1
  - row 2, column 3: tile_overlay rain splash frame 2
  - row 2, column 4: tile_overlay storm rain frame 1
  - row 3, column 1: tile_overlay storm rain frame 2
  - row 3, column 2: tile_overlay snowfall frame 1
  - row 3, column 3: tile_overlay snowfall frame 2
  - row 3, column 4: tile_overlay move command marker
  - row 4, column 1: tile_overlay attack command marker
  - row 4, column 2: tile_overlay gather command marker
  - row 4, column 3: tile_overlay build command marker
  - row 4, column 4: tile_overlay rally marker
- Sheet 2 of 2: 4 by 4 grid
  - row 1, column 1: tile_overlay attack-move marker
  - row 1, column 2: screen_ui hold-position marker
  - row 1, column 3: tile_overlay selection
  - row 1, column 4: tile_overlay group selection
  - row 2, column 1: tile_overlay range-ring dot
  - row 2, column 2: tile_overlay valid build preview
  - row 2, column 3: tile_overlay invalid build preview
  - row 2, column 4: tile_overlay wall preview
  - row 3, column 1: screen_ui garrison indicator
  - row 3, column 2: screen_ui queued unit marker
  - row 3, column 3: screen_ui active research marker
  - row 3, column 4: screen_ui completed research icon treatment
