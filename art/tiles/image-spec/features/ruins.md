# Ruins Feature Image Generation Prompt

Generate Realm image sheets for **ruins feature**.

## Art Brief

- Asset group: features
- Asset id: ruins
- Visual design: transparent ruin object or footprint feature anchored to tile centre
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Team colour required: no
- Default state: `spring`

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

- row 1, column 1: `spring` - ancient rubble and broken stone ruins feature, spring: fresh, recovering, greener look
- row 1, column 2: `summer` - ancient rubble and broken stone ruins feature, summer: full growth or dry high-sun look
- row 1, column 3: `autumn` - ancient rubble and broken stone ruins feature, autumn: muted, yellowing, leaf-littered, or spent look
- row 2, column 1: `winter` - winter: ruins with light snow caught in cracks and on rubble tops
- row 2, column 2: `damaged` - more collapsed ruin footprint with broken stones
- row 2, column 3: `overgrown` - ruins with small vegetation growth and moss

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one anchored sprite and its contact shadow fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm ruins feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Team colour is not required. Create one sprite for each listed state or variant. The default state is spring. If there are more than 16 items, split them across multiple images, each image using a 4 by 4 grid. Order items left to right and top to bottom within each sheet. Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Sheet 1 of 1: 3 by 2 grid
  - row 1, column 1: ancient rubble and broken stone ruins feature, spring: fresh, recovering, greener look
  - row 1, column 2: ancient rubble and broken stone ruins feature, summer: full growth or dry high-sun look
  - row 1, column 3: ancient rubble and broken stone ruins feature, autumn: muted, yellowing, leaf-littered, or spent look
  - row 2, column 1: winter: ruins with light snow caught in cracks and on rubble tops
  - row 2, column 2: more collapsed ruin footprint with broken stones
  - row 2, column 3: ruins with small vegetation growth and moss
