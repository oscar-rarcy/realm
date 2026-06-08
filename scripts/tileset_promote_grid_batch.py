#!/usr/bin/env python3
"""Split a generated grid sheet, promote runtime PNGs, and record evidence.

Slot map format:

{
  "slots": [
    {
      "slot": 1,
      "runtime": "assets/tiles/...",
      "candidate": "optional candidate output path",
      "size": 48,
      "work_id": "optional coverage work id",
      "canonical_prompt_export": "art/tiles/image-spec/...",
      "canonical_json_spec": "art/tiles/image-json/...",
      "style_contract": "realm_paper_cutout_small_tile"
    }
  ]
}
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from PIL import Image


EXPECTED_STYLE_BY_RUNTIME = (
    ("assets/tiles/decals/", "realm_simplified_hand_painted_ground_decal"),
    ("assets/tiles/effects-ui/", "realm_effect_overlay"),
    ("assets/tiles/entities/", "realm_paper_cutout_small_tile"),
    ("assets/tiles/features/", "realm_paper_cutout_small_tile"),
    ("assets/tiles/grounds/", "realm_ground_slab_small_tile"),
    ("assets/tiles/projectiles/", "realm_projectile_cutout"),
)


def sha256_or_none(path_text: str | Path | None) -> str | None:
    if not path_text:
        return None
    path = Path(path_text)
    if not path.exists() or not path.is_file():
        return None
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def default_style(runtime: str) -> str:
    for prefix, style in EXPECTED_STYLE_BY_RUNTIME:
        if runtime.startswith(prefix):
            return style
    return "realm_paper_cutout_small_tile"


def clean_image(img: Image.Image, mode: str, size: int) -> Image.Image:
    rgba = img.convert("RGBA")
    data = bytearray(rgba.tobytes())
    for i in range(0, len(data), 4):
        r, g, b, a = data[i], data[i + 1], data[i + 2], data[i + 3]
        if not a:
            continue
        is_magenta = r >= 185 and g <= 90 and b >= 145 and (r - g) >= 100 and (b - g) >= 80
        greyish = max(r, g, b) - min(r, g, b) <= 18
        bright_checker = greyish and r >= 218 and g >= 218 and b >= 218
        remove = is_magenta or (mode in {"decal", "effect", "ui"} and bright_checker)
        if remove:
            data[i + 3] = 0
    return Image.frombytes("RGBA", rgba.size, bytes(data)).resize((size, size), Image.Resampling.LANCZOS)


def load_slots(slot_map: Path) -> list[dict[str, Any]]:
    data = json.loads(slot_map.read_text(encoding="utf-8"))
    slots = data.get("slots", data if isinstance(data, list) else [])
    if not isinstance(slots, list):
        raise SystemExit("slot map must be a list or contain a slots list")
    return [s for s in slots if isinstance(s, dict) and s.get("runtime")]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sheet", required=True)
    parser.add_argument("--grid-manifest", required=True)
    parser.add_argument("--slot-map", required=True)
    parser.add_argument("--candidate-dir", required=True)
    parser.add_argument("--batch-id", required=True)
    parser.add_argument("--clean-mode", choices=["actor", "decal", "effect", "ui", "none"], default="actor")
    parser.add_argument("--review-note", required=True)
    parser.add_argument("--review-artifact")
    parser.add_argument("--reviewer", default="codex_visual_review")
    parser.add_argument("--accept-coverage", action="store_true")
    args = parser.parse_args()

    sheet_path = Path(args.sheet)
    grid = json.loads(Path(args.grid_manifest).read_text(encoding="utf-8"))
    grid_slots = grid.get("slots", [])
    slot_map = load_slots(Path(args.slot_map))
    candidate_dir = Path(args.candidate_dir)
    candidate_dir.mkdir(parents=True, exist_ok=True)
    review_path = Path("art/tiles/reviews/production-review.json")
    review = json.loads(review_path.read_text(encoding="utf-8")) if review_path.exists() else {"assets": {}, "patterns": []}
    review.setdefault("assets", {})
    ledger = Path("art/tiles/generation-ledger.jsonl")
    ledger.parent.mkdir(parents=True, exist_ok=True)

    promoted: list[dict[str, Any]] = []
    with Image.open(sheet_path) as sheet_img:
        sheet = sheet_img.convert("RGBA")
        sx = sheet.width / float(grid.get("size", sheet.width))
        sy = sheet.height / float(grid.get("size", sheet.height))
        for entry in slot_map:
            slot_no = int(entry.get("slot", len(promoted) + 1))
            grid_entry = grid_slots[slot_no - 1]
            box = grid_entry["box"]
            crop_box = [round(box[0] * sx), round(box[1] * sy), round(box[2] * sx), round(box[3] * sy)]
            runtime = Path(entry["runtime"])
            runtime.parent.mkdir(parents=True, exist_ok=True)
            size = int(entry.get("size", 48))
            cleaned = clean_image(sheet.crop(tuple(crop_box)), args.clean_mode, size)
            candidate = Path(entry.get("candidate") or (candidate_dir / (runtime.stem + ".png")))
            candidate.parent.mkdir(parents=True, exist_ok=True)
            cleaned.save(candidate)
            cleaned.save(runtime)
            style = entry.get("style_contract") or default_style(runtime.as_posix())
            prompt = entry.get("canonical_prompt_export")
            spec = entry.get("canonical_json_spec")
            event_id = f"{args.batch_id}-{slot_no:02d}"
            review["assets"][runtime.as_posix()] = {
                "status": "accepted_runtime_art",
                "style_contract": style,
                "canonical_prompt_export": prompt,
                "canonical_json_spec": spec,
                "reference_images": entry.get("reference_images", []),
                "generation_ledger_id": event_id,
                "reviewed_at": datetime.now(timezone.utc).date().isoformat(),
                "reviewer": args.reviewer,
                "review_artifact": args.review_artifact,
                "notes": args.review_note,
            }
            event = {
                "id": event_id,
                "created_at": datetime.now(timezone.utc).isoformat(),
                "asset_ids": [entry.get("work_id") or runtime.as_posix()],
                "canonical_prompt_export": prompt,
                "prompt_sha256": sha256_or_none(prompt),
                "canonical_json_spec": spec,
                "json_spec_sha256": sha256_or_none(spec),
                "reference_images": entry.get("reference_images", []),
                "reference_image_sha256": {p: sha256_or_none(p) for p in entry.get("reference_images", [])},
                "premade_grid_path": grid.get("source_image") or args.grid_manifest,
                "premade_grid_sha256": sha256_or_none(grid.get("source_image") or args.grid_manifest),
                "seed_asset_path": entry.get("seed_asset_path"),
                "seed_asset_sha256": sha256_or_none(entry.get("seed_asset_path")),
                "imagegen_operation": "promote_grid_batch",
                "candidate_output_paths": [sheet_path.as_posix(), candidate.as_posix()],
                "candidate_output_sha256": {sheet_path.as_posix(): sha256_or_none(sheet_path), candidate.as_posix(): sha256_or_none(candidate)},
                "split_output_paths": [candidate.as_posix()],
                "split_output_sha256": {candidate.as_posix(): sha256_or_none(candidate)},
                "accepted_runtime_paths": [runtime.as_posix()],
                "accepted_runtime_sha256": {runtime.as_posix(): sha256_or_none(runtime)},
                "review_status": "accepted_runtime_art",
                "notes": args.review_note,
            }
            with ledger.open("a", encoding="utf-8") as f:
                f.write(json.dumps(event, sort_keys=True) + "\n")
            if args.accept_coverage and entry.get("work_id"):
                subprocess.run(
                    ["python", "scripts/tileset_coverage.py", "accept", "--work-id", entry["work_id"], "--review", args.review_note],
                    check=True,
                )
            promoted.append({"slot": slot_no, "runtime": runtime.as_posix(), "candidate": candidate.as_posix(), "crop_box": crop_box})

    review_path.parent.mkdir(parents=True, exist_ok=True)
    review_path.write_text(json.dumps(review, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    manifest = {
        "schema": "realm.grid_batch_promotion.v1",
        "batch_id": args.batch_id,
        "sheet": sheet_path.as_posix(),
        "grid_manifest": args.grid_manifest,
        "slot_map": args.slot_map,
        "promoted": promoted,
    }
    (candidate_dir / "promotion_manifest.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"promoted": len(promoted), "manifest": (candidate_dir / "promotion_manifest.json").as_posix()}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
