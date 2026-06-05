# Realm Tileset Next Improvement Pass Plan - 2026-06-05

## Status Correction

The tileset is **not done**.

The latest `scripts/tileset_quality_audit.py` result of `issues=0` only proves a narrow set of checks:

- ground files are no longer placeholder-sized,
- promoted PNGs do not have obvious opaque magenta contamination,
- simple runtime entity `idle` / `death` aliases exist,
- SDL code has image-loading/draw-path evidence for features, decals, projectiles, and effects/UI.

It does **not** prove production visual quality, style consistency, reference correctness, or that generated assets are acceptable art.

The next pass should treat the current tree as a mixed prototype/runtime wiring state, not as a finished tileset.

## Evidence Gathered In This Pass

Representative evidence sheets were generated under:

```text
build/tileset-next-pass-evidence/decals.png
build/tileset-next-pass-evidence/effects_ui.png
build/tileset-next-pass-evidence/archer_crossbow_aim_back.png
build/tileset-next-pass-evidence/ground_refs_old.png
build/tileset-next-pass-evidence/ground_refs_new.png
```

These are diagnostic build artifacts, not production art.

## What Is Acceptable Right Now

### Grounds Are Mechanically Fixed, But Reference Source Needs Updating

The 18 missing/placeholder ground files were replaced with 1024 by 1024 runtime PNGs. They are no longer ignored by the ground loader and they are broadly in the raised-square ground family.

However, the generation used the older mixed ground reference atlas. The better source style is the newer small-tile-optimized ground reference family:

```text
art/reference/ground/shades-of-grey/
art/reference/ground/blank.png
art/reference/ground/grass.png
```

The attached user reference shows the target direction more clearly: a high-contrast rounded raised-square slab with a thick dark outer edge, bright upper/left bevel, soft painterly green interior, and strong readability when downscaled.

The next pass should update `art/tiles/reference/grounds` so the canonical positive ground references point to the newer sources, not the old `16x-ground` / `16x-water-edges` atlas.

Current old references observed:

```text
art/tiles/reference/grounds/examples/16x-ground.png
art/tiles/reference/grounds/examples/16x-water-edges.png
art/tiles/reference/grounds/examples/blank-1.png
art/tiles/reference/grounds/examples/blank-2.png
art/tiles/reference/grounds/examples/blank-3.png
art/tiles/reference/grounds/examples/grass.png
art/tiles/reference/grounds/generated/unknown/v001-user-grey-template/source.png
```

Recommended reference update:

- promote `art/reference/ground/blank.png` as the canonical geometry reference,
- promote `art/reference/ground/grass.png` as the canonical painted grass/style reference,
- promote `art/reference/ground/shades-of-grey/*.png` as contrast/lighting/edge references,
- demote old `16x-ground` and `16x-water-edges` to diagnostic/legacy references only,
- update ground prompts/provenance to say the newer references are mandatory positive references.

## What Clearly Did Not Work

### 1. Decals Are Programmatic Mockups, Not Production Assets

Current files under `assets/tiles/decals` are tiny 48 by 48 PNGs, mostly around 108-184 bytes. They are mostly transparent with a few simple dots or lines.

Examples from inspection:

```text
assets/tiles/decals/cobble_patch.png size=48x48 bytes=166
assets/tiles/decals/crates_barrels.png size=48x48 bytes=153
assets/tiles/decals/farm_tracks.png size=48x48 bytes=114
assets/tiles/decals/packed_path.png size=48x48 bytes=108
assets/tiles/decals/road.png size=48x48 bytes=112
assets/tiles/decals/wheel_ruts.png size=48x48 bytes=112
```

These should be classified as `mockup_runtime_placeholder`, not accepted production art.

Failure mode:

- the audit checked for magenta and wiring,
- the renderer can now draw decals,
- but the images themselves are simple procedural placeholders.

Required fix:

- add a decal-specific production quality gate,
- regenerate decals using canonical prompt exports and visible references,
- keep them as alpha-clean overlays, but with actual painted detail,
- require manual/contact-sheet review before runtime acceptance.

### 2. Most Effects/UI Assets Are Also Programmatic Mockups

Many files under `assets/tiles/effects-ui` are also 48 by 48 low-byte placeholder shapes. They are technically transparent and drawable, but they are not production-quality art.

Examples from inspection:

```text
assets/tiles/effects-ui/attack_marker.png size=48x48 bytes=216
assets/tiles/effects-ui/build_marker.png size=48x48 bytes=217
assets/tiles/effects-ui/gather_marker.png size=48x48 bytes=217
assets/tiles/effects-ui/move_marker.png size=48x48 bytes=216
assets/tiles/effects-ui/rain_frame_1.png size=48x48 bytes=129
assets/tiles/effects-ui/snowfall_frame_1.png size=48x48 bytes=129
assets/tiles/effects-ui/storm_rain_frame_1.png size=48x48 bytes=129
```

A few projectile images are more real than the markers, for example:

```text
assets/tiles/effects-ui/arrow_projectile.png size=32x32 bytes=1127
assets/tiles/effects-ui/catapult_boulder_projectile.png size=32x32 bytes=2112
assets/tiles/effects-ui/tower_bolt_projectile.png size=32x32 bytes=1261
assets/tiles/effects-ui/warship_shot_projectile.png size=32x32 bytes=1954
```

But even these need style review against the target visual language.

Required fix:

- split `effects-ui` into separate subcontracts:
  - command markers,
  - selection/range rings,
  - weather overlays,
  - hit/spark/dust/splash effects,
  - projectile sprites,
  - build previews,
- decide which should stay procedural renderer shapes and which should be generated art,
- if procedural is preferred for UI clarity, do not count those as missing art,
- if generated art is preferred, regenerate from canonical effects/UI prompts and references.

### 3. Entity Style Is Still Inconsistent

The current entity tree contains assets that are alpha-clean but stylistically wrong.

User example:

```text
assets/tiles/entities/archer/crossbow__aim/back
```

Observed files:

```text
frame_00_base.png size=48x48 bytes=2575 transparent=1725/2304 opaque=375/2304
frame_01_base.png size=48x48 bytes=2575 transparent=1725/2304 opaque=375/2304
```

The problem is not alpha. The problem is visual language. The asset does not match the stronger magenta-grid/paper-cutout archer style that worked better in earlier generation.

Required fix:

- stop accepting entity frames based on alpha and path existence,
- define a canonical unit style seed set,
- choose one accepted archer seed as the style ancestor for all archer/crossbow states,
- regenerate one identity frame first,
- place that accepted seed into slot 1 of a prebuilt grid,
- use edit mode to branch into aim/fire/reload/death states,
- reject states that drift into a different render style.

### 4. Runtime Wiring Was Necessary But Not Sufficient

The SDL wiring patch was useful because files in these lanes can now be drawn:

- features,
- decals,
- projectiles,
- effects/UI.

But wiring a placeholder lane just makes placeholders visible. The acceptance model must separate:

- `has_runtime_path`,
- `has_draw_path`,
- `has_nonplaceholder_art`,
- `matches_style_contract`,
- `manual_review_accepted`,
- `runtime_accepted`.

The previous audit collapsed too many of these into one green state.

### 5. Projectile Manifest Paths Need Normalization

Projectile manifests currently point at effects/UI files using paths like:

```text
../effects-ui/arrow_projectile.png
../effects-ui/catapult_boulder_projectile.png
```

From a manifest in `assets/tiles/projectiles/<slug>/manifest.json`, that relative path is awkward and should be normalized. The loader can tolerate this, but the manifest itself should not depend on tolerant fallback behavior.

Required fix:

- normalize projectile manifest image paths to an unambiguous convention,
- either use repo-root-relative paths or paths relative to `assets/tiles/projectiles/<slug>/`,
- add a manifest validation check that resolves every referenced image exactly.

### 6. The Ground Reference Tree Is Stale

The current `art/tiles/reference/grounds` tree still mixes old references and generated unknown references. That encourages generators to copy the wrong tile language.

The newer reference family is better optimized for small runtime tiles:

```text
art/reference/ground/blank.png
art/reference/ground/grass.png
art/reference/ground/shades-of-grey/grey-1.png
art/reference/ground/shades-of-grey/grey-2.png
art/reference/ground/shades-of-grey/grey-3.png
art/reference/ground/shades-of-grey/grey-4.png
art/reference/ground/shades-of-grey/grey-5.png
art/reference/ground/shades-of-grey/grey-6.png
art/reference/ground/shades-of-grey/high-contrast.png
```

Required fix:

- update the canonical ground reference folder to use these,
- document roles for each one:
  - geometry reference,
  - contrast reference,
  - grass material reference,
  - small-tile readability reference,
- mark old references as legacy/negative or diagnostic only.

## Additional Problems To Add To The Next Pass

### Mockup Detection Is Missing

The audit needs a new `mockup_like_asset` class.

Useful heuristics:

- PNG file size below a threshold for the lane, for example decals/effects under 1 KB,
- very low color count,
- extreme transparency with only a few simple line/dot pixels,
- identical or near-identical marker shapes across many unrelated assets,
- no candidate provenance,
- no manual review status,
- generated by a deterministic placeholder script rather than Image Gen.

This should not automatically fail true minimalist UI assets, but it should block production acceptance unless the asset is explicitly allowed as procedural/minimal UI.

### Provenance Still Needs To Be Mandatory Outside Grounds

The new ground batch has provenance. Most older generated assets do not.

Required provenance fields:

- canonical prompt path and hash,
- canonical JSON spec path and hash,
- exact prompt sent to Image Gen,
- generated source path under `C:\Users\Edward\.codex\generated_images\...`,
- candidate path under `art/tiles/candidates/...`,
- visible reference paths and roles,
- negative/rejected references,
- prebuilt grid manifest,
- split manifest,
- alpha/magenta QA,
- style-review status,
- manual-review status,
- runtime promotion path.

Any production asset missing this should be `unreviewed_runtime_asset`, not accepted.

### Style Contracts Need To Be Concrete, Not Just Prompt Prose

The next pass needs a canonical style matrix with hard examples.

Proposed lanes:

| Lane | Desired style | Runtime alpha | Positive style ancestors |
|---|---|---|---|
| Grounds | raised square painted slab, optimized for small tiles | opaque | `art/reference/ground/blank.png`, `grass.png`, `shades-of-grey` |
| Decals | painted low overlay matching slab perspective | alpha-clean | new decal seed needed |
| Features | decision needed: paper-cutout object vs map-integrated feature | alpha-clean or tile-integrated by subtype | forest/reeds/berries seed needed |
| Units | paper-cutout standee on transparent background | alpha-clean | peasant, good archer seed |
| Animals | paper-cutout bestiary standee | alpha-clean | accepted boar/deer/sheep/wolf seed, if reviewed |
| Siege/ships | paper-cutout object with readable operator/shape | alpha-clean | fishing boat, siege seed needed |
| Buildings | decision needed: paper-cutout building or painted map object | alpha-clean or footprint art by subtype | no accepted full set yet |
| Projectiles | compact readable painted cutout | alpha-clean | arrow/bolt/boulder seed needed |
| Effects/UI | split between procedural UI and generated art | alpha-clean | explicit per-subtype seed needed |

The most important unresolved style decision is still buildings/features: should they share the paper-cutout object language, or should they remain map-integrated painted objects?

### Prompt Exports Must Be Correct Before Generation

The skill now says canonical prompt exports are mandatory. The next pass should enforce that in code:

- no production generation from improvised prompts,
- if a prompt says the wrong style, fix `scripts/export_image_generation_prompts.py` or its source docs first,
- regenerate `art/tiles/image-spec` and `art/tiles/image-json`,
- record prompt/spec hashes in provenance.

### Prebuilt Grids Worked, But Need Better Lane Defaults

Grounds should use 2 by 2 grids when high-resolution source tiles are needed.

Actors/projectiles/effects should usually use 4 by 4 magenta grids unless the slot map says otherwise.

Decals may need a hybrid:

- neutral or transparent-looking cell backgrounds for review,
- magenta alpha-removal background only where needed,
- fixed gutters and crop boxes,
- no generated labels inside crop boxes.

### Need A Style Seed Cascade For Each Lane

Do not regenerate all missing/wrong assets independently.

Use this order:

1. Pick one accepted seed per lane.
2. Generate or repair one 1024 by 1024 identity asset in the exact target style.
3. Review it manually/contact-sheet style.
4. Place the identity seed into slot 1 of a prebuilt grid.
5. Use Image Gen edit mode to derive nearby states.
6. Branch outward to closest related assets only after the source seed works.

Example cascades:

- `peasant` -> `militia` -> `spearman` -> `pikeman`.
- good `archer` seed -> `crossbow` states -> `tower bolt` projectile style.
- `fishing_boat` -> `transport` -> `warship`.
- `catapult` seed -> `trebuchet` -> `ram`.
- accepted `grass` slab -> `meadow` / `dirt` / `road` / `mud`.
- decal seed -> `scuffs` -> `packed_path` -> `wheel_ruts` -> `road` decal.

## Proposed Next-Pass Order

### Phase 1: Fix References And Audit Gates

1. Update `art/tiles/reference/grounds` to use the newer ground reference family.
2. Add a `mockup_like_asset` quality gate for decals/effects/UI and possibly projectiles.
3. Add a `missing_provenance` / `manual_review_pending` gate for production acceptance.
4. Normalize projectile manifest image paths and add manifest-reference validation.
5. Update the quality audit so `issues=0` means production-quality lanes, not just path/wiring health.

### Phase 2: Reclassify Current Assets

Mark current assets into one of these statuses:

- `accepted_runtime_art`,
- `runtime_placeholder`,
- `mockup_runtime_placeholder`,
- `wired_unreviewed_art`,
- `wrong_style_runtime_art`,
- `needs_regeneration`,
- `procedural_by_design`.

Expected classifications:

- new grounds: probably `wired_unreviewed_art` or `accepted_runtime_art` after reference correction review,
- decals: `mockup_runtime_placeholder`,
- most effects/UI markers/weather: `mockup_runtime_placeholder` or `procedural_by_design`,
- some projectiles: `wired_unreviewed_art`,
- archer crossbow aim/back and many other entities: `wrong_style_runtime_art`,
- older buildings/siege/ships: likely `wrong_style_runtime_art` unless a contact-sheet review proves otherwise.

### Phase 3: Regenerate Small Controlled Batches

Recommended first batch:

1. Ground reference refresh and maybe re-run 2-4 key grounds using the new reference family to prove the style.
2. One decal style seed, probably `scuffs` or `packed_path`.
3. One effects/UI subgroup decision: either keep command markers procedural or regenerate them as art.
4. One unit style repair: archer/crossbow states from the good archer seed.

Do not regenerate the full tileset until these seeds pass review.

### Phase 4: Runtime Visual Smoke

After each accepted small batch:

- run a native tileset lab or SDL smoke path,
- capture screenshots/contact sheets,
- confirm the asset is actually visible in-game,
- record screenshot/evidence path in provenance.

## Specific Work Items

### Ground References

- [ ] Replace or restructure `art/tiles/reference/grounds/examples` around the newer reference family.
- [ ] Add roles to `art/tiles/reference/grounds/README.md`.
- [ ] Mark old `16x-ground` and `16x-water-edges` as legacy/diagnostic.
- [ ] Add the attached/user-style grass tile as the target visual description if it is saved into the repo later.

### Decals

- [ ] Mark all current `assets/tiles/decals/*.png` as mockups.
- [ ] Add decal mockup detection to `scripts/tileset_quality_audit.py`.
- [ ] Create a decal positive reference sheet.
- [ ] Generate one decal identity seed.
- [ ] Regenerate `scuffs`, `packed_path`, `wheel_ruts`, and `road` as real alpha-clean overlays.

### Effects/UI

- [ ] Decide which UI markers remain procedural.
- [ ] Do not count procedural UI markers as missing generated art.
- [ ] Mark current generated/simple marker PNGs as mockups unless explicitly retained.
- [ ] Regenerate true effects: hit spark, dust, splash, weather particles, projectile impact.
- [ ] Review projectile images separately from command markers.

### Entities

- [ ] Build representative contact sheets per unit/building group.
- [ ] Mark wrong-style assets, starting with `assets/tiles/entities/archer/crossbow__aim/back`.
- [ ] Choose the accepted archer style seed.
- [ ] Regenerate crossbow states by edit from the accepted archer seed.
- [ ] Add style-review metadata before promotion.

### Runtime/Manifest Hygiene

- [ ] Normalize projectile manifest image paths.
- [ ] Add manifest path resolution checks.
- [ ] Keep renderer wiring, but do not let wiring imply acceptance.
- [ ] Add a lane status report that says which assets are visible, which are placeholders, and which are accepted.

## Acceptance Definition For The Next Pass

An asset should count as accepted only if all of these are true:

1. The asset has a canonical prompt/spec source.
2. The asset used visible positive references with recorded roles.
3. The asset was generated or edited from a prebuilt grid/seed where appropriate.
4. The asset has provenance.
5. The asset passes alpha/magenta QA.
6. The asset passes mockup/placeholder QA.
7. The asset matches the lane style contract.
8. The asset is actually requested by runtime code or explicitly marked future-only.
9. The asset appears in a review sheet or runtime screenshot.
10. Manual/style review is accepted.

Only then should coverage or ledger status say `accepted_runtime_art`.

## Key Takeaway

The main failure was not just bad generation. It was that the workflow treated generated file existence, runtime wiring, and production art acceptance as the same thing.

The next improvement pass should first repair that acceptance model, then regenerate small style-seeded batches using the newer ground references and explicit lane style contracts.
