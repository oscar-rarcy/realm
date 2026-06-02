# Wheat Crop Feature Image Generation Prompt

Generate Realm image sheets for **wheat crop feature**.

## Art Brief

- Asset group: features
- Asset id: wheat_crop
- Visual design: transparent crop feature anchored to tile centre over field or dirt ground
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Team colour required: no
- Default state: `spring_full`

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

Generate **one sprite for each listed state or variant**. There are 16 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.
Use a small contact shadow only where it helps anchor the sprite to the tile.
Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.
Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.

### Sheet 1 of 1

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `spring_full` - wheat or crop field feature, spring: fresh, recovering, greener look, full resource amount, abundant and untouched
- row 1, column 2: `spring_mostly_full` - wheat or crop field feature, spring: fresh, recovering, greener look, mostly full resource amount, slightly reduced
- row 1, column 3: `spring_mostly_empty` - wheat or crop field feature, spring: fresh, recovering, greener look, mostly empty resource amount, sparse but still readable
- row 1, column 4: `spring_depleted` - wheat or crop field feature, spring: fresh, recovering, greener look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
- row 2, column 1: `summer_full` - wheat or crop field feature, summer: full growth or dry high-sun look, full resource amount, abundant and untouched
- row 2, column 2: `summer_mostly_full` - wheat or crop field feature, summer: full growth or dry high-sun look, mostly full resource amount, slightly reduced
- row 2, column 3: `summer_mostly_empty` - wheat or crop field feature, summer: full growth or dry high-sun look, mostly empty resource amount, sparse but still readable
- row 2, column 4: `summer_depleted` - wheat or crop field feature, summer: full growth or dry high-sun look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
- row 3, column 1: `autumn_full` - wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, full resource amount, abundant and untouched
- row 3, column 2: `autumn_mostly_full` - wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, mostly full resource amount, slightly reduced
- row 3, column 3: `autumn_mostly_empty` - wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, mostly empty resource amount, sparse but still readable
- row 3, column 4: `autumn_depleted` - wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
- row 4, column 1: `winter_full` - wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, full resource amount, abundant and untouched
- row 4, column 2: `winter_mostly_full` - wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, mostly full resource amount, slightly reduced
- row 4, column 3: `winter_mostly_empty` - wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, mostly empty resource amount, sparse but still readable
- row 4, column 4: `winter_depleted` - wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one anchored sprite and its contact shadow fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm wheat crop feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Team colour is not required. Create one sprite for each listed state or variant. The default state is spring_full. If there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Sheet 1 of 1: 4 by 4 grid
  - row 1, column 1: wheat or crop field feature, spring: fresh, recovering, greener look, full resource amount, abundant and untouched
  - row 1, column 2: wheat or crop field feature, spring: fresh, recovering, greener look, mostly full resource amount, slightly reduced
  - row 1, column 3: wheat or crop field feature, spring: fresh, recovering, greener look, mostly empty resource amount, sparse but still readable
  - row 1, column 4: wheat or crop field feature, spring: fresh, recovering, greener look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
  - row 2, column 1: wheat or crop field feature, summer: full growth or dry high-sun look, full resource amount, abundant and untouched
  - row 2, column 2: wheat or crop field feature, summer: full growth or dry high-sun look, mostly full resource amount, slightly reduced
  - row 2, column 3: wheat or crop field feature, summer: full growth or dry high-sun look, mostly empty resource amount, sparse but still readable
  - row 2, column 4: wheat or crop field feature, summer: full growth or dry high-sun look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
  - row 3, column 1: wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, full resource amount, abundant and untouched
  - row 3, column 2: wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, mostly full resource amount, slightly reduced
  - row 3, column 3: wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, mostly empty resource amount, sparse but still readable
  - row 3, column 4: wheat or crop field feature, autumn: muted, yellowing, leaf-littered, or spent look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
  - row 4, column 1: wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, full resource amount, abundant and untouched
  - row 4, column 2: wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, mostly full resource amount, slightly reduced
  - row 4, column 3: wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, mostly empty resource amount, sparse but still readable
  - row 4, column 4: wheat or crop field feature, winter: frosted, snow-dusted, frozen, or dormant look, fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop
