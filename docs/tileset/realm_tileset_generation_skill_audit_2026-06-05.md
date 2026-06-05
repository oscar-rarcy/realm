# Realm Tileset Generation Skill Audit - 2026-06-05

This report audits the recent `realm-tileset-from-images` generation sweep. It focuses on why the run produced mixed results, why the coverage report looked complete, and why many generated assets are not actually visible in normal gameplay.

No replacement assets were generated for this audit.

## Executive Summary

The run failed in four different ways that were collapsed into one `accepted` status:

1. **Runtime wiring failure**: many generated files exist under `assets/tiles`, but current SDL gameplay does not request those paths or does not have a draw path for that lane.
2. **Acceptance-gate failure**: `scripts/tileset_coverage.py` accepts path existence plus ledger hashes. It does not check visual style, alpha cleanup, ground dimensions, runtime selectability, lane draw support, or manual review metadata.
3. **Style-contract failure**: the prompt set does not enforce one coherent Realm style across lanes. Units and animals ask for paper-cutout sprites, while features ask for watercolor map-integrated art and the index says buildings/decals should be painted map-art.
4. **Reference/provenance failure**: the repo contains many useful references, but the generated artifacts do not record systematic use of them. The durable metadata I found records no positive references and pending manual review.

The result is a mechanically full asset tree, not a visually coherent or fully live tileset.

## Evidence Artifacts

Generated during this audit:

- `build/tileset-audit/representative-runtime-assets.png`: representative contact sheet for current runtime files.
- `build/tileset-audit/evidence-summary.json`: file counts, dimensions, ledger groups, runtime action-path presence, reference metadata summary.
- `build/tileset-audit/alpha-magenta-summary.json`: alpha and residual magenta summary for runtime PNGs.
- `build/tileset-coverage-audit.md`: current coverage report generated from `scripts/tileset_coverage.py report`.

Key observed counts:

| Evidence | Result |
|---|---:|
| Current coverage work items reported accepted | 808 / 808 |
| Production ledger accepted rows | 1016 |
| Runtime ground PNGs | 20 |
| Runtime ground PNGs under 128 px | 18 |
| Entity PNGs | 1358 at 48x48, 174 at 32x32 |
| Feature PNGs | 56 at 48x48 |
| Decal PNGs | 13 at 48x48 |
| Workbench metadata files found | 64 |
| Workbench metadata files with recorded references | 0 |
| Workbench metadata files with `manual_review: pending` | 64 |
| Generated-reference context files found under `build/generated-reference-context` | 0 |

## What Is Actually Used In The Game

### Global Runtime Gate

Image tiles are local SDL-only. The current `imageTilesetEnabled()` implementation returns false for web builds and true only for SDL tileset/emoji mode, lab override, or `REALM_IMAGE_TILESET`:

- `src/render/sdl/display_glyphs.cpp`: `imageTilesetEnabled()`.
- `src/render/sdl/display_glyphs.cpp`: `drawEntityImageResolved()` returns false immediately if image tiles are disabled.

This means generated PNGs under `assets/tiles` are not a web tileset today. They are local SDL tileset-mode overrides.

### Grounds

The game can load ground images through:

- `src/render/sdl/display_glyphs.cpp`: `drawGroundTexture()` and `applyTerrainTexture()`.
- `src/render/sdl/tileset_assets.cpp`: `tilesetLoadGroundTileScaled()` and `loadImageTexture()`.

But the loader explicitly ignores placeholder-sized ground images:

- `src/render/sdl/tileset_assets.cpp`: images with width or height below 128 are marked `placeholder-sized ground image ignored`.

The missing-asset/logging path also treats a ground as accepted-looking only if the file is at least 100000 bytes:

- `src/render/sdl/display_glyphs.cpp`: `runtimeGroundAssetLooksAccepted()`.

Current state:

- `assets/tiles/grounds/grass.png`: 1254x1254, real.
- `assets/tiles/grounds/unknown.png`: 1254x1254, real.
- 18 other ground files: 48x48, approximately 446-460 bytes, runtime placeholders.

Why this failed:

- The ground production status already says these files are `runtime_placeholder` and should not be treated as production art just because files exist.
- The coverage script treated their required paths as present and accepted them because the ledger said accepted.
- The runtime loader and the ground-status document disagree with the coverage result.

Ground result:

- Grass and unknown are live enough to render.
- Most other ground tiles are not real generated ground tiles and are ignored by the ground loader.

### Decals

Decal specs and files exist under `assets/tiles/decals`, but I did not find a runtime draw path that loads these PNGs. The renderer logs missing decal paths in `logMissingVisualTileParts()`, but the map draw path does not call a decal image loader.

Why this failed:

- Decals were treated as direct base assets by coverage.
- Coverage does not verify that the lane has a draw path.
- The 13 decal files are all tiny 48x48 files, many around 100-184 bytes.

Decal result:

- They are generated/runtime-placed files, but not visibly used in normal SDL map rendering.

### Features

Feature manifests and PNGs exist under `assets/tiles/features`, for example `berry_bush`, `forest`, `pine`, and `reeds`.

The runtime checks feature manifest existence:

- `src/render/sdl/display_glyphs.cpp`: `featureManifestPath()`.
- `src/render/sdl/display_glyphs.cpp`: `hasTerrainImageTile()` checks for a feature manifest.
- `src/render/sdl/display_glyphs.cpp`: `logMissingVisualTileParts()` logs missing feature manifests.

But I did not find code that loads or draws `assets/tiles/features/<feature>/<state>/*.png`. Concealing features still use symbolic occluder glyphs:

- `src/render/sdl/display_glyphs.cpp`: `drawFeatureOccluderIfNeeded()` draws `featureOccluderGlyph()`, not feature PNGs.

Why this failed:

- Feature generation/promote created runtime files before the renderer had feature image drawing.
- Coverage only checked files/manifests, not draw support.
- The visual prompt for features intentionally asks for a different style than units/animals.

Feature result:

- Feature assets are not meaningfully live as image tiles.
- Their manifests can suppress some missing-asset logging, which makes coverage look better than visual reality.

### Entities: Units, Animals, Ships, Siege, Buildings

Entity image drawing is real:

- `src/render/sdl/display_glyphs.cpp`: `drawEntityImageResolved()`.
- `src/render/sdl/tileset_assets.cpp`: `tilesetLoadEntityFrame()`.

But it is strict about the exact action/direction/frame path:

```text
assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_base.png
```

Before loading, the renderer calls:

- `tilesetEntityFrameExists(e.type, action, direction, frameIndex)`.

The action comes from:

- `src/core/entity_animation.cpp`: `entityAnimationActionId()`.
- `src/core/entity_animation.cpp`: `findEntityActionAnimationSpec()`.

Current resolver behavior:

- Peasant has detailed action specs.
- Non-peasant live entities mostly resolve to plain `idle`.
- Non-peasant dead entities can resolve to `death`.
- Building-specific states such as `complete`, `training`, `garrisoned`, `construction`, and `ruin_footprint` are not selected by the current entity animation resolver.

This creates two classes of generated entity assets.

Live or partly live because they have plain runtime paths:

| Entity | Current live `idle/front/frame_00_base.png` exists | `death/front/frame_00_base.png` exists |
|---|---:|---:|
| boar | yes | yes |
| wolf | yes | yes |
| catapult | yes | yes |
| fishing_boat | yes | yes |
| warship | yes | yes |
| transport | yes | yes |
| ram | yes | yes |

Generated but not selected for normal live gameplay because action names do not match current resolver:

| Entity | Generated action examples | Current resolver asks for |
|---|---|---|
| archer | `self_bow__idle`, `crossbow__idle` | `idle` |
| militia | `basic_weapons__idle` | `idle` |
| knight | `basic_weapons__open_helmet__idle`, `iron_weapons__plate_helm__idle` | `idle` |
| spearman | `short_spear__idle`, `pike__idle` | `idle` |
| trebuchet | `traction_trebuchet__idle`, `counterweight_trebuchet__idle` | `idle` |
| deer | `idle_graze` | `idle` |
| sheep | `idle_graze` | `idle` |

Buildings have a different mismatch:

| Building examples | Generated path | Current resolver asks for |
|---|---|---|
| town_hall, house, barracks, dock | `complete/south/frame_00_base.png` | `idle/front/frame_00_base.png` |
| farm | `sowing`, `growing`, `ripe`, etc. | `idle/front/frame_00_base.png` |
| wall/gate | `straight`, `corner`, `closed`, `open`, etc. | `idle/front/frame_00_base.png` |

Why this failed:

- The spec exporter generated a future/desired art taxonomy.
- The runtime action resolver is still much simpler.
- Coverage validated the future taxonomy, not the current runtime selector.

Entity result:

- Some simple actors render.
- Many generated units/buildings are present on disk but invisible in normal play.
- The apparent mixed in-game result is expected from the path mismatch.

### Projectiles And Effects/UI

Projectile and effect files exist mainly under `assets/tiles/effects-ui`, and projectile manifests exist under `assets/tiles/projectiles`.

Current map renderer projectile path still draws glyphs:

- `src/render/sdl/map_renderer.cpp`: `drawProjectileSpriteAt()` calls `drawCentered()` with `projectile.glyph`.

Action markers still draw procedural shapes/glyphs:

- `src/render/sdl/map_renderer.cpp`: `drawActionMarkerIndicator()`.

I did not find a runtime loader call for `assets/tiles/effects-ui/*.png` in these draw paths.

Why this failed:

- The generation sweep produced assets for a lane that the renderer does not yet consume.
- Coverage accepted the files because direct required paths existed.

Projectile/effects result:

- They are asset files, not live visual replacements.

## Why The Coverage Report Said Everything Passed

`scripts/tileset_coverage.py` builds expected work items from generated JSON specs. Its classification checks:

- required runtime paths exist,
- manifest placeholder flags,
- ledger record exists and says `accepted`,
- spec/prompt hashes match,
- runtime file hashes match the ledger.

It does not check:

- image dimensions beyond existence,
- ground loader minimum size,
- whether an entity path is requested by current gameplay,
- whether feature/decal/projectile/effect image draw code exists,
- whether a PNG has usable alpha,
- whether magenta key cleanup succeeded,
- whether style matches the intended reference,
- whether references were used,
- whether a human review contact sheet was accepted,
- whether workbench metadata says `manual_review: accepted`.

The `accept` command records whatever item is currently being accepted. It does not inspect workbench metadata or visual QA artifacts:

- `scripts/tileset_coverage.py`: `command_accept()`.

Why this failed:

- `accepted` currently means "ledger hash agrees with files", not "game-visible and visually approved".
- Historical/duplicate ledger records make the state noisier: the ledger has 1016 accepted rows while the current coverage planner sees 808 work items.

Required change:

Coverage status should be split into at least:

- `generated_file_exists`,
- `runtime_selectable`,
- `runtime_drawn`,
- `alpha_clean`,
- `style_reviewed`,
- `accepted_runtime`.

## Why The Style Was Inconsistent

The prompt set itself is inconsistent by lane.

### Unit And Animal Prompt Style

Unit and animal prompts contain a `Tiny Sprite Style Contract`:

- paper-cutout style,
- thick black outline,
- cream/off-white die-cut border,
- flat muted medieval/storybook colour,
- pure magenta background.

This is visible in:

- `art/tiles/image-spec/units/archer.md`.
- `art/tiles/image-spec/units/knight.md`.
- `art/tiles/image-spec/animals/boar.md`.

This is why archer and some animals are closer to the target style.

### Feature Prompt Style

Feature prompts contain a `Map-Integrated Feature Style Contract`:

- hand-drawn watercolor map-feature style,
- no cream paper border,
- no sticker outline,
- lower/contact area blends into the map.

This is visible in:

- `art/tiles/image-spec/features/berry_bush.md`.

This directly explains why berry bush does not look like the paper-cutout assets. The prompt asks it not to.

### Index-Level Style Split

The generated prompt index states:

- Unit and animal actor prompts use the tiny medieval paper-cutout style.
- Projectile prompts use the same moving paper-cutout treatment.
- Building and decal prompts use simplified painted map-art, not paper cutouts.
- Ground and feature prompts use map-integrated hand-drawn watercolor styling.

This is not a model accident. The current source-of-truth prompt contract asks different lanes to use different styles.

Why this failed:

- There is no single canonical Realm style guide that says which visual language each lane must use.
- The skill has lane contracts, but it does not force the current user-desired style matrix.
- The phrase "use references only for equipment and silhouette" means unit references are explicitly not style references.

Required change:

Add a style-decision document or skill section that is consulted before generation. It should name the style target per lane and classify reference images by role:

| Reference role | Allowed use |
|---|---|
| Style reference | outline weight, paper border, palette, simplification, finish |
| Equipment reference | gear, weapons, tack, armour, shields |
| Pose reference | stance, angle, silhouette |
| Geometry reference | ground slab shape, contact shadow, crop |
| Negative reference | do not reuse; only diagnostic |

Right now the workflow has equipment/silhouette references, but not a strong positive style-reference gate for most lanes.

## Why References Were Not Reliably Used

The repo contains many reference images:

- `art/reference/animals`.
- `art/reference/units`.
- `art/reference/buildings`.
- `art/reference/features`.
- `art/reference/ground`.
- `art/reference/decals`.
- `art/reference/projectile`.

But the durable generation evidence does not show systematic use:

- `build/generated-reference-context` has no files.
- The generated catapult reference manifest says no visual context images were found.
- The 64 workbench metadata files found under `art/tiles/workbench` all have `references: []`.
- Those same 64 metadata files all have `manual_review: pending`.
- Animal candidate folders I inspected have no metadata, prompt, notes, or manifest files alongside the generated frames.

The helper supports reference use:

- `.agents/skills/realm-tileset-from-images/scripts/reference_images.py` can prepare context grids.
- `.agents/skills/realm-tileset-from-images/scripts/realm_tileset.py process-frame` can record `references`.
- `.agents/skills/realm-tileset-from-images/scripts/realm_tileset.py prompt-batch` can include positive references.

Why this failed:

- Reference use was optional at the command level.
- The acceptance gate did not require recorded positive references.
- The generation metadata did not have to include viewed images, role labels, or context grids.
- Some groups default to `art/reference/<group>`, but generated-reference prep appears not to have been run for most assets.
- The prompt text often says references should not be copied for style, which conflicts with wanting a reference-driven style.

Required change:

For every generated batch, require a provenance file containing:

- prompt file path and hash,
- generated JSON spec path and hash,
- visible positive reference paths,
- role labels for each reference,
- negative/rejected references excluded,
- generated output source path,
- split manifest,
- alpha/magenta QA,
- contact-sheet review path,
- manual review status.

Do not allow `tileset_coverage.py accept` unless that provenance exists and says `manual_review: accepted`.

## Why Magenta Still Appears In Some Runtime Assets

Some assets that are otherwise selected by the renderer still contain opaque magenta backgrounds.

Sample alpha checks:

| Asset | Size | Transparent pixels | Opaque magenta pixels | Result |
|---|---:|---:|---:|---|
| `assets/tiles/entities/boar/idle/front/frame_00_base.png` | 48x48 | 0 | 1465 | bad runtime cutout |
| `assets/tiles/entities/deer/idle_graze/front/frame_00_base.png` | 48x48 | 0 | 1525 | bad runtime cutout |
| `assets/tiles/entities/sheep/idle_graze/front/frame_00_base.png` | 48x48 | 0 | 1586 | bad runtime cutout |
| `assets/tiles/entities/wolf/idle/front/frame_00_base.png` | 48x48 | 0 | 1638 | bad runtime cutout |
| `assets/tiles/entities/fishing_boat/idle/front/frame_00_base.png` | 48x48 | 1708 | 0 | clean cutout |
| `assets/tiles/entities/archer/self_bow__idle/front/frame_00_base.png` | 48x48 | 1563 | 0 | clean cutout |

Group summary:

- Boar, deer, sheep, and wolf each have 24-28 fully opaque base files with large magenta backgrounds.
- Feature PNGs all have small residual magenta counts.
- Several unit/siege/ship groups have low residual magenta specks, usually tens or hundreds of pixels rather than full backgrounds.

Why this failed:

- The magenta background cleanup was not applied consistently to all promoted assets.
- Candidate frames for animals lack local metadata proving they went through `process-frame`.
- Coverage did not run alpha/magenta QA before accepting.

Required change:

Add a runtime PNG QA gate:

- actor/base images must have transparent background unless the lane explicitly uses full-square ground art,
- opaque magenta above a tiny threshold is a hard failure,
- fully opaque actor sprites are a hard failure unless explicitly marked as full-tile art,
- feature/decal sprites must be alpha-clean,
- team masks must not be silently required for neutral animals.

## Why Grid Generation Was Unstable

The prompt files often describe a 4 by 4 grid in text, but the generation target is still a freeform image request. For example:

- Archer asks for a 4 by 4 grid.
- Knight has 24 states and asks for multiple 4 by 4 sheets.
- Boar has 8 states and asks for a 3 by 3 grid.
- Feature sheets ask for 4 by 4 grids.

The skill has grid-detection and split helpers, but it does not require a prebuilt grid as the edit target for every sheet. Text-only grid instructions leave too much freedom for image generation to create wrong counts, uneven cells, labels, extra panels, or non-square grids.

Why this failed:

- The model was asked to draw the grid instead of being given an authoritative grid.
- The prompt says "at most 16 states" in a "4 by 4 grid", which does not always mean "exactly these 16 locked cells".
- Some assets intentionally use non-4x4 grids, such as boar's 3x3, so the workflow expectation was not uniform.
- Coverage validates split outputs, not whether the source sheet was a clean prebuilt edit target.

Required change:

For actor and feature sheets, make the default:

1. Build a blank 1024x1024 sheet with exact cells, gutters, and manifest.
2. Put slot labels only in the external manifest/prompt, never inside pixels.
3. Use the sheet as an image-edit base target.
4. If a style seed exists, programmatically place it into slot 1 before the edit.
5. Split only by the prepared manifest, not by inferred generated grid geometry, unless doing diagnostic recovery.

This matches the user's observation that the archer-style magenta-grid result worked better.

## Lane-By-Lane Failure Analysis

### Grounds

What worked:

- `grass.png` and `unknown.png` are real large slab-style ground tiles.
- The ground reference audit is good and has a strong slab-geometry policy.

What failed:

- 18 of 20 runtime ground files are 48x48 placeholders.
- The runtime loader ignores them for ground drawing.
- Coverage accepted them anyway.
- Ground production status already says they are placeholders, but coverage does not read that status.

Root cause:

- Ground generation and coverage were not aligned with the ground loader's actual minimum viable source contract.

Fix:

- Coverage must reject ground files below 128x128 and preferably below the current real-source threshold.
- Ground status should be machine-readable or imported into the coverage planner.
- Do not list placeholder ground files as accepted runtime art.

### Decals

What worked:

- Decal files were generated at expected paths.

What failed:

- No runtime draw path found.
- Tiny files are accepted by coverage.
- No visual evidence of style acceptance.

Root cause:

- Generation ran before the decal renderer existed.

Fix:

- Mark decals `generated_not_wired` until a decal draw path exists.
- Add a decal visual QA sheet before acceptance.

### Features

What worked:

- Feature files and manifests exist.
- Concealing features have split `back` and `front_occluder` assets.

What failed:

- Feature PNGs are not drawn by the current renderer.
- Feature prompts intentionally use watercolor/map-integrated style, not paper cutout.
- Berry bush style mismatch follows from the prompt contract.

Root cause:

- The feature lane mixed future renderer design with current runtime acceptance and used a different visual style target.

Fix:

- Add feature image drawing before accepting feature runtime art.
- Decide whether features should be paper-cutout objects, map-integrated watercolor, or a hybrid.
- If the desired style is paper cutout, change the feature prompt contract before regenerating.

### Units

What worked:

- Some generated unit outputs are alpha-clean and close to paper-cutout style.
- The archer result demonstrates that a magenta grid/source workflow can work visually.

What failed:

- Many unit paths are not selected by current runtime action names.
- Some generated units have only tiered/future action folders.
- Curated references are explicitly equipment/silhouette references, not style references.

Root cause:

- The spec exporter generated future art states while runtime animation selection remains minimal.
- The prompt contract did not force reference-driven style; it relied on text.

Fix:

- Either wire runtime action specs/variant selection to the generated taxonomy, or generate current-runtime aliases such as `idle/front/frame_00_base.png`.
- Require positive style seed/reference use for each batch.

### Animals

What worked:

- Animals are visually closer to the paper-cutout target than many other lanes.
- Boar and wolf have plain `idle` paths that can be selected by the current renderer.

What failed:

- Animal base PNGs still have opaque magenta backgrounds.
- Deer and sheep use `idle_graze`, while current runtime asks for `idle`.
- Candidate folders lack local provenance metadata.

Root cause:

- Animal generation/copy was not consistently processed through alpha cleanup.
- Animal action names drifted from the current renderer.

Fix:

- Run alpha/magenta QA before promotion.
- Add current-runtime aliases or update runtime action names.
- Require provenance metadata for animal candidates.

### Ships And Siege

What worked:

- Fishing boat is relatively clean and stylistically closer to target.
- Catapult, ram, warship, and transport have plain `idle` folders, so some can render.

What failed:

- Ships and siege drift toward small rendered RTS object/icon art.
- Siege/ships lack curated reference support according to the prompt language.
- Trebuchet generated tiered action names not selected by current runtime.

Root cause:

- Large/mechanical silhouettes need stronger style seeds and references than text prompts provide.
- Runtime action naming is inconsistent across siege units.

Fix:

- Use single accepted 1024 style seed per siege/ship family, then derive sheet variants by edit.
- Add references for siege and ships or explicitly create generated references before production.
- Align runtime action aliases.

### Buildings

What worked:

- Building files exist for many desired future states.
- Some 32x32 outputs are alpha-clean.

What failed:

- Current gameplay renderer does not request building paths such as `complete/south/frame_00_base.png`.
- Buildings use a different prompt style than units/animals.
- Large buildings became tiny icons rather than a coherent footprint/prefab style.

Root cause:

- Building art was generated for the target architecture, but the current renderer still uses entity fallback paths.
- Building footprint/slicing is not fully implemented as a runtime draw contract.

Fix:

- Add building-specific resolver and draw code before accepting building runtime art.
- Do not generate broad building state coverage until footprint/slicing rules are runtime-testable.
- Use style seeds per building family.

### Projectiles, Effects, UI

What worked:

- Files exist for the planned effect/UI assets.

What failed:

- Current projectile and marker draw paths still use glyphs/procedural shapes.
- No PNG loader found for effects/UI map draw.

Root cause:

- Planned assets were generated before runtime support.

Fix:

- Mark these lanes as `not_wired`.
- Add draw paths and screenshot smoke tests before coverage can accept them.

## Why The Skill Workflow Made This Likely

The skill contains several good policies, but the actual run found gaps between policy and enforcement.

Good policies already present:

- Separate `assets/tiles` runtime output from `art/tiles` workbench/candidates.
- Classify lanes before generation.
- Store generated references separately from user references.
- Use `art/tiles/image-spec` and `art/tiles/image-json` as source-of-truth prompt/spec exports.
- Use prebuilt helpers for zoom stops, generated references, ground variants, and sprite processing.

Missing enforcement:

- No mandatory style guide selection before generation.
- No mandatory positive reference provenance.
- No mandatory prebuilt grid input.
- No mandatory 1024 style seed workflow.
- No mandatory alpha/magenta QA.
- No mandatory runtime-selectability QA.
- No mandatory lane draw-support QA.
- No mandatory manual visual review before ledger acceptance.
- No clear separation between candidate acceptance and runtime acceptance.

## Recommended Skill Changes Before More Generation

### 0. Agreed Process Changes From Follow-Up Review

The follow-up review confirms that the next step is not to generate more remaining tiles. The next step is to harden the skill and production workflow so the next generated batch is stylistically controlled, reference-grounded, runtime-aware, and reviewable.

These decisions should be treated as mandatory production rules for future Realm tileset generation:

1. **Use canonical prompt exports only.** The skill must always start from `art/tiles/image-spec/...` and `art/tiles/image-json/...` produced by the repo exporters. If the prompt is wrong, fix the exporter or source docs and regenerate. Do not use improvised prompts for production assets.
2. **Use prebuilt grids by default.** The skill should create grid images with Python, save a slot manifest, show the grid as the image-edit base target, and split outputs by the manifest. Do not rely on text-only instructions for the model to invent the sheet layout.
3. **Use visible positive references.** Before image generation or edit, the relevant style seed, style reference, pose reference, equipment reference, or geometry reference must be visible to the image generator and labeled by role.
4. **Grow the style organically from nearest accepted neighbors.** Start with a small number of successful style anchors, then branch to the closest next asset. For example: peasant to militia, archer to crossbowman, accepted animal to related animal, fishing boat to other boats, catapult/ram to other siege. Avoid generating unrelated families independently when a closer style ancestor exists.
5. **Generate identity first, then sheets.** For units, cavalry, siege, ships, buildings, and other hard assets, first create or edit one standalone high-resolution identity image in the exact desired style. Once that idle/identity image is accepted, programmatically place it into the first grid slot and use image edit to derive the remaining states.
6. **Prefer 4 by 4 grids.** A 4 by 4 sheet is the default maximum production batch. More than 16 states must be split. Smaller grids are allowed only when the canonical prompt or slot manifest explicitly requires them.
7. **Record complete provenance.** Every generated sheet needs durable metadata: prompt/spec paths and hashes, exact prompt text sent, reference paths and roles, generated source path, candidate path, grid manifest, split manifest, alpha/magenta QA, mask QA, style review status, and runtime promotion paths.
8. **Add a style QA gate.** `accepted` must not mean "file exists." It must mean the asset passes a human-visible style check against the lane style contract and the nearest accepted style seed.
9. **Add programmatic image QA.** Runtime sprites, features, decals, projectiles, effects, and UI overlays must fail if substantial opaque magenta remains after promotion. Actor sprites should also fail if their background is fully opaque when the lane expects a cutout. Grounds need separate size/top-down tile checks.
10. **Keep runtime wiring explicit.** Assets for lanes not currently drawn by the game should be tracked as generated candidates or future/not-wired, not accepted runtime art.

The most important correction is cultural as much as technical: the generation workflow must optimize for a coherent art direction, not for reaching `100%` path coverage quickly.

### 1. Add A Required Style Matrix

Before generation, the skill should load a canonical style matrix such as:

| Lane | Style | Background | Runtime source form |
|---|---|---|---|
| Grounds | raised square terrain slab | full square, no alpha | large square source |
| Units/animals/ships/siege | tiny medieval paper-cutout standee | magenta or alpha, later alpha-clean | 48x48 runtime sprite |
| Buildings | decision needed: paper-cutout footprint pieces or painted map prefab | alpha-clean | building-specific runtime resolver |
| Features | decision needed: paper-cutout object or map-integrated watercolor | alpha-clean | feature draw path required |
| Decals | low painted overlay | alpha-clean | decal draw path required |
| Projectiles/effects/UI | compact overlay sprites | alpha-clean | draw path required |

The key decision is whether features/buildings should share the unit paper-cutout language. The current prompts say no. The user's desired style appears closer to yes for many upright objects.

This should probably become a dedicated style contract inside the tileset skill rather than scattered prompt prose. It should define:

- the approved style anchors,
- which asset families inherit from each anchor,
- which accepted asset is the nearest style ancestor for each new asset,
- which reference images are style references versus equipment/pose references,
- which lanes are intentionally not paper cutout,
- examples of accepted and rejected outputs.

The style contract should be consulted before every generated batch. If the contract and the exported prompt disagree, generation should stop until the exporter or contract is corrected.

### 2. Make Reference Roles Mandatory

Every generation prompt should list references by role:

- style reference,
- pose reference,
- equipment reference,
- geometry reference,
- negative/rejected reference.

The generator-facing prompt should say exactly what to borrow and what not to borrow from each.

### 3. Use Prebuilt Grids By Default

For sheet generation:

- Generate the grid programmatically.
- Make it visible as the base/edit target.
- Do not ask the model to invent the grid.
- Split by manifest.
- Reject outputs with extra rows/columns, merged cells, labels, or shifted panels.

### 4. Add The Seed-Then-Sheet Workflow

For difficult assets:

1. Generate one 1024x1024 standalone source in the exact style.
2. Review it as a style seed.
3. Programmatically place it into slot 1 of a prepared grid.
4. Ask image generation to edit the grid into the remaining states while preserving the seed style.
5. Split, process, alpha-clean, and review.

This should become the default for cavalry, infantry, ships, siege, and buildings.

### 5. Make Coverage Runtime-Aware

Coverage should reject or downgrade:

- ground files below loader minimum size,
- entity paths not selected by current runtime,
- features/decals/effects without draw paths,
- direct runtime files with opaque magenta,
- records without accepted manual review,
- records without positive reference provenance when references are available.

Suggested statuses:

- `missing`,
- `generated_candidate`,
- `runtime_file_exists`,
- `not_runtime_wired`,
- `runtime_placeholder`,
- `alpha_failed`,
- `style_needs_review`,
- `style_rejected`,
- `accepted_runtime`.

### 6. Add Lane Smoke Tests

Before accepting a lane:

- Render a deterministic lab/map screenshot that contains the asset.
- Confirm the expected PNG path was requested or loaded.
- Check the screenshot for nonblank/non-magenta pixels at the expected location.
- Store the screenshot path in the ledger review record.

For non-wired lanes, coverage should report `not_runtime_wired`, not `accepted`.

## Recommended Order Of Work

1. Stop treating the current coverage report as visual completion.
2. Patch coverage to detect placeholders, alpha/magenta failure, and missing runtime draw support.
3. Decide the canonical style matrix, especially for features/buildings.
4. Update prompt exporter style contracts at `scripts/export_image_generation_prompts.py`, not by hand-editing generated Markdown.
5. Add runtime aliases or renderer support for the action folders you actually want to use.
6. Regenerate a tiny proof batch only after the above gates exist.
7. Use the seed-then-grid workflow for the next hard asset, probably one cavalry or siege unit, and prove it in-game before scaling up.

## Bottom Line

The generation run did not fail because image generation is unusable. It failed because the process lacked hard gates between:

- generated vs reviewed,
- reviewed vs runtime-clean,
- runtime-clean vs runtime-selected,
- runtime-selected vs visually coherent.

The paper-cutout style can work. The archer, animals, and fishing boat show the direction. The next iteration should focus on enforcing the style and evidence pipeline before generating more assets.
