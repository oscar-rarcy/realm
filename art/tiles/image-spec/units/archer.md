# Archer Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Archer**.

## Art Brief

- Source role: ranged infantry; Crossbows research changes range
- Visual design: Bow/crossbow-ready archer with hood and quiver
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: yes

## Team Colour Slots

- hood trim
- quiver wrap
- shoulder sash

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
- Team colour: Use team colour only in deliberate maskable areas such as banners, shields, cloth trim, pennants, sails, or painted markers. Keep skin, stone, wood, shadows, weapons, animals, and cargo out of team colour.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Keep the same unit identity, clothing, armour, hull, siege frame, weapon set, and carried-equipment scale across every state.
- State changes should be literal and readable: attacks show the weapon or projectile setup, gathering shows the tool/resource, carrying shows the carried material, and death/decay keeps durable gear visible.
- Do not add terrain patches, target enemies, resource nodes, UI badges, or extra helper characters inside the cell.

## States To Generate

Generate **one frame for each state**. There are 10 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

### Sheet 1 of 1

Use a **4 by 3** grid for this sheet.

- row 1, column 1: `idle` - idle
- row 1, column 2: `walk` - walk
- row 1, column 3: `aim` - aim
- row 1, column 4: `release` - release
- row 2, column 1: `reload` - reload
- row 2, column 2: `optional_upgraded_crossbow_variant` - optional upgraded crossbow variant
- row 2, column 3: `crossbows_overlay` - visible crossbow upgrade equipment overlay for completed Crossbows research
- row 2, column 4: `crossbows_action_variant` - full archer action variant using crossbow equipment for completed Crossbows research
- row 3, column 1: `dead` - dead human body lying on the ground with clothing, armour, weapons, and equipment still intact
- row 3, column 2: `decayed` - human skeleton remains, with armour, weapons, bows, shields, tools, and other equipment still intact and readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Archer. The footprint is 1 by 1 tile(s). Team colour is required. Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each listed state. If there are more than 16 states, split them across multiple images, each image using a 4 by 4 grid. Order states left to right and top to bottom within each sheet. Keep the character or building consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork.

Slot order:
- Sheet 1 of 1: 4 by 3 grid
  - row 1, column 1: idle
  - row 1, column 2: walk
  - row 1, column 3: aim
  - row 1, column 4: release
  - row 2, column 1: reload
  - row 2, column 2: optional upgraded crossbow variant
  - row 2, column 3: visible crossbow upgrade equipment overlay for completed Crossbows research
  - row 2, column 4: full archer action variant using crossbow equipment for completed Crossbows research
  - row 3, column 1: dead human body lying on the ground with clothing, armour, weapons, and equipment still intact
  - row 3, column 2: human skeleton remains, with armour, weapons, bows, shields, tools, and other equipment still intact and readable
