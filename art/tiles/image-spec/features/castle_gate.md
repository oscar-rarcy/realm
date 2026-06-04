# Castle Gate Feature Image Generation Prompt

Generate Realm image sheets for **castle gate feature**.

## Art Brief

- Asset group: features
- Asset id: castle_gate
- Visual design: transparent castle gate feature anchored to tile centre over castle floor
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `closed`

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

- Final accepted standalone source canvas: 48 by 48 px.
- Use this resolution from the generated JSON spec for every accepted standalone sprite; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 48 by 48 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one sprite for each listed state or variant**. There are 9 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.
Use a small contact shadow only where it helps anchor the sprite to the tile.
Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.
Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.

### Sheet

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `closed` - closed castle gate, clearly blocking
- row 1, column 2: `open` - open castle gate, clearly passable
- row 1, column 3: `locked` - locked or barred castle gate
- row 2, column 1: `rain_wet_dark` - castle gate darkened by rain
- row 2, column 2: `frost` - castle gate with frost on stone and wood edges
- row 2, column 3: `snow_light` - castle gate with light snow on upper surfaces
- row 3, column 1: `snow_heavy` - castle gate with heavier settled snow while passability state stays readable
- row 3, column 2: `damaged` - damaged castle gate with cracks and splintering
- row 3, column 3: `broken` - broken castle gate remains

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one map-integrated feature and any contact blending fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Let lower/contact edges fade softly to alpha or toward #ff00ff magenta when the feature should blend into the map.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm castle gate feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Create one sprite for each of the 9 listed states or variants. The default state is closed.  Order items left to right and top to bottom within each sheet. Use clean readable map-integrated watercolor feature styling, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 3 by 3
  - row 1, column 1: closed castle gate, clearly blocking
  - row 1, column 2: open castle gate, clearly passable
  - row 1, column 3: locked or barred castle gate
  - row 2, column 1: castle gate darkened by rain
  - row 2, column 2: castle gate with frost on stone and wood edges
  - row 2, column 3: castle gate with light snow on upper surfaces
  - row 3, column 1: castle gate with heavier settled snow while passability state stays readable
  - row 3, column 2: damaged castle gate with cracks and splintering
  - row 3, column 3: broken castle gate remains
