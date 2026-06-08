#!/usr/bin/env python3
"""Replay accepted entity contact sheets into runtime assets at current policy size.

This is a one-off repair helper for assets that were generated at useful
contact-sheet resolution but were promoted into `assets/tiles/entities` as tiny
32/48 px runtime PNGs. It does not call image generation.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from typing import Any

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
REALM_TILESET_HELPER = ROOT / ".agents" / "skills" / "realm-tileset-from-images" / "scripts" / "realm_tileset.py"
ASSET_ENTITIES = ROOT / "assets" / "tiles" / "entities"
CANDIDATE_ROOTS = {
    "animal": ROOT / "art" / "tiles" / "candidates" / "animals",
    "unit": ROOT / "art" / "tiles" / "candidates" / "units",
    "building": ROOT / "art" / "tiles" / "candidates" / "buildings",
}
SPEC_ROOTS = {
    "animal": ROOT / "art" / "tiles" / "image-json" / "animals",
    "unit": ROOT / "art" / "tiles" / "image-json" / "units",
    "building": ROOT / "art" / "tiles" / "image-json" / "buildings",
}


def load_realm_tileset_helper() -> Any:
    spec = importlib.util.spec_from_file_location("realm_tileset_replay", REALM_TILESET_HELPER)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"could not load helper: {REALM_TILESET_HELPER}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def read_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict[str, Any]) -> None:
    path.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def infer_asset_type(entity: str, manifest: dict[str, Any]) -> str:
    raw = str(manifest.get("asset_type") or "")
    if raw in CANDIDATE_ROOTS:
        return raw
    for asset_type, root in CANDIDATE_ROOTS.items():
        if (root / entity / "sheets").exists():
            return asset_type
    return raw or "unit"


def load_spec(entity: str, asset_type: str) -> dict[str, Any]:
    path = SPEC_ROOTS.get(asset_type, ROOT / "missing") / f"{entity}.json"
    return read_json(path) if path.exists() else {}


def source_canvas(spec: dict[str, Any], manifest: dict[str, Any]) -> dict[str, Any]:
    art = spec.get("art")
    if isinstance(art, dict) and isinstance(art.get("source_canvas"), dict):
        return dict(art["source_canvas"])
    if isinstance(manifest.get("source_canvas"), dict):
        return dict(manifest["source_canvas"])
    return {}


def action_source_canvas(
    spec_actions: dict[str, dict[str, Any]],
    manifest: dict[str, Any],
    action_id: str,
    fallback: dict[str, Any],
) -> dict[str, Any]:
    action_spec = spec_actions.get(action_id, {})
    if isinstance(action_spec.get("source_canvas"), dict):
        return dict(action_spec["source_canvas"])
    manifest_action = manifest.get("actions", {}).get(action_id, {})
    if isinstance(manifest_action, dict) and isinstance(manifest_action.get("source_canvas"), dict):
        return dict(manifest_action["source_canvas"])
    return dict(fallback)


def target_dimensions(canvas: dict[str, Any], manifest: dict[str, Any]) -> tuple[int, int]:
    width = canvas.get("width_px")
    height = canvas.get("height_px")
    if isinstance(width, int) and width > 0 and isinstance(height, int) and height > 0:
        return width, height
    size = int(manifest.get("sprite_size") or 256)
    return size, size


def sheet_for_direction(sheets_dir: Path, direction: str) -> Path | None:
    candidates = [
        sheets_dir / f"{direction}_contact_sheet.png",
        sheets_dir / f"{direction}.png",
    ]
    if direction in {"front", "back"}:
        candidates.append(sheets_dir / "front_back_contact_sheet.png")
    for path in candidates:
        if path.exists():
            return path
    return None


def action_spec_map(spec: dict[str, Any]) -> dict[str, dict[str, Any]]:
    return {str(action.get("id")): action for action in spec.get("actions", []) if action.get("id")}


def frame_pairs(manifest: dict[str, Any], direction: str) -> list[tuple[str, int, dict[str, Any]]]:
    pairs: list[tuple[str, int, dict[str, Any]]] = []
    for action_id, action in manifest.get("actions", {}).items():
        frames = action.get("directions", {}).get(direction, [])
        for index, frame in enumerate(frames):
            pairs.append((str(action_id), index, frame if isinstance(frame, dict) else {}))
    return pairs


def frames_for_spec_action(action: dict[str, Any], asset_type: str) -> int:
    for key in ("frames_recommended", "frame_count"):
        value = action.get(key)
        if isinstance(value, int) and value > 0:
            return value
    for key in ("phases", "frames"):
        value = action.get(key)
        if isinstance(value, list) and value:
            return len(value)
    if asset_type in {"unit", "animal"}:
        return 2
    return 1


def canonical_frame_pairs(
    spec: dict[str, Any],
    manifest: dict[str, Any],
    direction: str,
    asset_type: str,
) -> list[tuple[str, int, dict[str, Any]]]:
    pairs: list[tuple[str, int, dict[str, Any]]] = []
    manifest_actions = manifest.get("actions", {})
    for action in spec.get("actions", []):
        if not isinstance(action, dict) or not action.get("id"):
            continue
        action_id = str(action["id"])
        manifest_frames = manifest_actions.get(action_id, {}).get("directions", {}).get(direction, [])
        for frame_index in range(frames_for_spec_action(action, asset_type)):
            old_frame = manifest_frames[frame_index] if frame_index < len(manifest_frames) and isinstance(manifest_frames[frame_index], dict) else {}
            pairs.append((action_id, frame_index, old_frame))
    return pairs


def canonical_action_slots(
    spec: dict[str, Any],
    manifest: dict[str, Any],
    direction: str,
    asset_type: str,
) -> list[tuple[str, list[tuple[int, dict[str, Any]]]]]:
    slots: list[tuple[str, list[tuple[int, dict[str, Any]]]]] = []
    manifest_actions = manifest.get("actions", {})
    for action in spec.get("actions", []):
        if not isinstance(action, dict) or not action.get("id"):
            continue
        action_id = str(action["id"])
        manifest_frames = manifest_actions.get(action_id, {}).get("directions", {}).get(direction, [])
        frame_slots: list[tuple[int, dict[str, Any]]] = []
        for frame_index in range(frames_for_spec_action(action, asset_type)):
            old_frame = manifest_frames[frame_index] if frame_index < len(manifest_frames) and isinstance(manifest_frames[frame_index], dict) else {}
            frame_slots.append((frame_index, old_frame))
        slots.append((action_id, frame_slots))
    return slots


def canonical_actions_for_manifest(spec: dict[str, Any], asset_type: str, directions: list[str]) -> dict[str, Any]:
    actions: dict[str, Any] = {}
    for action in spec.get("actions", []):
        if not isinstance(action, dict) or not action.get("id"):
            continue
        action_id = str(action["id"])
        frame_count = frames_for_spec_action(action, asset_type)
        frame_ms = 250
        durations = action.get("frame_durations_ms")
        if isinstance(durations, list) and durations:
            frame_ms = int(durations[0] or frame_ms)
        elif isinstance(action.get("frame_ms"), int):
            frame_ms = int(action["frame_ms"])
        entry: dict[str, Any] = {
            "frame_ms": frame_ms,
            "loop": action.get("loop", action_id != "death"),
            "hold_last": action.get("hold_last", action_id == "death"),
            "description": action.get("description", action_id),
            "family": action.get("family", action_id),
            "fit_profile": action.get("fit_profile", "lying" if action_id == "death" else "standing"),
            "directions": {direction: [{} for _ in range(frame_count)] for direction in directions},
        }
        if isinstance(action.get("placement"), dict):
            entry["placement"] = action["placement"]
        if isinstance(action.get("source_canvas"), dict):
            entry["source_canvas"] = action["source_canvas"]
        if isinstance(action.get("visual_envelope"), dict):
            entry["visual_envelope"] = action["visual_envelope"]
        if action.get("visual_envelope_note"):
            entry["visual_envelope_note"] = action["visual_envelope_note"]
        phases = action.get("phases")
        if isinstance(phases, list):
            entry["phases"] = phases
        actions[action_id] = entry
    return actions


def slot_indices_for_pairs(
    pairs: list[tuple[str, int, dict[str, Any]]],
    boxes: list[tuple[int, int, int, int]],
    asset_type: str,
) -> list[int]:
    death_frame_count = sum(1 for pair in pairs if pair[0] == "death")
    if (
        asset_type == "animal"
        and pairs
        and pairs[-1][0] == "death"
        and death_frame_count == 2
        and len(pairs) >= 2
        and len(boxes) >= len(pairs) + 2
    ):
        death_start = next((index for index, pair in enumerate(pairs) if pair[0] == "death"), len(pairs) - 2)
        indices = list(range(death_start))
        indices.extend([death_start, len(boxes) - 1])
        return indices[: len(pairs)]
    return list(range(min(len(pairs), len(boxes))))


def ordered_slot_indices(box_count: int, cols: int | None, rows: int | None, order: str) -> list[int]:
    if order == "column-major" and cols and rows:
        indices: list[int] = []
        for col in range(cols):
            for row in range(rows):
                index = row * cols + col
                if index < box_count:
                    indices.append(index)
        return indices
    return list(range(box_count))


def profile_for_action(rt: Any, action_id: str, action: dict[str, Any], spec_actions: dict[str, dict[str, Any]], spec: dict[str, Any]) -> str:
    if action.get("fit_profile"):
        return str(action["fit_profile"])
    if action_id in spec_actions and spec_actions[action_id].get("fit_profile"):
        return str(spec_actions[action_id]["fit_profile"])
    fallback = {"id": action_id, "description": action.get("description", ""), "family": action.get("family", "")}
    return rt.default_fit_profile_for_action(fallback, spec)


def clear_audit_magenta(img: Image.Image) -> Image.Image:
    rgba = img.convert("RGBA")
    pixels = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pixels[x, y]
            if a > 0 and r >= 175 and b >= 145 and g <= 105:
                pixels[x, y] = (r, g, b, 0)
    return rgba


def is_foreground_pixel(r: int, g: int, b: int, a: int) -> bool:
    if a <= 8:
        return False
    if r >= 175 and b >= 145 and g <= 105:
        return False
    if r >= 238 and g >= 238 and b >= 238 and max(r, g, b) - min(r, g, b) <= 14:
        return False
    return True


def projection_runs(values: list[int], threshold: int, *, min_width: int = 8) -> list[tuple[int, int]]:
    runs: list[tuple[int, int]] = []
    start: int | None = None
    for index, value in enumerate(values):
        if value > threshold and start is None:
            start = index
        elif value <= threshold and start is not None:
            if index - start >= min_width:
                runs.append((start, index))
            start = None
    if start is not None and len(values) - start >= min_width:
        runs.append((start, len(values)))
    return runs


def foreground_bbox_in_region(
    img: Image.Image,
    region: tuple[int, int, int, int],
) -> tuple[int, int, int, int, int] | None:
    x0, y0, x1, y1 = region
    pixels = img.load()
    min_x, min_y = x1, y1
    max_x, max_y = x0 - 1, y0 - 1
    count = 0
    for y in range(y0, y1):
        for x in range(x0, x1):
            r, g, b, a = pixels[x, y]
            if is_foreground_pixel(r, g, b, a):
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
                count += 1
    if count <= 0:
        return None
    return (min_x, min_y, max_x + 1, max_y + 1, count)


def rectangle_around_bbox(
    bbox: tuple[int, int, int, int, int],
    nominal_width: int,
    nominal_height: int,
    width: int,
    height: int,
) -> tuple[int, int, int, int]:
    x0, y0, x1, y1, _count = bbox
    target_aspect = max(1, nominal_width) / max(1, nominal_height)
    content_w = max(1, x1 - x0)
    content_h = max(1, y1 - y0)
    rect_w = max(1, nominal_width, content_w)
    rect_h = max(1, nominal_height, content_h)
    if rect_w / rect_h < target_aspect:
        rect_w = int(round(rect_h * target_aspect))
    else:
        rect_h = int(round(rect_w / target_aspect))
    rect_w = min(width, max(1, rect_w))
    rect_h = min(height, max(1, rect_h))
    cx = (x0 + x1) / 2
    cy = (y0 + y1) / 2
    left = int(round(cx - rect_w / 2))
    top = int(round(cy - rect_h / 2))
    left = max(0, min(left, max(0, width - rect_w)))
    top = max(0, min(top, max(0, height - rect_h)))
    return (left, top, min(width, left + rect_w), min(height, top + rect_h))


def retarget_box(
    img: Image.Image,
    region: tuple[int, int, int, int],
    target_width: int,
    target_height: int,
) -> tuple[int, int, int, int]:
    bbox = foreground_bbox_in_region(img, region)
    if bbox is None:
        return region
    region_w = max(1, region[2] - region[0])
    region_h = max(1, region[3] - region[1])
    target_aspect = max(1, target_width) / max(1, target_height)
    region_area = region_w * region_h
    nominal_w = max(1, int(round((region_area * target_aspect) ** 0.5)))
    nominal_h = max(1, int(round(nominal_w / target_aspect)))
    return rectangle_around_bbox(bbox, nominal_w, nominal_h, img.width, img.height)


def detected_content_boxes(
    img: Image.Image,
    cols: int,
    rows: int,
    *,
    min_content_pixels: int,
    target_width: int,
    target_height: int,
) -> tuple[list[tuple[int, int, int, int]], str, dict[str, Any]]:
    rgba = img.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    row_counts = [0 for _ in range(height)]
    col_counts = [0 for _ in range(width)]
    for y in range(height):
        for x in range(width):
            r, g, b, a = pixels[x, y]
            if is_foreground_pixel(r, g, b, a):
                row_counts[y] += 1
                col_counts[x] += 1

    row_threshold = max(10, int(width * 0.01))
    col_threshold = max(10, int(height * 0.01))
    row_runs = projection_runs(row_counts, row_threshold, min_width=max(8, height // 200))
    col_runs = projection_runs(col_counts, col_threshold, min_width=max(8, width // 200))
    nominal_w = max(1, int(round(width / max(1, cols))))
    nominal_h = max(1, int(round(height / max(1, rows))))
    target_aspect = max(1, target_width) / max(1, target_height)
    if nominal_w / nominal_h < target_aspect:
        nominal_h = max(1, int(round(nominal_w / target_aspect)))
    else:
        nominal_w = max(1, int(round(nominal_h * target_aspect)))

    boxes: list[tuple[int, int, int, int]] = []
    if row_runs and col_runs:
        cells: list[tuple[float, float, tuple[int, int, int, int, int]]] = []
        for row in row_runs:
            for col in col_runs:
                bbox = foreground_bbox_in_region(rgba, (col[0], row[0], col[1], row[1]))
                if bbox is not None and bbox[4] >= min_content_pixels:
                    cells.append(((row[0] + row[1]) / 2, (col[0] + col[1]) / 2, bbox))
        cells.sort(key=lambda item: (item[0], item[1]))
        boxes = [rectangle_around_bbox(cell[2], nominal_w, nominal_h, width, height) for cell in cells]
        if boxes:
            return boxes, "projection-content-cells", {
                "rows": len(row_runs),
                "cols": len(col_runs),
                "nominal_size": [nominal_w, nominal_h],
            }

    # Fallback: connected foreground components. This handles non-grid sheets
    # but still sorts detected sprite clusters in reading order.
    visited = bytearray(width * height)
    components: list[tuple[int, int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            if visited[idx]:
                continue
            r, g, b, a = pixels[x, y]
            if not is_foreground_pixel(r, g, b, a):
                continue
            stack = [(x, y)]
            visited[idx] = 1
            min_x = max_x = x
            min_y = max_y = y
            count = 0
            while stack:
                px, py = stack.pop()
                count += 1
                min_x = min(min_x, px)
                max_x = max(max_x, px)
                min_y = min(min_y, py)
                max_y = max(max_y, py)
                for nx in (px - 1, px, px + 1):
                    for ny in (py - 1, py, py + 1):
                        if nx < 0 or ny < 0 or nx >= width or ny >= height:
                            continue
                        nidx = ny * width + nx
                        if visited[nidx]:
                            continue
                        nr, ng, nb, na = pixels[nx, ny]
                        if is_foreground_pixel(nr, ng, nb, na):
                            visited[nidx] = 1
                            stack.append((nx, ny))
            if count >= min_content_pixels:
                components.append((min_x, min_y, max_x + 1, max_y + 1, count))
    components.sort(key=lambda box: (((box[1] + box[3]) / 2), ((box[0] + box[2]) / 2)))
    boxes = [rectangle_around_bbox(component, nominal_w, nominal_h, width, height) for component in components]
    return boxes, "connected-components", {"nominal_size": [nominal_w, nominal_h]}


def grid_cell_content_boxes(
    img: Image.Image,
    cols: int,
    rows: int,
    *,
    min_content_pixels: int,
    target_width: int,
    target_height: int,
) -> tuple[list[tuple[int, int, int, int]], str, dict[str, Any]]:
    rgba = img.convert("RGBA")
    width, height = rgba.size
    target_aspect = max(1, target_width) / max(1, target_height)
    nominal_w = max(1, int(round(width / max(1, cols))))
    nominal_h = max(1, int(round(height / max(1, rows))))
    if nominal_w / nominal_h < target_aspect:
        nominal_h = max(1, int(round(nominal_w / target_aspect)))
    else:
        nominal_w = max(1, int(round(nominal_h * target_aspect)))

    boxes: list[tuple[int, int, int, int]] = []
    empty_cells = 0
    for row in range(max(1, rows)):
        y0 = int(round(row * height / max(1, rows)))
        y1 = int(round((row + 1) * height / max(1, rows)))
        for col in range(max(1, cols)):
            x0 = int(round(col * width / max(1, cols)))
            x1 = int(round((col + 1) * width / max(1, cols)))
            bbox = foreground_bbox_in_region(rgba, (x0, y0, x1, y1))
            if bbox is None or bbox[4] < min_content_pixels:
                empty_cells += 1
                continue
            rect = rectangle_around_bbox(bbox, nominal_w, nominal_h, width, height)
            boxes.append(
                (
                    max(x0, rect[0]),
                    max(y0, rect[1]),
                    min(x1, rect[2]),
                    min(y1, rect[3]),
                )
            )

    return boxes, "grid-cell-content", {
        "rows": rows,
        "cols": cols,
        "empty_cells": empty_cells,
        "nominal_size": [nominal_w, nominal_h],
    }


def replay_entity(rt: Any, manifest_path: Path, args: argparse.Namespace) -> dict[str, Any]:
    entity = manifest_path.parent.name
    manifest = read_json(manifest_path)
    current_size = int(manifest.get("sprite_size") or 0)
    asset_type = infer_asset_type(entity, manifest)
    spec = load_spec(entity, asset_type)
    canvas = source_canvas(spec, manifest)
    detection_target_width, detection_target_height = target_dimensions(canvas, manifest)
    spec_actions = action_spec_map(spec)
    action_canvases = [
        action_source_canvas(spec_actions, manifest, str(action.get("id")), canvas)
        for action in spec.get("actions", [])
        if isinstance(action, dict) and action.get("id")
    ]
    action_targets = [target_dimensions(action_canvas, manifest) for action_canvas in action_canvases]
    if action_targets:
        target_width = max(width for width, _height in action_targets)
        target_height = max(height for _width, height in action_targets)
    else:
        target_width, target_height = target_dimensions(canvas, manifest)
    target_max = max(target_width, target_height)
    if current_size >= target_max and not args.force_size:
        return {"entity": entity, "status": "skipped", "reason": f"sprite_size {current_size} already >= target {target_width}x{target_height}"}

    sheets_dir = CANDIDATE_ROOTS.get(asset_type, ROOT / "missing") / entity / "sheets"
    if not sheets_dir.exists():
        return {"entity": entity, "status": "skipped", "reason": "no contact-sheet directory"}

    if args.canonical_actions and spec.get("render", {}).get("directions"):
        directions = [str(direction) for direction in spec["render"]["directions"]]
    else:
        directions = sorted({direction for action in manifest.get("actions", {}).values() for direction in action.get("directions", {})})
    if not directions:
        return {"entity": entity, "status": "skipped", "reason": "manifest has no directions"}

    work: list[dict[str, Any]] = []
    skipped_directions: list[dict[str, str]] = []
    for direction in directions:
        sheet = sheet_for_direction(sheets_dir, direction)
        if sheet is None:
            skipped_directions.append({"direction": direction, "reason": "no contact sheet"})
            continue
        with Image.open(sheet) as opened:
            source = opened.convert("RGBA")
            if args.crop_mode == "grid":
                boxes, method, detection = grid_cell_content_boxes(
                    source,
                    args.cols,
                    args.rows,
                    min_content_pixels=args.min_content_pixels,
                    target_width=detection_target_width,
                    target_height=detection_target_height,
                )
            else:
                boxes, method, detection = detected_content_boxes(
                    source,
                    args.cols,
                    args.rows,
                    min_content_pixels=args.min_content_pixels,
                    target_width=detection_target_width,
                    target_height=detection_target_height,
                )
        if not boxes:
            skipped_directions.append({"direction": direction, "reason": "no detected content boxes"})
            continue
        action_slots = canonical_action_slots(spec, manifest, direction, asset_type) if args.canonical_actions and args.one_slot_per_action else []
        pairs = canonical_frame_pairs(spec, manifest, direction, asset_type) if args.canonical_actions else frame_pairs(manifest, direction)
        if not pairs:
            continue
        ordered_indices = ordered_slot_indices(len(boxes), args.cols, args.rows, args.slot_order)
        if action_slots:
            slot_indices = ordered_indices[: min(len(action_slots), len(boxes))]
            expected_slots = len(action_slots)
        else:
            pair_indices = slot_indices_for_pairs(pairs, boxes, asset_type)
            slot_indices = [ordered_indices[index] for index in pair_indices if index < len(ordered_indices)]
            expected_slots = len(pairs)
        if len(slot_indices) < expected_slots and not args.allow_fewer_slots:
            skipped_directions.append(
                {
                    "direction": direction,
                    "reason": f"{len(boxes)} detected content boxes for {expected_slots} manifest slots",
                }
            )
            continue
        insufficient_slots: list[str] = []
        if not args.allow_upscale:
            if action_slots:
                for slot_offset, (action_id, _frame_slots) in enumerate(action_slots[: len(slot_indices)]):
                    slot_index = slot_indices[slot_offset]
                    action_canvas = action_source_canvas(spec_actions, manifest, action_id, canvas)
                    action_target_width, action_target_height = target_dimensions(action_canvas, manifest)
                    retargeted = retarget_box(source, boxes[slot_index], action_target_width, action_target_height)
                    slot_w = retargeted[2] - retargeted[0]
                    slot_h = retargeted[3] - retargeted[1]
                    if slot_w < action_target_width or slot_h < action_target_height:
                        insufficient_slots.append(f"{action_id} source {slot_w}x{slot_h} < target {action_target_width}x{action_target_height}")
            else:
                for pair_offset, (action_id, _frame_index, _old_frame) in enumerate(pairs[: len(slot_indices)]):
                    slot_index = slot_indices[pair_offset]
                    action_canvas = action_source_canvas(spec_actions, manifest, action_id, canvas)
                    action_target_width, action_target_height = target_dimensions(action_canvas, manifest)
                    retargeted = retarget_box(source, boxes[slot_index], action_target_width, action_target_height)
                    slot_w = retargeted[2] - retargeted[0]
                    slot_h = retargeted[3] - retargeted[1]
                    if slot_w < action_target_width or slot_h < action_target_height:
                        insufficient_slots.append(f"{action_id}#{_frame_index:02d} source {slot_w}x{slot_h} < target {action_target_width}x{action_target_height}")
        if insufficient_slots:
            preview = "; ".join(insufficient_slots[:4])
            if len(insufficient_slots) > 4:
                preview += f"; {len(insufficient_slots) - 4} more"
            skipped_directions.append({"direction": direction, "reason": preview})
            continue
        work.append(
            {
                "direction": direction,
                "sheet": sheet,
                "action_slots": action_slots,
                "pairs": pairs,
                "boxes": boxes,
                "slot_indices": slot_indices,
                "method": method,
                "detection": detection,
            }
        )

    if not work:
        reason = "; ".join(f"{item['direction']}: {item['reason']}" for item in skipped_directions) or "no replayable directions"
        return {"entity": entity, "status": "skipped", "reason": reason}

    if args.dry_run:
        frame_total = 0
        for item in work:
            if item["action_slots"]:
                frame_total += sum(len(frame_slots) for _action_id, frame_slots in item["action_slots"][: len(item["slot_indices"])])
            else:
                frame_total += len(item["slot_indices"])
        return {
            "entity": entity,
            "status": "would_replay" if not skipped_directions else "would_replay_partial",
            "asset_type": asset_type,
            "from_size": current_size,
            "to_size": target_max,
            "to_dimensions": [target_width, target_height],
            "directions": [item["direction"] for item in work],
            "skipped_directions": skipped_directions,
            "frames": frame_total,
            "slot_mode": "one_slot_per_action" if any(item["action_slots"] for item in work) else "one_slot_per_frame",
            "detection": {
                item["direction"]: {
                    "method": item["method"],
                    "boxes": len(item["boxes"]),
                    **item["detection"],
                }
                for item in work
            },
        }

    key_color = rt.parse_color(args.magenta)
    team_color = rt.parse_color(args.team_color)
    if args.canonical_actions:
        manifest["actions"] = canonical_actions_for_manifest(spec, asset_type, [item["direction"] for item in work])
    frame_count = 0
    for item in work:
        direction = item["direction"]
        with Image.open(item["sheet"]) as opened:
            sheet = opened.convert("RGBA")
        boxes = item["boxes"]
        slot_indices = item["slot_indices"]
        if item["action_slots"]:
            for slot_offset, (action_id, frame_slots) in enumerate(item["action_slots"][: len(slot_indices)]):
                slot_index = slot_indices[slot_offset]
                action = manifest["actions"][action_id]
                fit_profile = profile_for_action(rt, action_id, action, spec_actions, spec)
                action_canvas = action_source_canvas(spec_actions, manifest, action_id, canvas)
                action_target_width, action_target_height = target_dimensions(action_canvas, manifest)
                source_box = boxes[slot_index] if args.crop_mode == "grid" else retarget_box(sheet, boxes[slot_index], action_target_width, action_target_height)
                panel = sheet.crop(source_box)
                base, team_mask, stats = rt.process_tile_image(
                    panel,
                    {"fit_profile": fit_profile},
                    (action_target_width, action_target_height),
                    key_color,
                    args.magenta_threshold,
                    team_color,
                    args.team_threshold,
                )
                base = clear_audit_magenta(base)
                team_mask = clear_audit_magenta(team_mask)
                action_dir = manifest_path.parent / action_id / direction
                action_dir.mkdir(parents=True, exist_ok=True)
                for frame_index, old_frame in frame_slots:
                    base_path = action_dir / f"frame_{frame_index:02d}_base.png"
                    mask_path = action_dir / f"frame_{frame_index:02d}_teammask.png"
                    base.save(base_path)
                    team_mask.save(mask_path)
                    new_frame = dict(old_frame)
                    new_frame.update(
                        {
                            "base": f"{action_id}/{direction}/frame_{frame_index:02d}_base.png",
                            "team_mask": f"{action_id}/{direction}/frame_{frame_index:02d}_teammask.png",
                            "source": rel(item["sheet"]),
                            "source_slot": slot_index + 1,
                            "source_crop_box": list(source_box),
                            "source_bbox": stats.get("source_bbox"),
                            "final_bbox": stats.get("final_bbox"),
                            "anchor": stats.get("anchor"),
                            "anchor_offset": stats.get("anchor_offset"),
                            "scale": stats.get("scale"),
                            "fit_profile": fit_profile,
                            "manual_review": new_frame.get("manual_review", "pending"),
                        }
                    )
                    action["directions"][direction][frame_index] = new_frame
                    frame_count += 1
            continue

        for pair_offset, (action_id, frame_index, old_frame) in enumerate(item["pairs"][: len(slot_indices)]):
            slot_index = slot_indices[pair_offset]
            action = manifest["actions"][action_id]
            fit_profile = profile_for_action(rt, action_id, action, spec_actions, spec)
            action_canvas = action_source_canvas(spec_actions, manifest, action_id, canvas)
            action_target_width, action_target_height = target_dimensions(action_canvas, manifest)
            source_box = boxes[slot_index] if args.crop_mode == "grid" else retarget_box(sheet, boxes[slot_index], action_target_width, action_target_height)
            panel = sheet.crop(source_box)
            base, team_mask, stats = rt.process_tile_image(
                panel,
                {"fit_profile": fit_profile},
                (action_target_width, action_target_height),
                key_color,
                args.magenta_threshold,
                team_color,
                args.team_threshold,
            )
            base = clear_audit_magenta(base)
            team_mask = clear_audit_magenta(team_mask)
            action_dir = manifest_path.parent / action_id / direction
            action_dir.mkdir(parents=True, exist_ok=True)
            base_path = action_dir / f"frame_{frame_index:02d}_base.png"
            mask_path = action_dir / f"frame_{frame_index:02d}_teammask.png"
            base.save(base_path)
            team_mask.save(mask_path)
            new_frame = dict(old_frame)
            new_frame.update(
                {
                    "base": f"{action_id}/{direction}/frame_{frame_index:02d}_base.png",
                    "team_mask": f"{action_id}/{direction}/frame_{frame_index:02d}_teammask.png",
                    "source": rel(item["sheet"]),
                    "source_slot": slot_index + 1,
                    "source_crop_box": list(source_box),
                    "source_bbox": stats.get("source_bbox"),
                    "final_bbox": stats.get("final_bbox"),
                    "anchor": stats.get("anchor"),
                    "anchor_offset": stats.get("anchor_offset"),
                    "scale": stats.get("scale"),
                    "fit_profile": fit_profile,
                    "manual_review": new_frame.get("manual_review", "pending"),
                }
            )
            action["directions"][direction][frame_index] = new_frame
            frame_count += 1

    manifest["asset_type"] = asset_type
    manifest["sprite_size"] = target_max
    if canvas:
        manifest["source_canvas"] = canvas
    placement = rt.default_manifest_placement(asset_type, target_width, target_height)
    footprint = canvas.get("footprint") if isinstance(canvas.get("footprint"), dict) else None
    if footprint:
        placement["footprint"] = [int(footprint.get("w", 1) or 1), int(footprint.get("h", 1) or 1)]
    visual_envelope = canvas.get("visual_envelope") if isinstance(canvas.get("visual_envelope"), dict) else None
    if visual_envelope:
        placement["visual_envelope"] = [
            int(visual_envelope.get("w", 1) or 1),
            int(visual_envelope.get("h", 1) or 1),
        ]
    manifest["placement"] = placement
    manifest["source_workflow"] = "contact-sheet-resolution-replay"
    write_json(manifest_path, manifest)

    return {
        "entity": entity,
        "status": "replayed" if not skipped_directions else "replayed_partial",
        "asset_type": asset_type,
        "from_size": current_size,
        "to_size": target_max,
        "to_dimensions": [target_width, target_height],
        "directions": [item["direction"] for item in work],
        "skipped_directions": skipped_directions,
        "frames": frame_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--entity", action="append", default=[], help="limit to one entity; may be repeated")
    parser.add_argument("--apply", action="store_true", help="write runtime assets; default is dry-run")
    parser.add_argument("--force-size", action="store_true", help="replay even if manifest size is already at target")
    parser.add_argument("--allow-upscale", action="store_true", help="allow runtime output larger than the source sheet slot")
    parser.add_argument("--allow-fewer-slots", action="store_true", help="replay detected slots even when the manifest has extra frames")
    parser.add_argument("--canonical-actions", action="store_true", help="replace manifest actions with the generated image-json action contract")
    parser.add_argument("--one-slot-per-action", action="store_true", help="with canonical actions, use one source sheet slot for every frame of each action")
    parser.add_argument("--crop-mode", choices=["content", "grid"], default="content", help="content detects drifting sprites; grid crops fixed authored cells then tightens by bbox")
    parser.add_argument("--cols", type=int, default=4)
    parser.add_argument("--rows", type=int, default=4)
    parser.add_argument("--slot-order", choices=["row-major", "column-major"], default="row-major")
    parser.add_argument("--min-content-pixels", type=int, default=256)
    parser.add_argument("--magenta", default="#ff00ff")
    parser.add_argument("--magenta-threshold", type=float, default=62.0)
    parser.add_argument("--team-color", default="#0088cc")
    parser.add_argument("--team-threshold", type=float, default=42.0)
    parser.add_argument("--json-out", default="")
    args = parser.parse_args()
    args.dry_run = not args.apply

    rt = load_realm_tileset_helper()
    def normalize_entity_name(name: str) -> str:
        return name.strip().lower().replace("_", "-")

    requested = {normalize_entity_name(name) for name in args.entity}
    manifests = sorted(ASSET_ENTITIES.glob("*/manifest.json"))
    if requested:
        manifests = [path for path in manifests if normalize_entity_name(path.parent.name) in requested]

    results = [replay_entity(rt, path, args) for path in manifests]
    summary: dict[str, int] = {}
    for result in results:
        summary[result["status"]] = summary.get(result["status"], 0) + 1
    report = {"schema": "realm.entity_contact_sheet_replay.v1", "dry_run": args.dry_run, "summary": summary, "results": results}
    if args.json_out:
        out = ROOT / args.json_out
        out.parent.mkdir(parents=True, exist_ok=True)
        write_json(out, report)
    print(json.dumps(report, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
