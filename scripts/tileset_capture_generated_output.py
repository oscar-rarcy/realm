#!/usr/bin/env python3
"""Copy a generated image into the tileset evidence workspace."""

from __future__ import annotations

import argparse
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def latest_png(root: Path) -> Path:
    files = [p for p in root.rglob("*.png") if p.is_file()]
    if not files:
        raise SystemExit(f"no png files found under {root}")
    return max(files, key=lambda p: p.stat().st_mtime)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", help="Exact generated PNG path. Prefer this over --latest.")
    parser.add_argument("--latest", action="store_true", help="Use latest generated PNG under --generated-root.")
    parser.add_argument("--generated-root", default=str(Path.home() / ".codex" / "generated_images"))
    parser.add_argument("--out", required=True, help="Destination PNG under art/tiles/candidates or build evidence.")
    parser.add_argument("--prompt-file")
    parser.add_argument("--grid-template")
    parser.add_argument("--role", default="candidate")
    parser.add_argument("--notes", default="")
    args = parser.parse_args()

    source = Path(args.input) if args.input else latest_png(Path(args.generated_root))
    if not source.exists():
        raise SystemExit(f"generated image not found: {source}")
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(source.read_bytes())
    manifest = {
        "schema": "realm.generated_output_capture.v1",
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "source": str(source),
        "source_sha256": sha256(source),
        "out": out.as_posix(),
        "out_sha256": sha256(out),
        "prompt_file": args.prompt_file,
        "grid_template": args.grid_template,
        "role": args.role,
        "notes": args.notes,
    }
    out.with_suffix(".capture.json").write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(out.as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
