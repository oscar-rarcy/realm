#!/usr/bin/env python3
"""Production-readiness audit for Realm tileset assets.

This is stricter than scripts/tileset_quality_audit.py. It checks whether
runtime PNGs are credible production art with traceable provenance, not just
whether they exist and can be loaded.
"""

from __future__ import annotations

import argparse
import fnmatch
import hashlib
import json
import math
import os
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any, Iterable

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:
    raise SystemExit("Pillow is required for tileset production audit: pip install Pillow") from exc


SEVERITY_ORDER = {"blocker": 0, "high": 1, "medium": 2, "review": 3, "info": 4}
PNG_SUFFIX = ".png"
RUNTIME_LANES = (
    "assets/tiles/grounds",
    "assets/tiles/decals",
    "assets/tiles/effects-ui",
    "assets/tiles/entities",
    "assets/tiles/features",
    "assets/tiles/projectiles",
)
LOW_INFO_LANES = ("assets/tiles/decals/", "assets/tiles/effects-ui/")
EXPECTED_STYLE_CONTRACTS = (
    ("assets/tiles/decals/", {"realm_simplified_hand_painted_ground_decal"}),
    ("assets/tiles/grounds/", {"realm_ground_slab_small_tile", "realm_paper_cutout_small_tile"}),
    ("assets/tiles/entities/", {"realm_paper_cutout_small_tile"}),
    ("assets/tiles/features/", {"realm_paper_cutout_small_tile", "realm_map_integrated_painted_feature"}),
    ("assets/tiles/projectiles/", {"realm_paper_cutout_small_tile", "realm_projectile_cutout"}),
    ("assets/tiles/effects-ui/", {"realm_effect_overlay", "realm_ui_marker", "procedural_overlay"}),
)
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
PROJECTILE_EFFECT_FILENAMES = {
    "arrow_projectile.png",
    "catapult_boulder_projectile.png",
    "tower_bolt_projectile.png",
    "warship_shot_projectile.png",
}
GROUND_CURRENT_REQUIRED = (
    "art/tiles/reference/grounds/current/blank.png",
    "art/tiles/reference/grounds/current/grass.png",
    "art/tiles/reference/grounds/current/shades-of-grey",
    "art/tiles/reference/grounds/current/source-manifest.json",
)
GROUND_STALE_REFERENCE_PATTERNS = (
    "art/tiles/reference/grounds/examples/*",
    "art/tiles/reference/grounds/generated/unknown/*",
    "art/tiles/reference/grounds/blank-*.png",
    "art/tiles/reference/grounds/grass.png",
)
KNOWN_STALE_PROMPT_TEXT = (
    "art/tiles/reference/grounds/examples/",
    "art/tiles/reference/grounds/generated/unknown/",
    "art/tiles/reference/grounds/blank-",
    "art/tiles/reference/grounds/grass.png",
)
EXPECTED_NEW_GROUND_SOURCES = (
    "art/reference/ground/shades-of-grey",
    "art/reference/ground/blank.png",
    "art/reference/ground/grass.png",
)
PROVENANCE_DIRS = (
    "art/tiles/candidates",
    "art/tiles/image-spec",
    "art/tiles/image-json",
    "art/tiles/prompts",
    "art/tiles/reviews",
)
PROVENANCE_FILES = ("art/tiles/generation-ledger.jsonl", "art/tiles/production-ledger.jsonl")
TEXT_SUFFIXES = {".json", ".jsonl", ".md", ".txt", ".yaml", ".yml", ".csv"}


@dataclass
class AssetStats:
    rel: str
    path: Path
    width: int
    height: int
    byte_size: int
    sha256: str
    visible_pixels: int
    transparent_pixels: int
    magenta_pixels: int
    unique_visible_colors: int
    bbox: tuple[int, int, int, int] | None

    @property
    def area(self) -> int:
        return self.width * self.height

    @property
    def visible_ratio(self) -> float:
        return 0.0 if self.area == 0 else self.visible_pixels / self.area


@dataclass
class Issue:
    category: str
    severity: str
    message: str
    path: str | None = None
    evidence: dict[str, Any] = field(default_factory=dict)

    def to_json(self) -> dict[str, Any]:
        out = {"severity": self.severity, "category": self.category, "message": self.message}
        if self.path:
            out["path"] = self.path
        if self.evidence:
            out["evidence"] = self.evidence
        return out


def relpath(path: Path, root: Path) -> str:
    return path.resolve().relative_to(root.resolve()).as_posix()


def read_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as f:
        return json.load(f)


def file_sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def image_stats(path: Path, root: Path) -> AssetStats:
    with Image.open(path) as img:
        rgba = img.convert("RGBA")
        w, h = rgba.size
        data = rgba.tobytes()

    visible = transparent = magenta = 0
    colors: set[tuple[int, int, int, int]] = set()
    min_x, min_y, max_x, max_y = w, h, -1, -1
    for i in range(0, len(data), 4):
        r, g, b, a = data[i], data[i + 1], data[i + 2], data[i + 3]
        pixel_index = i // 4
        x, y = pixel_index % w, pixel_index // w
        if a == 0:
            transparent += 1
            continue
        visible += 1
        if r >= 220 and g <= 60 and b >= 180:
            magenta += 1
        if len(colors) <= 100_000:
            colors.add((r, g, b, a))
        min_x, min_y = min(min_x, x), min(min_y, y)
        max_x, max_y = max(max_x, x), max(max_y, y)

    return AssetStats(
        rel=relpath(path, root),
        path=path,
        width=w,
        height=h,
        byte_size=path.stat().st_size,
        sha256=file_sha256(path),
        visible_pixels=visible,
        transparent_pixels=transparent,
        magenta_pixels=magenta,
        unique_visible_colors=len(colors),
        bbox=None if visible == 0 else (min_x, min_y, max_x, max_y),
    )


def iter_runtime_pngs(root: Path) -> Iterable[Path]:
    for lane in RUNTIME_LANES:
        base = root / lane
        if base.exists():
            yield from sorted(base.rglob(f"*{PNG_SUFFIX}"))


def looks_mockup_like(stats: AssetStats) -> bool:
    if not stats.rel.startswith(LOW_INFO_LANES):
        return False
    small_canvas = stats.width <= 64 and stats.height <= 64
    tiny_file = stats.byte_size < 1024
    few_visible_pixels = stats.visible_pixels < 700
    few_colors = stats.unique_visible_colors <= 16
    very_sparse = stats.visible_ratio < 0.25
    return small_canvas and tiny_file and (few_visible_pixels or few_colors or very_sparse)


def has_bright_crop_box_artifact(stats: AssetStats) -> bool:
    if not stats.rel.startswith("assets/tiles/entities/"):
        return False
    if stats.rel.endswith("_teammask.png"):
        return False
    if stats.width > 96 or stats.height > 96:
        return False
    with Image.open(stats.path) as img:
        rgba = img.convert("RGBA")
        pix = rgba.load()
        row_hits = [0 for _ in range(rgba.height)]
        col_hits = [0 for _ in range(rgba.width)]
        for y in range(rgba.height):
            for x in range(rgba.width):
                r, g, b, a = pix[x, y]
                if not a:
                    continue
                greyish = max(r, g, b) - min(r, g, b) <= 18
                bright = r >= 218 and g >= 218 and b >= 218
                if greyish and bright:
                    row_hits[y] += 1
                    col_hits[x] += 1
    row_threshold = max(10, int(stats.width * 0.28))
    col_threshold = max(10, int(stats.height * 0.28))
    long_rows = sum(1 for count in row_hits if count >= row_threshold)
    long_cols = sum(1 for count in col_hits if count >= col_threshold)
    if long_rows >= 2 and long_cols >= 2:
        return True

    # Image generation often leaves low-alpha/off-white crop rectangles or
    # magenta guide fragments that are visible in contact sheets but too dim
    # for the bright-pixel detector above. Detect only detached sparse line or
    # rectangle components so legitimate spears, reins, and silhouette details
    # connected to the main sprite do not fail the audit.
    with Image.open(stats.path) as img:
        rgba = img.convert("RGBA")
        pix = rgba.load()
        w, h = rgba.size
        visible = {(x, y) for y in range(h) for x in range(w) if pix[x, y][3] > 0}
        components: list[list[tuple[int, int]]] = []
        while visible:
            start = visible.pop()
            stack = [start]
            comp = [start]
            while stack:
                x, y = stack.pop()
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if (nx, ny) in visible:
                        visible.remove((nx, ny))
                        stack.append((nx, ny))
                        comp.append((nx, ny))
            components.append(comp)
    if len(components) <= 1:
        return False
    components.sort(key=len, reverse=True)
    for comp in components[1:]:
        xs = [x for x, _ in comp]
        ys = [y for _, y in comp]
        x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
        bw, bh = x1 - x0 + 1, y1 - y0 + 1
        if bw <= 0 or bh <= 0:
            continue
        density = len(comp) / float(bw * bh)
        line_like = (bw >= max(8, int(w * 0.25)) and bh <= 3) or (bh >= max(8, int(h * 0.25)) and bw <= 3)
        sparse_rect = bw >= max(12, int(w * 0.35)) and bh >= max(12, int(h * 0.35)) and density < 0.18
        magenta_like = 0
        pale_like = 0
        for x, y in comp:
            r, g, b, a = pix[x, y]
            if r > 200 and b > 120 and g < 140:
                magenta_like += 1
            greyish = max(r, g, b) - min(r, g, b) <= 35
            if greyish and r >= 170 and g >= 150 and b >= 130:
                pale_like += 1
        suspicious_colour = (magenta_like + pale_like) >= max(3, len(comp) // 3)
        if suspicious_colour and (line_like or sparse_rect):
            return True
    return False


def expected_style_contracts(rel: str) -> set[str]:
    if rel.startswith("assets/tiles/effects-ui/"):
        name = rel.rsplit("/", 1)[-1]
        if name in PROJECTILE_EFFECT_FILENAMES:
            return {"realm_projectile_cutout"}
    if rel.startswith("assets/tiles/entities/"):
        parts = rel.split("/")
        if len(parts) > 3 and parts[3] in BUILDING_ENTITY_SLUGS:
            return {"realm_map_integrated_painted_feature"}
    for prefix, contracts in EXPECTED_STYLE_CONTRACTS:
        if rel.startswith(prefix):
            return contracts
    return {"realm_paper_cutout_small_tile"}


def load_review_index(root: Path) -> tuple[dict[str, dict[str, Any]], list[dict[str, Any]], list[Issue]]:
    issues: list[Issue] = []
    review_path = root / "art/tiles/reviews/production-review.json"
    if not review_path.exists():
        issues.append(
            Issue(
                "missing_production_review_index",
                "high",
                "Missing production review index. Runtime assets need accepted/placeholder/needs_regeneration evidence.",
                "art/tiles/reviews/production-review.json",
            )
        )
        return {}, [], issues
    try:
        data = read_json(review_path)
    except Exception as exc:
        issues.append(Issue("invalid_production_review_index", "blocker", f"Could not parse production review index: {exc}", relpath(review_path, root)))
        return {}, [], issues

    assets = data.get("assets", {}) if isinstance(data, dict) else {}
    patterns = data.get("patterns", []) if isinstance(data, dict) else []
    legacy_assets = {
        k: v
        for k, v in data.items()
        if isinstance(data, dict) and isinstance(k, str) and k.startswith("assets/") and isinstance(v, dict)
    }
    assets.update(legacy_assets)
    if not isinstance(assets, dict) or not isinstance(patterns, list):
        issues.append(Issue("invalid_production_review_index", "blocker", "production-review.json must contain object fields assets and patterns.", relpath(review_path, root)))
        return {}, [], issues
    return assets, patterns, issues


def review_for(rel: str, exact: dict[str, dict[str, Any]], patterns: list[dict[str, Any]]) -> dict[str, Any] | None:
    if rel in exact:
        return exact[rel]
    for item in patterns:
        glob = item.get("glob") if isinstance(item, dict) else None
        if glob and fnmatch.fnmatch(rel, glob):
            return item
    return None


def build_text_index(root: Path) -> str:
    chunks: list[str] = []
    for rel in PROVENANCE_FILES:
        path = root / rel
        if not path.exists():
            continue
        try:
            chunks.append(f"\n--- {rel} ---\n{path.read_text(encoding='utf-8', errors='ignore').lower()}")
        except OSError:
            pass
    for base_rel in PROVENANCE_DIRS:
        base = root / base_rel
        if not base.exists():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            try:
                text = path.read_text(encoding="utf-8", errors="ignore")
            except OSError:
                continue
            chunks.append(f"\n--- {relpath(path, root)} ---\n{text.lower()}")
    return "\n".join(chunks)


def check_generation_ledger(root: Path, issues: list[Issue]) -> None:
    ledger = root / "art/tiles/generation-ledger.jsonl"
    if not ledger.exists():
        issues.append(
            Issue(
                "missing_generation_ledger",
                "high",
                "Missing generation ledger. Future audits cannot prove which canonical prompts, references, grids, seeds, or candidates produced runtime assets.",
                "art/tiles/generation-ledger.jsonl",
            )
        )
        return
    for line_no, line in enumerate(ledger.read_text(encoding="utf-8", errors="ignore").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            event = json.loads(line)
        except json.JSONDecodeError as exc:
            issues.append(Issue("invalid_generation_ledger_entry", "blocker", f"Could not parse ledger JSON line {line_no}: {exc}", "art/tiles/generation-ledger.jsonl"))
            continue
        missing = [field for field in ("id", "created_at", "canonical_prompt_export", "accepted_runtime_paths") if not event.get(field)]
        if missing:
            issues.append(
                Issue(
                    "incomplete_generation_ledger_entry",
                    "medium",
                    f"Generation ledger line {line_no} is missing required fields.",
                    "art/tiles/generation-ledger.jsonl",
                    {"missing": missing},
                )
            )


def has_provenance(stats: AssetStats, text_index: str) -> bool:
    rel = stats.rel.lower()
    name = Path(stats.rel).name.lower()
    stem = Path(stats.rel).stem.lower()
    return rel in text_index or name in text_index or (len(stem) >= 8 and stem in text_index)


def check_ground_references(root: Path, issues: list[Issue]) -> None:
    for required in GROUND_CURRENT_REQUIRED:
        if not (root / required).exists():
            issues.append(
                Issue(
                    "missing_current_ground_reference",
                    "high",
                    "Ground generation should use the new small-tile reference set through art/tiles/reference/grounds/current.",
                    required,
                    {"expected_source_refs": list(EXPECTED_NEW_GROUND_SOURCES)},
                )
            )

    grounds_ref = root / "art/tiles/reference/grounds"
    if grounds_ref.exists():
        for path in sorted(grounds_ref.rglob("*")):
            if not path.is_file():
                continue
            rel = relpath(path, root)
            if any(fnmatch.fnmatch(rel, pattern) for pattern in GROUND_STALE_REFERENCE_PATTERNS):
                issues.append(Issue("stale_ground_reference_present", "medium", "Old ground reference remains in the active ground reference tree and can be accidentally reused.", rel))

    for prompt_dir in (root / "art/tiles/image-spec/grounds", root / "art/tiles/image-json/grounds"):
        if not prompt_dir.exists():
            continue
        for path in sorted(prompt_dir.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            text = path.read_text(encoding="utf-8", errors="ignore")
            hits = [needle for needle in KNOWN_STALE_PROMPT_TEXT if needle in text]
            if hits:
                issues.append(
                    Issue(
                        "ground_prompt_uses_stale_reference",
                        "high",
                        "Canonical ground prompt export still mentions an old/stale ground reference path.",
                        relpath(path, root),
                        {"stale_references": hits},
                    )
                )


def check_manifests(root: Path, issues: list[Issue]) -> None:
    for base in (root / "assets/tiles/projectiles", root / "assets/tiles/features", root / "assets/tiles/entities"):
        if not base.exists():
            continue
        for manifest in sorted(base.rglob("manifest.json")):
            try:
                data = read_json(manifest)
            except Exception as exc:
                issues.append(Issue("invalid_runtime_manifest", "blocker", f"Could not parse manifest: {exc}", relpath(manifest, root)))
                continue
            for field in ("image", "base", "mask", "source"):
                value = data.get(field) if isinstance(data, dict) else None
                if not isinstance(value, str) or not value.endswith(PNG_SUFFIX):
                    continue
                candidate = (manifest.parent / value).resolve()
                if not candidate.exists():
                    repo_candidate = (root / value).resolve()
                    issues.append(
                        Issue(
                            "manifest_image_reference_unresolved",
                            "high",
                            f"Manifest field {field} does not resolve from the manifest directory.",
                            relpath(manifest, root),
                            {
                                "field": field,
                                "value": value,
                                "manifest_relative_target": os.path.relpath(candidate, root).replace("\\", "/"),
                                "repo_relative_exists": repo_candidate.exists(),
                            },
                        )
                    )


def check_duplicate_entity_frames(stats_by_hash: dict[str, list[AssetStats]], issues: list[Issue]) -> list[AssetStats]:
    duplicate_stats: list[AssetStats] = []
    groups: dict[str, list[AssetStats]] = defaultdict(list)
    for stats_list in stats_by_hash.values():
        for stats in stats_list:
            if not stats.rel.startswith("assets/tiles/entities/"):
                continue
            name = Path(stats.rel).name
            if not name.startswith("frame_") or not name.endswith("_base.png"):
                continue
            parts = stats.rel.split("/")
            if len(parts) >= 7:
                groups["/".join(parts[:6])].append(stats)

    for group_key, frame_stats in groups.items():
        by_hash: dict[str, list[AssetStats]] = defaultdict(list)
        for stats in frame_stats:
            by_hash[stats.sha256].append(stats)
        for same_hash in by_hash.values():
            if len(same_hash) > 1:
                duplicate_stats.extend(same_hash)
                issues.append(
                    Issue(
                        "duplicate_entity_animation_frame",
                        "medium",
                        "Entity action/direction contains exact duplicate base frames; this usually means a generated animation state collapsed.",
                        group_key,
                        {"duplicate_files": [s.rel for s in same_hash], "sha256": same_hash[0].sha256},
                    )
                )
    return duplicate_stats


def check_asset_statuses(
    assets: list[AssetStats],
    review_exact: dict[str, dict[str, Any]],
    review_patterns: list[dict[str, Any]],
    text_index: str,
    issues: list[Issue],
) -> tuple[list[AssetStats], list[AssetStats], list[AssetStats], list[AssetStats]]:
    mockups: list[AssetStats] = []
    magenta: list[AssetStats] = []
    unreviewed: list[AssetStats] = []
    bright_boxes: list[AssetStats] = []
    for stats in assets:
        review = review_for(stats.rel, review_exact, review_patterns)
        status = review.get("status") if isinstance(review, dict) else None
        style_contract = review.get("style_contract") if isinstance(review, dict) else None

        if stats.magenta_pixels:
            magenta.append(stats)
            issues.append(Issue("magenta_leakage", "blocker", "Opaque or semi-opaque magenta remains in a runtime PNG.", stats.rel, {"magenta_pixels": stats.magenta_pixels}))

        if looks_mockup_like(stats) and status != "procedural_by_design":
            mockups.append(stats)
            issues.append(
                Issue(
                    "mockup_like_runtime_asset",
                    "high",
                    "Runtime decal/effects asset looks like a tiny procedural placeholder rather than generated final art.",
                    stats.rel,
                    {
                        "size": [stats.width, stats.height],
                        "bytes": stats.byte_size,
                        "visible_pixels": stats.visible_pixels,
                        "unique_visible_colors": stats.unique_visible_colors,
                        "visible_ratio": round(stats.visible_ratio, 4),
                    },
                )
            )

        if has_bright_crop_box_artifact(stats):
            bright_boxes.append(stats)
            issues.append(
                Issue(
                    "bright_crop_box_artifact",
                    "high",
                    "Runtime entity sprite appears to contain a bright rectangular crop-box or checkerboard artifact.",
                    stats.rel,
                    {
                        "size": [stats.width, stats.height],
                        "bytes": stats.byte_size,
                        "visible_pixels": stats.visible_pixels,
                    },
                )
            )

        if review is None:
            unreviewed.append(stats)
            issues.append(Issue("missing_asset_production_review", "review", "Runtime PNG has no production review status. Style correctness cannot be audited later without this.", stats.rel))
        elif status in {"placeholder", "runtime_placeholder", "mockup_runtime_placeholder", "needs_regeneration", "wrong_style_runtime_art"}:
            issues.append(Issue("review_marked_not_production_ready", "high", f"Production review marks this asset as {status}.", stats.rel, review))
        elif status in {"accepted", "accepted_runtime_art"} and style_contract not in expected_style_contracts(stats.rel):
            issues.append(
                Issue(
                    "accepted_asset_missing_style_contract",
                    "medium",
                    "Accepted asset does not explicitly record one of the expected Realm style contracts for its lane.",
                    stats.rel,
                    {"review": review, "expected_style_contracts": sorted(expected_style_contracts(stats.rel))},
                )
            )

        if not has_provenance(stats, text_index) and status != "procedural_by_design":
            issues.append(
                Issue(
                    "missing_generation_provenance",
                    "medium",
                    "Runtime PNG is not linked from candidate/prompt/review text. Future audits cannot trace prompt, reference images, or generation lineage.",
                    stats.rel,
                    {"expected_places": list(PROVENANCE_DIRS)},
                )
            )
    return mockups, magenta, unreviewed, bright_boxes


def make_contact_sheet(items: list[AssetStats], out_path: Path, root: Path, title: str, limit: int = 80) -> str | None:
    if not items:
        return None
    out_path.parent.mkdir(parents=True, exist_ok=True)
    display = items[:limit]
    thumb, label_h, pad = 96, 42, 10
    cols = min(5, max(1, math.ceil(math.sqrt(len(display)))))
    rows = math.ceil(len(display) / cols)
    title_h = 30
    sheet = Image.new("RGB", (cols * (thumb + pad) + pad, title_h + rows * (thumb + label_h + pad) + pad), (248, 246, 238))
    draw = ImageDraw.Draw(sheet)
    try:
        font = ImageFont.truetype("arial.ttf", 10)
        title_font = ImageFont.truetype("arial.ttf", 15)
    except OSError:
        font = ImageFont.load_default()
        title_font = ImageFont.load_default()
    draw.text((pad, 8), title, fill=(38, 35, 28), font=title_font)
    for idx, stats in enumerate(display):
        col, row = idx % cols, idx // cols
        x, y = pad + col * (thumb + pad), title_h + pad + row * (thumb + label_h + pad)
        sheet.paste(Image.new("RGB", (thumb, thumb), (255, 0, 255)), (x, y))
        with Image.open(stats.path) as img:
            rgba = img.convert("RGBA")
            rgba.thumbnail((thumb, thumb), Image.Resampling.LANCZOS)
            sheet.paste(rgba, (x + (thumb - rgba.width) // 2, y + (thumb - rgba.height) // 2), rgba)
        draw.rectangle((x, y, x + thumb - 1, y + thumb - 1), outline=(65, 58, 48))
        short = stats.rel if len(stats.rel) <= 38 else "..." + stats.rel[-35:]
        draw.text((x, y + thumb + 4), short[:19], fill=(38, 35, 28), font=font)
        draw.text((x, y + thumb + 17), short[19:38], fill=(38, 35, 28), font=font)
        draw.text((x, y + thumb + 30), f"{stats.width}x{stats.height} {stats.byte_size}b", fill=(92, 79, 61), font=font)
    if len(items) > limit:
        draw.text((pad, sheet.height - pad - 14), f"Showing first {limit} of {len(items)} items.", fill=(120, 35, 35), font=font)
    sheet.save(out_path)
    return relpath(out_path, root)


def write_markdown(path: Path, issues: list[Issue], sheets: dict[str, str], asset_count: int) -> None:
    counts_by_severity = Counter(issue.severity for issue in issues)
    counts_by_category = Counter(issue.category for issue in issues)
    sorted_issues = sorted(issues, key=lambda i: (SEVERITY_ORDER.get(i.severity, 99), i.category, i.path or ""))
    lines = [
        "# Realm Tileset Production Audit",
        "",
        "This report checks production-readiness evidence, not just loader wiring.",
        "",
        f"- Runtime PNGs scanned: {asset_count}",
        f"- Issues found: {len(issues)}",
        "",
        "## Counts by severity",
        "",
    ]
    for sev in ("blocker", "high", "medium", "review", "info"):
        lines.append(f"- {sev}: {counts_by_severity.get(sev, 0)}")
    lines.extend(["", "## Counts by category", ""])
    for category, count in counts_by_category.most_common():
        lines.append(f"- {category}: {count}")
    if sheets:
        lines.extend(["", "## Contact sheets", ""])
        for name, sheet in sheets.items():
            lines.append(f"- {name}: `{sheet}`")
    lines.extend(
        [
            "",
            "## What this audit can and cannot prove",
            "",
            "- It can prove stale references, unresolved manifests, duplicate frames, magenta leakage, missing review/provenance, and low-information mockup-like assets.",
            "- It cannot prove exact style match for a detailed sprite without recorded generation lineage and human review.",
            "- Assets should only be accepted when a production review records status, style contract, source prompt export, reference images, and reviewer notes.",
            "",
            "## Issues",
            "",
        ]
    )
    if not sorted_issues:
        lines.append("No issues found.")
    for idx, issue in enumerate(sorted_issues, start=1):
        path_text = f" `{issue.path}`" if issue.path else ""
        lines.append(f"{idx}. **{issue.severity} / {issue.category}**{path_text}: {issue.message}")
        if issue.evidence:
            evidence = json.dumps(issue.evidence, ensure_ascii=False, sort_keys=True)
            lines.append(f"   Evidence: `{evidence[:697] + '...' if len(evidence) > 700 else evidence}`")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=".", help="Realm repository root.")
    parser.add_argument("--json-out", default="build/tileset-production-audit.json")
    parser.add_argument("--md-out", default="build/tileset-production-audit.md")
    parser.add_argument("--sheet-dir", default="build/tileset-production-audit")
    parser.add_argument("--fail-on", choices=["blocker", "high", "medium", "review", "never"], default="high")
    args = parser.parse_args()
    root = Path(args.repo_root).resolve()
    issues: list[Issue] = []

    review_exact, review_patterns, review_issues = load_review_index(root)
    issues.extend(review_issues)
    check_generation_ledger(root, issues)
    check_ground_references(root, issues)
    check_manifests(root, issues)
    text_index = build_text_index(root)
    assets = [image_stats(path, root) for path in iter_runtime_pngs(root)]

    stats_by_hash: dict[str, list[AssetStats]] = defaultdict(list)
    for stats in assets:
        stats_by_hash[stats.sha256].append(stats)
    duplicate_frames = check_duplicate_entity_frames(stats_by_hash, issues)
    mockups, magenta, unreviewed, bright_boxes = check_asset_statuses(assets, review_exact, review_patterns, text_index, issues)

    sheet_dir = root / args.sheet_dir
    sheets: dict[str, str] = {}
    for name, items, title in (
        ("mockup_like_runtime_assets", mockups, "Mockup-like runtime decals/effects/UI"),
        ("magenta_leakage", magenta, "Runtime images with magenta leakage"),
        ("bright_crop_box_artifacts", bright_boxes, "Runtime entity sprites with bright crop-box artifacts"),
        ("duplicate_entity_frames", duplicate_frames, "Duplicate entity base animation frames"),
        ("unreviewed_runtime_assets", unreviewed, "Runtime PNGs missing production review"),
    ):
        result = make_contact_sheet(items, sheet_dir / f"{name}.png", root, title)
        if result:
            sheets[name] = result

    sorted_issues = sorted(issues, key=lambda i: (SEVERITY_ORDER.get(i.severity, 99), i.category, i.path or ""))
    payload = {
        "summary": {
            "runtime_pngs_scanned": len(assets),
            "issues": len(issues),
            "by_severity": dict(Counter(issue.severity for issue in issues)),
            "by_category": dict(Counter(issue.category for issue in issues)),
            "contact_sheets": sheets,
        },
        "issues": [issue.to_json() for issue in sorted_issues],
        "asset_stats": [
            {
                "path": stats.rel,
                "width": stats.width,
                "height": stats.height,
                "bytes": stats.byte_size,
                "sha256": stats.sha256,
                "visible_pixels": stats.visible_pixels,
                "transparent_pixels": stats.transparent_pixels,
                "magenta_pixels": stats.magenta_pixels,
                "unique_visible_colors": stats.unique_visible_colors,
                "bbox": stats.bbox,
            }
            for stats in assets
        ],
    }
    json_out = root / args.json_out
    json_out.parent.mkdir(parents=True, exist_ok=True)
    json_out.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    write_markdown(root / args.md_out, sorted_issues, sheets, len(assets))

    if args.fail_on == "never":
        return 0
    threshold = SEVERITY_ORDER.get(args.fail_on, 99)
    highest = min((SEVERITY_ORDER.get(issue.severity, 99) for issue in issues), default=99)
    return 1 if highest <= threshold else 0


if __name__ == "__main__":
    raise SystemExit(main())
