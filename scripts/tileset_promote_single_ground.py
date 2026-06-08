#!/usr/bin/env python3
"""Promote one generated ground tile through the standard Realm evidence path."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
from pathlib import Path

from PIL import Image


DEFAULT_GENERATED_ROOT = Path.home() / ".codex" / "generated_images"
GROUND_REFERENCES = [
    "art/tiles/reference/grounds/current/grass.png",
    "art/tiles/reference/grounds/current/blank.png",
]


def latest_png(root: Path) -> Path:
    files = [path for path in root.rglob("*.png") if path.is_file()]
    if not files:
        raise SystemExit(f"no png files found under {root}")
    return max(files, key=lambda path: path.stat().st_mtime)


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run(cmd: list[str]) -> None:
    subprocess.run(cmd, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", required=True, help="Ground slug, for example rocky.")
    source_group = parser.add_mutually_exclusive_group(required=True)
    source_group.add_argument("--source", help="Generated source PNG to promote.")
    source_group.add_argument("--latest-generated", action="store_true", help="Use the newest PNG under --generated-root.")
    parser.add_argument("--generated-root", default=str(DEFAULT_GENERATED_ROOT))
    parser.add_argument("--version", default="v001-imagegen")
    parser.add_argument("--candidate-dir", help="Override candidate directory.")
    parser.add_argument("--runtime", help="Override runtime PNG path.")
    parser.add_argument("--work-id", help="Override coverage work id.")
    parser.add_argument("--prompt", help="Override canonical prompt export path.")
    parser.add_argument("--spec", help="Override canonical JSON spec path.")
    parser.add_argument("--size", type=int, default=1024, help="Runtime output size.")
    parser.add_argument("--review-note", required=True)
    parser.add_argument("--review-artifact", help="Override post-promotion review sheet path.")
    parser.add_argument("--reference-image", action="append", default=[], help="Extra reference image for review/provenance.")
    parser.add_argument("--accept-coverage", action="store_true", help="Accept coverage after gate pass. Use only after the source has already been visually accepted.")
    parser.add_argument("--force", action="store_true", help="Allow overwriting existing candidate files in the target directory.")
    args = parser.parse_args()

    slug = args.slug
    source = Path(args.source) if args.source else latest_png(Path(args.generated_root))
    if not source.exists():
        raise SystemExit(f"source image not found: {source}")

    candidate_dir = Path(args.candidate_dir or f"art/tiles/candidates/grounds/{slug}/{args.version}")
    runtime = Path(args.runtime or f"assets/tiles/grounds/{slug}.png")
    work_id = args.work_id or f"grounds/{slug}/base"
    prompt = args.prompt or f"art/tiles/image-spec/grounds/{slug}.md"
    spec = args.spec or f"art/tiles/image-json/grounds/{slug}.json"
    review_artifact = args.review_artifact or f"build/tileset-review/ground-{slug.replace('_', '-')}-regenerated.png"
    batch_path = Path(f"build/tileset-single-ground-{slug}.json")
    source_out = candidate_dir / "source.png"
    split_dir = candidate_dir / "split"
    grid_manifest = candidate_dir / "grid-manifest.json"
    slot_map = candidate_dir / "slot-map.json"

    if source_out.exists() and not args.force:
        raise SystemExit(f"{source_out} already exists; pass --force or choose --version")

    candidate_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, source_out)

    with Image.open(source_out) as img:
        width, height = img.size
    if width != height:
        raise SystemExit(f"ground source must be square, got {width}x{height}: {source_out}")

    if runtime.exists():
        shutil.copy2(runtime, candidate_dir / f"previous-runtime-{slug}.png")

    references = GROUND_REFERENCES + args.reference_image
    write_json(
        grid_manifest,
        {
            "schema": "realm.grid_manifest.v1",
            "source_image": source_out.as_posix(),
            "size": width,
            "columns": 1,
            "rows": 1,
            "slots": [{"slot": 1, "row": 1, "column": 1, "box": [0, 0, width, height]}],
        },
    )
    write_json(
        slot_map,
        {
            "slots": [
                {
                    "slot": 1,
                    "runtime": runtime.as_posix(),
                    "candidate": (split_dir / f"{slug}.png").as_posix(),
                    "size": args.size,
                    "work_id": work_id,
                    "canonical_prompt_export": prompt,
                    "canonical_json_spec": spec,
                    "style_contract": "realm_ground_slab_small_tile",
                    "reference_images": references,
                }
            ]
        },
    )
    write_json(
        batch_path,
        {
            "batch": [
                {
                    "work_id": work_id,
                    "group": "grounds",
                    "slug": slug,
                    "style_contract": "realm_ground_slab_small_tile",
                    "required_paths": [runtime.as_posix()],
                    "prompt": prompt,
                    "spec": spec,
                }
            ]
        },
    )

    ledger_cmd = [
        "python",
        "scripts/tileset_generation_ledger_append.py",
        "--id",
        f"{slug}-{args.version}-source",
        "--asset-id",
        work_id,
        "--canonical-prompt-export",
        prompt,
        "--canonical-json-spec",
        spec,
        "--imagegen-operation",
        "built_in_image_gen_generate",
        "--candidate-output-path",
        source_out.as_posix(),
        "--review-status",
        "candidate_generated_pending_promotion",
        "--notes",
        f"Generated replacement {slug} ground slab from canonical prompt/spec and current ground references.",
    ]
    for ref in references:
        ledger_cmd.extend(["--reference-image", ref])
    run(ledger_cmd)

    run(
        [
            "python",
            "scripts/tileset_promote_grid_batch.py",
            "--sheet",
            source_out.as_posix(),
            "--grid-manifest",
            grid_manifest.as_posix(),
            "--slot-map",
            slot_map.as_posix(),
            "--candidate-dir",
            split_dir.as_posix(),
            "--batch-id",
            f"{slug}-{args.version}",
            "--clean-mode",
            "none",
            "--review-note",
            args.review_note,
            "--review-artifact",
            review_artifact,
        ]
    )

    gate_cmd = [
        "python",
        "scripts/tileset_visual_review_gate.py",
        "--batch",
        batch_path.as_posix(),
        "--out",
        review_artifact,
        "--label-strip",
        "assets/tiles/grounds/",
        "--report-out",
        str(Path(review_artifact).with_name(Path(review_artifact).stem + "-gate.json")),
    ]
    for ref in references:
        gate_cmd.extend(["--reference-image", ref])
    run(gate_cmd)

    if args.accept_coverage:
        run(["python", "scripts/tileset_coverage.py", "accept", "--work-id", work_id, "--review", args.review_note])

    print(
        json.dumps(
            {
                "source": source_out.as_posix(),
                "runtime": runtime.as_posix(),
                "grid_manifest": grid_manifest.as_posix(),
                "slot_map": slot_map.as_posix(),
                "batch": batch_path.as_posix(),
                "review_artifact": review_artifact,
                "accepted_coverage": bool(args.accept_coverage),
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
