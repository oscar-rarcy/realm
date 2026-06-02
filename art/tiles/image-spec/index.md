# Realm Image Generation Prompts

This folder is the complete Realm image-generation specification for the current visual asset architecture.

## Generation Contract

- Generate groups in this order: grounds, features, decals, units, animals, buildings, ammunition, effects-ui.
- Generate one image sheet for each `Sheet` section in each prompt.
- When a unit or animal prompt lists multiple directions, generate the full sheet set once per direction.
- Treat these generated sheets as review contact sheets first; once a slot is accepted, generate or crop a standalone square production image for that slot.
- For standalone production images, use transparent background where possible, or a single flat #ff00ff magenta key background for later cleanup.
- Keep emoji, symbol, ASCII, and procedural fallbacks readable until replacement art exists.
- Peasant idle is the only sprite lane assumed to already exist; every other prompt should be treated as needed art.
- Ground prompts are top-down square tile art. Feature prompts are transparent anchored sprites. Decal prompts are transparent ground overlays. Ammunition and effects/UI prompts are transparent overlays.
- Unit and building sheets may show ammunition only before release; released projectiles belong in the ammunition prompts.
- Unit and animal `front` is a three-quarter screen-right RTS angle; `back` is the matching rear-right angle. Do not generate mirrored left-facing source art.
- Do not add text labels, numbers, watermarks, cropped artwork, or baked UI chrome to generated image sheets.

## Shared Design

- [Environment state design](environment-state-design.md)

## Grounds

- [grass ground](grounds/grass.md)
- [meadow ground](grounds/meadow.md)
- [dirt ground](grounds/dirt.md)
- [road ground](grounds/road.md)
- [mud ground](grounds/mud.md)
- [sand ground](grounds/sand.md)
- [dunes ground](grounds/dunes.md)
- [snow ground](grounds/snow.md)
- [tundra ground](grounds/tundra.md)
- [ice ground](grounds/ice.md)
- [water ground](grounds/water.md)
- [shallows ground](grounds/shallows.md)
- [marsh ground](grounds/marsh.md)
- [gravel ground](grounds/gravel.md)
- [ash ground](grounds/ash.md)
- [lava ground](grounds/lava.md)
- [hills ground](grounds/hills.md)
- [rocky ground](grounds/rocky.md)
- [castle floor ground](grounds/castle_floor.md)

## Features

- [forest feature](features/forest.md)
- [pine feature](features/pine.md)
- [palm feature](features/palm.md)
- [dead tree feature](features/dead_tree.md)
- [berry bush feature](features/berry_bush.md)
- [wheat crop feature](features/wheat_crop.md)
- [fish shoal feature](features/fish_shoal.md)
- [gold deposit feature](features/gold_deposit.md)
- [stone boulders feature](features/stone_boulders.md)
- [mountain peak feature](features/mountain_peak.md)
- [reeds feature](features/reeds.md)
- [ruins feature](features/ruins.md)
- [castle wall feature](features/castle_wall.md)
- [castle gate feature](features/castle_gate.md)

## Decals

- [flowers decal](decals/flowers.md)
- [tall grass decal](decals/tall_grass.md)
- [grass tufts decal](decals/grass_tufts.md)
- [small stones decal](decals/small_stones.md)
- [puddles decal](decals/puddles.md)
- [dirt scuffs decal](decals/dirt_scuffs.md)
- [packed path marks decal](decals/packed_path_marks.md)
- [cobble patches decal](decals/cobble_patches.md)
- [wheel ruts decal](decals/wheel_ruts.md)
- [yard clutter decal](decals/yard_clutter.md)
- [crates and barrels decal](decals/crates_barrels.md)
- [log piles decal](decals/log_piles.md)
- [farm tracks decal](decals/farm_tracks.md)
- [muddy footprints decal](decals/muddy_footprints.md)
- [snow-trampled path marks decal](decals/snow_trampled_path_marks.md)
- [ore bins decal](decals/ore_bins.md)
- [sacks decal](decals/sacks.md)
- [dock barrels decal](decals/dock_barrels.md)

## Units

- [Peasant](units/peasant.md)
- [Militia](units/militia.md)
- [Archer](units/archer.md)
- [Knight](units/knight.md)
- [Spearman](units/spearman.md)
- [Catapult](units/catapult.md)
- [Trebuchet](units/trebuchet.md)
- [Fishing Boat](units/fishing_boat.md)
- [Warship](units/warship.md)
- [Transport](units/transport.md)
- [Ram](units/ram.md)

## Animals

- [Deer](animals/deer.md)
- [Wolf](animals/wolf.md)
- [Sheep](animals/sheep.md)
- [Boar](animals/boar.md)

## Buildings

- [Town Hall](buildings/town_hall.md)
- [House](buildings/house.md)
- [Barracks](buildings/barracks.md)
- [Stable](buildings/stable.md)
- [Tower](buildings/tower.md)
- [Farm](buildings/farm.md)
- [Blacksmith](buildings/blacksmith.md)
- [Church](buildings/church.md)
- [Market](buildings/market.md)
- [Wall](buildings/wall.md)
- [Gate](buildings/gate.md)
- [Castle](buildings/castle.md)
- [Lumber Camp](buildings/lumber_camp.md)
- [Mining Camp](buildings/mining_camp.md)
- [Mill](buildings/mill.md)
- [Dock](buildings/dock.md)

## Ammunition

- [Arrow](ammunition/arrow.md)
- [Crossbow Bolt](ammunition/crossbow_bolt.md)
- [Flaming Arrow](ammunition/flaming_arrow.md)
- [Tower Bolt](ammunition/tower_bolt.md)
- [Warship Arrow Volley](ammunition/warship_arrow_volley.md)
- [Catapult Boulder](ammunition/catapult_boulder.md)
- [Trebuchet Boulder](ammunition/trebuchet_boulder.md)

## Effects-Ui

- [Effects UI](effects-ui/effects-ui.md)
