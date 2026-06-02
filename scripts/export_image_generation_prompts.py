#!/usr/bin/env python3
"""Export human-readable Realm image-generation prompts as Markdown."""

from __future__ import annotations

import argparse
import itertools
import math
import re
import shutil
from pathlib import Path
from typing import Any

from export_tile_specs import (
    AMMUNITION_SPECS,
    ENTITY_RANGES,
    PLAYER_SIGIL,
    ROOT,
    ammunition_refs_for_entity,
    category_for_entity,
    clean_cell,
    entity_profile,
    enum_values,
    is_military_unit,
    is_operated_unit,
    lower_slug,
    parse_audit_tables,
    parse_stats,
    recommended_player_colour,
    peasant_actions,
    read_text,
    research_visual_lines_for_entity,
    split_list,
)


OUT_DEFAULT = "art/tiles/image-spec"
MAX_SLOTS_PER_SHEET = 16
SEASONS = [
    ("spring", "spring: fresh, recovering, greener look"),
    ("summer", "summer: full growth or dry high-sun look"),
    ("autumn", "autumn: muted, yellowing, leaf-littered, or spent look"),
    ("winter", "winter: frosted, snow-dusted, frozen, or dormant look"),
]
DEPLETION_LEVELS = [
    ("full", "full resource amount, abundant and untouched"),
    ("mostly_full", "mostly full resource amount, slightly reduced"),
    ("mostly_empty", "mostly empty resource amount, sparse but still readable"),
    ("depleted", "fully depleted, no usable resource remaining"),
]
FEATURE_TERRAINS = {
    "T_FOREST", "T_PINE", "T_PALM", "T_DEAD_TREE",
    "T_MOUNTAIN", "T_STONE", "T_REEDS", "T_GOLD",
    "T_WHEAT", "T_BERRY", "T_FISH", "T_RUINS",
    "T_CASTLE_WALL", "T_CASTLE_GATE",
}
DECAL_TERRAINS = {"T_TALL_GRASS", "T_FLOWERS"}
TERRAIN_GROUP_DIR = {
    "ground": "grounds",
    "feature": "features",
    "decal": "decals",
}

def ground_specs() -> list[tuple[str, str, str, list[dict[str, str]]]]:
    return [
    ("grass", "grass ground", "top-down square tileable grass floor, readable as normal temperate movement ground", seasonal_items("temperate grassland tile", "winter: patchy snow or frost over grass, not a full snow biome tile")),
    ("meadow", "meadow ground", "top-down square lush meadow floor with softer vegetation than grass", seasonal_items("lush meadow ground", "winter: meadow with frost and patchy snow")),
    ("dirt", "dirt ground", "top-down square bare earth floor for worn paths, building yards, and exposed soil", [
        state_item("spring_damp", "bare earth in spring, damp with small green regrowth"),
        state_item("summer_dry", "bare earth in summer, dry and dusty"),
        state_item("autumn_wet", "bare earth in autumn, darker with leaf litter"),
        state_item("winter_frozen", "bare earth in winter, frozen with frost and patchy snow"),
        *rain_frames("rain", "bare earth"),
        *rain_frames("storm", "bare earth"),
    ]),
    ("road", "road ground", "top-down square packed road or cobble movement route floor", [
        state_item("spring_clear", "stone or packed road in clear spring weather"),
        state_item("summer_dry", "stone or packed road in dry summer weather"),
        state_item("autumn_leaf_litter", "stone or packed road with autumn leaf litter at the edges"),
        state_item("winter_snow_edges", "stone or packed road with snow gathered on edges but route still readable"),
        *rain_frames("rain", "stone or packed road"),
        *rain_frames("storm", "stone or packed road"),
    ]),
    ("mud", "mud ground", "top-down square wet mud floor with ruts and puddled surface", [
        state_item("clear_wet", "mud terrain in clear weather, wet dark surface and puddles"),
        state_item("drying_edges", "mud terrain drying toward dirt, cracked edges and shrinking puddles"),
        *rain_frames("rain", "mud terrain"),
        *rain_frames("storm", "mud terrain"),
        state_item("winter_frozen", "frozen mud with hard glossy ruts"),
        state_item("snow_dusted", "mud with dirty snow and slush on top"),
    ]),
    ("sand", "sand ground", "top-down square sandy floor for desert and beach areas", [
        state_item("base_clear", "default sandy ground; use this as the season-invariant default"),
        *rain_frames("rain", "sandy ground"),
        state_item("wind_scoured", "sandy ground with wind-scoured grain variation"),
    ]),
    ("dunes", "dunes ground", "top-down square sand dune ridge floor, not an upright feature", [
        state_item("base_clear", "default sand dune ridges; use this as the season-invariant default"),
        *rain_frames("rain", "sand dune ridges"),
        state_item("wind_scoured", "sand dune ridges with wind-shaped highlights and troughs"),
    ]),
    ("snow", "snow ground", "top-down square fully snow-covered replacement floor", [
        state_item("winter_default", "default snow cover, winter biome or fully snow-covered replacement terrain"),
        *rain_frames("snowfall", "snow cover"),
        state_item("packed_snow", "packed or wind-smoothed snow with subtle blue-grey dents"),
        state_item("deep_snow", "deep snow blanket with softened terrain detail"),
        state_item("thaw_slush", "dirty thawing slush with grass or earth beginning to show"),
        state_item("dirty_edges", "snow cover with muddy or trampled dirty edges"),
    ]),
    ("tundra", "tundra ground", "top-down square snow-biome floor with seasonal thaw/refreeze identity", [
        state_item("tundra_spring_thaw", "snow-biome floor in spring thaw, wet tundra and melting snow patches"),
        state_item("tundra_summer_bare_ground", "snow-biome floor in summer, bare tundra ground with hardy vegetation"),
        state_item("tundra_autumn_refreeze", "snow-biome floor in autumn refreeze, frost returning to tundra"),
        state_item("tundra_winter_snow", "snow-biome floor in winter, snow-covered tundra with cold blue shadows"),
        *rain_frames("snowfall", "tundra ground"),
    ]),
    ("ice", "ice ground", "top-down square frozen ice floor for frozen water or slick ground", [
        state_item("winter_default", "default frozen ice tile, blue-white with readable cracks"),
        *rain_frames("snowfall", "frozen ice"),
        state_item("thin_ice", "thin translucent ice with water colour still visible"),
        state_item("solid_ice", "solid winter ice with stronger blue-white coverage"),
        state_item("cracked_ice", "cracked ice with safe small-RTS readability"),
        state_item("thawing_edge", "thawing ice edge with water showing through"),
    ]),
    ("water", "water ground", "top-down square deep water source tile with animated/weather-reactive surface states", [
        state_item("clear", "deep water in clear weather"),
        *rain_frames("rain", "deep water"),
        *rain_frames("storm", "deep water"),
        state_item("thin_ice_edge", "deep water beginning to freeze at the edge"),
        state_item("thawing_open_water", "deep water reopening during thaw with broken ice edges"),
    ]),
    ("shallows", "shallows ground", "top-down square shallow water floor with visible bed", [
        state_item("clear", "shallow water with visible bed in clear weather"),
        *rain_frames("rain", "shallow water with visible bed"),
        *rain_frames("storm", "shallow water with visible bed"),
        state_item("thin_ice_edge", "shallow water beginning to freeze at the edge"),
        state_item("thawing_open_water", "shallow water reopening during thaw with broken ice edges"),
    ]),
    ("marsh", "marsh ground", "top-down square wet marsh floor, puddled and slow-looking but not an upright reed feature", [
        *seasonal_items("marsh wet ground", "winter: marsh wet ground frozen with frost and snow caught in vegetation"),
        *rain_frames("rain", "marsh wet ground"),
        *rain_frames("storm", "marsh wet ground"),
    ]),
    ("gravel", "gravel ground", "top-down square gravel floor for ruins, rocky paths, and exposed stone chips", seasonal_items("gravel terrain", "winter: gravel with frost and patchy snow between stones")),
    ("ash", "ash ground", "top-down square volcanic ash floor", [
        state_item("base_clear", "volcanic ash plain default state"),
        state_item("ember_flecks", "volcanic ash with subtle ember flecks"),
        state_item("rain_wet_dark", "volcanic ash darkened by rain"),
        state_item("storm_wet_dark", "volcanic ash very dark and wet in storm"),
        state_item("snow_dusted_rare", "volcanic ash with rare snow dusting"),
    ]),
    ("lava", "lava ground", "top-down square lava fissure or lava field floor with glow states", [
        state_item("base_glow", "lava fissure default glow"),
        state_item("glow_bright", "lava fissure brighter heat pulse state"),
        state_item("glow_dim", "lava fissure dimmer cooling pulse state"),
        *rain_frames("rain_steam", "lava fissure with rain steam"),
        *rain_frames("storm_steam", "lava fissure with storm steam"),
    ]),
    ("hills", "hills ground", "top-down square rolling hill and ridge floor, not a tall upright mountain sprite", seasonal_items("rolling hill terrain", "winter: rolling hills with snow on ridges and shaded exposed earth")),
    ("rocky", "rocky ground", "top-down square rocky floor used under mountains, boulders, and mineral deposits", [
        state_item("base_clear", "default rocky ground with small cracks and stone texture"),
        state_item("rain_wet_dark", "rocky ground darkened by rain"),
        state_item("frost", "rocky ground with frost in cracks and shaded crevices"),
        state_item("snow_light", "rocky ground with light snow in crevices"),
        state_item("snow_heavy", "rocky ground with heavier snow while rocks stay readable"),
    ]),
    ("castle_floor", "castle floor ground", "top-down square old castle paving or fortification floor", seasonal_items("old castle floor paving", "winter: old paving with frost, snow in cracks, and dirty exposed stones")),
]

def feature_specs() -> list[tuple[str, str, str, list[dict[str, str]], bool]]:
    return [
    ("forest", "forest feature", "transparent upright deciduous forest cluster anchored to tile centre; passable, harvestable, concealing", seasonal_depletion_items("oak forest canopy, trunks, and contact shadow", "fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed"), True),
    ("pine", "pine feature", "transparent upright pine forest cluster anchored to tile centre; passable, harvestable, concealing", seasonal_depletion_items("pine forest with conifer boughs, trunks, and contact shadow", "fully depleted wood resource: pine stumps, fallen branches, and exposed dirt where trees were removed"), True),
    ("palm", "palm feature", "transparent upright palm grove anchored to tile centre over sand", depletion_items("palm grove wood resource", "fully depleted palm grove: cut stumps, fallen fronds, and exposed sand/dirt"), False),
    ("dead_tree", "dead tree feature", "transparent upright dead-tree cluster anchored to tile centre", depletion_items("dead tree wood resource with grey trunks and moss", "fully depleted dead-tree resource: broken stumps and fallen limbs on dirt"), False),
    ("berry_bush", "berry bush feature", "transparent upright berry-bush feature anchored to tile centre over normal biome ground", seasonal_depletion_items("berry bush with visible berries", "fully depleted empty berry bush, sparse leaves and no visible berries"), False),
    ("wheat_crop", "wheat crop feature", "transparent crop feature anchored to tile centre over field or dirt ground", seasonal_depletion_items("wheat or crop field feature", "fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop"), False),
    ("fish_shoal", "fish shoal feature", "transparent water feature or low decal-like sprite anchored to tile centre", depletion_items("fish shoal with splashes and ripples on water", "fully depleted fish shoal: open water ripples with no visible fish"), False),
    ("gold_deposit", "gold deposit feature", "transparent ore deposit feature anchored to tile centre over rocky or dirt ground", depletion_items("gold deposit with bright ore vein or nuggets", "fully depleted mineable gold: dull rock scar and dirt with no bright ore left"), False),
    ("stone_boulders", "stone boulders feature", "transparent blocking boulder feature anchored to tile centre over rocky or biome ground", [
        state_item("base_clear", "default blocking stone boulder cluster"),
        state_item("rain_wet_dark", "stone boulders darkened by rain"),
        state_item("frost", "stone boulders with frost in cracks"),
        state_item("snowcap_light", "stone boulders with light snow caps"),
        state_item("snowcap_heavy", "stone boulders with heavier snow while silhouette stays readable"),
        state_item("damaged", "cracked or chipped stone boulders"),
    ], False),
    ("mountain_peak", "mountain peak feature", "transparent tall blocking mountain peak feature anchored to tile centre, allowed to overhang neighbouring tiles", [
        state_item("base_clear", "default mountain peak"),
        state_item("rain_wet_dark", "mountain peak darkened by rain"),
        state_item("frost", "mountain peak with frost in cracks and shaded crevices"),
        state_item("snowcap_light", "mountain peak with light snow cap"),
        state_item("snowcap_heavy", "mountain peak with heavier snow while silhouette stays readable"),
        state_item("damaged", "cracked or broken mountain face variant"),
    ], False),
    ("reeds", "reeds feature", "transparent upright wetland reeds anchored to tile centre; passable, slowing, concealing", [
        *seasonal_items("reed bed upright feature over wet ground", "winter: reed bed frozen with frost and snow caught in stems"),
        *rain_frames("rain", "reed bed upright feature"),
        *rain_frames("storm", "reed bed upright feature"),
        state_item("trampled", "reeds bent aside or trampled but still concealing"),
    ], True),
    ("ruins", "ruins feature", "transparent ruin object or footprint feature anchored to tile centre", [
        *seasonal_items("ancient rubble and broken stone ruins feature", "winter: ruins with light snow caught in cracks and on rubble tops"),
        state_item("damaged", "more collapsed ruin footprint with broken stones"),
        state_item("overgrown", "ruins with small vegetation growth and moss"),
    ], False),
    ("castle_wall", "castle wall feature", "transparent blocking castle wall feature anchored to tile centre", [
        state_item("base_clear", "default castle wall segment"),
        state_item("rain_wet_dark", "castle wall darkened by rain"),
        state_item("frost", "castle wall with frost on stone edges"),
        state_item("snow_light", "castle wall with light snow on upper surfaces"),
        state_item("snow_heavy", "castle wall with heavier settled snow while joins stay readable"),
        state_item("damaged", "damaged cracked castle wall segment"),
        state_item("broken", "broken castle wall segment with rubble"),
    ], False),
    ("castle_gate", "castle gate feature", "transparent castle gate feature anchored to tile centre over castle floor", [
        state_item("closed", "closed castle gate, clearly blocking"),
        state_item("open", "open castle gate, clearly passable"),
        state_item("locked", "locked or barred castle gate"),
        state_item("rain_wet_dark", "castle gate darkened by rain"),
        state_item("frost", "castle gate with frost on stone and wood edges"),
        state_item("snow_light", "castle gate with light snow on upper surfaces"),
        state_item("snow_heavy", "castle gate with heavier settled snow while passability state stays readable"),
        state_item("damaged", "damaged castle gate with cracks and splintering"),
        state_item("broken", "broken castle gate remains"),
    ], False),
]

def decal_specs() -> list[tuple[str, str, str, list[dict[str, str]]]]:
    return [
    ("flowers", "flowers decal", "transparent low wildflower overlay that sits flat on grass or meadow", seasonal_items("wildflower ground decal", "winter: dormant flower stems with patchy snow")),
    ("tall_grass", "tall grass decal", "transparent low/medium grass clump overlay; visual variation, not an independent blocker", seasonal_items("tall grass clumps", "winter: flattened frosted tall grass with patchy snow")),
    ("grass_tufts", "grass tufts decal", "transparent low grass tuft overlay for natural ground variation", seasonal_items("small grass tufts", "winter: small frosted grass tufts poking through snow")),
    ("small_stones", "small stones decal", "transparent low scattered-stone overlay for rocky, dirt, and path variation", [state_item("base_clear", "small flat scattered stones"), state_item("frost", "small stones with frost"), state_item("snow_dusted", "small stones with light snow dusting")]),
    ("puddles", "puddles decal", "transparent flat puddle overlay for rain, marsh, mud, and road edges", [state_item("rain_puddle_small", "small rain puddle"), state_item("rain_puddle_large", "larger rain puddle"), state_item("storm_puddle_choppy", "storm puddle with choppy surface"), state_item("frozen_puddle", "frozen puddle with thin ice")]),
    ("dirt_scuffs", "dirt scuffs decal", "transparent sparse dirt scuffs for low wear and settlement edges", [state_item("sparse", "sparse dirt scuffs"), state_item("medium", "medium dirt scuffs"), state_item("heavy", "heavy dirt scuffs")]),
    ("packed_path_marks", "packed path marks decal", "transparent packed dirt path fragments for medium wear", [state_item("short_fragment", "short packed path fragment"), state_item("straight_fragment", "straight packed path fragment"), state_item("corner_fragment", "corner or bend packed path fragment"), state_item("intersection_fragment", "small packed path intersection fragment")]),
    ("cobble_patches", "cobble patches decal", "transparent cobble or road creep patches for high wear", [state_item("small_patch", "small cobble patch"), state_item("edge_patch", "cobble edge patch"), state_item("dense_patch", "dense cobble patch"), state_item("broken_patch", "broken uneven cobble patch")]),
    ("wheel_ruts", "wheel ruts decal", "transparent wheel rut overlay for roads, yards, farms, and transport paths", [state_item("light_ruts", "light wheel ruts"), state_item("deep_ruts", "deep wheel ruts"), state_item("muddy_ruts", "muddy wheel ruts"), state_item("snow_ruts", "wheel ruts through snow")]),
    ("yard_clutter", "yard clutter decal", "transparent settlement clutter overlay that does not block movement", [state_item("small_clutter", "small yard clutter scraps"), state_item("tools_clutter", "tools and small worksite clutter"), state_item("mixed_clutter", "mixed settlement clutter"), state_item("snow_dusted_clutter", "snow-dusted yard clutter")]),
    ("crates_barrels", "crates and barrels decal", "transparent crates and barrels overlay for market, dock, town, and storage yards", [state_item("crates", "small crates cluster"), state_item("barrels", "small barrels cluster"), state_item("mixed", "mixed crates and barrels"), state_item("snow_dusted", "snow-dusted crates and barrels")]),
    ("log_piles", "log piles decal", "transparent log-pile overlay for lumber camps and forest work areas", [state_item("small_pile", "small log pile"), state_item("stacked_pile", "stacked log pile"), state_item("split_logs", "split logs and chips"), state_item("snow_dusted", "snow-dusted log pile")]),
    ("farm_tracks", "farm tracks decal", "transparent farm track and furrow overlay", [state_item("furrows", "simple farm furrows"), state_item("cart_tracks", "farm cart tracks"), state_item("harvest_tracks", "harvested-field tracks"), state_item("snow_dead_tracks", "snowy or winter-dead farm tracks")]),
    ("muddy_footprints", "muddy footprints decal", "transparent muddy footprint overlay for wet settlement and path wear", [state_item("sparse", "sparse muddy footprints"), state_item("cluster", "cluster of muddy footprints"), state_item("trail", "short footprint trail"), state_item("smudged", "smudged muddy footprints")]),
    ("snow_trampled_path_marks", "snow-trampled path marks decal", "transparent trampled-snow path overlay", [state_item("light_trample", "light trampled snow path marks"), state_item("packed_trample", "packed trampled snow path marks"), state_item("dirty_trample", "dirty trampled snow with exposed ground"), state_item("wheel_trample", "trampled snow with wheel marks")]),
    ("ore_bins", "ore bins decal", "transparent ore-bin and rock-pile overlay for mining camps", [state_item("small_ore_bin", "small ore bin"), state_item("ore_pile", "ore pile and bin"), state_item("mixed_rocks", "mixed rocks and ore scraps"), state_item("snow_dusted", "snow-dusted ore bins")]),
    ("sacks", "sacks decal", "transparent sacks and grain bags overlay for mills, farms, and markets", [state_item("small_sacks", "small sacks cluster"), state_item("grain_bags", "grain bags"), state_item("mixed_sacks", "mixed sacks and small crates"), state_item("snow_dusted", "snow-dusted sacks")]),
    ("dock_barrels", "dock barrels decal", "transparent dockside barrels, rope, and cargo overlay", [state_item("barrels_rope", "barrels and rope"), state_item("fish_crates", "fish crates and wet dock clutter"), state_item("cargo_stack", "small dock cargo stack"), state_item("snow_dusted", "snow-dusted dock cargo")]),
]


def grid_for_count(count: int) -> tuple[int, int]:
    if count <= 1:
        return 1, 1
    cols = math.ceil(math.sqrt(count))
    rows = math.ceil(count / cols)
    return cols, rows


def sheet_chunks(items: list[dict[str, str]]) -> list[list[dict[str, str]]]:
    return [items[i:i + MAX_SLOTS_PER_SHEET] for i in range(0, len(items), MAX_SLOTS_PER_SHEET)] or [[]]


def grid_for_sheet(total_count: int, sheet_count: int) -> tuple[int, int]:
    if total_count > MAX_SLOTS_PER_SHEET:
        return 4, 4
    return grid_for_count(sheet_count)


def slot_name(index: int, cols: int) -> str:
    row = index // cols + 1
    col = index % cols + 1
    return f"row {row}, column {col}"


def state_item(state_id: str, description: str) -> dict[str, str]:
    return {"id": state_id, "description": description}


def depletion_items(subject: str, depleted_result: str) -> list[dict[str, str]]:
    items = []
    for level_id, level_desc in DEPLETION_LEVELS:
        desc = f"{subject}, {level_desc}"
        if level_id == "depleted":
            desc = f"{subject}, {depleted_result}"
        items.append(state_item(level_id, desc))
    return items


def seasonal_depletion_items(subject: str, depleted_result: str) -> list[dict[str, str]]:
    items = []
    for season_id, season_desc in SEASONS:
        for level_id, level_desc in DEPLETION_LEVELS:
            desc = f"{subject}, {season_desc}, {level_desc}"
            if level_id == "depleted":
                desc = f"{subject}, {season_desc}, {depleted_result}"
            items.append(state_item(f"{season_id}_{level_id}", desc))
    return items


def seasonal_items(subject: str, winter_desc: str | None = None) -> list[dict[str, str]]:
    items = []
    for season_id, season_desc in SEASONS:
        desc = f"{subject}, {season_desc}"
        if season_id == "winter" and winter_desc:
            desc = winter_desc
        items.append(state_item(season_id, desc))
    return items


def rain_frames(prefix: str, subject: str) -> list[dict[str, str]]:
    if prefix.startswith("snowfall"):
        frame_1 = "falling snow interaction frame 1 with small snow puffs or fresh specks"
        frame_2 = "falling snow interaction frame 2 with shifted snow puffs or fresh specks"
    elif prefix.startswith("storm"):
        frame_1 = "storm reaction frame 1 with heavier splash, chop, wet-sheen, or steam details"
        frame_2 = "storm reaction frame 2 with shifted heavier splash, chop, wet-sheen, or steam details"
    elif prefix.startswith("rain_steam"):
        frame_1 = "rain-steam reaction frame 1 with small steam wisps"
        frame_2 = "rain-steam reaction frame 2 with shifted steam wisps"
    elif prefix.startswith("storm_steam"):
        frame_1 = "storm-steam reaction frame 1 with denser steam wisps"
        frame_2 = "storm-steam reaction frame 2 with shifted denser steam wisps"
    else:
        frame_1 = "rain reaction frame 1 with small splash or wet-sheen details"
        frame_2 = "rain reaction frame 2 with shifted splash or wet-sheen details"
    return [
        state_item(f"{prefix}_frame_1", f"{subject}, {frame_1}"),
        state_item(f"{prefix}_frame_2", f"{subject}, {frame_2}"),
    ]


def death_states_for(enum_name: str, category: str) -> list[dict[str, str]]:
    if category == "animals":
        return [
            {"id": "dead_unharvested", "description": "freshly killed animal body lying on the ground, full carcass, species silhouette still readable"},
            {"id": "partly_harvested", "description": "partly butchered and harvested carcass, some meat or hide cleanly removed, species still readable"},
            {"id": "mostly_harvested", "description": "mostly butchered and harvested carcass, clean bones beginning to show, not rotting"},
            {"id": "depleted_skeleton", "description": "fully harvested clean skeleton remains, species silhouette still readable, not decayed or rotten"},
        ]
    if category != "units":
        return []
    if enum_name == "E_PEASANT":
        return [
            {"id": "dead", "description": "dead villager body lying on the ground with clothing and simple tools still intact"},
            {"id": "decayed", "description": "human skeleton remains, with small clothing and tool scraps still readable"},
        ]
    if enum_name in {"E_CATAPULT", "E_TREBUCHET", "E_FISHING_BOAT", "E_WARSHIP", "E_TRANSPORT", "E_RAM"}:
        return [
            {"id": "dead", "description": "destroyed wreck, broken but still recognizable"},
            {"id": "decayed", "description": "weathered wreckage, with durable wood, metal, wheels, hull, or siege parts still readable"},
        ]
    return [
        {"id": "dead", "description": "dead human body lying on the ground with clothing, armour, weapons, and equipment still intact"},
        {"id": "decayed", "description": "human skeleton remains, with armour, weapons, and equipment still intact and readable"},
    ]


def add_death_states(enum_name: str, category: str, actions: list[dict[str, str]]) -> list[dict[str, str]]:
    death_states = death_states_for(enum_name, category)
    if not death_states:
        return actions
    filtered = [
        action for action in actions
        if "death" not in action["id"]
        and action["id"] not in {"dead", "decayed"}
        and action["id"] not in {"dead_unharvested", "partly_harvested", "mostly_harvested", "depleted_skeleton"}
        and "dead" not in action["description"].lower()
        and "decayed" not in action["description"].lower()
        and "harvested" not in action["description"].lower()
        and "destroyed wreck" not in action["description"].lower()
    ]
    return filtered + death_states


def append_unique_actions(actions: list[dict[str, str]], extras: list[dict[str, str]]) -> None:
    seen = {action["id"] for action in actions}
    for extra in extras:
        if extra["id"] in seen:
            continue
        actions.append(extra)
        seen.add(extra["id"])


def research_tier_variants(enum_name: str) -> list[dict[str, Any]]:
    lines = research_visual_lines_for_entity(enum_name)
    if not lines:
        return []
    variants: list[dict[str, Any]] = []
    for combo in itertools.product(*[line["tiers"] for line in lines]):
        variant_id = "__".join(tier["id"] for tier in combo)
        variant_name = " + ".join(tier["name"] for tier in combo)
        descriptions = [tier["description"] for tier in combo]
        researched = [tier["research"] for tier in combo if tier.get("research")]
        variants.append(
            {
                "id": variant_id,
                "name": variant_name,
                "description": "; ".join(descriptions),
                "research": researched,
                "is_default": not researched,
            }
        )
    return variants


def apply_research_variants(enum_name: str, actions: list[dict[str, str]]) -> list[dict[str, str]]:
    variants = research_tier_variants(enum_name)
    if not variants:
        return actions
    expanded: list[dict[str, str]] = []
    for variant in variants:
        for action in actions:
            expanded.append(
                {
                    "id": f"{variant['id']}__{action['id']}",
                    "description": f"{variant['name']}: {action['description']}; {variant['description']}",
                }
            )
    return expanded


def guided_action_description(enum_name: str, action_id: str, description: str) -> str:
    action = action_id.lower()
    if "no airborne ammunition" in description:
        return description
    if enum_name == "E_ARCHER":
        if action == "aim":
            return "aim with ammunition held in the weapon; ammunition is not airborne"
        if action == "release":
            return "release follow-through after the shot; weapon discharged, no airborne ammunition visible"
        if action == "reload":
            return "reload by taking ammunition from the quiver or bolt case; no airborne ammunition visible"
    if enum_name == "E_WARSHIP" and "fire" in action:
        return "fire or arrow-volley follow-through; do not draw released arrows or airborne shot in the ship frame"
    if enum_name in {"E_TOWER", "E_CASTLE"} and "firing" in action:
        return f"{description}; show launcher/garrison reaction only, with no airborne ammunition in the building frame"
    if enum_name == "E_CATAPULT":
        if action == "idle":
            return "idle catapult with one visible human operator at the controls; no loaded boulder unless the state says load"
        if action == "roll":
            return "operator walking beside the handles while the wheeled catapult rolls; no airborne ammunition"
        if action == "load":
            return "operator loading the boulder into the sling; ammunition visible because it has not been released"
        if action == "fire":
            return "operator has just released the throwing arm; sling empty and no airborne boulder visible"
        if action == "recoil":
            return "operator bracing after firing; arm recoiling empty, no airborne boulder visible"
    if enum_name == "E_TREBUCHET":
        if action == "idle":
            return "idle trebuchet with one visible human operator beside the winch or sling; no loaded boulder unless the state says load"
        if action == "roll":
            return "operator guiding the wheeled trebuchet while it rolls; no airborne ammunition"
        if action == "load":
            return "operator loading the boulder into the sling; ammunition visible because it has not been released"
        if action == "fire":
            return "operator has just released the trebuchet; sling empty and no airborne boulder visible"
        if action == "recoil":
            return "operator bracing after firing; throwing arm recoiling empty, no airborne boulder visible"
    if enum_name == "E_RAM":
        if action == "idle":
            return "idle ram with one visible human operator at the handles or cover opening"
        if action == "roll":
            return "operator walking with the wheeled ram as it rolls"
        if "impact" in action or "ramming" in action:
            return "operator bracing the ram during impact; no projectile or extra crew"
    return description


def building_environment_states(enum_name: str) -> list[dict[str, str]]:
    if enum_name == "E_FARM":
        return [
            state_item("night_lit", "completed farm at night with a tiny warm lantern or torch marker, crops still readable"),
            state_item("rain_frame_1", "completed farm in rain, wet furrows and small splash frame 1"),
            state_item("rain_frame_2", "completed farm in rain, wet furrows and small splash frame 2"),
            state_item("snow_light", "completed farm with light snow on furrows and crop edges"),
            state_item("snow_heavy", "completed farm heavily snowed or winter-dead but still identifiable as a farm"),
        ]
    return [
        state_item("night_lit", "completed building at night with warm torch, candle, forge, or window light; keep team colour readable"),
        state_item("rain_frame_1", "completed building in rain, wet roof/ground and drip or splash detail frame 1"),
        state_item("rain_frame_2", "completed building in rain, wet roof/ground and drip or splash detail frame 2"),
        state_item("snow_light", "completed building with light snow on roof edges, ledges, and ground contact"),
        state_item("snow_heavy", "completed building with heavy settled snow while silhouette and team colour stay readable"),
    ]


def entity_actions(enum_name: str, category: str, stats: dict[str, Any], audit: dict[str, str]) -> list[dict[str, str]]:
    if enum_name == "E_PEASANT":
        actions = []
        for action in peasant_actions():
            if action["id"] == "death":
                continue
            description = action.get("description") or action.get("id", "")
            actions.append({"id": action["id"], "description": clean_cell(description)})
        return add_death_states(enum_name, category, actions)

    out = []
    profile = entity_profile(enum_name, audit)
    for state in split_list(profile.get("required_states", "")):
        action_id = lower_slug(state.replace("/", " "))
        out.append({"id": action_id, "description": guided_action_description(enum_name, action_id, state)})
    if category == "buildings":
        append_unique_actions(out, [
            state_item("construction_0_foundation", "0-33 percent construction: foundation footprint and early site materials"),
            state_item("construction_1_frame", "34-66 percent construction: visible frame and scaffolding"),
            state_item("construction_2_nearly_complete", "67-99 percent construction: nearly complete shell with final work visible"),
            state_item("complete", "complete usable building"),
            state_item("damaged", "damaged building below half HP, readable but not destroyed"),
        ])
        if enum_name in {"E_TOWNHALL", "E_CASTLE"}:
            append_unique_actions(out, [state_item("training_peasant", "building visibly training a peasant")])
        if enum_name == "E_BARRACKS":
            append_unique_actions(out, [state_item("training_infantry", "building visibly training infantry")])
        if enum_name == "E_STABLE":
            append_unique_actions(out, [state_item("training_cavalry", "building visibly training cavalry")])
        if enum_name == "E_DOCK":
            append_unique_actions(out, [state_item("training_ship", "building visibly training a ship")])
        if enum_name in {"E_TOWNHALL", "E_HOUSE", "E_TOWER", "E_CASTLE"}:
            append_unique_actions(out, [
                state_item("garrisoned", "building visibly contains a garrison"),
                state_item("garrison_firing", "building body visibly reacting while its garrison fires"),
            ])
        if enum_name == "E_BLACKSMITH":
            append_unique_actions(out, [
                state_item("researching_iron_weapons", "blacksmith visibly researching Iron Weapons"),
                state_item("researching_crossbows", "blacksmith visibly researching Crossbows"),
            ])
        append_unique_actions(out, building_environment_states(enum_name))
    if enum_name == "E_TRANSPORT":
        append_unique_actions(out, [
            state_item("empty", "transport with no visible cargo load"),
            state_item("loaded_partial", "transport partially loaded using covered cargo, weight, flags, or silhouette cues"),
            state_item("loaded_full", "transport fully loaded using covered cargo, weight, flags, or silhouette cues"),
            state_item("load_unload", "transport in load or unload state without drawing passenger identities"),
        ])
    for action in out:
        action["description"] = guided_action_description(enum_name, action["id"], action["description"])
    return apply_research_variants(enum_name, add_death_states(enum_name, category, out))


def terrain_items(enum_name: str, audit: dict[str, str]) -> list[dict[str, str]]:
    if enum_name == "T_BERRY":
        return seasonal_depletion_items(
            "berry bush with visible berries",
            "fully depleted empty berry bush, sparse leaves and no visible berries; runtime replacement is grass",
        )
    if enum_name == "T_WHEAT":
        return seasonal_depletion_items(
            "wheat or crop field",
            "fully depleted or harvested stubble/furrows; in winter this should read as dead or snowed crop",
        )
    if enum_name in {"T_FOREST", "T_PINE"}:
        subject = "oak forest canopy and trunks" if enum_name == "T_FOREST" else "pine forest with conifer boughs"
        return seasonal_depletion_items(
            subject,
            "fully depleted wood resource: stumps, fallen branches, and exposed dirt where trees were removed",
        )
    if enum_name == "T_PALM":
        return depletion_items(
            "palm grove wood resource on sandy ground",
            "fully depleted palm grove: cut stumps, fallen fronds, and exposed sand/dirt",
        )
    if enum_name == "T_DEAD_TREE":
        return depletion_items(
            "dead tree wood resource with grey trunks and moss",
            "fully depleted dead-tree resource: broken stumps and fallen limbs on dirt",
        )
    if enum_name == "T_GOLD":
        return depletion_items(
            "gold deposit with bright ore vein or nuggets",
            "fully depleted mineable gold: dull rock scar and dirt with no bright ore left",
        )
    if enum_name == "T_FISH":
        return depletion_items(
            "fish shoal with splashes and ripples on water",
            "fully depleted fish shoal: open water ripples with no visible fish; runtime replacement is water",
        )

    seasonal_ground = {
        "T_GRASS": ("temperate grassland tile", "winter: patchy snow or frost over grass, not a full snow biome tile"),
        "T_TALL_GRASS": ("tall grass clumps", "winter: flattened frosted tall grass with patchy snow"),
        "T_FLOWERS": ("wildflower ground", "winter: dormant flower stems with patchy snow"),
        "T_MEADOW": ("lush meadow ground", "winter: meadow with frost and patchy snow"),
        "T_HILLS": ("rolling hill terrain", "winter: rolling hills with snow on ridges and shaded exposed earth"),
        "T_RUINS": ("ancient rubble and broken stone ruins", "winter: ruins with light snow caught in cracks and on rubble tops"),
        "T_GRAVEL": ("gravel terrain", "winter: gravel with frost and patchy snow between stones"),
        "T_CASTLE_FLOOR": ("old castle floor paving", "winter: old paving with frost, snow in cracks, and dirty exposed stones"),
    }
    if enum_name in seasonal_ground:
        subject, winter_desc = seasonal_ground[enum_name]
        return seasonal_items(subject, winter_desc)

    if enum_name == "T_DIRT":
        return [
            state_item("spring_damp", "bare earth in spring, damp with small green regrowth"),
            state_item("summer_dry", "bare earth in summer, dry and dusty"),
            state_item("autumn_wet", "bare earth in autumn, darker with leaf litter"),
            state_item("winter_frozen", "bare earth in winter, frozen with frost and patchy snow"),
            *rain_frames("rain", "bare earth"),
            *rain_frames("storm", "bare earth"),
        ]
    if enum_name == "T_ROAD":
        return [
            state_item("spring_clear", "stone or packed road in clear spring weather"),
            state_item("summer_dry", "stone or packed road in dry summer weather"),
            state_item("autumn_leaf_litter", "stone or packed road with autumn leaf litter at the edges"),
            state_item("winter_snow_edges", "stone or packed road with snow gathered on edges but route still readable"),
            *rain_frames("rain", "stone or packed road"),
            *rain_frames("storm", "stone or packed road"),
        ]
    if enum_name == "T_MUD":
        return [
            state_item("clear_wet", "mud terrain in clear weather, wet dark surface and puddles"),
            state_item("drying_edges", "mud terrain drying toward dirt, cracked edges and shrinking puddles"),
            *rain_frames("rain", "mud terrain"),
            *rain_frames("storm", "mud terrain"),
            state_item("winter_frozen", "frozen mud with hard glossy ruts"),
            state_item("snow_dusted", "mud with dirty snow and slush on top"),
        ]
    if enum_name in {"T_WATER", "T_SHALLOWS"}:
        subject = "deep water" if enum_name == "T_WATER" else "shallow water with visible bed"
        return [
            state_item("clear", f"{subject} in clear weather"),
            *rain_frames("rain", subject),
            *rain_frames("storm", subject),
            state_item("thin_ice_edge", f"{subject} beginning to freeze at the edge"),
            state_item("thawing_open_water", f"{subject} reopening during thaw with broken ice edges"),
        ]
    if enum_name in {"T_MARSH", "T_REEDS"}:
        subject = "marsh wet ground" if enum_name == "T_MARSH" else "reed bed over wet ground"
        return [
            *seasonal_items(subject, f"winter: {subject} frozen with frost and snow caught in vegetation"),
            *rain_frames("rain", subject),
            *rain_frames("storm", subject),
        ]
    if enum_name == "T_SNOW":
        return [
            state_item("winter_default", "default snow cover, winter biome or fully snow-covered replacement terrain"),
            *rain_frames("snowfall", "snow cover"),
            state_item("packed_snow", "packed or wind-smoothed snow with subtle blue-grey dents"),
            state_item("deep_snow", "deep snow blanket with softened terrain detail"),
            state_item("thaw_slush", "dirty thawing slush with grass or earth beginning to show"),
            state_item("dirty_edges", "snow cover with muddy or trampled dirty edges"),
        ]
    if enum_name == "T_ICE":
        return [
            state_item("winter_default", "default frozen ice tile, blue-white with readable cracks"),
            *rain_frames("snowfall", "frozen ice"),
            state_item("thin_ice", "thin translucent ice with water colour still visible"),
            state_item("solid_ice", "solid winter ice with stronger blue-white coverage"),
            state_item("cracked_ice", "cracked ice with safe small-RTS readability"),
            state_item("thawing_edge", "thawing ice edge with water showing through"),
        ]
    if enum_name in {"T_SAND", "T_DUNES"}:
        subject = "sandy ground" if enum_name == "T_SAND" else "sand dune ridges"
        return [
            state_item("base_clear", f"default {subject}; use this as the season-invariant default"),
            *rain_frames("rain", subject),
            state_item("wind_scoured", f"{subject} with wind-scoured ridges or grain variation"),
        ]
    if enum_name in {"T_MOUNTAIN", "T_STONE"}:
        subject = "mountain peak" if enum_name == "T_MOUNTAIN" else "stone boulder terrain"
        return [
            state_item("base_clear", f"default {subject}"),
            state_item("rain_wet_dark", f"{subject} darkened by rain"),
            state_item("frost", f"{subject} with frost in cracks and shaded crevices"),
            state_item("snowcap_light", f"{subject} with light snow cap"),
            state_item("snowcap_heavy", f"{subject} with heavier snow while silhouette stays readable"),
        ]
    if enum_name == "T_LAVA":
        return [
            state_item("base_glow", "lava fissure default glow"),
            state_item("glow_bright", "lava fissure brighter heat pulse state"),
            state_item("glow_dim", "lava fissure dimmer cooling pulse state"),
            *rain_frames("rain_steam", "lava fissure with rain steam"),
            *rain_frames("storm_steam", "lava fissure with storm steam"),
        ]
    if enum_name == "T_ASH":
        return [
            state_item("base_clear", "volcanic ash plain default state"),
            state_item("ember_flecks", "volcanic ash with subtle ember flecks"),
            state_item("rain_wet_dark", "volcanic ash darkened by rain"),
            state_item("storm_wet_dark", "volcanic ash very dark and wet in storm"),
            state_item("snow_dusted_rare", "volcanic ash with rare snow dusting"),
        ]
    if enum_name in {"T_CASTLE_WALL", "T_CASTLE_GATE"}:
        subject = "ruined castle wall" if enum_name == "T_CASTLE_WALL" else "ruined castle gate threshold"
        return [
            state_item("base_clear", f"default {subject}"),
            *rain_frames("rain", subject),
            state_item("frost", f"{subject} with frost on stone edges"),
            state_item("snow_light", f"{subject} with light snow on upper surfaces"),
            state_item("snow_heavy", f"{subject} with heavier settled snow while joins stay readable"),
        ]

    raw = audit.get("required_variants", "")
    if not raw:
        return [{"id": "base_tile", "description": "base terrain tile"}]
    items = []
    for item in re.split(r";", raw):
        item = clean_cell(item)
        if item:
            items.append({"id": lower_slug(item), "description": item})
    return items or [{"id": "base_tile", "description": raw}]


def terrain_layer_contract(enum_name: str) -> dict[str, str]:
    if enum_name in FEATURE_TERRAINS:
        return {
            "group": "features",
            "noun": "feature sprite",
            "projection": "transparent feature sprite anchored over a projected isometric map tile",
            "generation": "transparent-background feature sprites anchored to the tile centre; do not draw a full ground tile except a small contact shadow if needed",
            "item": "sprite",
        }
    if enum_name in DECAL_TERRAINS:
        return {
            "group": "decals",
            "noun": "terrain decal",
            "projection": "transparent low ground decal over a top-down source tile",
            "generation": "transparent low/flat decal overlays that sit on top of normal ground",
            "item": "decal",
        }
    return {
        "group": "grounds",
        "noun": "ground tile",
        "projection": "top-down square source tile, projected into an isometric diamond in-app",
        "generation": "top-down square source tiles that the app will project into isometric diamonds",
        "item": "tile",
    }


def md_list(items: list[str]) -> str:
    if not items:
        return "- None\n"
    return "".join(f"- {item}\n" for item in items)


def player_colour_text(player_colour: dict[str, str] | None) -> str:
    if not player_colour:
        return "none"
    return f"{player_colour['name']} ({player_colour['hex']})"


def create_each_sentence(item_noun: str, count: int, singular_label: str, plural_label: str) -> str:
    label = singular_label if count == 1 else plural_label
    return f"Create one {item_noun} for each of the {count} listed {label}."


def split_guidance_sentence(count: int, plural_label: str) -> str:
    if count <= MAX_SLOTS_PER_SHEET:
        return ""
    return (
        f" Since there are more than {MAX_SLOTS_PER_SHEET} {plural_label}, split them across multiple images, "
        "each image using a 4 by 4 grid."
    )


def sheet_label(sheet_index: int, sheet_count: int) -> str:
    if sheet_count == 1:
        return "Sheet"
    return f"Sheet {sheet_index} of {sheet_count}"


def append_slot_order(lines: list[str], sheets: list[list[dict[str, str]]], total_count: int) -> None:
    for sheet_index, sheet in enumerate(sheets, start=1):
        cols, rows = grid_for_sheet(total_count, len(sheet))
        if len(sheets) > 1:
            lines.append(f"- Sheet {sheet_index} of {len(sheets)}: {cols} by {rows} grid")
        else:
            lines.append(f"- Grid: {cols} by {rows}")
        for i, item in enumerate(sheet):
            lines.append(f"  - {slot_name(i, cols)}: {item['description']}")


def common_sheet_contract(
    group: str,
    item_noun: str,
    team_color_required: bool,
    player_colour: dict[str, str] | None = None,
) -> list[str]:
    team_rule_lines = []
    if team_color_required:
        team_rule_lines = [
            f"- Team colour: Use the recommended preview player colour {player_colour_text(player_colour)} only in deliberate maskable areas such as banners, shields, cloth trim, pennants, sails, or painted markers. Keep skin, stone, wood, shadows, weapons, animals, and cargo out of team colour."
        ]
    if group == "grounds":
        return [
            "## Image Output Contract",
            "",
            "- Output kind: reference contact sheet for planning and review.",
            "- Per-cell target: one complete tile sample matching the listed state, filling its grid cell edge-to-edge.",
            "- Background: use opaque tile art inside each cell. Do not use transparency for ground cells unless the state explicitly needs water edge alpha in a later production pass.",
            "- Gutters: keep clear separation between cells so each tile sample can be cropped independently.",
            "- Consistency: keep the same material identity, palette, lighting direction, detail scale, and outline weight across every slot in the file.",
            "- Tile edges: make each cell seamless on all four edges; do not add interior padding, drop shadows, borders, vignettes, or fade-outs.",
            *team_rule_lines,
            "- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, diamond-shaped tiles, or extra unlisted states.",
            "",
        ]
    background = "Use a transparent sheet background. If the image tool cannot produce alpha, use one flat #ff00ff magenta background and clear gutters between cells."
    return [
        "## Image Output Contract",
        "",
        "- Output kind: reference contact sheet for planning and review.",
        f"- Per-cell target: one complete {item_noun} matching the listed state, centred in its grid cell.",
        f"- Background: {background}",
        "- Gutters: keep clear separation between cells so each slot can be cropped or regenerated independently.",
        "- Consistency: keep the same asset identity, palette, lighting direction, scale, and outline weight across every slot in the file.",
        "- Margins: leave enough padding that no silhouette, weapon, tool, projectile, shadow, crop, corpse, decal, or effect touches a cell edge.",
        *team_rule_lines,
        "- Negative prompt: no text, labels, numbers, arrows, UI chrome, watermarks, signatures, photo texture, heavy blur, cropped silhouettes, or extra unlisted states.",
        "",
    ]


def sheet_prompt_background_sentence(group: str) -> str:
    if group == "grounds":
        return "Use opaque edge-to-edge tile art in each cell, with clear gutters between cells and no transparent border."
    return "Use transparent background, or a single flat #ff00ff magenta background if transparency is not available."


def production_follow_up(group: str, item_noun: str) -> list[str]:
    if group == "grounds":
        details = [
            "Final production ground art should be exported as one standalone square tile per accepted state.",
            "Each tile should be seamless edge-to-edge and still read clearly after the game projects it into an isometric diamond.",
            "Avoid distinctive repeated landmarks near tile edges unless the state is intentionally road, water, lava, or path-like.",
        ]
    elif group == "decals":
        details = [
            "Final production decal art should be exported as one standalone square image per accepted state.",
            "Use transparent background or a flat #ff00ff magenta key background, with only the low overlay art visible.",
            "Keep decal opacity and silhouette subtle enough that it reads as ground wear or clutter, not a blocking object.",
        ]
    elif group == "features":
        details = [
            "Final production feature art should be exported as one standalone square image per accepted state.",
            "Use transparent background or a flat #ff00ff magenta key background, with one anchored sprite and its contact shadow fully inside the square.",
            "Keep the tile anchor visually stable across depletion, weather, damage, and seasonal variants.",
        ]
    elif group == "effects-ui":
        details = [
            "Final production effect/UI art should be exported as one standalone square image per accepted item.",
            "Use transparent background or a flat #ff00ff magenta key background, with the effect centred and readable over both light and dark terrain.",
            "Keep rings and command markers centred on the tile anchor; keep screen UI markers compact and readable at small scale.",
        ]
    else:
        details = [
            "Final production sprite art should be exported as one standalone square image per accepted state, direction, and frame.",
            "Use transparent background or a flat #ff00ff magenta key background, with the full sprite and shadow inside the square.",
            "Keep feet, hull base, wheels, siege base, or building footprint anchored consistently across variants.",
        ]
    return [
        "## Production Follow-Up",
        "",
        *[f"- {line}" for line in details],
        f"- Treat the sheet as the visual decision record; generate or crop final production {item_noun} images only after the sheet slot is accepted.",
        "",
    ]


def sigil_targets(team_slots: list[str]) -> str:
    if not team_slots:
        return "the main player-colour cloth marker"
    preferred = [
        slot for slot in team_slots
        if any(word in slot.lower() for word in ["shield", "tabard", "sail", "pennant", "banner", "plaque", "flag", "sash"])
    ]
    chosen = preferred or team_slots[:1]
    chosen = [slot if slot.lower().startswith("the ") else f"the {slot}" for slot in chosen]
    if len(chosen) == 1:
        return chosen[0]
    return ", ".join(chosen[:-1]) + f", and {chosen[-1]}"


def player_colour_contract(
    category: str,
    stats: dict[str, Any],
    player_colour: dict[str, str] | None,
    team_slots: list[str],
) -> list[str]:
    if category not in {"units", "buildings"}:
        return []
    lines = [
        "## Player Colour",
        "",
        f"- Use {player_colour_text(player_colour)} for the player-colour areas listed above.",
    ]
    if is_military_unit(category, stats):
        lines.extend(
            [
                f"- Add a {PLAYER_SIGIL['description']} on {sigil_targets(team_slots)}.",
            ]
        )
    lines.append("")
    return lines


def ammunition_contract(enum_name: str) -> list[str]:
    refs = ammunition_refs_for_entity(enum_name)
    if not refs:
        return []
    lines = [
        "## Ammunition References",
        "",
        "- Unit/building sheets may show ammunition only while it is still loaded, nocked, held, or otherwise not yet released.",
        "- Released, airborne, or impact ammunition must be generated from the ammunition files below, not baked into the unit/building frame.",
    ]
    for slug in refs:
        lines.append(f"- `{slug}`: `art/tiles/image-spec/ammunition/{slug}.md`")
    lines.append("")
    return lines


def entity_direction_contract(category: str, directions: list[str]) -> list[str]:
    if category == "buildings":
        return [
            "## Direction And Anchor Contract",
            "",
            "- `south` means the building is drawn in the Realm isometric three-quarter view, with the readable front facing down-screen/right enough to match the map perspective.",
            "- Keep the footprint visually centred on the tile footprint listed above. Larger buildings may fill their footprint, but should not look like a full terrain tile.",
            "- Use a consistent ground-contact baseline and shadow direction across construction, completed, damaged, garrisoned, production, weather, and ruin states.",
            "",
        ]
    if "front" in directions and "back" in directions:
        return [
            "## Direction And Anchor Contract",
            "",
            "- `front` means a three-quarter RTS front angle, body or object turned about 30-45 degrees toward screen right. It is not a flat face-on mascot pose.",
            "- `back` means the matching rear-right three-quarter angle, with shoulders, hull, wheels, cloak, or equipment forming a visible diagonal. It is not a flat rear diagram.",
            "- Do not generate mirrored left-facing source art. The renderer mirrors front/back source art when needed.",
            "- Keep feet, corpse baseline, wheels, boat hull contact, siege base, carried goods, and weapon arcs inside the cell with stable anchor and scale.",
            "",
        ]
    return []


def entity_quality_notes(enum_name: str, category: str, stats: dict[str, Any]) -> list[str]:
    if category == "buildings":
        return [
            "## Entity-Specific Art Notes",
            "",
            "- Draw one coherent building design across every state; construction, damaged, garrisoned, training, weather, and ruin states should all visibly derive from the same structure.",
            "- Do not bake a full square terrain tile into the building art. A small contact shadow and immediate footprint dirt are acceptable.",
            "- Keep doors, banners, roofline, walls, team-colour markers, and silhouette readable at small RTS scale.",
            "- Production or research states should add visible activity cues such as banners, lit windows, work glow, smoke, open doors, or small queue markers without becoming UI icons.",
            "",
        ]
    if category == "animals":
        return [
            "## Entity-Specific Art Notes",
            "",
            "- Keep species silhouette readable in living, attacking, fleeing, dead, partly harvested, mostly harvested, and skeleton states.",
            "- Carcass states should lie naturally on the ground and stay inside the cell; avoid gore-heavy imagery.",
            "- The depleted skeleton must still suggest the original animal species rather than a generic bone pile.",
            "",
        ]
    if category == "units":
        notes = [
            "## Entity-Specific Art Notes",
            "",
            "- Keep the same unit identity, clothing, armour, hull, siege frame, weapon set, and carried-equipment scale across every state.",
            "- State changes should be literal and readable: attacks show the weapon setup before release or the follow-through after release, gathering shows the tool/resource, carrying shows the carried material, and death/decay keeps durable gear visible.",
            "- Do not add terrain patches, target enemies, resource nodes, UI badges, or unrelated helper characters inside the cell.",
        ]
        if enum_name in {"E_FISHING_BOAT", "E_WARSHIP", "E_TRANSPORT"}:
            notes.append("- Naval units should sit on transparent background without baked water, while hull direction and sail/team-colour areas remain readable.")
        if is_operated_unit(enum_name):
            notes.append("- This is an operated movable machine: show exactly one visible human operator in every intact state, actively handling the machine for that state.")
            notes.append("- The operator counts as part of the unit identity; do not add extra crew beyond that one operator.")
        if enum_name in {"E_CATAPULT", "E_TREBUCHET", "E_RAM"}:
            notes.append("- Siege units should keep wheels, frame, sling/ram head, and destroyed wreck silhouettes readable from both source directions.")
        notes.append("")
        return notes
    return []


def research_visual_contract(enum_name: str) -> list[str]:
    variants = research_tier_variants(enum_name)
    if not variants:
        return []
    lines = [
        "## Research Visual Tiers",
        "",
        "Generate the complete state set for each equipment tier below.",
    ]
    for variant in variants:
        if variant["is_default"]:
            label = "Starting equipment (default, no research required)"
        elif len(variant["research"]) == 1:
            label = f"After {variant['research'][0]} research"
        else:
            label = "After " + " and ".join(variant["research"]) + " research"
        lines.append(f"- {label}: {variant['description']}.")
    lines.append("")
    return lines


def entity_markdown(enum_name: str, category: str, stats: dict[str, Any], audit: dict[str, str]) -> str:
    profile = entity_profile(enum_name, audit)
    actions = entity_actions(enum_name, category, stats, audit)
    sheets = sheet_chunks(actions)
    directions = ["front", "back"] if category != "buildings" else ["south"]
    team_color_required = category in {"units", "buildings"}
    team_slots = split_list(profile.get("team_color_slots", "")) if team_color_required else []
    player_colour = recommended_player_colour(enum_name, stats, audit) if team_color_required else None
    ammunition_sentence = " Use ammunition reference files for released projectiles." if ammunition_refs_for_entity(enum_name) else ""
    footprint = stats["footprint"]

    direction_sentence = (
        f"Generate one Realm sprite reference sheet per direction for **{stats['name']}**."
        if len(directions) > 1
        else f"Generate a Realm sprite reference sheet for **{stats['name']}**."
    )
    direction_prompt = (
        f"Valid directions are {', '.join(directions)}. Produce one sheet at a time for the requested direction, "
        "using the same state grid for each direction."
        if len(directions) > 1
        else f"Use {directions[0]} direction artwork."
    )

    lines = [
        f"# {stats['name']} Image Generation Prompt",
        "",
        direction_sentence,
        "",
        "## Art Brief",
        "",
        f"- Source role: {profile.get('role') or 'unspecified'}",
        f"- Visual design: {profile.get('visual_design') or 'unspecified'}",
        f"- Projection: upright sprite anchored over projected isometric map tiles",
        f"- Footprint: {footprint['w']} by {footprint['h']} tile(s)",
        f"- Directions: {', '.join(directions)}",
        *(["- Team colour required: yes"] if team_color_required else []),
        "",
        "## Team Colour Slots",
        "",
        md_list(team_slots).rstrip(),
        "",
        *player_colour_contract(category, stats, player_colour, team_slots),
        *ammunition_contract(enum_name),
        *research_visual_contract(enum_name),
        *entity_direction_contract(category, directions),
        *common_sheet_contract(category, "sprite frame", team_color_required, player_colour),
        *entity_quality_notes(enum_name, category, stats),
        "## States To Generate",
        "",
        f"Generate **one frame for each state**. There are {len(actions)} state(s). Each image may contain at most **16 states** in a **4 by 4** grid.",
        "",
    ]

    if category == "buildings":
        lines.extend(
            [
                "Environment states are generated only for the completed building. Do not make a full cross-product of construction, damaged, garrisoned, and weather states.",
                "Night states should add visible warm light sources; broad nighttime dimming can still be handled by the renderer.",
                "",
            ]
        )
    elif category == "animals":
        lines.extend(
            [
                "Animal carcass states use four depletion levels: dead unharvested, partly harvested, mostly harvested, and depleted skeleton.",
                "Harvested animal states must look butchered and processed for food or hide, not rotten, moldy, or naturally decayed.",
                "",
            ]
        )
        if enum_name == "E_WOLF":
            lines.extend(
                [
                    "Wolf carcass depletion states are for visual consistency with other animals only; wolf carcass harvesting is not enabled in gameplay.",
                    "",
                ]
            )

    for sheet_index, sheet in enumerate(sheets, start=1):
        cols, rows = grid_for_sheet(len(actions), len(sheet))
        label = sheet_label(sheet_index, len(sheets))
        lines.extend([f"### {label}", ""])
        lines.append(f"Use a **{cols} by {rows}** grid for this sheet.")
        if len(actions) > MAX_SLOTS_PER_SHEET and len(sheet) < MAX_SLOTS_PER_SHEET:
            lines.append("Leave unused cells empty.")
        lines.append("")
        for i, action in enumerate(sheet):
            lines.append(f"- {slot_name(i, cols)}: `{action['id']}` - {action['description']}")
        lines.append("")

    team_colour_prompt = ""
    if team_color_required:
        team_colour_prompt = (
            f"Team colour is required and the recommended preview player colour is {player_colour_text(player_colour)}. "
        )
    state_create_prompt = create_each_sentence("frame", len(actions), "state", "states")
    state_split_prompt = split_guidance_sentence(len(actions), "states")
    lines.extend(
        [
            *production_follow_up(category, "sprite frame"),
            "",
            "## Prompt",
            "",
            (
                f"Generate sprites for my Realm {stats['name']}. The footprint is {footprint['w']} by "
                f"{footprint['h']} tile(s). {team_colour_prompt}"
                f"{direction_prompt} {state_create_prompt}{state_split_prompt} Order states left to right "
                "and top to bottom within each sheet. Keep the character or building "
                "consistent across every slot. Use transparent background, or a single flat #ff00ff magenta background "
                "if transparency is not available. Use clean readable small-RTS proportions, stable anchor, clear gutters, "
                f"no text labels, no numbers, no watermark, and no cropped artwork.{ammunition_sentence}"
            ),
            "",
            "Slot order:",
        ]
    )
    append_slot_order(lines, sheets, len(actions))
    lines.append("")
    return "\n".join(lines)


def terrain_markdown(enum_name: str, runtime_name: str, audit: dict[str, str]) -> str:
    items = terrain_items(enum_name, audit)
    sheets = sheet_chunks(items)
    default_state = items[0]["id"] if items else "base_tile"
    layer = terrain_layer_contract(enum_name)
    lines = [
        f"# {runtime_name} Terrain Image Generation Prompt",
        "",
        f"Generate a Realm terrain reference sheet for **{runtime_name}**.",
        "",
        "## Art Brief",
        "",
        f"- Source role: {audit.get('terrain_type', 'terrain')} ({layer['noun']})",
        f"- Visual design: {audit.get('visual_design', 'unspecified')}",
        f"- Projection: {layer['projection']}",
        f"- Layer category: {layer['group']}",
        "- Footprint: 1 by 1 tile",
        "- Directions: tile",
        f"- Default state: `{default_state}`",
        "",
        "## States Or Variants To Generate",
        "",
        f"Generate **one {layer['item']} for each listed state or variant group**. There are {len(items)} item(s). Each image may contain at most **16 items** in a **4 by 4** grid.",
        "The first listed item is the default state. Use renderer tinting or global precipitation overlays for broad ambience; generate asset-specific states where the material, snow cover, rain reaction, or resource amount visibly changes.",
        "",
    ]
    if enum_name in {"T_FOREST", "T_PINE", "T_REEDS"}:
        lines.extend(
            [
                "Concealing feature contract: generate or plan separate `back` and `front_occluder` layers so units can be drawn partly behind vegetation.",
                "If the art tool can only produce one sprite, use the same sprite plus an occlusion mask until split-layer art exists.",
                "",
            ]
        )
    for sheet_index, sheet in enumerate(sheets, start=1):
        cols, rows = grid_for_sheet(len(items), len(sheet))
        label = sheet_label(sheet_index, len(sheets))
        lines.extend([f"### {label}", ""])
        lines.append(f"Use a **{cols} by {rows}** grid for this sheet.")
        if len(items) > MAX_SLOTS_PER_SHEET and len(sheet) < MAX_SLOTS_PER_SHEET:
            lines.append("Leave unused cells empty.")
        lines.append("")
        for i, item in enumerate(sheet):
            lines.append(f"- {slot_name(i, cols)}: `{item['id']}` - {item['description']}")
        lines.append("")

    lines.extend(
        [
            "",
            "## Prompt",
            "",
            (
                f"Generate Realm terrain assets for {runtime_name}. Use {layer['generation']}. "
                f"{create_each_sentence(layer['item'], len(items), 'state or variant group', 'states or variant groups')} "
                f"The default state is {default_state}. "
                f"{split_guidance_sentence(len(items), 'items')} "
                "Order items left to right and top to bottom within each sheet. Keep the terrain style consistent "
                "across every slot. Use clean readable small-RTS tile art, no text labels, no numbers, no watermark, "
                "and no cropped artwork."
            ),
            "",
            "Slot order:",
        ]
    )
    append_slot_order(lines, sheets, len(items))
    lines.append("")
    return "\n".join(lines)


def target_asset_markdown(
    group: str,
    slug: str,
    display_name: str,
    visual_design: str,
    projection: str,
    generation: str,
    item_noun: str,
    items: list[dict[str, str]],
    extra_notes: list[str] | None = None,
) -> str:
    sheets = sheet_chunks(items)
    default_state = items[0]["id"] if items else "base"
    title_name = display_name.title().replace(" Ui ", " UI ").replace(" Ui", " UI")
    lines = [
        f"# {title_name} Image Generation Prompt",
        "",
        f"Generate Realm image sheets for **{display_name}**.",
        "",
        "## Art Brief",
        "",
        f"- Asset group: {group}",
        f"- Asset id: {slug}",
        f"- Visual design: {visual_design}",
        f"- Projection: {projection}",
        "- Footprint: 1 by 1 tile",
        "- Directions: tile",
        f"- Default state: `{default_state}`",
        "",
        *common_sheet_contract(group, item_noun, False),
        "## States Or Variants To Generate",
        "",
        f"Generate **one {item_noun} for each listed state or variant**. There are {len(items)} item(s). Each image may contain at most **16 items** in a **4 by 4** grid.",
        "The first listed item is the default. Keep the style, scale, lighting angle, contact shadow strength, and palette consistent across every slot.",
        "",
    ]
    for note in extra_notes or []:
        lines.append(note)
    if extra_notes:
        lines.append("")

    for sheet_index, sheet in enumerate(sheets, start=1):
        cols, rows = grid_for_sheet(len(items), len(sheet))
        label = sheet_label(sheet_index, len(sheets))
        lines.extend([f"### {label}", ""])
        lines.append(f"Use a **{cols} by {rows}** grid for this sheet.")
        if len(items) > MAX_SLOTS_PER_SHEET and len(sheet) < MAX_SLOTS_PER_SHEET:
            lines.append("Leave unused cells empty.")
        lines.append("")
        for i, item in enumerate(sheet):
            lines.append(f"- {slot_name(i, cols)}: `{item['id']}` - {item['description']}")
        lines.append("")

    lines.extend(
        [
            *production_follow_up(group, item_noun),
            "## Prompt",
            "",
            (
                f"Generate Realm {display_name} image sheets. Use {generation}. "
                f"{create_each_sentence(item_noun, len(items), 'state or variant', 'states or variants')} "
                f"The default state is {default_state}. "
                f"{split_guidance_sentence(len(items), 'items')} "
                "Order items left to right and top to bottom within each sheet. "
                "Use clean readable small-RTS art, stable scale, clear gutters, no text labels, no numbers, no watermark, and no cropped artwork. "
                f"{sheet_prompt_background_sentence(group)}"
            ),
            "",
            "Slot order:",
        ]
    )
    append_slot_order(lines, sheets, len(items))
    lines.append("")
    return "\n".join(lines)


def ground_markdown(slug: str, display_name: str, visual_design: str, items: list[dict[str, str]]) -> str:
    return target_asset_markdown(
        "grounds",
        slug,
        display_name,
        visual_design,
        "top-down square source tile, projected into an isometric diamond by the game",
        "top-down square source tiles that tile cleanly at the edges and remain readable after isometric projection",
        "tile",
        items,
        [
            "Ground art must be a top-down square source tile. Do not generate isometric diamond source art.",
            "Each cell should be an edge-to-edge seamless tile sample, with no transparent border, drop shadow, grid line, or vignette.",
            "Keep detail broad enough for repeated tiling; avoid unique rocks, flowers, footprints, or landmarks unless that feature is the actual material state.",
            "Do not include upright objects, buildings, units, labels, or baked shadows from separate feature sprites.",
        ],
    )


def feature_markdown(slug: str, display_name: str, visual_design: str, items: list[dict[str, str]], concealing: bool) -> str:
    notes = [
        "Feature art must be a transparent anchored sprite. Do not include a full ground tile behind it.",
        "Use a small contact shadow only where it helps anchor the sprite to the tile.",
        "Use the same camera height and isometric lighting as unit and building sprites, not a flat icon view.",
        "Keep the bottom anchor stable: depletion, snow, rain, damage, or trample states should not shift the object across the tile.",
    ]
    if concealing:
        notes.extend(
            [
                "Concealing feature contract: define separate `back` and `front_occluder` layers so units can appear partly behind foliage or reeds.",
                "The `back` layer should contain trunks, rear foliage, stems, and contact details; the `front_occluder` layer should contain only the foreground coverage that can overlap units.",
            ]
        )
    return target_asset_markdown(
        "features",
        slug,
        display_name,
        visual_design,
        "transparent upright sprite anchored over a projected isometric map tile",
        "transparent-background anchored sprites with no full ground tile, consistent anchor position, and readable silhouette",
        "sprite",
        items,
        notes,
    )


def decal_markdown(slug: str, display_name: str, visual_design: str, items: list[dict[str, str]]) -> str:
    return target_asset_markdown(
        "decals",
        slug,
        display_name,
        visual_design,
        "transparent low or flat overlay that sits on top of ground",
        "transparent low/flat overlay decals that sit on the ground and do not imply an independent blocking object",
        "decal",
        items,
        [
            "Decal art must be transparent and sit on the ground.",
            "Use a top-down or very shallow map-overlay view, not an upright icon view.",
            "Keep the decal mostly inside the centre of the tile with soft edges so it can layer over many ground types.",
            "The decal must not imply an independent blocking object or a full terrain replacement.",
        ],
    )


def effects_ui_items() -> list[dict[str, str]]:
    return [
        state_item("melee_hit_spark", "upright_world melee hit spark"),
        state_item("arrow_hit", "upright_world arrow impact"),
        state_item("boulder_impact", "upright_world boulder dust impact"),
        state_item("boulder_water_splash", "upright_world boulder water splash"),
        state_item("building_hit_dust", "upright_world building hit dust"),
        state_item("rain_frame_1", "tile_overlay rain splash frame 1"),
        state_item("rain_frame_2", "tile_overlay rain splash frame 2"),
        state_item("storm_rain_frame_1", "tile_overlay storm rain frame 1"),
        state_item("storm_rain_frame_2", "tile_overlay storm rain frame 2"),
        state_item("snowfall_frame_1", "tile_overlay snowfall frame 1"),
        state_item("snowfall_frame_2", "tile_overlay snowfall frame 2"),
        state_item("move_marker", "tile_overlay move command marker"),
        state_item("attack_marker", "tile_overlay attack command marker"),
        state_item("gather_marker", "tile_overlay gather command marker"),
        state_item("build_marker", "tile_overlay build command marker"),
        state_item("rally_marker", "tile_overlay rally marker"),
        state_item("attack_move_marker", "tile_overlay attack-move marker"),
        state_item("hold_position_marker", "screen_ui hold-position marker"),
        state_item("selection_ring", "tile_overlay selection"),
        state_item("group_selection_ring", "tile_overlay group selection"),
        state_item("range_ring_dot", "tile_overlay range-ring dot"),
        state_item("build_preview_valid", "tile_overlay valid build preview"),
        state_item("build_preview_invalid", "tile_overlay invalid build preview"),
        state_item("wall_preview", "tile_overlay wall preview"),
        state_item("garrison_indicator", "screen_ui garrison indicator"),
        state_item("queued_unit_marker", "screen_ui queued unit marker"),
        state_item("research_active_marker", "screen_ui active research marker"),
        state_item("completed_research_icon_treatment", "screen_ui completed research icon treatment"),
    ]


def ammunition_markdown(spec: dict[str, Any]) -> str:
    return target_asset_markdown(
        "ammunition",
        spec["slug"],
        spec["name"],
        spec["description"],
        "transparent upright_world projectile sprite or tiny animation",
        "transparent ammunition sprites after release, separate from unit/building sheets",
        "ammunition sprite",
        spec["states"],
        [
            "Ammunition art is used only after release. Do not include the firing unit, launcher, building, target, impact burst, water splash, terrain, UI labels, or motion arrows.",
            "Keep the sprite compact, centred, readable over light and dark terrain, and suitable for animation between tiles.",
            "If the ammunition has multiple states, keep the same projectile identity while changing only the animated part such as flame flicker or volley spacing.",
        ],
    )


def effects_ui_markdown() -> str:
    return target_asset_markdown(
        "effects-ui",
        "effects-ui",
        "effects UI sprites",
        "clean readable small-RTS effects that remain legible over terrain and units",
        "mixed transparent overlays; each item declares tile_overlay, upright_world, or screen_ui",
        "transparent overlay sprites; do not include terrain, units, buildings, text labels, numbers, watermarks, or cropped artwork",
        "sprite",
        effects_ui_items(),
        [
            "Effects and UI items are separate transparent overlays, not terrain, unit, animal, or building sprites.",
            "tile_overlay items sit on the map, upright_world items face the camera, and screen_ui items are drawn in interface space.",
            "Use enough contrast and alpha separation that effects remain readable over grass, snow, water, lava, buildings, and units.",
            "Selection, range, preview, and command-marker items should align to the tile centre and avoid filled backgrounds.",
        ],
    )


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def export_prompts(out_dir: Path, clean: bool) -> dict[str, list[tuple[str, str]]]:
    realm_h = read_text(ROOT / "include" / "realm.h")
    entity_defs_cpp = read_text(ROOT / "src" / "core" / "entity_defs.cpp")
    audit_md = read_text(ROOT / "docs" / "tileset" / "realm_tileset_visual_audit.md")

    entity_order = enum_values(realm_h, "EntityType")
    stats = parse_stats(entity_defs_cpp)
    entity_audit, _terrain_audit = parse_audit_tables(audit_md)

    if clean and out_dir.exists():
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    index: dict[str, list[tuple[str, str]]] = {
        "grounds": [], "features": [], "decals": [],
        "units": [], "animals": [], "buildings": [], "ammunition": [], "effects-ui": [],
    }

    for slug, display_name, visual_design, items in ground_specs():
        rel = Path("grounds") / f"{slug}.md"
        write(out_dir / rel, ground_markdown(slug, display_name, visual_design, items))
        index["grounds"].append((display_name, rel.as_posix()))

    for slug, display_name, visual_design, items, concealing in feature_specs():
        rel = Path("features") / f"{slug}.md"
        write(out_dir / rel, feature_markdown(slug, display_name, visual_design, items, concealing))
        index["features"].append((display_name, rel.as_posix()))

    for slug, display_name, visual_design, items in decal_specs():
        rel = Path("decals") / f"{slug}.md"
        write(out_dir / rel, decal_markdown(slug, display_name, visual_design, items))
        index["decals"].append((display_name, rel.as_posix()))

    for enum_name in entity_order:
        category = category_for_entity(enum_name, entity_order)
        if not category:
            continue
        record = stats[enum_name]
        text = entity_markdown(enum_name, category, record, entity_audit.get(enum_name, {}))
        rel = Path(category) / f"{record['slug']}.md"
        write(out_dir / rel, text)
        index[category].append((record["name"], rel.as_posix()))

    for spec in AMMUNITION_SPECS:
        rel = Path("ammunition") / f"{spec['slug']}.md"
        write(out_dir / rel, ammunition_markdown(spec))
        index["ammunition"].append((spec["name"], rel.as_posix()))

    write(out_dir / "environment-state-design.md", environment_state_design_markdown())
    write(out_dir / "effects-ui" / "effects-ui.md", effects_ui_markdown())
    index["effects-ui"].append(("Effects UI", "effects-ui/effects-ui.md"))
    write(out_dir / "index.md", index_markdown(index))
    return index


def index_markdown(index: dict[str, list[tuple[str, str]]]) -> str:
    lines = [
        "# Realm Image Generation Prompts",
        "",
        "This folder is the complete Realm image-generation specification for the current visual asset architecture.",
        "",
        "## Generation Contract",
        "",
        "- Generate groups in this order: grounds, features, decals, units, animals, buildings, ammunition, effects-ui.",
        "- Generate one image sheet for each `Sheet` section in each prompt.",
        "- When a unit or animal prompt lists multiple directions, generate the full sheet set once per direction.",
        "- Treat these generated sheets as review contact sheets first; once a slot is accepted, generate or crop a standalone square production image for that slot.",
        "- For standalone production images, use transparent background where possible, or a single flat #ff00ff magenta key background for later cleanup.",
        "- Keep emoji, symbol, ASCII, and procedural fallbacks readable until replacement art exists.",
        "- Peasant idle is the only sprite lane assumed to already exist; every other prompt should be treated as needed art.",
        "- Ground prompts are top-down square tile art. Feature prompts are transparent anchored sprites. Decal prompts are transparent ground overlays. Ammunition and effects/UI prompts are transparent overlays.",
        "- Unit and building sheets may show ammunition only before release; released projectiles belong in the ammunition prompts.",
        "- Unit and animal `front` is a three-quarter screen-right RTS angle; `back` is the matching rear-right angle. Do not generate mirrored left-facing source art.",
        "- Do not add text labels, numbers, watermarks, cropped artwork, or baked UI chrome to generated image sheets.",
        "",
        "## Shared Design",
        "",
        "- [Environment state design](environment-state-design.md)",
        "",
    ]
    for group, entries in index.items():
        lines.extend([f"## {group.title()}", ""])
        for name, path in entries:
            lines.append(f"- [{name}]({path})")
        lines.append("")
    return "\n".join(lines)


def environment_state_design_markdown() -> str:
    return """# Realm Environment State Design

This prompt set treats seasons, weather, night, and depletion as visual states for image generation.

## Defaults

- The first listed state in each terrain prompt is the default.
- Snow and ice default to winter.
- Season-invariant terrain such as sand, dunes, lava, ash, stone, and mountains uses a `base_clear` default.
- Temperate ground and vegetation default to spring because it is the clearest non-extreme seasonal read.

## Terrain

- Terrain states are generated only where the tile material itself changes.
- Broad ambience, such as nighttime dimming or sparse precipitation, should remain a renderer overlay when possible.
- Rain and storm variants use two states where surface reaction matters: water, shallows, marsh, reeds, mud, road, dirt, buildings, and lava steam.
- Snow is both a terrain and an overlay idea. `T_SNOW` remains the fully snow-covered or snow-biome terrain; other terrain can still have light/heavy snow or frost variants.
- Winter conversion still matters: many ground tiles become `T_SNOW`, and water-like tiles can become `T_ICE`.

## Resources

- Resource depletion uses four visual levels: full, mostly full, mostly empty, depleted.
- Depletion should be interpreted as a ratio of the tile's starting resource amount, not an absolute number.
- Berries, wheat, oak forest, and pine forest use four seasons times four depletion levels, giving exactly 16 states.
- Gold, palms, dead trees, and fish use depletion states without seasonal cross-products.
- Depleted art should show the final visual before the runtime replacement terrain takes over.

## Buildings

- Buildings keep their structural states separate: complete, construction, damaged, garrisoned, training, ruin, and similar.
- Environment states are generated only for the completed building: night lit, rain frame 1, rain frame 2, light snow, and heavy snow.
- Night-lit building states should add warm torches, candles, forge light, window light, or lanterns. The renderer can still dim the whole scene for night.
- Do not generate every construction or damaged state in every season unless a later asset pass explicitly needs it.

## Animals

- Animals use four carcass states after death: dead unharvested, partly harvested, mostly harvested, and depleted skeleton.
- The depleted state for animals is always a skeleton.
- Equipment and durable objects should remain visible on military units and vehicles; animal carcasses should keep species silhouette readable.
"""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default=OUT_DEFAULT, help="output directory")
    parser.add_argument("--clean", action="store_true", help="remove existing output directory first")
    args = parser.parse_args()

    index = export_prompts((ROOT / args.out).resolve(), args.clean)
    print(
        "exported "
        + ", ".join(f"{len(entries)} {group}" for group, entries in index.items())
        + f" to {args.out}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
