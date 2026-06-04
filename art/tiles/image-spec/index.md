# Realm Image Generation Prompts

This folder is the complete Realm image-generation specification for the current visual asset architecture.

## Generation Contract

- Generate groups in this order: grounds, features, decals, units, animals, buildings, projectiles, effects, user_interface.
- Generate one image sheet for each `Sheet` section in each prompt.
- When a unit or animal prompt lists multiple directions, generate the full sheet set once per direction.
- Treat these generated sheets as review contact sheets first; once a slot is accepted, generate or crop a standalone square production image for that slot.
- For standalone production images, use transparent background where possible, or a single flat #ff00ff magenta key background for later cleanup.
- Keep emoji, symbol, ASCII, and procedural fallbacks readable until replacement art exists.
- Peasant idle is the only sprite lane assumed to already exist; every other prompt should be treated as needed art.
- Ground prompts are top-down square tile art. Feature prompts are transparent anchored sprites. Decal prompts are transparent ground overlays. Projectile, effect, and user-interface prompts are transparent overlays.
- User-supplied unit references under `art/reference/units/` are equipment and silhouette references only; generated unit sheets must be stylized Realm art, not copies of the reference image style or pixels.
- Unit and animal actor prompts use the generated JSON source-canvas resolution for accepted standalone frames. Current actor sprites are 48 by 48 px.
- Unit and animal actor prompts use the tiny medieval paper-cutout style. Human face rules appear only for unit prompts; animal face rules appear for animal prompts and Knight.
- Projectile prompts use the same moving paper-cutout treatment because projectiles move through the world.
- Building and decal prompts use simplified painted map-art, not paper cutouts.
- Ground and feature prompts use map-integrated hand-drawn watercolor styling. Feature and transition-like edges may fade to transparency, or toward #ff00ff magenta when alpha is unavailable.
- Unit and building sheets may show a projectile only before release; released projectiles belong in the projectile prompts.
- Unit and animal `front` is a three-quarter screen-right RTS angle; `back` is the matching rear-right angle. Do not generate mirrored left-facing source art.
- Do not add text labels, numbers, watermarks, cropped artwork, or baked UI chrome to generated image sheets.

## Shared Design

- [Environment state design](environment-state-design.md)

## Grounds

- [grass ground](grounds/grass.md)
- [meadow ground](grounds/meadow.md)
- [dirt ground](grounds/dirt.md)
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

- [road decal](decals/road.md)
- [flowers decal](decals/flowers.md)
- [tall grass decal](decals/tall_grass.md)
- [scuffs decal](decals/scuffs.md)
- [packed path decal](decals/packed_path.md)
- [cobble patch decal](decals/cobble_patch.md)
- [wheel ruts decal](decals/wheel_ruts.md)
- [yard clutter decal](decals/yard_clutter.md)
- [crates and barrels decal](decals/crates_barrels.md)
- [log piles decal](decals/log_piles.md)
- [farm tracks decal](decals/farm_tracks.md)
- [muddy footprints decal](decals/muddy_footprints.md)
- [snow trampled path decal](decals/snow_trampled_path.md)

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
- [Wooden Bridge](buildings/wooden_bridge.md)
- [Stone Bridge](buildings/stone_bridge.md)

## Projectiles

- [Arrow](projectiles/arrow.md)
- [Crossbow Bolt](projectiles/crossbow_bolt.md)
- [Flaming Arrow](projectiles/flaming_arrow.md)
- [Tower Bolt](projectiles/tower_bolt.md)
- [Warship Arrow Volley](projectiles/warship_arrow_volley.md)
- [Catapult Boulder](projectiles/catapult_boulder.md)
- [Trebuchet Boulder](projectiles/trebuchet_boulder.md)

## Effects

- [Melee Hit Spark](effects/melee_hit_spark.md)
- [Arrow Hit](effects/arrow_hit.md)
- [Boulder Impact](effects/boulder_impact.md)
- [Boulder Water Splash](effects/boulder_water_splash.md)
- [Building Hit Dust](effects/building_hit_dust.md)
- [Rain Frame 1](effects/rain_frame_1.md)
- [Rain Frame 2](effects/rain_frame_2.md)
- [Storm Rain Frame 1](effects/storm_rain_frame_1.md)
- [Storm Rain Frame 2](effects/storm_rain_frame_2.md)
- [Snowfall Frame 1](effects/snowfall_frame_1.md)
- [Snowfall Frame 2](effects/snowfall_frame_2.md)

## User_Interface

- [Move Marker](user_interface/move_marker.md)
- [Attack Marker](user_interface/attack_marker.md)
- [Gather Marker](user_interface/gather_marker.md)
- [Build Marker](user_interface/build_marker.md)
- [Rally Marker](user_interface/rally_marker.md)
- [Attack Move Marker](user_interface/attack_move_marker.md)
- [Hold Position Marker](user_interface/hold_position_marker.md)
- [Selection Ring](user_interface/selection_ring.md)
- [Group Selection Ring](user_interface/group_selection_ring.md)
- [Range Ring Dot](user_interface/range_ring_dot.md)
- [Build Preview Valid](user_interface/build_preview_valid.md)
- [Build Preview Invalid](user_interface/build_preview_invalid.md)
- [Wall Preview](user_interface/wall_preview.md)
- [Garrison Indicator](user_interface/garrison_indicator.md)
- [Queued Unit Marker](user_interface/queued_unit_marker.md)
- [Research Active Marker](user_interface/research_active_marker.md)
- [Completed Research Icon Treatment](user_interface/completed_research_icon_treatment.md)
