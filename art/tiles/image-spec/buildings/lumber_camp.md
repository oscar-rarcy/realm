# Lumber Camp Image Generation Prompt

Generate a Realm sprite reference sheet for **Lumber Camp**.

## Art Brief

- Source role: wood drop-off
- Visual design: Log piles, saw frame, chopping block, shed
- Projection: upright sprite anchored over projected isometric map tiles
- Footprint: 2 by 2 tile(s)
- Directions: south
- Team colour required: yes

## Team Colour Slots

- small camp banner

## Player Colour

- Use blue (#00AFFF) for the player-colour areas listed above.

## Simplified Building Style Contract

- Use a highly simplified medieval storybook building style that belongs to the map, not a movable paper standee.
- Do not add a cream paper border, die-cut edge, sticker outline, holder, base, or freestanding paper-cutout treatment.
- Use bold readable silhouettes, flat muted painted colour areas, and chunky simplified linework only where it clarifies rooflines, doors, windows, banners, damage, construction, or production state.
- Keep the structure grounded into its footprint with subtle contact darkening, dirt, cobble, or yard cues where appropriate.
- Avoid realistic rendering, tiny architectural ornament, thin hatching, glossy digital polish, and noisy texture.
- Team-colour areas remain maskable accents such as banners, awnings, flags, shield signs, or roof trim; do not flood the building body with team colour.

## Direction And Anchor Contract

- `south` means the building is drawn in the Realm isometric three-quarter view, with the readable front facing down-screen/right enough to match the map perspective.
- Keep the footprint visually centred on the tile footprint listed above. Larger buildings may fill their footprint, but should not look like a full terrain tile.
- Use a consistent ground-contact baseline and shadow direction across construction, completed, damaged, garrisoned, production, weather, and ruin states.

## Aspect Ratio

- Image generation preset: Square (1:1).
- Use this aspect ratio for the whole contact sheet; crop accepted slots into production sprites after review.

## Output Resolution

- Final accepted standalone source canvas: 32 by 32 px.
- Use this resolution from the generated JSON spec for every accepted standalone sprite frame; contact-sheet slots may be larger, but each slot must be cleanly crop/downscale-safe to 32 by 32 px.

## Image Output Contract

- Output kind: reference contact sheet for planning and review.
- Per-cell target: one complete sprite frame matching the listed state, centred in its grid cell.
- Background: Use transparent background, or a single flat #ff00ff magenta background if transparency is not available.
- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.
- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.
- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.
- Team colour: Use the recommended preview player colour blue (#00AFFF) only in deliberate maskable areas such as banners, shields, cloth trim, pennants, sails, or painted markers. Keep skin, stone, wood, shadows, weapons, animals, and cargo out of team colour.
- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.

## Entity-Specific Art Notes

- Draw one coherent building design across every state; construction, damaged, garrisoned, training, weather, and ruin states should all visibly derive from the same structure.
- Do not bake a full square terrain tile into the building art. A small contact shadow and immediate footprint dirt are acceptable.
- Keep doors, banners, roofline, walls, team-colour markers, and silhouette readable at small RTS scale.
- Production or research states should add visible activity cues such as banners, lit windows, work glow, smoke, open doors, or small queue markers without becoming UI icons.

## States To Generate

Generate **one frame for each state**. There are 13 state(s). Each image may contain at most **16 states** in a **4 by 4** grid.

Environment states are generated only for the completed building. Do not make a full cross-product of construction, damaged, garrisoned, and weather states.
Night states should add visible warm light sources; broad nighttime dimming can still be handled by the renderer.

### Sheet

Use a **4 by 4** grid for this sheet.

- row 1, column 1: `complete` - complete
- row 1, column 2: `active_deposit` - active/deposit
- row 1, column 3: `construction` - construction
- row 1, column 4: `ruin_footprint` - ruin footprint
- row 2, column 1: `construction_0_foundation` - 0-33 percent construction: foundation footprint and early site materials
- row 2, column 2: `construction_1_frame` - 34-66 percent construction: visible frame and scaffolding
- row 2, column 3: `construction_2_nearly_complete` - 67-99 percent construction: nearly complete shell with final work visible
- row 2, column 4: `damaged` - damaged building below half HP, readable but not destroyed
- row 3, column 1: `night_lit` - completed building at night with warm torch, candle, forge, or window light; keep team colour readable
- row 3, column 2: `rain_frame_1` - completed building in rain, wet roof/ground and drip or splash detail frame 1
- row 3, column 3: `rain_frame_2` - completed building in rain, wet roof/ground and drip or splash detail frame 2
- row 3, column 4: `snow_light` - completed building with light snow on roof edges, ledges, and ground contact
- row 4, column 1: `snow_heavy` - completed building with heavy settled snow while silhouette and team colour stay readable

## Production Follow-Up

- Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.
- Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.
- Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.
- Treat the sheet as the visual decision record; generate or crop final production sprite frame images only after the sheet slot is accepted.


## Prompt

Generate sprites for my Realm Lumber Camp. The footprint is 2 by 2 tile(s). Team colour is required and the recommended preview player colour is blue (#00AFFF). Use south direction artwork. Create one frame for each of the 13 listed states. Order states left to right and top to bottom within each sheet. Keep the subject consistent across every slot. Final accepted standalone frames use the generated spec resolution: 32 by 32 px. Use transparent background, or a single flat #ff00ff magenta background if transparency is not available. Use clean readable simplified medieval painted-building proportions, stable anchor, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. 

Slot order:
- Grid: 4 by 4
  - row 1, column 1: complete
  - row 1, column 2: active/deposit
  - row 1, column 3: construction
  - row 1, column 4: ruin footprint
  - row 2, column 1: 0-33 percent construction: foundation footprint and early site materials
  - row 2, column 2: 34-66 percent construction: visible frame and scaffolding
  - row 2, column 3: 67-99 percent construction: nearly complete shell with final work visible
  - row 2, column 4: damaged building below half HP, readable but not destroyed
  - row 3, column 1: completed building at night with warm torch, candle, forge, or window light; keep team colour readable
  - row 3, column 2: completed building in rain, wet roof/ground and drip or splash detail frame 1
  - row 3, column 3: completed building in rain, wet roof/ground and drip or splash detail frame 2
  - row 3, column 4: completed building with light snow on roof edges, ledges, and ground contact
  - row 4, column 1: completed building with heavy settled snow while silhouette and team colour stay readable
