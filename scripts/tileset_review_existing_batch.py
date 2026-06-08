#!/usr/bin/env python3
"""Create a contact sheet for a coverage batch and optionally accept it."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


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


def make_contact(items: list[dict], out: Path, label_strip: str = "", reference_images: list[str] | None = None) -> None:
    refs = [(path, f"ref:{Path(path).stem}") for path in (reference_images or [])]
    runtime_paths = [(item["required_paths"][0], item["required_paths"][0].replace(label_strip, "")) for item in items]
    paths = refs + runtime_paths
    thumb, pad, label_h, cols = 96, 10, 34, 4
    rows = (len(paths) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (thumb + pad) + pad, rows * (thumb + label_h + pad) + pad), (248, 246, 238))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 10)
    except OSError:
        font = ImageFont.load_default()
    for idx, (rel, label) in enumerate(paths):
        col, row = idx % cols, idx // cols
        x, y = pad + col * (thumb + pad), pad + row * (thumb + label_h + pad)
        sheet.paste(Image.new("RGB", (thumb, thumb), (255, 0, 255)), (x, y))
        with Image.open(rel) as img:
            rgba = img.convert("RGBA")
            rgba.thumbnail((thumb, thumb), Image.Resampling.NEAREST)
            sheet.paste(rgba, (x + (thumb - rgba.width) // 2, y + (thumb - rgba.height) // 2), rgba)
        draw.text((x, y + thumb + 3), label[:22], fill=(30, 30, 30), font=font)
        draw.text((x, y + thumb + 16), label[22:44], fill=(30, 30, 30), font=font)
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch", default="build/tileset-next-batch.json")
    parser.add_argument("--out", required=True)
    parser.add_argument("--label-strip", default="")
    parser.add_argument("--style-contract", default="realm_paper_cutout_small_tile")
    parser.add_argument("--reference-image", action="append", default=[], help="Optional reference thumbnails to place before batch items.")
    parser.add_argument("--review-note", required=True)
    parser.add_argument("--reviewer", default="codex_visual_review")
    parser.add_argument("--accept", action="store_true")
    parser.add_argument("--write-review", action="store_true")
    parser.add_argument("--append-ledger", action="store_true")
    args = parser.parse_args()

    items = json.loads(Path(args.batch).read_text(encoding="utf-8"))["batch"]
    out = Path(args.out)
    make_contact(items, out, args.label_strip, args.reference_image)

    review_path = Path("art/tiles/reviews/production-review.json")
    review = json.loads(review_path.read_text(encoding="utf-8")) if review_path.exists() else {"assets": {}, "patterns": []}
    review.setdefault("assets", {})
    ledger = Path("art/tiles/generation-ledger.jsonl")
    ledger.parent.mkdir(parents=True, exist_ok=True)

    for item in items:
        runtime = item["required_paths"][0]
        event_id = f"tileset-review-existing-{item['work_id'].replace('/', '-')}"
        if args.write_review:
            review["assets"][runtime] = {
                "status": "accepted_runtime_art",
                "style_contract": args.style_contract,
                "canonical_prompt_export": item.get("prompt"),
                "canonical_json_spec": item.get("spec"),
                "reference_images": args.reference_image,
                "generation_ledger_id": event_id,
                "reviewed_at": datetime.now(timezone.utc).date().isoformat(),
                "reviewer": args.reviewer,
                "review_artifact": out.as_posix(),
                "notes": args.review_note,
            }
        if args.append_ledger:
            event = {
                "id": event_id,
                "created_at": datetime.now(timezone.utc).isoformat(),
                "asset_ids": [item["work_id"]],
                "canonical_prompt_export": item.get("prompt"),
                "prompt_sha256": sha256_or_none(item.get("prompt")),
                "canonical_json_spec": item.get("spec"),
                "json_spec_sha256": sha256_or_none(item.get("spec")),
                "reference_images": args.reference_image,
                "reference_image_sha256": {path: sha256_or_none(path) for path in args.reference_image},
                "premade_grid_path": None,
                "premade_grid_sha256": None,
                "seed_asset_path": None,
                "seed_asset_sha256": None,
                "imagegen_operation": "review_existing_runtime_asset",
                "candidate_output_paths": [],
                "candidate_output_sha256": {},
                "split_output_paths": [],
                "split_output_sha256": {},
                "accepted_runtime_paths": [runtime],
                "accepted_runtime_sha256": {runtime: sha256_or_none(runtime)},
                "review_status": "accepted_runtime_art",
                "notes": args.review_note,
            }
            with ledger.open("a", encoding="utf-8") as f:
                f.write(json.dumps(event, sort_keys=True) + "\n")
        if args.accept:
            subprocess.run(["python", "scripts/tileset_coverage.py", "accept", "--work-id", item["work_id"], "--review", args.review_note], check=True)

    if args.write_review:
        review_path.parent.mkdir(parents=True, exist_ok=True)
        review_path.write_text(json.dumps(review, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"items": len(items), "contact_sheet": out.as_posix()}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
