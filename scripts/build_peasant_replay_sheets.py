#!/usr/bin/env python3
"""Build peasant front/back replay sheets from existing interleaved batches."""

from __future__ import annotations

import json
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
MAGENTA = (255, 0, 255, 255)
OUT_DIR = ROOT / "art" / "tiles" / "candidates" / "units" / "peasant" / "sheets"
CELL = 256
COLS = 6
ROWS = 6


def crop_cell(path: Path, cols: int, rows: int, row: int, col: int) -> Image.Image:
    with Image.open(path) as opened:
        img = opened.convert("RGBA")
        x0 = round(col * img.width / cols)
        x1 = round((col + 1) * img.width / cols)
        y0 = round(row * img.height / rows)
        y1 = round((row + 1) * img.height / rows)
        return img.crop((x0, y0, x1, y1)).resize((CELL, CELL), Image.Resampling.LANCZOS)


def paste_slot(sheet: Image.Image, slot_index: int, cell: Image.Image) -> None:
    x = (slot_index % COLS) * CELL
    y = (slot_index // COLS) * CELL
    sheet.paste(cell, (x, y))


def add_interleaved_batch(
    slots: list[dict[str, object]],
    path: str,
    actions: list[str],
) -> None:
    source = ROOT / path
    for row, action in enumerate(actions):
        for frame in range(2):
            slots.append({"action": action, "frame": frame, "direction": "front", "path": path, "row": row, "col": frame * 2})
            slots.append({"action": action, "frame": frame, "direction": "back", "path": path, "row": row, "col": frame * 2 + 1})
    if not source.exists():
        raise SystemExit(f"missing source sheet: {source}")


def main() -> int:
    slots: list[dict[str, object]] = []
    idle_path = "art/tiles/candidates/units/peasant/idle/v002-paper-cutout/batch_source_keyed_flatgrid.png"
    slots.extend(
        [
            {"action": "idle", "frame": 0, "direction": "front", "path": idle_path, "row": 0, "col": 0, "cols": 2, "rows": 2},
            {"action": "idle", "frame": 0, "direction": "back", "path": idle_path, "row": 0, "col": 1, "cols": 2, "rows": 2},
            {"action": "idle", "frame": 1, "direction": "front", "path": idle_path, "row": 1, "col": 0, "cols": 2, "rows": 2},
            {"action": "idle", "frame": 1, "direction": "back", "path": idle_path, "row": 1, "col": 1, "cols": 2, "rows": 2},
        ]
    )
    add_interleaved_batch(
        slots,
        "art/tiles/candidates/units/peasant/actions_walk_chop_mine_berries/v001-paper-cutout/batch_source_keyed_flatgrid.png",
        ["walk", "chop_wood", "mine_gold", "gather_berries"],
    )
    add_interleaved_batch(
        slots,
        "art/tiles/candidates/units/peasant/actions_hoe_wheat_build_wood/v001-paper-cutout/batch_source_keyed_flatgrid.png",
        ["hoe_soil", "gather_wheat", "build", "carry_wood"],
    )
    add_interleaved_batch(
        slots,
        "art/tiles/candidates/units/peasant/actions_gold_berries_wheat_meat/v001-paper-cutout/batch_source_keyed_flatgrid.png",
        ["carry_gold", "carry_berries", "carry_wheat", "gather_meat"],
    )
    add_interleaved_batch(
        slots,
        "art/tiles/candidates/units/peasant/actions_carrymeat_attack_death/v001-paper-cutout/batch_source_keyed_flatgrid.png",
        ["carry_meat", "club_attack", "death", "decayed"],
    )

    direction_slots = {
        "front": [slot for slot in slots if slot["direction"] == "front"],
        "back": [slot for slot in slots if slot["direction"] == "back"],
    }
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    provenance: dict[str, object] = {"schema": "realm.peasant_replay_sheet.v1", "cell": CELL, "cols": COLS, "rows": ROWS, "directions": {}}
    for direction, dir_slots in direction_slots.items():
        sheet = Image.new("RGBA", (COLS * CELL, ROWS * CELL), MAGENTA)
        for index, slot in enumerate(dir_slots):
            cell = crop_cell(
                ROOT / str(slot["path"]),
                int(slot.get("cols", 4)),
                int(slot.get("rows", 4)),
                int(slot["row"]),
                int(slot["col"]),
            )
            paste_slot(sheet, index, cell)
        out = OUT_DIR / f"{direction}_contact_sheet.png"
        sheet.save(out)
        provenance["directions"][direction] = [{"slot": index + 1, **slot} for index, slot in enumerate(dir_slots)]

    (OUT_DIR / "peasant_replay_sheet_provenance.json").write_text(json.dumps(provenance, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
    print(json.dumps({"out_dir": OUT_DIR.as_posix(), "slots_per_direction": {k: len(v) for k, v in direction_slots.items()}}, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
