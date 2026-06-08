#!/usr/bin/env python3
"""Accept current reviewed runtime art for every matching work item in a slug."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

import tileset_coverage as coverage


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_STATUSES = ("stale",)
ENTITY_GROUPS = {"animals", "buildings", "units"}


def repo_path(path: str | Path) -> Path:
    raw = Path(path)
    return raw if raw.is_absolute() else ROOT / raw


def default_label_strip(slug: str) -> str:
    return f"assets/tiles/entities/{slug}/"


def style_contract_for(item: coverage.WorkItem) -> str:
    if item.group == "decals":
        return "realm_simplified_hand_painted_ground_decal"
    if item.group == "grounds":
        return "realm_ground_slab_small_tile"
    if item.group == "projectiles":
        return "realm_projectile_cutout"
    if item.group == "buildings":
        return "realm_map_integrated_painted_feature"
    if item.group in {"animals", "features", "units"}:
        return "realm_paper_cutout_small_tile"
    if item.group in {"effects", "user_interface"}:
        return "realm_effect_overlay"
    return "realm_paper_cutout_small_tile"


def load_items(args: argparse.Namespace) -> list[coverage.WorkItem]:
    ns = argparse.Namespace(root=args.root, ledger=args.ledger, refresh_specs=args.refresh_specs)
    return coverage.load_and_classify(ns)


def item_matches(item: coverage.WorkItem, args: argparse.Namespace) -> bool:
    groups = set(args.group or ENTITY_GROUPS)
    statuses = set(args.status or DEFAULT_STATUSES)
    return item.slug == args.slug and item.group in groups and item.status in statuses


def build_batch(items: list[coverage.WorkItem], args: argparse.Namespace) -> dict[str, Any]:
    batch = []
    for item in coverage.sorted_items(items):
        if not item_matches(item, args):
            continue
        primary = item.required_paths[0] if item.required_paths else ""
        if not primary or not repo_path(primary).exists():
            raise SystemExit(f"{item.work_id} cannot be accepted: primary runtime path is missing: {primary}")
        batch.append(item.to_dict())
    return {
        "schema": "realm.tileset_generation_batch.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "slug": args.slug,
        "groups": args.group or sorted(ENTITY_GROUPS),
        "status_filter": args.status or list(DEFAULT_STATUSES),
        "batch": batch,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", required=True)
    parser.add_argument("--group", action="append", choices=sorted(ENTITY_GROUPS), help="Entity group to include; repeatable.")
    parser.add_argument("--status", action="append", choices=coverage.STATUS_ORDER, help="Status to accept; repeatable.")
    parser.add_argument("--root", default=coverage.rel(coverage.IMAGE_JSON_ROOT), help="generated image-json root")
    parser.add_argument("--ledger", default=coverage.rel(coverage.LEDGER_DEFAULT), help="production ledger JSONL path")
    parser.add_argument("--refresh-specs", action="store_true", help="regenerate image-json and image-spec first")
    parser.add_argument("--batch-out", help="Batch JSON path.")
    parser.add_argument("--out", help="Contact sheet path to write as the review artifact.")
    parser.add_argument("--label-strip", help="Label prefix to strip from contact-sheet labels.")
    parser.add_argument("--style-contract", help="Override style contract recorded in production-review.json.")
    parser.add_argument("--reference-image", action="append", default=[], help="Optional reference thumbnails to include.")
    parser.add_argument("--review-note", required=True)
    parser.add_argument("--reviewer", default="codex_visual_review")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    out = repo_path(args.out or f"build/tileset-review/{args.slug}-slug-accepted.png")
    batch_out = repo_path(args.batch_out or f"build/tileset-review/{args.slug}-slug-accept-batch.json")
    label_strip = args.label_strip if args.label_strip is not None else default_label_strip(args.slug)

    items = load_items(args)
    payload = build_batch(items, args)
    if not payload["batch"]:
        raise SystemExit(f"no matching work items to accept for slug={args.slug!r}")
    style_contract = args.style_contract or style_contract_for(next(item for item in items if item.work_id == payload["batch"][0]["work_id"]))

    batch_out.parent.mkdir(parents=True, exist_ok=True)
    batch_out.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")

    if args.dry_run:
        print(
            json.dumps(
                {
                    "schema": "realm.tileset_accept_slug.v1",
                    "dry_run": True,
                    "slug": args.slug,
                    "items": len(payload["batch"]),
                    "batch": batch_out.as_posix(),
                    "would_write_contact_sheet": out.as_posix(),
                    "style_contract": style_contract,
                    "work_ids": [item["work_id"] for item in payload["batch"]],
                },
                indent=2,
                ensure_ascii=True,
            )
        )
        return 0

    cmd = [
        sys.executable,
        "scripts/tileset_review_existing_batch.py",
        "--batch",
        batch_out.as_posix(),
        "--out",
        out.as_posix(),
        "--label-strip",
        label_strip,
        "--style-contract",
        style_contract,
        "--review-note",
        args.review_note,
        "--reviewer",
        args.reviewer,
        "--write-review",
        "--append-ledger",
        "--accept",
    ]
    for ref in args.reference_image:
        cmd.extend(["--reference-image", ref])
    subprocess.run(cmd, cwd=ROOT, check=True)
    print(
        json.dumps(
            {
                "schema": "realm.tileset_accept_slug.v1",
                "dry_run": False,
                "slug": args.slug,
                "items": len(payload["batch"]),
                "batch": batch_out.as_posix(),
                "contact_sheet": out.as_posix(),
                "style_contract": style_contract,
            },
            indent=2,
            ensure_ascii=True,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
