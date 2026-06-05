# Realm Tileset Production Audit Tooling

This pass adds a stricter audit layer for the tileset pipeline. The goal is to avoid another false completion signal where runtime files exist and load, but many are stale, procedural mockups, wrong style, or not traceable to the reference images and canonical prompts.

## Tool

Run:

```sh
python scripts/tileset_production_audit.py --json-out build/tileset-production-audit.json --md-out build/tileset-production-audit.md --sheet-dir build/tileset-production-audit
```

Outputs:

- `build/tileset-production-audit.json`: machine-readable issues and per-image stats.
- `build/tileset-production-audit.md`: human-readable summary and issue list.
- `build/tileset-production-audit/*.png`: contact sheets for visual review groups.

Related helper tools:

```sh
python scripts/tileset_make_grid_template.py --out art/tiles/workbench/grids/unit-4x4.png --cols 4 --rows 4 --size 1024 --label unit-idle-4x4
```

For actor sheets, prefer magenta-filled cells with gutters:

```sh
python scripts/tileset_make_grid_template.py --out art/tiles/workbench/grids/deer-4x4.png --cols 4 --rows 4 --size 1024 --cell-fill magenta --gutter 8
```

Capture generated output immediately:

```sh
python scripts/tileset_capture_generated_output.py --input C:\Users\Edward\.codex\generated_images\...\generated.png --out art/tiles/candidates/animals/deer/v001/batch_source.png --prompt-file art/tiles/candidates/animals/deer/v001/prompt.txt --grid-template art/tiles/workbench/grids/deer-4x4.png
```

Promote a reviewed grid batch:

```sh
python scripts/tileset_promote_grid_batch.py --sheet art/tiles/candidates/animals/deer/v001/batch_source.png --grid-manifest art/tiles/workbench/grids/deer-4x4.manifest.json --slot-map art/tiles/candidates/animals/deer/v001/slot-map.json --candidate-dir art/tiles/candidates/animals/deer/v001/split --batch-id deer-v001 --clean-mode actor --review-note "accepted after contact-sheet review" --review-artifact build/tileset-review/deer-v001.png --accept-coverage
```

Review and accept existing runtime art without regenerating:

```sh
python scripts/tileset_review_existing_batch.py --batch build/tileset-next-batch.json --out build/tileset-review/boar-batch.png --label-strip assets/tiles/entities/boar/ --review-note "matches canonical boar paper-cutout animal style" --write-review --append-ledger --accept
```

Record rejected generations:

```sh
python scripts/tileset_record_rejected_generation.py --id deer-v001-rejected-crop-box --candidate art/tiles/candidates/animals/deer/v001/batch_source.png --canonical-prompt-export art/tiles/image-spec/animals/deer.md --canonical-json-spec art/tiles/image-json/animals/deer.json --grid-template art/tiles/workbench/grids/deer-4x4.png --reason "visible white crop boxes around every sprite" --review-artifact build/tileset-review/deer-rejected.png
```

```sh
python scripts/tileset_generation_ledger_append.py --canonical-prompt-export art/tiles/image-spec/entities/peasant.md --reference-image art/reference/ground/grass.png --premade-grid-path art/tiles/workbench/grids/unit-4x4.png --accepted-runtime-path assets/tiles/entities/peasant/idle/front/frame_00_base.png --review-status unreviewed
```

Schemas/examples:

- `art/tiles/reviews/production-review.schema.json`
- `art/tiles/reviews/production-review.example.json`
- `art/tiles/generation-ledger.schema.json`

## Why this exists

`scripts/tileset_quality_audit.py` is a loader/runtime blocker gate. It can catch things like magenta leakage, alpha background errors, missing aliases, and runtime wiring gaps. That is necessary, but not enough.

The production audit checks whether the files are credible production art and whether future agents can prove where they came from.

## Checks implemented

- `missing_current_ground_reference`: the active ground-reference workspace is missing the current small-tile reference set.
- `stale_ground_reference_present`: old ground references still exist in the active reference tree where they can be accidentally reused.
- `ground_prompt_uses_stale_reference`: canonical ground prompt exports still mention old reference paths.
- `mockup_like_runtime_asset`: decals/effects/UI PNGs look like tiny procedural placeholders based on size, byte count, visible pixels, and color count.
- `bright_crop_box_artifact`: entity sprites contain bright rectangular crop boxes or transparency-checker remnants.
- `duplicate_entity_animation_frame`: exact duplicate entity base frames inside one action/direction.
- `magenta_leakage`: opaque or semi-opaque magenta remains in runtime art.
- `manifest_image_reference_unresolved`: a runtime manifest references an image path that does not resolve relative to that manifest.
- `missing_production_review_index`: there is no production review index for final asset status.
- `missing_generation_ledger`: there is no machine-readable ledger proving which prompts, references, grids, seeds, or candidates were used.
- `missing_asset_production_review`: a runtime PNG has no explicit accepted/placeholder/needs-regeneration review state.
- `missing_generation_provenance`: a runtime PNG is not linked from candidate, prompt, review, generation ledger, or production ledger text, so later audits cannot trace it.
- `accepted_asset_missing_style_contract`: an accepted asset does not explicitly record the Realm paper-cutout small-tile style contract.

## What this can and cannot automate

The audit can prove stale references, unresolved manifest paths, duplicate frames, magenta leakage, missing review/provenance, and obvious low-information mockups.

It cannot prove detailed style correctness from pixels alone. A cavalry sprite can be the wrong rendering style while still having transparency, file size, and dimensions that look plausible. For that reason, final acceptance requires review metadata.

## Review/provenance schema to collect going forward

Create `art/tiles/reviews/production-review.json`:

```json
{
  "assets": {
    "assets/tiles/entities/peasant/idle/front/frame_00_base.png": {
      "status": "accepted_runtime_art",
      "style_contract": "realm_paper_cutout_small_tile",
      "canonical_prompt_export": "art/tiles/image-spec/entities/peasant.md",
      "reference_images": [
        "art/reference/ground/grass.png",
        "art/tiles/candidates/entities/peasant/idle_seed.png"
      ],
      "generation_ledger_id": "2026-06-05-peasant-idle-v001",
      "reviewed_at": "2026-06-05",
      "reviewer": "human",
      "notes": "Matches paper-cutout style, transparent background, 4x4 extraction source accepted."
    }
  },
  "patterns": [
    {
      "glob": "assets/tiles/effects-ui/debug_*.png",
      "status": "procedural_by_design",
      "style_contract": "debug_overlay",
      "notes": "Debug-only overlay; not production art."
    }
  ]
}
```

Allowed `status` values:

- `accepted_runtime_art`
- `procedural_by_design`
- `runtime_placeholder`
- `mockup_runtime_placeholder`
- `wrong_style_runtime_art`
- `needs_regeneration`

Only `accepted_runtime_art` with `style_contract: realm_paper_cutout_small_tile` should count as finished production art.

## Generation ledger to collect going forward

The audit is much stronger if each generation records a machine-readable event in `art/tiles/generation-ledger.jsonl`.

Recommended fields:

- `id`
- `created_at`
- `asset_ids`
- `canonical_prompt_export`
- `prompt_sha256`
- `reference_images`
- `reference_image_sha256`
- `premade_grid_path`
- `seed_asset_path`
- `imagegen_operation`
- `candidate_output_paths`
- `split_output_paths`
- `accepted_runtime_paths`
- `review_status`
- `notes`

`art/tiles/production-ledger.jsonl` remains the coverage acceptance ledger written by `scripts/tileset_coverage.py accept`. `art/tiles/generation-ledger.jsonl` is the generation/provenance ledger. Production audit now reads both so coverage acceptance and generation provenance are bridged instead of treated as unrelated evidence.

This gives future audits enough evidence to answer:

- Did this use the canonical prompt export?
- Which references were attached?
- Was a premade grid used?
- Was the idle seed generated and then reused?
- Which candidate became the runtime asset?
- Was the final asset manually accepted or marked for regeneration?

## Future generation workflow

1. Export canonical prompts from `scripts/export_image_generation_prompts.py`; do not improvise prompts.
2. Build the premade grid template programmatically for the target sheet size.
3. Generate or refine the closest already-accepted seed first.
4. For a unit, nail the idle/front seed before branching into related directions and actions.
5. Record every generation in `art/tiles/generation-ledger.jsonl`.
6. Split/process into runtime assets.
7. Run `scripts/tileset_quality_audit.py` for loader blockers.
8. Run `scripts/tileset_production_audit.py` for production readiness.
9. Review contact sheets and update `art/tiles/reviews/production-review.json`.
10. Only then accept coverage or call the tileset complete.

## Immediate gaps this tool is expected to expose

- Decals are likely to be flagged as mockup-like runtime assets.
- Many `effects-ui` images are likely to be flagged as mockup-like runtime assets.
- Existing runtime assets likely lack production review metadata.
- The ground reference workspace likely lacks the new current reference mirror.
- Old ground references may still be present in the active tree.
- Some entity animation states may contain duplicate frames or wrong-style frames that need review metadata to distinguish from accepted art.
