#!/usr/bin/env python3
"""Realm tileset extraction and review helpers."""

from __future__ import annotations

import argparse
import colorsys
import html
import json
import math
import shutil
import subprocess
from collections import deque
from datetime import datetime
from pathlib import Path
from typing import Any

from PIL import Image, ImageColor, ImageDraw, ImageFont


SELF_TILE = "self_tile"
ADJACENT_TARGET = "adjacent_target_tile_or_entity"

FIT_PROFILES: dict[str, dict[str, Any]] = {
    "standing": {"max_w": 34, "max_h": 42, "anchor": [24, 39], "padding": 4},
    "wide_tool": {"max_w": 44, "max_h": 42, "anchor": [24, 39], "padding": 2},
    "kneeling": {"max_w": 40, "max_h": 35, "anchor": [24, 40], "padding": 3},
    "lying": {"max_w": 44, "max_h": 25, "anchor": [24, 42], "padding": 2},
}

PEASANT_ACTIONS: list[dict[str, Any]] = [
    {
        "id": "idle",
        "frame_ms": 20000,
        "loop": False,
        "hold_last": True,
        "transition_after_ms": 20000,
        "family": "idle",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "phases": [
            "relaxed idle, arms at sides, both feet planted",
            "long-idle hold pose, arms crossed, both feet planted",
        ],
    },
    {
        "id": "walk",
        "frame_ms": 180,
        "loop": True,
        "family": "gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "phases": [
            "walking gait with the front/near leg forward and the rear/far leg back",
            "walking gait with the rear/far leg forward and the front/near leg back",
        ],
    },
    {
        "id": "chop_wood",
        "frame_ms": 320,
        "loop": True,
        "family": "swing",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "wide_tool",
        "tool": "wood axe",
        "phases": [
            "axe at the bottom/contact part of the chop, axe head low and forward",
            "axe raised high at the top of the swing",
        ],
    },
    {
        "id": "mine_gold",
        "frame_ms": 320,
        "loop": True,
        "family": "swing",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "wide_tool",
        "tool": "pickaxe",
        "phases": [
            "pickaxe at the bottom/contact part of the mining swing, pick head low and forward",
            "pickaxe raised high at the top of the swing",
        ],
    },
    {
        "id": "gather_berries",
        "frame_ms": 700,
        "loop": True,
        "family": "gather",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "kneeling",
        "phases": [
            "one hand reaching out toward berries beside a basket",
            "hand back in the basket with berries",
        ],
    },
    {
        "id": "hoe_soil",
        "frame_ms": 520,
        "loop": True,
        "family": "work_stroke",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "wide_tool",
        "tool": "long-handled farming hoe with a small flat rectangular blade, not an axe or pickaxe",
        "phases": [
            "arms outstretched with the hoe extended away from the body",
            "arms pulled in after the hoe stroke while still holding the same farming hoe",
        ],
    },
    {
        "id": "gather_wheat",
        "frame_ms": 520,
        "loop": True,
        "family": "gather",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "wide_tool",
        "tool": "sickle",
        "phases": [
            "using a sickle to cut wheat",
            "still holding the sickle while the free hand reaches for wheat",
        ],
    },
    {
        "id": "build",
        "frame_ms": 300,
        "loop": True,
        "family": "hammer",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "kneeling",
        "tool": "hammer",
        "phases": [
            "kneeling builder with hammer raised up",
            "kneeling builder with hammer down",
        ],
    },
    {
        "id": "carry_wood",
        "frame_ms": 180,
        "loop": True,
        "family": "carry_gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "carry": "bundled logs held securely in both arms",
        "phases": [
            "carrying bundled logs while walking, front/near leg forward",
            "carrying bundled logs while walking, rear/far leg forward",
        ],
    },
    {
        "id": "carry_gold",
        "frame_ms": 180,
        "loop": True,
        "family": "carry_gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "carry": "pile of grey stones and yellow gold ore held in both arms",
        "phases": [
            "carrying stones and gold ore while walking, front/near leg forward",
            "carrying stones and gold ore while walking, rear/far leg forward",
        ],
    },
    {
        "id": "carry_berries",
        "frame_ms": 180,
        "loop": True,
        "family": "carry_gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "carry": "basket of red berries held in both arms",
        "phases": [
            "carrying berries while walking, front/near leg forward",
            "carrying berries while walking, rear/far leg forward",
        ],
    },
    {
        "id": "carry_wheat",
        "frame_ms": 180,
        "loop": True,
        "family": "carry_gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "carry": "bundle of wheat held securely in both arms",
        "phases": [
            "carrying wheat while walking, front/near leg forward",
            "carrying wheat while walking, rear/far leg forward",
        ],
    },
    {
        "id": "gather_meat",
        "frame_ms": 520,
        "loop": True,
        "family": "gather",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "kneeling",
        "tool": "knife",
        "phases": [
            "holding a knife while actively cutting or reaching toward meat",
            "still holding the knife while taking meat with the free hand",
        ],
    },
    {
        "id": "carry_meat",
        "frame_ms": 180,
        "loop": True,
        "family": "carry_gait",
        "target_relation": SELF_TILE,
        "fit_profile": "standing",
        "carry": "large cut of meat held in both arms",
        "phases": [
            "carrying meat while walking, front/near leg forward",
            "carrying meat while walking, rear/far leg forward",
        ],
    },
    {
        "id": "club_attack",
        "frame_ms": 260,
        "loop": True,
        "family": "swing",
        "target_relation": ADJACENT_TARGET,
        "fit_profile": "wide_tool",
        "tool": "wooden club",
        "phases": [
            "club at the top of the attack swing, held overhead but still inside the tile",
            "club at the bottom/contact part of the attack swing, still fully inside the tile",
        ],
    },
    {
        "id": "death",
        "frame_ms": 30000,
        "loop": False,
        "hold_last": True,
        "family": "one_shot",
        "target_relation": SELF_TILE,
        "fit_profile": "lying",
        "phases": [
            "dead villager body lying on the ground, not a skeleton",
            "skeleton remains of the same villager in the same ground area, with small clothing scraps",
        ],
    },
]

PEASANT_ACTION_BY_ID = {action["id"]: action for action in PEASANT_ACTIONS}


def fallback_unit_spec(entity: str) -> dict[str, Any]:
    return {
        "schema": "realm.unit_animation_spec.fallback.v1",
        "entity": entity,
        "directions": ["front", "back"],
        "runtime_mirrors_horizontal": True,
        "projection": "isometric terrain diamonds with upright sprites",
        "actions": PEASANT_ACTIONS,
        "source": "python-fallback",
    }


def normalize_unit_spec(raw: dict[str, Any], source: str) -> dict[str, Any]:
    actions = []
    for action in raw.get("actions", []):
        frames = action.get("frames", [])
        phases = action.get("phases")
        if phases is None:
            phases = [str(frame.get("description", "")) for frame in frames]
        durations = action.get("frame_durations_ms")
        if durations is None:
            durations = [int(frame.get("duration_ms", 0)) for frame in frames]
        frame_ms = int(action.get("frame_ms", next((d for d in durations if d > 0), 250)))
        normalized = {
            "id": action.get("id") or action.get("action"),
            "description": action.get("description", ""),
            "frame_ms": frame_ms,
            "loop": bool(action.get("loop", False)),
            "hold_last": bool(action.get("hold_last", False)),
            "transition_after_ms": int(action.get("transition_after_ms", 0)),
            "family": action.get("family", ""),
            "target_relation": action.get("target_relation", SELF_TILE),
            "range_tiles": int(action.get("range_tiles", 0)),
            "fit_profile": action.get("fit_profile", "standing"),
            "phases": phases,
            "frame_durations_ms": durations,
        }
        if action.get("tool"):
            normalized["tool"] = action["tool"]
        if action.get("carry"):
            normalized["carry"] = action["carry"]
        actions.append(normalized)
    raw["actions"] = actions
    raw["source"] = source
    return raw


def default_spec_binary() -> Path | None:
    for candidate in (Path("bin/realm.exe"), Path("bin/realm-gfx"), Path("bin/realm")):
        if candidate.exists():
            return candidate
    return None


def load_code_unit_spec(entity: str, binary: str | None = None) -> dict[str, Any] | None:
    binary_path = Path(binary) if binary else default_spec_binary()
    if not binary_path:
        return None
    result = subprocess.run(
        [str(binary_path), "--dump-animation-spec", entity],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        return None
    return normalize_unit_spec(json.loads(result.stdout), f"code:{binary_path}")


def load_unit_spec(entity: str, source: str = "auto", binary: str | None = None, spec_json: str | None = None) -> dict[str, Any]:
    if spec_json:
        path = Path(spec_json)
        return normalize_unit_spec(json.loads(path.read_text(encoding="utf-8")), f"json:{path}")
    if source in {"auto", "code"}:
        loaded = load_code_unit_spec(entity, binary)
        if loaded is not None:
            return loaded
        if source == "code":
            raise SystemExit("could not load animation spec from game binary; build Realm or pass --spec-source fallback")
    return fallback_unit_spec(entity)


def spec_actions(unit_spec: dict[str, Any] | None = None) -> list[dict[str, Any]]:
    return unit_spec["actions"] if unit_spec else PEASANT_ACTIONS

PEASANT_STATE_DESCRIPTIONS = {
    1: [
        "idle: standing idle, relaxed arms at sides, both feet planted.",
        "walk: walking with the front/near leg forward and the rear/far leg back.",
        "chop_wood: chopping wood at the bottom of the axe swing, axe head low and forward.",
        "mine_gold: mining at the bottom of the pickaxe swing, pickaxe head low and forward.",
        "gather_berries: gathering berries with one hand reaching out toward a basket or bush-side basket.",
        "hoe_soil: hoeing soil with arms outstretched and a long-handled farming hoe extended away from the body; the hoe has a small flat rectangular blade and is not an axe or pickaxe.",
        "gather_wheat: gathering wheat using a sickle, sickle actively cutting wheat.",
        "build: kneeling builder, hammer raised up.",
        "carry_wood: carrying bundled logs while walking, front/near leg forward.",
        "carry_gold: carrying a pile of stones and gold ore while walking, front/near leg forward.",
        "carry_berries: carrying a basket of red berries while walking, front/near leg forward.",
        "carry_wheat: carrying a bundle of wheat while walking, front/near leg forward.",
        "gather_meat: gathering meat while holding a knife, knife actively cutting or reaching toward meat.",
        "carry_meat: carrying a large cut of meat while walking, front/near leg forward.",
        "club_attack: club attack with the club at the top of the swing, held overhead.",
        "death: dead villager body lying on the ground, not a skeleton.",
    ],
    2: [
        "idle: standing with arms crossed.",
        "walk: walking with the rear/far leg forward and the front/near leg back.",
        "chop_wood: axe at the top of the swing.",
        "mine_gold: pickaxe at the top of the swing.",
        "gather_berries: hand in basket with berries.",
        "hoe_soil: arms pulled in after the hoe stroke, still holding the long-handled farming hoe with a small flat rectangular blade; not an axe or pickaxe.",
        "gather_wheat: still holding the sickle while the free hand reaches for wheat.",
        "build: kneeling builder, hammer down.",
        "carry_wood: carrying bundled logs while walking, rear/far leg forward.",
        "carry_gold: carrying stones and gold ore while walking, rear/far leg forward.",
        "carry_berries: carrying berries while walking, rear/far leg forward.",
        "carry_wheat: carrying wheat while walking, rear/far leg forward.",
        "gather_meat: still holding the knife while taking meat with the free hand.",
        "carry_meat: carrying meat while walking, rear/far leg forward.",
        "club_attack: club at the bottom of the swing.",
        "death: skeleton remains of the same villager in the same pose area, with clothing scraps visible enough to read as the same unit.",
    ],
}

PROMPT_PATH = Path(__file__).resolve().parents[1] / "references" / "villager-peasant-sheet.md"


def parse_color(raw: str) -> tuple[int, int, int]:
    value = ImageColor.getrgb(raw)
    if len(value) == 4:
        return value[:3]
    return value


def color_distance(a: tuple[int, int, int], b: tuple[int, int, int]) -> float:
    return math.sqrt(sum((int(a[i]) - int(b[i])) ** 2 for i in range(3)))


def is_key_rgb(rgb: tuple[int, int, int], key: tuple[int, int, int], threshold: float) -> bool:
    if color_distance(rgb, key) <= threshold:
        return True
    r, g, b = rgb
    kr, kg, kb = key
    if kr > 200 and kb > 200 and kg < 80:
        return r > 180 and b > 160 and g < 120 and r + b - (2 * g) > 220
    return False


def is_high_confidence_key_rgb(rgb: tuple[int, int, int], key: tuple[int, int, int]) -> bool:
    r, g, b = rgb
    kr, kg, kb = key
    if kr > 200 and kb > 200 and kg < 80:
        return r >= 220 and b >= 210 and g <= 95 and r + b - (2 * g) >= 300
    return color_distance(rgb, key) <= 18.0


def is_page_background_rgb(rgb: tuple[int, int, int]) -> bool:
    r, g, b = rgb
    return r >= 238 and g >= 238 and b >= 238 and max(r, g, b) - min(r, g, b) <= 14


def grid_boxes(width: int, height: int, cols: int, rows: int) -> list[tuple[int, int, int, int]]:
    boxes: list[tuple[int, int, int, int]] = []
    for row in range(rows):
        for col in range(cols):
            x0 = round(col * width / cols)
            y0 = round(row * height / rows)
            x1 = round((col + 1) * width / cols)
            y1 = round((row + 1) * height / rows)
            side = min(x1 - x0, y1 - y0)
            cx = (x0 + x1) // 2
            cy = (y0 + y1) // 2
            boxes.append((cx - side // 2, cy - side // 2, cx - side // 2 + side, cy - side // 2 + side))
    return boxes


def grid_key_boxes(
    img: Image.Image,
    cols: int,
    rows: int,
    key: tuple[int, int, int],
    threshold: float,
) -> tuple[list[tuple[int, int, int, int]], bool]:
    rgb = img.convert("RGB")
    width, height = rgb.size
    pix = rgb.load()
    boxes: list[tuple[int, int, int, int]] = []
    for row in range(rows):
        for col in range(cols):
            gx0 = round(col * width / cols)
            gy0 = round(row * height / rows)
            gx1 = round((col + 1) * width / cols)
            gy1 = round((row + 1) * height / rows)
            min_x, min_y = gx1, gy1
            max_x, max_y = gx0, gy0
            count = 0
            for y in range(gy0, gy1):
                for x in range(gx0, gx1):
                    if is_key_rgb(pix[x, y], key, threshold):
                        min_x = min(min_x, x)
                        min_y = min(min_y, y)
                        max_x = max(max_x, x)
                        max_y = max(max_y, y)
                        count += 1
            cell_area = max(1, (gx1 - gx0) * (gy1 - gy0))
            if count < cell_area * 0.18:
                return [], False
            side = max(max_x - min_x + 1, max_y - min_y + 1)
            cx = (min_x + max_x + 1) // 2
            cy = (min_y + max_y + 1) // 2
            x0 = max(0, cx - side // 2)
            y0 = max(0, cy - side // 2)
            x1 = min(width, x0 + side)
            y1 = min(height, y0 + side)
            if x1 - x0 != side:
                x0 = max(0, x1 - side)
            if y1 - y0 != side:
                y0 = max(0, y1 - side)
            boxes.append((x0, y0, x1, y1))
    return boxes, len(boxes) == cols * rows


def find_magenta_square_boxes(
    img: Image.Image,
    cols: int,
    rows: int,
    key: tuple[int, int, int],
    threshold: float,
) -> tuple[list[tuple[int, int, int, int]], str]:
    boxes, ok = grid_key_boxes(img, cols, rows, key, threshold)
    if ok:
        return boxes, "grid-key-bins"

    rgb = img.convert("RGB")
    width, height = rgb.size
    pix = rgb.load()
    mask = [bytearray(width) for _ in range(height)]
    for y in range(height):
        row = mask[y]
        for x in range(width):
            if is_key_rgb(pix[x, y], key, threshold):
                row[x] = 1

    visited = bytearray(width * height)
    components: list[tuple[int, int, int, int, int]] = []
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            if not mask[y][x] or visited[idx]:
                continue
            q: deque[tuple[int, int]] = deque([(x, y)])
            visited[idx] = 1
            min_x = max_x = x
            min_y = max_y = y
            area = 0
            while q:
                px, py = q.popleft()
                area += 1
                min_x = min(min_x, px)
                max_x = max(max_x, px)
                min_y = min(min_y, py)
                max_y = max(max_y, py)
                for nx, ny in ((px + 1, py), (px - 1, py), (px, py + 1), (px, py - 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    nidx = ny * width + nx
                    if mask[ny][nx] and not visited[nidx]:
                        visited[nidx] = 1
                        q.append((nx, ny))
            if area >= 256:
                components.append((min_x, min_y, max_x + 1, max_y + 1, area))

    expected = cols * rows
    image_area = width * height
    square_components = []
    for box in components:
        x0, y0, x1, y1, area = box
        bw = x1 - x0
        bh = y1 - y0
        if area > image_area * 0.45:
            continue
        if min(bw, bh) <= 0:
            continue
        ratio = max(bw, bh) / min(bw, bh)
        if ratio <= 1.25:
            side = max(bw, bh)
            cx = (x0 + x1) // 2
            cy = (y0 + y1) // 2
            sx0 = max(0, cx - side // 2)
            sy0 = max(0, cy - side // 2)
            square_components.append((sx0, sy0, min(width, sx0 + side), min(height, sy0 + side)))

    if len(square_components) >= expected:
        square_components.sort(key=lambda b: (b[1], b[0]))
        return square_components[:expected], "magenta-components"

    return grid_boxes(width, height, cols, rows), "grid-fallback"


def transparent_key_from_edges(
    crop: Image.Image,
    key: tuple[int, int, int],
    threshold: float,
) -> tuple[Image.Image, dict[str, int]]:
    rgba = crop.convert("RGBA")
    width, height = rgba.size
    pix = rgba.load()
    visited = bytearray(width * height)
    q: deque[tuple[int, int]] = deque()

    def maybe_seed(x: int, y: int) -> None:
        idx = y * width + x
        if visited[idx]:
            return
        rgb = pix[x, y][:3]
        if is_key_rgb(rgb, key, threshold) or is_page_background_rgb(rgb):
            visited[idx] = 1
            q.append((x, y))

    for x in range(width):
        maybe_seed(x, 0)
        maybe_seed(x, height - 1)
    for y in range(height):
        maybe_seed(0, y)
        maybe_seed(width - 1, y)

    removed = 0
    while q:
        x, y = q.popleft()
        r, g, b, _a = pix[x, y]
        pix[x, y] = (r, g, b, 0)
        removed += 1
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nx < 0 or ny < 0 or nx >= width or ny >= height:
                continue
            idx = ny * width + nx
            if visited[idx]:
                continue
            rgb = pix[nx, ny][:3]
            if is_key_rgb(rgb, key, threshold) or is_page_background_rgb(rgb):
                visited[idx] = 1
                q.append((nx, ny))

    residual = 0
    opaque = 0
    stranded_removed = 0
    edge_fringe_removed = 0
    for y in range(height):
        for x in range(width):
            if pix[x, y][3] > 0:
                if is_high_confidence_key_rgb(pix[x, y][:3], key):
                    r, g, b, _a = pix[x, y]
                    pix[x, y] = (r, g, b, 0)
                    stranded_removed += 1

    for _ in range(2):
        to_clear: list[tuple[int, int]] = []
        for y in range(height):
            for x in range(width):
                if pix[x, y][3] == 0 or not is_key_rgb(pix[x, y][:3], key, threshold):
                    continue
                touches_clear = False
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height or pix[nx, ny][3] == 0:
                        touches_clear = True
                        break
                if touches_clear:
                    to_clear.append((x, y))
        if not to_clear:
            break
        for x, y in to_clear:
            r, g, b, _a = pix[x, y]
            pix[x, y] = (r, g, b, 0)
        edge_fringe_removed += len(to_clear)

    for y in range(height):
        for x in range(width):
            if pix[x, y][3] > 0:
                opaque += 1
                if is_key_rgb(pix[x, y][:3], key, threshold):
                    residual += 1

    return rgba, {
        "transparent_key_pixels": removed + stranded_removed + edge_fringe_removed,
        "stranded_key_pixels": stranded_removed,
        "edge_fringe_key_pixels": edge_fringe_removed,
        "residual_key_pixels": residual,
        "opaque_pixels": opaque,
    }


def team_mask_and_neutral_base(
    rgba: Image.Image,
    team_color: tuple[int, int, int],
    threshold: float,
) -> tuple[Image.Image, Image.Image, dict[str, int]]:
    base = rgba.copy().convert("RGBA")
    mask = Image.new("RGBA", base.size, (255, 255, 255, 0))
    bpix = base.load()
    mpix = mask.load()
    target_h, target_s, target_v = colorsys.rgb_to_hsv(*(c / 255.0 for c in team_color))
    masked = 0
    for y in range(base.height):
        for x in range(base.width):
            r, g, b, a = bpix[x, y]
            if a == 0:
                continue
            h, s, v = colorsys.rgb_to_hsv(r / 255.0, g / 255.0, b / 255.0)
            hue_delta = min(abs(h - target_h), 1.0 - abs(h - target_h)) * 360.0
            dist = color_distance((r, g, b), team_color)
            if hue_delta <= threshold and s >= max(0.18, target_s * 0.35) and v >= 0.12 and dist <= 190:
                strength = max(0.0, 1.0 - hue_delta / max(1.0, threshold))
                alpha = int(a * min(1.0, 0.38 + strength * 0.72))
                shade = int(max(70, min(255, v * 255)))
                mpix[x, y] = (shade, shade, shade, alpha)
                lum = int(0.2126 * r + 0.7152 * g + 0.0722 * b)
                neutral = int(max(35, min(230, lum)))
                bpix[x, y] = (neutral, neutral, neutral, a)
                masked += 1
    return base, mask, {"team_mask_pixels": masked}


def resize_frame(img: Image.Image, size: int) -> Image.Image:
    if img.width == size and img.height == size:
        return img
    return img.resize((size, size), Image.Resampling.LANCZOS)


def clear_resized_edge_haze(img: Image.Image) -> Image.Image:
    rgba = img.convert("RGBA")
    width, height = rgba.size
    pix = rgba.load()
    visited = bytearray(width * height)
    q: deque[tuple[int, int]] = deque()

    def is_haze(x: int, y: int) -> bool:
        r, g, b, a = pix[x, y]
        if a == 0:
            return True
        if a >= 248:
            return False
        return r >= 215 and b >= 215 and g >= 90

    def seed(x: int, y: int) -> None:
        idx = y * width + x
        if not visited[idx] and is_haze(x, y):
            visited[idx] = 1
            q.append((x, y))

    for x in range(width):
        seed(x, 0)
        seed(x, height - 1)
    for y in range(height):
        seed(0, y)
        seed(width - 1, y)

    while q:
        x, y = q.popleft()
        r, g, b, _a = pix[x, y]
        pix[x, y] = (r, g, b, 0)
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nx < 0 or ny < 0 or nx >= width or ny >= height:
                continue
            idx = ny * width + nx
            if not visited[idx] and is_haze(nx, ny):
                visited[idx] = 1
                q.append((nx, ny))
    return rgba


def action_spec(action_id: str, unit_spec: dict[str, Any] | None = None) -> dict[str, Any]:
    for action in spec_actions(unit_spec):
        if action["id"] == action_id:
            return action
    known = ", ".join(action["id"] for action in spec_actions(unit_spec))
    raise SystemExit(f"unknown peasant action '{action_id}'. Known actions: {known}")


def action_index(action_id: str, unit_spec: dict[str, Any] | None = None) -> int:
    for index, spec in enumerate(spec_actions(unit_spec)):
        if spec["id"] == action_id:
            return index
    action_spec(action_id, unit_spec)
    return 0


def phase_text(spec: dict[str, Any], frame: int) -> str:
    phases = spec.get("phases", [])
    if not phases:
        return f"animation frame {frame}"
    return str(phases[min(frame, len(phases) - 1)])


def frame_duration_ms(spec: dict[str, Any], frame: int) -> int:
    durations = spec.get("frame_durations_ms") or []
    if frame < len(durations) and int(durations[frame]) > 0:
        return int(durations[frame])
    return int(spec.get("frame_ms", 250))


def sentence(text: str) -> str:
    text = text.strip()
    return text if text.endswith((".", "!", "?")) else text + "."


def direction_text(direction: str) -> str:
    if direction == "front":
        return (
            "Realm front view: three-quarter front RTS sprite angle like the reference crop, "
            "body and face turned about 30-45 degrees toward screen right, one cheek and side of "
            "the helmet visible, not a straight-on symmetrical mascot pose"
        )
    if direction == "back":
        return (
            "Realm back view: rear-right three-quarter RTS sprite angle matching the same diagonal "
            "rotation as the front pose, facing away toward screen right; the shoulders, belt, hem, "
            "and boots form a visible diagonal rather than a horizontal straight-back view. Show the "
            "back plus the screen-right side of the helmet, tunic, pouch, sleeve, and boot closer to "
            "the camera, with the far side partly hidden. Not a flat straight-on rear diagram."
        )
    raise SystemExit("direction must be front or back")


def alpha_bbox(img: Image.Image, alpha_threshold: int = 8) -> tuple[int, int, int, int] | None:
    rgba = img.convert("RGBA")
    pix = rgba.load()
    min_x, min_y = rgba.width, rgba.height
    max_x, max_y = -1, -1
    for y in range(rgba.height):
        for x in range(rgba.width):
            if pix[x, y][3] > alpha_threshold:
                min_x = min(min_x, x)
                min_y = min(min_y, y)
                max_x = max(max_x, x)
                max_y = max(max_y, y)
    if max_x < min_x or max_y < min_y:
        return None
    return (min_x, min_y, max_x + 1, max_y + 1)


def bbox_record(box: tuple[int, int, int, int] | None) -> dict[str, int] | None:
    if box is None:
        return None
    x0, y0, x1, y1 = box
    return {"x": x0, "y": y0, "w": x1 - x0, "h": y1 - y0}


def normalize_sprite_canvas(
    transparent: Image.Image,
    size: int,
    fit_profile_id: str,
) -> tuple[Image.Image, dict[str, Any]]:
    profile = FIT_PROFILES[fit_profile_id]
    source = transparent.convert("RGBA")
    source_box = alpha_bbox(source)
    if source_box is None:
        return Image.new("RGBA", (size, size), (0, 0, 0, 0)), {
            "source_bbox": None,
            "final_bbox": None,
            "anchor": profile["anchor"],
            "anchor_offset": [0, 0],
            "scale": 1.0,
            "fit_profile": fit_profile_id,
        }

    crop = source.crop(source_box)
    max_w = max(1, int(profile["max_w"] * size / 48))
    max_h = max(1, int(profile["max_h"] * size / 48))
    scale = min(max_w / crop.width, max_h / crop.height)
    out_w = max(1, int(round(crop.width * scale)))
    out_h = max(1, int(round(crop.height * scale)))
    resized = crop.resize((out_w, out_h), Image.Resampling.LANCZOS)

    anchor_x = int(profile["anchor"][0] * size / 48)
    anchor_y = int(profile["anchor"][1] * size / 48)
    padding = int(profile.get("padding", 0) * size / 48)
    x = anchor_x - out_w // 2
    y = anchor_y - out_h
    x = min(max(padding, x), max(padding, size - padding - out_w))
    y = min(max(padding, y), max(padding, size - padding - out_h))

    canvas = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    canvas.alpha_composite(resized, (x, y))
    canvas = clear_resized_edge_haze(canvas)
    final_box = alpha_bbox(canvas)
    if final_box is None:
        anchor_offset = [0, 0]
    else:
        fx0, _fy0, fx1, fy1 = final_box
        anchor_offset = [int(round(((fx0 + fx1) / 2) - anchor_x)), int(fy1 - anchor_y)]

    return canvas, {
        "source_bbox": bbox_record(source_box),
        "final_bbox": bbox_record(final_box),
        "anchor": [anchor_x, anchor_y],
        "anchor_offset": anchor_offset,
        "scale": scale,
        "fit_profile": fit_profile_id,
    }


def residual_key_pixels(img: Image.Image, key: tuple[int, int, int], threshold: float) -> int:
    rgba = img.convert("RGBA")
    pix = rgba.load()
    count = 0
    for y in range(rgba.height):
        for x in range(rgba.width):
            if pix[x, y][3] > 0 and is_key_rgb(pix[x, y][:3], key, threshold):
                count += 1
    return count


def clear_residual_key_artifacts(img: Image.Image, key: tuple[int, int, int], threshold: float) -> Image.Image:
    rgba = img.convert("RGBA")
    pix = rgba.load()
    for _ in range(3):
        to_clear: list[tuple[int, int]] = []
        for y in range(rgba.height):
            for x in range(rgba.width):
                r, g, b, a = pix[x, y]
                if a == 0 or not is_key_rgb((r, g, b), key, threshold):
                    continue
                touches_clear = False
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if nx < 0 or ny < 0 or nx >= rgba.width or ny >= rgba.height or pix[nx, ny][3] == 0:
                        touches_clear = True
                        break
                if touches_clear or is_high_confidence_key_rgb((r, g, b), key) or a < 220:
                    to_clear.append((x, y))
        if not to_clear:
            break
        for x, y in to_clear:
            r, g, b, _a = pix[x, y]
            pix[x, y] = (r, g, b, 0)
    return rgba


def process_tile_image(
    raw_tile: Image.Image,
    spec: dict[str, Any],
    size: int,
    key_color: tuple[int, int, int],
    key_threshold: float,
    team_color: tuple[int, int, int],
    team_threshold: float,
) -> tuple[Image.Image, Image.Image, dict[str, Any]]:
    transparent, key_stats = transparent_key_from_edges(raw_tile, key_color, key_threshold)
    normalized, fit_stats = normalize_sprite_canvas(transparent, size, spec["fit_profile"])
    normalized = clear_residual_key_artifacts(normalized, key_color, key_threshold)
    key_stats["residual_key_pixels"] = residual_key_pixels(normalized, key_color, key_threshold)
    base, team_mask, team_stats = team_mask_and_neutral_base(normalized, team_color, team_threshold)
    return base, team_mask, {**key_stats, **fit_stats, **team_stats}


def frame_workbench_dir(root: Path, entity: str, action_id: str, direction: str, frame: int) -> Path:
    return root / entity / action_id / direction / f"frame_{frame:02d}"


def default_batch_slots(entity: str, action_id: str) -> list[tuple[str, int]]:
    if entity == "peasant" and action_id == "idle":
        return [("front", 0), ("back", 0), ("front", 1), ("back", 1)]
    raise SystemExit("pass --slot direction:frame for this batch source")


def parse_batch_slot(raw: str) -> tuple[str, int]:
    parts = raw.strip().replace("/", ":").split(":")
    if len(parts) != 2:
        raise SystemExit(f"invalid --slot {raw!r}; expected direction:frame, for example front:0")
    direction = parts[0].strip().lower()
    if direction not in {"front", "back"}:
        raise SystemExit(f"invalid direction in --slot {raw!r}; expected front or back")
    frame_raw = parts[1].strip().lower().removeprefix("frame_")
    try:
        frame = int(frame_raw)
    except ValueError as exc:
        raise SystemExit(f"invalid frame in --slot {raw!r}; expected an integer") from exc
    if frame < 0:
        raise SystemExit(f"invalid frame in --slot {raw!r}; expected a non-negative integer")
    return direction, frame


def panel_content_bbox(
    panel: Image.Image,
    key_color: tuple[int, int, int],
    key_threshold: float,
    background_threshold: float = 34.0,
    alpha_threshold: int = 8,
) -> tuple[int, int, int, int] | None:
    rgba = panel.convert("RGBA")
    pix = rgba.load()
    corners = [
        pix[0, 0][:3],
        pix[rgba.width - 1, 0][:3],
        pix[0, rgba.height - 1][:3],
        pix[rgba.width - 1, rgba.height - 1][:3],
    ]
    min_x, min_y = rgba.width, rgba.height
    max_x, max_y = -1, -1
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pix[x, y]
            if a <= alpha_threshold:
                continue
            rgb = (r, g, b)
            if is_key_rgb(rgb, key_color, key_threshold):
                continue
            if any(color_distance(rgb, corner) <= background_threshold for corner in corners):
                continue
            min_x = min(min_x, x)
            min_y = min(min_y, y)
            max_x = max(max_x, x)
            max_y = max(max_y, y)
    if max_x < min_x or max_y < min_y:
        return None
    return (min_x, min_y, max_x + 1, max_y + 1)


def validate_batch_panel(
    panel: Image.Image,
    label: str,
    key_color: tuple[int, int, int],
    key_threshold: float,
    min_content_pixels: int,
    min_margin: int,
) -> dict[str, Any]:
    bbox = panel_content_bbox(panel, key_color, key_threshold)
    if bbox is None:
        raise SystemExit(f"{label} has no detectable sprite content")
    x0, y0, x1, y1 = bbox
    content_pixels = 0
    rgba = panel.convert("RGBA")
    pix = rgba.load()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if pix[x, y][3] > 8:
                content_pixels += 1
    if content_pixels < min_content_pixels:
        raise SystemExit(f"{label} has too little detectable content: {content_pixels} pixels")
    margins = {
        "left": x0,
        "top": y0,
        "right": panel.width - x1,
        "bottom": panel.height - y1,
    }
    tight = {name: value for name, value in margins.items() if value < min_margin}
    if tight:
        details = ", ".join(f"{name}={value}" for name, value in tight.items())
        raise SystemExit(f"{label} content is too close to the panel edge: {details}")
    return {
        "content_bbox": bbox_record(bbox),
        "content_pixels": content_pixels,
        "margins": margins,
    }


def reference_slot_path(workbench: Path, entity: str, view: str, state: int, slot_index: int, action_id: str) -> Path:
    return workbench / entity / "reference" / f"{view}_state_{state}" / f"{slot_index + 1:02d}_{action_id}.png"


def slot_index_from_label(label: str, unit_spec: dict[str, Any] | None = None) -> int:
    raw = label.strip().lower().replace("_", "-")
    actions = spec_actions(unit_spec)
    for index, spec in enumerate(actions):
        if raw == spec["id"].replace("_", "-") or raw == spec["id"]:
            return index
    compact = raw.replace(" ", "").replace("-", "")
    compact = compact.replace("column", "c").replace("col", "c").replace("row", "r")
    if compact.isdigit():
        value = int(compact)
        if 1 <= value <= len(actions):
            return value - 1
    if compact.startswith("r") and "c" in compact:
        row_text, col_text = compact[1:].split("c", 1)
        if row_text.isdigit() and col_text.isdigit():
            row = int(row_text) - 1
            col = int(col_text) - 1
            if 0 <= row < 4 and 0 <= col < 4:
                return row * 4 + col
    words = raw.replace("-", " ").split()
    vertical = {"top": 0, "upper": 0, "first": 0, "second": 1, "third": 2, "bottom": 3, "lower": 3, "last": 3}
    horizontal = {"left": 0, "first": 0, "second": 1, "third": 2, "right": 3, "last": 3}
    row = next((vertical[word] for word in words if word in vertical), None)
    col = next((horizontal[word] for word in words if word in horizontal), None)
    if row is not None and col is not None:
        return row * 4 + col
    known = ", ".join(f"{i + 1}:{spec['id']}" for i, spec in enumerate(actions))
    raise SystemExit(f"could not parse reference slot '{label}'. Use top-left, row1-col1, 1-16, or action id. Known slots: {known}")


def slot_action(unit_spec: dict[str, Any], slot_index: int) -> dict[str, Any]:
    actions = spec_actions(unit_spec)
    if slot_index < 0 or slot_index >= len(actions):
        raise SystemExit(f"slot index {slot_index + 1} is outside the action list")
    return actions[slot_index]


def resolve_reference_slot(
    args: argparse.Namespace,
    unit_spec: dict[str, Any],
) -> tuple[str | None, int | None, str | None]:
    if not getattr(args, "reference_slot", None):
        return None, None, None
    view = getattr(args, "reference_view", None)
    state = getattr(args, "reference_state", None)
    if not view or not state:
        raise SystemExit("--reference-slot requires --reference-view and --reference-state")
    slot_index = slot_index_from_label(args.reference_slot, unit_spec)
    spec = slot_action(unit_spec, slot_index)
    frame = int(state) - 1
    ref_path = reference_slot_path(Path(args.workbench), args.entity, view, int(state), slot_index, spec["id"])
    return spec["id"], frame, str(ref_path)


def load_single_source_tile(
    path: Path,
    key: tuple[int, int, int],
    threshold: float,
) -> tuple[Image.Image, str]:
    img = Image.open(path).convert("RGBA")
    if img.width == img.height:
        return img, "single-image"
    boxes, method = find_magenta_square_boxes(img, 1, 1, key, threshold)
    if not boxes:
        return img, "single-image"
    return img.crop(boxes[0]), method


def frame_prompt_text(
    entity: str,
    action_id: str,
    direction: str,
    frame: int,
    references: list[str] | None = None,
    unit_spec: dict[str, Any] | None = None,
) -> str:
    if entity.lower() not in {"peasant", "villager"}:
        raise SystemExit("only the peasant/villager prompt is bundled so far")
    spec = action_spec(action_id, unit_spec)
    refs = references or []
    ref_lines = "\n".join(f"- Use visual reference: {ref}" for ref in refs) or "- No external reference image was provided; use the peasant registry and any user-pasted reference."
    tool_line = f"\nTool/object constraint: show a {spec['tool']}." if "tool" in spec else ""
    carry_line = f"\nCarried object constraint: keep the {spec['carry']} stable and readable across gait phases." if "carry" in spec else ""
    target_line = (
        "The sprite is anchored on the unit's own tile. The work target is adjacent; include only small carried/tool cues, not a full separate resource tile, unless a reference explicitly includes it."
        if spec["target_relation"] == ADJACENT_TARGET
        else "The sprite belongs on the unit's own tile; do not include a separate adjacent target tile."
    )
    return f"""Use case: stylized-concept
Asset type: single production source sprite for a 2D RTS game tileset
Primary request: Generate exactly one square 1024x1024 image for Realm {entity} action `{action_id}`, direction `{direction}`, frame {frame}.

Style reference: rounded cartoon RTS sprite, compact medieval male peasant/villager, blue helmet and blue tunic accents for player colour, brown leather boots and belt, tan skin, dark beard, thick clean outline, readable at 48x48 pixels. Keep the same camera angle, scale, lighting, palette, outline thickness, outfit, helmet, beard, and proportions as the supplied peasant references.

Action meaning: {spec.get("description", "")}

Reference continuity:
{ref_lines}

Canvas: the entire image is one flat, perfectly uniform #ff00ff magenta square tile background. No white gutter, no grid, no transparency, no labels, no text, no numbers, no watermark. Put exactly one complete sprite on the magenta tile.

Pose: {direction_text(direction)}. The exact animation phase is: {sentence(phase_text(spec, frame))}{tool_line}{carry_line}

Tile-fit requirements: keep the complete sprite, all limbs, held objects, tools, weapons, baskets, logs, meat, wheat, and motion arcs fully inside the magenta square with generous padding. If the pose would be too wide, make the pose more compact rather than letting anything leave the tile. The sprite may be lower in the frame for lying/dead poses, but it must still fit fully inside the tile.

Gameplay relation: {target_line}

Colour constraints: do not use magenta anywhere in the artwork itself, including berries, meat highlights, skin blush, cloth trim, shadows, or tool highlights. The only magenta must be the flat background key colour."""


def extract_tiles_from_sheet(
    path: Path,
    cols: int,
    rows: int,
    key: tuple[int, int, int],
    key_threshold: float,
) -> tuple[list[Image.Image], str]:
    img = Image.open(path).convert("RGBA")
    boxes, method = find_magenta_square_boxes(img, cols, rows, key, key_threshold)
    tiles = [img.crop(box) for box in boxes]
    return tiles, method


def source_args(args: argparse.Namespace) -> dict[tuple[str, int], Path]:
    raw = {
        ("front", 0): args.front_state_1,
        ("front", 1): args.front_state_2,
        ("back", 0): args.back_state_1,
        ("back", 1): args.back_state_2,
    }
    return {key: Path(value) for key, value in raw.items() if value}


def relative_to(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def copy_missing_source(
    sheets: dict[tuple[str, int], list[Image.Image]],
    methods: dict[tuple[str, int], str],
    assumptions: list[str],
) -> None:
    if not sheets:
        raise SystemExit("extract requires at least one source image")
    for direction in ("front", "back"):
        for state in (0, 1):
            key = (direction, state)
            if key in sheets:
                continue
            same_dir = (direction, 1 - state)
            other_dir = ("back" if direction == "front" else "front", state)
            other_any = ("back" if direction == "front" else "front", 1 - state)
            source_key = other_dir if other_dir in sheets else same_dir if same_dir in sheets else other_any
            if source_key not in sheets:
                source_key = next(iter(sheets))
            sheets[key] = [tile.copy() for tile in sheets[source_key]]
            methods[key] = "duplicated-from-" + source_key[0] + "-state-" + str(source_key[1] + 1)
            assumptions.append(
                f"{direction}_state_{state + 1} duplicated from {source_key[0]}_state_{source_key[1] + 1}"
            )


def build_manifest(args: argparse.Namespace, assumptions: list[str], unit_spec: dict[str, Any] | None = None) -> dict[str, Any]:
    actions = {}
    for spec in spec_actions(unit_spec):
        entry = {
            "frame_ms": spec["frame_ms"],
            "loop": spec["loop"],
            "hold_last": spec.get("hold_last", False),
            "description": spec.get("description", ""),
            "family": spec["family"],
            "target_relation": spec["target_relation"],
            "range_tiles": spec.get("range_tiles", 0),
            "fit_profile": spec["fit_profile"],
            "anchor": FIT_PROFILES[spec["fit_profile"]]["anchor"],
            "directions": {"front": [], "back": []},
        }
        durations = spec.get("frame_durations_ms") or []
        if durations:
            entry["frame_durations_ms"] = durations
        if "transition_after_ms" in spec:
            entry["transition_after_ms"] = spec["transition_after_ms"]
        actions[spec["id"]] = entry
    return {
        "schema": "realm.entity_tileset.v1",
        "entity": args.entity,
        "sprite_size": args.size,
        "team_color_source": getattr(args, "team_color", "#0088cc"),
        "source_key_color": getattr(args, "magenta", "#ff00ff"),
        "directions": ["front", "back"],
        "actions": actions,
        "assumptions": assumptions,
        "source_workflow": getattr(args, "source_workflow", "sheet"),
        "animation_spec_source": unit_spec.get("source", "python-fallback") if unit_spec else "python-fallback",
    }


def command_extract(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("this first extractor knows the peasant/villager 16-square action order")
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    actions = spec_actions(unit_spec)

    key_color = parse_color(args.magenta)
    team_color = parse_color(args.team_color)
    sheets: dict[tuple[str, int], list[Image.Image]] = {}
    methods: dict[tuple[str, int], str] = {}
    assumptions: list[str] = []
    for source_key, source_path in source_args(args).items():
        tiles, method = extract_tiles_from_sheet(source_path, args.cols, args.rows, key_color, args.magenta_threshold)
        expected = args.cols * args.rows
        if len(tiles) < expected:
            raise SystemExit(f"{source_path} yielded {len(tiles)} tiles, expected {expected}")
        sheets[source_key] = tiles[:expected]
        methods[source_key] = method

    copy_missing_source(sheets, methods, assumptions)

    out_root = Path(args.out) / entity
    if out_root.exists() and args.clean:
        shutil.rmtree(out_root)
    out_root.mkdir(parents=True, exist_ok=True)

    manifest = build_manifest(args, assumptions, unit_spec)
    qa_frames: list[dict[str, Any]] = []

    for action_index, action in enumerate(actions):
        action_id = action["id"]
        for direction in ("front", "back"):
            frames = []
            for state in (0, 1):
                raw_tile = sheets[(direction, state)][action_index]
                base, team_mask, stats = process_tile_image(
                    raw_tile,
                    action,
                    args.size,
                    key_color,
                    args.magenta_threshold,
                    team_color,
                    args.team_threshold,
                )
                action_dir = out_root / action_id / direction
                action_dir.mkdir(parents=True, exist_ok=True)
                base_path = action_dir / f"frame_{state:02d}_base.png"
                mask_path = action_dir / f"frame_{state:02d}_teammask.png"
                base.save(base_path)
                team_mask.save(mask_path)
                frame_entry = {
                    "base": relative_to(base_path, out_root),
                    "team_mask": relative_to(mask_path, out_root),
                    "source_state": state + 1,
                    "phase": phase_text(action, state),
                    "duration_ms": frame_duration_ms(action, state),
                    "source_bbox": stats.get("source_bbox"),
                    "final_bbox": stats.get("final_bbox"),
                    "anchor_offset": stats.get("anchor_offset"),
                    "scale": stats.get("scale"),
                    "manual_review": "pending",
                }
                frames.append(frame_entry)
                qa_frames.append(
                    {
                        "action": action_id,
                        "direction": direction,
                        "frame": state,
                        "base": base_path,
                        "team_mask": mask_path,
                        "method": methods[(direction, state)],
                        **stats,
                    }
                )
            manifest["actions"][action_id]["directions"][direction] = frames

    manifest["source_detection"] = {f"{k[0]}_state_{k[1] + 1}": v for k, v in sorted(methods.items())}
    manifest_path = out_root / "manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    review_dir = Path(args.review_out)
    write_review(manifest_path, review_dir, qa_frames)
    print(f"wrote manifest: {manifest_path}")
    print(f"wrote review: {review_dir / 'review.html'}")


def checkerboard(size: tuple[int, int], cell: int = 8) -> Image.Image:
    img = Image.new("RGBA", size, (255, 255, 255, 255))
    draw = ImageDraw.Draw(img)
    for y in range(0, size[1], cell):
        for x in range(0, size[0], cell):
            col = (218, 218, 218, 255) if ((x // cell) + (y // cell)) % 2 else (245, 245, 245, 255)
            draw.rectangle((x, y, x + cell - 1, y + cell - 1), fill=col)
    return img


def composite_preview(path: Path, size: int) -> Image.Image:
    base = Image.open(path).convert("RGBA").resize((size, size), Image.Resampling.NEAREST)
    bg = checkerboard((size, size))
    bg.alpha_composite(base)
    return bg


def team_colour_preview(base_path: Path, mask_path: Path, size: int, team_color: tuple[int, int, int]) -> Image.Image:
    base = Image.open(base_path).convert("RGBA").resize((size, size), Image.Resampling.NEAREST)
    mask = Image.open(mask_path).convert("RGBA").resize((size, size), Image.Resampling.NEAREST)
    out = compose_team_colour(base, mask, team_color)
    bg = checkerboard((size, size))
    bg.alpha_composite(out)
    return bg


def alpha_preview(path: Path, size: int) -> Image.Image:
    base = Image.open(path).convert("RGBA").resize((size, size), Image.Resampling.NEAREST)
    alpha = base.getchannel("A")
    out = Image.new("RGBA", (size, size), (20, 23, 27, 255))
    pix = out.load()
    apix = alpha.load()
    for y in range(size):
        for x in range(size):
            a = apix[x, y]
            if a:
                pix[x, y] = (a, a, a, 255)
    return out


def normalize_bbox_value(value: Any) -> dict[str, int] | None:
    if not isinstance(value, dict):
        return None
    try:
        return {
            "x": int(value["x"]),
            "y": int(value["y"]),
            "w": int(value["w"]),
            "h": int(value["h"]),
        }
    except (KeyError, TypeError, ValueError):
        return None


def bbox_anchor_overlay_preview(
    base_path: Path,
    mask_path: Path,
    size: int,
    team_color: tuple[int, int, int],
    row: dict[str, Any],
) -> Image.Image:
    out = team_colour_preview(base_path, mask_path, size, team_color)
    draw = ImageDraw.Draw(out)
    source_size = Image.open(base_path).size[0]
    scale = size / max(1, source_size)
    bbox = normalize_bbox_value(row.get("final_bbox"))
    if bbox:
        x0 = int(round(bbox["x"] * scale))
        y0 = int(round(bbox["y"] * scale))
        x1 = int(round((bbox["x"] + bbox["w"]) * scale)) - 1
        y1 = int(round((bbox["y"] + bbox["h"]) * scale)) - 1
        draw.rectangle((x0, y0, x1, y1), outline=(255, 88, 88, 255), width=2)
    anchor = row.get("anchor")
    if isinstance(anchor, (list, tuple)) and len(anchor) == 2:
        ax = int(round(int(anchor[0]) * scale))
        ay = int(round(int(anchor[1]) * scale))
        draw.line((ax - 5, ay, ax + 5, ay), fill=(255, 238, 80, 255), width=1)
        draw.line((ax, ay - 5, ax, ay + 5), fill=(255, 238, 80, 255), width=1)
        draw.line((0, ay, size - 1, ay), fill=(255, 238, 80, 90), width=1)
    return out


def compose_team_colour(base: Image.Image, mask: Image.Image, team_color: tuple[int, int, int]) -> Image.Image:
    base = base.convert("RGBA")
    mask = mask.convert("RGBA")
    out = base.copy()
    bpix = out.load()
    mpix = mask.load()
    for y in range(out.height):
        for x in range(out.width):
            br, bg, bb, ba = bpix[x, y]
            mr, _mg, _mb, ma = mpix[x, y]
            if ba == 0 or ma == 0:
                continue
            shade = mr / 255.0
            alpha = ma / 255.0
            tr = int(team_color[0] * shade)
            tg = int(team_color[1] * shade)
            tb = int(team_color[2] * shade)
            bpix[x, y] = (
                int(br * (1.0 - alpha) + tr * alpha),
                int(bg * (1.0 - alpha) + tg * alpha),
                int(bb * (1.0 - alpha) + tb * alpha),
                ba,
            )
    return out


def isometric_tile_preview(
    base_path: Path,
    mask_path: Path,
    team_color: tuple[int, int, int],
    anchor: list[int] | tuple[int, int] | None = None,
) -> Image.Image:
    canvas = Image.new("RGBA", (128, 112), (31, 35, 40, 255))
    draw = ImageDraw.Draw(canvas)
    cx, cy = 64, 74
    hw, hh = 48, 24
    draw.polygon([(cx, cy - hh), (cx + hw, cy), (cx, cy + hh), (cx - hw, cy)], fill=(70, 106, 70, 255))
    draw.line([(cx, cy - hh), (cx + hw, cy), (cx, cy + hh), (cx - hw, cy), (cx, cy - hh)], fill=(175, 205, 155, 230), width=1)
    draw.line([(cx - hw, cy), (cx + hw, cy)], fill=(255, 255, 255, 75), width=1)
    draw.line([(cx, cy - hh), (cx, cy + hh)], fill=(255, 255, 255, 60), width=1)

    base = Image.open(base_path).convert("RGBA")
    mask = Image.open(mask_path).convert("RGBA")
    sprite = compose_team_colour(base, mask, team_color)
    scale = 1.55
    sw = max(1, int(round(sprite.width * scale)))
    sh = max(1, int(round(sprite.height * scale)))
    sprite = sprite.resize((sw, sh), Image.Resampling.NEAREST)
    ax, ay = anchor if anchor else (24, 39)
    px = int(round(cx - ax * scale))
    py = int(round(cy - ay * scale))
    canvas.alpha_composite(sprite, (px, py))
    draw.rectangle((px, py, px + sw - 1, py + sh - 1), outline=(255, 255, 255, 45), width=1)
    draw.ellipse((cx - 2, cy - 2, cx + 2, cy + 2), fill=(255, 238, 80, 230))
    return canvas


def transparent_holes(path: Path) -> int:
    img = Image.open(path).convert("RGBA")
    width, height = img.size
    alpha = img.getchannel("A")
    pix = alpha.load()
    visited = bytearray(width * height)
    q: deque[tuple[int, int]] = deque()

    def seed(x: int, y: int) -> None:
        idx = y * width + x
        if pix[x, y] == 0 and not visited[idx]:
            visited[idx] = 1
            q.append((x, y))

    for x in range(width):
        seed(x, 0)
        seed(x, height - 1)
    for y in range(height):
        seed(0, y)
        seed(width - 1, y)
    while q:
        x, y = q.popleft()
        for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
            if nx < 0 or ny < 0 or nx >= width or ny >= height:
                continue
            idx = ny * width + nx
            if pix[nx, ny] == 0 and not visited[idx]:
                visited[idx] = 1
                q.append((nx, ny))

    holes = 0
    for y in range(height):
        for x in range(width):
            idx = y * width + x
            if pix[x, y] == 0 and not visited[idx]:
                holes += 1
    return holes


def edge_opaque_pixels(path: Path, alpha_threshold: int = 8) -> int:
    img = Image.open(path).convert("RGBA")
    pix = img.load()
    count = 0
    for x in range(img.width):
        if pix[x, 0][3] > alpha_threshold:
            count += 1
        if pix[x, img.height - 1][3] > alpha_threshold:
            count += 1
    for y in range(1, img.height - 1):
        if pix[0, y][3] > alpha_threshold:
            count += 1
        if pix[img.width - 1, y][3] > alpha_threshold:
            count += 1
    return count


def safe_int(value: Any, default: int = 0) -> int:
    try:
        if value == "":
            return default
        return int(value)
    except (TypeError, ValueError):
        return default


def manifest_frame_rows(manifest_path: Path) -> list[dict[str, Any]]:
    root = manifest_path.parent
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    rows: list[dict[str, Any]] = []
    for action_id, action in manifest["actions"].items():
        for direction, frames in action["directions"].items():
            for index, frame in enumerate(frames):
                anchor = frame.get("anchor") or action.get("anchor") or FIT_PROFILES.get(action.get("fit_profile", "standing"), FIT_PROFILES["standing"])["anchor"]
                rows.append(
                    {
                        "action": action_id,
                        "direction": direction,
                        "frame": index,
                        "base": root / frame["base"],
                        "team_mask": root / frame["team_mask"],
                        "frame_ms": action["frame_ms"],
                        "duration_ms": frame.get("duration_ms", ""),
                        "loop": action["loop"],
                        "hold_last": action.get("hold_last", False),
                        "target_relation": action.get("target_relation", ""),
                        "fit_profile": action.get("fit_profile", frame.get("fit_profile", "")),
                        "anchor": anchor,
                        "phase": frame.get("phase", ""),
                        "source_bbox": frame.get("source_bbox"),
                        "final_bbox": frame.get("final_bbox"),
                        "anchor_offset": frame.get("anchor_offset"),
                        "scale": frame.get("scale"),
                        "manual_review": frame.get("manual_review", "pending"),
                        "residual_key_pixels": frame.get("residual_key_pixels", ""),
                        "team_mask_pixels": frame.get("team_mask_pixels", ""),
                    }
                )
    return rows


def frame_consistency_warnings(
    rows: list[dict[str, Any]],
    *,
    action_filter: str | None = None,
    max_size_drift: int = 8,
    max_baseline_drift: int = 4,
    max_anchor_drift: int = 3,
    max_scale_ratio: float = 0.25,
) -> list[str]:
    warnings: list[str] = []
    grouped: dict[str, list[dict[str, Any]]] = {}
    for row in rows:
        action_id = str(row.get("action", ""))
        if action_filter and action_id != action_filter:
            continue
        grouped.setdefault(action_id, []).append(row)
    for action_id, action_rows in grouped.items():
        if len(action_rows) < 2:
            continue
        boxes = [normalize_bbox_value(row.get("final_bbox")) for row in action_rows]
        boxes = [box for box in boxes if box is not None]
        if len(boxes) >= 2:
            widths = [box["w"] for box in boxes]
            heights = [box["h"] for box in boxes]
            baselines = [box["y"] + box["h"] for box in boxes]
            if max(widths) - min(widths) > max_size_drift:
                warnings.append(f"{action_id}: final bbox width drift is {max(widths) - min(widths)} px")
            if max(heights) - min(heights) > max_size_drift:
                warnings.append(f"{action_id}: final bbox height drift is {max(heights) - min(heights)} px")
            if max(baselines) - min(baselines) > max_baseline_drift:
                warnings.append(f"{action_id}: final baseline drift is {max(baselines) - min(baselines)} px")
        offsets = [row.get("anchor_offset") for row in action_rows if isinstance(row.get("anchor_offset"), (list, tuple))]
        if len(offsets) >= 2:
            xs = [int(offset[0]) for offset in offsets]
            ys = [int(offset[1]) for offset in offsets]
            if max(xs) - min(xs) > max_anchor_drift:
                warnings.append(f"{action_id}: anchor x-offset drift is {max(xs) - min(xs)} px")
            if max(ys) - min(ys) > max_anchor_drift:
                warnings.append(f"{action_id}: anchor y-offset drift is {max(ys) - min(ys)} px")
        scales = []
        for row in action_rows:
            try:
                value = float(row.get("scale"))
            except (TypeError, ValueError):
                continue
            if value > 0:
                scales.append(value)
        if len(scales) >= 2 and min(scales) > 0:
            ratio = (max(scales) - min(scales)) / min(scales)
            if ratio > max_scale_ratio:
                warnings.append(f"{action_id}: source-to-final scale drift is {ratio:.2f}")
    return warnings


def write_review(manifest_path: Path, review_dir: Path, qa_frames: list[dict[str, Any]] | None = None) -> None:
    review_dir.mkdir(parents=True, exist_ok=True)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    team_color = parse_color(manifest.get("team_color_source", "#0088cc"))
    rows = qa_frames if qa_frames is not None else manifest_frame_rows(manifest_path)
    thumb = 72
    label_h = 36
    cols = 6
    row_count = max(1, math.ceil(len(rows) / cols))
    cell_w = thumb * 3 + 24
    cell_h = thumb + label_h
    sheet = Image.new("RGBA", (cols * cell_w, row_count * cell_h), (35, 38, 42, 255))
    alpha_sheet = Image.new("RGBA", (cols * cell_w, row_count * cell_h), (35, 38, 42, 255))
    overlay_sheet = Image.new("RGBA", (cols * cell_w, row_count * cell_h), (35, 38, 42, 255))
    iso_w, iso_h = 128, 148
    iso_sheet = Image.new("RGBA", (cols * iso_w, row_count * iso_h), (29, 32, 36, 255))
    draw = ImageDraw.Draw(sheet)
    alpha_draw = ImageDraw.Draw(alpha_sheet)
    overlay_draw = ImageDraw.Draw(overlay_sheet)
    iso_draw = ImageDraw.Draw(iso_sheet)
    font = ImageFont.load_default()
    qa_out = []
    for idx, row in enumerate(rows):
        x = (idx % cols) * cell_w
        y = (idx // cols) * cell_h
        ix = (idx % cols) * iso_w
        iy = (idx // cols) * iso_h
        base_path = Path(row["base"])
        mask_path = Path(row["team_mask"])
        sheet.alpha_composite(team_colour_preview(base_path, mask_path, thumb, team_color), (x, y))
        sheet.alpha_composite(composite_preview(base_path, thumb), (x + thumb + 8, y))
        mask_img = Image.open(mask_path).convert("RGBA").resize((thumb, thumb), Image.Resampling.NEAREST)
        mask_bg = checkerboard((thumb, thumb))
        mask_bg.alpha_composite(mask_img)
        sheet.alpha_composite(mask_bg, (x + (thumb + 8) * 2, y))
        alpha_sheet.alpha_composite(alpha_preview(base_path, thumb), (x, y))
        alpha_sheet.alpha_composite(mask_bg, (x + thumb + 8, y))
        overlay_sheet.alpha_composite(bbox_anchor_overlay_preview(base_path, mask_path, thumb, team_color, row), (x, y))
        overlay_sheet.alpha_composite(composite_preview(base_path, thumb), (x + thumb + 8, y))
        overlay_sheet.alpha_composite(mask_bg, (x + (thumb + 8) * 2, y))
        label = f"{row['action']} {row['direction']} f{row['frame']}"
        draw.text((x + 2, y + thumb + 2), label[:28], fill=(235, 235, 225, 255), font=font)
        alpha_draw.text((x + 2, y + thumb + 2), label[:28], fill=(235, 235, 225, 255), font=font)
        overlay_draw.text((x + 2, y + thumb + 2), label[:28], fill=(235, 235, 225, 255), font=font)
        iso_sheet.alpha_composite(isometric_tile_preview(base_path, mask_path, team_color, row.get("anchor")), (ix, iy))
        iso_draw.text((ix + 4, iy + 114), label[:24], fill=(235, 235, 225, 255), font=font)
        iso_draw.text((ix + 4, iy + 128), str(row.get("fit_profile", ""))[:24], fill=(180, 190, 185, 255), font=font)
        holes = transparent_holes(base_path)
        edge_pixels = edge_opaque_pixels(base_path)
        qa = {k: v for k, v in row.items() if k not in {"base", "team_mask"}}
        qa["base"] = str(base_path)
        qa["team_mask"] = str(mask_path)
        qa["transparent_hole_pixels"] = holes
        qa["edge_opaque_pixels"] = edge_pixels
        qa_out.append(qa)

    contact_path = review_dir / "contact_sheet.png"
    iso_contact_path = review_dir / "isometric_contact_sheet.png"
    alpha_mask_path = review_dir / "alpha_mask_sheet.png"
    overlay_path = review_dir / "bbox_anchor_overlay.png"
    sheet.save(contact_path)
    iso_sheet.save(iso_contact_path)
    alpha_sheet.save(alpha_mask_path)
    overlay_sheet.save(overlay_path)
    (review_dir / "qa.json").write_text(json.dumps(qa_out, indent=2) + "\n", encoding="utf-8")

    warnings = []
    for qa in qa_out:
        residual = safe_int(qa.get("residual_key_pixels", "0"))
        holes = safe_int(qa.get("transparent_hole_pixels", "0"))
        if residual:
            warnings.append(f"{qa['action']} {qa['direction']} f{qa['frame']}: {residual} residual key pixels")
        if holes > 1200:
            warnings.append(f"{qa['action']} {qa['direction']} f{qa['frame']}: {holes} transparent interior pixels")
        edge_pixels = safe_int(qa.get("edge_opaque_pixels", "0"))
        if edge_pixels:
            warnings.append(f"{qa['action']} {qa['direction']} f{qa['frame']}: {edge_pixels} opaque pixels touch the crop edge")
    warnings.extend(frame_consistency_warnings(qa_out))

    html_rows = "\n".join(
        "<tr>"
        + f"<td>{html.escape(qa['action'])}</td>"
        + f"<td>{html.escape(qa['direction'])}</td>"
        + f"<td>{html.escape(str(qa['frame']))}</td>"
        + f"<td>{html.escape(str(qa.get('frame_ms', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('loop', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('residual_key_pixels', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('team_mask_pixels', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('transparent_hole_pixels', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('edge_opaque_pixels', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('anchor_offset', '')))}</td>"
        + f"<td>{html.escape(str(qa.get('manual_review', '')))}</td>"
        + "</tr>"
        for qa in qa_out
    )
    warning_items = "\n".join(f"<li>{html.escape(w)}</li>" for w in warnings) or "<li>No script-level warnings.</li>"
    review_html = f"""<!doctype html>
<meta charset="utf-8">
<title>Realm Tileset Review</title>
<style>
body {{ font-family: system-ui, sans-serif; margin: 24px; background: #1f2328; color: #eceff4; }}
a {{ color: #8cc8ff; }}
img {{ image-rendering: pixelated; max-width: 100%; border: 1px solid #555; }}
table {{ border-collapse: collapse; margin-top: 16px; font-size: 13px; }}
td, th {{ border: 1px solid #555; padding: 4px 7px; }}
th {{ background: #30363d; }}
</style>
<h1>Realm Tileset Review</h1>
<p>Manifest: {html.escape(str(manifest_path))}</p>
<h2>Contact Sheet</h2>
<p><img src="contact_sheet.png" alt="contact sheet"></p>
<h2>Isometric Tile Placement</h2>
<p><img src="isometric_contact_sheet.png" alt="isometric tile placement contact sheet"></p>
<h2>Alpha And Mask</h2>
<p><img src="alpha_mask_sheet.png" alt="alpha and team mask sheet"></p>
<h2>Bounding Boxes And Anchors</h2>
<p><img src="bbox_anchor_overlay.png" alt="bounding box and anchor overlay"></p>
<h2>Warnings</h2>
<ul>{warning_items}</ul>
<h2>Frame QA</h2>
<table>
<tr><th>Action</th><th>Direction</th><th>Frame</th><th>ms</th><th>loop</th><th>residual key</th><th>team mask</th><th>hole px</th><th>edge px</th><th>anchor offset</th><th>review</th></tr>
{html_rows}
</table>
"""
    (review_dir / "review.html").write_text(review_html, encoding="utf-8")


def command_review(args: argparse.Namespace) -> None:
    write_review(Path(args.manifest), Path(args.review_out))
    print(f"wrote review: {Path(args.review_out) / 'review.html'}")


def command_verify_peasant_idle(args: argparse.Namespace) -> None:
    manifest_path = Path(args.manifest)
    review_dir = Path(args.review_out)
    write_review(manifest_path, review_dir)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    failures: list[str] = []
    if manifest.get("entity") != "peasant":
        failures.append("manifest entity is not peasant")
    assumptions = [str(item) for item in manifest.get("assumptions", [])]
    idle_assumptions = [item for item in assumptions if item.startswith("idle/")]
    if idle_assumptions:
        failures.append(f"manifest has idle assumptions: {idle_assumptions}")
    idle = manifest.get("actions", {}).get("idle")
    if not idle:
        failures.append("missing idle action")
    else:
        if safe_int(idle.get("frame_ms", 0)) != 20000:
            failures.append("idle frame_ms is not 20000")
        if idle.get("loop") is not False:
            failures.append("idle loop must be false")
        if idle.get("hold_last") is not True:
            failures.append("idle hold_last must be true")
        if safe_int(idle.get("transition_after_ms", 0)) != 20000:
            failures.append("idle transition_after_ms is not 20000")
        directions = idle.get("directions", {})
        for direction in ("front", "back"):
            frames = directions.get(direction)
            if not isinstance(frames, list) or len(frames) != 2:
                failures.append(f"idle/{direction} must contain exactly two frames")
                continue
            for frame_index, frame in enumerate(frames):
                base = manifest_path.parent / frame.get("base", "")
                mask = manifest_path.parent / frame.get("team_mask", "")
                label = f"idle/{direction}/frame_{frame_index:02d}"
                if not base.exists():
                    failures.append(f"{label} missing base PNG: {base}")
                if not mask.exists():
                    failures.append(f"{label} missing team mask PNG: {mask}")
                if not frame.get("source"):
                    failures.append(f"{label} missing production source provenance")
                if safe_int(frame.get("residual_key_pixels", 0)):
                    failures.append(f"{label} has residual key pixels")
                if base.exists() and edge_opaque_pixels(base):
                    failures.append(f"{label} has opaque edge-touching pixels")
                if safe_int(frame.get("team_mask_pixels", 0)) <= 0:
                    failures.append(f"{label} has no team-mask pixels")
                if frame_index == 1 and "arms crossed" not in str(frame.get("phase", "")).lower():
                    failures.append(f"{label} phase is not the arms-crossed long-idle pose")
    qa_path = review_dir / "qa.json"
    if qa_path.exists():
        qa_rows = json.loads(qa_path.read_text(encoding="utf-8"))
        for row in qa_rows:
            if row.get("action") != "idle":
                continue
            holes = safe_int(row.get("transparent_hole_pixels", 0))
            if holes > args.max_holes:
                failures.append(
                    f"idle/{row.get('direction')}/frame_{row.get('frame')} has {holes} transparent interior pixels"
                )
        failures.extend(frame_consistency_warnings(qa_rows, action_filter="idle"))
    if failures:
        print(f"wrote review: {review_dir / 'review.html'}")
        print("peasant idle verification failed:")
        for failure in failures:
            print(f"- {failure}")
        raise SystemExit(1)
    print(f"wrote review: {review_dir / 'review.html'}")
    print(f"wrote contact sheet: {review_dir / 'contact_sheet.png'}")
    print(f"wrote isometric contact sheet: {review_dir / 'isometric_contact_sheet.png'}")
    print(f"wrote alpha/mask sheet: {review_dir / 'alpha_mask_sheet.png'}")
    print(f"wrote bbox/anchor overlay: {review_dir / 'bbox_anchor_overlay.png'}")
    print("peasant idle verification passed: 2 front frames, 2 back frames, 20000 ms long-idle hold")


def command_split_reference(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager reference splitting is bundled so far")
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    actions = spec_actions(unit_spec)
    key_color = parse_color(args.magenta)
    tiles, method = extract_tiles_from_sheet(Path(args.sheet), args.cols, args.rows, key_color, args.magenta_threshold)
    out_dir = Path(args.workbench) / entity / "reference" / f"{args.view}_state_{args.state}"
    out_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "entity": entity,
        "source_sheet": str(Path(args.sheet)),
        "view": args.view,
        "state": int(args.state),
        "detection_method": method,
        "animation_spec_source": unit_spec.get("source", "python-fallback"),
        "slots": [],
    }
    for index, spec in enumerate(actions):
        path = out_dir / f"{index + 1:02d}_{spec['id']}.png"
        tiles[index].save(path)
        manifest["slots"].append(
            {
                "index": index + 1,
                "action": spec["id"],
                "phase": phase_text(spec, int(args.state) - 1),
                "path": str(path),
            }
        )
    manifest_path = out_dir / "reference_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote reference slots: {out_dir}")
    print(f"wrote reference manifest: {manifest_path}")


def command_prompt_frame(args: argparse.Namespace) -> None:
    unit_spec = load_unit_spec(args.entity, args.spec_source, args.spec_binary, args.spec_json)
    slot_action_id, slot_frame, slot_ref = resolve_reference_slot(args, unit_spec)
    action_id = args.action or slot_action_id
    frame = args.frame if args.frame is not None else slot_frame
    if action_id is None:
        raise SystemExit("prompt-frame needs --action, or --reference-slot with --reference-view and --reference-state")
    if frame is None:
        raise SystemExit("prompt-frame needs --frame, or --reference-slot with --reference-state")
    refs = [str(Path(ref)) for ref in args.reference]
    if slot_ref and slot_ref not in refs:
        refs.append(slot_ref)
    prompt = frame_prompt_text(args.entity, action_id, args.direction, frame, refs, unit_spec)
    if args.prompt_out:
        out = Path(args.prompt_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(prompt + "\n", encoding="utf-8")
        print(f"wrote prompt: {out}")
    else:
        print(prompt)


def command_slot_info(args: argparse.Namespace) -> None:
    unit_spec = load_unit_spec(args.entity, args.spec_source, args.spec_binary, args.spec_json)
    slot_index = slot_index_from_label(args.slot, unit_spec)
    spec = slot_action(unit_spec, slot_index)
    frame = int(args.state) - 1
    path = reference_slot_path(Path(args.workbench), args.entity, args.view, int(args.state), slot_index, spec["id"])
    payload = {
        "entity": args.entity,
        "slot": args.slot,
        "slot_index": slot_index + 1,
        "row": slot_index // 4 + 1,
        "col": slot_index % 4 + 1,
        "action": spec["id"],
        "direction": args.view,
        "state": int(args.state),
        "frame": frame,
        "phase": phase_text(spec, frame),
        "reference": str(path),
        "reference_exists": path.exists(),
    }
    print(json.dumps(payload, indent=2))


def command_prompt_batch_source(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager batch prompting is bundled so far")
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    spec = action_spec(args.action, unit_spec)
    slots = [parse_batch_slot(raw) for raw in args.slot] if args.slot else default_batch_slots(entity, args.action)
    references = "\n".join(f"- Positive reference to view before generation: {Path(ref)}" for ref in args.reference) or "- no positive local reference attached"
    panel_lines = []
    for index, (direction, frame) in enumerate(slots):
        row = index // args.cols + 1
        col = index % args.cols + 1
        panel_lines.append(
            f"{index + 1}. row {row}, column {col}: {direction} frame {frame}, {phase_text(spec, frame)}, {direction_text(direction)}"
        )
    prompt = f"""Use case: stylized-concept
Asset type: coherent production batch source for a 2D RTS game tileset
Primary request: Generate exactly one square image containing a {args.cols} by {args.rows} batch sheet for the Realm {entity} `{args.action}` animation. This batch sheet is an intermediate consistency source; it will be split into standalone production `source.png` files before processing.

Shared character identity: one compact medieval male peasant/villager with the same rounded cartoon RTS proportions, blue helmet, blue tunic/player-colour cloth accents, tan skin, dark beard, brown belt, brown boots, thick clean outline, and readable small-sprite silhouette in every panel.

Consistency requirements: every panel must use the identical character design, palette, scale, camera height, lighting, outline weight, pixel-art/painted sprite finish, and foot baseline. All source art faces slightly toward screen right. Do not create left-facing source art; left facings are mirrored by the Realm renderer at runtime.

Panel order. Use these labels as instructions only; do not draw labels or text:
{chr(10).join(panel_lines)}

Layout: one square image, {args.cols} columns by {args.rows} rows, equal square panels, generous padding inside each panel, one complete centered sprite per panel, flat uniform #ff00ff magenta or clean transparent background. No gutters or labels are required, but the visual grid must be easy to split mechanically.

Positive references that must be viewed before generation:
{references}

Reference hygiene: view only positive references that show the desired angle or the full source sheet. Do not view or attach wrong-angle crops, flat rear diagrams, face-on front crops, inconsistent failed batches, or other rejected images as generation references; they are for human diagnosis only and bias the pose.

Constraints: no text, no numbers, no watermark, no cropped reference-sheet art as the final sprite, no straight-on mascot front view, no flat rear diagram, no mirrored left-facing variants, no magenta inside the character or carried objects. Frame 1 of Peasant idle is the arms-crossed long-idle pose in both front and back panels."""
    if args.prompt_out:
        out = Path(args.prompt_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(prompt + "\n", encoding="utf-8")
        print(f"wrote batch prompt: {out}")
    else:
        print(prompt)


def slugify_version(raw: str) -> str:
    slug = raw.strip().lower().replace("\\", "-").replace("/", "-")
    cleaned = []
    for ch in slug:
        if ch.isalnum() or ch in ("-", "_", "."):
            cleaned.append(ch)
        elif ch.isspace():
            cleaned.append("-")
    slug = "".join(cleaned).strip("-._")
    if not slug:
        raise SystemExit("version must contain at least one filename-safe character")
    if slug in {".", ".."}:
        raise SystemExit(f"invalid version: {raw}")
    return slug


def default_candidate_version() -> str:
    return datetime.now().strftime("%Y%m%d-%H%M%S")


def command_store_generated(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager generated-source storing is bundled so far")
    source_path = Path(args.input)
    if not source_path.exists():
        raise SystemExit(f"generated image not found: {source_path}")
    version = slugify_version(args.version or default_candidate_version())
    candidate_dir = Path(args.candidates) / entity / args.action / version
    if candidate_dir.exists() and any(candidate_dir.iterdir()) and not args.force:
        raise SystemExit(f"candidate version already exists; use --force or a new --version: {candidate_dir}")
    candidate_dir.mkdir(parents=True, exist_ok=True)

    image = Image.open(source_path).convert("RGBA")
    out_name = "batch_source.png" if args.kind == "batch-source" else "source.png"
    stored_path = candidate_dir / out_name
    same_image_path = source_path.resolve() == stored_path.resolve() if stored_path.exists() else False
    if not same_image_path:
        image.save(stored_path)

    manifest: dict[str, Any] = {
        "schema": "realm.generated_candidate.v1",
        "entity": entity,
        "action": args.action,
        "version": version,
        "kind": args.kind,
        "status": args.status,
        "original_input": str(source_path),
        "stored_image": str(stored_path),
        "image_size": {"w": image.width, "h": image.height},
        "notes": args.note,
    }
    if args.prompt_file:
        prompt_path = Path(args.prompt_file)
        if not prompt_path.exists():
            raise SystemExit(f"prompt file not found: {prompt_path}")
        prompt_out = candidate_dir / "prompt.txt"
        same_prompt_path = prompt_out.exists() and prompt_path.resolve() == prompt_out.resolve()
        if not same_prompt_path:
            shutil.copy2(prompt_path, prompt_out)
        manifest["prompt_file"] = str(prompt_out)
    if args.reference:
        manifest["positive_references"] = [str(Path(ref)) for ref in args.reference]

    manifest_path = candidate_dir / "candidate_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"stored generated candidate: {stored_path}")
    print(f"wrote candidate manifest: {manifest_path}")
    if args.kind == "batch-source":
        print("split with:")
        print(
            "python .agents\\skills\\realm-tileset-from-images\\scripts\\realm_tileset.py "
            f"split-batch-source --entity {entity} --spec-source code --action {args.action} "
            f"--sheet {stored_path} --force"
        )


def command_split_batch_source(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager batch splitting is bundled so far")
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    spec = action_spec(args.action, unit_spec)
    slots = [parse_batch_slot(raw) for raw in args.slot] if args.slot else default_batch_slots(entity, args.action)
    expected = args.cols * args.rows
    if len(slots) != expected:
        raise SystemExit(f"slot count {len(slots)} does not match {args.cols}x{args.rows} batch source")
    if len(set(slots)) != len(slots):
        raise SystemExit("batch source slots must be unique direction/frame pairs")

    sheet_path = Path(args.sheet)
    if not sheet_path.exists():
        raise SystemExit(f"batch source not found: {sheet_path}")
    sheet = Image.open(sheet_path).convert("RGBA")
    if sheet.width < args.min_size or sheet.height < args.min_size:
        raise SystemExit(f"batch source is too small: {sheet.width}x{sheet.height}, expected at least {args.min_size}px")
    if abs(sheet.width - sheet.height) > args.square_tolerance:
        raise SystemExit(f"batch source must be square or near-square, got {sheet.width}x{sheet.height}")

    boxes = grid_boxes(sheet.width, sheet.height, args.cols, args.rows)
    key_color = parse_color(args.magenta)
    action_root = Path(args.workbench) / entity / args.action
    manifest: dict[str, Any] = {
        "schema": "realm.batch_source_manifest.v1",
        "entity": entity,
        "action": args.action,
        "source_sheet": str(sheet_path),
        "grid": {"cols": args.cols, "rows": args.rows},
        "animation_spec_source": unit_spec.get("source", "python-fallback"),
        "slots": [],
    }
    for index, ((direction, frame), box) in enumerate(zip(slots, boxes)):
        label = f"{args.action}/{direction}/frame_{frame:02d}"
        if frame >= len(spec.get("phases", [])):
            raise SystemExit(f"{label} is outside the code-derived frame count")
        panel = sheet.crop(box)
        if panel.width != panel.height:
            side = min(panel.width, panel.height)
            panel = panel.crop(((panel.width - side) // 2, (panel.height - side) // 2, (panel.width + side) // 2, (panel.height + side) // 2))
        stats = validate_batch_panel(panel, label, key_color, args.magenta_threshold, args.min_content_pixels, args.min_margin)
        out_dir = frame_workbench_dir(Path(args.workbench), entity, args.action, direction, frame)
        out_path = out_dir / "source.png"
        if out_path.exists() and not args.force:
            raise SystemExit(f"refusing to overwrite existing source without --force: {out_path}")
        out_dir.mkdir(parents=True, exist_ok=True)
        panel.save(out_path)
        manifest["slots"].append(
            {
                "index": index + 1,
                "direction": direction,
                "frame": frame,
                "phase": phase_text(spec, frame),
                "source_box": bbox_record(box),
                "output": str(out_path),
                **stats,
            }
        )

    manifest_path = action_root / "batch_source_manifest.json"
    action_root.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote split batch sources under: {action_root}")
    print(f"wrote batch source manifest: {manifest_path}")


def command_process_frame(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager single-frame processing is bundled so far")
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    spec = action_spec(args.action, unit_spec)
    key_color = parse_color(args.magenta)
    team_color = parse_color(args.team_color)
    prompt_text = ""
    if args.prompt_file:
        prompt_path = Path(args.prompt_file)
        if not prompt_path.exists():
            raise SystemExit(f"prompt file not found: {prompt_path}")
        prompt_text = prompt_path.read_text(encoding="utf-8")
    elif args.write_prompt:
        prompt_text = frame_prompt_text(args.entity, args.action, args.direction, args.frame, args.reference, unit_spec)
    raw_tile, method = load_single_source_tile(Path(args.input), key_color, args.magenta_threshold)
    base, team_mask, stats = process_tile_image(
        raw_tile,
        spec,
        args.size,
        key_color,
        args.magenta_threshold,
        team_color,
        args.team_threshold,
    )

    out_root = Path(args.out) / entity
    action_dir = out_root / args.action / args.direction
    action_dir.mkdir(parents=True, exist_ok=True)
    base_path = action_dir / f"frame_{args.frame:02d}_base.png"
    mask_path = action_dir / f"frame_{args.frame:02d}_teammask.png"
    base.save(base_path)
    team_mask.save(mask_path)

    work_dir = frame_workbench_dir(Path(args.workbench), entity, args.action, args.direction, args.frame)
    work_dir.mkdir(parents=True, exist_ok=True)
    if prompt_text:
        (work_dir / "prompt.md").write_text(prompt_text.rstrip() + "\n", encoding="utf-8")
    metadata = {
        "schema": "realm.frame_metadata.v1",
        "entity": entity,
        "action": args.action,
        "direction": args.direction,
        "frame": args.frame,
        "phase": phase_text(spec, args.frame),
        "duration_ms": frame_duration_ms(spec, args.frame),
        "family": spec["family"],
        "target_relation": spec["target_relation"],
        "range_tiles": spec.get("range_tiles", 0),
        "fit_profile": spec["fit_profile"],
        "animation_spec_source": unit_spec.get("source", "python-fallback"),
        "source": str(Path(args.input)),
        "source_detection_method": method,
        "references": [str(Path(ref)) for ref in args.reference],
        "prompt_file": str(Path(args.prompt_file)) if args.prompt_file else str(work_dir / "prompt.md") if prompt_text else "",
        "base": str(base_path),
        "team_mask": str(mask_path),
        "manual_review": args.manual_review,
        **stats,
    }
    metadata_path = work_dir / "metadata.json"
    metadata_path.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    print(f"wrote frame base: {base_path}")
    print(f"wrote frame team mask: {mask_path}")
    print(f"wrote frame metadata: {metadata_path}")


def load_frame_metadata(workbench: Path, entity: str, action_id: str, direction: str, frame: int) -> dict[str, Any]:
    path = frame_workbench_dir(workbench, entity, action_id, direction, frame) / "metadata.json"
    if not path.exists():
        return {}
    return json.loads(path.read_text(encoding="utf-8"))


def command_assemble(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager manifest assembly is bundled so far")
    args.source_workflow = "single-frame"
    unit_spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    manifest = build_manifest(args, [], unit_spec)
    out_root = Path(args.out) / entity
    workbench = Path(args.workbench)
    assumptions: list[str] = manifest["assumptions"]

    for spec in spec_actions(unit_spec):
        action_id = spec["id"]
        expected_frames = len(spec.get("phases", [])) or 1
        for direction in ("front", "back"):
            frames = []
            for frame in range(expected_frames):
                base_path = out_root / action_id / direction / f"frame_{frame:02d}_base.png"
                mask_path = out_root / action_id / direction / f"frame_{frame:02d}_teammask.png"
                if not base_path.exists() or not mask_path.exists():
                    assumptions.append(f"{action_id}/{direction}/frame_{frame:02d} missing from single-frame source set")
                    continue
                metadata = load_frame_metadata(workbench, entity, action_id, direction, frame)
                frames.append(
                    {
                        "base": relative_to(base_path, out_root),
                        "team_mask": relative_to(mask_path, out_root),
                        "phase": metadata.get("phase", phase_text(spec, frame)),
                        "duration_ms": metadata.get("duration_ms", frame_duration_ms(spec, frame)),
                        "source": metadata.get("source", ""),
                        "source_bbox": metadata.get("source_bbox"),
                        "final_bbox": metadata.get("final_bbox"),
                        "anchor": metadata.get("anchor", FIT_PROFILES[spec["fit_profile"]]["anchor"]),
                        "anchor_offset": metadata.get("anchor_offset"),
                        "scale": metadata.get("scale"),
                        "manual_review": metadata.get("manual_review", "pending"),
                        "residual_key_pixels": metadata.get("residual_key_pixels", 0),
                        "team_mask_pixels": metadata.get("team_mask_pixels", 0),
                        "transparent_key_pixels": metadata.get("transparent_key_pixels", 0),
                        "edge_fringe_key_pixels": metadata.get("edge_fringe_key_pixels", 0),
                    }
                )
            manifest["actions"][action_id]["directions"][direction] = frames

    manifest_path = out_root / "manifest.json"
    out_root.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    write_review(manifest_path, Path(args.review_out))
    print(f"wrote manifest: {manifest_path}")
    print(f"wrote review: {Path(args.review_out) / 'review.html'}")


def command_unit_spec(args: argparse.Namespace) -> None:
    entity = args.entity.strip().lower().replace("_", "-")
    if entity != "peasant":
        raise SystemExit("only peasant/villager unit specs are bundled so far")
    spec = load_unit_spec(entity, args.spec_source, args.spec_binary, args.spec_json)
    if args.out:
        out = Path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(spec, indent=2) + "\n", encoding="utf-8")
        md = out.with_suffix(".md")
        lines = [
            f"# Realm {entity.title()} Tileset Spec",
            "",
            "Tileset mode renders terrain as isometric-projected diamonds and renders units as upright sprites anchored over the unit tile.",
            "",
            "| Action | Family | Target relation | Frames | Timing | Loop | Fit |",
            "|---|---|---|---|---:|---|---|",
        ]
        for action in spec_actions(spec):
            frames = "<br>".join(action.get("phases", []))
            lines.append(
                f"| `{action['id']}` | {action['family']} | {action['target_relation']} | {frames} | {action['frame_ms']} ms | {action['loop']} | {action['fit_profile']} |"
            )
        md.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"wrote unit spec: {out}")
        print(f"wrote unit spec notes: {md}")
    else:
        print(json.dumps(spec, indent=2))


def command_prompt(args: argparse.Namespace) -> None:
    if args.entity.lower() not in {"peasant", "villager"}:
        raise SystemExit("only the peasant/villager prompt is bundled so far")
    view = args.view
    state = int(args.state)
    facing = (
        "from the Realm front angle: three-quarter front RTS sprite view, body and face turned "
        "about 30-45 degrees toward screen right like the reference villager, not straight-on"
    )
    if view == "back":
        facing = (
            "from the Realm back angle: three-quarter rear RTS sprite view facing away toward screen right; "
            "show the back and side of the helmet, tunic, belt, pack or basket, and rear side of tools"
        )
    rows = "\n".join(f"{i + 1}. {line}" for i, line in enumerate(PEASANT_STATE_DESCRIPTIONS[state]))
    print(
        f"""Use case: stylized-concept
Asset type: reference contact sheet for a 2D RTS game tileset
Primary request: Generate exactly one image: the {view}_state_{state} reference sheet for the Realm peasant/villager unit. This is not final production art; final frames will be generated one at a time later.

Style reference: match the supplied villager sprite sheets: rounded cartoon RTS style, compact proportions, blue helmet and blue tunic accents, brown leather boots and belt, tan skin, dark beard, thick clean outline, readable at small size.

Canvas/layout: one square image containing a clear 4 by 4 contact sheet with consistent row and column spacing. A single solid #ff00ff magenta background across the whole sheet is fine; separate magenta squares and white gutters are not required. Put exactly one complete sprite centered in each visual grid slot.

Character consistency: every square shows the same compact medieval male peasant/villager {facing}. Keep the same scale, camera angle, lighting, outline thickness, outfit, helmet, beard, and blue player-colour cloth across all 16 squares.

Exact 16 squares, left to right, top to bottom:
{rows}

Constraints: no labels, no text, no numbers, no watermark. Do not use magenta in the character, berries, meat, skin, clothing trim, shadows, or tool highlights. Keep every sprite readable and visually separated in its grid slot. Do not draw transparency."""
    )


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    def add_spec_source_args(p: argparse.ArgumentParser) -> None:
        p.add_argument("--spec-source", choices=["auto", "code", "fallback"], default="auto")
        p.add_argument("--spec-binary", help="Realm binary to call with --dump-animation-spec")
        p.add_argument("--spec-json", help="Read a previously exported unit animation JSON spec")

    extract = sub.add_parser("extract", help="crop source sheets into Realm entity assets")
    extract.add_argument("--entity", required=True)
    add_spec_source_args(extract)
    extract.add_argument("--front-state-1")
    extract.add_argument("--front-state-2")
    extract.add_argument("--back-state-1")
    extract.add_argument("--back-state-2")
    extract.add_argument("--out", default="assets/tiles/entities")
    extract.add_argument("--review-out", default="build/tileset-review")
    extract.add_argument("--cols", type=int, default=4)
    extract.add_argument("--rows", type=int, default=4)
    extract.add_argument("--size", type=int, default=48)
    extract.add_argument("--magenta", default="#ff00ff")
    extract.add_argument("--magenta-threshold", type=float, default=62.0)
    extract.add_argument("--team-color", default="#0088cc")
    extract.add_argument("--team-threshold", type=float, default=42.0)
    extract.add_argument("--clean", action="store_true")
    extract.set_defaults(func=command_extract)

    review = sub.add_parser("review", help="rebuild review artifacts from a manifest")
    review.add_argument("--manifest", required=True)
    review.add_argument("--review-out", required=True)
    review.set_defaults(func=command_review)

    verify_idle = sub.add_parser("verify-peasant-idle", help="verify the exact Peasant idle production set and rebuild review artifacts")
    verify_idle.add_argument("--manifest", default="assets/tiles/entities/peasant/manifest.json")
    verify_idle.add_argument("--review-out", default="build/tileset-review/peasant-idle")
    verify_idle.add_argument("--max-holes", type=int, default=1200)
    verify_idle.set_defaults(func=command_verify_peasant_idle)

    split = sub.add_parser("split-reference", help="split a 4x4 contact sheet into named workbench reference slots")
    split.add_argument("--entity", default="peasant")
    add_spec_source_args(split)
    split.add_argument("--sheet", required=True)
    split.add_argument("--view", choices=["front", "back"], required=True)
    split.add_argument("--state", choices=["1", "2"], required=True)
    split.add_argument("--workbench", default="art/tiles/workbench")
    split.add_argument("--cols", type=int, default=4)
    split.add_argument("--rows", type=int, default=4)
    split.add_argument("--magenta", default="#ff00ff")
    split.add_argument("--magenta-threshold", type=float, default=62.0)
    split.set_defaults(func=command_split_reference)

    slot_info = sub.add_parser("slot-info", help="resolve a reference-sheet slot to action, frame, and reference crop")
    slot_info.add_argument("--entity", default="peasant")
    add_spec_source_args(slot_info)
    slot_info.add_argument("--slot", required=True, help="top-left, row1-col1, 1-16, or action id")
    slot_info.add_argument("--view", choices=["front", "back"], required=True)
    slot_info.add_argument("--state", choices=["1", "2"], required=True)
    slot_info.add_argument("--workbench", default="art/tiles/workbench")
    slot_info.set_defaults(func=command_slot_info)

    prompt_frame = sub.add_parser("prompt-frame", help="print a single-frame production prompt from the peasant registry")
    prompt_frame.add_argument("--entity", default="peasant")
    add_spec_source_args(prompt_frame)
    prompt_frame.add_argument("--action")
    prompt_frame.add_argument("--direction", choices=["front", "back"], required=True)
    prompt_frame.add_argument("--frame", type=int)
    prompt_frame.add_argument("--reference", action="append", default=[])
    prompt_frame.add_argument("--reference-slot", help="top-left, row1-col1, 1-16, or action id")
    prompt_frame.add_argument("--reference-view", choices=["front", "back"])
    prompt_frame.add_argument("--reference-state", choices=["1", "2"])
    prompt_frame.add_argument("--workbench", default="art/tiles/workbench")
    prompt_frame.add_argument("--prompt-out")
    prompt_frame.set_defaults(func=command_prompt_frame)

    prompt_batch = sub.add_parser("prompt-batch-source", help="print a coherent multi-frame production batch prompt")
    prompt_batch.add_argument("--entity", default="peasant")
    add_spec_source_args(prompt_batch)
    prompt_batch.add_argument("--action", default="idle")
    prompt_batch.add_argument("--cols", type=int, default=2)
    prompt_batch.add_argument("--rows", type=int, default=2)
    prompt_batch.add_argument("--slot", action="append", default=[], help="grid-order direction:frame, for example front:0")
    prompt_batch.add_argument("--reference", action="append", default=[])
    prompt_batch.add_argument("--prompt-out")
    prompt_batch.set_defaults(func=command_prompt_batch_source)

    store_generated = sub.add_parser("store-generated", help="copy a generated image into a versioned Realm candidate folder")
    store_generated.add_argument("--entity", default="peasant")
    store_generated.add_argument("--action", default="idle")
    store_generated.add_argument("--input", required=True, help="generated PNG from .codex or another transient location")
    store_generated.add_argument("--candidates", default="art/tiles/candidates")
    store_generated.add_argument("--version", help="filename-safe version id, defaults to YYYYMMDD-HHMMSS")
    store_generated.add_argument("--kind", choices=["batch-source", "single-frame-source"], default="batch-source")
    store_generated.add_argument("--status", choices=["candidate", "accepted", "rejected"], default="candidate")
    store_generated.add_argument("--prompt-file")
    store_generated.add_argument("--reference", action="append", default=[], help="positive reference that was visible for generation")
    store_generated.add_argument("--note", action="append", default=[])
    store_generated.add_argument("--force", action="store_true")
    store_generated.set_defaults(func=command_store_generated)

    split_batch = sub.add_parser("split-batch-source", help="split a coherent batch source into standalone workbench source.png frames")
    split_batch.add_argument("--entity", default="peasant")
    add_spec_source_args(split_batch)
    split_batch.add_argument("--action", default="idle")
    split_batch.add_argument("--sheet", required=True)
    split_batch.add_argument("--workbench", default="art/tiles/workbench")
    split_batch.add_argument("--cols", type=int, default=2)
    split_batch.add_argument("--rows", type=int, default=2)
    split_batch.add_argument("--slot", action="append", default=[], help="grid-order direction:frame, for example front:0")
    split_batch.add_argument("--magenta", default="#ff00ff")
    split_batch.add_argument("--magenta-threshold", type=float, default=62.0)
    split_batch.add_argument("--min-size", type=int, default=512)
    split_batch.add_argument("--square-tolerance", type=int, default=8)
    split_batch.add_argument("--min-content-pixels", type=int, default=256)
    split_batch.add_argument("--min-margin", type=int, default=8)
    split_batch.add_argument("--force", action="store_true")
    split_batch.set_defaults(func=command_split_batch_source)

    process = sub.add_parser("process-frame", help="process one 1024 source tile into final base/mask assets")
    process.add_argument("--entity", default="peasant")
    add_spec_source_args(process)
    process.add_argument("--action", required=True)
    process.add_argument("--direction", choices=["front", "back"], required=True)
    process.add_argument("--frame", type=int, required=True)
    process.add_argument("--input", required=True)
    process.add_argument("--out", default="assets/tiles/entities")
    process.add_argument("--workbench", default="art/tiles/workbench")
    process.add_argument("--size", type=int, default=48)
    process.add_argument("--magenta", default="#ff00ff")
    process.add_argument("--magenta-threshold", type=float, default=62.0)
    process.add_argument("--team-color", default="#0088cc")
    process.add_argument("--team-threshold", type=float, default=42.0)
    process.add_argument("--reference", action="append", default=[])
    process.add_argument("--prompt-file")
    process.add_argument("--write-prompt", action="store_true")
    process.add_argument("--manual-review", default="pending")
    process.set_defaults(func=command_process_frame)

    assemble = sub.add_parser("assemble", help="assemble a manifest from single-frame outputs")
    assemble.add_argument("--entity", default="peasant")
    add_spec_source_args(assemble)
    assemble.add_argument("--out", default="assets/tiles/entities")
    assemble.add_argument("--workbench", default="art/tiles/workbench")
    assemble.add_argument("--review-out", default="build/tileset-review/peasant-single")
    assemble.add_argument("--size", type=int, default=48)
    assemble.add_argument("--magenta", default="#ff00ff")
    assemble.add_argument("--team-color", default="#0088cc")
    assemble.set_defaults(func=command_assemble)

    unit_spec = sub.add_parser("unit-spec", help="print or write the code-derived peasant tileset action spec")
    unit_spec.add_argument("--entity", default="peasant")
    add_spec_source_args(unit_spec)
    unit_spec.add_argument("--out")
    unit_spec.set_defaults(func=command_unit_spec)

    prompt = sub.add_parser("prompt", help="print a bundled generation prompt")
    prompt.add_argument("--entity", default="peasant")
    prompt.add_argument("--view", choices=["front", "back"], default="front")
    prompt.add_argument("--state", choices=["1", "2"], default="1")
    prompt.set_defaults(func=command_prompt)
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
