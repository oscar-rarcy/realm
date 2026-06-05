# Forest Feature Image Generation Prompt

Generate Realm image sheets for **forest feature**.

## Art Brief

- Asset group: features
- Asset id: forest
- Visual design: transparent upright deciduous forest cluster anchored to tile centre; passable, harvestable, concealing
- Projection: transparent upright sprite anchored over a projected isometric map tile
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `spring_full`

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

- Final accepted standalone source canvas: 512 by 512 px.
- Use this resolution from the generated JSON spec for every accepted standalone sprite; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 512 by 512 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite matching the listed state, centred in its grid cell.
- Background: Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## States Or Variants To Generate

Generate **one sprite for each listed state or variant**. There are 16 item(s). Each image may contain at most **16 items** in a **4 by 4** grid.
The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.

Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.
Use a small contact shadow only where it helps anchor the sprite to the tile.
Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.
Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.
Concealing feature contract: define separate `back` and `front_occluder` layers so units can appear partly behind foliage or reeds.
The `back` layer should contain trunks, rear foliage, stems, and contact details; the `front_occluder` layer should contain only the foreground coverage that can overlap units.

### Sheet

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `spring_full` - oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, full resource amount, abundant and untouched
- row 1, column 2: `spring_mostly_full` - oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, mostly full resource amount, slightly reduced
- row 1, column 3: `spring_mostly_empty` - oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, mostly empty resource amount, sparse but still readable
- row 1, column 4: `spring_depleted` - oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
- row 2, column 1: `summer_full` - oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, full resource amount, abundant and untouched
- row 2, column 2: `summer_mostly_full` - oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, mostly full resource amount, slightly reduced
- row 2, column 3: `summer_mostly_empty` - oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, mostly empty resource amount, sparse but still readable
- row 2, column 4: `summer_depleted` - oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
- row 3, column 1: `autumn_full` - oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, full resource amount, abundant and untouched
- row 3, column 2: `autumn_mostly_full` - oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, mostly full resource amount, slightly reduced
- row 3, column 3: `autumn_mostly_empty` - oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, mostly empty resource amount, sparse but still readable
- row 3, column 4: `autumn_depleted` - oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
- row 4, column 1: `winter_full` - oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, full resource amount, abundant and untouched
- row 4, column 2: `winter_mostly_full` - oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, mostly full resource amount, slightly reduced
- row 4, column 3: `winter_mostly_empty` - oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, mostly empty resource amount, sparse but still readable
- row 4, column 4: `winter_depleted` - oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed

## Production Follow-Up

- Final production feature art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with one map-integrated feature and any contact blending fully inside the square.
- Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.
- Let lower/contact edges fade softly to alpha or toward #ff00ff magenta when the feature should blend into the map.
- Treat the sheet as the visual decision record; generate or crop final production sprite images only after the sheet slot is accepted.

## Prompt

Generate Realm forest feature image sheets. Use transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette. Create one sprite for each of the 16 listed states or variants. The default state is spring_full.  Order items left to right and top to bottom within each sheet. Use clean readable map-integrated watercolor feature styling, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 4 by 4
  - row 1, column 1: oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, full resource amount, abundant and untouched
  - row 1, column 2: oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, mostly full resource amount, slightly reduced
  - row 1, column 3: oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, mostly empty resource amount, sparse but still readable
  - row 1, column 4: oak forest canopy, trunks, and contact shadow, spring: fresh, recovering, greener look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
  - row 2, column 1: oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, full resource amount, abundant and untouched
  - row 2, column 2: oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, mostly full resource amount, slightly reduced
  - row 2, column 3: oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, mostly empty resource amount, sparse but still readable
  - row 2, column 4: oak forest canopy, trunks, and contact shadow, summer: full growth or dry high-sun look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
  - row 3, column 1: oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, full resource amount, abundant and untouched
  - row 3, column 2: oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, mostly full resource amount, slightly reduced
  - row 3, column 3: oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, mostly empty resource amount, sparse but still readable
  - row 3, column 4: oak forest canopy, trunks, and contact shadow, autumn: muted, yellowing, leaf-littered, or spent look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
  - row 4, column 1: oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, full resource amount, abundant and untouched
  - row 4, column 2: oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, mostly full resource amount, slightly reduced
  - row 4, column 3: oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, mostly empty resource amount, sparse but still readable
  - row 4, column 4: oak forest canopy, trunks, and contact shadow, winter: frosted, snow-dusted, frozen, or dormant look, fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed
