# Realm Environment State Design

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

- Animals use the runtime `death` action with two frames: freshly dead readable carcass, then the same animal's clean depleted skeleton remains.
- Do not create separate partly harvested or mostly harvested runtime actions unless the C++ animation contract adds them.
- Equipment and durable objects should remain visible on military units and vehicles; animal carcasses should keep species silhouette readable.
