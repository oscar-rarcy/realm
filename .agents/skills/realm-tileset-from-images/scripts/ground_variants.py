#!/usr/bin/env python3
"""Prepare and ingest Realm ground variant image sheets.

This helper intentionally leaves the image-generation step outside the script.
Use ``prepare`` to create an editable reference grid and prompt text, then pass
the generated sheet to ``ingest`` to store, split, inspect, and write candidate
metadata.
"""

from __future__ import annotations

import argparse
import json
import math
import re
import shutil
from datetime import datetime
from pathlib import Path
from typing import Any

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageStat


SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT = SCRIPT_DIR.parents[3]
DEFAULT_SEASONAL_SLOTS = [
    ("spring", "fresh recovering temperate grass, greener and lively, close to the source tile"),
    ("summer", "fuller warm high-sun grass with slightly drier yellow-green patches"),
    ("autumn", "muted spent grass with yellowing and brown olive tones, modest leaf litter"),
    ("winter", "patchy snow or frost over visible grass, not a full snow biome tile"),
]


def slug_id(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9]+", "_", text.strip().lower()).strip("_")
    return value or "slot"


def repo_path(path: str | Path) -> Path:
    raw = Path(path).expanduser()
    return raw if raw.is_absolute() else REPO_ROOT / raw


def rel(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO_ROOT.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + "\n", encoding="utf-8")


def parse_slots(raw_slots: list[str]) -> list[dict[str, str]]:
    if not raw_slots:
        return [{"slot": name, "description": description} for name, description in DEFAULT_SEASONAL_SLOTS]

    slots: list[dict[str, str]] = []
    for raw in raw_slots:
        for part in raw.split(","):
            value = part.strip()
            if not value:
                continue
            if "=" in value:
                name, description = value.split("=", 1)
            elif ":" in value:
                name, description = value.split(":", 1)
            else:
                name, description = value, value
            slot = slug_id(name)
            slots.append({"slot": slot, "description": description.strip() or slot})

    names = [item["slot"] for item in slots]
    if len(set(names)) != len(names):
        raise SystemExit("slot names must be unique")
    return slots


def infer_rows(slot_count: int, cols: int, rows: int | None) -> int:
    if cols <= 0:
        raise SystemExit("--cols must be positive")
    if rows is None:
        return math.ceil(slot_count / cols)
    if rows <= 0:
        raise SystemExit("--rows must be positive")
    return rows


def require_grid_capacity(slots: list[dict[str, str]], cols: int, rows: int) -> None:
    total = cols * rows
    if len(slots) != total:
        raise SystemExit(f"expected {total} slots for {cols}x{rows}, got {len(slots)}")


def grid_box(col: int, row: int, tile_size: int, gap: int) -> tuple[int, int, int, int]:
    x0 = col * (tile_size + gap)
    y0 = row * (tile_size + gap)
    return x0, y0, x0 + tile_size, y0 + tile_size


def grid_size(cols: int, rows: int, tile_size: int, gap: int) -> tuple[int, int]:
    return cols * tile_size + max(0, cols - 1) * gap, rows * tile_size + max(0, rows - 1) * gap


def build_grid(source: Path, out: Path, cols: int, rows: int, tile_size: int | None, gap: int) -> tuple[int, int]:
    img = Image.open(source).convert("RGBA")
    if img.width != img.height:
        raise SystemExit(f"{source} is not square: {img.width}x{img.height}")
    size = tile_size or img.width
    if size <= 0:
        raise SystemExit("--tile-size must be positive")
    if img.width != size:
        img = img.resize((size, size), Image.Resampling.LANCZOS)

    width, height = grid_size(cols, rows, size, gap)
    sheet = Image.new("RGBA", (width, height), "#00000000")
    for row in range(rows):
        for col in range(cols):
            x0, y0, _, _ = grid_box(col, row, size, gap)
            sheet.alpha_composite(img, (x0, y0))
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    return size, gap


def slot_lines(slots: list[dict[str, str]], cols: int) -> list[str]:
    lines = []
    for index, item in enumerate(slots):
        row = index // cols + 1
        col = index % cols + 1
        lines.append(f"- row {row}, column {col}: {item['slot']} - {item['description']}")
    return lines


def build_prompt(asset: str, variant_group: str, slots: list[dict[str, str]], cols: int, rows: int) -> str:
    slot_text = "\n".join(slot_lines(slots, cols))
    return f"""
Edit the visible {cols}x{rows} Realm {asset} ground tile grid into {variant_group} variants while preserving the same sheet layout and the exact same physical raised terrain slab shape in every cell.

Asset type: top-down square ground tile contact sheet for a tile-based RTS.
Base/edit target role: the visible grid is authoritative for crop, canvas size, gutters, top-down camera, tile scale, lighting direction, physical slab geometry, subtle bevel, chipped worn side faces, worn corners, and dark contact shadow.
Editable area: change only the terrain state on the top face of each slab. Do not repaint, brighten, outline, recolour, thicken, clean up, or simplify the side faces, corners, contact shadows, or gutters.
Style reference role: preserve the accepted {asset} tile's painterly brush style and muted material-coloured slab sides.

Slot order, left to right and top to bottom:
{slot_text}

Required output: one square {cols}x{rows} sheet with one complete full-square top-down raised ground slab per cell, clear gutters, no labels, no text, no numbers, no watermark, no perspective scene, no horizon, no upright objects, no characters, no buildings, no props, no UI markers. The top terrain surface must remain continuous into the chipped worn side faces; do not add an inner rectangle, decorative surround, outline, rim, trim, or UI-style edging.
""".strip()


def command_prepare(args: argparse.Namespace) -> None:
    slots = parse_slots(args.slot)
    rows = infer_rows(len(slots), args.cols, args.rows)
    require_grid_capacity(slots, args.cols, rows)

    asset = slug_id(args.asset)
    variant_group = slug_id(args.variant_group)
    source = repo_path(args.source or f"assets/tiles/grounds/{asset}.png")
    out = repo_path(args.out or f"build/tileset-grids/{asset}-{variant_group}-template-{args.cols}x{rows}.png")
    prompt_out = repo_path(args.prompt_out or out.with_suffix(".prompt.txt"))
    manifest_out = repo_path(args.manifest_out or out.with_suffix(".manifest.json"))

    tile_size, gap = build_grid(source, out, args.cols, rows, args.tile_size, args.gap)
    prompt = build_prompt(asset, variant_group, slots, args.cols, rows)
    write_text(prompt_out, prompt)

    manifest = {
        "schema": "realm.ground_variant_prepare.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "asset_type": "ground",
        "asset": asset,
        "variant_group": variant_group,
        "source": rel(source),
        "template_sheet": rel(out),
        "prompt": rel(prompt_out),
        "cols": args.cols,
        "rows": rows,
        "tile_size": tile_size,
        "gap": gap,
        "slots": [
            {
                **item,
                "index": index,
                "row": index // args.cols,
                "col": index % args.cols,
                "box": list(grid_box(index % args.cols, index // args.cols, tile_size, gap)),
            }
            for index, item in enumerate(slots)
        ],
        "next_step": "Use image generation to edit the template sheet, then run this script's ingest command on the generated sheet.",
    }
    write_json(manifest_out, manifest)
    print(f"wrote template: {out}")
    print(f"wrote prompt: {prompt_out}")
    print(f"wrote manifest: {manifest_out}")


def clean_version(raw: str | None) -> str:
    if raw:
        return slug_id(raw).replace("_", "-")
    return "v" + datetime.now().strftime("%Y%m%d-%H%M%S") + "-generated-sheet"


def infer_scaled_geometry(
    width: int,
    height: int,
    cols: int,
    rows: int,
    *,
    template_width: int | None = None,
    template_height: int | None = None,
    template_gap: int | None = None,
    explicit_gap: int | None = None,
) -> tuple[int, int, str]:
    if explicit_gap is not None:
        tile_w = (width - max(0, cols - 1) * explicit_gap) / cols
        tile_h = (height - max(0, rows - 1) * explicit_gap) / rows
        if tile_w != int(tile_w) or tile_h != int(tile_h) or int(tile_w) != int(tile_h):
            raise SystemExit(f"sheet does not split cleanly with --gap {explicit_gap}: {width}x{height}")
        return int(tile_w), explicit_gap, "explicit"

    expected_gap = 0
    if template_width and template_height and template_gap is not None:
        scale = min(width / template_width, height / template_height)
        expected_gap = max(0, int(round(template_gap * scale)))

    candidates = list(range(max(0, expected_gap - 16), expected_gap + 17))
    candidates.extend(range(0, min(128, max(width, height)) + 1))
    best: tuple[int, int] | None = None
    for gap in dict.fromkeys(candidates):
        tile_w_num = width - max(0, cols - 1) * gap
        tile_h_num = height - max(0, rows - 1) * gap
        if tile_w_num <= 0 or tile_h_num <= 0:
            continue
        if tile_w_num % cols != 0 or tile_h_num % rows != 0:
            continue
        tile_w = tile_w_num // cols
        tile_h = tile_h_num // rows
        if tile_w != tile_h:
            continue
        if best is None or abs(gap - expected_gap) < abs(best[1] - expected_gap):
            best = (tile_w, gap)
    if best is None:
        raise SystemExit(f"could not infer square grid geometry for {width}x{height} as {cols}x{rows}; pass --gap")
    reason = "scaled-template" if template_gap is not None else "inferred"
    return best[0], best[1], reason


def split_sheet(sheet: Image.Image, out_dir: Path, slots: list[dict[str, str]], cols: int, tile_size: int, gap: int, force: bool) -> dict[str, str]:
    out_dir.mkdir(parents=True, exist_ok=True)
    outputs: dict[str, str] = {}
    for index, item in enumerate(slots):
        slot = item["slot"]
        row = index // cols
        col = index % cols
        tile = sheet.crop(grid_box(col, row, tile_size, gap))
        out = out_dir / f"{slot}.png"
        if out.exists() and not force:
            raise SystemExit(f"{out} exists; pass --force to overwrite")
        tile.save(out)
        outputs[slot] = rel(out)
    return outputs


def edge_restore_mask(size: tuple[int, int], band: int, feather: float) -> Image.Image:
    width, height = size
    mask = Image.new("L", size, 255)
    draw = ImageDraw.Draw(mask)
    draw.rectangle((band, band, width - band - 1, height - band - 1), fill=0)
    if feather > 0:
        mask = mask.filter(ImageFilter.GaussianBlur(radius=feather))
    return mask


def restore_source_edges(source: Path, split_paths: dict[str, str], edge_fraction: float, feather_fraction: float) -> dict[str, Any]:
    reference = Image.open(source).convert("RGBA")
    restored_tiles = []
    for slot, path_text in split_paths.items():
        path = repo_path(path_text)
        tile = Image.open(path).convert("RGBA")
        ref = reference.resize(tile.size, Image.Resampling.LANCZOS)
        band = max(8, min(96, int(round(min(tile.size) * edge_fraction))))
        feather = max(0.0, min(32.0, min(tile.size) * feather_fraction))
        mask = edge_restore_mask(tile.size, band, feather)
        Image.composite(ref, tile, mask).save(path)
        restored_tiles.append(
            {
                "slot": slot,
                "path": rel(path),
                "edge_band_px": band,
                "feather_px": round(feather, 2),
            }
        )
    return {
        "schema": "realm.ground_variant_edge_restore.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "source": rel(source),
        "tiles": restored_tiles,
        "notes": [
            "Generated top material was kept in the interior; source perimeter was restored to preserve the approved slab side style.",
            "Use visual review to confirm the blend line stays inside the natural bevel/top transition.",
        ],
    }


def luminance(rgb: tuple[float, float, float]) -> float:
    return rgb[0] * 0.2126 + rgb[1] * 0.7152 + rgb[2] * 0.0722


def tile_metrics(tile: Image.Image, edge_fraction: float) -> dict[str, Any]:
    rgba = tile.convert("RGBA")
    width, height = rgba.size
    band = max(8, min(64, int(round(min(width, height) * edge_fraction))))
    center_margin = max(band * 2, int(round(min(width, height) * 0.22)))
    center_box = (
        center_margin,
        center_margin,
        max(center_margin + 1, width - center_margin),
        max(center_margin + 1, height - center_margin),
    )
    edge_regions = [
        rgba.crop((0, 0, width, band)),
        rgba.crop((0, height - band, width, height)),
        rgba.crop((0, 0, band, height)),
        rgba.crop((width - band, 0, width, height)),
    ]
    edge_mean_rgb = [0.0, 0.0, 0.0]
    for region in edge_regions:
        mean = ImageStat.Stat(region.convert("RGB")).mean
        for channel in range(3):
            edge_mean_rgb[channel] += mean[channel] / len(edge_regions)
    center_mean_rgb = ImageStat.Stat(rgba.crop(center_box).convert("RGB")).mean[:3]
    edge_luma = luminance(tuple(edge_mean_rgb))  # type: ignore[arg-type]
    center_luma = luminance(tuple(center_mean_rgb))  # type: ignore[arg-type]
    alpha = ImageStat.Stat(rgba.getchannel("A")).mean[0]
    return {
        "size": [width, height],
        "edge_band_px": band,
        "edge_luminance": round(edge_luma, 2),
        "center_luminance": round(center_luma, 2),
        "edge_center_luminance_delta": round(abs(edge_luma - center_luma), 2),
        "mean_alpha": round(alpha, 2),
    }


def inspect_split_tiles(
    split_paths: dict[str, str],
    tile_size: int,
    edge_fraction: float,
    min_edge_delta: float,
    min_alpha: float,
) -> dict[str, Any]:
    tiles = []
    failures = []
    for slot, path_text in split_paths.items():
        path = repo_path(path_text)
        metrics = tile_metrics(Image.open(path), edge_fraction)
        reasons: list[str] = []
        if metrics["size"] != [tile_size, tile_size]:
            reasons.append("not expected square size")
        if metrics["mean_alpha"] < min_alpha:
            reasons.append("unexpected transparency")
        if metrics["edge_center_luminance_delta"] < min_edge_delta:
            reasons.append("weak raised-edge contrast")
        item = {"slot": slot, "path": rel(path), "ok": not reasons, "reasons": reasons, "metrics": metrics}
        tiles.append(item)
        if reasons:
            failures.append(item)
    return {
        "schema": "realm.ground_variant_inspection.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "ok": not failures,
        "failures": failures,
        "tiles": tiles,
        "notes": [
            "Mechanical QA only. Visually inspect the sheet and each crop before accepting.",
            "A weak edge contrast warning can mean the raised terrain slab shape was softened or lost.",
        ],
    }


def edge_bands(img: Image.Image, band: int) -> list[Image.Image]:
    rgba = img.convert("RGBA")
    width, height = rgba.size
    return [
        rgba.crop((0, 0, width, band)),
        rgba.crop((0, height - band, width, height)),
        rgba.crop((0, 0, band, height)),
        rgba.crop((width - band, 0, width, height)),
    ]


def edge_mae(reference: Image.Image, tile: Image.Image, edge_fraction: float) -> float:
    tile_rgba = tile.convert("RGBA")
    ref = reference.convert("RGBA").resize(tile_rgba.size, Image.Resampling.LANCZOS)
    band = max(8, min(64, int(round(min(tile_rgba.size) * edge_fraction))))
    total = 0.0
    for ref_band, tile_band in zip(edge_bands(ref, band), edge_bands(tile_rgba, band)):
        diff = ImageChops.difference(ref_band.convert("RGB"), tile_band.convert("RGB"))
        total += sum(ImageStat.Stat(diff).mean) / 3.0
    return round(total / 4.0, 2)


def edge_qa(source: Path, split_paths: dict[str, str], edge_fraction: float, warn_mae: float) -> dict[str, Any]:
    reference = Image.open(source)
    tiles = []
    warnings = []
    for slot, path_text in split_paths.items():
        path = repo_path(path_text)
        tile = Image.open(path)
        mae = edge_mae(reference, tile, edge_fraction)
        item = {
            "slot": slot,
            "path": rel(path),
            "edge_rgb_mean_absolute_error": mae,
            "ok": mae <= warn_mae,
            "reasons": [] if mae <= warn_mae else ["edge differs substantially from source slab"],
        }
        tiles.append(item)
        if not item["ok"]:
            warnings.append(item)
    return {
        "schema": "realm.ground_variant_edge_qa.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "source": rel(source),
        "ok": not warnings,
        "warn_edge_mae": warn_mae,
        "warnings": warnings,
        "tiles": tiles,
        "notes": [
            "This compares source and candidate edge regions after resizing the source to each crop.",
            "Seasonal color changes can legitimately affect this score; use it as a drift warning, not as acceptance.",
        ],
    }


def load_prepare_manifest(path: str | None) -> dict[str, Any] | None:
    return read_json(repo_path(path)) if path else None


def append_note(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    existing = path.read_text(encoding="utf-8") if path.exists() else ""
    path.write_text(existing.rstrip() + "\n\n" + text.rstrip() + "\n", encoding="utf-8")


def command_ingest(args: argparse.Namespace) -> None:
    prepared = load_prepare_manifest(args.prepare_manifest)
    asset = slug_id(args.asset or (prepared or {}).get("asset", "ground"))
    variant_group = slug_id(args.variant_group or (prepared or {}).get("variant_group", "variants"))
    cols = int(args.cols or (prepared or {}).get("cols", 2))
    slots = parse_slots(args.slot) if args.slot else [
        {"slot": str(item["slot"]), "description": str(item.get("description") or item["slot"])}
        for item in (prepared or {}).get("slots", [])
    ]
    if not slots:
        slots = parse_slots([])
    rows = infer_rows(len(slots), cols, args.rows or (prepared or {}).get("rows"))
    require_grid_capacity(slots, cols, rows)

    version = clean_version(args.version)
    candidate_dir = repo_path(args.out_dir or Path(args.candidate_root) / asset / variant_group / version)
    if candidate_dir.exists() and not args.force:
        raise SystemExit(f"{candidate_dir} exists; pass --force to overwrite files in it")
    candidate_dir.mkdir(parents=True, exist_ok=True)

    source = repo_path(args.source or (prepared or {}).get("source") or f"assets/tiles/grounds/{asset}.png")
    generated_sheet = repo_path(args.sheet)
    stored_sheet = candidate_dir / "batch_source.png"
    original_sheet = None
    sheet_img = Image.open(generated_sheet).convert("RGBA")
    template_width = None
    template_height = None
    if prepared:
        template_width, template_height = grid_size(cols, rows, int(prepared["tile_size"]), int(prepared["gap"]))

    if args.resize_mode == "template":
        if not template_width or not template_height:
            raise SystemExit("--resize-mode template requires --prepare-manifest")
        original_sheet = candidate_dir / "batch_source.original.png"
        shutil.copy2(generated_sheet, original_sheet)
        sheet_img = sheet_img.resize((template_width, template_height), Image.Resampling.LANCZOS)
        sheet_img.save(stored_sheet)
    else:
        shutil.copy2(generated_sheet, stored_sheet)

    tile_size, gap, geometry_source = infer_scaled_geometry(
        sheet_img.width,
        sheet_img.height,
        cols,
        rows,
        template_width=template_width,
        template_height=template_height,
        template_gap=int(prepared["gap"]) if prepared else None,
        explicit_gap=args.gap,
    )

    split_paths = split_sheet(sheet_img, candidate_dir / "split", slots, cols, tile_size, gap, args.force)
    edge_restore_report_path = None
    edge_restore_report = None
    if args.restore_source_edge and source.exists():
        edge_restore_report = restore_source_edges(source, split_paths, args.restore_edge_fraction, args.restore_edge_feather_fraction)
        edge_restore_report_path = candidate_dir / "edge_restore.json"
        write_json(edge_restore_report_path, edge_restore_report)

    effective_min_edge_delta = 0.0 if edge_restore_report else args.min_edge_delta
    inspection = inspect_split_tiles(split_paths, tile_size, args.edge_fraction, effective_min_edge_delta, args.min_alpha)
    inspection_path = candidate_dir / "grid_inspection.json"
    write_json(inspection_path, inspection)

    edge_report_path = None
    edge_report = None
    if source.exists():
        edge_report = edge_qa(source, split_paths, args.edge_fraction, args.warn_edge_mae)
        edge_report_path = candidate_dir / "edge_qa.json"
        write_json(edge_report_path, edge_report)

    if prepared:
        write_json(candidate_dir / "prepare_manifest.json", prepared)
        prompt_value = prepared.get("prompt")
        prompt_path = repo_path(prompt_value) if isinstance(prompt_value, str) and prompt_value.strip() else None
        if prompt_path and prompt_path.is_file():
            shutil.copy2(prompt_path, candidate_dir / "prompt.txt")

    manifest = {
        "schema": "realm.ground_candidate.v1",
        "asset_type": "ground",
        "slug": asset,
        "variant_group": variant_group,
        "version": version,
        "status": "candidate",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "source_reference": rel(source),
        "generator_output": rel(generated_sheet),
        "candidate_sheet": rel(stored_sheet),
        "original_candidate_sheet": rel(original_sheet) if original_sheet else None,
        "split_tiles": split_paths,
        "grid": {
            "cols": cols,
            "rows": rows,
            "sheet_size": [sheet_img.width, sheet_img.height],
            "split_tile_size": tile_size,
            "split_gap": gap,
            "geometry_source": geometry_source,
            "resize_mode": args.resize_mode,
            "source_edge_restored": bool(edge_restore_report),
            "min_edge_delta_used": effective_min_edge_delta,
        },
        "qa": {
            "mechanical_inspection": rel(inspection_path),
            "mechanical_ok": inspection["ok"],
            "edge_restore": rel(edge_restore_report_path) if edge_restore_report_path else None,
            "edge_qa": rel(edge_report_path) if edge_report_path else None,
            "edge_qa_ok": edge_report["ok"] if edge_report else None,
            "visual_review": "pending",
        },
    }
    write_json(candidate_dir / "candidate_manifest.json", manifest)

    note_text = f"""
# {asset.title()} {variant_group.title()} Candidate {version}

Generated from `{rel(source)}`.

## Result

- `batch_source.png`: stored generated sheet.
- `split/`: standalone square crops for {", ".join(item["slot"] for item in slots)}.
- `edge_restore.json`: source slab-edge restore {"was applied" if edge_restore_report else "was not applied"}.
- `grid_inspection.json`: mechanical QA {"passed" if inspection["ok"] else "has warnings"}.
- `edge_qa.json`: source-edge drift QA {"passed" if (edge_report and edge_report["ok"]) else "has warnings" if edge_report else "was not run"}.

## Follow-Up

- Visually inspect the sheet and each split tile before accepting any slot.
- Split geometry used `tile_size={tile_size}` and `gap={gap}` from `{geometry_source}` geometry.
- These are candidates only. Runtime seasonal or variant selection is not wired by this helper.
"""
    write_text(candidate_dir / "notes.md", note_text)

    if args.append_ground_notes:
        notes_file = repo_path(args.ground_notes_file)
        append_note(
            notes_file,
            f"- {datetime.now().date()}: {asset} {variant_group} {version} ingested as candidate at `{rel(candidate_dir)}`; "
            f"mechanical_ok={inspection['ok']}; edge_qa_ok={edge_report['ok'] if edge_report else 'not_run'}; "
            f"tile_size={tile_size}; gap={gap}.",
        )

    print(f"stored candidate: {candidate_dir}")
    print(f"wrote manifest: {candidate_dir / 'candidate_manifest.json'}")
    print(f"wrote inspection: {inspection_path}")
    if edge_report_path:
        print(f"wrote edge QA: {edge_report_path}")


def command_edge_qa(args: argparse.Namespace) -> None:
    source = repo_path(args.source)
    split_dir = repo_path(args.split_dir)
    if args.slot:
        slots = [slug_id(part) for raw in args.slot for part in raw.split(",") if part.strip()]
    else:
        slots = [path.stem for path in sorted(split_dir.glob("*.png")) if path.name != "batch_source.png"]
    if not slots:
        raise SystemExit(f"no PNG tiles found in {split_dir}; pass --slot")
    split_paths = {slot: rel(split_dir / f"{slot}.png") for slot in slots}
    missing = [path for path in split_paths.values() if not repo_path(path).exists()]
    if missing:
        raise SystemExit("missing split tiles:\n" + "\n".join(missing))
    report = edge_qa(source, split_paths, args.edge_fraction, args.warn_edge_mae)
    out = repo_path(args.out or split_dir.parent / "edge_qa.json")
    write_json(out, report)
    print(f"wrote edge QA: {out}")
    print("ok" if report["ok"] else f"warnings: {', '.join(item['slot'] for item in report['warnings'])}")
    if args.fail and not report["ok"]:
        raise SystemExit(1)


def command_promote_reviewed(args: argparse.Namespace) -> None:
    candidate_dir = repo_path(args.candidate_dir)
    manifest_path = candidate_dir / "candidate_manifest.json"
    manifest = read_json(manifest_path) if manifest_path.exists() else {}
    asset = slug_id(args.asset or manifest.get("slug") or candidate_dir.parents[1].name)

    if args.source:
        source = repo_path(args.source)
    elif args.slot:
        split_tiles = manifest.get("split_tiles", {})
        if args.slot not in split_tiles:
            raise SystemExit(f"slot {args.slot!r} is not listed in {manifest_path}")
        source = repo_path(split_tiles[args.slot])
    elif (candidate_dir / "source.png").exists():
        source = candidate_dir / "source.png"
    else:
        raise SystemExit("pass --source or --slot, or use a candidate folder with source.png")

    if not source.exists():
        raise SystemExit(f"source image does not exist: {source}")

    out = repo_path(args.out or f"assets/tiles/grounds/{asset}.png")
    assets_root = (REPO_ROOT / "assets" / "tiles").resolve()
    if not out.resolve().is_relative_to(assets_root):
        raise SystemExit(f"runtime output must stay under {assets_root}: {out}")

    out.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, out)

    promotion = {
        "promoted_at": datetime.now().isoformat(timespec="seconds"),
        "promoted_source": rel(source),
        "runtime_target": rel(out),
        "review": args.review,
    }
    manifest["status"] = args.status
    manifest["runtime_target"] = rel(out)
    manifest["promotion"] = promotion
    write_json(manifest_path, manifest)

    print(f"promoted reviewed candidate: {source}")
    print(f"runtime target: {out}")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="create an editable grid and prompt for ground variants")
    prepare.add_argument("--asset", default="grass")
    prepare.add_argument("--variant-group", default="seasonal")
    prepare.add_argument("--source", help="accepted source tile; defaults to assets/tiles/grounds/<asset>.png")
    prepare.add_argument("--out")
    prepare.add_argument("--prompt-out")
    prepare.add_argument("--manifest-out")
    prepare.add_argument("--cols", type=int, default=2)
    prepare.add_argument("--rows", type=int)
    prepare.add_argument("--tile-size", type=int)
    prepare.add_argument("--gap", type=int, default=48)
    prepare.add_argument("--slot", action="append", default=[], help="slot or slot=description; may be comma-separated")
    prepare.set_defaults(func=command_prepare)

    ingest = sub.add_parser("ingest", help="store, split, and QA a generated ground variant sheet")
    ingest.add_argument("--sheet", required=True, help="generated sheet to ingest")
    ingest.add_argument("--prepare-manifest", help="manifest written by prepare")
    ingest.add_argument("--asset")
    ingest.add_argument("--variant-group")
    ingest.add_argument("--source")
    ingest.add_argument("--candidate-root", default="art/tiles/candidates/grounds")
    ingest.add_argument("--out-dir")
    ingest.add_argument("--version")
    ingest.add_argument("--cols", type=int)
    ingest.add_argument("--rows", type=int)
    ingest.add_argument("--gap", type=int)
    ingest.add_argument("--slot", action="append", default=[], help="slot or slot=description; may be comma-separated")
    ingest.add_argument("--resize-mode", choices=["keep", "template"], default="keep")
    ingest.add_argument("--edge-fraction", type=float, default=0.05)
    ingest.add_argument("--min-edge-delta", type=float, default=7.0)
    ingest.add_argument("--min-alpha", type=float, default=250.0)
    ingest.add_argument("--warn-edge-mae", type=float, default=55.0)
    ingest.add_argument("--restore-source-edge", action=argparse.BooleanOptionalAction, default=True)
    ingest.add_argument("--restore-edge-fraction", type=float, default=0.075)
    ingest.add_argument("--restore-edge-feather-fraction", type=float, default=0.012)
    ingest.add_argument("--append-ground-notes", action="store_true")
    ingest.add_argument("--ground-notes-file", default="art/tiles/reference/grounds/notes/ground-generation-notes.md")
    ingest.add_argument("--force", action="store_true")
    ingest.set_defaults(func=command_ingest)

    for command_name, help_text in [
        ("edge-qa", "compare split tile edge regions to an accepted source tile"),
        ("border-qa", "compatibility alias for edge-qa"),
    ]:
        qa = sub.add_parser(command_name, help=help_text)
        qa.add_argument("--source", required=True)
        qa.add_argument("--split-dir", required=True)
        qa.add_argument("--slot", action="append", default=[], help="slot name, or comma-separated slot names")
        qa.add_argument("--edge-fraction", type=float, default=0.05)
        qa.add_argument("--warn-edge-mae", type=float, default=55.0)
        qa.add_argument("--out")
        qa.add_argument("--fail", action="store_true")
        qa.set_defaults(func=command_edge_qa)

    promote = sub.add_parser("promote-reviewed", help="copy a reviewed ground candidate into assets/tiles so the game can load it")
    promote.add_argument("--candidate-dir", required=True)
    promote.add_argument("--asset", help="ground asset slug; defaults to manifest slug")
    promote.add_argument("--slot", help="split tile slot to promote, for generated sheets")
    promote.add_argument("--source", help="explicit source image to promote")
    promote.add_argument("--out", help="runtime output path; defaults to assets/tiles/grounds/<asset>.png")
    promote.add_argument("--status", default="accepted_runtime_trial")
    promote.add_argument("--review", default="reviewed and accepted for runtime trial")
    promote.set_defaults(func=command_promote_reviewed)
    return parser


def main() -> None:
    args = build_parser().parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
