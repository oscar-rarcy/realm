# Realm tileset visual audit and art specification

This is a production-facing specification for a future tileset. It does **not** draw the tiles. It enumerates the actual entities, terrain, biome, season, weather, overlay, and UI concepts currently used by the uploaded game sources.

Audit sources used: `globals.cpp` entity stats table; `display.cpp` entity emoji mappings; `mapgen.cpp` map-generation terrain/biome distribution; `entity.cpp` construction, gathering, combat, winter, weather, paving, animals; `input.cpp` build/train menus and command behaviour; `render.cpp` ASCII terrain rendering; `gfx_renderer.cpp` tileset/isometric projection, terrain diamonds, entity glyph placement, and UI overlays; `include/realm.h` enum and entity state definitions.

Canonical visual layer architecture: `docs/tileset/realm_visual_asset_architecture.md`.

Related mechanics coverage audit: `docs/tileset/realm_animation_state_mechanics_audit.md`.

---

## 1. Core format

### Camera and tile geometry

Realm has two visual projections. ASCII mode presents the underlying map as an orthogonal square grid. Tileset/graphics mode projects those logical squares as **isometric diamonds** and draws units, resources, buildings, and other interactable objects as upright sprites over those diamonds. Source art should target the correct layer: ground terrain is authored as a top-down square source tile that the app projects into an isometric diamond, while features/entities are transparent upright sprites anchored over the logical tile centre.

Recommended production format:

| Asset type | Logical footprint | Sprite canvas | Anchor |
|---|---:|---:|---|
| Ground terrain | 1×1 map tile | top-down square source, projected in-app | whole logical tile |
| Terrain features/resources | 1×1 map tile | 48×48 px, transparent | upright sprite anchored over tile centre |
| Units/animals | 1×1 map tile | 48×48 px, transparent | upright sprite, stance anchor over tile centre |
| Ships/siege | 1×1 map tile | 48×48 px, transparent | upright sprite, stance anchor over tile centre; may visually overhang but footprint remains 1×1 |
| Buildings | source footprint, sliced by tile | 32×32 px per occupied cell | footprint top-left matches entity `x,y` |
| Projectiles/weather/selection | overlay | 32×32 px | centred on tile |

Use transparent pixels for sprite cutout only. Do not use transparency as the player-colour signal.

### Rendering layer order

Use this layer order so terrain, seasons, player colours, and overlays do not fight each other:

1. Base ground tile, authored top-down and projected/skewed into the isometric diamond.
2. Terrain feature/resource sprite or decal: trees, berries, gold, fish, wheat, ruins, reeds, etc.
3. Terrain transition masks: edges, corners, shoreline, path borders.
4. Seasonal overlay: spring growth, summer dryness, autumn foliage, winter snow/ice, thaw.
5. Weather mutation overlay: mud, wet sheen, frost, storm-darkening.
6. Building ground halo: dirt/cobbles/yard merge around completed buildings.
7. Entity/building shadow.
8. Entity/building neutral base sprite.
9. Player-colour mask layer for owned units/buildings/ships.
10. Action/effect layer: projectiles, impact, alert mark, construction scaffold pulse.
11. Selection/cursor/range/wall-preview overlays.
12. Fog-of-war / unexplored overlay.

---

## 2. Player-colour system

### Required approach

Use a **two-layer team-colour mask**, not zero alpha.

For every player-owned unit, building, ship, and siege engine:

- `base`: neutral greys, browns, blacks, leather, iron, stone, wood, cloth shadows.
- `team_mask`: greyscale alpha or white mask showing where player colour is applied.
- Optional `team_shade`: grayscale multiplier for folds, bevels, shield curvature, banner shadows.

Formula concept:

```text
final_rgb = base_rgb * (1 - mask_alpha) + (player_rgb * shade) * mask_alpha
```

A single-sheet fallback is acceptable only if the engine cannot handle mask sprites. In that case, use **preview cyan / mask key `#00AFFF`**, then remap only exact key-colour pixels. Avoid using that exact colour elsewhere in unit/building art.

### Preview and player colours

Current terminal player identity colours are effectively:

| Player | Current role | Recommended preview RGB | Use in art preview |
|---:|---|---:|---|
| P0 | human player | `#00AFFF` cyan | default preview colour |
| P1 | enemy | `#FF0000` red | enemy preview |
| P2 | enemy | `#D78700` orange | third-player preview |
| P3 | enemy | `#5F005F` purple | fourth-player preview |

Night variants should be generated in-engine by dimming/desaturating the final player colour, not by drawing separate unit sheets.

### Where team colour belongs

Units should remain mostly neutral. Put team colour only on high-readability, non-essential areas:

- Peasant: tunic strip, belt sash, small shoulder cloth.
- Militia: shield face, tabard stripe, helmet plume.
- Archer: hood trim, quiver wrap, small shoulder sash.
- Knight: shield, caparison, lance pennant, saddle cloth.
- Catapult/ram: small pennants, shield plaques, crew cloth, wheel hub mark.
- Ships: sail panels, pennants, stern flag; hull stays brown/black.
- Buildings: flags, awnings, banners, door cloths, shield signs, roof trim. Avoid colouring the whole building body.

Neutral animals and neutral terrain objects must not use team masks.

---

## 3. Actual renderable entities

There are 29 renderable entity types plus `E_NONE`. All player-owned units/buildings require a team-colour mask. Nature-owned animals do not.

### 3.1 Player units and vehicles

| Enum | Name | Footprint | Source role | Required visual design | Team-colour slots | Required states |
|---|---|---:|---|---|---|---|
| `E_PEASANT` | Peasant | 1×1 | worker, builder, gatherer, basic attacker | Hooded medieval labourer with tunic, belt, simple tool; neutral cloth/leather | tunic strip, belt sash, shoulder cloth | idle, walk, mine gold, chop wood, pick berries, build/scaffold hammering, tend farm, carry gold, carry wood, carry food, weak attack, dead, decayed skeleton |
| `E_MILITIA` | Militia | 1×1 | melee infantry | Spearman/swordsman with round shield, simple helmet, rough gambeson | shield face, tabard stripe, plume | idle, walk, attack swing/thrust, hold-position, hit/alert, dead, decayed skeleton with armour and weapons |
| `E_ARCHER` | Archer | 1×1 | ranged infantry; Crossbows research changes range | Bow/crossbow-ready archer with hood and quiver | hood trim, quiver wrap, shoulder sash | idle, walk, aim, release, reload; optional upgraded crossbow variant, dead, decayed skeleton with bow and quiver |
| `E_KNIGHT` | Knight | 1×1 | fast heavy cavalry | Armoured rider on horse; should read as cavalry even at small size | shield, saddle cloth, pennant | idle, trot, charge/strike, hit/alert, dead, decayed skeleton with armour, horse gear, and weapon |
| `E_CATAPULT` | Catapult | 1×1 logical; may overhang | siege ranged, splash, standoff | Wood-and-iron torsion/sling machine; exaggerated throwing arm | small pennants, side shield plaques, crew cloth | idle, roll, load, fire, recoil, damaged/alert, destroyed wreck, decayed wreckage |
| `E_RAM` | Ram | 1×1 logical; may overhang | building-only siege | Covered battering ram with log snout, wheels, dark hide/wood cover | side shield plaques, pennant, cloth trim | idle, roll, impact/ramming, damaged/alert, destroyed wreck, decayed wreckage |
| `E_FISHING_BOAT` | Fishing Boat | 1×1 water | gathers fish, returns to dock | Small brown skiff/canoe with net or fish line | tiny flag or sail patch | idle, row/sail, fish/net cast, carrying fish, destroyed wreck, decayed wreckage |
| `E_WARSHIP` | Warship | 1×1 water | naval combat | Larger longboat/war galley silhouette, shielded sides | sail stripe, shields, pennant | idle, sail, fire/arrow volley, damaged/alert, destroyed wreck, decayed wreckage |
| `E_TRANSPORT` | Transport | 1×1 water | carries garrisoned units | Broad ferry/barge, visible cargo deck | sail/pennant, side banner | idle, sail, load/unload, cargo full indicator, destroyed wreck, decayed wreckage |

Minimum animation recommendation for all mobile entities: four facing directions (`N`, `E`, `S`, `W`) with `E` mirrorable to `W` if needed; 4 idle frames; 6 walk/sail frames; 6 attack/work frames. The current game does not store facing explicitly, so the initial engine can display a single canonical direction, but the art should be prepared for direction inference from motion/target deltas.

### 3.2 Buildings

Buildings use multi-tile footprints from the stats table. The future tileset should not draw them as one opaque rectangle. Each building should be a **sliced footprint prefab** whose occupied cells stack visually into a coherent structure.

| Enum | Name | Footprint | Role in game | Required visual design | Team-colour slots | Required states |
|---|---|---:|---|---|---|---|
| `E_TOWNHALL` | Town Hall | 3×3 | starting base, depot, supply +10, garrison 6, vision, AI can train peasants from it | Clustered civic compound: central hall/keep, side annexes, courtyard, doorway facing south | large banner, door cloth, roof trim, town shield sign | complete, under construction 0/33/66%, garrisoned, damaged, ruin footprint |
| `E_HOUSE` | House | 2×2 | supply +5, garrison 4, defensive if garrisoned | Thatch/wood cottage, chimney, small fenced yard | small pennant, door cloth | complete, construction, garrisoned, ruin footprint |
| `E_BARRACKS` | Barracks | 3×2 | trains militia, archer, catapult, ram | Long timber hall with weapon racks and practice yard | flags, shield sign, awning | complete, training, construction, ruin footprint |
| `E_STABLE` | Stable | 3×2 | trains knight | Long stable block, hay, paddock fence, horse posts | stable banner, saddle blankets | complete, training, construction, ruin footprint |
| `E_TOWER` | Tower | 1×1 | defensive ranged, garrison 3, extra vision | Stone watchtower, battlements, torch/readable height | flag or shield plaque | complete, firing, garrisoned, construction, damaged |
| `E_FARM` | Farm | 1×1 | food generator when tended; dies in winter | Cultivated wheat plot, furrows, scarecrow/sickle marker | tiny post flag optional, very subtle | sowing, growing, ripe, tended, winter-dead/snowed, depleted/dead |
| `E_BLACKSMITH` | Blacksmith | 2×2 | research Iron Weapons/Crossbows; speeds training | Forge, chimney, anvil yard, glowing coals | hanging sign, awning stripe | complete, researching, construction, ruin footprint |
| `E_CHURCH` | Church | 2×2 | heals nearby, vision | Small stone chapel, cross, stained glass, grave/path | banner/door cloth only | complete, healing aura, construction, ruin footprint |
| `E_MARKET` | Market | 2×2 | passive gold income | Trade stalls, awnings, crates, coin sign | awning stripes and flags | complete, income sparkle, construction, ruin footprint |
| `E_WALL` | Wall | 1×1 | drag-built blocker | Stone wall block that joins cleanly to neighbours | tiny banner only at intervals, not every tile | straight, corner, T-junction, cross, end-cap, construction |
| `E_GATE` | Gate | 1×1 | passable when open; auto/locked state | Compact portcullis/gatehouse readable in any wall direction | small pennant/shield | closed, open, locked-open, locked-closed, construction |
| `E_CASTLE` | Castle | 4×4 | major base, supply +15, garrison 10, vision, defensive base | Multi-tower stone fortress with inner keep and courtyard | flags on towers, banners, shield plaques | complete, garrisoned, damaged, construction, ruin footprint |
| `E_LUMBER_CAMP` | Lumber Camp | 2×2 | wood drop-off | Log piles, saw frame, chopping block, shed | small camp banner | complete, active/deposit, construction, ruin footprint |
| `E_MINING_CAMP` | Mining Camp | 2×2 | gold drop-off | Mine hut, timber supports, ore carts, lantern | small banner, cart flag | complete, active/deposit, construction, ruin footprint |
| `E_MILL` | Mill | 2×2 | food drop-off and enables farm harvest | Windmill or waterless hand-mill with grain sacks | sail/cloth accents, door banner | complete, operating, construction, ruin footprint |
| `E_DOCK` | Dock | 2×2 | fish drop-off; trains naval units; must touch water | Wooden pier/boathouse that has land and water-facing pieces | flag, sail cloth, awning | complete, training, construction, shoreline variants, ruin footprint |

#### Building slicing rules

- Every building footprint should have a transparent outer margin so adjacent terrain remains visible.
- Use a shared, mergeable **settlement halo** layer: dirt/cobble/path/crates around completed buildings. Halos should merge when buildings are adjacent, matching the game’s path-wear/building-creep behaviour.
- Building shadows should fall consistently down/right and should not fill the entire tile rectangle.
- Large destroyed buildings currently become `T_RUINS`; therefore every building with footprint area ≥4 should have a corresponding ruin-ground footprint. Farm, wall, gate, and tower can use small rubble/dead variants but do not need persistent large ruins.

#### Town Hall modular 3×3 layout

Use this 3×3 plan so it reads as “a group of buildings together” while still occupying one logical Town Hall entity:

```text
NW: side annex roof       N: rear roof / hall ridge     NE: chimney + small annex
W:  side wall + crates    C: main hall/tower/flag       E: workshop/store wing
SW: courtyard / path      S: main entrance steps        SE: carts / notice board
```

This lets multiple civic buildings visually stack into a settlement rather than appear as isolated icons.

#### Castle modular 4×4 layout

```text
corner towers at all corners
outer wall segments on all edges
2×2 inner keep/courtyard core
south edge has gate/entry emphasis
```

The player-colour mask should be concentrated on flags/banners over towers, not on stone walls.

### 3.3 Nature animals

| Enum | Name | Footprint | Behaviour in game | Required visual design | Required states |
|---|---|---:|---|---|---|
| `E_DEER` | Deer | 1×1 | spawns in herds; flees in herd when units approach; food on kill | Brown/tan deer, clear antlers or slim silhouette | idle/graze, walk, flee, dead, decayed skeleton |
| `E_WOLF` | Wolf | 1×1 | hunts units; bolder in winter; avoids settlements outside winter | Grey wolf, low predatory silhouette | idle, prowl, attack, winter-aggressive variant optional, dead, decayed skeleton |
| `E_SHEEP` | Sheep | 1×1 | domestic sheep near bases; flees individually; food on kill | White/off-white sheep, rounded silhouette | idle/graze, walk, flee, dead, decayed skeleton |
| `E_BOAR` | Boar | 1×1 | charges nearby units, rampages after hit; food on kill | Dark brown boar, tusks, heavy body | idle, walk, charge, attack, dead, decayed skeleton |

Animals are Gaia/nature-owned and must not have team-colour slots.

---

## 4. Actual terrain and nature tile manifest

The game has 33 terrain IDs. The UI names are shown here because they are the player-facing names.

Recommended variant scheme:

- **Continuous ground materials**: 12 seamless centre variants plus a reusable 47-mask blob autotile set for edges. Seasons should be overlays unless the material’s identity changes.
- **Object/resource decals**: 12 object variants on transparent background, plus optional density states if the engine later wants resource amount feedback.
- **Animated materials**: use frame counts matching current code where possible: water 6, shallows 4, marsh 3, reeds 4, gold shimmer 8, lava 6, fish 2.
- **Winter severity**: do not draw full separate winter sheets for everything. Use mild winter terrain plus snow/frost overlays, with opacity/coverage driven by season severity.

| Terrain enum | UI name | Type | Visual design | Required variants / animation | Runtime notes |
|---|---|---|---|---|---|
| `T_GRASS` | Grassland | continuous ground | Default temperate grass, uneven small blades, no strong pattern | 12 base variants; spring, summer dry, autumn dull, winter mild overlay compatibility | Can wear into dirt/road; winter converts/overlays to snow |
| `T_TALL_GRASS` | Tall Grass | ground + vegetation | Taller clumps, swaying stems, good concealment feel | 8 clump variants; 4 sway frames; seasonal colour overlays | Can become mud/dirt/road; winter snowed |
| `T_FLOWERS` | Wildflowers | decorative ground | Grass with lavender/blue/yellow/red flower clusters | 12 variants split across 4 flower colours; spring bloom overlay; autumn dead-stalk variant | Fades in autumn; winter snowed |
| `T_MEADOW` | Meadow | continuous ground | Lusher open green, sparse small flowers | 12 base variants; seasonal overlays | Can host farms; winter snowed |
| `T_FOREST` | Oak Forest | resource object | Deciduous tree canopy, trunks partly visible; oak/leafy mixed shapes | 16 tree variants; forest edge masks; autumn early/mid/late leaves; winter branch snow | Gatherable wood; depleted to dirt |
| `T_PINE` | Pine Forest | resource object | Dark conifer silhouettes, triangular clusters | 12 pine variants; snow-cap overlay; autumn slight yellowing | Gatherable wood; depleted to dirt |
| `T_PALM` | Palm Grove | resource object | Desert/coastal palms, tan trunks, green crowns | 8 palm variants; sand/root shadow variants | Gatherable wood; depleted to dirt |
| `T_DEAD_TREE` | Dead Tree | resource object | Swamp dead trunks, grey/brown limbs, moss | 8 variants, including stump/fallen limb | Gatherable wood; depleted to dirt |
| `T_MOUNTAIN` | Mountain | blocker terrain | Tall grey rock, high contrast peak, dark base | 12 variants; cliff-edge masks; snowcap optional | Blocks land; not resource |
| `T_HILLS` | Rolling Hills | ground terrain | Low brown/olive slopes, curved contour highlights | 8 variants; snow overlay | Slows/visual variation; can be snowed in winter overlay |
| `T_STONE` | Stone | blocker/resource-looking terrain | Rock piles/boulders, grey with hard silhouette | 8 boulder variants | Blocks land; currently not gathered as a resource |
| `T_WATER` | Deep Water | animated liquid | Dark blue deep water with wave bands | 6-loop animation; 12 shoreline masks; night/dawn tint | Blocks land; passable to boats; winter can freeze to ice |
| `T_SHALLOWS` | Shallows | animated liquid/shore | Teal water, visible sand/riverbed | 4-loop animation; shoreline blend masks | Land can wade slowly; boats pass; winter can freeze |
| `T_MARSH` | Marshland | animated wet ground | Green-brown wet pools, soft mud, algae | 3-loop subtle ripple variants; grass/reed edge masks | Land passable but slow; boats blocked; rain may affect movement |
| `T_REEDS` | Reed Bed | animated vegetation/water | Vertical reed strokes, wet ground below | 4 sway frames; 8 clump variants | Land and boats can pass; winter can freeze visually |
| `T_GOLD` | Gold Deposit | resource object | Bright ore vein/nuggets in rock/dirt | 8 shimmer frames; 10 vein variants | Gatherable gold; depleted to dirt |
| `T_SAND` | Sandy Ground | continuous ground | Soft tan desert/coast sand, wind grain | 12 variants; dune transition masks; wet edge for shoreline | Slow movement; weather can slow further |
| `T_DUNES` | Sand Dunes | ground relief | Curved dune ridges, light highlights | 8 ridge variants; 4 orientation variants | Slow/terrain identity for desert |
| `T_SNOW` | Snow Cover | continuous ground | Snow blanket with subtle blue/grey dents | 12 variants; light/heavy/deep overlay compatibility; thaw slush overlay | Main winter-converted terrain; tundra base in snow biome |
| `T_ICE` | Frozen Ice | continuous ground/liquid replacement | Blue-white ice, cracks, slick reflections | 8 crack variants; thin/solid ice variants; shore masks | Winter replacement for water/shallows/marsh/reeds; passable to land, blocks boats |
| `T_DIRT` | Bare Earth | continuous ground | Worn brown dirt, trampled grass edges | 12 variants; path-edge masks; wet version | Created by traffic, buildings, resource depletion; can regrow grass |
| `T_ROAD` | Stone Road | path terrain | Packed road/cobble line, readable as movement route | 47 path autotiles; straight, corner, T, cross, ends; 6 worn variants | Created by heavy traffic; decays to dirt |
| `T_MUD` | Mud | weather terrain | Wet dark brown/green mud, puddles, boot marks | 8 variants; wet sheen overlay | Created by rain/storm; dries to dirt |
| `T_WHEAT` | Wheat Field | resource/field terrain | Standing golden crop, rows/furrows | 8 growth variants; summer ripe gold; autumn spent; winter snow/dead | Can be turned into farm by peasant; hides enemies in current render logic |
| `T_BERRY` | Berry Bush | resource object | Bushes with red/purple berries on green base | 10 bush variants; depleted/empty optional | Gatherable food; depleted to grass |
| `T_FISH` | Fish Shoal | water resource | Fish splash/ripple on water/shallows | 2 fish/shoal frames plus water ripple | Gatherable by fishing boat; depleted to water; blocks land |
| `T_RUINS` | Ancient Ruins | terrain object/ground | Broken stone, rubble, old foundations | 12 rubble variants; large-building ruin footprints | Created by mapgen and destroyed large buildings |
| `T_GRAVEL` | Gravel | ground terrain | Grey pebble field, sparse stones | 8 variants; winter snow overlay | Used in desert/volcanic and winter overlay targets |
| `T_LAVA` | Lava Fissure | animated blocker | Red/orange fissures, glowing cracks, hot surface | 6-loop animation; hot shimmer frames; ash/stone edges | Blocks land; volcanic biome |
| `T_ASH` | Volcanic Ash | continuous ground | Dark near-black ash, grey speckle | 12 variants; ember flecks optional | Slow movement; volcanic biome base |
| `T_CASTLE_WALL` | Castle Wall | ancient map ruin | Static ruined fortress wall, darker than player wall | 16 wall-join variants, ruined/broken alternatives | Terrain from mapgen; separate from player `E_WALL` |
| `T_CASTLE_FLOOR` | Castle Floor | ancient map ruin | Brown/grey old paving inside castle ruins | 12 variants; dirt/grass invasion overlays | Terrain from mapgen; faster movement like paved ground |
| `T_CASTLE_GATE` | Castle Gate | ancient map ruin | Old gate threshold/arch base | open ruined gate variants | Terrain from mapgen; separate from player `E_GATE` |

---

## 5. Biome design

The splash screen supports seven biome choices: Temperate, Desert, Snow/Tundra, Swamp, Forest/Woodland, Volcanic, and Ocean/Coastal. Random maps combine biome patches.

| Biome enum | Player-facing name | Primary terrain identity | Palette direction | Distinctive assets |
|---|---|---|---|---|
| `B_TEMPERATE` | Temperate | grass, tall grass, flowers, meadow, oak forest | saturated green, spring flowers, summer/dry/autumn transitions | grass/meadow/flower variants, oak forest, wheat patches |
| `B_DESERT` | Desert | sand, dunes, gravel, palms | tan, amber, pale dune highlights | palm groves, dune ridges, heat-dry ground |
| `B_SNOW` | Snow/Tundra | snow, pine, stone | white/blue/grey, but thaws in summer | tundra thaw grass, snow-pine, ice cracks |
| `B_SWAMP` | Swamp | marsh, reeds, shallows, dead trees | dark green, algae, wet browns | reeds, dead tree, marsh ripples, mud |
| `B_FOREST` | Forest/Woodland | oak forest, pine, berries, grass | deep canopy greens with seasonal canopy change | dense tree edge masks, berry clumps, leaf litter |
| `B_VOLCANIC` | Volcanic | lava, stone, mountain, gravel, gold, ash | black/grey ash, orange/red lava, bright gold | lava fissures, ash plains, rich ore veins |
| `B_OCEAN` | Ocean/Coastal | water, shallows, sand, reeds, islands | deep blue, teal shallows, tan island edges | shoreline masks, fish shoals, coastal forest/palms optional |

Biome-specific base-tint variants should come from palette overlays where possible, not from duplicating every terrain tile.

---

## 6. Seasons and winter severity

The game has four seasons: Spring, Summer, Autumn, Winter. Winter also has material conversions and severity effects. Design seasons as a **base seasonal look plus overlay layers**.

### 6.1 Base seasonal appearances

| Season | Visual target | Affected assets |
|---|---|---|
| Spring | Fresh greens, thaw, scattered flowers, wet recovering ground | grass, meadow, forest, flowers, snow thaw, river edges |
| Summer | Strong greens in forests, drier yellow-green patches in grass, ripe wheat | grass, forest, wheat, meadow |
| Autumn | Yellow/orange/red foliage, brown grass, dead flowers, first frost late | oak forest, grass/tall grass/meadow, flowers, wheat, pine slight yellowing |
| Winter default/mild | Patchy snow, frosted grass, snow on trees, frozen water patches | grass family, forest, pine, water, marsh, reeds, hills, gravel |

### 6.2 Winter severity overlays

Use these overlays so strong winters can be shown by opacity/coverage without separate terrain sheets for every severity:

| Overlay ID | Purpose | Suggested use |
|---|---|---|
| `season_frost_dust_01-04` | late autumn / very mild winter frost | low-opacity white edge specks; apply to grass/dirt/flowers/meadow |
| `season_snow_patch_01-08` | mild winter patchy snow | 25–45% opacity/coverage; lets ground show through |
| `season_snow_blanket_01-08` | normal winter | 50–80% coverage; current render never needs perfectly flat white except native snow |
| `season_snow_deep_01-06` | strong winter overlay | add over mild/normal snow at 35–75% opacity; softens terrain detail |
| `season_tree_snow_deciduous_01-06` | snow caught in oak branches | overlay on `T_FOREST` while preserving trunk/canopy shape |
| `season_tree_snow_pine_01-06` | snow on pine boughs | overlay on `T_PINE` |
| `season_ice_thin_01-06` | early freeze or thawing water | apply to water/shallows/marsh/reeds at low coverage |
| `season_ice_solid_01-08` | strong winter frozen water | apply as replacement/overlay to `T_ICE` |
| `season_thaw_slush_01-06` | late winter / early spring | dirty snow edges, puddles, grass showing through |

Implementation recommendation:

- Default winter = `season_snow_patch` plus selective tree snow.
- Strong winter = default winter plus `season_snow_deep` or higher overlay opacity.
- Water severity = `season_ice_thin` → `season_ice_solid`; in thaw, blend back through `season_thaw_slush` and open-water masks.
- Tundra/snow biome in summer should not remain pure winter white. Use `snow_tundra_thaw` variants with grass and bare ground showing through.

### 6.3 Terrain transformation facts to support

The current simulation changes terrain, so the tileset must include these states:

- Winter converts grass/tall grass/flowers/meadow/dirt/road/gravel/ruins/sand/dunes/wheat/berry/mud/castle floor to `T_SNOW`.
- Winter converts water/shallows/marsh/reeds to `T_ICE`.
- Spring thaw restores pre-winter terrain gradually.
- Rain and storms turn grass/meadow/dirt/tall grass into `T_MUD`.
- Clear weather dries `T_MUD` to `T_DIRT`.
- Traffic turns natural ground into `T_DIRT`, then `T_ROAD`.
- Buildings create dirt haloes around them over time.
- Unused roads decay to dirt; dirt can regrow grass.
- Gathered resources deplete into replacement terrain: fish → water, berries → grass, wood/gold → dirt.
- Destroyed large buildings leave `T_RUINS`.

---

## 7. Weather, projectiles, and tactical overlays

### Weather

| Weather enum | Visual asset | Notes |
|---|---|---|
| `W_CLEAR` | none | normal seasonal terrain |
| `W_RAIN` | sparse blue-grey rain dots/streaks, wet darkening | also creates mud and slows natural-ground movement |
| `W_STORM` | denser rain, darker ambience, optional lightning flash | also conceals enemies like night and creates more mud |
| `W_SNOW` | sparse white snowflakes | late autumn/winter only; does not create mud |

Weather overlays must be transparent and should never cover units/buildings heavily. Current render uses very sparse precipitation.

### Terrain, weather, night, and depletion state model

Image-generation prompts should treat seasons, weather reactions, night lighting, and resource depletion as **visual states**. They are not runtime paths or asset manifests.

Defaults:

- The first state in each generated terrain prompt is the default state.
- Snow and ice default to winter.
- Season-invariant materials such as sand, dunes, lava, ash, stone, and mountains use a clear/base state as their default.
- Temperate ground and vegetation default to spring because it is the most neutral readable non-extreme season.

Terrain policy:

- Generate explicit terrain states only when the material itself changes: seasonal colour, frost/snow cover, wetness/splashes, thaw, or resource amount.
- Keep broad ambience as renderer work where possible: global night dimming, sparse precipitation, and general storm darkness should not require a separate tile for every terrain.
- Rain and storm variants should be two-state mini animations only on surfaces where the surface visibly reacts: water, shallows, marsh, reeds, mud, road, dirt, lava steam, and completed buildings.
- Keep `T_SNOW` as the full snow-cover or snow-biome terrain. Other terrain may still have light/heavy snow, frost, or slush states.
- Keep `T_ICE` as the frozen-water replacement terrain. Water-like terrain may still have thin-ice and thaw-edge states.

Resource depletion:

- Use four visual depletion levels: full, mostly full, mostly empty, depleted.
- Depletion is a ratio of that tile's starting resource amount, not an absolute count.
- Berries, wheat, oak forest, and pine forest use 4 seasons × 4 depletion levels = 16 prompt states.
- Gold, palms, dead trees, and fish use depletion states without season cross-products unless later gameplay proves the extra states are needed.
- The depleted state should show the last readable depleted form before the runtime replacement terrain takes over.

Buildings:

- Keep structural states separate: complete, construction, damaged, garrisoned, training, ruin, and similar.
- Generate environment states only for the completed building: night-lit, rain frame 1, rain frame 2, light snow, heavy snow.
- Night-lit building art should add warm torches, candles, forge light, window light, or lanterns. Do not rely on a generated dark tint alone; the renderer can dim the scene globally.
- Do not generate every construction/damaged/garrisoned state in every season unless a later pass specifically needs it.

Animals:

- Animals use four post-death carcass/depletion states: dead unharvested, partly harvested, mostly harvested, depleted skeleton.
- The final animal depletion state is always a skeleton.
- Military units and vehicles keep the two death states already defined: dead/destroyed and decayed skeleton/wreckage with armour, weapons, bows, shields, tools, wood, metal, wheels, hulls, or siege parts still readable.

### Projectiles and combat effects

| Effect | Current source behaviour | Required art |
|---|---|---|
| Arrow | archer projectile | small arrow/bolt in 4 directions; gold/brown streak |
| Tower bolt | tower/garrison projectile | brighter bolt/tracer; separate from normal arrow |
| Catapult boulder | catapult projectile | stone boulder arc/dot; impact dust/splash |
| Alert marker | entity recently hit/attacking pulses as `!` | small red/orange exclamation or flash ring |
| Catapult splash | damages 1-tile radius around impact | dust ring / rubble burst overlay |

### Tactical/UI map overlays

| Overlay | Required visual design |
|---|---|
| Cursor | gold square/outline that reads on snow, water, grass, and dark terrain |
| Selection | reversible/bright outline; should work on 1×1 units and multi-cell buildings |
| Group selection | small base ring or corner brackets for each selected unit |
| Drag selection box | bright border overlay, not terrain-replacing |
| Range ring | subtle dotted ring; does not cover occupied tiles |
| Wall drag preview | translucent wall blocks along Bresenham line; team-colour tinted |
| Fog unexplored | black/empty or very dark mask |
| Fog explored | dark desaturated terrain overlay |
| Night/dawn/dusk | whole-scene tint layer; avoid redrawing separate unit sheets |
| HP bar | green/yellow/red UI bar, not part of entity sprite |
| Training/research progress | UI progress bars and queue icons |
| Minimap dots | player, enemy, animal, water, forest, gold, sand, snow, mountain, castle colours |

---

## 8. Build/train menu audit

The build and train options actually exposed by input should drive icon production.

### Buildable from Peasant

Player build menu includes:

`House`, `Barracks`, `Stable`, `Tower`, `Farm`, `Wall`, `Gate`, `Blacksmith`, `Church`, `Market`, `Castle`, `Lumber Camp`, `Mining Camp`, `Mill`, `Dock`.

`Town Hall` exists, spawns at game start, and AI can build forward town halls, but it is not currently in the player build menu.

### Trainable from buildings

| Producer | Units shown in player train menu |
|---|---|
| Town Hall | Peasant |
| Barracks | Militia, Archer, Catapult, Ram |
| Stable | Knight |
| Dock | Fishing Boat, Warship, Transport |

The Blacksmith has research icons for:

- `Iron Weapons`: militia/knight attack upgrade.
- `Crossbows`: archer range upgrade.

These need UI icons even if they do not require full alternate unit sprites.

---

## 9. Exact asset list by sheet

### `terrain_ground.png`

Required base ground/material tiles:

- `grass`, `tall_grass`, `flowers`, `meadow`
- `sand`, `dunes`, `snow`, `ice`, `dirt`, `road`, `mud`, `gravel`, `ash`
- `hills`, `mountain`, `stone`
- `water`, `shallows`, `marsh`, `reeds`, `lava`
- `castle_floor`, `castle_wall`, `castle_gate`, `ruins`

### `terrain_resources.png`

Required resource/object tiles:

- `forest_oak`, `pine`, `palm`, `dead_tree`
- `gold_deposit`
- `wheat_field`
- `berry_bush`
- `fish_shoal`

### `terrain_seasons.png`

Required overlays:

- `spring_new_growth`, `spring_flower_extra`
- `summer_dry_grass`, `summer_ripe_wheat`
- `autumn_grass`, `autumn_leaf_early`, `autumn_leaf_mid`, `autumn_leaf_late`, `autumn_dead_flower`, `autumn_frost_first`
- `winter_snow_patch`, `winter_snow_blanket`, `winter_snow_deep`, `winter_tree_snow_oak`, `winter_tree_snow_pine`, `winter_ice_thin`, `winter_ice_solid`, `spring_thaw_slush`

### `buildings.png` + `buildings_teammask.png`

Required building prefabs:

- Town Hall 3×3
- House 2×2
- Barracks 3×2
- Stable 3×2
- Tower 1×1
- Farm 1×1
- Blacksmith 2×2
- Church 2×2
- Market 2×2
- Wall 1×1 autotile/join set
- Gate 1×1 states
- Castle 4×4
- Lumber Camp 2×2
- Mining Camp 2×2
- Mill 2×2
- Dock 2×2 shore variants

For each: `complete`, `construction_0`, `construction_1`, `construction_2`, `damaged`, and `snowcap_light/heavy` overlay. For area ≥4, include ruin footprint.

### `units.png` + `units_teammask.png`

Required player-owned unit sprites:

- Peasant
- Militia
- Archer
- Knight
- Catapult
- Ram
- Fishing Boat
- Warship
- Transport

Each should include idle, move, action/attack/work, and hit/alert frames. Peasant requires the most state variety because it visually represents almost every economy action.

### `animals.png`

Required Gaia sprites:

- Deer
- Wolf
- Sheep
- Boar

Include idle, move/flee/prowl, and attack where applicable. No team mask.

### `effects_ui.png`

Required overlays and UI sprites:

- Rain, storm rain, snowflake
- Arrow, tower bolt, boulder, boulder impact
- Alert marker
- Cursor, selection brackets, group selection marker, range-ring dot, wall-preview tile
- Fog unexplored, fog explored
- HP bar pieces, progress bar pieces, queue icon frames
- Resource UI icons: gold, wood, food, population, idle peasant, idle building
- Time/weather icons: sun, moon, rain, storm, snow, clear
- Research icons: Iron Weapons, Crossbows

---

## 10. Practical production priorities

1. **Core readability first**: grass/dirt/road/water/forest/gold/wheat/berries, Peasant, Town Hall, House, Barracks, Wall/Gate, Tower, Farm.
2. **Combat kit second**: Militia, Archer, Knight, Catapult, Ram, projectiles, selection/range overlays.
3. **Biome expansion third**: desert, snow/tundra, swamp, volcanic, ocean/coastal full terrain variants.
4. **Season/weather pass fourth**: overlays and animation for autumn/winter/thaw/rain/mud.
5. **Polish pass last**: garrison indicators, training/research effects, building haloes, minimap icon set.

The most important structural decision is the team-colour mask layer. It gives the “slot for colour” the user described without contaminating neutral unit art or losing transparency semantics.
