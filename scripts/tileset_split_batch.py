#!/usr/bin/env python3
"""Split a tileset coverage batch into homogeneous review batches."""

from __future__ import annotations

import argparse
import json
import re
from collections import defaultdict
from pathlib import Path
from typing import Any


BUILDING_ENTITY_SLUGS = {
    "barracks",
    "blacksmith",
    "castle",
    "church",
    "dock",
    "farm",
    "gate",
    "house",
    "lumber_camp",
    "market",
    "mill",
    "mining_camp",
    "stable",
    "stone_bridge",
    "tower",
    "town_hall",
    "wall",
    "wooden_bridge",
}


def safe_name(value: str) -> str:
    value = value.replace("/", "-").replace("\\", "-")
    return re.sub(r"[^A-Za-z0-9_.-]+", "-", value).strip("-") or "batch"


def style_contract(item: dict[str, Any]) -> str:
    paths = item.get("required_paths") or []
    rel = str(paths[0]) if paths else ""
    slug = str(item.get("slug") or "")
    if rel.startswith("assets/tiles/decals/"):
        return "realm_simplified_hand_painted_ground_decal"
    if rel.startswith("assets/tiles/grounds/"):
        return "realm_ground_slab_small_tile"
    if rel.startswith("assets/tiles/projectiles/"):
        return "realm_projectile_cutout"
    if rel.startswith("assets/tiles/effects-ui/"):
        return "realm_effect_overlay"
    if rel.startswith("assets/tiles/entities/") and slug in BUILDING_ENTITY_SLUGS:
        return "realm_map_integrated_painted_feature"
    return "realm_paper_cutout_small_tile"


def key_for(item: dict[str, Any], parts: list[str]) -> str:
    values: list[str] = []
    for part in parts:
        if part == "style_contract":
            values.append(style_contract(item))
        elif part == "status":
            values.append(str(item.get("status") or "unknown"))
        else:
            values.append(str(item.get(part) or "unknown"))
    return "__".join(values)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch", default="build/tileset-next-batch.json")
    parser.add_argument("--out-dir", default="build")
    parser.add_argument("--prefix", default="tileset-next-batch")
    parser.add_argument(
        "--by",
        default="style_contract,group,slug",
        help="Comma-separated item keys. Special key: style_contract.",
    )
    parser.add_argument("--summary-out")
    args = parser.parse_args()

    src_path = Path(args.batch)
    src = json.loads(src_path.read_text(encoding="utf-8"))
    items = src.get("batch", src if isinstance(src, list) else [])
    if not isinstance(items, list):
        raise SystemExit("batch must be a list or contain a batch list")
    parts = [part.strip() for part in args.by.split(",") if part.strip()]
    grouped: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for item in items:
        if not isinstance(item, dict):
            continue
        grouped[key_for(item, parts)].append(item)

    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    outputs = []
    for key, group_items in sorted(grouped.items()):
        path = out_dir / f"{args.prefix}-{safe_name(key)}.json"
        payload = {"schema": src.get("schema", "realm.tileset_next_batch.v1"), "batch": group_items}
        path.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        outputs.append({"key": key, "path": path.as_posix(), "items": len(group_items)})

    summary = {"source": src_path.as_posix(), "split_by": parts, "groups": outputs}
    text = json.dumps(summary, indent=2, ensure_ascii=True) + "\n"
    if args.summary_out:
        Path(args.summary_out).write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
