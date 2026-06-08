#!/usr/bin/env python3
"""Build a slug-level tileset review packet for entity runtime art."""

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
DEFAULT_STATUSES = ("accepted", "stale", "needs_review")
ENTITY_GROUPS = {"animals", "buildings", "units"}


def repo_path(path: str | Path) -> Path:
    raw = Path(path)
    return raw if raw.is_absolute() else ROOT / raw


def default_label_strip(slug: str) -> str:
    return f"assets/tiles/entities/{slug}/"


def load_items(args: argparse.Namespace) -> list[coverage.WorkItem]:
    ns = argparse.Namespace(root=args.root, ledger=args.ledger, refresh_specs=args.refresh_specs)
    return coverage.load_and_classify(ns)


def item_matches(item: coverage.WorkItem, args: argparse.Namespace) -> bool:
    groups = set(args.group or ENTITY_GROUPS)
    statuses = set(args.status or DEFAULT_STATUSES)
    return item.slug == args.slug and item.group in groups and item.status in statuses


def build_batch(items: list[coverage.WorkItem], args: argparse.Namespace) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    batch = []
    skipped = []
    for item in coverage.sorted_items(items):
        if not item_matches(item, args):
            continue
        primary = item.required_paths[0] if item.required_paths else ""
        if primary and repo_path(primary).exists():
            batch.append(item.to_dict())
        else:
            skipped.append(
                {
                    "work_id": item.work_id,
                    "status": item.status,
                    "reason": f"primary runtime path is not reviewable: {primary}",
                }
            )
    payload = {
        "schema": "realm.tileset_generation_batch.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "slug": args.slug,
        "groups": args.group or sorted(ENTITY_GROUPS),
        "status_filter": args.status or list(DEFAULT_STATUSES),
        "batch": batch,
    }
    return payload, skipped


def run_json(cmd: list[str]) -> dict[str, Any]:
    proc = subprocess.run(cmd, cwd=ROOT, check=True, text=True, capture_output=True)
    text = proc.stdout.strip()
    return json.loads(text) if text else {}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--slug", required=True)
    parser.add_argument("--group", action="append", choices=sorted(ENTITY_GROUPS), help="Entity group to include; repeatable.")
    parser.add_argument("--status", action="append", choices=coverage.STATUS_ORDER, help="Status to include; repeatable.")
    parser.add_argument("--root", default=coverage.rel(coverage.IMAGE_JSON_ROOT), help="generated image-json root")
    parser.add_argument("--ledger", default=coverage.rel(coverage.LEDGER_DEFAULT), help="production ledger JSONL path")
    parser.add_argument("--refresh-specs", action="store_true", help="regenerate image-json and image-spec first")
    parser.add_argument("--batch-out", help="Batch JSON path.")
    parser.add_argument("--out", help="Contact sheet path.")
    parser.add_argument("--cleanup-json", help="Cleanup dry-run report path.")
    parser.add_argument("--label-strip", help="Label prefix to strip from contact-sheet labels.")
    parser.add_argument("--reference-image", action="append", default=[], help="Optional reference thumbnails to include.")
    parser.add_argument("--gate", action="store_true", help="Also run tileset_visual_review_gate.py on the slug batch.")
    parser.add_argument(
        "--audit-mode",
        choices=("full", "cache", "batch", "none"),
        default="batch",
        help="Audit mode passed to tileset_visual_review_gate.py when --gate is used.",
    )
    parser.add_argument("--gate-report", help="Gate report path when --gate is used.")
    args = parser.parse_args()

    out = repo_path(args.out or f"build/tileset-review/{args.slug}-slug-review.png")
    batch_out = repo_path(args.batch_out or f"build/tileset-review/{args.slug}-slug-review-batch.json")
    cleanup_json = repo_path(args.cleanup_json or f"build/tileset-review/{args.slug}-slug-cleanup-dry-run.json")
    label_strip = args.label_strip if args.label_strip is not None else default_label_strip(args.slug)

    items = load_items(args)
    payload, skipped = build_batch(items, args)
    if not payload["batch"]:
        raise SystemExit(f"no reviewable runtime items matched slug={args.slug!r}")

    batch_out.parent.mkdir(parents=True, exist_ok=True)
    batch_out.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")

    review_cmd = [
        sys.executable,
        "scripts/tileset_review_existing_batch.py",
        "--batch",
        batch_out.as_posix(),
        "--out",
        out.as_posix(),
        "--label-strip",
        label_strip,
        "--review-note",
        "inspection only: slug-level runtime art review packet",
    ]
    for ref in args.reference_image:
        review_cmd.extend(["--reference-image", ref])
    subprocess.run(review_cmd, cwd=ROOT, check=True)

    cleanup = run_json(
        [
            sys.executable,
            "scripts/tileset_cleanup_artifacts.py",
            "--batch",
            batch_out.as_posix(),
            "--dry-run",
            "--json-out",
            cleanup_json.as_posix(),
        ]
    )

    gate_report = None
    if args.gate:
        gate_report_path = repo_path(args.gate_report or f"build/tileset-review/{args.slug}-slug-review-gate.json")
        subprocess.run(
            [
                sys.executable,
                "scripts/tileset_visual_review_gate.py",
                "--batch",
                batch_out.as_posix(),
                "--out",
                out.as_posix(),
                "--label-strip",
                label_strip,
                "--cleanup-json",
                cleanup_json.as_posix(),
                "--audit-mode",
                args.audit_mode,
                "--report-out",
                gate_report_path.as_posix(),
            ],
            cwd=ROOT,
            check=False,
        )
        gate_report = gate_report_path.as_posix()

    summary = {
        "schema": "realm.tileset_entity_slug_review.v1",
        "slug": args.slug,
        "items": len(payload["batch"]),
        "batch": batch_out.as_posix(),
        "contact_sheet": out.as_posix(),
        "cleanup_json": cleanup_json.as_posix(),
        "cleanup_findings": cleanup.get("findings", 0),
        "gate_report": gate_report,
        "skipped": skipped,
    }
    print(json.dumps(summary, indent=2, ensure_ascii=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
