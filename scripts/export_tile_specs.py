#!/usr/bin/env python3
"""Export Realm sprite specification JSON from game data and the tileset audit."""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
ENTITY_RANGES = {
    "units": ("E_PEASANT", "E_RAM"),
    "buildings": ("E_TOWNHALL", "E_DOCK"),
    "animals": ("E_DEER", "E_BOAR"),
}
PLAYER_TEAM_COLOR_CATEGORIES = {"units", "buildings"}
PEASANT_SPEC = ROOT / "art" / "tiles" / "workbench" / "peasant" / "unit_spec.json"
FEATURE_TERRAINS = {
    "T_FOREST", "T_PINE", "T_PALM", "T_DEAD_TREE",
    "T_MOUNTAIN", "T_STONE", "T_REEDS", "T_GOLD",
    "T_WHEAT", "T_BERRY", "T_FISH", "T_RUINS",
    "T_CASTLE_WALL", "T_CASTLE_GATE",
}
DECAL_TERRAINS = {"T_TALL_GRASS", "T_FLOWERS"}


def terrain_layer_group(enum_name: str) -> str:
    if enum_name in FEATURE_TERRAINS:
        return "features"
    if enum_name in DECAL_TERRAINS:
        return "decals"
    return "grounds"


def lower_slug(text: str) -> str:
    slug = re.sub(r"[^a-zA-Z0-9]+", "_", text.strip().lower()).strip("_")
    return slug or "unknown"


def terrain_projection(enum_name: str) -> str:
    if enum_name in FEATURE_TERRAINS:
        return "transparent feature sprite anchored over a projected isometric map tile"
    if enum_name in DECAL_TERRAINS:
        return "transparent low ground decal over a top-down source tile"
    return "top-down square source tile, projected into an isometric diamond in-app"


def clean_cell(text: str) -> str:
    text = text.strip()
    text = re.sub(r"<br\s*/?>", "; ", text, flags=re.I)
    text = text.replace("`", "")
    text = re.sub(r"\s+", " ", text)
    return text.strip()


def split_list(text: str) -> list[str]:
    text = clean_cell(text)
    if not text or text.lower() in {"none", "n/a", "-"}:
        return []
    pieces = re.split(r",|;", text)
    return [piece.strip() for piece in pieces if piece.strip()]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def enum_values(source: str, enum_name: str) -> list[str]:
    match = re.search(rf"enum\s+{enum_name}\s*\{{(?P<body>.*?)\}};", source, re.S)
    if not match:
        raise RuntimeError(f"could not find enum {enum_name}")
    body = re.sub(r"//.*", "", match.group("body"))
    values: list[str] = []
    for raw in body.split(","):
        name = raw.strip()
        if not name:
            continue
        name = name.split("=")[0].strip()
        if name:
            values.append(name)
    return values


def parse_stats(source: str) -> dict[str, dict[str, Any]]:
    match = re.search(r"const\s+EntityStats\s+STATS\[\]\s*=\s*\{(?P<body>.*?)\};", source, re.S)
    if not match:
        raise RuntimeError("could not find STATS table")
    records: list[list[str]] = []
    for line in match.group("body").splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        body = line.strip().removeprefix("{").removesuffix(",").removesuffix("}")
        records.append(next(csv.reader([body], skipinitialspace=True)))

    realm_h = read_text(ROOT / "include" / "realm.h")
    entity_names = enum_values(realm_h, "EntityType")
    out: dict[str, dict[str, Any]] = {}
    for enum_name, fields in zip(entity_names, records):
        if len(fields) < 16:
            raise RuntimeError(f"bad STATS row for {enum_name}: {fields}")
        out[enum_name] = {
            "enum": enum_name,
            "name": fields[0],
            "slug": lower_slug(fields[0]),
            "glyph": fields[1].strip().strip("'"),
            "max_hp": int(fields[2]),
            "attack": int(fields[3]),
            "range": int(fields[4]),
            "speed": int(fields[5]),
            "attack_speed_ticks": int(fields[6]),
            "cost_gold": int(fields[7]),
            "cost_wood": int(fields[8]),
            "train_time_ticks": int(fields[9]),
            "footprint": {"w": int(fields[10]), "h": int(fields[11])},
            "supply_provided": int(fields[12]),
            "supply_used": int(fields[13]),
            "is_building": fields[14].strip() == "true",
            "traits": [part.strip() for part in fields[15].split("|") if part.strip() and part.strip() != "0"],
        }
    return out


def parse_terrain_names(source: str) -> dict[str, str]:
    return {
        enum_name: name
        for enum_name, name in re.findall(r"case\s+(T_\w+):\s*return\s+\"([^\"]+)\";", source)
    }


def parse_terrain_glyphs(source: str) -> dict[str, str]:
    glyphs: dict[str, str] = {}
    for enum_name, glyph in re.findall(r"case\s+(T_\w+):\s*return\s+'([^']*)';", source):
        glyphs[enum_name] = glyph
    return glyphs


def parse_audit_tables(markdown: str) -> tuple[dict[str, dict[str, str]], dict[str, dict[str, str]]]:
    entities: dict[str, dict[str, str]] = {}
    terrains: dict[str, dict[str, str]] = {}
    for raw in markdown.splitlines():
        line = raw.strip()
        if not line.startswith("| `"):
            continue
        cells = [clean_cell(cell) for cell in line.strip("|").split("|")]
        if not cells:
            continue
        key = cells[0]
        if key.startswith("E_") and len(cells) >= 6:
            if len(cells) >= 7:
                entities[key] = {
                    "name": cells[1],
                    "role": cells[3],
                    "visual_design": cells[4],
                    "team_color_slots": cells[5],
                    "required_states": cells[6],
                }
            else:
                entities[key] = {
                    "name": cells[1],
                    "role": cells[3],
                    "visual_design": cells[4],
                    "team_color_slots": "",
                    "required_states": cells[5],
                }
        elif key.startswith("T_") and len(cells) >= 6:
            terrains[key] = {
                "ui_name": cells[1],
                "terrain_type": cells[2],
                "visual_design": cells[3],
                "required_variants": cells[4],
                "runtime_notes": cells[5],
            }
    return entities, terrains


def category_for_entity(enum_name: str, entity_order: list[str]) -> str | None:
    index = entity_order.index(enum_name)
    for category, (start, end) in ENTITY_RANGES.items():
        if entity_order.index(start) <= index <= entity_order.index(end):
            return category
    return None


def default_entity_actions(category: str, required_states: str) -> list[dict[str, Any]]:
    actions = []
    for state in split_list(required_states):
        action_id = lower_slug(state.replace("/", " "))
        if not action_id:
            continue
        actions.append(
            {
                "id": action_id,
                "description": state,
                "source": "docs/tileset/realm_tileset_visual_audit.md",
                "frames_recommended": 2 if category != "buildings" else 1,
            }
        )
    return actions


def peasant_actions() -> list[dict[str, Any]]:
    if not PEASANT_SPEC.exists():
        return []
    spec = json.loads(PEASANT_SPEC.read_text(encoding="utf-8"))
    return spec.get("actions", [])


def entity_spec(
    enum_name: str,
    category: str,
    stats: dict[str, Any],
    audit: dict[str, str],
) -> dict[str, Any]:
    actions = peasant_actions() if enum_name == "E_PEASANT" else default_entity_actions(
        category, audit.get("required_states", "")
    )
    team_color_required = category in PLAYER_TEAM_COLOR_CATEGORIES
    return {
        "schema": "realm.sprite_spec.v1",
        "asset_type": category[:-1] if category.endswith("s") else category,
        "enum": enum_name,
        "slug": stats["slug"],
        "name": stats["name"],
        "runtime": {
            "glyph": stats["glyph"],
            "stats": {
                "max_hp": stats["max_hp"],
                "attack": stats["attack"],
                "range": stats["range"],
                "speed": stats["speed"],
                "attack_speed_ticks": stats["attack_speed_ticks"],
                "cost_gold": stats["cost_gold"],
                "cost_wood": stats["cost_wood"],
                "train_time_ticks": stats["train_time_ticks"],
                "supply_provided": stats["supply_provided"],
                "supply_used": stats["supply_used"],
                "traits": stats["traits"],
            },
            "footprint": stats["footprint"],
            "team_color_required": team_color_required,
        },
        "art": {
            "projection": "upright sprite anchored over projected isometric map tiles",
            "directions": ["front", "back"] if category != "buildings" else ["south"],
            "runtime_mirrors_horizontal": category != "buildings",
            "team_color_slots": split_list(audit.get("team_color_slots", "")) if team_color_required else [],
            "visual_design": audit.get("visual_design", ""),
            "source_role": audit.get("role", ""),
        },
        "states": split_list(audit.get("required_states", "")),
        "actions": actions,
        "paths": {
            "runtime_root": f"assets/tiles/entities/{stats['slug']}",
            "manifest": f"assets/tiles/entities/{stats['slug']}/manifest.json",
        },
        "sources": [
            "include/realm.h",
            "src/globals.cpp",
            "docs/tileset/realm_tileset_visual_audit.md",
        ],
    }


def terrain_spec(
    enum_name: str,
    runtime_name: str,
    glyph: str,
    audit: dict[str, str],
) -> dict[str, Any]:
    slug = lower_slug(enum_name.removeprefix("T_"))
    return {
        "schema": "realm.terrain_sprite_spec.v1",
        "asset_type": terrain_layer_group(enum_name).removesuffix("s"),
        "enum": enum_name,
        "slug": slug,
        "name": runtime_name,
        "ui_name": audit.get("ui_name", runtime_name),
        "runtime": {
            "glyph": glyph,
        },
        "art": {
            "projection": terrain_projection(enum_name),
            "layer_category": terrain_layer_group(enum_name),
            "terrain_type": audit.get("terrain_type", ""),
            "visual_design": audit.get("visual_design", ""),
            "required_variants": audit.get("required_variants", ""),
            "runtime_notes": audit.get("runtime_notes", ""),
            "team_color_required": False,
        },
        "paths": {
            "runtime_root": f"assets/tiles/terrain/{slug}",
            "base": f"assets/tiles/terrain/{slug}.png",
        },
        "sources": [
            "include/realm.h",
            "src/entity.cpp",
            "src/gfx_renderer.cpp",
            "docs/tileset/realm_tileset_visual_audit.md",
        ],
    }


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=False, ensure_ascii=True) + "\n", encoding="utf-8")


def export_specs(out_dir: Path, clean: bool) -> dict[str, Any]:
    realm_h = read_text(ROOT / "include" / "realm.h")
    globals_cpp = read_text(ROOT / "src" / "globals.cpp")
    entity_cpp = read_text(ROOT / "src" / "entity.cpp")
    gfx_renderer_cpp = read_text(ROOT / "src" / "gfx_renderer.cpp")
    audit_md = read_text(ROOT / "docs" / "tileset" / "realm_tileset_visual_audit.md")

    entity_order = enum_values(realm_h, "EntityType")
    terrain_order = enum_values(realm_h, "Terrain")
    stats = parse_stats(globals_cpp)
    terrain_names = parse_terrain_names(entity_cpp)
    terrain_glyphs = parse_terrain_glyphs(gfx_renderer_cpp)
    entity_audit, terrain_audit = parse_audit_tables(audit_md)

    if clean and out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    index: dict[str, Any] = {
        "schema": "realm.tile_specs_index.v1",
        "generated_by": "scripts/export_tile_specs.py",
        "groups": {
            "grounds": [], "features": [], "decals": [],
            "units": [], "animals": [], "buildings": [],
        },
    }

    for enum_name in entity_order:
        category = category_for_entity(enum_name, entity_order)
        if not category:
            continue
        record = stats[enum_name]
        spec = entity_spec(enum_name, category, record, entity_audit.get(enum_name, {}))
        rel = Path(category) / f"{record['slug']}.json"
        write_json(out_dir / rel, spec)
        index["groups"][category].append(
            {"enum": enum_name, "name": record["name"], "slug": record["slug"], "path": rel.as_posix()}
        )

    for enum_name in terrain_order:
        runtime_name = terrain_names.get(enum_name, enum_name.removeprefix("T_").replace("_", " ").title())
        glyph = terrain_glyphs.get(enum_name, "")
        slug = lower_slug(enum_name.removeprefix("T_"))
        spec = terrain_spec(enum_name, runtime_name, glyph, terrain_audit.get(enum_name, {}))
        group = terrain_layer_group(enum_name)
        rel = Path(group) / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"][group].append(
            {"enum": enum_name, "name": runtime_name, "slug": slug, "path": rel.as_posix()}
        )

    write_json(out_dir / "index.json", index)
    return index


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="art/tiles/image-json", help="output directory")
    parser.add_argument("--clean", action="store_true", help="remove existing output directory first")
    args = parser.parse_args()

    index = export_specs((ROOT / args.out).resolve(), args.clean)
    counts = {key: len(value) for key, value in index["groups"].items()}
    print(
        "exported "
        + ", ".join(f"{count} {name}" for name, count in counts.items())
        + f" to {args.out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
