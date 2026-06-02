# Sheep Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Sheep**.

## Art Brief

- Source role: domestic sheep near bases; flees individually; food on kill
- Visual design: White/off-white sheep, rounded silhouette
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: no

## Team Colour Slots

- None

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
- Team colour: Do not use team colour markers.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Keep species silhouette readable in living, attacking, fleeing, dead, partly harvested, mostly harvested, and skeleton states.
- Carcass states should lie naturally on the ground and stay inside the cell; avoid gore-heavy imagery.
- The depleted skeleton must still suggest the original animal species rather than a generic bone pile.

## States To Generate

Generate **one frame for each state**. There are 7 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

Animal carcass states use four depletion levels: dead unharvested, partly harvested, mostly harvested, and depleted skeleton.

### Sheet 1 of 1

Use a **3 by 3** grid for this sheet.

- row 1, column 1: `idle_graze` - idle/graze
- row 1, column 2: `walk` - walk
- row 1, column 3: `flee` - flee
- row 2, column 1: `dead_unharvested` - dead animal body lying on the ground, full carcass, species silhouette still readable
- row 2, column 2: `partly_harvested` - partly harvested carcass, some meat or hide removed, species still readable
- row 2, column 3: `mostly_harvested` - mostly harvested carcass, sparse remains with bones beginning to show
- row 3, column 1: `depleted_skeleton` - fully depleted decayed skeleton remains, species silhouette still readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Sheep. The footprint is 1 by 1 tile(s). Team colour is not required. Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each listed state. If there are more than 16 states, split them across multiple images, each image using a 4 by 4 grid. Order states left to right and top to bottom within each sheet. Keep the character or building consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork.

Slot order:
- Sheet 1 of 1: 3 by 3 grid
  - row 1, column 1: idle/graze
  - row 1, column 2: walk
  - row 1, column 3: flee
  - row 2, column 1: dead animal body lying on the ground, full carcass, species silhouette still readable
  - row 2, column 2: partly harvested carcass, some meat or hide removed, species still readable
  - row 2, column 3: mostly harvested carcass, sparse remains with bones beginning to show
  - row 3, column 1: fully depleted decayed skeleton remains, species silhouette still readable
