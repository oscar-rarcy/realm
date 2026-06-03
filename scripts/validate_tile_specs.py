#!/usr/bin/env python3
"""Validate generated Realm tile/image JSON against the current code-backed v2 inventory."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from export_tile_specs import GAME_TYPES_HEADER, ROOT, enum_values, read_text


EXPECTED_GROUPS = [
    "grounds",
    "features",
    "decals",
    "units",
    "animals",
    "buildings",
    "projectiles",
    "effects",
    "user_interface",
]
SCHEMA_BY_GROUP = {
    "grounds": "realm.ground_spec.v2",
    "features": "realm.feature_spec.v2",
    "decals": "realm.decal_spec.v2",
    "units": "realm.actor_sprite_spec.v2",
    "animals": "realm.actor_sprite_spec.v2",
    "buildings": "realm.building_spec.v2",
    "projectiles": "realm.projectile_sprite_spec.v2",
    "effects": "realm.effect_spec.v1",
    "user_interface": "realm.ui_asset_spec.v1",
}
ASSET_TYPE_BY_GROUP = {
    "grounds": "ground",
    "features": "feature",
    "decals": "decal",
    "units": "unit",
    "animals": "animal",
    "buildings": "building",
    "projectiles": "projectile",
    "effects": "effect",
    "user_interface": "user_interface",
}


class Reporter:
    def __init__(self, mode: str) -> None:
        self.mode = mode
        self.errors: list[str] = []
        self.warnings: list[str] = []

    def error(self, message: str) -> None:
        self.errors.append(message)

    def warn(self, message: str) -> None:
        self.warnings.append(message)


def load_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_index(index: dict, reporter: Reporter) -> None:
    if index.get("schema") != "realm.tile_specs_index.v2":
        reporter.error("index.json must use schema realm.tile_specs_index.v2")
    groups = index.get("groups")
    if not isinstance(groups, dict):
        reporter.error("index.json groups must be an object")
        return
    for group in EXPECTED_GROUPS:
        if group not in groups:
            reporter.error(f"index.json missing group {group}")


def validate_asset(group: str, entry: dict, asset_path: Path, asset: dict, reporter: Reporter) -> None:
    if asset.get("slug") != asset_path.stem:
        reporter.error(f"{asset_path}: slug must match filename")
    if asset.get("slug") != entry.get("slug"):
        reporter.error(f"{asset_path}: index slug does not match asset slug")
    if asset.get("asset_type") != ASSET_TYPE_BY_GROUP[group]:
        reporter.error(f"{asset_path}: asset_type must be {ASSET_TYPE_BY_GROUP[group]}")
    if asset.get("schema") != SCHEMA_BY_GROUP[group]:
        reporter.error(f"{asset_path}: schema must be {SCHEMA_BY_GROUP[group]}")
    if "render" not in asset:
        reporter.error(f"{asset_path}: missing render")
    if "paths" not in asset:
        reporter.error(f"{asset_path}: missing paths")
    if not asset.get("sources"):
        reporter.error(f"{asset_path}: sources must be non-empty")
    if reporter.mode == "strict-v2":
        if asset.get("asset_type") == "ammunition":
            reporter.error(f"{asset_path}: ammunition is not allowed in strict-v2")
        if asset.get("schema") == "realm.ammunition_sprite_spec.v1":
            reporter.error(f"{asset_path}: v1 ammunition schema is not allowed in strict-v2")
        if "art" in asset and "projection" in asset["art"]:
            reporter.error(f"{asset_path}: art.projection is not allowed in strict-v2")
    else:
        if asset.get("asset_type") == "ammunition":
            reporter.warn(f"{asset_path}: legacy ammunition asset_type should migrate to projectile")
        if "art" in asset and "projection" in asset["art"]:
            reporter.warn(f"{asset_path}: legacy art.projection should migrate to render.projection_mode")


def validate_inventory(index: dict, reporter: Reporter) -> None:
    game_types = read_text(GAME_TYPES_HEADER)
    expected_grounds = {enum for enum in enum_values(game_types, "GroundType") if enum != "G_ROAD"}
    expected_features = {enum for enum in enum_values(game_types, "FeatureType") if enum != "F_NONE"}
    expected_decals = set(enum_values(game_types, "VisualDecalType"))

    groups = index["groups"]
    seen_grounds = {item.get("enum") for item in groups.get("grounds", [])}
    seen_features = {item.get("enum") for item in groups.get("features", [])}
    seen_decals = {item.get("enum") for item in groups.get("decals", [])}

    missing_grounds = sorted(expected_grounds - seen_grounds)
    missing_features = sorted(expected_features - seen_features)
    missing_decals = sorted(expected_decals - seen_decals)
    if missing_grounds:
        reporter.error(f"missing grounds: {', '.join(missing_grounds)}")
    if missing_features:
        reporter.error(f"missing features: {', '.join(missing_features)}")
    if missing_decals:
        reporter.error(f"missing decals: {', '.join(missing_decals)}")
    if any(item.get("slug") == "road" for item in groups.get("grounds", [])):
        reporter.error("road must not appear in grounds")
    if not any(item.get("enum") == "VD_ROAD" for item in groups.get("decals", [])):
        reporter.error("VD_ROAD must appear in decals")


def validate_cross_refs(asset_root: Path, reporter: Reporter) -> None:
    projectile_dir = asset_root / "projectiles"
    known_projectiles = {path.stem for path in projectile_dir.glob("*.json")}
    for group in ("units", "animals", "buildings"):
        for path in (asset_root / group).glob("*.json"):
            asset = load_json(path)
            refs = asset.get("combat", {}).get("projectiles", [])
            for ref in refs:
                if ref not in known_projectiles:
                    reporter.error(f"{path}: unknown combat.projectiles ref {ref}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=["compatibility", "strict-v2"], default="compatibility")
    parser.add_argument("--root", default="art/tiles/image-json", help="directory to validate")
    args = parser.parse_args()

    reporter = Reporter(args.mode)
    asset_root = (ROOT / args.root).resolve()
    index_path = asset_root / "index.json"
    if not index_path.exists():
        print(f"missing index: {index_path}", file=sys.stderr)
        return 1

    index = load_json(index_path)
    validate_index(index, reporter)
    groups = index.get("groups", {})
    for group in EXPECTED_GROUPS:
        for entry in groups.get(group, []):
            rel = entry.get("path")
            if not rel:
                reporter.error(f"index group {group} contains entry without path")
                continue
            asset_path = asset_root / rel
            if not asset_path.exists():
                reporter.error(f"missing asset file: {asset_path}")
                continue
            validate_asset(group, entry, asset_path, load_json(asset_path), reporter)

    validate_inventory(index, reporter)
    validate_cross_refs(asset_root, reporter)

    for warning in reporter.warnings:
        print(f"warning: {warning}")
    for error in reporter.errors:
        print(f"error: {error}", file=sys.stderr)
    return 1 if reporter.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
