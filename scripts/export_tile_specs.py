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
GAME_TYPES_HEADER = ROOT / "src" / "core" / "game_types.h"
ENTITY_RANGES = {
    "units": ("E_PEASANT", "E_RAM"),
    "buildings": ("E_TOWNHALL", "E_DOCK"),
    "animals": ("E_DEER", "E_BOAR"),
}
BRIDGE_BUILDING_ENUMS = {"E_WOODEN_BRIDGE", "E_STONE_BRIDGE"}
PLAYER_TEAM_COLOR_CATEGORIES = {"units", "buildings"}
PEASANT_SPEC = ROOT / "art" / "tiles" / "workbench" / "peasant" / "unit_spec.json"
SELF_TILE = "self_tile"
ADJACENT_TARGET = "adjacent_target_tile_or_entity"
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
PLAYER_SIGIL = {
    "id": "player-sigil",
    "description": "white diagonal stripe running from top left to bottom right",
}
KNIGHT_COMMON_TEAM_COLOR_SLOTS = [
    "shield face",
    "horse bridle/headstall/face straps",
    "reins",
]
KNIGHT_PLATE_HELM_TEAM_COLOR_SLOTS = [
    *KNIGHT_COMMON_TEAM_COLOR_SLOTS,
    "saddle/saddle cloth",
]
KNIGHT_IRON_WEAPONS_HEAVY_LANCE_DESCRIPTION = (
    "heavy mounted cavalry lance, longer than the default short spear, with wooden shaft, large bright iron lance head, "
    "visible iron socket/fittings, and metal butt cap"
)
KNIGHT_IRON_WEAPONS_SMALL_PENNANT_DESCRIPTION = (
    "small practical lance pennant in blue #00AFFF with a white diagonal stripe, pointing/trailing outward away from "
    "the rider rather than inward across the rider"
)
KNIGHT_TIER_RULES: dict[str, dict[str, Any]] = {
    "basic_weapons__open_helmet": {
        "team_color_slots": KNIGHT_COMMON_TEAM_COLOR_SLOTS,
        "description": (
            "default/basic Knight with no research active; open nasal helmet, visible face, simple mail coif, "
            "and light shoulder protection only; reads as the mounted villager upgraded with cheap early cavalry gear, "
            "not polished noble cavalry; no horse armour and no barding; brown leather saddle and dark brown leather "
            "girth/belly strap; horse bridle/headstall/face straps and reins use blue #00AFFF; short spear with "
            "plain wooden shaft, leather grip, and dull bronze or scrap-metal spearhead; no pennant; round wooden "
            "shield with blue #00AFFF face, white diagonal stripe, and optional simple central boss; shield is "
            "strapped to the rider's anatomical left forearm, relaxed on the far side and partly visible; rider's "
            "anatomical left hand still holds or gathers the reins"
        ),
        "team_color_rules": [
            "Use blue #00AFFF on the shield face, horse bridle/headstall/face straps, and reins.",
            "Put the white diagonal stripe on the shield face.",
            "No Plate Helm is active, so keep the saddle brown leather.",
            "Keep the girth/belly strap under the horse dark brown leather; never make it blue.",
            "No pennant, horse armour, barding, lance, kite shield, or polished noble-cavalry treatment.",
            "Keep horse body, mane, tail, hooves, saddle leather, girth strap, shadows, wood, skin, and plain metal out of team colour.",
        ],
    },
    "basic_weapons__plate_helm": {
        "team_color_slots": KNIGHT_PLATE_HELM_TEAM_COLOR_SLOTS,
        "description": (
            "Plate Helm active and Iron Weapons inactive; same Plate Helm armour package as iron_weapons__plate_helm: "
            "closed plate helm, blue plume/crest, and full plate-style rider armour silhouette; same horse armour/barding "
            "package as iron_weapons__plate_helm, with plated head armour/chanfron, neck plates/crinet, and chest/front "
            "armour/peytral; saddle/saddle cloth becomes blue #00AFFF; dark brown leather girth/belly strap under the "
            "horse remains non-team-colour and must never become blue; horse bridle/headstall/face straps and reins remain blue #00AFFF where visible; same short spear "
            "as the default/basic variant, with plain wooden shaft, leather grip, and dull bronze or scrap-metal spearhead; "
            "no pennant and do not upgrade to a lance; same round wooden shield as the default/basic variant, with blue "
            "#00AFFF face and white diagonal stripe; no reinforced iron rim, rivets, or kite-shield upgrade; shield is "
            "strapped to the rider's anatomical left forearm, relaxed on the far side and partly visible; rider's anatomical "
            "left hand still holds or gathers the reins"
        ),
        "team_color_rules": [
            "Use blue #00AFFF on the shield face, horse bridle/headstall/face straps, reins, and saddle/saddle cloth.",
            "Put the white diagonal stripe on the shield face.",
            "Plate Helm is active, so the saddle/saddle cloth becomes blue #00AFFF.",
            "Keep the girth/belly strap under the horse dark brown leather; never make it blue.",
            "Horse armour/barding is present because Plate Helm is active, but the girth strap, horse body, and plain metal are not team colour.",
            "Keep the basic short spear and round wooden shield; do not add a lance, pennant, kite shield, iron rim, or iron rivets in this tier.",
            "Keep horse body, mane, tail, hooves, girth strap, shadows, wood, skin, and plain metal out of team colour.",
        ],
    },
    "iron_weapons__open_helmet": {
        "team_color_slots": [
            *KNIGHT_COMMON_TEAM_COLOR_SLOTS,
            "small lance pennant",
        ],
        "description": (
            "Iron Weapons active and Plate Helm inactive; same open-helmet/light-armour package as basic_weapons__open_helmet: "
            "open nasal helmet, visible face, simple mail coif, and light shoulder protection; no full plate armour and no "
            "plume/crest; no horse armour and no barding; brown leather saddle and dark brown leather girth/belly strap; "
            f"horse bridle/headstall/face straps and reins use blue #00AFFF; {KNIGHT_IRON_WEAPONS_HEAVY_LANCE_DESCRIPTION}; "
            f"{KNIGHT_IRON_WEAPONS_SMALL_PENNANT_DESCRIPTION}; pentagonal/kite-style shield "
            "with blue #00AFFF face, white diagonal stripe, reinforced iron rim, and iron rivets/fasteners; shield is strapped "
            "to the rider's anatomical left forearm, relaxed on the far side and partly visible; rider's anatomical left hand "
            "still holds or gathers the reins"
        ),
        "team_color_rules": [
            "Use blue #00AFFF on the shield face, horse bridle/headstall/face straps, reins, and small lance pennant.",
            "Put the white diagonal stripe on the shield face and the small lance pennant.",
            "The small lance pennant points/trails outward away from the rider, not inward across the rider.",
            "No Plate Helm is active, so keep the saddle brown leather.",
            "Keep the girth/belly strap under the horse dark brown leather; never make it blue.",
            "Keep the open nasal helmet and visible face; do not add Plate Helm armour features in this tier.",
            "No horse armour or barding; Iron Weapons alone upgrades to the heavy mounted cavalry lance, upgraded shield, and small pennant.",
            "Keep horse body, mane, tail, hooves, saddle leather, girth strap, shadows, wood, skin, and plain metal out of team colour.",
        ],
    },
    "iron_weapons__plate_helm": {
        "team_color_slots": [
            *KNIGHT_PLATE_HELM_TEAM_COLOR_SLOTS,
            "small lance pennant",
        ],
        "description": (
            "both Plate Helm and Iron Weapons active; same Plate Helm armour package as basic_weapons__plate_helm: closed "
            "plate helm, blue plume/crest, and full plate-style rider armour silhouette; same horse armour/barding package "
            "as basic_weapons__plate_helm, with plated head armour/chanfron, neck plates/crinet, and chest/front armour/peytral; "
            "saddle/saddle cloth becomes blue #00AFFF; dark brown leather girth/belly strap under the horse remains "
            f"non-team-colour and must never become blue; horse bridle/headstall/face straps and reins remain blue #00AFFF where visible; {KNIGHT_IRON_WEAPONS_HEAVY_LANCE_DESCRIPTION}; "
            f"{KNIGHT_IRON_WEAPONS_SMALL_PENNANT_DESCRIPTION}; same pentagonal/kite-style "
            "Iron Weapons shield as iron_weapons__open_helmet, with blue #00AFFF face, white diagonal stripe, reinforced iron rim, "
            "and iron rivets/fasteners; shield is strapped to the rider's anatomical left forearm, relaxed on the far side and "
            "partly visible; rider's anatomical left hand still holds or gathers the reins"
        ),
        "team_color_rules": [
            "Use blue #00AFFF on the shield face, horse bridle/headstall/face straps, reins, saddle/saddle cloth, and small lance pennant.",
            "Put the white diagonal stripe on the shield face and the small lance pennant.",
            "The small lance pennant points/trails outward away from the rider, not inward across the rider.",
            "Plate Helm is active, so the saddle/saddle cloth becomes blue #00AFFF.",
            "Keep the girth/belly strap under the horse dark brown leather; never make it blue.",
            "Horse armour/barding is present because Plate Helm is active, and should match basic_weapons__plate_helm except for weapon/shield differences.",
            "Keep the lance pennant small and practical, not a large banner or ceremonial flag.",
            "Keep horse body, mane, tail, hooves, girth strap, shadows, wood, skin, and plain metal out of team colour.",
        ],
    },
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
                "description": "early/basic cavalry armour with an open nasal helmet, visible face, simple mail coif, and lighter shoulder protection",
            },
            {
                "id": "plate_helm",
                "name": "Plate Helm",
                "research": "Plate Helm",
                "description": "upgraded cavalry armour with a closed plate helm, stronger cheek protection, brighter solid helmet brow, and heavier neck protection",
            },
        ],
    },
}
PLAYER_COLOURS = {
    "blue": {"name": "blue", "hex": "#00AFFF"},
    "red": {"name": "red", "hex": "#FF0000"},
    "green": {"name": "green", "hex": "#00B050"},
}
GENERATION_ASPECTS = {
    "square_1_1": {
        "preset": "Square",
        "ratio": "1:1",
        "description": "default square image-generation aspect ratio",
    },
    "portrait_4_3": {
        "preset": "Portrait",
        "ratio": "4:3",
        "description": "taller cavalry sheet with extra vertical room for horse and rider silhouette",
    },
    "story_16_9": {
        "preset": "Story",
        "ratio": "16:9",
        "description": "wide sheet with extra horizontal room for long spears, pikes, and lances",
    },
}
DEFAULT_GENERATION_ASPECT_ID = "square_1_1"
ENTITY_GENERATION_ASPECT_OVERRIDES = {
    "E_KNIGHT": "portrait_4_3",
    "E_SPEARMAN": "story_16_9",
}
KNIGHT_LANCE_VARIANT_ASPECTS = {
    "iron_weapons__open_helmet": "story_16_9",
    "iron_weapons__plate_helm": "story_16_9",
}
BLUE_CONTEXT_KEYWORDS = {
    "blue", "water", "naval", "ship", "warship", "boat", "dock", "fishing", "fish",
    "shoal", "wave", "sail", "galley", "skiff", "barge", "ferry",
}
RED_CONTEXT_KEYWORDS = {"red", "flame", "flaming", "burning", "lava", "ember"}
RED_CONTEXT_PHRASES = {"fire ship", "fireship"}
OPERATED_UNIT_ENUMS = {"E_CATAPULT", "E_TREBUCHET", "E_RAM"}
PROJECTILES_BY_ENTITY = {
    "E_ARCHER": ["arrow", "crossbow_bolt"],
    "E_CATAPULT": ["catapult_boulder"],
    "E_TREBUCHET": ["trebuchet_boulder"],
    "E_WARSHIP": ["warship_arrow_volley"],
    "E_TOWER": ["tower_bolt"],
    "E_CASTLE": ["tower_bolt", "trebuchet_boulder"],
}
AMMUNITION_BY_ENTITY = PROJECTILES_BY_ENTITY
PROJECTILE_SPECS = [
    {
        "slug": "arrow",
        "name": "Arrow",
        "description": "standard blue-feathered arrow ammunition after release",
        "states": [
            {"id": "in_flight", "description": "single blue-feathered arrow in flight, readable diagonal silhouette, no bow or archer"},
        ],
    },
    {
        "slug": "crossbow_bolt",
        "name": "Crossbow Bolt",
        "description": "compact blue-feathered crossbow bolt ammunition after release",
        "states": [
            {"id": "in_flight", "description": "short blue-feathered bolt in flight, strong head and shaft silhouette, no crossbow"},
        ],
    },
    {
        "slug": "flaming_arrow",
        "name": "Flaming Arrow",
        "description": "future blue-feathered flaming arrow ammunition with a tiny flame loop",
        "states": [
            {"id": "flame_frame_1", "description": "blue-feathered arrow in flight with small flame flicker frame 1"},
            {"id": "flame_frame_2", "description": "blue-feathered arrow in flight with shifted small flame flicker frame 2"},
        ],
    },
    {
        "slug": "tower_bolt",
        "name": "Tower Bolt",
        "description": "heavy defensive bolt with a narrow blue painted tail stripe, fired by towers or garrisons",
        "states": [
            {"id": "in_flight", "description": "heavy bolt in flight with a narrow blue painted tail stripe, readable at small RTS scale, no tower"},
        ],
    },
    {
        "slug": "warship_arrow_volley",
        "name": "Warship Arrow Volley",
        "description": "small grouped naval volley of blue-feathered arrows after release",
        "states": [
            {"id": "volley_frame_1", "description": "compact volley of blue-feathered arrows in flight frame 1, no ship or water wake"},
            {"id": "volley_frame_2", "description": "compact volley of blue-feathered arrows in flight frame 2 with shifted arrows, no ship or water wake"},
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
AMMUNITION_SPECS = PROJECTILE_SPECS
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
GROUND_LEGACY_TERRAINS = {
    "G_GRASS": ["T_GRASS", "T_BERRY"],
    "G_MEADOW": ["T_MEADOW", "T_WHEAT"],
    "G_DIRT": ["T_DIRT"],
    "G_MUD": ["T_MUD"],
    "G_SAND": ["T_SAND", "T_PALM"],
    "G_DUNES": ["T_DUNES"],
    "G_SNOW": [],
    "G_TUNDRA": ["T_SNOW", "T_PINE"],
    "G_ICE": ["T_ICE"],
    "G_WATER": ["T_WATER", "T_FISH"],
    "G_SHALLOWS": ["T_SHALLOWS"],
    "G_MARSH": ["T_MARSH", "T_REEDS"],
    "G_GRAVEL": ["T_GRAVEL", "T_RUINS"],
    "G_ASH": ["T_ASH", "T_DEAD_TREE"],
    "G_LAVA": ["T_LAVA"],
    "G_HILLS": ["T_HILLS"],
    "G_ROCKY": ["T_MOUNTAIN", "T_STONE", "T_GOLD"],
    "G_CASTLE_FLOOR": ["T_CASTLE_FLOOR", "T_CASTLE_WALL", "T_CASTLE_GATE"],
    "G_ROAD": ["T_ROAD"],
}
FEATURE_LEGACY_TERRAINS = {
    "F_FOREST": ["T_FOREST"],
    "F_PINE": ["T_PINE"],
    "F_PALM": ["T_PALM"],
    "F_DEAD_TREE": ["T_DEAD_TREE"],
    "F_BERRY_BUSH": ["T_BERRY"],
    "F_WHEAT_CROP": ["T_WHEAT"],
    "F_FISH_SHOAL": ["T_FISH"],
    "F_GOLD_DEPOSIT": ["T_GOLD"],
    "F_STONE_BOULDERS": ["T_STONE"],
    "F_MOUNTAIN_PEAK": ["T_MOUNTAIN"],
    "F_REEDS": ["T_REEDS"],
    "F_RUINS": ["T_RUINS"],
    "F_CASTLE_WALL": ["T_CASTLE_WALL"],
    "F_CASTLE_GATE": ["T_CASTLE_GATE"],
}
DECAL_LEGACY_TERRAINS = {
    "VD_ROAD": ["T_ROAD"],
    "VD_FLOWERS": ["T_FLOWERS"],
    "VD_TALL_GRASS": ["T_TALL_GRASS"],
}
DECAL_RUNTIME_CONTEXT = {
    "VD_ROAD": "legacy_terrain_bridge",
    "VD_FLOWERS": "legacy_terrain_bridge",
    "VD_TALL_GRASS": "legacy_terrain_bridge",
    "VD_SCUFFS": "wear_threshold_25",
    "VD_PACKED_PATH": "wear_threshold_55",
    "VD_COBBLE_PATCH": "wear_threshold_80",
    "VD_WHEEL_RUTS": "wear_threshold_45_default",
    "VD_MUDDY_FOOTPRINTS": "wear_threshold_45_on_mud",
    "VD_SNOW_TRAMPLED_PATH": "wear_threshold_45_on_snow",
    "VD_YARD_CLUTTER": "future_building_context",
    "VD_CRATES_BARRELS": "future_building_context",
    "VD_LOG_PILES": "future_building_context",
    "VD_FARM_TRACKS": "future_building_context",
}
HARVESTABLE_FEATURE_ENUMS = {
    "F_FOREST", "F_PINE", "F_PALM", "F_DEAD_TREE",
    "F_BERRY_BUSH", "F_WHEAT_CROP", "F_FISH_SHOAL", "F_GOLD_DEPOSIT",
}
SPLIT_FEATURE_ENUMS = {"F_FOREST", "F_PINE", "F_REEDS"}
PROJECTILE_RUNTIME_ASSETS = {
    "arrow": "arrow_projectile",
    "crossbow_bolt": "arrow_projectile",
    "flaming_arrow": "arrow_projectile",
    "tower_bolt": "tower_bolt_projectile",
    "warship_arrow_volley": "warship_shot_projectile",
    "catapult_boulder": "catapult_boulder_projectile",
    "trebuchet_boulder": "catapult_boulder_projectile",
}
EFFECT_ASSET_NAMES = [
    "melee_hit_spark", "arrow_hit", "boulder_impact", "boulder_water_splash",
    "building_hit_dust", "rain_frame_1", "rain_frame_2", "storm_rain_frame_1",
    "storm_rain_frame_2", "snowfall_frame_1", "snowfall_frame_2",
]
USER_INTERFACE_ASSET_NAMES = [
    "move_marker", "attack_marker", "gather_marker", "build_marker", "rally_marker",
    "attack_move_marker", "hold_position_marker", "selection_ring", "group_selection_ring",
    "range_ring_dot", "build_preview_valid", "build_preview_invalid", "wall_preview",
    "garrison_indicator", "queued_unit_marker", "research_active_marker",
    "completed_research_icon_treatment",
]


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
    return PROJECTILES_BY_ENTITY.get(enum_name, [])


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


def team_color_slots_for_entity(enum_name: str, profile: dict[str, str], team_color_required: bool) -> list[str]:
    if not team_color_required:
        return []
    if enum_name == "E_KNIGHT":
        return [
            *KNIGHT_COMMON_TEAM_COLOR_SLOTS,
            "saddle/saddle cloth only when Plate Helm is active",
        ]
    return split_list(profile.get("team_color_slots", ""))


def team_color_variant_rules_for_entity(enum_name: str) -> list[dict[str, Any]]:
    if enum_name != "E_KNIGHT":
        return []
    return [
        {
            "id": variant_id,
            "name": variant_id.replace("__", " + ").replace("_", " ").title(),
            "team_color_slots": list(rule["team_color_slots"]),
            "rules": list(rule["team_color_rules"]),
        }
        for variant_id, rule in KNIGHT_TIER_RULES.items()
    ]


def generation_aspect(aspect_id: str = DEFAULT_GENERATION_ASPECT_ID) -> dict[str, str]:
    aspect = GENERATION_ASPECTS.get(aspect_id, GENERATION_ASPECTS[DEFAULT_GENERATION_ASPECT_ID])
    return {
        "id": aspect_id if aspect_id in GENERATION_ASPECTS else DEFAULT_GENERATION_ASPECT_ID,
        "preset": aspect["preset"],
        "ratio": aspect["ratio"],
        "description": aspect["description"],
    }


def generation_aspect_for_group(group: str) -> dict[str, str]:
    return generation_aspect(DEFAULT_GENERATION_ASPECT_ID)


def generation_aspect_for_entity(enum_name: str, category: str) -> dict[str, str]:
    if category != "units":
        return generation_aspect_for_group(category)
    return generation_aspect(ENTITY_GENERATION_ASPECT_OVERRIDES.get(enum_name, DEFAULT_GENERATION_ASPECT_ID))


def generation_aspect_for_research_variant(enum_name: str, variant_id: str) -> dict[str, str] | None:
    if enum_name == "E_KNIGHT" and variant_id in KNIGHT_LANCE_VARIANT_ASPECTS:
        return generation_aspect(KNIGHT_LANCE_VARIANT_ASPECTS[variant_id])
    return None


def generation_aspect_variant_rules_for_entity(enum_name: str, category: str) -> list[dict[str, str]]:
    base = generation_aspect_for_entity(enum_name, category)
    rules: list[dict[str, str]] = []
    if enum_name == "E_KNIGHT":
        for variant_id, aspect_id in KNIGHT_LANCE_VARIANT_ASPECTS.items():
            aspect = generation_aspect(aspect_id)
            if aspect["id"] == base["id"]:
                continue
            rules.append(
                {
                    "variant": variant_id,
                    "preset": aspect["preset"],
                    "ratio": aspect["ratio"],
                    "description": "use for Iron Weapons Knight lance states so the lance has horizontal room",
                }
            )
    return rules


def source_canvas_for_entity(enum_name: str, category: str) -> dict[str, Any]:
    if category in {"units", "animals"}:
        return {
            "width_px": 48,
            "height_px": 48,
            "unit": "px",
            "scope": "per accepted standalone actor sprite frame",
            "source": "docs/tileset/realm_tileset_visual_audit.md core format table",
        }
    if category == "buildings":
        return {
            "width_px": 32,
            "height_px": 32,
            "unit": "px",
            "scope": "per occupied building footprint cell",
            "source": "docs/tileset/realm_tileset_visual_audit.md core format table",
        }
    return {}


def source_canvas_for_group(group: str) -> dict[str, Any]:
    if group == "grounds":
        return {
            "width_px": 1024,
            "height_px": 1024,
            "unit": "px",
            "scope": "per standalone high-resolution ground source tile",
            "source": "realm-tileset-from-images ground contract",
        }
    if group in {"features", "decals"}:
        return {
            "width_px": 48,
            "height_px": 48,
            "unit": "px",
            "scope": "per accepted standalone tile-anchored source image",
            "source": "docs/tileset/realm_tileset_visual_audit.md core format table",
        }
    if group in {"projectiles", "effects", "user_interface"}:
        return {
            "width_px": 32,
            "height_px": 32,
            "unit": "px",
            "scope": "per accepted standalone overlay, projectile, or UI source image",
            "source": "docs/tileset/realm_tileset_visual_audit.md core format table",
        }
    return {}


def research_tier_variants_for_entity(enum_name: str) -> list[dict[str, Any]]:
    lines = research_visual_lines_for_entity(enum_name)
    if not lines:
        return []
    variants: list[dict[str, Any]] = []
    for combo in itertools.product(*[line["tiers"] for line in lines]):
        variant_id = "__".join(tier["id"] for tier in combo)
        description = "; ".join(tier["description"] for tier in combo)
        if enum_name == "E_KNIGHT" and variant_id in KNIGHT_TIER_RULES:
            description = KNIGHT_TIER_RULES[variant_id]["description"]
        variant_aspect = generation_aspect_for_research_variant(enum_name, variant_id)
        researched = [tier["research"] for tier in combo if tier.get("research")]
        variants.append(
            {
                "id": variant_id,
                "name": " + ".join(tier["name"] for tier in combo),
                "description": description,
                "research": researched,
                "is_default": not researched,
                **({"generation_aspect": variant_aspect} if variant_aspect else {}),
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
            if variant.get("generation_aspect"):
                item["generation_aspect"] = variant["generation_aspect"]
            expanded.append(item)
    return expanded


def guided_entity_action_description(enum_name: str, action_id: str, description: str) -> str:
    action = action_id.lower()
    if enum_name != "E_KNIGHT":
        return description
    shield_reins = (
        "shield is strapped to the rider's anatomical left forearm, relaxed on the far side beside or behind "
        "the horse neck/shoulder area, about one third to one half visible; the rider's anatomical left hand "
        "still holds or gathers the reins, with reins passing naturally near the shield-side hand"
    )
    if action == "idle":
        return (
            "idle mounted stance: rider's anatomical right hand carries the spear/lance upright or near-upright; "
            f"{shield_reins}; shield is not front-presented; reins are relaxed rather than tense"
        )
    if action == "trot":
        return (
            "controlled trot: rider's anatomical right hand carries the spear/lance while the horse moves; "
            f"{shield_reins}; horse can be guided by legs, knees, seat, spurs, training, and relaxed reins"
        )
    if "charge" in action or "strike" in action:
        return (
            "charge/strike action with the spear/lance allowed to angle for the attack; "
            f"{shield_reins}; keep the shield strapped, not gripped as a whole-hand object"
        )
    if "hit" in action or "alert" in action:
        return (
            "hit/alert mounted pose with the spear/lance still in the rider's anatomical right hand; "
            f"{shield_reins}; shield arm remains relaxed rather than actively presenting the shield"
        )
    return description


def apply_entity_action_guidance(enum_name: str, actions: list[dict[str, Any]]) -> list[dict[str, Any]]:
    guided: list[dict[str, Any]] = []
    for action in actions:
        item = dict(action)
        item["description"] = guided_entity_action_description(
            enum_name,
            str(item.get("id", "")),
            str(item.get("description", item.get("id", ""))),
        )
        guided.append(item)
    return guided


def recommended_player_colour(enum_name: str, stats: dict[str, Any], audit: dict[str, str]) -> dict[str, str]:
    if enum_name in {"E_WOODEN_BRIDGE", "E_STONE_BRIDGE"}:
        return PLAYER_COLOURS["red"]
    if enum_name == "E_KNIGHT":
        return PLAYER_COLOURS["blue"]
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

    game_types_h = read_text(GAME_TYPES_HEADER)
    entity_names = enum_values(game_types_h, "EntityType")
    out: dict[str, dict[str, Any]] = {}
    for enum_name, fields in zip(entity_names, records):
        if len(fields) < 16:
            raise RuntimeError(f"bad STATS row for {enum_name}: {fields}")
        if len(fields) >= 17:
            cost_gold_i = 7
            cost_wood_i = 8
            cost_food_i = 9
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


def parse_named_cases(source: str, function_name: str) -> dict[str, str]:
    match = re.search(rf"const\s+char\*\s+{function_name}\([^)]*\)\s*\{{(?P<body>.*?)^\}}", source, re.S | re.M)
    if not match:
        raise RuntimeError(f"could not find {function_name}")
    return {
        enum_name: value
        for enum_name, value in re.findall(r"case\s+(\w+):\s*return\s+\"([^\"]+)\";", match.group("body"))
    }


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
    if enum_name in BRIDGE_BUILDING_ENUMS:
        return "buildings"
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
        return [dict(action) for action in PEASANT_ACTIONS]
    spec = json.loads(PEASANT_SPEC.read_text(encoding="utf-8"))
    return spec.get("actions", []) or [dict(action) for action in PEASANT_ACTIONS]


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
    actions = apply_entity_action_guidance(enum_name, actions)
    actions = apply_research_variants_to_actions(enum_name, actions)
    team_color_required = category in PLAYER_TEAM_COLOR_CATEGORIES
    team_color_slots = team_color_slots_for_entity(enum_name, profile, team_color_required)
    player_colour = recommended_player_colour(enum_name, stats, audit) if team_color_required else None
    military_sigil = PLAYER_SIGIL if is_military_unit(category, stats) else None
    generation_aspect_spec = generation_aspect_for_entity(enum_name, category)
    render = {
        "layer": "building" if category == "buildings" else "actor",
        "projection_mode": "upright_world",
        "projection_factor": 0.0,
        "depth_bucket": "building" if category == "buildings" else "actor",
        "anchor": "footprint_origin" if category == "buildings" else "tile_center",
        "directions": ["front", "back"] if category != "buildings" else ["south"],
        "runtime_mirrors_horizontal": category != "buildings",
    }
    placement = {
        "footprint": stats["footprint"],
    }
    if category == "buildings":
        placement["origin"] = "south_west"
    states: list[str]
    if category == "buildings":
        if enum_name == "E_WOODEN_BRIDGE":
            states = [
                "span_single_east_west", "span_single_north_south",
                "construction_0_foundation", "construction_1_frame", "construction_2_nearly_complete",
                "damaged", "broken",
            ]
        elif enum_name == "E_STONE_BRIDGE":
            states = [
                "span_single_east_west", "span_single_north_south",
                "span_half_east_west", "span_half_north_south",
                "span_joined_east_west", "span_joined_north_south",
                "construction_0_foundation", "construction_1_frame", "construction_2_nearly_complete",
                "damaged", "broken",
            ]
        else:
            states = [
                "construction_0_foundation", "construction_1_frame", "construction_2_nearly_complete",
                "complete", "damaged", "garrisoned", "garrison_firing", "training_peasant",
                "training_infantry", "training_cavalry", "training_ship", "researching_iron_weapons",
                "researching_crossbows", "researching_pikes", "researching_counterweight",
                "researching_plate_helm",
            ]
    elif category == "animals":
        states = ["alive", "dead_unharvested", "partly_harvested", "mostly_harvested", "depleted_skeleton"]
    elif enum_name == "E_TRANSPORT":
        states = ["empty", "loaded_partial", "loaded_full", "load_unload", "wreck", "decayed_wreck"]
    else:
        states = split_list(profile.get("required_states", ""))
    combat_projectiles = ammunition_refs_for_entity(enum_name)
    return {
        "schema": "realm.building_spec.v2" if category == "buildings" else "realm.actor_sprite_spec.v2",
        "asset_type": category[:-1] if category.endswith("s") else category,
        "id": stats["slug"],
        "enum": enum_name,
        "slug": stats["slug"],
        "name": stats["name"],
        "render": render,
        "placement": placement,
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
            "legacy_footprint": stats["footprint"],
        },
        "entity": {
            "kind": "building" if category == "buildings" else "actor",
            "actor_type": None if category == "buildings" else category[:-1],
            "rests_on_tile_center": category != "buildings",
            "can_interpolate_between_tiles": category != "buildings",
        },
        "team_color": {
            "required": team_color_required,
            "slots": team_color_slots,
            "variant_rules": team_color_variant_rules_for_entity(enum_name),
            "recommended_player_colour": player_colour,
            "player_sigil": military_sigil,
        },
        "combat": {
            "projectiles": combat_projectiles,
        },
        "visual_variants": {
            "research_lines": research_visual_lines_for_entity(enum_name),
            "resolved_variants": research_tier_variants_for_entity(enum_name),
        },
        "operator": {
            "visible_operator_required": is_operated_unit(enum_name),
            "count": 1 if is_operated_unit(enum_name) else 0,
            "contract": (
                "Show exactly one visible human operator actively handling, pushing, loading, firing, bracing, or inspecting this movable machine."
                if is_operated_unit(enum_name)
                else ""
            ),
        },
        "art": {
            "legacy_projection": "upright sprite anchored over projected isometric map tiles",
            "visual_design": profile.get("visual_design", ""),
            "source_role": profile.get("role", ""),
            "generation_aspect": generation_aspect_spec,
            "generation_aspect_variant_rules": generation_aspect_variant_rules_for_entity(enum_name, category),
            "source_canvas": source_canvas_for_entity(enum_name, category),
        },
        "states": states,
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
    runtime_asset = PROJECTILE_RUNTIME_ASSETS.get(slug, slug)
    return {
        "schema": "realm.projectile_sprite_spec.v2",
        "asset_type": "projectile",
        "id": slug,
        "slug": slug,
        "name": spec["name"],
        "render": {
            "layer": "projectile",
            "projection_mode": "upright_world",
            "projection_factor": 0.0,
            "depth_bucket": "projectile",
            "anchor": "world_position",
        },
        "runtime": {
            "legacy_asset_type": "ammunition",
            "legacy_runtime_asset": runtime_asset,
            "glyph_fallback": "-" if "boulder" not in slug else "o",
        },
        "projectile": {
            "is_tile_content": False,
            "impact_effect": "boulder_impact" if "boulder" in slug else "arrow_hit",
        },
        "art": {
            "legacy_projection": "transparent upright_world projectile sprite or tiny animation",
            "visual_design": spec["description"],
            "generation_aspect": generation_aspect_for_group("projectiles"),
            "source_canvas": source_canvas_for_group("projectiles"),
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
            "runtime_root": f"assets/tiles/effects-ui/{runtime_asset}.png",
            "manifest": f"assets/tiles/projectiles/{slug}/manifest.json",
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


def title_from_slug(slug: str) -> str:
    return slug.replace("_", " ").title()


def ground_gameplay(enum_name: str) -> dict[str, Any]:
    base: dict[str, Any] = {
        "passability": {"land": "passable", "boat": "blocked"},
        "buildable": True,
        "needs_design_review": False,
    }
    if enum_name == "G_WATER":
        base["passability"] = {"land": "blocked", "boat": "passable"}
        base["buildable"] = False
    elif enum_name == "G_SHALLOWS":
        base["passability"] = {"land": "passable", "boat": "passable"}
        base["buildable"] = False
        base["movement_ticks_modifier"] = 1
    elif enum_name in {"G_MARSH", "G_SAND", "G_DUNES", "G_SNOW", "G_TUNDRA", "G_ICE", "G_ASH"}:
        base["buildable"] = enum_name not in {"G_MARSH", "G_ICE"}
        base["movement_ticks_modifier"] = 1
    elif enum_name == "G_MUD":
        base["buildable"] = False
        base["movement_ticks_modifier"] = 2
    elif enum_name == "G_LAVA":
        base["passability"] = {"land": "blocked", "boat": "blocked"}
        base["buildable"] = False
    elif enum_name in {"G_HILLS", "G_ROCKY"}:
        base["buildable"] = False
        base["needs_design_review"] = True
    elif enum_name in {"G_DIRT", "G_CASTLE_FLOOR"}:
        base["movement_ticks_modifier"] = -1
    return base


def feature_states(enum_name: str) -> list[str]:
    if enum_name in HARVESTABLE_FEATURE_ENUMS:
        return ["full", "mostly_full", "mostly_empty", "depleted"]
    if enum_name == "F_CASTLE_GATE":
        return ["default", "open", "closed", "locked", "damaged", "broken"]
    if enum_name in {"F_CASTLE_WALL", "F_RUINS"}:
        return ["default", "damaged", "broken"]
    return ["default"]


def ground_spec_v2(enum_name: str, slug: str) -> dict[str, Any]:
    return {
        "schema": "realm.ground_spec.v2",
        "asset_type": "ground",
        "id": slug,
        "slug": slug,
        "name": title_from_slug(slug),
        "enum": enum_name,
        "render": {
            "layer": "ground",
            "projection_mode": "surface_projected",
            "projection_factor": 1.0,
            "depth_bucket": "surface",
            "anchor": "tile",
        },
        "runtime": {
            "legacy_terrain_enums": GROUND_LEGACY_TERRAINS.get(enum_name, []),
        },
        "gameplay": ground_gameplay(enum_name),
        "art": {
            "generation_aspect": generation_aspect_for_group("grounds"),
            "source_canvas": source_canvas_for_group("grounds"),
        },
        "paths": {
            "runtime_root": f"assets/tiles/grounds/{slug}",
            "base": f"assets/tiles/grounds/{slug}.png",
        },
        "sources": [
            "src/core/game_types.h",
            "src/core/terrain_defs.cpp",
            "src/render/sdl/display_glyphs.cpp",
        ],
    }


def feature_spec_v2(enum_name: str, slug: str) -> dict[str, Any]:
    return {
        "schema": "realm.feature_spec.v2",
        "asset_type": "feature",
        "id": slug,
        "slug": slug,
        "name": title_from_slug(slug),
        "enum": enum_name,
        "render": {
            "layer": "feature",
            "projection_mode": "upright_world",
            "projection_factor": 0.0,
            "depth_bucket": "standing_front",
            "anchor": "tile_center",
            "feature_layers": {
                "split_ready": enum_name in SPLIT_FEATURE_ENUMS,
            },
        },
        "runtime": {
            "legacy_terrain_enums": FEATURE_LEGACY_TERRAINS.get(enum_name, []),
        },
        "states": feature_states(enum_name),
        "placement": {
            "footprint": {"w": 1, "h": 1},
        },
        "art": {
            "generation_aspect": generation_aspect_for_group("features"),
            "source_canvas": source_canvas_for_group("features"),
        },
        "paths": {
            "runtime_root": f"assets/tiles/features/{slug}",
            "manifest": f"assets/tiles/features/{slug}/manifest.json",
        },
        "sources": [
            "src/core/game_types.h",
            "src/core/terrain_defs.cpp",
        ],
    }


def decal_projection_mode(enum_name: str) -> str:
    if enum_name in {"VD_FLOWERS", "VD_TALL_GRASS"}:
        return "semi_upright_decal"
    return "surface_decal"


def decal_spec_v2(enum_name: str, slug: str) -> dict[str, Any]:
    return {
        "schema": "realm.decal_spec.v2",
        "asset_type": "decal",
        "id": slug,
        "slug": slug,
        "name": title_from_slug(slug),
        "enum": enum_name,
        "render": {
            "layer": "decal",
            "projection_mode": decal_projection_mode(enum_name),
            "projection_factor": 0.35 if enum_name in {"VD_FLOWERS", "VD_TALL_GRASS"} else 1.0,
            "depth_bucket": "surface_overlay",
            "anchor": "tile",
        },
        "runtime": {
            "legacy_terrain_enums": DECAL_LEGACY_TERRAINS.get(enum_name, []),
            "emission_context": DECAL_RUNTIME_CONTEXT.get(enum_name, "runtime"),
        },
        "art": {
            "generation_aspect": generation_aspect_for_group("decals"),
            "source_canvas": source_canvas_for_group("decals"),
        },
        "paths": {
            "runtime_root": f"assets/tiles/decals/{slug}",
            "base": f"assets/tiles/decals/{slug}.png",
        },
        "sources": [
            "src/core/game_types.h",
            "src/core/terrain_defs.cpp",
            "scripts/export_image_generation_prompts.py",
        ],
    }


def effect_spec_v2(slug: str) -> dict[str, Any]:
    return {
        "schema": "realm.effect_spec.v1",
        "asset_type": "effect",
        "id": slug,
        "slug": slug,
        "name": title_from_slug(slug),
        "render": {
            "layer": "effect",
            "projection_mode": "upright_world",
            "projection_factor": 0.0,
            "depth_bucket": "effect",
            "anchor": "world_position",
        },
        "art": {
            "generation_aspect": generation_aspect_for_group("effects"),
            "source_canvas": source_canvas_for_group("effects"),
        },
        "paths": {
            "runtime_root": f"assets/tiles/effects-ui/{slug}.png",
        },
        "sources": [
            "src/sim/save_load.cpp",
        ],
    }


def ui_asset_spec_v2(slug: str) -> dict[str, Any]:
    return {
        "schema": "realm.ui_asset_spec.v1",
        "asset_type": "user_interface",
        "id": slug,
        "slug": slug,
        "name": title_from_slug(slug),
        "render": {
            "layer": "ui",
            "projection_mode": "screen_space",
            "projection_factor": 0.0,
            "depth_bucket": "ui",
            "anchor": "screen_position",
        },
        "art": {
            "generation_aspect": generation_aspect_for_group("user_interface"),
            "source_canvas": source_canvas_for_group("user_interface"),
        },
        "paths": {
            "runtime_root": f"assets/tiles/effects-ui/{slug}.png",
        },
        "sources": [
            "src/sim/save_load.cpp",
        ],
    }


def write_json(path: Path, data: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, sort_keys=False, ensure_ascii=True) + "\n", encoding="utf-8")


def export_specs(out_dir: Path, clean: bool) -> dict[str, Any]:
    game_types_h = read_text(GAME_TYPES_HEADER)
    entity_defs_cpp = read_text(ROOT / "src" / "core" / "entity_defs.cpp")
    terrain_defs_cpp = read_text(ROOT / "src" / "core" / "terrain_defs.cpp")
    audit_md = read_text(ROOT / "docs" / "tileset" / "realm_tileset_visual_audit.md")

    entity_order = enum_values(game_types_h, "EntityType")
    ground_order = enum_values(game_types_h, "GroundType")
    feature_order = enum_values(game_types_h, "FeatureType")
    decal_order = enum_values(game_types_h, "VisualDecalType")
    stats = parse_stats(entity_defs_cpp)
    entity_audit, _terrain_audit = parse_audit_tables(audit_md)
    ground_names = parse_named_cases(terrain_defs_cpp, "groundTypeName")
    feature_names = parse_named_cases(terrain_defs_cpp, "featureTypeName")
    decal_names = parse_named_cases(terrain_defs_cpp, "visualDecalName")

    if clean and out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    index: dict[str, Any] = {
        "schema": "realm.tile_specs_index.v2",
        "generated_by": "scripts/export_tile_specs.py",
        "schema_version": 2,
        "compatibility": {
            "reads_v1": True,
            "writes_v2": True,
            "legacy_groups": {"ammunition": "projectiles"},
        },
        "groups": {
            "grounds": [], "features": [], "decals": [],
            "units": [], "animals": [], "buildings": [],
            "projectiles": [], "effects": [], "user_interface": [],
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

    for spec_record in PROJECTILE_SPECS:
        spec = ammunition_spec(spec_record)
        rel = Path("projectiles") / f"{spec_record['slug']}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["projectiles"].append(
            {"name": spec_record["name"], "slug": spec_record["slug"], "path": rel.as_posix()}
        )

    for enum_name in ground_order:
        if enum_name == "G_ROAD":
            continue
        slug = ground_names.get(enum_name, lower_slug(enum_name.removeprefix("G_")))
        spec = ground_spec_v2(enum_name, slug)
        rel = Path("grounds") / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["grounds"].append(
            {"enum": enum_name, "name": spec["name"], "slug": slug, "path": rel.as_posix()}
        )

    for enum_name in feature_order:
        if enum_name == "F_NONE":
            continue
        slug = feature_names.get(enum_name, lower_slug(enum_name.removeprefix("F_")))
        spec = feature_spec_v2(enum_name, slug)
        rel = Path("features") / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["features"].append(
            {"enum": enum_name, "name": spec["name"], "slug": slug, "path": rel.as_posix()}
        )

    for enum_name in decal_order:
        slug = decal_names.get(enum_name, lower_slug(enum_name.removeprefix("VD_")))
        spec = decal_spec_v2(enum_name, slug)
        rel = Path("decals") / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["decals"].append(
            {"enum": enum_name, "name": spec["name"], "slug": slug, "path": rel.as_posix()}
        )

    for slug in EFFECT_ASSET_NAMES:
        spec = effect_spec_v2(slug)
        rel = Path("effects") / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["effects"].append(
            {"name": spec["name"], "slug": slug, "path": rel.as_posix()}
        )

    for slug in USER_INTERFACE_ASSET_NAMES:
        spec = ui_asset_spec_v2(slug)
        rel = Path("user_interface") / f"{slug}.json"
        write_json(out_dir / rel, spec)
        index["groups"]["user_interface"].append(
            {"name": spec["name"], "slug": slug, "path": rel.as_posix()}
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
