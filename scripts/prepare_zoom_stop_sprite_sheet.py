#!/usr/bin/env python3
"""Prepare an Image Gen sheet for native-size Realm sprite zoom stops."""

from __future__ import annotations

import argparse
import json
import re
import shutil
from datetime import datetime
from pathlib import Path
from typing import Any

from PIL import Image


CANVAS_SIZE = 1024
MAGENTA = "#ff00ff"
REPO_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_ZOOM_SOURCE = REPO_ROOT / "src" / "render" / "sdl" / "camera.cpp"


def slug_id(text: str) -> str:
    value = re.sub(r"[^A-Za-z0-9]+", "_", text.strip().lower()).strip("_")
    return value or "sprite"


def resolve_path(raw: str) -> Path:
    return Path(raw).expanduser()


def renderer_zoom_array_name(zoom_mode: str) -> str:
    return "kAsciiZoomTiles" if zoom_mode == "ascii" else "kTilesetZoomTiles"


def renderer_zoom_tiles(zoom_mode: str, source: Path) -> tuple[list[int], str]:
    array_name = renderer_zoom_array_name(zoom_mode)
    text = source.read_text(encoding="utf-8")
    match = re.search(rf"\b{array_name}\s*=\s*\{{(?P<body>.*?)\}};", text, flags=re.S)
    if not match:
        raise SystemExit(f"could not find {array_name} in {source}")
    tiles = [int(value) for value in re.findall(r"\b\d+\b", match.group("body"))]
    if not tiles:
        raise SystemExit(f"{array_name} in {source} does not contain any tile sizes")
    if sorted(set(tiles)) != tiles:
        raise SystemExit(f"{array_name} in {source} must be strictly increasing")
    return tiles, f"{source.as_posix()}::{array_name}"


def pick_zoom_tiles(available: list[int], count: int) -> list[int]:
    if count <= 0:
        raise SystemExit("--stops must be positive")
    if count >= len(available):
        return available
    selected: list[int] = []
    for index in range(count):
        pos = round(index * (len(available) - 1) / max(1, count - 1))
        tile = available[pos]
        if tile not in selected:
            selected.append(tile)
    cursor = 0
    while len(selected) < count and cursor < len(available):
        tile = available[cursor]
        if tile not in selected:
            selected.append(tile)
        cursor += 1
    return sorted(selected)


def selected_zoom_tiles(args: argparse.Namespace) -> tuple[list[int], list[int], str]:
    zoom_source = resolve_path(args.zoom_source)
    available, source_label = renderer_zoom_tiles(args.zoom_mode, zoom_source)
    min_tile = args.min_tile if args.min_tile is not None else available[0]
    max_tile = args.max_tile if args.max_tile is not None else available[-1]
    if min_tile <= 0 or max_tile < min_tile:
        raise SystemExit("invalid tile range")
    ranged = [tile for tile in available if min_tile <= tile <= max_tile]
    if not ranged:
        raise SystemExit(f"no renderer zoom tiles fall inside {min_tile}..{max_tile}")
    selected = ranged if args.stops is None else pick_zoom_tiles(ranged, args.stops)
    return ranged, selected, source_label


def transparent_to_magenta(image: Image.Image) -> Image.Image:
    rgba = image.convert("RGBA")
    background = Image.new("RGBA", rgba.size, MAGENTA)
    background.alpha_composite(rgba)
    return background.convert("RGB")


def fit_image(image: Image.Image, max_size: int, resample: Image.Resampling) -> Image.Image:
    rgba = image.convert("RGBA")
    ratio = min(max_size / rgba.width, max_size / rgba.height)
    width = max(1, int(round(rgba.width * ratio)))
    height = max(1, int(round(rgba.height * ratio)))
    return rgba.resize((width, height), resample)


def paste_centered(canvas: Image.Image, sprite: Image.Image, box: tuple[int, int, int, int]) -> None:
    x0, y0, x1, y1 = box
    x = x0 + ((x1 - x0) - sprite.width) // 2
    y = y0 + ((y1 - y0) - sprite.height) // 2
    canvas.alpha_composite(sprite.convert("RGBA"), (x, y))


def rect_dict(box: tuple[int, int, int, int]) -> dict[str, int]:
    x0, y0, x1, y1 = box
    return {"x": x0, "y": y0, "w": x1 - x0, "h": y1 - y0}


def packed_stop_cells(
    ordered_stops: list[tuple[int, int]],
    *,
    cell_padding: int,
    sheet_padding: int,
) -> list[tuple[int, int, int, int]]:
    available_width = CANVAS_SIZE - sheet_padding * 2
    available_height = CANVAS_SIZE - sheet_padding * 2
    if available_width <= 0 or available_height <= 0:
        raise SystemExit("--sheet-padding leaves no drawable sheet area")

    rows: list[dict[str, Any]] = []
    row: list[dict[str, int]] = []
    row_width = 0
    row_height = 0
    for _, sprite_px in ordered_stops:
        cell = sprite_px + cell_padding * 2
        if cell > available_width or cell > available_height:
            raise SystemExit(
                f"sprite size {sprite_px}px needs a {cell}px cell, which cannot fit on a 1024 sheet"
            )
        if row and row_width + cell > available_width:
            rows.append({"items": row, "width": row_width, "height": row_height})
            row = []
            row_width = 0
            row_height = 0
        row.append({"cell": cell, "sprite_px": sprite_px})
        row_width += cell
        row_height = max(row_height, cell)
    if row:
        rows.append({"items": row, "width": row_width, "height": row_height})

    total_height = sum(int(item["height"]) for item in rows)
    if total_height > available_height:
        raise SystemExit(
            f"{len(ordered_stops)} zoom stops need {total_height}px of packed row height, "
            f"but only {available_height}px is available; pass --stops for a smaller experimental sheet"
        )

    boxes: list[tuple[int, int, int, int]] = []
    y = sheet_padding + (available_height - total_height) // 2
    for packed_row in rows:
        x = sheet_padding + (available_width - int(packed_row["width"])) // 2
        for item in packed_row["items"]:
            cell = int(item["cell"])
            boxes.append((x, y, x + cell, y + cell))
            x += cell
        y += int(packed_row["height"])
    return boxes


def inferred_asset_profile(args: argparse.Namespace) -> str:
    if args.asset_profile != "auto":
        return args.asset_profile
    text = " ".join(
        str(value).lower()
        for value in (args.subject or "", args.source or "", args.out or "")
    )
    if args.contains_human or any(word in text for word in ("peasant", "militia", "archer", "spearman", "knight", "villager", "human", "unit")):
        return "human"
    if args.contains_animal or any(word in text for word in ("animal", "wolf", "boar", "deer", "sheep", "horse")):
        return "animal"
    if args.is_building or any(word in text for word in ("building", "house", "castle", "tower", "hall", "barracks", "dock", "mill", "camp", "stable", "market", "church")):
        return "building"
    if args.is_terrain or any(word in text for word in ("ground", "terrain", "grass", "water", "snow", "sand", "mud", "road")):
        return "terrain"
    if args.is_decal or "decal" in text:
        return "decal"
    if any(word in text for word in ("projectile", "arrow", "bolt", "boulder")):
        return "projectile"
    if any(word in text for word in ("effect", "impact", "marker", "ring", "ui")):
        return "effect"
    return "generic"


def profile_prompt_block(profile: str) -> str:
    common = """Progressive simplification is the most important rule:
- the largest version should remain as close to the original as possible
- each smaller version should be simplified more than the one before it
- the smaller the sprite gets, the fewer details it should contain
- do not simplify all sizes by the same amount

Use clear progressive level of detail:
- largest size: preserve the original detail
- next size down: remove tiny noise and small texture
- next size down: merge small details into larger readable shapes
- next size down: reduce internal detail heavily
- smallest sizes: keep only the essential silhouette and major colour blocks

Important line rule:
- black outlines and important identity marks should shrink more slowly than the sprite itself
- as sprites get smaller, keep important outlines relatively thicker and stronger so they remain readable

Simplify aggressively as size decreases:
- remove tiny texture
- remove noise and clutter
- remove tiny decorative details
- merge small details into larger readable shapes
- reduce the number of internal lines
- strengthen the silhouette
- keep colour areas bold and clearly separated
- slightly exaggerate important features if needed for readability"""

    if profile == "human":
        return common + """

Human readability rules:
- the face must remain legible at every size
- simplify the face instead of letting it become blurry or noisy
- eyes should remain visible, even as simple dots or marks
- the nose and mouth may become one or two minimal marks at small sizes
- keep clothing, armour, weapon, shield, and team-colour areas consistent with the reference

Human priority order:
1. silhouette
2. face and eyes
3. weapon and shield
4. main clothing and team-colour blocks
5. only details that still read clearly at that size"""

    if profile == "animal":
        return common + """

Animal readability rules:
- preserve the animal species, stance, head shape, body mass, legs, tail, horns, ears, or snout as applicable
- keep the eyes or head marks readable at every size, but do not make the animal look human
- keep the stylized medieval bestiary feel: simplified, slightly uncanny, emblematic, and readable as game art
- remove fur texture and tiny markings before sacrificing the head silhouette or stance

Animal priority order:
1. species silhouette
2. head, eye mark, and snout/ears/horns
3. stance and leg readability
4. main colour blocks
5. only markings that still read clearly at that size"""

    if profile == "building":
        return common + """

Building readability rules:
- preserve the footprint identity, roofline, entrance, towers, banners, windows, and large structural masses as applicable
- simplify masonry, wood grain, shingles, and tiny trim before changing the building silhouette
- keep team-colour flags, banners, shields, or roof markers readable if present
- do not add characters, scenery, terrain, or UI elements

Building priority order:
1. overall silhouette and roofline
2. entrance and main structural masses
3. team-colour markers if present
4. main material blocks
5. only texture lines that still read clearly at that size"""

    if profile == "terrain":
        return common + """

Terrain readability rules:
- preserve the material identity first: grass, mud, snow, water, stone, sand, lava, or similar
- simplify texture into larger patches as size decreases
- keep any approved slab or edge geometry coherent if it is part of the source
- do not add characters, faces, weapons, UI, or unrelated objects

Terrain priority order:
1. material identity
2. tile or slab silhouette if present
3. major light/dark patches
4. edge or contact shape if present
5. only texture detail that still reads clearly at that size"""

    if profile == "decal":
        return common + """

Decal readability rules:
- preserve the decal's flat mark, footprint, scatter pattern, or overlay identity
- simplify tiny pieces into fewer larger readable shapes as size decreases
- keep the decal suitable for being drawn over terrain
- do not add characters, terrain tiles, UI frames, or extra objects

Decal priority order:
1. decal footprint and silhouette
2. major marks or clusters
3. main colour blocks
4. only small fragments that still read clearly at that size"""

    if profile == "projectile":
        return common + """

Projectile readability rules:
- preserve direction, head/tip, tail, trail, flame, or payload identity as applicable
- exaggerate the silhouette and contrast at small sizes
- remove tiny texture before losing directionality
- do not add launchers, targets, terrain, UI, or impact effects unless they are part of the projectile

Projectile priority order:
1. direction and silhouette
2. head/tip or payload
3. tail/trail/flame marker
4. main colour cue
5. only details that still read clearly at that size"""

    if profile == "effect":
        return common + """

Effect and UI readability rules:
- preserve the icon, marker, impact, ring, or overlay purpose
- keep the shape centred and readable over light and dark terrain
- simplify particle detail into clear symbolic shapes at smaller sizes
- do not add characters, terrain, labels, or unrelated UI chrome

Effect priority order:
1. effect shape and gameplay meaning
2. centre/anchor readability
3. main colour cue
4. only interior details that still read clearly at that size"""

    return common + """

Generic sprite readability rules:
- preserve the source identity, silhouette, orientation, palette, and anchor
- simplify smallest versions into strong readable shapes rather than noisy miniature copies
- do not add unrelated objects, labels, terrain, or UI

Generic priority order:
1. silhouette
2. orientation and anchor
3. main colour blocks
4. only details that still read clearly at that size"""


def build_prompt(args: argparse.Namespace, stops: list[dict[str, Any]]) -> str:
    stop_lines = "\n".join(
        f"- stop {item['index'] + 1}: {item['sprite_px']} by {item['sprite_px']} px sprite, "
        f"tile zoom {item['tile_px']} px, box x={item['box']['x']} y={item['box']['y']} "
        f"w={item['box']['w']} h={item['box']['h']}"
        for item in stops
    )
    subject = args.subject or slug_id(Path(args.source).stem).replace("_", " ")
    profile = inferred_asset_profile(args)
    profile_block = profile_prompt_block(profile)
    return f"""Use the attached 1024 by 1024 magenta-background sheet as the exact layout.

Every sprite on the sheet is an exact in-game zoom-stop size for Realm's SDL tileset renderer.
The largest sprite is both the closest zoom-stop and the visual reference for identity, pose, silhouette, equipment, palette, lighting direction, and team-colour areas.

This is an image edit, not a new image generation.
Redraw the same {subject} as native clean small-RTS sprite art in each stop box. Do not merely resize the largest stop.
Asset prompt profile: {profile}.

Layout rules:
- Preserve the overall image structure, canvas, layout, and composition.
- Preserve the magenta #ff00ff background everywhere outside the sprite pixels.
- Keep each sprite centred in its original square stop box and keep every stop's canvas size unchanged.
- Do not rearrange sprites, spacing, alignment, order, or relative size progression.
- Do not add labels, text, borders, scenery, terrain, UI chrome, or extra sprites.
- Keep the largest stop almost unchanged except for minor cleanup if necessary.

Rendering rules:
- This is not a resize exercise.
- This is not a pixel-art exercise.
- Do not simply denoise or scale the existing sprites.
- Do not treat all versions equally.
- Do not turn the smaller versions into pixel art.
- Do not use visible square pixels, dithering, blocky pixel-art rendering, or aliased pixel-sprite styling.
- Redraw each sprite as if it had originally been designed for that exact display size, while still rendered cleanly at native resolution.

Identity rules:
- Keep the same character or object identity, pose, facing direction, silhouette, colours, equipment, and core visual identity in every version.
- Keep the stance or contact anchor stable.

{profile_block}

Zoom stops to redraw, listed largest to smallest:
{stop_lines}
"""


def command_prepare(args: argparse.Namespace) -> None:
    source = resolve_path(args.source)
    out = resolve_path(args.out)
    prompt_out = resolve_path(args.prompt_out) if args.prompt_out else out.with_suffix(".prompt.md")
    manifest_out = resolve_path(args.manifest_out) if args.manifest_out else out.with_suffix(".manifest.json")

    src = Image.open(source).convert("RGBA")
    available_tiles, tile_stops, zoom_source = selected_zoom_tiles(args)
    sprite_sizes = [max(1, int(tile * args.entity_scale)) for tile in tile_stops]
    ordered_stops = sorted(zip(tile_stops, sprite_sizes), key=lambda item: item[1], reverse=True)
    stop_cells = packed_stop_cells(
        ordered_stops,
        cell_padding=args.cell_padding,
        sheet_padding=args.sheet_padding,
    )

    canvas = Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), MAGENTA)

    stops: list[dict[str, Any]] = []
    for index, ((tile_px, sprite_px), cell_box) in enumerate(zip(ordered_stops, stop_cells)):
        cell_x0, cell_y0, cell_x1, cell_y1 = cell_box
        sprite = fit_image(src, sprite_px, Image.Resampling.LANCZOS)
        cell_w = cell_x1 - cell_x0
        sprite_box = (
            cell_x0 + (cell_w - sprite_px) // 2,
            cell_y0 + (cell_w - sprite_px) // 2,
            cell_x0 + (cell_w - sprite_px) // 2 + sprite_px,
            cell_y0 + (cell_w - sprite_px) // 2 + sprite_px,
        )
        paste_centered(canvas, sprite, sprite_box)
        stops.append(
            {
                "index": index,
                "tile_px": tile_px,
                "sprite_px": sprite_px,
                "cell_box": rect_dict(cell_box),
                "box": rect_dict(sprite_box),
                "layout_lane": "packed",
                "runtime_formula": f"int(tile_px * {args.entity_scale})",
            }
        )

    out.parent.mkdir(parents=True, exist_ok=True)
    transparent_to_magenta(canvas).save(out)

    manifest = {
        "schema": "realm.zoom_stop_sprite_sheet.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "source": source.as_posix(),
        "sheet": out.as_posix(),
        "prompt": prompt_out.as_posix(),
        "canvas": {"w": CANVAS_SIZE, "h": CANVAS_SIZE, "background": MAGENTA},
        "layout": {
            "mode": "packed_variable_cells",
            "order": "largest_to_smallest",
            "sheet_padding": args.sheet_padding,
            "cell_padding": args.cell_padding,
        },
        "zoom": {
            "mode": args.zoom_mode,
            "source": zoom_source,
            "min_tile_px": available_tiles[0],
            "max_tile_px": available_tiles[-1],
            "runtime_wheel_tiles": available_tiles,
            "selected_tiles": tile_stops,
            "visual_order_tiles": [int(item["tile_px"]) for item in stops],
            "visual_order": "largest_to_smallest",
            "entity_scale": args.entity_scale,
        },
        "stops": stops,
        "notes": [
            "Use this sheet as an Image Gen edit target.",
            "After generation, split accepted stop boxes from the edited sheet using this manifest.",
            "Runtime zoom-stop art is optional; only promoted and reviewed stops are loaded by the game.",
        ],
    }
    manifest_out.parent.mkdir(parents=True, exist_ok=True)
    manifest_out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    prompt_out.parent.mkdir(parents=True, exist_ok=True)
    prompt_out.write_text(build_prompt(args, stops), encoding="utf-8")
    print(f"wrote sheet: {out}")
    print(f"wrote manifest: {manifest_out}")
    print(f"wrote prompt: {prompt_out}")
    print(f"zoom source: {zoom_source}")
    print("selected tile zooms:", ", ".join(str(tile) for tile in tile_stops))
    print("selected sprite sizes:", ", ".join(str(size) for size in sprite_sizes))
    print("visual sprite order:", ", ".join(str(item["sprite_px"]) for item in stops))


def magenta_to_alpha(
    image: Image.Image,
    tolerance: int = 0,
    *,
    remove_spill: bool = False,
    alpha_threshold: int = 0,
) -> Image.Image:
    rgba = image.convert("RGBA")
    pixels = rgba.load()
    for y in range(rgba.height):
        for x in range(rgba.width):
            r, g, b, a = pixels[x, y]
            magenta_key = r >= 255 - tolerance and g <= tolerance and b >= 255 - tolerance
            magenta_spill = remove_spill and r >= 80 and b >= 80 and g <= 90 and abs(r - b) <= 110
            if a <= alpha_threshold or magenta_key or magenta_spill:
                pixels[x, y] = (r, g, b, 0)
            elif a > 0:
                pixels[x, y] = (r, g, b, a)
    return rgba


def is_magenta_pixel(pixel: tuple[int, int, int, int], tolerance: int) -> bool:
    r, g, b, a = pixel
    if a == 0:
        return True
    return r >= 255 - tolerance and g <= tolerance and b >= 255 - tolerance


def detect_non_magenta_components(image: Image.Image, *, tolerance: int, min_pixels: int) -> list[dict[str, Any]]:
    rgba = image.convert("RGBA")
    width, height = rgba.size
    pixels = rgba.load()
    visited = bytearray(width * height)
    components: list[dict[str, Any]] = []

    for start_y in range(height):
        for start_x in range(width):
            start_i = start_y * width + start_x
            if visited[start_i]:
                continue
            visited[start_i] = 1
            if is_magenta_pixel(pixels[start_x, start_y], tolerance):
                continue

            stack = [(start_x, start_y)]
            count = 0
            min_x = max_x = start_x
            min_y = max_y = start_y
            while stack:
                x, y = stack.pop()
                count += 1
                min_x = min(min_x, x)
                max_x = max(max_x, x)
                min_y = min(min_y, y)
                max_y = max(max_y, y)
                for nx, ny in ((x + 1, y), (x - 1, y), (x, y + 1), (x, y - 1)):
                    if nx < 0 or ny < 0 or nx >= width or ny >= height:
                        continue
                    ni = ny * width + nx
                    if visited[ni]:
                        continue
                    visited[ni] = 1
                    if not is_magenta_pixel(pixels[nx, ny], tolerance):
                        stack.append((nx, ny))

            if count >= min_pixels:
                components.append(
                    {
                        "pixels": count,
                        "box": (min_x, min_y, max_x + 1, max_y + 1),
                        "center": ((min_x + max_x + 1) / 2.0, (min_y + max_y + 1) / 2.0),
                        "size": (max_x - min_x + 1, max_y - min_y + 1),
                    }
                )
    return components


def order_stop_components(
    components: list[dict[str, Any]],
    expected_count: int,
    *,
    drop_largest_reference: bool,
) -> list[dict[str, Any]]:
    if len(components) < expected_count:
        raise SystemExit(f"detected {len(components)} candidate components, expected at least {expected_count}")
    by_area = sorted(components, key=lambda item: item["pixels"], reverse=True)
    if drop_largest_reference and len(by_area) > expected_count:
        candidates = by_area[1:expected_count + 1]
    else:
        candidates = by_area[:expected_count]
    if len(candidates) != expected_count:
        raise SystemExit(f"detected {len(candidates)} usable stop components, expected {expected_count}")
    return sorted(candidates, key=lambda item: item["pixels"], reverse=True)


def fit_component_to_stop(component: Image.Image, sprite_px: int, padding: int, args: argparse.Namespace) -> Image.Image:
    transparent = magenta_to_alpha(
        component,
        args.component_tolerance,
        remove_spill=args.remove_magenta_spill,
        alpha_threshold=args.alpha_threshold,
    )
    bbox = transparent.getbbox()
    if bbox:
        transparent = transparent.crop(bbox)
    max_size = max(1, sprite_px - padding * 2)
    ratio = min(max_size / transparent.width, max_size / transparent.height)
    width = max(1, int(round(transparent.width * ratio)))
    height = max(1, int(round(transparent.height * ratio)))
    transparent = transparent.resize((width, height), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (sprite_px, sprite_px), (0, 0, 0, 0))
    out.alpha_composite(transparent, ((sprite_px - width) // 2, (sprite_px - height) // 2))
    return out


def split_detected_components(
    sheet: Image.Image,
    stops: list[dict[str, Any]],
    out_dir: Path,
    args: argparse.Namespace,
    *,
    drop_largest_reference: bool,
) -> list[dict[str, Any]]:
    components = detect_non_magenta_components(
        sheet,
        tolerance=args.component_tolerance,
        min_pixels=args.component_min_pixels,
    )
    ordered = order_stop_components(
        components,
        len(stops),
        drop_largest_reference=drop_largest_reference,
    )
    written: list[dict[str, Any]] = []
    for stop, component in zip(stops, ordered):
        x0, y0, x1, y1 = component["box"]
        crop = sheet.crop((x0, y0, x1, y1))
        tile_px = int(stop["tile_px"])
        sprite_px = int(stop["sprite_px"])
        index = int(stop["index"])
        tile = fit_component_to_stop(crop, sprite_px, args.detected_padding, args)
        filename = f"stop_{index + 1:02d}_tile_{tile_px:03d}_sprite_{sprite_px:03d}.png"
        out = out_dir / filename
        if out.exists() and not args.force:
            raise SystemExit(f"{out} exists; pass --force to overwrite")
        tile.save(out)
        written.append(
            {
                "index": index,
                "tile_px": tile_px,
                "sprite_px": sprite_px,
                "path": out.as_posix(),
                "detected_box": {"x": x0, "y": y0, "w": x1 - x0, "h": y1 - y0},
                "detected_pixels": component["pixels"],
            }
        )
    return written


def command_split(args: argparse.Namespace) -> None:
    sheet_path = resolve_path(args.sheet)
    manifest_path = resolve_path(args.manifest)
    out_dir = resolve_path(args.out_dir)
    sheet = Image.open(sheet_path).convert("RGBA")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    canvas = manifest.get("canvas", {})
    expected_width = int(canvas.get("w", sheet.width)) if isinstance(canvas, dict) else sheet.width
    expected_height = int(canvas.get("h", sheet.height)) if isinstance(canvas, dict) else sheet.height
    if (sheet.width, sheet.height) != (expected_width, expected_height):
        if not args.normalize_canvas:
            raise SystemExit(
                f"{sheet_path} is {sheet.width}x{sheet.height}, but manifest canvas is "
                f"{expected_width}x{expected_height}; pass --normalize-canvas to resize before splitting"
            )
        sheet = sheet.resize((expected_width, expected_height), Image.Resampling.LANCZOS)
    stops = manifest.get("stops")
    if not isinstance(stops, list) or not stops:
        raise SystemExit(f"{manifest_path} does not contain zoom stops")

    out_dir.mkdir(parents=True, exist_ok=True)
    if args.detect_components:
        outputs = split_detected_components(
            sheet,
            stops,
            out_dir,
            args,
            drop_largest_reference=isinstance(manifest.get("reference"), dict),
        )
    else:
        outputs: list[dict[str, Any]] = []
        for stop in stops:
            box = stop.get("box")
            if not isinstance(box, dict):
                raise SystemExit("stop is missing box metadata")
            x = int(box["x"])
            y = int(box["y"])
            w = int(box["w"])
            h = int(box["h"])
            tile_px = int(stop["tile_px"])
            sprite_px = int(stop["sprite_px"])
            index = int(stop["index"])
            crop = sheet.crop((x, y, x + w, y + h))
            if args.transparent:
                crop = magenta_to_alpha(
                    crop,
                    args.component_tolerance,
                    remove_spill=args.remove_magenta_spill,
                    alpha_threshold=args.alpha_threshold,
                )
            filename = f"stop_{index + 1:02d}_tile_{tile_px:03d}_sprite_{sprite_px:03d}.png"
            out = out_dir / filename
            if out.exists() and not args.force:
                raise SystemExit(f"{out} exists; pass --force to overwrite")
            crop.save(out)
            outputs.append(
                {
                    "index": index,
                    "tile_px": tile_px,
                    "sprite_px": sprite_px,
                    "path": out.as_posix(),
                    "source_box": {"x": x, "y": y, "w": w, "h": h},
                }
            )

    split_manifest = {
        "schema": "realm.zoom_stop_sprite_split.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "sheet": sheet_path.as_posix(),
        "source_manifest": manifest_path.as_posix(),
        "transparent": bool(args.transparent),
        "detect_components": bool(args.detect_components),
        "outputs": outputs,
        "notes": [
            "These are candidate/workbench outputs until reviewed and wired into a runtime zoom-stop asset contract.",
            "Exact #ff00ff pixels were converted to alpha only when transparent=true.",
        ],
    }
    manifest_out = resolve_path(args.manifest_out) if args.manifest_out else out_dir / "split_manifest.json"
    manifest_out.write_text(json.dumps(split_manifest, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {len(outputs)} zoom stops: {out_dir}")
    print(f"wrote split manifest: {manifest_out}")


def zoom_asset_name(frame_index: int, sprite_px: int, kind: str) -> str:
    suffix = "_teammask.png" if kind == "teammask" else "_base.png"
    return f"frame_{frame_index:02d}_zoom_{sprite_px:03d}{suffix}"


def load_split_outputs(args: argparse.Namespace) -> list[dict[str, Any]]:
    if args.split_manifest:
        manifest = json.loads(resolve_path(args.split_manifest).read_text(encoding="utf-8"))
        outputs = manifest.get("outputs")
        if not isinstance(outputs, list) or not outputs:
            raise SystemExit(f"{args.split_manifest} does not contain split outputs")
        return outputs

    if not args.split_dir:
        raise SystemExit("pass --split-manifest or --split-dir")
    split_dir = resolve_path(args.split_dir)
    outputs: list[dict[str, Any]] = []
    for path in sorted(split_dir.glob("stop_*_sprite_*.png")):
        match = re.match(r"stop_(\d+)_tile_(\d+)_sprite_(\d+)\.png$", path.name)
        if not match:
            continue
        outputs.append(
            {
                "index": int(match.group(1)) - 1,
                "tile_px": int(match.group(2)),
                "sprite_px": int(match.group(3)),
                "path": path.as_posix(),
            }
        )
    if not outputs:
        raise SystemExit(f"no split outputs found in {split_dir}")
    return outputs


def command_promote(args: argparse.Namespace) -> None:
    outputs = load_split_outputs(args)
    frame_dir = resolve_path(args.assets_root) / args.entity / args.action / args.direction
    frame_dir.mkdir(parents=True, exist_ok=True)

    copied: list[dict[str, Any]] = []
    for item in outputs:
        source = resolve_path(str(item["path"]))
        sprite_px = int(item["sprite_px"])
        dest = frame_dir / zoom_asset_name(args.frame, sprite_px, args.kind)
        if dest.exists() and not args.force:
            raise SystemExit(f"{dest} exists; pass --force to overwrite")
        shutil.copy2(source, dest)
        copied.append(
            {
                "sprite_px": sprite_px,
                "source": source.as_posix(),
                "dest": dest.as_posix(),
                "kind": args.kind,
            }
        )

    manifest = {
        "schema": "realm.zoom_stop_sprite_promote.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "entity": args.entity,
        "action": args.action,
        "direction": args.direction,
        "frame": args.frame,
        "kind": args.kind,
        "outputs": copied,
        "notes": [
            "Runtime zoom-stop files are optional overrides. Missing stops fall back to the canonical frame.",
            "Base zoom stops use frame_XX_zoom_NNN_base.png. Team masks use frame_XX_zoom_NNN_teammask.png.",
        ],
    }
    manifest_out = (
        resolve_path(args.manifest_out)
        if args.manifest_out
        else Path("art") / "tiles" / "workbench" / "zoom-stops" / "promoted" / args.entity / args.action / args.direction / f"frame_{args.frame:02d}_{args.kind}_promote.json"
    )
    manifest_out.parent.mkdir(parents=True, exist_ok=True)
    manifest_out.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"promoted {len(copied)} {args.kind} zoom stops to {frame_dir}")
    print(f"wrote promote manifest: {manifest_out}")


def parse_canonical_frame_name(path: Path) -> int | None:
    match = re.match(r"frame_(\d+)_base\.png$", path.name)
    if not match:
        return None
    return int(match.group(1))


def discover_canonical_frames(assets_root: Path, args: argparse.Namespace) -> list[dict[str, Any]]:
    frames: list[dict[str, Any]] = []
    if not assets_root.exists():
        return frames
    for base in sorted(assets_root.glob("*/*/*/frame_*_base.png")):
        if "_zoom_" in base.name:
            continue
        rel = base.relative_to(assets_root)
        if len(rel.parts) != 4:
            continue
        entity, action, direction, filename = rel.parts
        frame_index = parse_canonical_frame_name(Path(filename))
        if frame_index is None:
            continue
        if args.entity and entity != args.entity:
            continue
        if args.action and action != args.action:
            continue
        if args.direction and direction != args.direction:
            continue
        if args.frame is not None and frame_index != args.frame:
            continue
        frames.append(
            {
                "entity": entity,
                "action": action,
                "direction": direction,
                "frame": frame_index,
                "base": base,
                "mask": base.with_name(f"frame_{frame_index:02d}_teammask.png"),
            }
        )
    return frames


def command_status(args: argparse.Namespace) -> None:
    assets_root = resolve_path(args.assets_root)
    available_tiles, selected_tiles, zoom_source = selected_zoom_tiles(args)
    expected_sizes = [max(1, int(tile * args.entity_scale)) for tile in selected_tiles]
    frames = discover_canonical_frames(assets_root, args)

    results: list[dict[str, Any]] = []
    missing_required = 0
    complete = 0
    for frame in frames:
        frame_dir = Path(frame["base"]).parent
        base_stops = []
        mask_stops = []
        frame_missing = []
        mask_expected = Path(frame["mask"]).exists()
        for tile_px, sprite_px in zip(selected_tiles, expected_sizes):
            base_path = frame_dir / zoom_asset_name(int(frame["frame"]), sprite_px, "base")
            mask_path = frame_dir / zoom_asset_name(int(frame["frame"]), sprite_px, "teammask")
            base_exists = base_path.exists()
            mask_exists = mask_path.exists()
            base_stops.append({"tile_px": tile_px, "sprite_px": sprite_px, "path": base_path.as_posix(), "exists": base_exists})
            mask_stops.append({"tile_px": tile_px, "sprite_px": sprite_px, "path": mask_path.as_posix(), "exists": mask_exists})
            if not base_exists:
                frame_missing.append(base_path.as_posix())
            if mask_expected and not mask_exists:
                frame_missing.append(mask_path.as_posix())
        ok = not frame_missing
        if ok:
            complete += 1
        missing_required += len(frame_missing)
        results.append(
            {
                "entity": frame["entity"],
                "action": frame["action"],
                "direction": frame["direction"],
                "frame": frame["frame"],
                "canonical_base": Path(frame["base"]).as_posix(),
                "canonical_mask": Path(frame["mask"]).as_posix() if mask_expected else None,
                "ok": ok,
                "missing": frame_missing,
                "base_stops": base_stops,
                "mask_stops": mask_stops if mask_expected else [],
            }
        )

    report = {
        "schema": "realm.zoom_stop_sprite_status.v1",
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "assets_root": assets_root.as_posix(),
        "zoom": {
            "source": zoom_source,
            "min_tile_px": available_tiles[0],
            "max_tile_px": available_tiles[-1],
            "runtime_wheel_tiles": available_tiles,
            "selected_tiles": selected_tiles,
            "expected_sprite_sizes": expected_sizes,
            "entity_scale": args.entity_scale,
        },
        "summary": {
            "frames_checked": len(results),
            "frames_complete": complete,
            "missing_required_files": missing_required,
            "ok": missing_required == 0,
        },
        "frames": results,
    }

    if args.out:
        out = resolve_path(args.out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        print(f"wrote status: {out}")

    if args.markdown_out:
        out_md = resolve_path(args.markdown_out)
        out_md.parent.mkdir(parents=True, exist_ok=True)
        lines = [
            "# Realm Zoom-Stop Sprite Status",
            "",
            f"- Frames checked: {len(results)}",
            f"- Frames complete: {complete}",
            f"- Missing required files: {missing_required}",
            f"- Expected sprite sizes: {', '.join(str(size) for size in expected_sizes)}",
            "",
            "| Asset | Frame | Status | Missing |",
            "| --- | ---: | --- | ---: |",
        ]
        for item in results:
            asset = f"{item['entity']}/{item['action']}/{item['direction']}"
            lines.append(f"| {asset} | {item['frame']} | {'ok' if item['ok'] else 'missing'} | {len(item['missing'])} |")
        out_md.write_text("\n".join(lines) + "\n", encoding="utf-8")
        print(f"wrote markdown status: {out_md}")

    print(json.dumps(report["summary"], indent=2))
    if args.fail and missing_required:
        raise SystemExit(1)


def command_sizes(args: argparse.Namespace) -> None:
    _, selected, _ = selected_zoom_tiles(args)
    for tile in selected:
        print(f"{tile}\t{max(1, int(tile * args.entity_scale))}")


def add_common_zoom_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--zoom-mode", choices=("tileset", "ascii"), default="tileset")
    parser.add_argument("--zoom-source", default=str(DEFAULT_ZOOM_SOURCE))
    parser.add_argument("--min-tile", type=int)
    parser.add_argument("--max-tile", type=int)
    parser.add_argument("--stops", type=int, help="optional sampled subset; default uses every renderer zoom stop")
    parser.add_argument("--entity-scale", type=float, default=1.55)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    prepare = sub.add_parser("prepare", help="build a 1024x1024 magenta Image Gen zoom-stop sheet")
    prepare.add_argument("--source", required=True)
    prepare.add_argument("--out", required=True)
    prepare.add_argument("--subject", help="human-readable subject name for the prompt")
    prepare.add_argument(
        "--asset-profile",
        choices=("auto", "human", "animal", "building", "terrain", "decal", "projectile", "effect", "generic"),
        default="auto",
    )
    prepare.add_argument("--contains-human", action="store_true")
    prepare.add_argument("--contains-animal", action="store_true")
    prepare.add_argument("--is-building", action="store_true")
    prepare.add_argument("--is-terrain", action="store_true")
    prepare.add_argument("--is-decal", action="store_true")
    prepare.add_argument("--prompt-out")
    prepare.add_argument("--manifest-out")
    prepare.add_argument("--cell-padding", type=int, default=8)
    prepare.add_argument("--sheet-padding", type=int, default=8)
    add_common_zoom_args(prepare)
    prepare.set_defaults(func=command_prepare)

    split = sub.add_parser("split", help="split an edited zoom-stop sheet using its manifest")
    split.add_argument("--sheet", required=True)
    split.add_argument("--manifest", required=True)
    split.add_argument("--out-dir", required=True)
    split.add_argument("--manifest-out")
    split.add_argument("--transparent", action="store_true", help="convert exact #ff00ff pixels to alpha")
    split.add_argument("--normalize-canvas", action="store_true", help="resize generated sheets back to the manifest canvas before cropping")
    split.add_argument("--detect-components", action="store_true", help="detect non-magenta sprite components instead of using fixed manifest boxes")
    split.add_argument("--component-tolerance", type=int, default=36)
    split.add_argument("--component-min-pixels", type=int, default=16)
    split.add_argument("--detected-padding", type=int, default=0)
    split.add_argument("--remove-magenta-spill", action="store_true")
    split.add_argument("--alpha-threshold", type=int, default=0)
    split.add_argument("--force", action="store_true")
    split.set_defaults(func=command_split)

    promote = sub.add_parser("promote", help="copy split zoom stops into the runtime entity asset convention")
    promote.add_argument("--split-manifest", help="split_manifest.json produced by the split command")
    promote.add_argument("--split-dir", help="directory containing stop_XX_tile_YYY_sprite_ZZZ.png files")
    promote.add_argument("--assets-root", default="assets/tiles/entities")
    promote.add_argument("--entity", required=True)
    promote.add_argument("--action", required=True)
    promote.add_argument("--direction", required=True)
    promote.add_argument("--frame", type=int, required=True)
    promote.add_argument("--kind", choices=("base", "teammask"), default="base")
    promote.add_argument("--manifest-out")
    promote.add_argument("--force", action="store_true")
    promote.set_defaults(func=command_promote)

    status = sub.add_parser("status", help="report missing runtime zoom-stop sprites")
    status.add_argument("--assets-root", default="assets/tiles/entities")
    status.add_argument("--entity")
    status.add_argument("--action")
    status.add_argument("--direction")
    status.add_argument("--frame", type=int)
    status.add_argument("--out")
    status.add_argument("--markdown-out")
    status.add_argument("--fail", action="store_true")
    add_common_zoom_args(status)
    status.set_defaults(func=command_status)

    sizes = sub.add_parser("sizes", help="print selected tile zooms and resulting entity sprite sizes")
    add_common_zoom_args(sizes)
    sizes.set_defaults(func=command_sizes)
    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
