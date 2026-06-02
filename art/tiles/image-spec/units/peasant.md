# Peasant Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Peasant**.

## Art Brief

- Source role: worker, builder, gatherer, basic attacker
- Visual design: Hooded medieval labourer with tunic, belt, simple tool; neutral cloth/leather
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: yes

## Team Colour Slots

- tunic strip
- belt sash
- shoulder cloth

## Player Colour

- Use blue (#00AFFF) for the player-colour areas listed above.

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
- Team colour: Use the recommended preview player colour blue (#00AFFF) only in deliberate maskable areas such as banners, shields, cloth trim, pennants, sails, or painted markers. Keep skin, stone, wood, shadows, weapons, animals, and cargo out of team colour.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Keep the same unit identity, clothing, armour, hull, siege frame, weapon set, and carried-equipment scale across every state.
- State changes should be literal and readable: attacks show the weapon setup before release or the follow-through after release, gathering shows the tool/resource, carrying shows the carried material, and death/decay keeps durable gear visible.
- Do not add terrain patches, target enemies, resource nodes, UI badges, or unrelated helper characters inside the cell.

## States To Generate

Generate **one frame for each state**. There are 17 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

### Sheet 1 of 2

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `idle` - Worker is standing idle, then settles into a long-idle arms-crossed pose.
- row 1, column 2: `walk` - Worker walks across the map with an alternating two-step gait.
- row 1, column 3: `chop_wood` - Worker chops an adjacent tree or wood resource with an axe.
- row 1, column 4: `mine_gold` - Worker mines an adjacent gold deposit with a pickaxe.
- row 2, column 1: `gather_berries` - Worker gathers berries from an adjacent berry resource.
- row 2, column 2: `hoe_soil` - Worker tends a farm on an adjacent tile with a farming hoe.
- row 2, column 3: `gather_wheat` - Worker gathers wheat with a sickle.
- row 2, column 4: `build` - Worker kneels and hammers an adjacent construction site.
- row 3, column 1: `carry_wood` - Worker carries wood back to a drop-off while walking.
- row 3, column 2: `carry_gold` - Worker carries gold ore back to a drop-off while walking.
- row 3, column 3: `carry_berries` - Worker carries a basket of berries back to a drop-off while walking.
- row 3, column 4: `carry_wheat` - Worker carries wheat back to a drop-off while walking.
- row 4, column 1: `gather_meat` - Worker gathers meat from an adjacent animal carcass with a knife.
- row 4, column 2: `carry_meat` - Worker carries meat back to a drop-off while walking.
- row 4, column 3: `club_attack` - Worker attacks an adjacent target with a wooden club.
- row 4, column 4: `dead` - dead villager body lying on the ground with clothing and simple tools still intact

### Sheet 2 of 2

Use a **4 by 4** grid for this sheet.
Leave unused cells empty.

- row 1, column 1: `decayed` - human skeleton remains, with small clothing and tool scraps still readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Peasant. The footprint is 1 by 1 tile(s). Team colour is required and the recommended preview player colour is blue (#00AFFF). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each of the 17 listed states. Since there are more than 16 states, split them across multiple images, each image using a 4 by 4 grid. Order states left to right and top to bottom within each sheet. Keep the character or building consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork.

Slot order:
- Sheet 1 of 2: 4 by 4 grid
  - row 1, column 1: Worker is standing idle, then settles into a long-idle arms-crossed pose.
  - row 1, column 2: Worker walks across the map with an alternating two-step gait.
  - row 1, column 3: Worker chops an adjacent tree or wood resource with an axe.
  - row 1, column 4: Worker mines an adjacent gold deposit with a pickaxe.
  - row 2, column 1: Worker gathers berries from an adjacent berry resource.
  - row 2, column 2: Worker tends a farm on an adjacent tile with a farming hoe.
  - row 2, column 3: Worker gathers wheat with a sickle.
  - row 2, column 4: Worker kneels and hammers an adjacent construction site.
  - row 3, column 1: Worker carries wood back to a drop-off while walking.
  - row 3, column 2: Worker carries gold ore back to a drop-off while walking.
  - row 3, column 3: Worker carries a basket of berries back to a drop-off while walking.
  - row 3, column 4: Worker carries wheat back to a drop-off while walking.
  - row 4, column 1: Worker gathers meat from an adjacent animal carcass with a knife.
  - row 4, column 2: Worker carries meat back to a drop-off while walking.
  - row 4, column 3: Worker attacks an adjacent target with a wooden club.
  - row 4, column 4: dead villager body lying on the ground with clothing and simple tools still intact
- Sheet 2 of 2: 4 by 4 grid
  - row 1, column 1: human skeleton remains, with small clothing and tool scraps still readable
