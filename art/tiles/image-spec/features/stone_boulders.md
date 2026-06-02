# Stone Boulders Feature Image Generation Prompt

Generate Realm image sheets for **stone boulders feature**.

## Art Brief

- Asset group: features
- Asset id: stone_boulders
- Visual design: transparent blocking boulder feature anchored to tile centre over rocky or biome ground
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Team colour required: no
- Default state: `base_clear`

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Team colour: Do not use team colour markers.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one sprite for each listed state or variant**. There are 6 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.
Use a small contact shadow only where it helps anchor the sprite to the tile.
Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.
Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.

### Sheet 1 of 1

Use a **3 by 2** grid for this sheet.

- row 1, column 1: `base_clear` - default blocking stone boulder cluster
- row 1, column 2: `rain_wet_dark` - stone boulders darkened by rain
- row 1, column 3: `frost` - stone boulders with frost in cracks
- row 2, column 1: `snowcap_light` - stone boulders with light snow caps
- row 2, column 2: `snowcap_heavy` - stone boulders with heavier snow while silhouette stays readable
- row 2, column 3: `damaged` - cracked or chipped stone boulders

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one anchored sprite and its contact shadow fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm stone boulders feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Team colour is not required. Create one sprite for each listed state or variant. The default state is base_clear. If there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Sheet 1 of 1: 3 by 2 grid
  - row 1, column 1: default blocking stone boulder cluster
  - row 1, column 2: stone boulders darkened by rain
  - row 1, column 3: stone boulders with frost in cracks
  - row 2, column 1: stone boulders with light snow caps
  - row 2, column 2: stone boulders with heavier snow while silhouette stays readable
  - row 2, column 3: cracked or chipped stone boulders
