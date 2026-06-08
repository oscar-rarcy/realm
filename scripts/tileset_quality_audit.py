#!/usr/bin/env python3
"""Audit Realm tileset assets for production-quality blockers.

This is intentionally stricter than scripts/tileset_coverage.py. Coverage answers
"do expected files exist and match the ledger?" This script answers "are those
files likely usable as production runtime art right now?"
"""

from __future__ import annotations

import argparse
import json
from collections import Counter, defaultdict
from pathlib import Path
from typing import Any

from PIL import Image

from tileset_resolution_policy import resolution_gate_failure, source_resolution_policy


ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / "assets" / "tiles"
IMAGE_JSON = ROOT / "art" / "tiles" / "image-json"

SPRITE_GROUPS = {
    "entities",
    "features",
    "decals",
    "effects-ui",
}

NOT_WIRED_GROUPS = {
    "features": "feature PNG manifests exist, but current SDL draw path still uses feature glyph occluders",
    "decals": "decal PNGs exist, but current SDL map draw path does not load decal images",
    "projectiles": "projectile manifests exist, but current projectile draw path still uses glyphs/effects-ui aliases",
    "effects-ui": "effects/UI PNGs exist, but current map draw paths still use procedural/glyph effects",
}

WIRING_EVIDENCE = {
    "features": [
        "tilesetLoadFeatureTileScaled",
        "drawFeatureTextureWithFallbacks",
        "drawFeatureOccluderIfNeeded",
    ],
    "decals": [
        "tilesetLoadDecalTileScaled",
        "drawDecalTexture",
        "drawVisualTilePartImages",
    ],
    "projectiles": [
        "tilesetLoadProjectileTileScaled",
        "drawProjectileSpriteAt",
    ],
    "effects-ui": [
        "tilesetLoadEffectUiTileScaled",
        "actionMarkerAssetId",
    ],
}

ENTITY_ALIAS_SOURCES = {
    "archer": {"idle": "self_bow__idle", "death": "self_bow__dead"},
    "militia": {"idle": "basic_weapons__idle", "death": "basic_weapons__dead"},
    "knight": {"idle": "basic_weapons__open_helmet__idle", "death": "basic_weapons__open_helmet__dead"},
    "spearman": {"idle": "short_spear__idle", "death": "short_spear__dead"},
    "trebuchet": {"idle": "traction_trebuchet__idle", "death": "traction_trebuchet__destroyed_wreck"},
    "deer": {"idle": "idle_graze", "death": "dead"},
    "sheep": {"idle": "idle_graze", "death": "dead"},
}


def rel(path: Path) -> str:
    return path.resolve().relative_to(ROOT.resolve()).as_posix()


def image_stats(path: Path) -> dict[str, Any]:
    with Image.open(path).convert("RGBA") as img:
        width, height = img.size
        total = width * height
        transparent = 0
        opaque_magenta = 0
        semi_magenta = 0
        pixels = img.get_flattened_data() if hasattr(img, "get_flattened_data") else img.getdata()
        for r, g, b, a in pixels:
            if a < 8:
                transparent += 1
            is_magenta = r >= 175 and b >= 145 and g <= 105
            if is_magenta and a >= 240:
                opaque_magenta += 1
            elif is_magenta and a >= 8:
                semi_magenta += 1
        return {
            "width": width,
            "height": height,
            "bytes": path.stat().st_size,
            "transparent_pixels": transparent,
            "opaque_magenta_pixels": opaque_magenta,
            "semi_magenta_pixels": semi_magenta,
            "total_pixels": total,
            "alpha_bbox": alpha_bbox(img),
        }


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def alpha_bbox(img: Image.Image) -> dict[str, int]:
    bbox = img.getchannel("A").getbbox()
    if not bbox:
        return {"x": 0, "y": 0, "width": 0, "height": 0}
    left, top, right, bottom = bbox
    return {"x": left, "y": top, "width": right - left, "height": bottom - top}


def source_policy_from_spec(path: Path, group: str, action_id: str | None = None) -> dict[str, Any]:
    data = load_json(path)
    canvas = None
    if action_id:
        for action in data.get("actions", []):
            if isinstance(action, dict) and action.get("id") == action_id and isinstance(action.get("source_canvas"), dict):
                canvas = action["source_canvas"]
                break
    if not isinstance(canvas, dict):
        canvas = data.get("art", {}).get("source_canvas")
    if not isinstance(canvas, dict):
        canvas = data.get("source_canvas")
    footprint = None
    if isinstance(canvas, dict) and isinstance(canvas.get("footprint"), dict):
        footprint = canvas["footprint"]
    elif isinstance(data.get("placement"), dict) and isinstance(data["placement"].get("footprint"), dict):
        footprint = data["placement"]["footprint"]
    visual_envelope = canvas.get("visual_envelope") if isinstance(canvas, dict) else None
    policy = source_resolution_policy(group, footprint=footprint, visual_envelope=visual_envelope)
    if not policy:
        return {}
    policy = dict(policy)
    if isinstance(canvas, dict):
        for key in (
            "width_px",
            "height_px",
            "target_kind",
            "min_width_px",
            "min_height_px",
            "min_longest_side_px",
            "profile",
            "range",
            "visual_envelope",
        ):
            if key in canvas:
                policy[key] = canvas[key]
    return policy


def expected_source_policy_for_asset(path: Path) -> tuple[dict[str, Any], str]:
    try:
        parts = path.relative_to(ASSETS).parts
    except ValueError:
        return ({}, "")
    if not parts:
        return ({}, "")

    group = parts[0]
    if group == "grounds" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        return (source_policy_from_spec(IMAGE_JSON / "grounds" / f"{slug}.json", "grounds"), f"grounds/{slug}")
    if group == "features" and len(parts) >= 2:
        slug = parts[1]
        return (source_policy_from_spec(IMAGE_JSON / "features" / f"{slug}.json", "features"), f"features/{slug}")
    if group == "decals" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        return (source_policy_from_spec(IMAGE_JSON / "decals" / f"{slug}.json", "decals"), f"decals/{slug}")
    if group == "entities" and len(parts) >= 2:
        slug = parts[1]
        action_id = parts[2] if len(parts) >= 5 and parts[-1].endswith(".png") else None
        for spec_group in ("units", "animals", "buildings"):
            policy = source_policy_from_spec(IMAGE_JSON / spec_group / f"{slug}.json", spec_group, action_id)
            if policy:
                spec_id = f"{spec_group}/{slug}"
                if action_id:
                    spec_id += f"/{action_id}"
                return (policy, spec_id)
    if group == "effects-ui" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        for spec_group in ("effects", "user_interface", "projectiles"):
            policy = source_policy_from_spec(IMAGE_JSON / spec_group / f"{slug}.json", spec_group)
            if policy:
                return (policy, f"{spec_group}/{slug}")
        return (source_resolution_policy("effects"), f"effects-ui/{slug}")
    return ({}, "")


def audit_png(path: Path) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    stats = image_stats(path)
    parts = path.relative_to(ASSETS).parts
    group = parts[0] if parts else ""
    policy, spec_id = expected_source_policy_for_asset(path)
    resolution_failure = resolution_gate_failure(stats["width"], stats["height"], policy)
    if resolution_failure:
        issues.append(
            {
                "kind": "undersized_runtime_source",
                "path": rel(path),
                "message": resolution_failure,
                "expected": {"policy": policy, "spec": spec_id},
                "stats": stats,
            }
        )
    lane = "ground" if group == "grounds" else "sprite"
    if lane == "ground":
        if stats["width"] < 128 or stats["height"] < 128:
            issues.append(
                {
                    "kind": "ground_placeholder_size",
                    "path": rel(path),
                    "message": "ground source is below runtime-quality minimum; likely ignored or placeholder-like",
                    "stats": stats,
                }
            )
        return issues

    if group in SPRITE_GROUPS:
        if stats["opaque_magenta_pixels"] > 8:
            issues.append(
                {
                    "kind": "opaque_magenta",
                    "path": rel(path),
                    "message": "runtime sprite/overlay has residual opaque magenta after promotion",
                    "stats": stats,
                }
            )
        if group == "entities" and path.name.endswith("_base.png") and stats["transparent_pixels"] == 0:
            issues.append(
                {
                    "kind": "opaque_actor_background",
                    "path": rel(path),
                    "message": "entity base frame is fully opaque; actor sprites should usually be cutouts",
                    "stats": stats,
                }
            )
    return issues


def audit_entity_manifest_metadata() -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    entities_root = ASSETS / "entities"
    if not entities_root.exists():
        return issues
    for manifest_path in sorted(entities_root.glob("*/manifest.json")):
        manifest = load_json(manifest_path)
        entity = manifest_path.parent.name
        if not manifest.get("asset_type"):
            issues.append(
                {
                    "kind": "missing_entity_manifest_asset_type",
                    "path": rel(manifest_path),
                    "message": "entity manifest should declare asset_type so placement defaults do not depend on historical sprite_size assumptions",
                    "entity": entity,
                }
            )
        placement = manifest.get("placement")
        if not isinstance(placement, dict):
            issues.append(
                {
                    "kind": "missing_entity_manifest_placement",
                    "path": rel(manifest_path),
                    "message": "entity manifest should declare top-level placement with source_size, anchor, scale_policy, footprint, and depth",
                    "entity": entity,
                }
            )
            continue
        canvas = manifest.get("source_canvas")
        if not isinstance(canvas, dict):
            continue
        width = canvas.get("width_px")
        height = canvas.get("height_px")
        if isinstance(width, int) and isinstance(height, int):
            expected_source_size = [width, height]
            if placement.get("source_size") != expected_source_size:
                issues.append(
                    {
                        "kind": "entity_manifest_source_size_mismatch",
                        "path": rel(manifest_path),
                        "message": "placement.source_size must match source_canvas width_px/height_px",
                        "entity": entity,
                        "expected": expected_source_size,
                        "actual": placement.get("source_size"),
                    }
                )
            sprite_size = manifest.get("sprite_size")
            if isinstance(sprite_size, int) and sprite_size < max(width, height):
                issues.append(
                    {
                        "kind": "entity_manifest_sprite_size_below_canvas",
                        "path": rel(manifest_path),
                        "message": "manifest sprite_size must be at least the longest source_canvas axis",
                        "entity": entity,
                        "expected_min": max(width, height),
                        "actual": sprite_size,
                    }
                )
        visual_envelope = canvas.get("visual_envelope")
        if isinstance(visual_envelope, dict):
            expected_envelope = [
                int(visual_envelope.get("w", 1) or 1),
                int(visual_envelope.get("h", 1) or 1),
            ]
            if placement.get("visual_envelope") != expected_envelope:
                issues.append(
                    {
                        "kind": "entity_manifest_visual_envelope_mismatch",
                        "path": rel(manifest_path),
                        "message": "placement.visual_envelope must match source_canvas.visual_envelope",
                        "entity": entity,
                        "expected": expected_envelope,
                        "actual": placement.get("visual_envelope"),
                    }
                )
        footprint = canvas.get("footprint")
        if isinstance(footprint, dict):
            expected_footprint = [
                int(footprint.get("w", 1) or 1),
                int(footprint.get("h", 1) or 1),
            ]
            if placement.get("footprint") != expected_footprint:
                issues.append(
                    {
                        "kind": "entity_manifest_footprint_mismatch",
                        "path": rel(manifest_path),
                        "message": "placement.footprint must match source_canvas.footprint",
                        "entity": entity,
                        "expected": expected_footprint,
                        "actual": placement.get("footprint"),
                    }
                )
        for action_id, action in sorted(manifest.get("actions", {}).items()):
            if not isinstance(action, dict):
                continue
            action_canvas = action.get("source_canvas")
            action_placement = action.get("placement")
            if not isinstance(action_canvas, dict):
                continue
            action_width = action_canvas.get("width_px")
            action_height = action_canvas.get("height_px")
            if isinstance(action_width, int) and isinstance(action_height, int):
                sprite_size = manifest.get("sprite_size")
                if isinstance(sprite_size, int) and sprite_size < max(action_width, action_height):
                    issues.append(
                        {
                            "kind": "entity_manifest_sprite_size_below_action_canvas",
                            "path": rel(manifest_path),
                            "message": "manifest sprite_size must be at least the longest action source_canvas axis",
                            "entity": entity,
                            "action": action_id,
                            "expected_min": max(action_width, action_height),
                            "actual": sprite_size,
                        }
                    )
                if not isinstance(action_placement, dict):
                    issues.append(
                        {
                            "kind": "missing_entity_action_placement",
                            "path": rel(manifest_path),
                            "message": "action with source_canvas should declare action-level placement",
                            "entity": entity,
                            "action": action_id,
                        }
                    )
                    continue
                expected_source_size = [action_width, action_height]
                if action_placement.get("source_size") != expected_source_size:
                    issues.append(
                        {
                            "kind": "entity_action_source_size_mismatch",
                            "path": rel(manifest_path),
                            "message": "action placement.source_size must match action source_canvas width_px/height_px",
                            "entity": entity,
                            "action": action_id,
                            "expected": expected_source_size,
                            "actual": action_placement.get("source_size"),
                        }
                    )
            action_envelope = action_canvas.get("visual_envelope")
            if isinstance(action_envelope, dict) and isinstance(action_placement, dict):
                expected_envelope = [
                    int(action_envelope.get("w", 1) or 1),
                    int(action_envelope.get("h", 1) or 1),
                ]
                if action_placement.get("visual_envelope") != expected_envelope:
                    issues.append(
                        {
                            "kind": "entity_action_visual_envelope_mismatch",
                            "path": rel(manifest_path),
                            "message": "action placement.visual_envelope must match action source_canvas.visual_envelope",
                            "entity": entity,
                            "action": action_id,
                            "expected": expected_envelope,
                            "actual": action_placement.get("visual_envelope"),
                        }
                    )
    return issues


def action_frame_exists(entity_root: Path, action: str, direction: str, frame: int) -> bool:
    return (entity_root / action / direction / f"frame_{frame:02d}_base.png").exists()


def audit_entity_aliases() -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    entities_root = ASSETS / "entities"
    if not entities_root.exists():
        return issues
    for entity_root in sorted(p for p in entities_root.iterdir() if p.is_dir()):
        slug = entity_root.name
        for action in ("idle", "death"):
            for direction in ("front", "back"):
                if not action_frame_exists(entity_root, action, direction, 0):
                    source = ENTITY_ALIAS_SOURCES.get(slug, {}).get(action)
                    issues.append(
                        {
                            "kind": "missing_runtime_action_alias",
                            "path": rel(entity_root),
                            "message": f"current simple runtime selector may request {action}/{direction}/frame_00_base.png",
                            "entity": slug,
                            "action": action,
                            "direction": direction,
                            "suggested_source_action": source,
                        }
                    )
    return issues


def audit_wiring() -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    code_text = "\n".join(
        path.read_text(encoding="utf-8", errors="ignore")
        for path in [
            ROOT / "include" / "tileset_assets.h",
            ROOT / "src" / "render" / "sdl" / "tileset_assets.cpp",
            ROOT / "src" / "render" / "sdl" / "display_glyphs.cpp",
            ROOT / "src" / "render" / "sdl" / "camera.cpp",
            ROOT / "src" / "render" / "sdl" / "map_renderer.cpp",
        ]
        if path.exists()
    )
    for group, message in NOT_WIRED_GROUPS.items():
        root = ASSETS / group
        has_assets = root.exists() and (any(root.rglob("*.png")) or any(root.rglob("manifest.json")))
        has_wiring = all(symbol in code_text for symbol in WIRING_EVIDENCE[group])
        if has_assets and not has_wiring:
            issues.append(
                {
                    "kind": "lane_not_fully_runtime_wired",
                    "path": rel(root),
                    "message": message,
                    "group": group,
                }
            )
    return issues


def audit_assets() -> dict[str, Any]:
    issues: list[dict[str, Any]] = []
    for path in sorted(ASSETS.rglob("*.png")):
        issues.extend(audit_png(path))
    issues.extend(audit_entity_manifest_metadata())
    issues.extend(audit_entity_aliases())
    issues.extend(audit_wiring())
    counts = Counter(issue["kind"] for issue in issues)
    by_group: dict[str, int] = defaultdict(int)
    for issue in issues:
        parts = Path(issue["path"]).parts
        if len(parts) >= 3 and parts[0] == "assets" and parts[1] == "tiles":
            by_group[parts[2]] += 1
    return {
        "schema": "realm.tileset_quality_audit.v1",
        "issue_count": len(issues),
        "issue_counts": dict(sorted(counts.items())),
        "group_issue_counts": dict(sorted(by_group.items())),
        "issues": issues,
    }


def render_markdown(payload: dict[str, Any], limit: int) -> str:
    lines = ["# Realm Tileset Quality Audit", ""]
    lines.append(f"Issues: {payload['issue_count']}")
    lines.append("")
    lines.append("## Issue Counts")
    lines.append("")
    lines.append("| Kind | Count |")
    lines.append("|---|---:|")
    for kind, count in payload["issue_counts"].items():
        lines.append(f"| `{kind}` | {count} |")
    lines.append("")
    lines.append("## Group Counts")
    lines.append("")
    lines.append("| Group | Count |")
    lines.append("|---|---:|")
    for group, count in payload["group_issue_counts"].items():
        lines.append(f"| `{group}` | {count} |")
    lines.append("")
    lines.append("## First Issues")
    lines.append("")
    lines.append("| Kind | Path | Message |")
    lines.append("|---|---|---|")
    for issue in payload["issues"][:limit]:
        lines.append(f"| `{issue['kind']}` | `{issue['path']}` | {issue['message']} |")
    remaining = len(payload["issues"]) - min(limit, len(payload["issues"]))
    if remaining > 0:
        lines.append(f"| ... | ... | {remaining} more issue(s) not shown |")
    return "\n".join(lines) + "\n"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--json-out", default="build/tileset-quality-audit.json")
    parser.add_argument("--md-out", default="build/tileset-quality-audit.md")
    parser.add_argument("--detail-limit", type=int, default=120)
    parser.add_argument(
        "--focus",
        choices=["all", "resolution"],
        default="all",
        help="resolution keeps only runtime-size and source/placement contract findings",
    )
    args = parser.parse_args()

    payload = audit_assets()
    if args.focus == "resolution":
        resolution_kinds = {
            "undersized_runtime_source",
            "missing_entity_manifest_asset_type",
            "missing_entity_manifest_placement",
            "entity_manifest_source_size_mismatch",
            "entity_manifest_sprite_size_below_canvas",
            "entity_manifest_sprite_size_below_action_canvas",
            "entity_manifest_visual_envelope_mismatch",
            "entity_manifest_footprint_mismatch",
            "missing_entity_action_placement",
            "entity_action_source_size_mismatch",
            "entity_action_visual_envelope_mismatch",
        }
        payload["issues"] = [issue for issue in payload["issues"] if issue["kind"] in resolution_kinds]
        payload["issue_count"] = len(payload["issues"])
        payload["issue_counts"] = dict(sorted(Counter(issue["kind"] for issue in payload["issues"]).items()))
        by_group: dict[str, int] = defaultdict(int)
        for issue in payload["issues"]:
            parts = Path(issue["path"]).parts
            if len(parts) >= 3 and parts[0] == "assets" and parts[1] == "tiles":
                by_group[parts[2]] += 1
        payload["group_issue_counts"] = dict(sorted(by_group.items()))
        payload["focus"] = args.focus
    json_out = ROOT / args.json_out
    md_out = ROOT / args.md_out
    json_out.parent.mkdir(parents=True, exist_ok=True)
    md_out.parent.mkdir(parents=True, exist_ok=True)
    json_out.write_text(json.dumps(payload, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    md_out.write_text(render_markdown(payload, args.detail_limit), encoding="utf-8")
    print(f"wrote {json_out}")
    print(f"wrote {md_out}")
    print(f"issues={payload['issue_count']}")
    for kind, count in payload["issue_counts"].items():
        print(f"{kind}={count}")
    return 1 if payload["issue_count"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
