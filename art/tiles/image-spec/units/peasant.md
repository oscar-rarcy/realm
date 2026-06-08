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

- Generation slot target before crop: about 256 by 256 px per accepted sprite frame.
- Contact sheets may be larger than this overall; divide the sheet by its grid to judge the approximate slot size.
- Slightly larger slots are fine. Do not downscale accepted art into tiny draw-size runtime proxies during promotion.
- Cropped runtime source floor: longest side at least 128 px after crop.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite frame matching the listed state, centred in its grid cell.
- Background: Use a flat pure #ff00ff magenta sheet background and clear gutters between cells.
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

Generate **one sprite frame for each listed slot**. There are 34 frame slot(s). Each image may contain at most **16 frame slots** in a **4 by 4** grid.

### Sheet 1 of 3

Use a **4 by 4** grid for this sheet.
Slot target for this sheet: about **256 by 256 px** per cell before crop.

- row 1, column 1: `idle` frame 00 - relaxed idle, arms at sides, both feet planted
- row 1, column 2: `idle` frame 01 - long-idle hold pose, arms crossed, both feet planted
- row 1, column 3: `walk` frame 00 - walking gait with the front/near leg forward and the rear/far leg back
- row 1, column 4: `walk` frame 01 - walking gait with the rear/far leg forward and the front/near leg back
- row 2, column 1: `chop_wood` frame 00 - axe at the bottom/contact part of the chop, axe head low and forward
- row 2, column 2: `chop_wood` frame 01 - axe raised high at the top of the swing
- row 2, column 3: `mine_gold` frame 00 - pickaxe at the bottom/contact part of the mining swing, pick head low and forward
- row 2, column 4: `mine_gold` frame 01 - pickaxe raised high at the top of the swing
- row 3, column 1: `gather_berries` frame 00 - one hand reaching out toward berries beside a basket
- row 3, column 2: `gather_berries` frame 01 - hand back in the basket with berries
- row 3, column 3: `hoe_soil` frame 00 - arms outstretched with the hoe extended away from the body
- row 3, column 4: `hoe_soil` frame 01 - arms pulled in after the hoe stroke while still holding the same farming hoe
- row 4, column 1: `gather_wheat` frame 00 - using a sickle to cut wheat
- row 4, column 2: `gather_wheat` frame 01 - still holding the sickle while the free hand reaches for wheat
- row 4, column 3: `build` frame 00 - kneeling builder with hammer raised up
- row 4, column 4: `build` frame 01 - kneeling builder with hammer down

### Sheet 2 of 3

Use a **4 by 4** grid for this sheet.
Slot target for this sheet: about **256 by 256 px** per cell before crop.

- row 1, column 1: `carry_wood` frame 00 - carrying bundled logs while walking, front/near leg forward
- row 1, column 2: `carry_wood` frame 01 - carrying bundled logs while walking, rear/far leg forward
- row 1, column 3: `carry_gold` frame 00 - carrying stones and gold ore while walking, front/near leg forward
- row 1, column 4: `carry_gold` frame 01 - carrying stones and gold ore while walking, rear/far leg forward
- row 2, column 1: `carry_berries` frame 00 - carrying berries while walking, front/near leg forward
- row 2, column 2: `carry_berries` frame 01 - carrying berries while walking, rear/far leg forward
- row 2, column 3: `carry_wheat` frame 00 - carrying wheat while walking, front/near leg forward
- row 2, column 4: `carry_wheat` frame 01 - carrying wheat while walking, rear/far leg forward
- row 3, column 1: `gather_meat` frame 00 - holding a knife while actively cutting or reaching toward meat
- row 3, column 2: `gather_meat` frame 01 - still holding the knife while taking meat with the free hand
- row 3, column 3: `carry_meat` frame 00 - carrying meat while walking, front/near leg forward
- row 3, column 4: `carry_meat` frame 01 - carrying meat while walking, rear/far leg forward
- row 4, column 1: `club_attack` frame 00 - club at the top of the attack swing, held overhead but still inside the tile
- row 4, column 2: `club_attack` frame 01 - club at the bottom/contact part of the attack swing, still fully inside the tile
- row 4, column 3: `dead` frame 00 - dead villager body lying on the ground with clothing and simple tools still intact
- row 4, column 4: `dead` frame 01 - dead villager body lying on the ground with clothing and simple tools still intact

### Sheet 3 of 3

Use a **2 by 1** grid for this sheet.
Slot target for this sheet: about **256 by 256 px** per cell before crop.
Leave unused cells empty.

- row 1, column 1: `decayed` frame 00 - human skeleton remains, with small clothing and tool scraps still readable
- row 1, column 2: `decayed` frame 01 - human skeleton remains, with small clothing and tool scraps still readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use a flat pure #ff00ff magenta background, with the full paper-cutout sprite inside the square.
- Keep feet, hull base, wheels, siege base, tack, weapons, and equipment anchored consistently across variants.
- Do not add a base, holder, terrain, board, decorative frame, or cast shadow; an extremely subtle contact shadow is acceptable only if it is needed for readability.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Peasant. The footprint is 1 by 1 tile(s). Team colour is required and the recommended preview player colour is blue (#00AFFF). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one sprite frame for each of the 34 listed frame slots. Since there are more than 16 frame slots, split them across multiple images, each image using a 4 by 4 grid. Order slots left to right and top to bottom within each sheet. Keep the subject consistent across every slot. Aim for about 256 by 256 px per sheet slot before crop; larger slots are fine if the sheet grid is clean. Keep the accepted runtime crop's longest side at least 128 px. Use a flat pure #ff00ff magenta sheet background and clear gutters between cells. Use clean readable tiny paper-cutout sprite proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. If unit reference images are supplied, use them only for equipment and silhouette cues, then redraw into stylized Realm sprite art; do not copy their source style or pixels.

Slot order:
- Sheet 1 of 3: 4 by 4 grid
  - row 1, column 1: `idle` frame 00 - relaxed idle, arms at sides, both feet planted
  - row 1, column 2: `idle` frame 01 - long-idle hold pose, arms crossed, both feet planted
  - row 1, column 3: `walk` frame 00 - walking gait with the front/near leg forward and the rear/far leg back
  - row 1, column 4: `walk` frame 01 - walking gait with the rear/far leg forward and the front/near leg back
  - row 2, column 1: `chop_wood` frame 00 - axe at the bottom/contact part of the chop, axe head low and forward
  - row 2, column 2: `chop_wood` frame 01 - axe raised high at the top of the swing
  - row 2, column 3: `mine_gold` frame 00 - pickaxe at the bottom/contact part of the mining swing, pick head low and forward
  - row 2, column 4: `mine_gold` frame 01 - pickaxe raised high at the top of the swing
  - row 3, column 1: `gather_berries` frame 00 - one hand reaching out toward berries beside a basket
  - row 3, column 2: `gather_berries` frame 01 - hand back in the basket with berries
  - row 3, column 3: `hoe_soil` frame 00 - arms outstretched with the hoe extended away from the body
  - row 3, column 4: `hoe_soil` frame 01 - arms pulled in after the hoe stroke while still holding the same farming hoe
  - row 4, column 1: `gather_wheat` frame 00 - using a sickle to cut wheat
  - row 4, column 2: `gather_wheat` frame 01 - still holding the sickle while the free hand reaches for wheat
  - row 4, column 3: `build` frame 00 - kneeling builder with hammer raised up
  - row 4, column 4: `build` frame 01 - kneeling builder with hammer down
- Sheet 2 of 3: 4 by 4 grid
  - row 1, column 1: `carry_wood` frame 00 - carrying bundled logs while walking, front/near leg forward
  - row 1, column 2: `carry_wood` frame 01 - carrying bundled logs while walking, rear/far leg forward
  - row 1, column 3: `carry_gold` frame 00 - carrying stones and gold ore while walking, front/near leg forward
  - row 1, column 4: `carry_gold` frame 01 - carrying stones and gold ore while walking, rear/far leg forward
  - row 2, column 1: `carry_berries` frame 00 - carrying berries while walking, front/near leg forward
  - row 2, column 2: `carry_berries` frame 01 - carrying berries while walking, rear/far leg forward
  - row 2, column 3: `carry_wheat` frame 00 - carrying wheat while walking, front/near leg forward
  - row 2, column 4: `carry_wheat` frame 01 - carrying wheat while walking, rear/far leg forward
  - row 3, column 1: `gather_meat` frame 00 - holding a knife while actively cutting or reaching toward meat
  - row 3, column 2: `gather_meat` frame 01 - still holding the knife while taking meat with the free hand
  - row 3, column 3: `carry_meat` frame 00 - carrying meat while walking, front/near leg forward
  - row 3, column 4: `carry_meat` frame 01 - carrying meat while walking, rear/far leg forward
  - row 4, column 1: `club_attack` frame 00 - club at the top of the attack swing, held overhead but still inside the tile
  - row 4, column 2: `club_attack` frame 01 - club at the bottom/contact part of the attack swing, still fully inside the tile
  - row 4, column 3: `dead` frame 00 - dead villager body lying on the ground with clothing and simple tools still intact
  - row 4, column 4: `dead` frame 01 - dead villager body lying on the ground with clothing and simple tools still intact
- Sheet 3 of 3: 2 by 1 grid
  - row 1, column 1: `decayed` frame 00 - human skeleton remains, with small clothing and tool scraps still readable
  - row 1, column 2: `decayed` frame 01 - human skeleton remains, with small clothing and tool scraps still readable
