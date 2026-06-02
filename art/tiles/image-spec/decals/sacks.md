# Sacks Decal Image Generation Prompt

Generate Realm image sheets for **sacks decal**.

## Art Brief

- Asset group: decals
- Asset id: sacks
- Visual design: transparent sacks and grain bags overlay for mills, farms, and markets
- Projection: transparent low or flat overlay that sits on top of ground
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `small_sacks`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete decal matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one decal for each listed state or variant**. There are 4 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Decal art must be transparent and sit on the ground.
Use a top-down or very shallow map-overlay view, not an upright icon view.
Keep the decal mostly inside the centre of the tile with soft edges so it can layer over many ground types.
The decal must not imply an independent blocking object or a full terrain replacement.

### Sheet

Use a **2 by 2** grid for this sheet.

- row 1, column 1: `small_sacks` - small sacks cluster
- row 1, column 2: `grain_bags` - grain bags
- row 2, column 1: `mixed_sacks` - mixed sacks and small crates
- row 2, column 2: `snow_dusted` - snow-dusted sacks

## Production Follow-Up

- Final production decal art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with only the low overlay art visible.
- Keep decal opacity and silhouette subtle enough that it reads as ground wear or clutter, not a blocking object.
- Treat the sheet as the visual decision record; generate or crop final production decal images only after the sheet slot is accepted.

## Prompt

Generate Realm sacks decal image sheets. Use transparent low/flat overlay decals that sit on the ground and do not imply an independent blocking object. Create one decal for each of the 4 listed states or variants. The default state is small_sacks.  Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 2 by 2
  - row 1, column 1: small sacks cluster
  - row 1, column 2: grain bags
  - row 2, column 1: mixed sacks and small crates
  - row 2, column 2: snow-dusted sacks
