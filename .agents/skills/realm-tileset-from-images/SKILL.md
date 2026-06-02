---
name: realm-tileset-from-images
description: Create Realm tileset assets from pasted or generated sprite-sheet images. Use when Codex needs to plan coherent batch or single-frame production sprite prompts, split generated batch sheets into standalone production sources, crop magenta-square sources into transparent PNG frames, create base plus team-colour mask layers, handle partial front/back or action states, update Realm tile manifests, or run visual QA for isometric placement, stray key colour, bad crops, transparent holes, inconsistent scale/anchors, and animation metadata.
---

# Realm Tileset From Images

## Repository Boundary

Keep final game assets and generation material separate.

- `assets/tiles/` is runtime-facing. Only put files here when the game can load them directly: entity manifests, `*_base.png`, `*_teammask.png`, and future terrain/feature/decal runtime assets.
- `art/tiles/` is the art-production workspace. Put prompts, JSON specs, references, source sheets, generated candidates, split workbench frames, provenance, and metadata here.
- `build/tileset-*` is temporary review or prompt output. It is useful for QA but is not a source of truth.
- `C:\Users\Edward\.codex\generated_images\...` is transient generator output. Copy any keeper into `art/tiles/candidates/...` with `store-generated` before using it.

Do not place `candidates`, `workbench`, `source`, `reference`, `image-spec`, or `image-json` under `assets/tiles/`. If one appears there, move it to the matching `art/tiles/` folder before continuing. Do not point runtime code, manifests, or docs at `art/tiles/`; runtime paths must continue to use `assets/tiles/...`.

## Golden Path

Use this skill for Realm art ingestion, not for general image editing.

1. Inspect the current game code and `docs/tileset/realm_tileset_visual_audit.md` before deciding required states, footprints, anchors, projection, or team-colour slots. Realm tileset mode uses isometric-projected terrain diamonds with upright sprites anchored over the tile centre.
2. Treat the C++ entity animation spec as the source of truth. Prefer `--spec-source code`, which calls `bin\realm.exe --dump-animation-spec <entity>`. Use `--spec-source fallback` only before the local game binary has been built.
3. Treat a 16-action peasant sheet as a planning/reference artifact, not the production source. It may be a simple contact sheet on a solid background. Split it into workbench references with `split-reference`, or use row/column wording such as "top left" when asking for a generated production source.
4. For any multi-frame production set, prefer a coherent batch source over independent generations. Use `prompt-batch-source` to generate one 2x2 intermediate sheet with the exact required frames and one shared character identity.
5. Immediately copy any generated image worth keeping out of `C:\Users\Edward\.codex\generated_images\...` with `store-generated`. Project source candidates live under `art/tiles/candidates/<entity>/<action>/<version>/`; `.codex/generated_images` is transient tool output and must not be used as the source of truth.
6. Use `split-batch-source` on the repo-local candidate sheet to split it into standalone workbench `source.png` files. The batch sheet is allowed only as a consistency intermediate; accepted runtime assets are still single-frame production assets.
7. Use `prompt-frame` and independent single-frame generation only as a fallback for replacing a failed panel. `prompt-frame` only writes a prompt; it does not generate pixels. When using the built-in image generator, first make every positive local reference image visible with `view_image`, then call the image generator, then store the chosen output with `store-generated` before splitting or processing. Do not show wrong-angle, flat, front-on, inconsistent, or otherwise rejected crops to the image generator as references; use them only for human diagnosis outside the generation call.
8. Process each single-frame source with `process-frame`; this removes magenta, fits the sprite to the 48x48 canvas, creates base/team-mask PNGs, and writes per-frame metadata under `art/tiles/workbench/<entity>/`.
9. Assemble or refresh the runtime manifest with `assemble`, which writes final loadable files under `assets/tiles/entities/...`, then inspect `review.html`, `contact_sheet.png`, `isometric_contact_sheet.png`, `alpha_mask_sheet.png`, and `bbox_anchor_overlay.png` before saying the tileset is ready.
10. Use `verify-peasant-idle` for Peasant idle work. It rebuilds review artifacts and fails if the four idle frames, timing, masks, provenance, crop/key QA, or large bbox/anchor/scale drift are wrong. Missing non-idle Peasant image actions are allowed for an idle-only asset set; the runtime must fall back to glyph rendering for those actions.
11. Keep the old `extract` command only as a fallback for quick 4x4 sheet ingestion or comparison passes. Do not use extracted reference-sheet crops as accepted production art.

## No-Friction Rules

- If a generated frame is not the correct action, state, direction, or right-facing source angle, regenerate before processing it.
- Do not infer that a filepath in a prompt is visible to image generation. Use `view_image` for every local reference crop or sheet that should affect generation.
- Only send positive references into image generation. If the only valid reference is the full 4x4/16-action sheet, send only that sheet. Do not send flat rear crops, face-on front crops, failed generated batches, or inconsistent frames as "costume-only" references; they bias the model toward the wrong pose. Once a generated back-angle panel is manually verified as correct, it can become a positive reference for later iterations.
- Do not process contact-sheet crops as final assets unless the user explicitly asks for a temporary mechanical test.
- Do not create mirrored left-facing source art. Generate only right-facing `front` and `back`; the renderer should mirror at runtime for leftward facings.
- Do not create a multi-frame set with separate image generation calls unless a coherent batch source failed and only one panel needs replacement.
- Do not accept a manifest with assumptions for a requested final production set.
- Prefer small deterministic script checks over hand-reading JSON when the same check is likely to recur.
- Do not let rejected generated images linger as possible future references. Once a candidate is rejected, record or state why, then delete its `$CODEX_HOME/generated_images/...` original and any `build/...` experiment copy when the user has approved cleanup. Keep accepted or still-active candidates only under `art/tiles/candidates/<entity>/<action>/<version>/`.
- Do not add generation or planning artifacts to `assets/tiles/`. `assets/tiles/` is only for files the renderer can load.
- Do not make runtime manifests depend on `art/tiles/`; manifests should record provenance from `art/tiles/`, but their asset frame paths must point inside `assets/tiles/`.

## Source Image Contract

Preferred final production sources are single square images, ideally 1024x1024, with one complete sprite on a flat uniform `#ff00ff` magenta background. The sprite and all tools, limbs, carried resources, weapon arcs, baskets, and lying/dead poses must be fully inside the square with generous padding. Production sources are never 4x4 grids.

A 2x2 coherent batch sheet is permitted only as a generation intermediate for one small production set. Split it immediately with `split-batch-source`; process only the resulting standalone `source.png` files. Do not point `process-frame` at the unsplit batch sheet.

Realm `front` does not mean face-on. It means the same three-quarter RTS front angle as the villager reference: body and face turned about 30-45 degrees toward screen right, one side of the helmet/body visible, no straight-on symmetrical mascot pose. Realm `back` is the matching rear-right three-quarter angle: the shoulders, belt, hem, and boots should form a visible diagonal, the near side/pouch/boot should read closer, and the far side should be partly hidden. It is not a flat rear diagram.

Reference/contact sheets are different: they are only visual guides for style, action order, and row/column selection. They do not need crop-safe magenta squares, gutters, transparency, or final tile boundaries. A clear 4x4 contact sheet on one solid magenta, white, black, or other plain background is fine. For generated reference sheets, prefer consistent row/column spacing; `split-reference` searches for magenta square components when present and otherwise falls back to fixed grid bins.

Reference hygiene matters more than quantity. Treat each candidate reference as either positive or rejected before loading it with `view_image`. Positive references must show the angle, pose language, and character design you want copied. Rejected references may explain what went wrong to the human operator, but should not be made visible to the image generator in the same generation turn. For Peasant idle, the mechanically split flat back and face-on front crops are not positive generation references for the production 2x2 batch; if no verified angled back source exists yet, use only the full 16-action sheet plus prompt text.

Do not process reference-sheet crops as final production art except for quick mechanical tests. Use `prompt-batch-source` for a multi-frame coherent source, or `prompt-frame` to generate a replacement standalone 1024x1024 production frame from a selected reference slot.

Do not delete every magenta-ish pixel globally. Remove magenta key pixels and plain page-background white connected to the tile crop edge by flood fill, remove only stranded pixels that are still very high-confidence key magenta, clear magenta-like edge fringe pixels only when they touch transparent background, and clear post-resize pink/white edge haze. This protects accidental magenta in skin, meat, berries, or clothing while cleaning generated backgrounds, gutters, and anti-aliased key edges.

## Commands

From the repo root:

Code-derived peasant unit/action spec:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py unit-spec `
  --entity peasant `
  --spec-source code `
  --out art\tiles\workbench\peasant\unit_spec.json
```

Peasant idle exact production lane:

```powershell
# First generate one coherent 2x2 batch source with the printed prompt.
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-batch-source `
  --entity peasant --spec-source code --action idle `
  --prompt-out build\tileset-candidates\peasant-idle-v001-prompt.txt

# After generation, store the chosen output in a repo-local candidate folder.
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py store-generated `
  --entity peasant --action idle `
  --input C:\Users\Edward\.codex\generated_images\...\chosen.png `
  --version v001-back-angle-clean `
  --kind batch-source `
  --status candidate `
  --prompt-file build\tileset-candidates\peasant-idle-v001-prompt.txt `
  --reference "C:\Users\Edward\Desktop\peasant 3.png"

python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py split-batch-source `
  --entity peasant --spec-source code --action idle `
  --sheet art\tiles\candidates\peasant\idle\v001-back-angle-clean\batch_source.png --force

python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity peasant --spec-source code --action idle --direction front --frame 0 `
  --input art\tiles\workbench\peasant\idle\front\frame_00\source.png --write-prompt
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity peasant --spec-source code --action idle --direction back --frame 0 `
  --input art\tiles\workbench\peasant\idle\back\frame_00\source.png --write-prompt
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity peasant --spec-source code --action idle --direction front --frame 1 `
  --input art\tiles\workbench\peasant\idle\front\frame_01\source.png --write-prompt
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity peasant --spec-source code --action idle --direction back --frame 1 `
  --input art\tiles\workbench\peasant\idle\back\frame_01\source.png --write-prompt
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py assemble `
  --entity peasant --spec-source code --review-out build\tileset-review\peasant-idle
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-peasant-idle `
  --manifest assets\tiles\entities\peasant\manifest.json `
  --review-out build\tileset-review\peasant-idle
```

Generate a coherent 2x2 production batch prompt:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-batch-source `
  --entity peasant `
  --spec-source code `
  --action idle
```

Default Peasant idle panel order is: top-left `front frame 0`, top-right `back frame 0`, bottom-left `front frame 1`, bottom-right `back frame 1`. Frame 1 must be the arms-crossed long-idle pose. The prompt names labels for the generator but also says not to draw text labels.

Split a coherent 2x2 production batch into standalone source frames:

Store generated output before splitting:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py store-generated `
  --entity peasant `
  --action idle `
  --input C:\Users\Edward\.codex\generated_images\...\chosen.png `
  --version v001-back-angle-clean `
  --kind batch-source `
  --status candidate `
  --reference "C:\Users\Edward\Desktop\peasant 3.png"
```

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py split-batch-source `
  --entity peasant `
  --spec-source code `
  --action idle `
  --sheet art\tiles\candidates\peasant\idle\v001-back-angle-clean\batch_source.png `
  --force
```

`split-batch-source` validates the expected panel count, square-ish sheet dimensions, non-empty sprite content, and minimum margins before writing `source.png` files. Use `--slot front:0 --slot back:0 --slot front:1 --slot back:1` to override the default grid order.

Split a 16-action reference sheet into named workbench slots:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py split-reference `
  --entity peasant `
  --sheet art\tiles\source\peasant\front_state_1.png `
  --view front --state 1
```

Resolve a human slot description before generating final art:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py slot-info `
  --entity peasant `
  --spec-source code `
  --slot top-left `
  --view front `
  --state 1
```

Generate an exact single-frame production prompt from a reference slot:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-frame `
  --entity peasant `
  --spec-source code `
  --direction front `
  --reference-slot top-left `
  --reference-view front `
  --reference-state 1
```

You can also generate by explicit action/frame path:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt-frame `
  --entity peasant `
  --spec-source code `
  --action walk `
  --direction front `
  --frame 0 `
  --reference art\tiles\workbench\peasant\reference\front_state_1\02_walk.png
```

Process one generated 1024x1024 frame:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py process-frame `
  --entity peasant `
  --spec-source code `
  --action walk `
  --direction front `
  --frame 0 `
  --input art\tiles\workbench\peasant\walk\front\frame_00\source.png `
  --write-prompt
```

Assemble the runtime manifest and review artifacts:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py assemble `
  --entity peasant `
  --spec-source code `
  --review-out build\tileset-review\peasant-single
```

Fallback sheet extraction:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py extract `
  --entity peasant `
  --front-state-1 art\tiles\source\peasant\front_state_1.png `
  --front-state-2 art\tiles\source\peasant\front_state_2.png `
  --back-state-1 art\tiles\source\peasant\back_state_1.png `
  --back-state-2 art\tiles\source\peasant\back_state_2.png `
  --out assets\tiles\entities `
  --review-out build\tileset-review\peasant `
  --cols 4 --rows 4 --size 48 `
  --team-color "#0088cc"
```

For partial sets, pass only the files available. The extractor duplicates missing animation states, copies front views to back views when necessary, and records every assumption in the manifest and QA report. The `assemble` command also records missing single-frame outputs as manifest assumptions. Do not treat generated assumptions as final art without visual review.

Useful review-only pass:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py review `
  --manifest assets\tiles\entities\peasant\manifest.json `
  --review-out build\tileset-review\peasant
```

Peasant idle QA pass:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py verify-peasant-idle `
  --manifest assets\tiles\entities\peasant\manifest.json `
  --review-out build\tileset-review\peasant-idle
```

Sheet prompt helper:

```powershell
python .agents\skills\realm-tileset-from-images\scripts\realm_tileset.py prompt --entity peasant --view front --state 1
```

## Output Convention

Runtime entity output lives under:

```text
assets/tiles/entities/<entity>/
├── manifest.json
└── <action>/<direction>/
    ├── frame_00_base.png
    ├── frame_00_teammask.png
    ├── frame_01_base.png
    └── frame_01_teammask.png
```

Directions are `front` and `back`. The game may mirror horizontally at runtime for left/right facings; do not generate mirrored sheets unless asked.

The manifest is the runtime contract. For each action it must include `frame_ms`, `loop`, `directions`, `frames`, and any `assumptions`. Its frame entries must resolve to files under `assets/tiles/entities/...`.

Art-production files live under `art/tiles/`, never under `assets/tiles/`.

The single-frame workbench lives under:

```text
art/tiles/workbench/<entity>/
├── unit_spec.json
├── reference/<view>_state_<state>/
└── <action>/<direction>/frame_XX/
    ├── prompt.md
    └── metadata.json
```

Frame metadata records prompt/reference provenance, source and final bounding boxes, anchor offset, scale, QA stats, and manual review status.

Generated source candidates live under:

```text
art/tiles/candidates/<entity>/<action>/<version>/
├── batch_source.png
├── candidate_manifest.json
└── prompt.txt
```

Use a meaningful version id such as `v001-back-angle-clean` or the default timestamp. This folder is the durable project-local record for generated source art. Do not rely on `C:\Users\Edward\.codex\generated_images\...` after a generation call; copy any candidate worth keeping with `store-generated`, then use the candidate path for `split-batch-source`.

Other art-production folders:

```text
art/tiles/image-spec/      Markdown prompt/spec exports
art/tiles/image-json/      JSON prompt/spec exports for tooling and audit
art/tiles/reference/       Positive reference sheets and visual guides
art/tiles/source/          Imported or generated source sheets
```

If a file helps create or review art but is not loaded by the game, it belongs in `art/tiles/` or temporary `build/tileset-*`, not `assets/tiles/`.

## Runtime Direction Contract

The art contract is two source directions: `front` and `back`. Source art always faces slightly right. The renderer is responsible for choosing front/back by the Realm isometric direction group and mirroring horizontally for slightly-left facings.

- Preferred visible facings use `front`.
- `back` is used only for upward/screen-back movement according to the code-derived Realm isometric direction mapping. Do not describe this as "bottom-three" without checking the current code; use screen direction names in tests and review notes.
- Diagonals must choose source plus mirror explicitly: down-right = front/not mirrored, down-left = front/mirrored, up-right = back/not mirrored, up-left = back/mirrored.
- Idle facing should hold the last movement or target-facing decision. A stopped Peasant should not snap back to default front if it was last facing up/back.
- If the current code cannot express these choices, patch the runtime before claiming in-game direction verification passed.

## Review Gate

Before accepting a processed set:

- Inspect `review.html` and the contact sheet PNG.
- Inspect `isometric_contact_sheet.png`; sprites should stand on the isometric diamond with the stance anchor over the tile centre.
- Inspect `alpha_mask_sheet.png` for unwanted transparent holes and empty or noisy team masks.
- Inspect `bbox_anchor_overlay.png` for scale drift, baseline drift, anchor drift, and inconsistent frame proportions.
- Confirm every frame is a square sprite canvas and the character/object remains centered.
- Treat any non-transparent pixels touching the crop edge as a regeneration or manual-crop issue unless the asset is intentionally edge-to-edge terrain.
- Check that magenta is gone from backgrounds but not removed from plausible art colours.
- Check the review contact sheet's composed team-colour preview first, then check `*_base.png` and `*_teammask.png` separately. The base image should look neutral; the composed preview should recover the chosen player colour.
- Confirm front/back direction and state order match the prompt.
- Confirm frame 1/state 2 for Peasant idle is visibly arms-crossed in both `front` and `back`.
- Confirm slow or one-shot animations are encoded in the manifest. Peasant idle should transition after about `20000` ms and hold the arms-crossed frame. Peasant death should have `loop: false`, `hold_last: true`, and a long transition, around `30000` ms per frame.

If QA flags residual key pixels or transparent holes, tune thresholds with `--magenta-threshold`, `--team-threshold`, or regenerate the source art with clearer key colour separation. Do not silently hand-wave the review.
