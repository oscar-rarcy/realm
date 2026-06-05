#!/usr/bin/env python3
"""Record a rejected generated image so failed styles are not reused."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


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


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ledger", default="art/tiles/generation-ledger.jsonl")
    parser.add_argument("--id", required=True)
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--canonical-prompt-export", required=True)
    parser.add_argument("--canonical-json-spec")
    parser.add_argument("--grid-template")
    parser.add_argument("--reason", required=True)
    parser.add_argument("--review-artifact")
    parser.add_argument("--notes", default="")
    args = parser.parse_args()

    event = {
        "id": args.id,
        "created_at": datetime.now(timezone.utc).isoformat(),
        "asset_ids": [],
        "canonical_prompt_export": args.canonical_prompt_export,
        "prompt_sha256": sha256_or_none(args.canonical_prompt_export),
        "canonical_json_spec": args.canonical_json_spec,
        "json_spec_sha256": sha256_or_none(args.canonical_json_spec),
        "reference_images": [],
        "reference_image_sha256": {},
        "premade_grid_path": args.grid_template,
        "premade_grid_sha256": sha256_or_none(args.grid_template),
        "seed_asset_path": None,
        "seed_asset_sha256": None,
        "imagegen_operation": "rejected_generation",
        "candidate_output_paths": [args.candidate],
        "candidate_output_sha256": {args.candidate: sha256_or_none(args.candidate)},
        "split_output_paths": [],
        "split_output_sha256": {},
        "accepted_runtime_paths": [],
        "accepted_runtime_sha256": {},
        "review_status": "rejected",
        "rejection_reason": args.reason,
        "review_artifact": args.review_artifact,
        "notes": args.notes,
    }
    ledger = Path(args.ledger)
    ledger.parent.mkdir(parents=True, exist_ok=True)
    with ledger.open("a", encoding="utf-8") as f:
        f.write(json.dumps(event, sort_keys=True) + "\n")
    print(args.id)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
