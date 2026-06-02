# Realm Peasant Tileset Spec

Tileset mode renders terrain as isometric-projected diamonds and renders units as upright sprites anchored over the unit tile.

| Action | Family | Target relation | Frames | Timing | Loop | Fit |
|---|---|---|---|---:|---|---|
| `idle` | idle | self_tile | Relaxed idle, arms at sides, both feet planted.<br>Long-idle hold pose, arms crossed, both feet planted. | 20000 ms | False | standing |
| `walk` | gait | self_tile | Walking gait with the front or near leg forward and the rear or far leg back.<br>Walking gait with the rear or far leg forward and the front or near leg back. | 90 ms | True | standing |
| `chop_wood` | swing | adjacent_target_tile_or_entity | Axe at the bottom or contact part of the chop, axe head low and forward.<br>Axe raised high at the top of the swing. | 160 ms | True | wide_tool |
| `mine_gold` | swing | adjacent_target_tile_or_entity | Pickaxe at the bottom or contact part of the mining swing, pick head low and forward.<br>Pickaxe raised high at the top of the swing. | 160 ms | True | wide_tool |
| `gather_berries` | gather | adjacent_target_tile_or_entity | One hand reaching out toward berries beside a basket.<br>Hand back in the basket with berries. | 350 ms | True | kneeling |
| `hoe_soil` | work_stroke | adjacent_target_tile_or_entity | Arms outstretched with the hoe extended away from the body.<br>Arms pulled in after the hoe stroke while still holding the same farming hoe. | 260 ms | True | wide_tool |
| `gather_wheat` | gather | adjacent_target_tile_or_entity | Using a sickle to cut wheat.<br>Still holding the sickle while the free hand reaches for wheat. | 260 ms | True | wide_tool |
| `build` | hammer | adjacent_target_tile_or_entity | Kneeling builder with hammer raised up.<br>Kneeling builder with hammer down. | 150 ms | True | kneeling |
| `carry_wood` | carry_gait | self_tile | Carrying bundled logs while walking, front or near leg forward.<br>Carrying bundled logs while walking, rear or far leg forward. | 90 ms | True | standing |
| `carry_gold` | carry_gait | self_tile | Carrying stones and gold ore while walking, front or near leg forward.<br>Carrying stones and gold ore while walking, rear or far leg forward. | 90 ms | True | standing |
| `carry_berries` | carry_gait | self_tile | Carrying berries while walking, front or near leg forward.<br>Carrying berries while walking, rear or far leg forward. | 90 ms | True | standing |
| `carry_wheat` | carry_gait | self_tile | Carrying wheat while walking, front or near leg forward.<br>Carrying wheat while walking, rear or far leg forward. | 90 ms | True | standing |
| `gather_meat` | gather | adjacent_target_tile_or_entity | Holding a knife while actively cutting or reaching toward meat.<br>Still holding the knife while taking meat with the free hand. | 260 ms | True | kneeling |
| `carry_meat` | carry_gait | self_tile | Carrying meat while walking, front or near leg forward.<br>Carrying meat while walking, rear or far leg forward. | 90 ms | True | standing |
| `club_attack` | swing | adjacent_target_tile_or_entity | Club at the top of the attack swing, held overhead but still inside the tile.<br>Club at the bottom or contact part of the attack swing, still fully inside the tile. | 130 ms | True | wide_tool |
| `death` | one_shot | self_tile | Dead villager body lying on the ground, not a skeleton.<br>Skeleton remains of the same villager in the same ground area, with small clothing and tool scraps. | 30000 ms | False | lying |
