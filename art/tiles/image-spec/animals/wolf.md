# Wolf Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Wolf**.

## Art Brief

- Source role: hunts units; bolder in winter; avoids settlements outside winter
- Visual design: Grey wolf, low predatory silhouette
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back

## Team Colour Slots

- None

## Tiny Sprite Style Contract

- Convert the provided or generated subject into an ultra-simplified tiny sprite for a medieval-themed board game.
- Preserve the same pose, facing direction, silhouette, species or character identity, clothing, armour, equipment, weapons, shields, harness, tack, accessories, gear placement, and major colour identity.
- Only change the visual style; do not add, remove, swap, or redesign equipment or body forms.

### Core Style

- Highly consistent paper-cutout sprite style, like a simplified medieval manuscript or marginalia illustration turned into a paper standee.
- Ultra-simplified shapes with a very thick, clean, continuous black outer outline around the whole subject.
- Chunky black internal lines only where essential; no thin delicate linework, sketchiness, crosshatching, tiny texture marks, painterly micro-detail, realistic rendering, soft shading, or modern glossy cartoon polish.
- Cream/off-white die-cut paper border following the silhouette, with black subject outline still clearly visible inside that border.
- Flat muted medieval/storybook painted colour areas, preserving the subject's major colour identity; no gradients or glossy effects.
- Simplify aggressively for tiny-size readability: keep silhouette, major colour blocks, essential equipment shapes, minimum facial features, and minimum tack, weapon, or shield lines needed for recognition.

### Animal Face Rules

- Preserve the animal character from the reference or visual brief.
- Eyes should be simple, black, bold, and may be slightly larger or more shaped than human eyes when needed for character.
- Animal faces should feel slightly uncanny in a medieval manuscript way, using simplified muzzle, snout, or beak shapes.
- Keep faces readable and a little odd, not cute-modern; do not over-detail fur, hair, feathers, or musculature.

### Body, Clothing, And Equipment

- Preserve important armour, clothing, shields, weapons, saddles, reins, harness, tack, and accessories faithfully.
- Represent equipment as simplified flat shapes with thick black outlines.
- Keep only the main seams, straps, borders, and forms needed for recognition; avoid tiny buckles, stitching, ornament, and surface texture.
- Shields, helmets, weapons, and horse tack must remain especially recognizable.

### Output Background

- Isolated paper-cutout sprite only, centred on a flat pure magenta background: #FF00FF.
- No base, holder, board, terrain, scene, extra props, decorative frame, realism, 3D render, or cast shadow.
- An extremely subtle contact shadow is allowed only if necessary for tiny-sprite readability.

## Direction And Anchor Contract

- `front` means a three-quarter RTS front angle, body or object turned about 30-45 degrees toward screen right. It is not a flat face-on mascot pose.
- `back` means the matching rear-right three-quarter angle, with shoulders, hull, wheels, cloak, or equipment forming a visible diagonal. It is not a flat rear diagram.
- Do not generate mirrored left-facing source art. The renderer mirrors front/back source art when needed.
- Keep feet, corpse baseline, wheels, boat hull contact, siege base, carried goods, and weapon arcs inside the cell with stable anchor and scale.

## Aspect Ratio

- Image generation preset: Square (1:1).
- Use this aspect ratio for the whole contact sheet; crop accepted slots into production sprites after review.

## Output Resolution

- Final accepted standalone source canvas: 512 by 512 px.
- Use this resolution from the generated JSON spec for every accepted standalone sprite frame; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 512 by 512 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite frame matching the listed state, centred in its grid cell.
- Background: Use a flat pure #ff00ff magenta sheet background and clear gutters between cells.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Keep species silhouette readable in living, attacking, fleeing, and runtime death frames.
- The runtime death action has two frames: frame 00 is the freshly dead animal body, and frame 01 is the same animal's clean depleted skeleton remains.
- Carcass and skeleton frames should lie naturally on the ground and stay inside the cell; avoid gore-heavy imagery.

## States To Generate

Generate **one frame for each state**. There are 5 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

Animal runtime death uses two frames: freshly dead readable carcass, then the same animal's clean depleted skeleton remains.
Do not generate separate partly harvested or mostly harvested runtime actions unless the C++ animation contract adds them.

Wolf death frames are for visual consistency with other animals only; wolf carcass harvesting is not enabled in gameplay.

### Sheet

Use a **3 by 2** grid for this sheet.

- row 1, column 1: `idle` - idle
- row 1, column 2: `prowl` - prowl
- row 1, column 3: `attack` - attack
- row 2, column 1: `winter_aggressive_variant_optional` - winter-aggressive variant optional
- row 2, column 2: `death` - runtime two-frame animal death sequence: freshly dead readable carcass followed by the same animal's clean depleted skeleton remains

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use a flat pure #ff00ff magenta background, with the full paper-cutout sprite inside the square.
- Keep feet, hull base, wheels, siege base, tack, weapons, and equipment anchored consistently across variants.
- Do not add a base, holder, terrain, board, decorative frame, or cast shadow; an extremely subtle contact shadow is acceptable only if it is needed for readability.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Wolf. The footprint is 1 by 1 tile(s). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each of the 5 listed states. Order states left to right and top to bottom within each sheet. Keep the subject consistent across every slot. Final accepted standalone frames use the generated spec resolution: 512 by 512 px. Use a flat pure #ff00ff magenta sheet background and clear gutters between cells. Use clean readable tiny paper-cutout sprite proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. If animal reference images are supplied, use them only for species identity, pose, facing direction, silhouette, and major colour cues, then redraw into stylized Realm sprite art; do not copy their source style or pixels.

Slot order:
- Grid: 3 by 2
  - row 1, column 1: idle
  - row 1, column 2: prowl
  - row 1, column 3: attack
  - row 2, column 1: winter-aggressive variant optional
  - row 2, column 2: runtime two-frame animal death sequence: freshly dead readable carcass followed by the same animal's clean depleted skeleton remains
