# Militia Image Generation Prompt

Generate one Realm sprite reference sheet per direction for **Militia**.

## Art Brief

- Source role: melee infantry
- Visual design: Swordsman with simple helmet, round shield in team color, rough gambeson in team color
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 1 by 1 tile(s)
- Directions: front, back
- Team colour required: yes

## Team Colour Slots

- shield face
- tabard stripe
- plume

## Player Colour

- Use blue (#00AFFF) for the player-colour areas listed above.
- Add a white diagonal stripe running from top left to bottom right on the shield face, and the tabard stripe.

## Research Visual Tiers

Generate the complete state set for each equipment tier below.
- Starting equipment (default, no research required): starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement.
- After Iron Weapons research: upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims.

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

Generate **one frame for each state**. There are 14 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

### Sheet

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `basic_weapons__idle` - Basic weapons: idle; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 1, column 2: `basic_weapons__walk` - Basic weapons: walk; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 1, column 3: `basic_weapons__attack_swing_thrust` - Basic weapons: attack swing/thrust; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 1, column 4: `basic_weapons__hold_position` - Basic weapons: hold-position; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 2, column 1: `basic_weapons__hit_alert` - Basic weapons: hit/alert; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 2, column 2: `basic_weapons__dead` - Basic weapons: dead human body lying on the ground with clothing, armour, weapons, and equipment still intact; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 2, column 3: `basic_weapons__decayed` - Basic weapons: human skeleton remains, with armour, weapons, and equipment still intact and readable; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
- row 2, column 4: `iron_weapons__idle` - Iron Weapons: idle; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 3, column 1: `iron_weapons__walk` - Iron Weapons: walk; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 3, column 2: `iron_weapons__attack_swing_thrust` - Iron Weapons: attack swing/thrust; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 3, column 3: `iron_weapons__hold_position` - Iron Weapons: hold-position; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 3, column 4: `iron_weapons__hit_alert` - Iron Weapons: hit/alert; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 4, column 1: `iron_weapons__dead` - Iron Weapons: dead human body lying on the ground with clothing, armour, weapons, and equipment still intact; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
- row 4, column 2: `iron_weapons__decayed` - Iron Weapons: human skeleton remains, with armour, weapons, and equipment still intact and readable; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Militia. The footprint is 1 by 1 tile(s). Team colour is required and the recommended preview player colour is blue (#00AFFF). Valid directions are front, back. Produce one sheet at a time for the requested direction, using the same state grid for each direction. Create one frame for each of the 14 listed states. Order states left to right and top to bottom within each sheet. Keep the character or building consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork.

Slot order:
- Grid: 4 by 4
  - row 1, column 1: Basic weapons: idle; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 1, column 2: Basic weapons: walk; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 1, column 3: Basic weapons: attack swing/thrust; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 1, column 4: Basic weapons: hold-position; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 2, column 1: Basic weapons: hit/alert; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 2, column 2: Basic weapons: dead human body lying on the ground with clothing, armour, weapons, and equipment still intact; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 2, column 3: Basic weapons: human skeleton remains, with armour, weapons, and equipment still intact and readable; starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement
  - row 2, column 4: Iron Weapons: idle; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 3, column 1: Iron Weapons: walk; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 3, column 2: Iron Weapons: attack swing/thrust; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 3, column 3: Iron Weapons: hold-position; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 3, column 4: Iron Weapons: hit/alert; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 4, column 1: Iron Weapons: dead human body lying on the ground with clothing, armour, weapons, and equipment still intact; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
  - row 4, column 2: Iron Weapons: human skeleton remains, with armour, weapons, and equipment still intact and readable; upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims
