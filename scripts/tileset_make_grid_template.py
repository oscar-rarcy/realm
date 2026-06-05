#!/usr/bin/env python3
"""Create a premade magenta-on-white image-generation grid template."""

from __future__ import annotations

import argparse
import json
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


def parse_cell(value: str) -> tuple[int, int]:
    if "," not in value:
        raise argparse.ArgumentTypeError("cell must be row,col")
    row_s, col_s = value.split(",", 1)
    return int(row_s), int(col_s)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", required=True)
    parser.add_argument("--cols", type=int, default=4)
    parser.add_argument("--rows", type=int, default=4)
    parser.add_argument("--size", type=int, default=1024)
    parser.add_argument("--line-width", type=int, default=6)
    parser.add_argument("--margin", type=int, default=18)
    parser.add_argument("--cell-fill", choices=["white", "magenta"], default="white")
    parser.add_argument("--gutter", type=int, default=0, help="Optional fixed gutter between filled cells.")
    parser.add_argument("--gutter-color", default="white")
    parser.add_argument("--line-color", default="#ff00ff")
    parser.add_argument("--seed", help="Optional seed image to place in one cell.")
    parser.add_argument("--seed-cell", type=parse_cell, default=(0, 0), help="row,col for optional seed image.")
    parser.add_argument("--label", help="Optional small label written outside the grid.")
    args = parser.parse_args()

    if args.cols <= 0 or args.rows <= 0:
        raise SystemExit("cols and rows must be positive")
    if args.size < 128:
        raise SystemExit("size must be at least 128")

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    img = Image.new("RGB", (args.size, args.size), args.gutter_color)
    draw = ImageDraw.Draw(img)
    magenta = args.line_color
    slots: list[dict[str, object]] = []

    if args.gutter > 0:
        cell_w = (args.size - (args.cols + 1) * args.gutter) / args.cols
        cell_h = (args.size - (args.rows + 1) * args.gutter) / args.rows
        fill = "#ff00ff" if args.cell_fill == "magenta" else "white"
        for row in range(args.rows):
            for col in range(args.cols):
                x0 = round(args.gutter + col * (cell_w + args.gutter))
                y0 = round(args.gutter + row * (cell_h + args.gutter))
                x1 = round(x0 + cell_w)
                y1 = round(y0 + cell_h)
                draw.rectangle((x0, y0, x1 - 1, y1 - 1), fill=fill)
                slots.append({"row": row + 1, "column": col + 1, "box": [x0, y0, x1, y1]})
    else:
        cell_w = args.size / args.cols
        cell_h = args.size / args.rows
        if args.cell_fill == "magenta":
            for row in range(args.rows):
                for col in range(args.cols):
                    x0 = round(col * cell_w)
                    y0 = round(row * cell_h)
                    x1 = round((col + 1) * cell_w)
                    y1 = round((row + 1) * cell_h)
                    draw.rectangle((x0, y0, x1 - 1, y1 - 1), fill="#ff00ff")
                    slots.append({"row": row + 1, "column": col + 1, "box": [x0, y0, x1, y1]})
        for col in range(args.cols + 1):
            x = round(col * cell_w)
            draw.line((x, 0, x, args.size), fill=magenta, width=args.line_width)
        for row in range(args.rows + 1):
            y = round(row * cell_h)
            draw.line((0, y, args.size, y), fill=magenta, width=args.line_width)
        if not slots:
            for row in range(args.rows):
                for col in range(args.cols):
                    slots.append(
                        {
                            "row": row + 1,
                            "column": col + 1,
                            "box": [
                                round(col * cell_w),
                                round(row * cell_h),
                                round((col + 1) * cell_w),
                                round((row + 1) * cell_h),
                            ],
                        }
                    )

    if args.seed:
        seed_path = Path(args.seed)
        if not seed_path.exists():
            raise SystemExit(f"seed image not found: {seed_path}")
        row, col = args.seed_cell
        if row < 0 or row >= args.rows or col < 0 or col >= args.cols:
            raise SystemExit("seed-cell is outside the grid")
        with Image.open(seed_path) as seed_img:
            seed = seed_img.convert("RGBA")
            slot_box = slots[row * args.cols + col]["box"]
            x_min, y_min, x_max, y_max = [int(v) for v in slot_box]
            slot_w = x_max - x_min
            slot_h = y_max - y_min
            max_w = max(1, slot_w - 2 * args.margin)
            max_h = max(1, slot_h - 2 * args.margin)
            seed.thumbnail((max_w, max_h), Image.Resampling.LANCZOS)
            x0 = round(x_min + (slot_w - seed.width) / 2)
            y0 = round(y_min + (slot_h - seed.height) / 2)
            img.paste(seed, (x0, y0), seed)

    if args.label:
        try:
            font = ImageFont.truetype("arial.ttf", 20)
        except OSError:
            font = ImageFont.load_default()
        draw.rectangle((8, args.size - 34, args.size - 8, args.size - 8), fill=(255, 255, 255))
        draw.text((14, args.size - 30), args.label, fill=(50, 40, 35), font=font)

    img.save(out)
    out.with_suffix(".manifest.json").write_text(
        json.dumps(
            {
                "schema": "realm.grid_template.v1",
                "cols": args.cols,
                "rows": args.rows,
                "size": args.size,
                "cell_fill": args.cell_fill,
                "gutter": args.gutter,
                "slots": slots,
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    print(out.as_posix())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
