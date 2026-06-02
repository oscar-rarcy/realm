# Dirt Ground Image Generation Prompt

Generate Realm image sheets for **dirt ground**.

## Art Brief

- Asset group: grounds
- Asset id: dirt
- Visual design: top-down square bare earth floor for worn paths, building yards, and exposed soil
- Projection: top-down square source tile, projected into an isometric diamond by the game
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `spring_damp`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete tile sample matching the listed state, filling its grid cell edge-to-edge.
- Background: use opaque tile art inside each cell. Do not use transparency for ground cells unless the state explicitly needs water edge alpha in a later production pass.
- Gutters: keep clear separation between cells so each tile sample can be cropped independently.
- Consistency: keep the same material identity, palette, lighting direction, detail scale, and outline weight across every slot in the file.
- Tile edges: make each cell seamless on all four edges; do not add interior padding, drop shadows, borders, vignettes, or fade-outs.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, diamond-shaped tiles, or extra unlisted states.

## States Or Variants To Generate

Generate **one tile for each listed state or variant**. There are 8 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Ground art must be a top-down square source tile. Do not generate isometric diamond source art.
Each cell should be an edge-to-edge seamless tile sample, with no transparent border, drop shadow, grid line, or vignette.
Keep detail broad enough for repeated tiling; avoid unique rocks, flowers, footprints, or landmarks unless that feature is the actual material state.
Do not include upright objects, buildings, units, labels, or baked shadows from separate feature sprites.

### Sheet

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `spring_damp` - bare earth in spring, damp with small green regrowth
- row 1, column 2: `summer_dry` - bare earth in summer, dry and dusty
- row 1, column 3: `autumn_wet` - bare earth in autumn, darker with leaf litter
- row 2, column 1: `winter_frozen` - bare earth in winter, frozen with frost and patchy snow
- row 2, column 2: `rain_frame_1` - bare earth, rain reaction frame 1 with small splash or wet-sheen details
- row 2, column 3: `rain_frame_2` - bare earth, rain reaction frame 2 with shifted splash or wet-sheen details
- row 3, column 1: `storm_frame_1` - bare earth, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
- row 3, column 2: `storm_frame_2` - bare earth, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details

## Production Follow-Up

- Final production ground art should be exported as one standalone square tile per accepted state.
- Each tile should be seamless edge-to-edge and still read clearly after the game projects it into an isometric diamond.
- Avoid distinctive repeated landmarks near tile edges unless the state is intentionally road, water, lava, or path-like.
- Treat the sheet as the visual decision record; generate or crop final production tile images only after the sheet slot is accepted.

## Prompt

Generate Realm dirt ground image sheets. Use top-down square source tiles that tile cleanly at the edges and remain readable after isometric projection. Create one tile for each of the 8 listed states or variants. The default state is spring_damp.  Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use opaque edge-to-edge tile art in each cell, with clear gutters between cells and no transparent border.

Slot order:
- Grid: 3 by 3
  - row 1, column 1: bare earth in spring, damp with small green regrowth
  - row 1, column 2: bare earth in summer, dry and dusty
  - row 1, column 3: bare earth in autumn, darker with leaf litter
  - row 2, column 1: bare earth in winter, frozen with frost and patchy snow
  - row 2, column 2: bare earth, rain reaction frame 1 with small splash or wet-sheen details
  - row 2, column 3: bare earth, rain reaction frame 2 with shifted splash or wet-sheen details
  - row 3, column 1: bare earth, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
  - row 3, column 2: bare earth, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details
