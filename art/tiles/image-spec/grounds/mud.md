# Mud Ground Image Generation Prompt

Generate Realm image sheets for **mud ground**.

## Art Brief

- Asset group: grounds
- Asset id: mud
- Visual design: top-down square wet mud floor with ruts and puddled surface
- Projection: top-down square source tile
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `clear_wet`

## Map-Ground Style Contract

- Use a hand-drawn watercolor map-surface style that feels like part of the board itself, not a paper cutout.
- Do not add a cream paper border, sticker outline, freestanding object edge, holder, or base.
- Keep the existing Realm raised square terrain-slab geometry: continuous top material, subtle bevel, chipped/worn side faces, worn corners, and dark contact shadow.
- Paint the terrain material with broad readable watercolor shapes and muted natural colour variation; avoid noisy texture, tiny details, photorealism, glossy digital finish, or decorative UI edging.
- Default ground tiles should fill the whole square. Edge, transition, shoreline, snow, melt, or overlay-like states may use transparency where the material fades out; if alpha is not available, fade softly toward #ff00ff magenta at transparent edges.

## Aspect Ratio

- Image generation preset: Square (1:1).
- Use this aspect ratio for the whole contact sheet; crop accepted slots into production sprites after review.

## Output Resolution

- Final accepted standalone source canvas: 1024 by 1024 px.
- Use this resolution from the generated JSON spec for every accepted standalone tile; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 1024 by 1024 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete square 3D terrain slab matching the listed state, filling its grid cell.
- Background: use opaque tile art inside each cell. Do not use transparency for ground cells unless the state explicitly needs water edge alpha in a later production pass.
- Gutters: keep clear separation between cells so each tile sample can be cropped independently.
- Consistency: keep the same material identity, palette, lighting direction, detail scale, and slab geometry across every slot in the file.
- Ground tile shape: match the approved Realm grass reference geometry: one thick square terrain slab seen from above, with a continuous top material surface, subtle bevel, chipped/worn side faces, worn corners, and a dark contact shadow outside the slab.
- Do not draw a separate outline, rim, trim, decorative surround, or UI-style edging around the material. The edge must read as the physical side of the terrain slab itself.
- Keep side faces muted and material-coloured according to the ground-specific side material guidance, not bright gold, yellow, glowing, clean, or high-contrast. Do not draw an inner rectangle or inset line between the top surface and the side faces.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, diamond-shaped tiles, or extra unlisted states.

## States Or Variants To Generate

Generate **one tile for each listed state or variant**. There are 8 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Ground art must be a top-down square tile. Do not generate perspective or angled scene art.
Each cell should be one physical square terrain slab: continuous top surface, subtle bevel, chipped/worn side faces, worn corners, and dark contact shadow outside the slab.
Slab side material for `mud`: dark wet brown peat and clay side material, with subtle glossy damp highlights; not green grass sides and not clean stone.
The slab edge is not decorative. Do not add an outline, rim, trim, decorative surround, or UI-style edging around the terrain material.
Side faces should be muted and material-coloured according to the ground-specific side material guidance, not bright gold or yellow. Do not draw an inner rectangle or inset line around the top surface.
If using a reference tile, preserve the reference's 3D slab geometry exactly and change only the terrain material or state on the slab unless the prompt explicitly asks for a new slab shape.
Keep detail broad enough for repeated tiling; avoid unique rocks, flowers, footprints, or landmarks unless that feature is the actual material state.
Do not include upright objects, buildings, units, labels, or baked shadows from separate feature sprites.
Hard failure: no missing 3D slab sides, no bright gold/yellow edging, no inner rectangle, no decorative surround or UI-like outline, and no invisible seamless texture tile.

### Sheet

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `clear_wet` - mud terrain in clear weather, wet dark surface and puddles
- row 1, column 2: `drying_edges` - mud terrain drying toward dirt, cracked edges and shrinking puddles
- row 1, column 3: `rain_frame_1` - mud terrain, rain reaction frame 1 with small splash or wet-sheen details
- row 2, column 1: `rain_frame_2` - mud terrain, rain reaction frame 2 with shifted splash or wet-sheen details
- row 2, column 2: `storm_frame_1` - mud terrain, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
- row 2, column 3: `storm_frame_2` - mud terrain, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details
- row 3, column 1: `winter_frozen` - frozen mud with hard glossy ruts
- row 3, column 2: `snow_dusted` - mud with dirty snow and slush on top

## Production Follow-Up

- Final production ground art should be exported as one standalone square tile per accepted state.
- Each tile should keep the physical 3D slab shape from the approved grass reference: continuous top material, subtle bevel, chipped/worn side faces, worn corners, and dark contact shadow.
- Each tile should use its ground-specific side material guidance for slab-side colour, chips, translucency cues, frost, mud, stone, paving, or glow.
- Reject any output where the generator turns the slab edge into a decorative surround, outline, rim, trim, or UI-like edging.
- Reject bright gold/yellow slab sides or an inner rectangle/inset line around the top material.
- Avoid distinctive repeated landmarks near tile edges unless the state is intentionally road, water, lava, or path-like.
- Treat the sheet as the visual decision record; generate or crop final production tile images only after the sheet slot is accepted.

## Prompt

Generate Realm mud ground image sheets. Use top-down square 3D terrain slabs that fill the whole square and match the approved Realm grass reference geometry: continuous top material, subtle bevel, worn corners, and dark contact shadow; side material guidance for mud: dark wet brown peat and clay side material, with subtle glossy damp highlights; not green grass sides and not clean stone. Create one tile for each of the 8 listed states or variants. The default state is clear_wet.  Order items left to right and top to bottom within each sheet. Use clean readable watercolor map-ground styling, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use opaque full-square slab art in each cell, with the same physical 3D terrain-tile sides, muted chipped worn corners, and dark contact shadow as the approved Realm grass reference; keep clear gutters between cells.

Slot order:
- Grid: 3 by 3
  - row 1, column 1: mud terrain in clear weather, wet dark surface and puddles
  - row 1, column 2: mud terrain drying toward dirt, cracked edges and shrinking puddles
  - row 1, column 3: mud terrain, rain reaction frame 1 with small splash or wet-sheen details
  - row 2, column 1: mud terrain, rain reaction frame 2 with shifted splash or wet-sheen details
  - row 2, column 2: mud terrain, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
  - row 2, column 3: mud terrain, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details
  - row 3, column 1: frozen mud with hard glossy ruts
  - row 3, column 2: mud with dirty snow and slush on top
