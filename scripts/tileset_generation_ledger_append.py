#!/usr/bin/env python3
"""Append a machine-readable tileset generation event."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path
from uuid import uuid4


def sha256_or_none(path_text: str | None) -> str | None:
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


def hashes(paths: list[str]) -> dict[str, str | None]:
    return {path: sha256_or_none(path) for path in paths}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", default="art/tiles/generation-ledger.jsonl")
    parser.add_argument("--id", default=None)
    parser.add_argument("--asset-id", action="append", default=[])
    parser.add_argument("--canonical-prompt-export", required=True)
    parser.add_argument("--reference-image", action="append", default=[])
    parser.add_argument("--premade-grid-path")
    parser.add_argument("--seed-asset-path")
    parser.add_argument("--imagegen-operation", default="generate")
    parser.add_argument("--candidate-output-path", action="append", default=[])
    parser.add_argument("--split-output-path", action="append", default=[])
    parser.add_argument("--accepted-runtime-path", action="append", default=[])
    parser.add_argument("--review-status", default="unreviewed")
    parser.add_argument("--notes", default="")
    args = parser.parse_args()

    event = {
        "id": args.id or f"tileset-{datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ')}-{uuid4().hex[:8]}",
        "created_at": datetime.now(timezone.utc).isoformat(),
        "asset_ids": args.asset_id,
        "canonical_prompt_export": args.canonical_prompt_export,
        "prompt_sha256": sha256_or_none(args.canonical_prompt_export),
        "reference_images": args.reference_image,
        "reference_image_sha256": hashes(args.reference_image),
        "premade_grid_path": args.premade_grid_path,
        "premade_grid_sha256": sha256_or_none(args.premade_grid_path),
        "seed_asset_path": args.seed_asset_path,
        "seed_asset_sha256": sha256_or_none(args.seed_asset_path),
        "imagegen_operation": args.imagegen_operation,
        "candidate_output_paths": args.candidate_output_path,
        "candidate_output_sha256": hashes(args.candidate_output_path),
        "split_output_paths": args.split_output_path,
        "split_output_sha256": hashes(args.split_output_path),
        "accepted_runtime_paths": args.accepted_runtime_path,
        "accepted_runtime_sha256": hashes(args.accepted_runtime_path),
        "review_status": args.review_status,
        "notes": args.notes,
    }

    ledger = Path(args.ledger)
    ledger.parent.mkdir(parents=True, exist_ok=True)
    with ledger.open("a", encoding="utf-8") as f:
        f.write(json.dumps(event, sort_keys=True) + "\n")
    print(event["id"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
