#!/usr/bin/env python3
"""Export Realm sprite specification JSON from game data and the tileset audit."""

from __future__ import annotations

import argparse
import csv
import itertools
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
PLAYER_SIGIL = {
    "id": "player-sigil",
    "description": "white diagonal stripe running from top left to bottom right",
}
RESEARCH_VISUAL_LINES = {
    "weapon_material": {
        "name": "weapon material",
        "entities": ["E_MILITIA", "E_KNIGHT"],
        "tiers": [
            {
                "id": "basic_weapons",
                "name": "Basic weapons",
                "research": None,
                "description": "starting equipment with wooden hafts, leather grips, dull bronze or scrap-metal blades, and minimal metal reinforcement",
            },
            {
                "id": "iron_weapons",
                "name": "Iron Weapons",
                "research": "Iron Weapons",
                "description": "upgraded equipment with bright iron blades, iron spear or lance tips, iron rivets, and reinforced shield rims",
            },
        ],
    },
    "archer_weapon": {
        "name": "archer weapon",
        "entities": ["E_ARCHER"],
        "tiers": [
            {
                "id": "self_bow",
                "name": "Self Bow",
                "research": None,
                "description": "starting archer equipment with a simple wooden self bow, bowstring, cloth quiver, and standard arrows",
            },
            {
                "id": "crossbow",
                "name": "Crossbows",
                "research": "Crossbows",
                "description": "upgraded archer equipment with a compact wooden crossbow, metal bow arms, bolt quiver, and short crossbow bolts",
            },
        ],
    },
    "spear_reach": {
        "name": "spear reach",
        "entities": ["E_SPEARMAN"],
        "tiers": [
            {
                "id": "short_spear",
                "name": "Short Spear",
                "research": None,
                "description": "starting spear equipment with a medium wooden spear and small iron or bronze spearhead",
            },
            {
                "id": "pike",
                "name": "Pikes",
                "research": "Pikes",
                "description": "upgraded spear equipment with a much longer pike shaft, larger iron pike head, and stronger bracing pose",
            },
        ],
    },
    "trebuchet_mechanism": {
        "name": "trebuchet mechanism",
        "entities": ["E_TREBUCHET"],
        "tiers": [
            {
                "id": "traction_trebuchet",
                "name": "Traction Trebuchet",
                "research": None,
                "description": "starting trebuchet mechanism with rope-pull rigging, lighter frame, small ballast, and visible winch work",
            },
            {
                "id": "counterweight_trebuchet",
                "name": "Counterweight",
                "research": "Counterweight",
                "description": "upgraded trebuchet mechanism with a large box counterweight, heavier braced frame, stronger axle, and faster-ready silhouette",
            },
        ],
    },
    "knight_helmet": {
        "name": "knight helmet",
        "entities": ["E_KNIGHT"],
        "tiers": [
            {
                "id": "open_helmet",
                "name": "Open Helmet",
                "research": None,
                "description": "starting cavalry armour with an open nasal helmet, visible face, simple mail coif, and lighter shoulder protection",
            },
            {
                "id": "plate_helm",
                "name": "Plate Helm",
                "research": "Plate Helm",
                "description": "upgraded cavalry armour with a closed plate helm, stronger cheek guards, brighter metal brow, and heavier neck protection",
            },
        ],
    },
}
PLAYER_COLOURS = {
    "blue": {"name": "blue", "hex": "#00AFFF"},
    "red": {"name": "red", "hex": "#FF0000"},
    "green": {"name": "green", "hex": "#00B050"},
}
BLUE_CONTEXT_KEYWORDS = {
    "blue", "water", "naval", "ship", "warship", "boat", "dock", "fishing", "fish",
    "shoal", "wave", "sail", "galley", "skiff", "barge", "ferry",
}
RED_CONTEXT_KEYWORDS = {"red", "flame", "flaming", "burning", "lava", "ember"}
RED_CONTEXT_PHRASES = {"fire ship", "fireship"}
OPERATED_UNIT_ENUMS = {"E_CATAPULT", "E_TREBUCHET", "E_RAM"}
AMMUNITION_BY_ENTITY = {
    "E_ARCHER": ["arrow", "crossbow_bolt"],
    "E_CATAPULT": ["catapult_boulder"],
    "E_TREBUCHET": ["trebuchet_boulder"],
    "E_WARSHIP": ["warship_arrow_volley"],
    "E_TOWER": ["tower_bolt"],
    "E_CASTLE": ["tower_bolt", "trebuchet_boulder"],
}
AMMUNITION_SPECS = [
    {
        "slug": "arrow",
        "name": "Arrow",
        "description": "standard arrow ammunition after release",
        "states": [
            {"id": "in_flight", "description": "single arrow in flight, readable diagonal silhouette, no bow or archer"},
        ],
    },
    {
        "slug": "crossbow_bolt",
        "name": "Crossbow Bolt",
        "description": "compact crossbow bolt ammunition after release",
        "states": [
            {"id": "in_flight", "description": "short bolt in flight, strong head and shaft silhouette, no crossbow"},
        ],
    },
    {
        "slug": "flaming_arrow",
        "name": "Flaming Arrow",
        "description": "future flaming arrow ammunition with a tiny flame loop",
        "states": [
            {"id": "flame_frame_1", "description": "arrow in flight with small flame flicker frame 1"},
            {"id": "flame_frame_2", "description": "arrow in flight with shifted small flame flicker frame 2"},
        ],
    },
    {
        "slug": "tower_bolt",
        "name": "Tower Bolt",
        "description": "heavy defensive bolt fired by towers or garrisons",
        "states": [
            {"id": "in_flight", "description": "heavy bolt in flight, readable at small RTS scale, no tower"},
        ],
    },
    {
        "slug": "warship_arrow_volley",
        "name": "Warship Arrow Volley",
        "description": "small grouped naval arrow volley after release",
        "states": [
            {"id": "volley_frame_1", "description": "compact arrow volley in flight frame 1, no ship or water wake"},
            {"id": "volley_frame_2", "description": "compact arrow volley in flight frame 2 with shifted arrows, no ship or water wake"},
        ],
    },
    {
        "slug": "catapult_boulder",
        "name": "Catapult Boulder",
        "description": "catapult boulder ammunition after release",
        "states": [
            {"id": "in_flight", "description": "single rough boulder in flight, no catapult or impact dust"},
        ],
    },
    {
        "slug": "trebuchet_boulder",
        "name": "Trebuchet Boulder",
        "description": "larger trebuchet boulder ammunition after release",
        "states": [
            {"id": "in_flight", "description": "large rough boulder in flight, no trebuchet or impact dust"},
        ],
    },
]
ENTITY_ART_FALLBACKS = {
    "E_ARCHER": {
        "required_states": "idle, walk, aim, release, reload, dead, decayed skeleton with bow and quiver",
    },
    "E_KNIGHT": {
        "required_states": "idle, trot, charge/strike, hit/alert, dead, decayed skeleton with armour horse gear and weapon",
    },
    "E_SPEARMAN": {
        "role": "anti-cavalry infantry",
        "visual_design": "Infantry soldier with long spear, small shield, simple helmet, and cloth accents",
        "team_color_slots": "small shield, spear pennon, shoulder sash",
        "required_states": "idle, walk, spear thrust, brace/hold-position, hit/alert, dead, decayed skeleton with spear and shield",
    },
    "E_TREBUCHET": {
        "role": "heavy siege ranged, long-range building breaker",
        "visual_design": "Tall counterweight trebuchet with wooden frame, sling, wheels, and one visible operator",
        "team_color_slots": "small pennant, side shield plaque, operator cloth",
        "required_states": "idle, roll, load, fire, recoil, damaged/alert, destroyed wreck, decayed wreckage",
    },
}
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


def entity_profile(enum_name: str, audit: dict[str, str]) -> dict[str, str]:
    fallback = ENTITY_ART_FALLBACKS.get(enum_name, {})
    return {
        "role": audit.get("role") or fallback.get("role", ""),
        "visual_design": audit.get("visual_design") or fallback.get("visual_design", ""),
        "team_color_slots": audit.get("team_color_slots") or fallback.get("team_color_slots", ""),
        "required_states": fallback.get("required_states") or audit.get("required_states", ""),
    }


def is_military_unit(category: str, stats: dict[str, Any]) -> bool:
    return category == "units" and "TR_MILITARY" in stats.get("traits", [])


def is_operated_unit(enum_name: str) -> bool:
    return enum_name in OPERATED_UNIT_ENUMS


def ammunition_refs_for_entity(enum_name: str) -> list[str]:
    return AMMUNITION_BY_ENTITY.get(enum_name, [])


def research_visual_lines_for_entity(enum_name: str) -> list[dict[str, Any]]:
    return [
        {
            "id": line_id,
            "name": line["name"],
            "tiers": line["tiers"],
        }
        for line_id, line in RESEARCH_VISUAL_LINES.items()
        if enum_name in line["entities"]
    ]


def research_tier_variants_for_entity(enum_name: str) -> list[dict[str, Any]]:
    lines = research_visual_lines_for_entity(enum_name)
    if not lines:
        return []
    variants: list[dict[str, Any]] = []
    for combo in itertools.product(*[line["tiers"] for line in lines]):
        researched = [tier["research"] for tier in combo if tier.get("research")]
        variants.append(
            {
                "id": "__".join(tier["id"] for tier in combo),
                "name": " + ".join(tier["name"] for tier in combo),
                "description": "; ".join(tier["description"] for tier in combo),
                "research": researched,
                "is_default": not researched,
            }
        )
    return variants


def apply_research_variants_to_actions(enum_name: str, actions: list[dict[str, Any]]) -> list[dict[str, Any]]:
    variants = research_tier_variants_for_entity(enum_name)
    if not variants:
        return actions
    expanded: list[dict[str, Any]] = []
    for variant in variants:
        for action in actions:
            item = dict(action)
            item["id"] = f"{variant['id']}__{action['id']}"
            item["description"] = f"{variant['name']}: {action['description']}; {variant['description']}"
            item["research_visual_variant"] = {
                "id": variant["id"],
                "name": variant["name"],
                "research": variant["research"],
                "is_default": variant["is_default"],
            }
            expanded.append(item)
    return expanded


def recommended_player_colour(enum_name: str, stats: dict[str, Any], audit: dict[str, str]) -> dict[str, str]:
    profile = entity_profile(enum_name, audit)
    context = " ".join(
        [
            enum_name,
            stats.get("name", ""),
            profile.get("role", ""),
            profile.get("visual_design", ""),
            profile.get("team_color_slots", ""),
            profile.get("required_states", ""),
        ]
    ).lower()
    tokens = set(re.findall(r"[a-z0-9]+", context))
    has_blue_context = any(keyword in tokens or f" {keyword} " in f" {context} " for keyword in BLUE_CONTEXT_KEYWORDS)
    has_red_context = (
        any(keyword in tokens or f" {keyword} " in f" {context} " for keyword in RED_CONTEXT_KEYWORDS)
        or any(phrase in context for phrase in RED_CONTEXT_PHRASES)
    )
    if has_blue_context and has_red_context:
        return PLAYER_COLOURS["green"]
    if has_blue_context:
        return PLAYER_COLOURS["red"]
    return PLAYER_COLOURS["blue"]


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
    match = re.search(r"const\s+EntityStats\s+STATS\[[^\]]*\]\s*=\s*\{(?P<body>.*?)\};", source, re.S)
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
        if len(fields) >= 17:
            cost_food_i = 7
            cost_gold_i = 8
            cost_wood_i = 9
            train_time_i = 10
            footprint_w_i = 11
            footprint_h_i = 12
            supply_provided_i = 13
            supply_used_i = 14
            is_building_i = 15
            traits_i = 16
        else:
            cost_food_i = None
            cost_gold_i = 7
            cost_wood_i = 8
            train_time_i = 9
            footprint_w_i = 10
            footprint_h_i = 11
            supply_provided_i = 12
            supply_used_i = 13
            is_building_i = 14
            traits_i = 15
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
            "cost_food": int(fields[cost_food_i]) if cost_food_i is not None else 0,
            "cost_gold": int(fields[cost_gold_i]),
            "cost_wood": int(fields[cost_wood_i]),
            "train_time_ticks": int(fields[train_time_i]),
            "footprint": {"w": int(fields[footprint_w_i]), "h": int(fields[footprint_h_i])},
            "supply_provided": int(fields[supply_provided_i]),
            "supply_used": int(fields[supply_used_i]),
            "is_building": fields[is_building_i].strip() == "true",
            "traits": [part.strip() for part in fields[traits_i].split("|") if part.strip() and part.strip() != "0"],
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
    profile = entity_profile(enum_name, audit)
    actions = peasant_actions() if enum_name == "E_PEASANT" else default_entity_actions(
        category, profile.get("required_states", "")
    )
    actions = apply_research_variants_to_actions(enum_name, actions)
    team_color_required = category in PLAYER_TEAM_COLOR_CATEGORIES
    player_colour = recommended_player_colour(enum_name, stats, audit) if team_color_required else None
    military_sigil = PLAYER_SIGIL if is_military_unit(category, stats) else None
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
            "team_color_slots": split_list(profile.get("team_color_slots", "")) if team_color_required else [],
            "recommended_player_colour": player_colour,
            "player_sigil": military_sigil,
            "operated_by_person": is_operated_unit(enum_name),
            "operator_contract": (
                "Show exactly one visible human operator actively handling, pushing, loading, firing, bracing, or inspecting this movable machine."
                if is_operated_unit(enum_name)
                else ""
            ),
            "ammunition": ammunition_refs_for_entity(enum_name),
            "research_visual_lines": research_visual_lines_for_entity(enum_name),
            "research_visual_variants": research_tier_variants_for_entity(enum_name),
            "visual_design": profile.get("visual_design", ""),
            "source_role": profile.get("role", ""),
        },
        "states": split_list(profile.get("required_states", "")),
        "actions": actions,
        "paths": {
            "runtime_root": f"assets/tiles/entities/{stats['slug']}",
            "manifest": f"assets/tiles/entities/{stats['slug']}/manifest.json",
        },
        "sources": [
            "include/realm.h",
            "src/core/entity_defs.cpp",
            "docs/tileset/realm_tileset_visual_audit.md",
        ],
    }


def ammunition_spec(spec: dict[str, Any]) -> dict[str, Any]:
    slug = spec["slug"]
    return {
        "schema": "realm.ammunition_sprite_spec.v1",
        "asset_type": "ammunition",
        "slug": slug,
        "name": spec["name"],
        "art": {
            "projection": "transparent upright_world projectile sprite or tiny animation",
            "visual_design": spec["description"],
            "team_color_required": False,
        },
        "states": [state["id"] for state in spec["states"]],
        "actions": [
            {
                "id": state["id"],
                "description": state["description"],
                "source": "scripts/export_tile_specs.py",
                "frames_recommended": 1,
            }
            for state in spec["states"]
        ],
        "paths": {
            "runtime_root": f"assets/tiles/ammunition/{slug}",
            "manifest": f"assets/tiles/ammunition/{slug}/manifest.json",
        },
        "sources": [
            "scripts/export_tile_specs.py",
            "scripts/export_image_generation_prompts.py",
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
            "src/core/terrain_defs.cpp",
            "src/render/sdl/display_glyphs.cpp",
            "docs/tileset/realm_tileset_visual_audit.md",
        ],
    }


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=False, ensure_ascii=True) + "\n", encoding="utf-8")


def export_specs(out_dir: Path, clean: bool) -> dict[str, Any]:
    realm_h = read_text(ROOT / "include" / "realm.h")
    entity_defs_cpp = read_text(ROOT / "src" / "core" / "entity_defs.cpp")
    terrain_defs_cpp = read_text(ROOT / "src" / "core" / "terrain_defs.cpp")
    sdl_display_glyphs_cpp = read_text(ROOT / "src" / "render" / "sdl" / "display_glyphs.cpp")
    audit_md = read_text(ROOT / "docs" / "tileset" / "realm_tileset_visual_audit.md")

    entity_order = enum_values(realm_h, "EntityType")
    terrain_order = enum_values(realm_h, "Terrain")
    stats = parse_stats(entity_defs_cpp)
    terrain_names = parse_terrain_names(terrain_defs_cpp)
    terrain_glyphs = parse_terrain_glyphs(sdl_display_glyphs_cpp)
    entity_audit, terrain_audit = parse_audit_tables(audit_md)

    if clean and out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    index: dict[str, Any] = {
        "schema": "realm.tile_specs_index.v1",
        "generated_by": "scripts/export_tile_specs.py",
        "groups": {
            "grounds": [], "features": [], "decals": [],
            "units": [], "animals": [], "buildings": [], "ammunition": [],
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

    for spec_record in AMMUNITION_SPECS:
        spec = ammunition_spec(spec_record)
        rel = Path("ammunition") / f"{spec_record['slug']}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["ammunition"].append(
            {"name": spec_record["name"], "slug": spec_record["slug"], "path": rel.as_posix()}
        )

    for enum_name in terrain_order:
        if enum_name.endswith("_COUNT"):
            continue
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
