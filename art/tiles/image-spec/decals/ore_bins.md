# Ore Bins Decal Image Generation Prompt

Generate Realm image sheets for **ore bins decal**.

## Art Brief

- Asset group: decals
- Asset id: ore_bins
- Visual design: transparent ore-bin and rock-pile overlay for mining camps
- Projection: transparent low or flat overlay that sits on top of ground
- Footprint: 1 by 1 tile
- Directions: tile
- Team colour required: no
- Default state: `small_ore_bin`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete decal matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Team colour: Do not use team colour markers.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one decal for each listed state or variant**. There are 4 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Decal art must be transparent and sit on the ground.
Use a top-down or very shallow map-overlay view, not an upright icon view.
Keep the decal mostly inside the centre of the tile with soft edges so it can layer over many ground types.
The decal must not imply an independent blocking object or a full terrain replacement.

### Sheet 1 of 1

Use a **2 by 2** grid for this sheet.

- row 1, column 1: `small_ore_bin` - small ore bin
- row 1, column 2: `ore_pile` - ore pile and bin
- row 2, column 1: `mixed_rocks` - mixed rocks and ore scraps
- row 2, column 2: `snow_dusted` - snow-dusted ore bins

## Production Follow-Up

- Final production decal art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with only the low overlay art visible.
- Keep decal opacity and silhouette subtle enough that it reads as ground wear or clutter, not a blocking object.
- Treat the sheet as the visual decision record; generate or crop final production decal images only after the sheet slot is accepted.

## Prompt

Generate Realm ore bins decal image sheets. Use transparent low/flat overlay decals that sit on the ground and do not imply an independent blocking object. Team colour is not required. Create one decal for each listed state or variant. The default state is small_ore_bin. If there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Sheet 1 of 1: 2 by 2 grid
  - row 1, column 1: small ore bin
  - row 1, column 2: ore pile and bin
  - row 2, column 1: mixed rocks and ore scraps
  - row 2, column 2: snow-dusted ore bins
