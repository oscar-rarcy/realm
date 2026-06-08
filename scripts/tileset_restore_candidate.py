#!/usr/bin/env python3
"""Restore runtime tiles from promotion-manifest candidate outputs."""

from __future__ import annotations

import argparse
import json
import shutil
from pathlib import Path
from typing import Any

from PIL import Image


def batch_runtime_paths(path: Path | None) -> set[str]:
    if not path:
        return set()
    data = json.loads(path.read_text(encoding="utf-8"))
    items = data.get("batch", data if isinstance(data, list) else [])
    paths: set[str] = set()
    for item in items:
        if not isinstance(item, dict):
            continue
        for rel in item.get("required_paths", []):
            if str(rel).endswith("_base.png"):
                paths.add(str(rel).replace("\\", "/"))
    return paths


def manifest_pairs(path: Path) -> list[dict[str, Any]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    promoted = data.get("promoted", data if isinstance(data, list) else [])
    sheet = str(data.get("sheet") or "").replace("\\", "/") if isinstance(data, dict) else ""
    pairs = []
    for item in promoted:
        if isinstance(item, dict) and item.get("runtime") and item.get("candidate"):
            pairs.append(
                {
                    "runtime": str(item["runtime"]).replace("\\", "/"),
                    "candidate": str(item["candidate"]).replace("\\", "/"),
                    "crop_box": item.get("crop_box"),
                    "sheet": sheet,
                    "manifest": path.as_posix(),
                }
            )
    return pairs


def clean_actor(img: Image.Image, size: int = 48) -> Image.Image:
    rgba = img.convert("RGBA")
    data = bytearray(rgba.tobytes())
    for i in range(0, len(data), 4):
        r, g, b, a = data[i], data[i + 1], data[i + 2], data[i + 3]
        if not a:
            continue
        is_magenta = r >= 185 and g <= 90 and b >= 145 and (r - g) >= 100 and (b - g) >= 80
        if is_magenta:
            data[i + 3] = 0
    return Image.frombytes("RGBA", rgba.size, bytes(data)).resize((size, size), Image.Resampling.LANCZOS)


def restore_pair(pair: dict[str, Any], dst: Path) -> str:
    sheet = Path(pair.get("sheet") or "")
    crop_box = pair.get("crop_box")
    if sheet.exists() and isinstance(crop_box, list) and len(crop_box) == 4:
        with Image.open(sheet) as src:
            restored = clean_actor(src.convert("RGBA").crop(tuple(int(v) for v in crop_box)))
        dst.parent.mkdir(parents=True, exist_ok=True)
        restored.save(dst)
        return "restored_from_sheet_crop"
    src = Path(pair["candidate"])
    if not src.exists():
        return "missing_candidate"
    dst.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dst)
    return "restored_from_candidate"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch", help="Limit restore to runtime paths listed in this coverage batch.")
    parser.add_argument("--runtime", action="append", default=[], help="Specific runtime path to restore. Repeatable.")
    parser.add_argument("--manifest", action="append", default=[], help="Promotion manifest path. Repeatable.")
    parser.add_argument("--manifest-root", default="art/tiles/candidates", help="Searched recursively when --manifest is omitted.")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--json-out")
    args = parser.parse_args()

    wanted = {p.replace("\\", "/") for p in args.runtime}
    wanted.update(batch_runtime_paths(Path(args.batch)) if args.batch else set())
    manifests = [Path(p) for p in args.manifest] if args.manifest else sorted(Path(args.manifest_root).rglob("promotion_manifest.json"))
    latest: dict[str, dict[str, Any]] = {}
    for manifest in manifests:
        if not manifest.exists():
            continue
        for pair in manifest_pairs(manifest):
            if not wanted or pair["runtime"] in wanted:
                latest[pair["runtime"]] = pair

    restored = []
    missing = sorted(wanted - set(latest)) if wanted else []
    for runtime, pair in sorted(latest.items()):
        dst = Path(runtime)
        record = {"runtime": runtime, "candidate": pair["candidate"], "manifest": pair["manifest"], "status": "dry_run" if args.dry_run else "restored"}
        if not args.dry_run:
            record["status"] = restore_pair(pair, dst)
        restored.append(record)

    report = {"restored": restored, "missing_runtime_paths": missing}
    text = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
    if args.json_out:
        Path(args.json_out).write_text(text, encoding="utf-8")
    print(text, end="")
    return 1 if missing or any(item["status"] == "missing_candidate" for item in restored) else 0


if __name__ == "__main__":
    raise SystemExit(main())
