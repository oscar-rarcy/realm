#!/usr/bin/env python3
"""Build, split, and inspect square tile grids for Realm art generation."""

from __future__ import annotations

import argparse
import json
import re
from datetime import datetime
from pathlib import Path
from typing import Any

from PIL import Image, ImageStat


def slug_id(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9]+", "_", text.strip().lower()).strip("_")
    return value or "tile"


def resolve_path(path: str) -> Path:
    return Path(path).expanduser()


def ensure_square(img: Image.Image, source: Path) -> None:
    if img.width != img.height:
        raise SystemExit(f"{source} is not square: {img.width}x{img.height}")


def parse_slots(raw_slots: list[str], cols: int, rows: int) -> list[str]:
    total = cols * rows
    if not raw_slots:
        return [f"r{row + 1}c{col + 1}" for row in range(rows) for col in range(cols)]
    slots: list[str] = []
    for raw in raw_slots:
        for part in raw.split(","):
            slot = slug_id(part)
            if slot:
                slots.append(slot)
    if len(slots) != total:
        raise SystemExit(f"expected {total} slot names for {cols}x{rows}, got {len(slots)}")
    if len(set(slots)) != len(slots):
        raise SystemExit("slot names must be unique")
    return slots


def grid_box(col: int, row: int, tile_size: int, gap: int) -> tuple[int, int, int, int]:
    x0 = col * (tile_size + gap)
    y0 = row * (tile_size + gap)
    return x0, y0, x0 + tile_size, y0 + tile_size


def write_manifest(
    out: Path,
    *,
    command: str,
    source: Path | None,
    sheet: Path,
    cols: int,
    rows: int,
    tile_size: int,
    gap: int,
    slots: list[str],
    extra: dict[str, Any] | None = None,
) -> None:
    items = []
    for index, name in enumerate(slots):
        row = index // cols
        col = index % cols
        items.append(
            {
                "slot": name,
                "index": index,
                "row": row,
                "col": col,
                "box": list(grid_box(col, row, tile_size, gap)),
            }
        )
    manifest: dict[str, Any] = {
        "schema": "realm.tile_grid.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "command": command,
        "source": source.as_posix() if source else None,
        "sheet": sheet.as_posix(),
        "cols": cols,
        "rows": rows,
        "tile_size": tile_size,
        "gap": gap,
        "slots": items,
    }
    if extra:
        manifest.update(extra)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")


def command_make(args: argparse.Namespace) -> None:
    source = resolve_path(args.source)
    out = resolve_path(args.out)
    img = Image.open(source).convert("RGBA")
    ensure_square(img, source)

    tile_size = args.tile_size or img.width
    if tile_size <= 0:
        raise SystemExit("--tile-size must be positive")
    if img.width != tile_size:
        img = img.resize((tile_size, tile_size), Image.Resampling.LANCZOS)

    slots = parse_slots(args.slot, args.cols, args.rows)
    width = args.cols * tile_size + max(0, args.cols - 1) * args.gap
    height = args.rows * tile_size + max(0, args.rows - 1) * args.gap
    sheet = Image.new("RGBA", (width, height), args.gap_color)
    for row in range(args.rows):
        for col in range(args.cols):
            x0, y0, _, _ = grid_box(col, row, tile_size, args.gap)
            sheet.alpha_composite(img, (x0, y0))

    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out)
    manifest_out = resolve_path(args.manifest_out) if args.manifest_out else out.with_suffix(".manifest.json")
    write_manifest(
        manifest_out,
        command="make-grid",
        source=source,
        sheet=out,
        cols=args.cols,
        rows=args.rows,
        tile_size=tile_size,
        gap=args.gap,
        slots=slots,
    )
    print(f"wrote grid: {out}")
    print(f"wrote manifest: {manifest_out}")


def load_grid_image(path: Path) -> Image.Image:
    img = Image.open(path).convert("RGBA")
    if img.width <= 0 or img.height <= 0:
        raise SystemExit(f"{path} has invalid dimensions")
    return img


def inferred_tile_size(width: int, height: int, cols: int, rows: int, gap: int) -> int:
    cell_w = (width - max(0, cols - 1) * gap) / cols
    cell_h = (height - max(0, rows - 1) * gap) / rows
    if abs(cell_w - round(cell_w)) > 0.01 or abs(cell_h - round(cell_h)) > 0.01:
        raise SystemExit(f"grid dimensions do not divide cleanly into {cols}x{rows} with gap {gap}: {width}x{height}")
    if int(round(cell_w)) != int(round(cell_h)):
        raise SystemExit(f"grid cells are not square: {cell_w}x{cell_h}")
    return int(round(cell_w))


def validate_grid_dimensions(width: int, height: int, cols: int, rows: int, tile_size: int, gap: int) -> None:
    expected_width = cols * tile_size + max(0, cols - 1) * gap
    expected_height = rows * tile_size + max(0, rows - 1) * gap
    if width != expected_width or height != expected_height:
        raise SystemExit(
            f"grid dimensions {width}x{height} do not match {cols}x{rows} "
            f"with tile size {tile_size} and gap {gap}: expected {expected_width}x{expected_height}"
        )


def command_split(args: argparse.Namespace) -> None:
    sheet_path = resolve_path(args.sheet)
    out_dir = resolve_path(args.out_dir)
    sheet = load_grid_image(sheet_path)
    tile_size = args.tile_size or inferred_tile_size(sheet.width, sheet.height, args.cols, args.rows, args.gap)
    validate_grid_dimensions(sheet.width, sheet.height, args.cols, args.rows, tile_size, args.gap)
    slots = parse_slots(args.slot, args.cols, args.rows)
    out_dir.mkdir(parents=True, exist_ok=True)

    written: list[dict[str, Any]] = []
    for index, name in enumerate(slots):
        row = index // args.cols
        col = index % args.cols
        box = grid_box(col, row, tile_size, args.gap)
        tile = sheet.crop(box)
        out = out_dir / f"{name}.png"
        if out.exists() and not args.force:
            raise SystemExit(f"{out} exists; pass --force to overwrite")
        tile.save(out)
        written.append({"slot": name, "path": out.as_posix(), "box": list(box)})

    manifest_out = resolve_path(args.manifest_out) if args.manifest_out else out_dir / "split_manifest.json"
    write_manifest(
        manifest_out,
        command="split-grid",
        source=None,
        sheet=sheet_path,
        cols=args.cols,
        rows=args.rows,
        tile_size=tile_size,
        gap=args.gap,
        slots=slots,
        extra={"outputs": written},
    )
    print(f"wrote {len(written)} tiles: {out_dir}")
    print(f"wrote manifest: {manifest_out}")


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
        for i in range(3):
            edge_mean_rgb[i] += mean[i] / len(edge_regions)
    center_mean_rgb = ImageStat.Stat(rgba.crop(center_box).convert("RGB")).mean
    edge_luma = luminance(tuple(edge_mean_rgb))  # type: ignore[arg-type]
    center_luma = luminance(tuple(center_mean_rgb[:3]))  # type: ignore[arg-type]
    alpha = ImageStat.Stat(rgba.getchannel("A")).mean[0]
    return {
        "size": [width, height],
        "edge_band_px": band,
        "edge_luminance": round(edge_luma, 2),
        "center_luminance": round(center_luma, 2),
        "edge_center_luminance_delta": round(abs(edge_luma - center_luma), 2),
        "mean_alpha": round(alpha, 2),
    }


def command_inspect(args: argparse.Namespace) -> None:
    sheet_path = resolve_path(args.sheet)
    sheet = load_grid_image(sheet_path)
    tile_size = args.tile_size or inferred_tile_size(sheet.width, sheet.height, args.cols, args.rows, args.gap)
    validate_grid_dimensions(sheet.width, sheet.height, args.cols, args.rows, tile_size, args.gap)
    slots = parse_slots(args.slot, args.cols, args.rows)
    results = []
    failures = []
    for index, name in enumerate(slots):
        row = index // args.cols
        col = index % args.cols
        box = grid_box(col, row, tile_size, args.gap)
        tile = sheet.crop(box)
        metrics = tile_metrics(tile, args.edge_fraction)
        ok = True
        reasons: list[str] = []
        if metrics["size"] != [tile_size, tile_size]:
            ok = False
            reasons.append("not expected square size")
        if metrics["mean_alpha"] < args.min_alpha:
            ok = False
            reasons.append("unexpected transparency")
        if metrics["edge_center_luminance_delta"] < args.min_edge_delta:
            ok = False
            reasons.append("weak raised-edge contrast")
        item = {
            "slot": name,
            "index": index,
            "row": row,
            "col": col,
            "box": list(box),
            "ok": ok,
            "reasons": reasons,
            "metrics": metrics,
        }
        results.append(item)
        if not ok:
            failures.append(item)

    report = {
        "schema": "realm.tile_grid_inspection.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "sheet": sheet_path.as_posix(),
        "cols": args.cols,
        "rows": args.rows,
        "tile_size": tile_size,
        "gap": args.gap,
        "ok": not failures,
        "failures": failures,
        "tiles": results,
        "notes": [
            "This is mechanical QA only. Codex must still visually inspect tiles for the requested season/material.",
            "A low edge delta is a warning that the generated sheet may have lost the Realm raised tile frame.",
        ],
    }
    out = resolve_path(args.out) if args.out else sheet_path.with_suffix(".inspect.json")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"wrote inspection: {out}")
    print("ok" if report["ok"] else f"failed slots: {', '.join(item['slot'] for item in failures)}")
    if failures and args.fail:
        raise SystemExit(1)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    make = sub.add_parser("make-grid", help="repeat one square tile into an editable grid sheet")
    make.add_argument("--source", required=True)
    make.add_argument("--out", required=True)
    make.add_argument("--cols", type=int, required=True)
    make.add_argument("--rows", type=int, required=True)
    make.add_argument("--tile-size", type=int, help="resize source tile before building the grid")
    make.add_argument("--gap", type=int, default=0)
    make.add_argument("--gap-color", default="#00000000")
    make.add_argument("--slot", action="append", default=[], help="slot name, or comma-separated slot names in grid order")
    make.add_argument("--manifest-out")
    make.set_defaults(func=command_make)

    split = sub.add_parser("split-grid", help="split an edited grid sheet into standalone square tiles")
    split.add_argument("--sheet", required=True)
    split.add_argument("--out-dir", required=True)
    split.add_argument("--cols", type=int, required=True)
    split.add_argument("--rows", type=int, required=True)
    split.add_argument("--tile-size", type=int)
    split.add_argument("--gap", type=int, default=0)
    split.add_argument("--slot", action="append", default=[], help="slot name, or comma-separated slot names in grid order")
    split.add_argument("--manifest-out")
    split.add_argument("--force", action="store_true")
    split.set_defaults(func=command_split)

    inspect = sub.add_parser("inspect-grid", help="mechanically inspect a square tile grid")
    inspect.add_argument("--sheet", required=True)
    inspect.add_argument("--cols", type=int, required=True)
    inspect.add_argument("--rows", type=int, required=True)
    inspect.add_argument("--tile-size", type=int)
    inspect.add_argument("--gap", type=int, default=0)
    inspect.add_argument("--slot", action="append", default=[], help="slot name, or comma-separated slot names in grid order")
    inspect.add_argument("--edge-fraction", type=float, default=0.05)
    inspect.add_argument("--min-edge-delta", type=float, default=7.0)
    inspect.add_argument("--min-alpha", type=float, default=250.0)
    inspect.add_argument("--out")
    inspect.add_argument("--fail", action="store_true")
    inspect.set_defaults(func=command_inspect)
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    if args.cols <= 0 or args.rows <= 0:
        raise SystemExit("--cols and --rows must be positive")
    if getattr(args, "gap", 0) < 0:
        raise SystemExit("--gap must be non-negative")
    args.func(args)


if __name__ == "__main__":
    main()
