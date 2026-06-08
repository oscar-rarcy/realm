# Gold Deposit Feature Image Generation Prompt

Generate Realm image sheets for **gold deposit feature**.

## Art Brief

- Asset group: features
- Asset id: gold_deposit
- Visual design: transparent ore deposit feature anchored to tile centre over rocky or dirt ground
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `full`

## Map-Integrated Feature Style Contract

- Use a hand-drawn watercolor map-feature style that feels grown out of or placed into the map, not a movable paper cutout.
- Do not add a cream paper border, sticker outline, holder, base, or freestanding paper edge.
- Use softened painted edges, broad readable shapes, muted natural colours, and selective chunky linework only where it clarifies silhouette or resource state.
- The lower/contact area should blend into the map with transparent softness; if alpha is not available, fade softly toward #ff00ff magenta where the sprite should become transparent.
- Keep the anchor stable across depletion, damage, season, and weather states.
- Concealing features should keep front/occluder areas readable without turning into opaque walls.

## Aspect Ratio

- Image generation preset: Square (1:1).
- Use this aspect ratio for the whole contact sheet; crop accepted slots into production sprites after review.

## Output Resolution

- Generation slot target before crop: about 256 by 256 px per accepted sprite.
- Contact sheets may be larger than this overall; divide the sheet by its grid to judge the approximate slot size.
- Slightly larger slots are fine. Do not downscale accepted art into tiny draw-size runtime proxies during promotion.
- Cropped runtime source floor: longest side at least 128 px after crop.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.
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

- row 1, column 1: `full` - gold deposit with bright ore vein or nuggets, full resource amount, abundant and untouched
- row 1, column 2: `mostly_full` - gold deposit with bright ore vein or nuggets, mostly full resource amount, slightly reduced
- row 2, column 1: `mostly_empty` - gold deposit with bright ore vein or nuggets, mostly empty resource amount, sparse but still readable
- row 2, column 2: `depleted` - gold deposit with bright ore vein or nuggets, fully depleted mineable gold: dull rock scar and dirt with no bright ore left

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one map-integrated feature and any contact blending fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Let lower/contact edges fade softly to alpha or toward #ff00ff magenta when the feature should blend into the map.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm gold deposit feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Create one sprite for each of the 4 listed states or variants. The default state is full.  Order items left to right and top to bottom within each sheet. Use clean readable map-integrated watercolor feature styling, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 2 by 2
  - row 1, column 1: gold deposit with bright ore vein or nuggets, full resource amount, abundant and untouched
  - row 1, column 2: gold deposit with bright ore vein or nuggets, mostly full resource amount, slightly reduced
  - row 2, column 1: gold deposit with bright ore vein or nuggets, mostly empty resource amount, sparse but still readable
  - row 2, column 2: gold deposit with bright ore vein or nuggets, fully depleted mineable gold: dull rock scar and dirt with no bright ore left
