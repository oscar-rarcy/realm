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
        for r, g, b, a in img.getdata():
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
        }


def load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def source_canvas_from_spec(path: Path) -> tuple[int, int]:
    data = load_json(path)
    canvas = data.get("art", {}).get("source_canvas")
    if not isinstance(canvas, dict):
        canvas = data.get("source_canvas")
    if not isinstance(canvas, dict):
        return (0, 0)
    try:
        return (int(canvas.get("width_px", 0)), int(canvas.get("height_px", 0)))
    except (TypeError, ValueError):
        return (0, 0)


def expected_source_canvas_for_asset(path: Path) -> tuple[int, int, str]:
    try:
        parts = path.relative_to(ASSETS).parts
    except ValueError:
        return (0, 0, "")
    if not parts:
        return (0, 0, "")

    group = parts[0]
    if group == "grounds" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        w, h = source_canvas_from_spec(IMAGE_JSON / "grounds" / f"{slug}.json")
        return (w, h, f"grounds/{slug}")
    if group == "features" and len(parts) >= 2:
        slug = parts[1]
        w, h = source_canvas_from_spec(IMAGE_JSON / "features" / f"{slug}.json")
        return (w, h, f"features/{slug}")
    if group == "decals" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        w, h = source_canvas_from_spec(IMAGE_JSON / "decals" / f"{slug}.json")
        return (w, h, f"decals/{slug}")
    if group == "entities" and len(parts) >= 2:
        slug = parts[1]
        for spec_group in ("units", "animals", "buildings"):
            w, h = source_canvas_from_spec(IMAGE_JSON / spec_group / f"{slug}.json")
            if w > 0 and h > 0:
                return (w, h, f"{spec_group}/{slug}")
    if group == "effects-ui" and len(parts) >= 2:
        slug = path.stem if len(parts) == 2 else parts[1]
        for spec_group in ("effects", "user_interface", "projectiles"):
            w, h = source_canvas_from_spec(IMAGE_JSON / spec_group / f"{slug}.json")
            if w > 0 and h > 0:
                return (w, h, f"{spec_group}/{slug}")
        return (512, 512, f"effects-ui/{slug}")
    return (0, 0, "")


def audit_png(path: Path) -> list[dict[str, Any]]:
    issues: list[dict[str, Any]] = []
    stats = image_stats(path)
    parts = path.relative_to(ASSETS).parts
    group = parts[0] if parts else ""
    expected_w, expected_h, spec_id = expected_source_canvas_for_asset(path)
    if expected_w > 0 and expected_h > 0 and (stats["width"] < expected_w or stats["height"] < expected_h):
        issues.append(
            {
                "kind": "undersized_runtime_source",
                "path": rel(path),
                "message": "runtime PNG is smaller than the generated source-canvas contract and will blur when drawn close",
                "expected": {"width": expected_w, "height": expected_h, "spec": spec_id},
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
    args = parser.parse_args()

    payload = audit_assets()
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
