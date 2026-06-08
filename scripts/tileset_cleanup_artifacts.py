#!/usr/bin/env python3
"""Detect or remove detached grid, crop-box, and magenta artifacts from tiles."""

from __future__ import annotations

import argparse
import json
import subprocess
from collections import deque
from pathlib import Path
from typing import Any

from PIL import Image


def batch_paths(path: Path | None) -> list[str]:
    if not path:
        return []
    data = json.loads(path.read_text(encoding="utf-8"))
    items = data.get("batch", data if isinstance(data, list) else [])
    out = []
    for item in items:
        if not isinstance(item, dict):
            continue
        for rel in item.get("required_paths", []):
            if str(rel).endswith("_base.png") or str(rel).endswith(".png"):
                out.append(str(rel))
                break
    return out


def components(img: Image.Image) -> list[list[tuple[int, int]]]:
    rgba = img.convert("RGBA")
    pix = rgba.load()
    w, h = rgba.size
    visible = {(x, y) for y in range(h) for x in range(w) if pix[x, y][3] > 0}
    out: list[list[tuple[int, int]]] = []
    while visible:
        start = visible.pop()
        q = deque([start])
        comp = [start]
        while q:
            x, y = q.popleft()
            for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                if (nx, ny) in visible:
                    visible.remove((nx, ny))
                    q.append((nx, ny))
                    comp.append((nx, ny))
        out.append(comp)
    out.sort(key=len, reverse=True)
    return out


def classify_component(comp: list[tuple[int, int]], pix: Any, w: int, h: int) -> str | None:
    xs = [x for x, _ in comp]
    ys = [y for _, y in comp]
    x0, y0, x1, y1 = min(xs), min(ys), max(xs), max(ys)
    bw, bh = x1 - x0 + 1, y1 - y0 + 1
    n = len(comp)
    density = n / float(bw * bh)
    line_like = (bw >= max(8, int(w * 0.22)) and bh <= 3) or (bh >= max(8, int(h * 0.22)) and bw <= 3)
    sparse_rect = bw >= max(10, int(w * 0.3)) and bh >= max(10, int(h * 0.3)) and density < 0.18
    edge_speck = n <= 3 and (x0 <= 1 or y0 <= 1 or x1 >= w - 2 or y1 >= h - 2)
    magenta = 0
    pale = 0
    for x, y in comp:
        r, g, b, a = pix[x, y]
        if r > 200 and b > 120 and g < 140:
            magenta += 1
        greyish = max(r, g, b) - min(r, g, b) <= 35
        if greyish and r >= 170 and g >= 150 and b >= 130:
            pale += 1
    suspicious_colour = (magenta + pale) >= max(2, n // 3)
    if line_like and suspicious_colour:
        return "detached_line"
    if sparse_rect and suspicious_colour:
        return "sparse_crop_rectangle"
    if edge_speck and (magenta or pale):
        return "edge_speck"
    if magenta >= max(2, n // 2):
        return "magenta_component"
    return None


def is_key_magenta(r: int, g: int, b: int, a: int) -> bool:
    return a > 0 and r >= 185 and g <= 110 and b >= 145 and (r - g) >= 80 and (b - g) >= 60


def is_pale_grid_pixel(r: int, g: int, b: int, a: int) -> bool:
    if a == 0:
        return False
    greyish = max(r, g, b) - min(r, g, b) <= 45
    warm_white = r >= 165 and g >= 145 and b >= 120
    return greyish and warm_white


def remove_key_magenta(img: Image.Image, dry_run: bool) -> int:
    pix = img.load()
    w, h = img.size
    removed = 0
    for y in range(h):
        for x in range(w):
            r, g, b, a = pix[x, y]
            if is_key_magenta(r, g, b, a):
                removed += 1
                if not dry_run:
                    pix[x, y] = (0, 0, 0, 0)
    return removed


def remove_pale_edge_lines(img: Image.Image, dry_run: bool) -> list[dict[str, Any]]:
    pix = img.load()
    w, h = img.size
    findings: list[dict[str, Any]] = []
    row_threshold = max(12, int(w * 0.35))
    col_threshold = max(12, int(h * 0.35))
    candidate_rows = []
    candidate_cols = []
    for y in range(h):
        xs = [x for x in range(w) if is_pale_grid_pixel(*pix[x, y])]
        if len(xs) >= row_threshold and (y <= 4 or y >= h - 5 or max(xs) - min(xs) + 1 >= row_threshold):
            candidate_rows.append((y, min(xs), max(xs), len(xs)))
    for x in range(w):
        ys = [y for y in range(h) if is_pale_grid_pixel(*pix[x, y])]
        if len(ys) >= col_threshold and (x <= 4 or x >= w - 5 or max(ys) - min(ys) + 1 >= col_threshold):
            candidate_cols.append((x, min(ys), max(ys), len(ys)))
    for y, x0, x1, n in candidate_rows:
        removed = 0
        for x in range(x0, x1 + 1):
            if is_pale_grid_pixel(*pix[x, y]):
                removed += 1
                if not dry_run:
                    pix[x, y] = (0, 0, 0, 0)
        if removed:
            findings.append({"kind": "pale_edge_row", "pixels": removed, "bbox": [x0, y, x1, y]})
    for x, y0, y1, n in candidate_cols:
        removed = 0
        for y in range(y0, y1 + 1):
            if is_pale_grid_pixel(*pix[x, y]):
                removed += 1
                if not dry_run:
                    pix[x, y] = (0, 0, 0, 0)
        if removed:
            findings.append({"kind": "pale_edge_col", "pixels": removed, "bbox": [x, y0, x, y1]})
    return findings


def clean_path(path: Path, dry_run: bool, remove_magenta_pixels: bool, remove_pale_lines: bool) -> dict[str, Any]:
    with Image.open(path) as src:
        img = src.convert("RGBA")
    pix = img.load()
    w, h = img.size
    comps = components(img)
    findings = []
    removed = 0
    if remove_magenta_pixels:
        magenta_removed = remove_key_magenta(img, dry_run)
        if magenta_removed:
            findings.append({"kind": "key_magenta_pixels", "pixels": magenta_removed, "bbox": None})
            if not dry_run:
                removed += magenta_removed
    if remove_pale_lines:
        pale_findings = remove_pale_edge_lines(img, dry_run)
        findings.extend(pale_findings)
        if not dry_run:
            removed += sum(item["pixels"] for item in pale_findings)
        pix = img.load()
    for comp in comps[1:]:
        kind = classify_component(comp, pix, w, h)
        if not kind:
            continue
        xs = [x for x, _ in comp]
        ys = [y for _, y in comp]
        finding = {"kind": kind, "pixels": len(comp), "bbox": [min(xs), min(ys), max(xs), max(ys)]}
        findings.append(finding)
        if not dry_run:
            for x, y in comp:
                pix[x, y] = (0, 0, 0, 0)
                removed += 1
    if removed and not dry_run:
        img.save(path)
    return {"path": path.as_posix(), "findings": findings, "removed_pixels": removed if not dry_run else sum(f["pixels"] for f in findings)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--batch")
    parser.add_argument("--path", action="append", default=[])
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--restore-first", action="store_true", help="Restore batch paths from promotion manifests before cleanup.")
    parser.add_argument("--remove-magenta-pixels", action="store_true", help="Remove key-colour magenta pixels even when attached to the main sprite edge.")
    parser.add_argument("--remove-pale-lines", action="store_true", help="Remove pale guide/crop rows or columns near crop edges.")
    parser.add_argument("--contact-out", help="Optional rebuilt contact sheet path after cleanup.")
    parser.add_argument("--label-strip", default="")
    parser.add_argument("--json-out")
    args = parser.parse_args()

    if args.restore_first:
        if not args.batch:
            raise SystemExit("--restore-first requires --batch")
        subprocess.run(["python", "scripts/tileset_restore_candidate.py", "--batch", args.batch], check=True)

    paths = [Path(p) for p in args.path] + [Path(p) for p in batch_paths(Path(args.batch) if args.batch else None)]
    seen = set()
    unique_paths = []
    for path in paths:
        key = path.as_posix()
        if key not in seen:
            seen.add(key)
            unique_paths.append(path)

    results = [clean_path(path, args.dry_run, args.remove_magenta_pixels, args.remove_pale_lines) for path in unique_paths if path.exists()]
    if args.contact_out and args.batch and not args.dry_run:
        subprocess.run(
            [
                "python",
                "scripts/tileset_review_existing_batch.py",
                "--batch",
                args.batch,
                "--out",
                args.contact_out,
                "--label-strip",
                args.label_strip,
                "--review-note",
                "inspection only: contact sheet rebuilt after artifact cleanup",
            ],
            check=True,
        )
    report = {"dry_run": args.dry_run, "images": results, "findings": sum(len(item["findings"]) for item in results)}
    text = json.dumps(report, indent=2, ensure_ascii=True) + "\n"
    if args.json_out:
        Path(args.json_out).write_text(text, encoding="utf-8")
    print(text, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
