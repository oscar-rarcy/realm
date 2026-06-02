# Fish Shoal Feature Image Generation Prompt

Generate Realm image sheets for **fish shoal feature**.

## Art Brief

- Asset group: features
- Asset id: fish_shoal
- Visual design: transparent water feature or low decal-like sprite anchored to tile centre
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `full`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one sprite for each listed state or variant**. There are 4 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.
Use a small contact shadow only where it helps anchor the sprite to the tile.
Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.
Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.

### Sheet

Use a **2 by 2** grid for this sheet.

- row 1, column 1: `full` - fish shoal with splashes and ripples on water, full resource amount, abundant and untouched
- row 1, column 2: `mostly_full` - fish shoal with splashes and ripples on water, mostly full resource amount, slightly reduced
- row 2, column 1: `mostly_empty` - fish shoal with splashes and ripples on water, mostly empty resource amount, sparse but still readable
- row 2, column 2: `depleted` - fish shoal with splashes and ripples on water, fully depleted fish shoal: open water ripples with no visible fish

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one anchored sprite and its contact shadow fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm fish shoal feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Create one sprite for each of the 4 listed states or variants. The default state is full.  Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 2 by 2
  - row 1, column 1: fish shoal with splashes and ripples on water, full resource amount, abundant and untouched
  - row 1, column 2: fish shoal with splashes and ripples on water, mostly full resource amount, slightly reduced
  - row 2, column 1: fish shoal with splashes and ripples on water, mostly empty resource amount, sparse but still readable
  - row 2, column 2: fish shoal with splashes and ripples on water, fully depleted fish shoal: open water ripples with no visible fish
