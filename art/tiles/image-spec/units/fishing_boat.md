# Fishing Boat Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Fishing Boat**.

## Art Brief

- Source role: gathers fish, returns to dock
- Visual design: Small brown skiff/canoe with net or fish line
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: yes

## Team Colour Slots

- tiny flag or sail patch

## Player Colour

- Use red (#FF0000) for the player-colour areas listed above.

## Curated Reference Role

- If user-supplied reference images from `art/reference/units/` are provided, use them only for gear, equipment, weapon tier, armour tier, shield placement, horse tack, and broad silhouette cues.
- Do not copy the reference image's exact pixels, finish, lighting, proportions, pose, background, or style.
- Redraw the result as stylized Realm small-RTS sprite art that follows this prompt's visual design, direction, team-colour slots, state grid, and output contract.
- Siege units and ships do not yet have curated unit references; generate those from the prompt until references are supplied.

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

### Human Face Rules

- Eyes are two bold tiny black dots.
- Nose is one short bold black stroke or dot-like mark.
- Mouth is one tiny short black line, or omitted if not needed.
- Keep facial marks simple, thick, and readable at tiny scale; no detailed lips, eyelids, teeth, or realistic facial modelling.
- Expression should be simple, readable, and slightly medieval-naive.

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
- Team colour: Use the recommended preview player colour red (#FF0000) only in deliberate maskable areas such as banners, shields, cloth trim, pennants, sails, or painted markers. Keep skin, stone, wood, shadows, weapons, animals, and cargo out of team colour.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Keep the same unit identity, clothing, armour, hull, siege frame, weapon set, and carried-equipment scale across every state.
- State changes should be literal and readable: attacks show the weapon setup before release or the follow-through after release, gathering shows the tool/resource, carrying shows the carried material, and death/decay keeps durable gear visible.
- Do not add terrain patches, target enemies, resource nodes, UI badges, or unrelated helper characters inside the cell.
- Naval units should sit on transparent background without baked water, while hull direction and sail/team-colour areas remain readable.

## States To Generate

Generate **one frame for each state**. There are 6 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

### Sheet

Use a **3 by 2** grid for this sheet.

- row 1, column 1: `idle` - idle
- row 1, column 2: `row_sail` - row/sail
- row 1, column 3: `fish_net_cast` - fish/net cast
- row 2, column 1: `carrying_fish` - carrying fish
- row 2, column 2: `dead` - destroyed wreck, broken but still recognizable
- row 2, column 3: `decayed` - weathered wreckage, with durable wood, metal, wheels, hull, or siege parts still readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use a flat pure #ff00ff magenta background, with the full paper-cutout sprite inside the square.
- Keep feet, hull base, wheels, siege base, tack, weapons, and equipment anchored consistently across variants.
- Do not add a base, holder, terrain, board, decorative frame, or cast shadow; an extremely subtle contact shadow is acceptable only if it is needed for readability.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Fishing Boat. The footprint is 1 by 1 tile(s). Team colour is required and the recommended preview player colour is red (#FF0000). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each of the 6 listed states. Order states left to right and top to bottom within each sheet. Keep the subject consistent across every slot. Final accepted standalone frames use the generated spec resolution: 512 by 512 px. Use a flat pure #ff00ff magenta sheet background and clear gutters between cells. Use clean readable tiny paper-cutout sprite proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. If unit reference images are supplied, use them only for equipment and silhouette cues, then redraw into stylized Realm sprite art; do not copy their source style or pixels.

Slot order:
- Grid: 3 by 2
  - row 1, column 1: idle
  - row 1, column 2: row/sail
  - row 1, column 3: fish/net cast
  - row 2, column 1: carrying fish
  - row 2, column 2: destroyed wreck, broken but still recognizable
  - row 2, column 3: weathered wreckage, with durable wood, metal, wheels, hull, or siege parts still readable
