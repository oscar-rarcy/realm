# Transport Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Transport**.

## Art Brief

- Source role: carries garrisoned units
- Visual design: Broad ferry/barge, visible cargo deck
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: yes

## Team Colour Slots

- sail/pennant
- side banner

## Player Colour

- Use red (#FF0000) for the player-colour areas listed above.

## Direction And Anchor Contract

- `front` means a three-quarter RTS front angle, body or object turned about 30-45 degrees toward screen right. It is not a flat face-on mascot pose.
- `back` means the matching rear-right three-quarter angle, with shoulders, hull, wheels, cloak, or equipment forming a visible diagonal. It is not a flat rear diagram.
- Do not generate mirrored left-facing source art. The renderer mirrors front/back source art when needed.
- Keep feet, corpse baseline, wheels, boat hull contact, siege base, carried goods, and weapon arcs inside the cell with stable anchor and scale.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite frame matching the listed state, centred in its grid cell.
- Background: Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells.
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

Generate **one frame for each state**. There are 9 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

### Sheet

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `idle` - idle
- row 1, column 2: `sail` - sail
- row 1, column 3: `load_unload` - load/unload
- row 2, column 1: `cargo_full_indicator` - cargo full indicator
- row 2, column 2: `empty` - transport with no visible cargo load
- row 2, column 3: `loaded_partial` - transport partially loaded using covered cargo, weight, flags, or silhouette cues
- row 3, column 1: `loaded_full` - transport fully loaded using covered cargo, weight, flags, or silhouette cues
- row 3, column 2: `dead` - destroyed wreck, broken but still recognizable
- row 3, column 3: `decayed` - weathered wreckage, with durable wood, metal, wheels, hull, or siege parts still readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Transport. The footprint is 1 by 1 tile(s). Team colour is required and the recommended preview player colour is red (#FF0000). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each of the 9 listed states. Order states left to right and top to bottom within each sheet. Keep the character or building consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork.

Slot order:
- Grid: 3 by 3
  - row 1, column 1: idle
  - row 1, column 2: sail
  - row 1, column 3: load/unload
  - row 2, column 1: cargo full indicator
  - row 2, column 2: transport with no visible cargo load
  - row 2, column 3: transport partially loaded using covered cargo, weight, flags, or silhouette cues
  - row 3, column 1: transport fully loaded using covered cargo, weight, flags, or silhouette cues
  - row 3, column 2: destroyed wreck, broken but still recognizable
  - row 3, column 3: weathered wreckage, with durable wood, metal, wheels, hull, or siege parts still readable
