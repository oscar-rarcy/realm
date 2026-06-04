# Tall Grass Decal Image Generation Prompt

Generate Realm image sheets for **tall grass decal**.

## Art Brief

- Asset group: decals
- Asset id: tall_grass
- Visual design: transparent low/medium grass clump overlay; visual variation, not an independent blocker
- Projection: transparent low or flat overlay that sits on top of ground
- Footprint: 1 by 1 tile
- Directions: tile
- Default state: `spring`

## Simplified Ground-Decal Style Contract

- Use a simplified hand-painted map-mark style, not a paper cutout.
- Do not add a cream paper border, die-cut edge, sticker outline, freestanding shadow, holder, or base.
- Keep shapes simple, readable, and low against the ground, with soft or broken edges that can layer over different terrain.
- Use muted painted colour areas and minimal broad linework only where needed for readability.
- Decals should feel like markings, plants, stones, puddles, scuffs, or wear on the map surface, not independent objects.
- If transparency is not available, let fading or soft transparent edges fade toward #ff00ff magenta at the boundary.

## Aspect Ratio

- Image generation preset: Square (1:1).
- Use this aspect ratio for the whole contact sheet; crop accepted slots into production sprites after review.

## Output Resolution

- Final accepted standalone source canvas: 48 by 48 px.
- Use this resolution from the generated JSON spec for every accepted standalone decal; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 48 by 48 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete decal matching the listed state, centred in its grid cell.
- Background: Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.
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

- row 1, column 1: `spring` - tall grass clumps, spring: fresh, recovering, greener look
- row 1, column 2: `summer` - tall grass clumps, summer: full growth or dry high-sun look
- row 2, column 1: `autumn` - tall grass clumps, autumn: muted, yellowing, leaf-littered, or spent look
- row 2, column 2: `winter` - winter: flattened frosted tall grass with patchy snow

## Production Follow-Up

- Final production decal art should be exported as one standalone square image per accepted state.
- Use transparent background or a flat #ff00ff magenta key background, with only the low simplified map-mark art visible.
- Keep decal opacity and silhouette subtle enough that it reads as ground wear or clutter, not a blocking object.
- Do not add paper borders, sticker outlines, freestanding shadows, holders, bases, or cream die-cut edges.
- Treat the sheet as the visual decision record; generate or crop final production decal images only after the sheet slot is accepted.

## Prompt

Generate Realm tall grass decal image sheets. Use transparent low/flat overlay decals that sit on the ground and do not imply an independent blocking object. Create one decal for each of the 4 listed states or variants. The default state is spring.  Order items left to right and top to bottom within each sheet. Use clean readable simplified hand-painted ground-decal styling, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.

Slot order:
- Grid: 2 by 2
  - row 1, column 1: tall grass clumps, spring: fresh, recovering, greener look
  - row 1, column 2: tall grass clumps, summer: full growth or dry high-sun look
  - row 2, column 1: tall grass clumps, autumn: muted, yellowing, leaf-littered, or spent look
  - row 2, column 2: winter: flattened frosted tall grass with patchy snow
