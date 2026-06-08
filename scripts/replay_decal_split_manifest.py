#!/usr/bin/env python3
"""Replay accepted decal split manifests into runtime PNGs at current policy size."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
from typing import Any

from PIL import Image

from tileset_resolution_policy import source_resolution_policy


ROOT = Path(__file__).resolve().parents[1]


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT.resolve()).as_posix()


def clean_flat_decal(img: Image.Image, size: tuple[int, int]) -> Image.Image:
    rgba = img.convert("RGBA")
    pixels = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pixels[x, y]
            if a == 0:
                continue
            is_magenta = r >= 185 and g <= 90 and b >= 145 and (r - g) >= 100 and (b - g) >= 80
            greyish = max(r, g, b) - min(r, g, b) <= 18
            bright_checker = greyish and r >= 218 and g >= 218 and b >= 218
            if is_magenta or bright_checker:
                pixels[x, y] = (r, g, b, 0)
    return rgba.resize(size, Image.Resampling.LANCZOS)


def replay_manifest(manifest_path: Path, *, apply: bool) -> dict[str, Any]:
    manifest = read_json(manifest_path)
    source_path = ROOT / manifest["batch_source"]
    policy = source_resolution_policy("decals")
    size = (int(policy["width_px"]), int(policy["height_px"]))
    results: list[dict[str, Any]] = []
    with Image.open(source_path) as opened:
        source = opened.convert("RGBA")
        for slot in manifest.get("slots", []):
            if not isinstance(slot, dict):
                continue
            crop_box = tuple(int(v) for v in slot["crop_box"])
            runtime = ROOT / slot["runtime"]
            candidate = ROOT / slot["candidate"]
            result = {
                "slot": int(slot.get("slot", len(results) + 1)),
                "slug": slot.get("slug"),
                "runtime": rel(runtime),
                "candidate": rel(candidate),
                "crop_box": list(crop_box),
                "to_dimensions": list(size),
            }
            if apply:
                cleaned = clean_flat_decal(source.crop(crop_box), size)
                runtime.parent.mkdir(parents=True, exist_ok=True)
                candidate.parent.mkdir(parents=True, exist_ok=True)
                cleaned.save(runtime)
                cleaned.save(candidate)
                result["runtime_sha256"] = sha256(runtime)
                result["candidate_sha256"] = sha256(candidate)
            results.append(result)
    return {
        "schema": "realm.decal_split_manifest_replay.v1",
        "dry_run": not apply,
        "manifest": rel(manifest_path),
        "batch_source": rel(source_path),
        "target_dimensions": list(size),
        "replayed": len(results),
        "results": results,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--apply", action="store_true")
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()

    result = replay_manifest(ROOT / args.manifest, apply=args.apply)
    text = json.dumps(result, indent=2)
    print(text)
    if args.json_out:
        out = ROOT / args.json_out
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(text + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
