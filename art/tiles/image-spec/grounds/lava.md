# Lava Ground Image Generation Prompt

Generate Realm image sheets for **lava ground**.

## Art Brief

- Asset group: grounds
- Asset id: lava
- Visual design: top-down square lava fissure or lava field floor with glow states
- Projection: top-down square source tile, projected into an isometric diamond by the game
- Footprint: 1 by 1 tile
- Directions: tile
- Team colour required: no
- Default state: `base_glow`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete tile sample matching the listed state, filling its grid cell edge-to-edge.
- Background: use opaque tile art inside each cell. Do not use transparency for ground cells unless the state explicitly needs water edge alpha in a later production pass.
- Gutters: keep clear separation between cells so each tile sample can be cropped independently.
- Consistency: keep the same material identity, palette, lighting direction, detail scale, and outline weight across every slot in the file.
- Tile edges: make each cell seamless on all four edges; do not add interior padding, drop shadows, borders, vignettes, or fade-outs.
- Team colour: Do not use team colour markers.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, diamond-shaped tiles, or extra unlisted states.

## States Or Variants To Generate

Generate **one tile for each listed state or variant**. There are 7 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Ground art must be a top-down square source tile. Do not generate isometric diamond source art.
Each cell should be an edge-to-edge seamless tile sample, with no transparent border, drop shadow, grid line, or vignette.
Keep detail broad enough for repeated tiling; avoid unique rocks, flowers, footprints, or landmarks unless that feature is the actual material state.
Do not include upright objects, buildings, units, labels, or baked shadows from separate feature sprites.

### Sheet 1 of 1

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `base_glow` - lava fissure default glow
- row 1, column 2: `glow_bright` - lava fissure brighter heat pulse state
- row 1, column 3: `glow_dim` - lava fissure dimmer cooling pulse state
- row 2, column 1: `rain_steam_frame_1` - lava fissure with rain steam, rain-steam reaction frame 1 with small steam wisps
- row 2, column 2: `rain_steam_frame_2` - lava fissure with rain steam, rain-steam reaction frame 2 with shifted steam wisps
- row 2, column 3: `storm_steam_frame_1` - lava fissure with storm steam, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
- row 3, column 1: `storm_steam_frame_2` - lava fissure with storm steam, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details

## Production Follow-Up

- Final production ground art should be exported as one standalone square tile per accepted state.
- Each tile should be seamless edge-to-edge and still read clearly after the game projects it into an isometric diamond.
- Avoid distinctive repeated landmarks near tile edges unless the state is intentionally road, water, lava, or path-like.
- Treat the sheet as the visual decision record; generate or crop final production tile images only after the sheet slot is accepted.

## Prompt

Generate Realm lava ground image sheets. Use top-down square source tiles that tile cleanly at the edges and remain readable after isometric projection. Team colour is not required. Create one tile for each listed state or variant. The default state is base_glow. If there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use opaque edge-to-edge tile art in each cell, with clear gutters between cells and no transparent border.

Slot order:
- Sheet 1 of 1: 3 by 3 grid
  - row 1, column 1: lava fissure default glow
  - row 1, column 2: lava fissure brighter heat pulse state
  - row 1, column 3: lava fissure dimmer cooling pulse state
  - row 2, column 1: lava fissure with rain steam, rain-steam reaction frame 1 with small steam wisps
  - row 2, column 2: lava fissure with rain steam, rain-steam reaction frame 2 with shifted steam wisps
  - row 2, column 3: lava fissure with storm steam, storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details
  - row 3, column 1: lava fissure with storm steam, storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details
