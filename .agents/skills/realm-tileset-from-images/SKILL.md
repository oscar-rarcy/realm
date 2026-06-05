---

name: realm-tileset-from-images
description: "Create Realm tileset assets from pasted or generated images across all visual lanes: grounds, decals, features, units, animals, ships, siege engines, buildings, projectiles, effects, tactical overlays, and UI markers. Use when Codex needs to classify an asset type, generate or refine image prompts, store generated candidates, split source sheets, process upright sprites into runtime frames and team-colour masks, ingest full-square top-down tile art, update manifests, or run visual QA for projection, anchors, footprints, masks, crops, fallbacks, and animation metadata."
---

# Realm Tileset From Images

## Purpose

Use this skill for Realm tileset asset production and ingestion, not for general image editing.

The first job is always to classify the requested asset lane. Do not assume the asset is a unit sprite.

Realm visual assets fall into these broad families:

* **Tile-space art**: grounds and most flat decals. These are authored as square top-down images.
* **Upright world-space art**: features, units, animals, ships, siege engines, buildings, projectiles, and many effects. These are transparent sprites or overlays anchored in the world.
* **Renderer colour effects**: fog, night, storm dimming, explored/unexplored tint, concealment tint, and team-colour remapping. These are not generated as base art.

Every map square has exactly one ground. Features, decals, entities, overlays, and effects are optional layers above it.

## Need-To-Know Policy

Be strict about what each audience needs to know.

Codex may need renderer details, paths, manifests, anchors, footprints, commands, fallback rules, and QA checks.

The image generator does not need most of that.

Image-generation prompts should contain only the visual facts needed to draw the requested image correctly.

Do not put internal pipeline details into image prompts unless they directly affect the pixels.

For ground prompts, do not mention runtime projection, isometric projection, diamonds, skewing, renderer transforms, manifests, or tile engine behaviour. The prompt should simply ask for a top-down square ground tile.

For actor prompts, mention only the visible action, pose, angle, equipment, background requirement, and team-colour slots. Do not include command names, file paths, runtime paths, manifest fields, or QA language.

For feature, decal, building, projectile, effect, and UI prompts, include only the visible asset type, background, orientation, state, and any visual constraints that matter.

## Image Generation Coordination

Use the `imagegen` skill whenever Realm tileset work needs generated or edited bitmap pixels.

### Non-Negotiable Production Rules

For Realm production tileset generation, do not improvise image-generation prompts.

Always start from the current canonical exports:

```powershell
python scripts\tileset_coverage.py next --limit 12 --out build\tileset-next-batch.json
```

For every work item or grouped sheet, open the listed generated Markdown prompt under `art/tiles/image-spec/` and generated JSON spec under `art/tiles/image-json/`. Use those exports as the prompt source of truth. If the exported prompt is insufficient, fix the exporter or the canonical docs first, regenerate the exports, and then generate art from the regenerated prompt. Do not silently replace the exported prompt with an ad-hoc prompt.

For every generated batch, create and retain provenance before accepting it:

* prompt file path and hash,
* JSON spec path and hash,
* exact generator prompt text actually sent,
* generated image source path under `C:\Users\Edward\.codex\generated_images\...`,
* candidate sheet path under `art/tiles/candidates/...`,
* prebuilt grid manifest path when a sheet is used,
* visible positive reference image paths and their roles,
* negative/rejected reference paths excluded from the prompt,
* split/crop manifest or fixed slot map,
* alpha and residual-magenta QA result,
* style-review status.

Do not call `scripts\tileset_coverage.py accept` for newly generated art unless the provenance exists and the style review is accepted. Coverage acceptance means runtime production acceptance, not merely file existence.

For local image edits with the built-in image generator, first make every base/edit target and every positive reference visible with `view_image`. The built-in image edit path works from images already visible in the conversation context.

For edits, label image roles explicitly before prompting:

* Base/edit target: authoritative for crop, canvas, slab geometry, side shape, anchor, unchanged pixels, and any sheet layout.
* Material/style reference: borrow only the named material, palette, brush style, or state.
* Geometry reference: borrow only slab shape, side faces, corner wear, and contact-shadow structure.
* Pose/equipment reference: borrow only stance, silhouette, equipment, weapon tier, armour tier, shield placement, horse tack, or operator posture.
* Negative reference: diagnostic only; do not copy its pixels, style, palette, crop, pose, or layout.

Do not ask Image Gen to "make this like that" without image roles. Do not use rejected candidates as positive references.

Copy accepted outputs from `C:\Users\Edward\.codex\generated_images\...` into `art/tiles/candidates/...` before they become project evidence or runtime assets. Rejected smoke outputs can remain transient unless they are useful diagnostic evidence.

After a candidate is reviewed and accepted, copy the runtime-ready image into `assets/tiles/...` so the actual game can load it. A candidate under `art/tiles/candidates/...` is evidence, not a live game asset.

### Prebuilt Grid Requirement

For sprite/contact sheets, programmatically create the grid before using Image Gen. Do not rely on text-only instructions that ask the model to invent the grid.

The default sheet is a 4 by 4 grid. If an asset needs more than 16 states or variants, split it into multiple 4 by 4 sheets. Use a smaller fixed grid only when the canonical exported prompt or slot map explicitly requires it, and record that exception in provenance.

The grid should be an image-edit base target:

* pure magenta cell backgrounds for actor/projectile sprites that will be alpha-cleaned,
* white or neutral gutters around magenta cells when that improves slot separation,
* no labels, numbers, or text inside the generated pixels,
* exact slot coordinates saved in a JSON manifest,
* deterministic crop boxes used for promotion.

If the output adds extra rows/columns, merges cells, moves sprites outside cells, adds labels, or changes the sheet geometry enough that the manifest crop boxes are no longer valid, reject it or keep it only as diagnostic evidence.

### Style Seed Cascade

For complex or style-sensitive lanes, generate the visual identity before generating the full sheet.

Default sequence:

1. Choose the closest accepted style ancestor. Examples: peasant before militia, archer before crossbowman, boar/deer/sheep/wolf before related animals, fishing boat before other small boats, catapult/ram before other siege.
2. Generate or edit one 1024 by 1024 standalone identity image in the exact target style.
3. Review that identity image as the style seed.
4. Programmatically place the accepted seed into the first or idle slot of the prepared grid.
5. Use Image Gen edit mode to fill the remaining states while preserving the seed's outline weight, palette, simplification level, paper border or map-art finish, scale, and anchor.
6. Promote only after the sheet passes grid, alpha, mask, and style QA.

This organic branching workflow is preferred over generating unrelated assets independently. Always work from the closest successful visual neighbor when one exists.

### Style Contract Gate

Before any production generation, identify the lane's current Realm style contract and the closest accepted style ancestor. If no style contract exists for the lane, write or update the canonical style guidance before generating.

The style contract should answer:

* Is this lane a paper-cutout standee, a map-integrated painted feature, a top-down ground surface, a decal/mark, a projectile cutout, or UI/effect overlay?
* Which accepted asset is the closest positive style seed?
* Which reference images are style references, and which are only equipment, pose, or geometry references?
* Which visual traits are mandatory: outline weight, cream paper edge, palette, simplification level, shadow policy, alpha policy, and team-colour policy?
* Which traits are explicitly rejected?

If the exported prompt and the style contract disagree, stop and correct the exporter or docs. Do not let ad-hoc prompt text resolve the conflict.

## Zoom-Stop Sprite Sheets

Use a zoom-stop sheet when a high-resolution upright sprite looks good at 1024 by 1024 but becomes blurry or visually over-detailed after normal runtime scaling.

This is an art-generation workbench step with an optional runtime override contract. Build the sheet under `art/tiles/workbench/...`, send it to Image Gen as an edit target, split accepted stops into candidates or workbench outputs, then promote reviewed outputs into runtime assets.

Prepare the sheet with the repo helper so the sizes match the current SDL zoom math:

```powershell
python scripts\prepare_zoom_stop_sprite_sheet.py prepare `
  --source art\tiles\candidates\units\peasant\source.png `
  --out art\tiles\workbench\zoom-stops\peasant\idle_front_00\input_sheet.png `
  --subject "peasant idle front frame 00" `
  --asset-profile human
```

By default, the helper reads the tileset zoom ladder directly from `src/render/sdl/camera.cpp` and emits one sprite for every renderer zoom stop. The current tileset ladder has 16 stops across tile zoom 14 to 288. The helper writes a 1024 by 1024 magenta sheet, a JSON manifest with each stop box, and a Markdown Image Gen prompt. The sheet uses packed variable cells ordered from largest to smallest; the largest zoom stop is also the visual identity reference.

Image Gen instructions for this lane should say that each stop must be redrawn natively at its exact pixel size rather than resized from the largest stop. Keep magenta outside the sprite pixels, preserve the stance anchor, and do not add labels, terrain, UI, or extra sprites.

Use need-to-know prompt profiles instead of one universal prompt. `--asset-profile auto` infers a profile from the subject and path, but pass an explicit profile when the asset category is known:

```text
human, animal, building, terrain, decal, projectile, effect, generic
```

Human prompts include face and eye readability rules. Animal prompts include species, head, eye, stance, and medieval-bestiary readability rules. Building prompts prioritize silhouette, roofline, entrances, structural masses, and team-colour markers. Terrain/decal/projectile/effect prompts omit human-only face and equipment rules.

After Image Gen returns an edited sheet, crop the exact stop boxes with:

```powershell
python scripts\prepare_zoom_stop_sprite_sheet.py split `
  --sheet art\tiles\workbench\zoom-stops\peasant\idle_front_00\edited_sheet.png `
  --manifest art\tiles\workbench\zoom-stops\peasant\idle_front_00\input_sheet.manifest.json `
  --out-dir art\tiles\candidates\zoom-stops\peasant\idle_front_00 `
  --transparent
```

If Image Gen changes the canvas size or moves the sprites away from the manifest boxes, use component detection instead:

```powershell
python scripts\prepare_zoom_stop_sprite_sheet.py split `
  --sheet art\tiles\workbench\zoom-stops\peasant\idle_front_00\edited_sheet.png `
  --manifest art\tiles\workbench\zoom-stops\peasant\idle_front_00\input_sheet.manifest.json `
  --out-dir art\tiles\workbench\zoom-stops\peasant\idle_front_00\split-detected `
  --transparent --normalize-canvas --detect-components `
  --component-tolerance 90 --remove-magenta-spill --alpha-threshold 12
```

Promote reviewed stop sprites into the runtime convention:

```powershell
python scripts\prepare_zoom_stop_sprite_sheet.py promote `
  --split-manifest art\tiles\workbench\zoom-stops\peasant\idle_front_00\split-detected\split_manifest.json `
  --entity peasant --action idle --direction front --frame 0 --kind base --force
```

Runtime zoom-stop files are optional overrides beside the normal frame:

```text
assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_zoom_NNN_base.png
assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_zoom_NNN_teammask.png
```

Use `status` to track whether base and team-mask stops exist:

```powershell
python scripts\prepare_zoom_stop_sprite_sheet.py status `
  --entity peasant --action idle --direction front --frame 0 `
  --out art\tiles\workbench\zoom-stops\status-peasant-idle-front-00.json `
  --markdown-out art\tiles\workbench\zoom-stops\status-peasant-idle-front-00.md `
  --fail
```

The runtime loader uses an exact stop for the requested draw size and falls back to the normal frame when that exact stop does not exist. Missing or partial zoom-stop sets must never make an entity blank or switch random frames to a different scale.

## Generated Reference Images

The skill may generate reference images before generating production sprites or tiles.

Generated references are useful when the spec describes an asset well but the generator would benefit from visual neighbourhood context, such as nearby units, similar equipment, related terrain materials, or prior generated style experiments.

Generated references are not final runtime art. They are reference-only inputs for later generation, review, or comparison.

Storage rules:

* Never write generated reference images into `art/reference/`. That folder is user-authored source reference material.
* Store generated references under `art/generated-reference/<lane>/<slug>/<version>/`.
* Temporary context grids and prompts may go under `build/generated-reference-context/...`.
* If a generated reference later becomes a production candidate, copy it separately into `art/tiles/candidates/...`; do not promote directly from `art/generated-reference/...` to runtime.
* If the generated reference is rejected, leave it out of future positive-reference prompts.

Need-to-know rule for generated-reference prompts:

* Include the generated spec summary, visual identity, key equipment/material/state facts, and output role.
* Include only nearby visual context that helps draw the reference.
* Do not include runtime manifest paths, renderer internals, or final asset-processing instructions.

For unit references, prefer a 4 by 4 visual context grid made from nearby user-supplied references, such as the same unit, similar infantry, similar cavalry, bow/crossbow/quiver references, or horse/tack references. The grid is context only. Tell the image generator not to copy its pixels, lighting, crop, or style.

Use the helper to prepare a prompt and optional 4 by 4 context grid:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\reference_images.py prepare `
  --group units --slug militia --version v001 `
  --context-root art\reference\units
```

Then make the prompt and context grid visible before calling image generation:

```powershell
Get-Content build\generated-reference-context\units\militia\v001\prompt.md
```

After image generation, store the keeper as a generated reference:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\reference_images.py store `
  --group units --slug militia --version v001 `
  --input C:\Users\Edward\.codex\generated_images\...\chosen.png `
  --prompt-file build\generated-reference-context\units\militia\v001\prompt.md
```

## Repository Boundary

Keep final game assets and generation material separate.

* `assets/tiles/` is runtime-facing. Only put files here when the game can load them directly.
* `art/tiles/` is the art-production workspace. Put candidates, workbench frames, provenance, and metadata here.
* `art/tiles/image-spec/` and `art/tiles/image-json/` are generated from game data, docs, and exporters. Treat them as source-of-truth prompt/spec exports, not reference-image storage. If these exports move later, prefer a spec-only home such as `art/specs/` and update the exporters/docs together.
* `art/tiles/reference/<lane>/` is the older lane reference/template area for durable tileset references and audits. Do not add newly generated reference images here unless the user explicitly asks for that legacy lane layout.
* `art/reference/` is the committed user-supplied reference library. Treat it as read-only user source material. Do not create, overwrite, clean up, or reorganize files under `art/reference/` unless the user explicitly asks for an edit to that exact folder.
* `art/reference/units/` contains user-supplied unit references. Use those images only for gear, equipment, weapon tier, armour tier, shield placement, horse tack, and broad silhouette cues. They are not final sprites, not runtime assets, and not the Realm style target.
* `art/generated-reference/<lane>/<slug>/<version>/` is for reference images generated by this skill from specs and optional visual context. These are reference-only outputs, not runtime assets and not user-supplied source references.
* `build/tileset-*` is temporary review or prompt output.
* `C:\Users\Edward\.codex\generated_images\...` is transient generator output. Copy keepers into `art/tiles/candidates/...` before using them.

Do not put `candidates`, `workbench`, `source`, `reference`, `generated-reference`, `image-spec`, or `image-json` under `assets/tiles/`.

Do not point runtime manifests or code at `art/tiles/`.

Do not point runtime manifests or code at `art/generated-reference/`.

Do not mark generated art as accepted runtime art until the accepted runtime-ready copy exists under `assets/tiles/...`.

Do not recreate `.agents/skills/realm-tileset-from-images/references`; prompt copy belongs in the repo exporter and regenerated `art/tiles/image-spec/` files.

## Sources Of Truth

Before deciding states, footprints, anchors, projection, or team-colour slots, inspect the current sources.

Use this order:

1. Current game code and runtime enums.
2. `docs/tileset/realm_visual_asset_architecture.md`.
3. `docs/tileset/realm_tileset_visual_audit.md`.
4. `docs/tileset/realm_animation_state_mechanics_audit.md`.
5. Generated Markdown under `art/tiles/image-spec/`.
6. Generated JSON under `art/tiles/image-json/`.
7. C++ `--dump-animation-spec` only for targeted unit animation behaviour checks.

Generated Markdown is the human-readable prompt surface.

Generated JSON is the lower-level source for asset type, action, direction, path, projection, anchor, footprint, state, mask, and helper-command details.

If docs and generated JSON disagree, report the mismatch. Treat docs as design intent and JSON as current exporter/runtime state.

After changing game data, terrain/entity definitions, tileset docs, or prompt exporters, regenerate specs:

```powershell
python -m py_compile scripts\export_image_generation_prompts.py scripts\export_tile_specs.py
python scripts\export_tile_specs.py --clean
python scripts\export_image_generation_prompts.py --clean
```

## Generate-All Planner

When the user asks to generate all Realm tileset assets, do not hand-build the global asset list. Start with the deterministic coverage planner:

```powershell
python scripts\tileset_coverage.py report --format md --out build\tileset-coverage.md
python scripts\tileset_coverage.py next --limit 12 --out build\tileset-next-batch.json
```

Use `--refresh-specs` only when the user wants current code/docs re-exported first, because it rewrites `art/tiles/image-json/` and `art/tiles/image-spec/`.

Treat `build/tileset-next-batch.json` as the work queue. For each item, open the listed JSON spec and Markdown prompt, generate or ingest the lane-correct candidate, promote only reviewed runtime-ready files to the listed `assets/tiles/...` paths, then record acceptance:

```powershell
python scripts\tileset_coverage.py accept --work-id <work-id> --review <review-note-or-artifact>
```

Do not use coverage acceptance as a shortcut around visual review. For newly generated art, acceptance requires:

* canonical exported prompt/spec were used,
* positive references or style seeds were shown to Image Gen when available,
* prebuilt grid manifest was used for sheet generation,
* runtime files are alpha-clean for sprites, overlays, decals, and features,
* residual opaque magenta is below the lane threshold,
* team-mask pixels are limited to intended team-colour slots,
* runtime paths are actually loadable by the current renderer or explicitly documented as future/not-wired,
* style review passed against the lane's canonical style contract,
* provenance file records the above evidence.

Programmatic QA should fail actor, feature, decal, projectile, effect, and UI assets that retain substantial opaque magenta after promotion. Full-square ground art is the main exception, and ground QA must instead check that the image satisfies the loader's expected source size and top-down tile contract.

If an asset lane is not currently drawn by the game, do not call it complete merely because files exist. Mark it as future/not-wired in the audit or coverage tooling until a renderer path and screenshot smoke proof exist.

After each batch, rerun the report and request the next batch. Do not claim the full tileset is complete until:

```powershell
python scripts\tileset_coverage.py verify --strict
```

passes. `missing`, `placeholder`, `stale`, `needs_review`, `unsupported`, and `unreachable` are not complete statuses.

Strict coverage is necessary but not sufficient for production art quality. Before claiming visual completion, also inspect the generated review sheets and runtime samples for style consistency, crop quality, alpha cleanup, mask correctness, and in-game visibility.

## Asset Lanes

| Lane               | Meaning                                                          | Source image contract                         | Runtime placement            |
| ------------------ | ---------------------------------------------------------------- | --------------------------------------------- | ---------------------------- |
| Ground             | Required floor material                                          | Full top-down square tile                     | Tile-space ground            |
| Surface decal      | Flat mark on ground                                              | Transparent top-down square overlay           | Tile-space overlay           |
| Semi-upright decal | Low non-interactable vegetation/detail                           | Transparent square overlay with slight height | Low overlay                  |
| Feature            | Gameplay object attached to a tile                               | Transparent upright sprite                    | Anchored over tile centre    |
| Unit               | Player mobile actor                                              | Transparent upright action sprite             | Anchored over tile centre    |
| Animal             | Neutral/hostile mobile actor                                     | Transparent upright action sprite             | Anchored over tile centre    |
| Ship/siege         | Mobile actor that may overhang                                   | Transparent upright action sprite             | Anchored over tile centre    |
| Building           | Static entity with footprint                                     | Sliced footprint prefab or building sprite    | Anchored to footprint origin |
| Projectile         | Moving tactical object                                           | Transparent small sprite                      | World-position anchor        |
| Effect/UI          | Impact, marker, weather, selection, range, preview, rally, alert | Transparent overlay or UI sprite              | Tile, world, or screen space |
| Colour effect      | Fog, night, tint, storm dimming, team remap                      | Not generated as base art                     | Renderer transform           |

## Draw Order

Target layer order:

1. Ground tile.
2. Ground transitions and autotile edges.
3. Ground decals.
4. Feature shadow.
5. Feature back/contact sprite.
6. Feature state overlay.
7. Entity/building shadow.
8. Entity/building base sprite.
9. Team-colour mask.
10. Entity/building state overlay.
11. Feature front/occluder sprite.
12. Effects/UI.
13. Fog, explored, night, storm, and other colour effects.

Do not bake later layers into earlier layers unless explicitly creating a temporary composite.

## Golden Path

1. Classify the asset lane.
2. Open the matching `art/tiles/image-spec/...` prompt if available.
3. Open the matching `art/tiles/image-json/...` spec when projection, anchor, states, paths, masks, or footprints matter.
4. Open the lane reference audit and production-status file when they exist.
5. Decide whether the target is accepted, candidate-only, reference-only, placeholder-only, or missing.
6. Use the lane-specific source image contract.
7. Write or generate a prompt that only includes need-to-know visual instructions.
8. Store any keeper image under `art/tiles/candidates/...`.
9. Split sheets only when they are generation intermediates or reference sheets.
10. Process according to lane. Do not run actor sprite processing on ground tiles.
11. After review acceptance, write final runtime files only under `assets/tiles/...` so the game uses them.
12. Run lane-specific QA.
13. Update the lane production-status file, reference audit, and notes when the status or reusable knowledge changes.
14. Report unsupported helper behaviour instead of forcing an asset through the wrong pipeline.

## Projection Metadata

Use generated JSON projection metadata for internal reasoning and runtime placement.

Do not copy projection metadata into image prompts unless the image generator needs it to draw the pixels correctly.

| Projection           | Internal meaning             | Prompt wording                       |
| -------------------- | ---------------------------- | ------------------------------------ |
| `surface_decal`      | Flat tile-space overlay      | “transparent top-down overlay”       |
| `semi_upright_decal` | Low decal with slight height | “transparent low decorative overlay” |
| `upright_world`      | World-space sprite           | “transparent upright sprite”         |
| `tile_overlay`       | Flat overlay over a tile     | “transparent tile marker/effect”     |
| `screen_ui`          | UI coordinates               | “clear UI icon/overlay”              |

Do not infer projection from the picture alone if generated JSON is available.

## Ground Contract

Use for grass, meadow, dirt, road when exported as ground, mud, sand, dunes, snow, tundra, ice, water, shallows, marsh, gravel, ash, lava, hills, rocky ground, and castle floor.

Grounds are not sprites.

Realm grounds are deliberately visible square 3D terrain slabs for a tile-based RTS. They should not look like invisible seamless terrain textures, but they also must not look like a decorative surround or UI-style edging around a flat texture. The approved grass reference is the target: a single thick, ceramic-like terrain tile seen from above, with a continuous grass top face that turns into chipped, worn, darker side material at the perimeter.

Source rules:

* Generate a full square top-down ground tile, preferably `1024x1024`.
* The whole square is the asset.
* Do not use magenta key background.
* Do not crop around an object.
* Do not resize into a `48x48` sprite canvas.
* Do not add transparent padding.
* Include the Realm ground slab shape: a raised square terrain tile with a top face, subtle bevel, chipped/worn side faces, worn corners, and a dark contact shadow outside the slab.
* The edge must be part of the physical terrain tile, like a worn ceramic or stone game-board square. Do not draw a separate outline, rim, decorative surround, UI-style edging, or trim around the material.
* Keep the side faces muted and material-coloured for the specific ground material and state. The approved grass reference is the geometry/style reference, not a universal side-colour reference.
* Do not make the slab sides bright gold, yellow, glowing, clean, high-contrast, or copied from an unrelated reference material.
* The top terrain surface should run naturally into the bevel. Do not draw an inner rectangle or inset line between the top surface and the side faces.
* Make adjacent tiles read as separate raised terrain slabs. Do not make invisible seamless texture joins unless the user explicitly asks for a non-Realm texture.
* Keep the material identity readable as a top-down terrain surface.
* Use separate states/sheets for animation, seasons, weather, or variants only when the spec requires them.

Reference/editing rule:

* If a close approved ground reference exists, prefer editing or strongly preserving that reference with image generation instead of generating from scratch.
* The approved unknown/unseen tile at `art/tiles/reference/grounds/generated/unknown/v001-user-grey-template/source.png` can be used as a neutral slab-shape template for new grounds.
* For ground variants, preserve the reference tile's physical slab geometry: top face, bevel, chipped/worn side faces, worn corners, and dark contact shadow. Change only the terrain material/state on the slab unless the task is specifically to redesign the slab shape.
* Do not call the slab edge a border in image-generation prompts. That wording tends to make the generator draw a decorative outline around the terrain instead of the approved thick 3D ground tile.
* The `blank-*.png` ground references are neutral slab geometry references only. Use them to lock crop, slab shape, chipped side faces, worn corners, and contact shadow, not as colour or material targets unless the desired ground is grey stone.

Ground image prompts must say only what the image generator needs:

* Top-down square ground tile.
* Full tile surface fills the image.
* Raised square terrain slab matching the closest approved reference.
* Continuous top terrain surface that turns into chipped, worn side material at the edges.
* Worn corners and dark contact shadow outside the slab.
* Muted side faces, not bright gold/yellow edging.
* Ground-specific faded side material: snow uses snow-white/blue-grey sides, water uses deep/translucent-looking blue wet sides, paving wraps stone/paver edges into the bevel, lava uses dark volcanic rock with restrained fissure glow, and so on.
* No inner rectangle, inset line, or separate outline between the top surface and the sides.
* No decorative surround, UI-style edging, outline, rim, or trim.
* No perspective view.
* No horizon.
* No upright object.
* No character, building, prop, label, or UI marker.

Ground image prompts must not mention:

* Isometric projection.
* Diamonds.
* Skewing.
* Runtime projection.
* Renderer transforms.
* Tile engine internals.
* Manifests or runtime paths.

A `2x2` or `4x4` generation sheet is allowed for related ground states. Split it into full-square state tiles. Each split tile remains a full square.

For a single standalone ground tile, use the same slab contract without sheet language:

* If the requested single tile already has an approved reference, such as grass at `art/tiles/reference/grounds/examples/grass.png`, do not generate a replacement from scratch. Use the approved reference directly or do a strict edit-preservation pass only when the material/state must change.
* Ask for one top-down square 3D terrain slab.
* Match `art/tiles/reference/grounds/examples/grass.png` for slab geometry unless a closer approved material reference exists.
* Say the terrain top surface should fill the tile and turn into chipped, worn side faces at the perimeter.
* Preserve worn corners, subtle bevel, and dark contact shadow outside the slab.
* Keep the side faces muted and material-coloured; avoid bright gold/yellow edging.
* Keep the top surface continuous into the bevel; avoid an inner rectangle or inset line.
* Do not ask for a decorative surround, outline, rim, trim, UI-style edging, or seamless texture.

For seasonal or material variants of an accepted ground tile, a useful pattern is:

1. Start from the closest accepted full-size source tile.
2. Use `tile_grid.py make-grid` to repeat it into a grid, usually `2x2` or `4x4`.
3. Ask image generation to edit each grid cell into the requested variant while preserving every cell's physical 3D terrain slab shape. This should be an edit-preservation task, not a from-scratch redraw.
4. Use `ground_variants.py ingest` or `tile_grid.py split-grid` to split the returned sheet into standalone square candidates.
5. For accepted-source variants, restore the source slab edge after splitting so Image Gen can alter the top material without inventing new side faces. `ground_variants.py ingest` does this by default with `--restore-source-edge`.
6. Use `tile_grid.py inspect-grid` or the ingest reports for mechanical checks, then visually inspect each split tile for the requested season/material.
7. Regenerate failed slots individually or regenerate the whole sheet, depending on whether the failure is local or the whole batch lost the art direction.

Do not draw labels inside the generated grid. Track slot names in the grid manifest and prompt text instead.

Optional helper route:

Use `scripts/ground_variants.py` when a ground variant batch needs repeatable prep, split, metadata, and QA, but keep using `tile_grid.py` directly for one-off crops or unusual layouts.

Prepare an editable grid and prompt:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py prepare --asset grass --variant-group seasonal
```

The generated prompt is meant for image generation. The helper does not call the generator.

After image generation, ingest the returned sheet:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py ingest --sheet C:\path\to\generated.png --prepare-manifest build\tileset-grids\grass-seasonal-template-2x2.manifest.json --version v001-generated-sheet
```

`ingest` copies the sheet into `art/tiles/candidates/grounds/<asset>/<variant-group>/<version>/`, splits it into slot crops, restores the source slab-edge ring by default, writes `candidate_manifest.json`, writes `notes.md`, runs mechanical grid inspection, and writes source-edge drift QA. If the image generator resized the sheet, `ingest` infers scaled split geometry from the prepare manifest; use `--resize-mode template` only when the generated sheet should be resized back to the prepared template size before splitting.

After visual review accepts a ground candidate for runtime use, promote the reviewed source or split slot into `assets/tiles/grounds/`:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py promote-reviewed --candidate-dir art\tiles\candidates\grounds\grass\spring\v003-reference-match
```

For a reviewed split sheet slot, choose the slot explicitly:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py promote-reviewed --candidate-dir art\tiles\candidates\grounds\grass\seasonal\v002-edge-restored --slot spring --asset grass
```

This copies the reviewed PNG into `assets/tiles/grounds/<asset>.png` by default and records the runtime target in the candidate manifest. Use `--out assets\tiles\grounds\<asset>\<state>.png` only when the runtime is already wired to load that state path.

Use custom slots without changing the script:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py prepare --asset grass --variant-group weather --slot dry="dry grass" --slot rain="wet darker grass" --cols 2 --rows 1
```

Run only the source-edge drift check on existing split crops:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\ground_variants.py edge-qa --source assets\tiles\grounds\grass.png --split-dir art\tiles\candidates\grounds\grass\seasonal\v001-generated-sheet\split
```

Ground QA:

* Square.
* Top-down.
* Full tile filled.
* No perspective scene.
* No unwanted alpha.
* No upright object that should be a feature.
* Physical 3D terrain slab shape matching current Realm ground references.
* Continuous material top face, subtle bevel, chipped/worn side faces, worn corners, and dark contact shadow preserved when using a reference.
* Muted material-coloured side faces, no bright gold/yellow edging, and no inner rectangle around the top face.
* Adjacent copies read as deliberate raised terrain slabs.
* Reads clearly as the intended ground material.
* For generated sheets, each split slot is checked as its own full-square tile before being accepted.

Hard failures:

* Missing physical raised square terrain slab.
* Output looks like flat terrain with a separate decorative surround, outline, rim, trim, or UI-like edging.
* Slab sides read as a bright gold/yellow outline instead of muted worn terrain material.
* Top face is boxed in by an inner rectangle or inset line.
* Slab sides are clean, flat, plastic, or materially different from the approved reference when the task says to match a reference.
* Output looks like an invisible seamless texture instead of a visible tile-based RTS ground square.
* The generator removed the 3D slab sides while changing the top material.

## Decal Contract

Use for flowers, tufts, stones, puddles, path scuffs, wheel ruts, settlement dirt, cobbles, crates, barrels, log piles, farm tracks, muddy footprints, and snow-trampled paths.

Surface decal rules:

* Generate a transparent square overlay.
* The square corresponds to one logical tile.
* The decal sits on the ground.
* Do not imply blocking, harvesting, ownership, or interaction.
* Do not use actor sprite processing.
* Do not add baked ground unless the decal is intentionally broad.

Semi-upright decal rules:

* Use for low vegetation and decorative marks with slight height.
* Keep it low and non-interactable.
* Do not turn it into a full feature.
* Respect `projection_factor` internally when present, but do not put the numeric projection factor into the image prompt unless needed.

Decal image prompts should say:

* Transparent square overlay.
* Top-down flat mark for surface decals.
* Low decorative vegetation/detail for semi-upright decals.
* No independent object.
* No character, building, UI marker, or label.

Decal QA:

* Transparent background.
* Correct projection metadata.
* Correct anchor.
* Does not look like an interactable object.
* Does not hide units unless intended.
* Runtime path matches JSON.

## Feature Contract

Use for forest, pine, palm, dead tree, berry bush, wheat crop, fish shoal, gold deposit, stone boulders, mountain peak, reeds, ruins, castle wall, and castle gate.

Rules:

* Generate a transparent upright sprite.
* Anchor to tile centre unless JSON says otherwise.
* May overhang the logical tile.
* Do not include a full ground tile unless a small contact patch or shadow is needed.
* Include depletion, open/closed, damaged, snowcap, wetness, frost, broken, or seasonal states only where gameplay or silhouette changes.
* Neutral features do not use team masks.
* Keep shadows subtle and compatible with terrain.

Feature image prompts should say:

* Transparent upright sprite.
* The object or resource to draw.
* State to draw, if relevant.
* No full ground tile.
* No character unless the feature specifically includes one.
* No UI marker, border, frame, or label.

Concealing feature rules:

* Forests, pines, and reeds should support front/back occlusion.
* Back/contact art draws before units.
* Front/occluder art draws after units.
* If split layers are unsupported, record that split-layer art or an occlusion mask is pending.
* Do not make duplicate hidden-unit sprites; concealment is a renderer/layer effect.

Feature QA:

* Transparent outside the object.
* Correct anchor and depth bucket.
* No baked full ground.
* Resource/depletion states readable.
* Blocking features read as blockers.
* Harvestable features read as resources.
* Concealing features have an occlusion plan.

## Actor Contract

Use for units, animals, ships, and siege.

Preferred production source:

* Single square image, ideally `1024x1024`.
* One complete upright sprite.
* Flat uniform `#ff00ff` magenta background, unless helpers support transparent sources.
* Full sprite, tools, weapons, carried resources, arcs, baskets, wreckage, and dead poses inside the square with padding.

Rules:

* Generate only required source directions, usually `front` and `back`.
* Do not create mirrored left-facing art unless the spec requires it.
* Runtime should mirror horizontally where allowed.
* `front` means three-quarter RTS front angle facing screen right, not face-on.
* `back` means matching rear-right three-quarter angle, not a flat rear diagram.
* Keep identity, scale, equipment, and pose language consistent.
* Prefer coherent batch generation for multi-frame sets.
* Use single-frame generation only to replace failed panels.
* Do not process contact-sheet crops as final art except for temporary mechanical tests.

Actor image prompts should say:

* One complete upright sprite.
* Exact unit/animal/vehicle identity.
* Exact action/state/frame.
* Exact source direction, such as front three-quarter or back three-quarter.
* Required equipment, carried resource, weapon, damage, or corpse state.
* Magenta background if current processing requires it.
* Team-colour slots if required.
* No labels, border, UI marker, or extra characters.

Team colour:

* Player-owned actors usually need base plus team-mask layers.
* Animals and neutral nature objects normally do not.
* Use preview cyan only for intended team-colour slots.
* Do not use transparency as player colour.
* Put team colour on readable accents, not the whole body.

Actor QA:

* Correct identity, action, frame, state, and direction.
* Correct three-quarter angle.
* Consistent scale, bbox, and anchor.
* Clean alpha after magenta removal.
* Clean team mask.
* No stray key-colour pixels.
* Dead/decayed frames remain aligned.
* Runtime manifest has no unaccepted assumptions.

## Building Contract

Use for Town Hall, House, Barracks, Stable, Tower, Farm, Blacksmith, Church, Market, Wall, Gate, Castle, Lumber Camp, Mining Camp, Mill, and Dock.

Buildings are entities, but not normal one-tile actors.

Rules:

* Read JSON footprint before final asset work.
* Do not shrink a multi-tile building into one `48x48` actor frame.
* Use JSON footprint and anchor, usually footprint origin.
* Use sliced footprint prefabs where required.
* Player-owned buildings need team-colour masks.
* Put team colour on flags, awnings, banners, door cloths, shield signs, roof trim, or similar accents.
* Do not colour the whole building body.
* Do not bake settlement halo/path creep into the building unless explicitly making a temporary composite.
* Large destroyed buildings usually become ruin footprint terrain/features, not long-lived dead building sprites.

Building image prompts should say:

* Building type.
* Footprint only if needed to draw the correct shape.
* State being drawn.
* Visible construction, damage, garrison, training, research, or ruin cues if relevant.
* Team-colour slots if required.
* Transparent background unless the current building lane requires another source format.
* No settlement halo unless requested.
* No UI marker, border, frame, or label.

Common building states:

* `construction_0_foundation`
* `construction_1_frame`
* `construction_2_nearly_complete`
* `complete`
* `damaged`
* `garrisoned`
* `garrison_firing`
* `training_*`
* `researching_*`
* `ruin_footprint`

Building QA:

* Correct footprint and origin.
* Correct slice alignment.
* Doors, flags, roofs, shadows, and masks align across cells.
* Team mask only covers intended slots.
* Construction/damaged/garrison/training/research states are useful and readable.
* Settlement halo remains a decal/overlay unless requested.
* Ruins match runtime expectation.

## Projectile, Effect, Weather, And UI Contract

Use for arrows, bolts, boulders, shots, impacts, hit sparks, water splashes, building dust, rain, storm rain, snowfall, alerts, command markers, rally markers, selection, range rings, build previews, wall previews, garrison indicators, queue markers, and research markers.

Rules:

* Effects have their own export group.
* Do not mix effects into unit, building, terrain, feature, or decal prompts.
* Use transparent backgrounds.
* Define projection internally as `tile_overlay`, `upright_world`, or `screen_ui`.
* Projectiles and impacts anchor to world position or path.
* Weather particles loop and are not baked into terrain.
* Selection, range, previews, and command markers must be readable but not obscure gameplay.
* UI icons are UI assets, not world terrain.

Effect image prompts should say:

* Transparent overlay or UI icon.
* Exact marker, projectile, impact, weather particle, or UI symbol.
* Whether it is flat, world-space, or screen/UI, only if visually needed.
* No terrain, unit, building, border, frame, or label unless the asset itself is a marker.

QA:

* Correct projection metadata.
* Correct anchor.
* Transparent background.
* No baked terrain/entity.
* Smooth loop if animated.
* Markers remain distinct at gameplay size.
* Valid/invalid build previews are unmistakable.

## Colour Effects

Do not generate base art for lighting-only changes:

* Dawn/dusk/night tint.
* Fog or explored dimming.
* Storm darkness.
* Selection dimming.
* Concealment tint/alpha.
* Team-colour remap.

Only generate art when a mechanic adds or removes a visible object: torches, damage, cargo, snow buildup, depletion, impact dust, weather particles, or markers.

## Reference Rules

Reference images strongly bias generation.

Legacy tileset reference/template layout:

```text
art/tiles/reference/<lane>/
├── examples/       # curated human-provided references
├── generated/      # generated images promoted after review
├── notes/          # reusable generation lessons
├── *-audit.md      # what each reference file shows and when to use it
└── *-production-status.md  # accepted, candidate, reference-only, and missing asset tracking
```

For existing ground references, use:

* `art/tiles/reference/grounds/ground-reference-audit.md` for what each reference shows and when to use it.
* `art/tiles/reference/grounds/ground-production-status.md` for what is accepted runtime art, candidate art, reference-only, placeholder, or missing.
* `art/tiles/reference/grounds/notes/ground-generation-notes.md` for reusable generation lessons.

Root user-reference rule:

* `art/reference/` is user-owned. Read it when the user supplied references are relevant. Do not write generated images, generated grids, cleanup edits, or reorganized copies there.
* For `art/reference/units/`, use curated references only as subject/equipment evidence unless the user explicitly asks to maintain that catalogue.

Generated-reference rule:

* New reference images generated by the skill belong under `art/generated-reference/<lane>/<slug>/<version>/`.
* Generated context grids and prompt packets belong under `build/generated-reference-context/...` unless the user asks to preserve them.
* A generated reference is not an accepted candidate and not a runtime asset. Copy it into `art/tiles/candidates/...` only if later production work needs to evaluate it as a candidate.

* Only send positive references into image generation.
* Do not send rejected images as references.
* Do not send wrong-angle, flat, face-on, inconsistent, or failed crops.
* Describe rejected examples in text instead.
* Use `view_image` for every local reference that should affect generation.
* Read the lane audit before re-inspecting old references; it records what each file shows.
* Read the lane production-status file before generating; it records what is already accepted, what is only a candidate/reference, and what still needs work.
* If the audit is missing, inspect the references once, then write the audit before relying on them repeatedly.
* If the production-status file is missing, create it for the lane before doing broad generation work.
* A full contact sheet may be a style/action reference if it is the best available reference.
* A grid/gallery reference is a reference sheet, not a runtime sheet; use its row/column notes to pick visually similar slots.
* Start with the reference that is visually closest to the target asset.
* Mechanically split crops are not automatically valid production references.
* Verified generated panels may become positive references later.
* When a generated image becomes a useful future reference, store it under `art/generated-reference/<lane>/<slug>/<version>/` and record its role in the manifest or notes.
* When a generated image becomes an accepted runtime asset, update the production-status file. Do not mark an asset complete only because a runtime fallback file exists.
* When a prompt lesson is reusable for a given tile or sprite, add it to the lane notes file so it does not need to be rediscovered.

## Sheet Rules

Sheet types:

* **Reference sheet**: visual guide only.
* **Generation batch sheet**: temporary coherent output; split before processing.
* **Runtime sheet**: valid only if the renderer explicitly loads sheets.

Actor production sources are never `4x4` contact sheets.

Grounds, decals, features, buildings, and effects may be generated in sheets when that helps keep related states consistent, but each state must be split, named, checked, and stored as its own asset unless the runtime explicitly expects a sheet.

For square tile lanes, prefer `scripts/tile_grid.py` over actor batch helpers when the sheet is only a repeated/editable tile grid. Actor helpers know about directions and frames; tile grids know only rows, columns, slot names, and square crops.

Use grid generation when consistency matters across related variants, such as seasonal grass, water-edge variations, snow coverage levels, damage states for a flat tile overlay, or a small family of matching UI markers.

When reviewing a generated grid:

* Split the grid first so each slot can be inspected independently.
* Run mechanical inspection to catch wrong dimensions, unexpected alpha, or weak slab-edge contrast.
* Use visual judgment for semantic checks such as spring/summer/autumn/winter, wet/dry, damaged/complete, or valid/invalid marker meaning.
* If one or two slots fail, regenerate or edit those slots individually. If the whole sheet loses the Realm edge style or material identity, regenerate the whole sheet from the original template.

## Candidate Storage

Copy keeper images out of transient generator output immediately.

Preferred layout:

```text
art/tiles/candidates/<lane>/<slug>/<state-or-action>/<version>/
├── source.png
├── batch_source.png
├── grid_manifest.json
├── split_manifest.json
├── prompt.txt
├── candidate_manifest.json
└── notes.md
```

For current helper commands that only accept `--entity` and `--action`, keep helper-compatible entity layouts.

Rejected images should not remain as future references. After rejection and approved cleanup, delete transient originals and temporary build copies.

## Commands

Run from the repo root.

Regenerate specs:

```powershell
python -m py_compile scripts\export_image_generation_prompts.py scripts\export_tile_specs.py
python scripts\export_tile_specs.py --clean
python scripts\export_image_generation_prompts.py --clean
```

Find generated specs:

```powershell
Get-ChildItem art\tiles\image-spec -Recurse -Filter *.md
Get-ChildItem art\tiles\image-json -Recurse -Filter *.json
```

Prepare and store generated reference images:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\reference_images.py prepare `
  --group units --slug militia --version v001 `
  --context-root art\reference\units

python .agents\skills\realm-tileset-from-images\scripts\reference_images.py store `
  --group units --slug militia --version v001 `
  --input C:\Users\Edward\.codex\generated_images\...\chosen.png `
  --prompt-file build\generated-reference-context\units\militia\v001\prompt.md
```

Run a focused native tileset ground visual test map:

```powershell
$env:REALM_TILESET_TEST_MAP='1'
.\bin\realm.exe
Remove-Item Env:\REALM_TILESET_TEST_MAP
```

The test map is paused and non-playable. It fills the visible area with the accepted grass tile and surrounds it with never-explored unknown tiles so ground texture and fog-of-war tile wiring can be checked without unrelated mapgen terrain.

Print a generated prompt when helper support exists:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt --entity <slug>
```

Inspect generated JSON for any lane:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec --entity <slug> --spec-source image-json

# Examples:
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec --entity archer --spec-source image-json
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec --entity wolf --spec-source image-json
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec --entity house --spec-source image-json
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec --entity grass --spec-source image-json
```

If helper output looks incomplete for the lane, inspect the JSON file directly and report the helper gap instead of forcing the asset through a wrong processor.

Generate production prompts from image-json:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-frame `
  --entity <slug> --spec-source image-json --action <action> --direction <direction> --frame <frame>

python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-batch-source `
  --entity <slug> --spec-source image-json --action <action> --cols <cols> --rows <rows>

# Examples:
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-frame `
  --entity archer --spec-source image-json --action self_bow__aim --direction front --frame 0

python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-batch-source `
  --entity wolf --spec-source image-json --action idle --cols 2 --rows 2

python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-batch-source `
  --entity house --spec-source image-json --action complete --cols 1 --rows 1
```

Store generated output when helper support exists:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py store-generated `
  --entity <slug> `
  --action <action> `
  --input C:\Users\Edward\.codex\generated_images\...\chosen.png `
  --version v001-clean `
  --kind batch-source `
  --status candidate `
  --prompt-file build\tileset-candidates\<slug>-<action>-v001-prompt.txt
```

Create and split square tile grids for ground/decal/effect variants:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\tile_grid.py make-grid `
  --source assets\tiles\grounds\grass.png `
  --out build\tileset-grids\grass-season-template-4x4.png `
  --cols 4 --rows 4 --tile-size 1024 `
  --slot spring,spring_wet,summer,summer_dry,autumn,autumn_wet,winter_light,winter_snow,spring_alt,summer_alt,autumn_alt,winter_alt,mossy,trampled,frosty,recovering

python .agents\skills\realm-tileset-from-images\scripts\tile_grid.py split-grid `
  --sheet art\tiles\candidates\grounds\grass\seasonal\v001\batch_source.png `
  --out-dir art\tiles\candidates\grounds\grass\seasonal\v001\split `
  --cols 4 --rows 4 --tile-size 1024 `
  --slot spring,spring_wet,summer,summer_dry,autumn,autumn_wet,winter_light,winter_snow,spring_alt,summer_alt,autumn_alt,winter_alt,mossy,trampled,frosty,recovering `
  --force

python .agents\skills\realm-tileset-from-images\scripts\tile_grid.py inspect-grid `
  --sheet art\tiles\candidates\grounds\grass\seasonal\v001\batch_source.png `
  --cols 4 --rows 4 --tile-size 1024 `
  --slot spring,spring_wet,summer,summer_dry,autumn,autumn_wet,winter_light,winter_snow,spring_alt,summer_alt,autumn_alt,winter_alt,mossy,trampled,frosty,recovering `
  --out art\tiles\candidates\grounds\grass\seasonal\v001\grid_inspection.json
```

Use `tile_grid.py` for mechanically repeated square tile grids. Use actor `split-batch-source` only when directions, frames, magenta backgrounds, or actor animation metadata matter.

Split actor batch source:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py split-batch-source `
  --entity <slug> `
  --spec-source image-json `
  --action <action> `
  --sheet art\tiles\candidates\<slug>\<action>\v001-clean\batch_source.png `
  --force
```

Use `--slot direction:frame` when grid order differs from JSON/default.

Process one actor-like frame into base and optional team-mask files:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity <slug> `
  --spec-source image-json `
  --action <action> `
  --direction <direction> `
  --frame <frame> `
  --input art\tiles\workbench\<slug>\<action>\<direction>\frame_00\source.png `
  --write-prompt
```

Do not use `process-frame` for grounds, flat decals, or multi-tile building slice sets unless the helper explicitly supports that lane. For those lanes, use the source contract and JSON path, store candidates, and report the missing finalization helper if one is needed.

Assemble actor-like runtime assets:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py assemble `
  --entity <slug> `
  --spec-source image-json `
  --review-out build\tileset-review\<slug>
```

Review existing manifest:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py review `
  --manifest assets\tiles\entities\<slug>\manifest.json `
  --review-out build\tileset-review\<slug>
```

Verify runtime placement metadata and regenerate the bullseye placement review:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-placement `
  --manifest assets\tiles\entities\<slug>\manifest.json `
  --review-out build\tileset-review\<slug>-placement
```

Run this before accepting any upright actor-like asset. The review output must include `bullseye_placement_sheet.png` and `placement_report.json`; placement verification must pass unless the remaining failures are explicitly documented and intentionally deferred.

Legacy extraction is fallback only:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py extract `
  --entity <slug> `
  --front-state-1 art\tiles\source\<slug>\front_state_1.png `
  --front-state-2 art\tiles\source\<slug>\front_state_2.png `
  --back-state-1 art\tiles\source\<slug>\back_state_1.png `
  --back-state-2 art\tiles\source\<slug>\back_state_2.png `
  --out assets\tiles\entities `
  --review-out build\tileset-review\<slug> `
  --cols 4 --rows 4 --size 48 `
  --team-color "#0088cc"
```

Use legacy extraction only for quick front/back comparison sheets. Do not use extracted reference-sheet crops as accepted production art.

## Runtime Output Conventions

Prefer generated JSON `paths` over guessed paths.

Common paths:

```text
assets/tiles/grounds/<slug>.png
assets/tiles/grounds/<slug>/<state>.png
assets/tiles/grounds/unknown.png  # never-explored/unseen tile, dimmed by fog-of-war in renderer

assets/tiles/decals/<slug>.png
assets/tiles/decals/<slug>/<state>.png

assets/tiles/features/<slug>/manifest.json
assets/tiles/features/<slug>/<state>.png
assets/tiles/features/<slug>/<state>_back.png
assets/tiles/features/<slug>/<state>_front_occluder.png

assets/tiles/entities/<entity>/manifest.json
assets/tiles/entities/<entity>/<action>/<direction>/frame_00_base.png
assets/tiles/entities/<entity>/<action>/<direction>/frame_00_teammask.png

assets/tiles/effects-ui/<slug>.png
assets/tiles/effects-ui/<slug>/<frame>.png
```

Do not invent a path if JSON provides one.

## Manifest Rules

A manifest is the runtime contract.

Actor/building manifests should include:

* Action or state id.
* Directions.
* Frames.
* Frame duration.
* Loop behaviour.
* Base image path.
* Team-mask image path when required.
* A top-level `placement` block with `projection`, `anchor_kind`, `source_size`, `anchor`, `scale_policy`, `footprint`, and `depth`.
* Frame-level `anchor` overrides only where a frame genuinely differs from the action or manifest placement.
* QA-only `final_bbox` and `anchor_offset` metadata for review drift checks.
* Assumptions, if any.

Feature manifests should include:

* States.
* Projection mode.
* Anchor.
* Optional back/front occluder layers.
* Feature traits if needed.
* Depletion or open/closed mapping where required.

Grounds and simple decals may not need manifests if JSON points directly to image paths.

Manifests must resolve only to `assets/tiles/...` paths. Do not accept hidden assumptions for final production assets.

## Fallback Contract

Sprite and tile art is additive. Missing art must not break gameplay.

Every entity, feature, ground, decal, overlay, and effect needs a readable fallback symbol, emoji, ASCII glyph, or procedural rendering.

Fallback order:

1. Valid loaded asset.
2. Same-entity, same-feature, or same-ground fallback.
3. Current emoji/symbol fallback.
4. One-character ASCII glyph.

Adding one sprite or tile must not require every related state to be complete before the game remains playable.

## Generic Asset Set Requirements

No unit, animal, building, feature, ground, decal, projectile, effect, or UI marker is special. If one asset needs custom handling, add that information to the generated specs or docs so every agent can discover it the same way.

Every generated asset set needs these facts available before final production:

* Lane: `ground`, `decal`, `feature`, `unit`, `animal`, `building`, `projectile`, `effect`, or `user_interface`.
* Runtime path: final `assets/tiles/...` output path or manifest path.
* Source contract: full top-down tile, transparent overlay, upright sprite, building footprint/slice, projectile/effect, or UI icon.
* Projection and anchor: enough to place the asset without guessing.
* Footprint: especially for buildings, ships, siege engines, and large features.
* Directions: usually `front` and `back` for mirrorable actors, `south` for buildings, or `default` for non-directional art.
* Actions/states: ids, descriptions, frame counts, timing, loop/hold behaviour, and visible phase notes.
* Team colour: whether required, intended slots, preview colour, and mask requirements.
* Visual variants: research tiers, cargo states, depletion, construction, damage, weather, season, and open/closed states when relevant.
* Fallback: glyph/symbol/procedural fallback still works when any frame is missing.
* QA gate: what must be inspected before accepting the runtime asset.

If any of those facts are missing, do not hard-code a local exception in this skill. Patch the source data, docs, or exporters, regenerate `art/tiles/image-json` and `art/tiles/image-spec`, then continue.

## Generic Sprite Set Workflow

Use this for any actor-like lane: units, animals, ships, siege engines, most projectiles, upright effects, and simple single-sprite buildings.

1. Load `art/tiles/image-json/<group>/<slug>.json`.
2. Confirm `asset_type`, `render.directions`, `render.runtime_mirrors_horizontal`, `placement.footprint`, `team_color`, `actions`, `paths`, and `sources`.
3. Load the matching Markdown prompt for the human-readable visual brief.
4. Enumerate the production matrix:

   ```text
   for each visual variant, if any
   for each action/state
   for each source direction
   for each frame index
   ```

5. Choose the smallest useful batch. Prefer one coherent batch per action or per closely related state group. Do not try to generate a whole unit's complete lifetime in one huge image if consistency or cropping will suffer.
6. Generate a reference/contact sheet only when it helps choose style, state order, or visual identity. Reference sheets are not final production sources.
7. Generate production sources as standalone square images or small batch sheets that split into standalone square images.
8. Store keepers under `art/tiles/candidates/...`.
9. Split batch sheets into workbench `source.png` frames.
10. Process each accepted actor-like frame into runtime base and team-mask files.
11. Assemble the manifest from image-json-backed actions and directions.
12. Run review artifacts and inspect contact sheet, isometric placement, alpha/mask, bbox/anchor overlay, and manifest assumptions.

For any unit, the required actions are whatever the generated JSON says. Archer can include self-bow and crossbow research variants. Militia and Knight can include weapon-tier variants. Siege engines can include visible operator constraints. Animals can include carcass/depletion states. The workflow is the same.

## Generic Tile And Overlay Workflow

Use this for grounds, decals, feature layers, weather overlays, tactical markers, and UI icons.

1. Load generated JSON and Markdown.
2. Confirm source contract and runtime path.
3. Generate the lane-correct image: top-down full square for grounds, transparent square overlay for decals/effects/UI, transparent upright sprite or split layer for features.
4. Store the keeper under `art/tiles/candidates/...`.
5. Split state sheets into one file per state if a sheet was used.
6. Write or place final files under the JSON runtime path.
7. Create or update a manifest only when the runtime expects one.
8. Run lane QA. For grounds this means tileability and top-down fill. For decals/effects/UI this means transparency and readability. For concealing features this means back/front occluder planning.

Do not use actor `process-frame` for top-down grounds or flat decals. If a finalization helper is missing for a lane, still produce the prompt/candidate/review work and report the exact missing helper.

## Making All Units Fully Generatable

To make every unit as ready as any other, the exporter/spec layer must carry the unit's production matrix.

For each unit/animal/ship/siege entity, verify or add:

* `states` and `actions` in image-json are complete and specific enough to draw.
* Each action has a stable id, description, recommended frame count, timing where known, loop/hold behaviour where relevant, and phase notes if the frames differ.
* Research or equipment variants are explicit action variants, overlays, or documented as intentionally shared art.
* Projectiles are referenced through projectile specs and are not baked into release/impact frames.
* Team-colour slots are defined for player-owned assets.
* Operator/cargo/garrison/death/decay/damage states are represented when they matter visually.
* Runtime selection knows how to request the action/state, direction, variant, and frame.
* Missing frame fallback is tested, so partial production sets are additive and never blank.

When this data is not present, the next step is to improve `scripts/export_tile_specs.py`, `scripts/export_image_generation_prompts.py`, or the tileset docs, then regenerate. The skill should not contain per-unit action tables.

Implementation gaps to report or fix when encountered:

* If `realm_tileset.py` has a verifier for only one asset, replace or supplement it with a generic manifest/review verifier before treating that asset as the model.
* If a unit action only says `idle frame 2` or has weak phase text, improve the action metadata in docs/exporters so prompts can ask for the right pose.
* If a lane has generated JSON but no finalization helper, keep prompt/candidate work moving and report the missing helper by lane and expected runtime output shape.
* If runtime cannot request a JSON-declared state, variant, direction, or frame, patch runtime selection or downgrade the state in the exporter; do not generate unreachable art silently.
* If manifests are placeholders or contain assumptions, do not call the asset complete until runtime files and manifest entries match the JSON contract.

## Stop And Report

Stop before producing final assets when:

* The asset lane is unclear.
* Docs and JSON disagree materially.
* Runtime path is missing.
* Projection mode is missing for a decal/effect/UI asset.
* A helper only supports actors but the requested asset is ground, decal, feature, effect, or multi-tile building.
* Building footprint is unknown.
* Team mask is required but slots are undefined.
* Ground art is not top-down.
* Upright sprite art is flat/top-down.
* Sprite identity changes across frames.
* Source image has labels, gutters, bad crops, wrong state, or wrong direction.
* Accepting output would require hidden manifest assumptions.

## Final Readiness Checklist

Before saying tileset work is ready, confirm:

* Asset lane was chosen correctly.
* Generated spec or JSON was used.
* Source image contract matched the lane.
* Keeper image was stored under `art/tiles/candidates/...`.
* Runtime files were written only under `assets/tiles/...`.
* No manifest points to `art/tiles/...`.
* Projection metadata, anchor, footprint, and layer order are correct.
* Team mask exists only where required.
* Fallback path still works for missing states.
* Review artifacts or manual previews were inspected.
* Unsupported helper behaviour or temporary assumptions were reported.
