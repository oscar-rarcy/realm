"""Shared Realm tileset source-resolution policy.

Generation works in sheets or standalone sources. Runtime promotion may crop
transparent margins from those sources. The renderer may then downsample runtime
PNGs into draw-size cached textures, but production tooling should not promote
tiny draw-size crops as canonical runtime art.
"""

from __future__ import annotations

import struct
from pathlib import Path
from typing import Any


GROUND_MIN_SOURCE_PX = 512
GROUND_TARGET_SOURCE_PX = 1024

SPRITE_PRE_CROP_TARGET_PX = 256
SPRITE_RUNTIME_MIN_LONGEST_SIDE_PX = 128

BUILDING_PRE_CROP_TARGET_PX_PER_FOOTPRINT_TILE = 256
BUILDING_RUNTIME_MIN_LONGEST_SIDE_PX_PER_FOOTPRINT_TILE = 128
BUILDING_TARGET_MAX_PX = 1536

SPRITE_LIKE_GROUPS = {
    "units",
    "animals",
    "features",
    "decals",
    "projectiles",
    "effects",
    "user_interface",
}


def footprint_axis(footprint: dict[str, int] | None = None) -> int:
    footprint = footprint or {"w": 1, "h": 1}
    width = max(1, int(footprint.get("w", 1) or 1))
    height = max(1, int(footprint.get("h", 1) or 1))
    return max(width, height)


def visual_envelope_axes(visual_envelope: dict[str, int] | None = None) -> tuple[int, int]:
    visual_envelope = visual_envelope or {"w": 1, "h": 1}
    width = max(1, int(visual_envelope.get("w", 1) or 1))
    height = max(1, int(visual_envelope.get("h", 1) or 1))
    return width, height


def source_resolution_policy(
    group: str,
    *,
    footprint: dict[str, int] | None = None,
    visual_envelope: dict[str, int] | None = None,
) -> dict[str, Any]:
    """Return generation targets and runtime tiny-source gates for a lane.

    `width_px`/`height_px` describe the pre-crop standalone source or contact
    sheet cell target. For cropped runtime sprites, `min_longest_side_px` is the
    acceptance gate used to catch tiny draw-size proxies without rejecting every
    valid crop from a 256-ish source slot.
    """

    if group == "grounds":
        return {
            "profile": "ground_tile_source",
            "width_px": GROUND_TARGET_SOURCE_PX,
            "height_px": GROUND_TARGET_SOURCE_PX,
            "target_kind": "standalone_full_tile",
            "min_width_px": GROUND_MIN_SOURCE_PX,
            "min_height_px": GROUND_MIN_SOURCE_PX,
            "unit": "px",
            "source": "realm range-based source-quality contract",
            "range": {
                "minimum_px": GROUND_MIN_SOURCE_PX,
                "recommended_px": GROUND_TARGET_SOURCE_PX,
                "maximum_px": None,
            },
        }

    if group == "buildings":
        axis = footprint_axis(footprint)
        target_px = min(
            BUILDING_TARGET_MAX_PX,
            max(SPRITE_PRE_CROP_TARGET_PX, BUILDING_PRE_CROP_TARGET_PX_PER_FOOTPRINT_TILE * axis),
        )
        min_longest_side = max(
            SPRITE_RUNTIME_MIN_LONGEST_SIDE_PX,
            BUILDING_RUNTIME_MIN_LONGEST_SIDE_PX_PER_FOOTPRINT_TILE * axis,
        )
        policy: dict[str, Any] = {
            "profile": "building_footprint_sprite_source",
            "width_px": target_px,
            "height_px": target_px,
            "target_kind": "pre_crop_generation_slot",
            "min_longest_side_px": min_longest_side,
            "unit": "px",
            "source": "realm range-based source-quality contract",
            "range": {
                "minimum_longest_side_px_after_crop": min_longest_side,
                "pre_crop_target_px": target_px,
                "maximum_px": None,
            },
            "pre_crop_target_px_per_footprint_tile": BUILDING_PRE_CROP_TARGET_PX_PER_FOOTPRINT_TILE,
            "runtime_min_longest_side_px_per_footprint_tile": BUILDING_RUNTIME_MIN_LONGEST_SIDE_PX_PER_FOOTPRINT_TILE,
            "pre_crop_target_px_cap": BUILDING_TARGET_MAX_PX,
        }
        if footprint:
            width = max(1, int(footprint.get("w", 1) or 1))
            height = max(1, int(footprint.get("h", 1) or 1))
            policy["footprint"] = {"w": width, "h": height}
        return policy

    if group in SPRITE_LIKE_GROUPS:
        envelope_w, envelope_h = visual_envelope_axes(visual_envelope)
        width_px = SPRITE_PRE_CROP_TARGET_PX * envelope_w
        height_px = SPRITE_PRE_CROP_TARGET_PX * envelope_h
        min_longest_side = SPRITE_RUNTIME_MIN_LONGEST_SIDE_PX * max(envelope_w, envelope_h)
        return {
            "profile": "sprite_overlay_source",
            "width_px": width_px,
            "height_px": height_px,
            "target_kind": "pre_crop_generation_slot",
            "min_longest_side_px": min_longest_side,
            "unit": "px",
            "source": "realm range-based source-quality contract",
            "range": {
                "minimum_longest_side_px_after_crop": min_longest_side,
                "pre_crop_target_px": max(width_px, height_px),
                "maximum_px": None,
            },
            "visual_envelope": {"w": envelope_w, "h": envelope_h},
            "pre_crop_target_px_per_visual_tile": SPRITE_PRE_CROP_TARGET_PX,
            "runtime_min_longest_side_px_per_visual_tile": SPRITE_RUNTIME_MIN_LONGEST_SIDE_PX,
        }

    return {}


def source_canvas_scope(group: str) -> str:
    if group == "grounds":
        return "per standalone high-resolution ground source tile"
    if group == "buildings":
        return (
            "per generated building sheet cell or standalone pre-crop source; "
            "256px per footprint tile is the target capped at 1536px"
        )
    if group in {"units", "animals"}:
        return "per generated actor sheet cell or standalone pre-crop source"
    if group in {"features", "decals"}:
        return "per generated tile-anchored sheet cell or standalone pre-crop source"
    if group in {"projectiles", "effects", "user_interface"}:
        return "per generated overlay, projectile, effect, or UI sheet cell or standalone pre-crop source"
    return "per generated sheet cell or standalone pre-crop source"


def png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as handle:
        header = handle.read(24)
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        return (0, 0)
    width, height = struct.unpack(">II", header[16:24])
    return (int(width), int(height))


def resolution_gate_failure(width: int, height: int, policy: dict[str, Any]) -> str | None:
    min_longest_side = int(policy.get("min_longest_side_px", 0) or 0)
    if min_longest_side > 0:
        longest_side = max(width, height)
        if longest_side < min_longest_side:
            target_width = int(policy.get("width_px", 0) or 0)
            target_height = int(policy.get("height_px", 0) or 0)
            return (
                f"runtime PNG is {width}x{height}, below source-quality floor: "
                f"longest side must be at least {min_longest_side}px after crop; "
                f"pre-crop generation target is {target_width}x{target_height}"
            )
        return None

    min_width = int(policy.get("min_width_px", 0) or 0)
    min_height = int(policy.get("min_height_px", 0) or 0)
    if min_width <= 0 or min_height <= 0:
        return None
    if width < min_width or height < min_height:
        target_width = int(policy.get("width_px", 0) or 0)
        target_height = int(policy.get("height_px", 0) or 0)
        return (
            f"runtime PNG is {width}x{height}, below source-quality minimum "
            f"{min_width}x{min_height}; preferred target is {target_width}x{target_height}"
        )
    return None
