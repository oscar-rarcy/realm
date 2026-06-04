# Realm Unit Reference Production Status

This file tracks reference coverage, not accepted runtime sprite completion.

Runtime-ready unit art still belongs under `assets/tiles/entities/...` after review and promotion. These reference images remain in `art/reference/units/` and should not be loaded by the game.

| Unit or concept | Reference status | Runtime sprite status | Notes |
|---|---|---|---|
| Peasant | Front/back reference present | Full generated paper-cutout runtime sprite set installed; review bundle generated | References show gear/read only. Mechanical QA is clean for 64 base/team-mask frames; visual polish may still refine individual action poses. |
| Militia | Front/back basic and Iron Weapons references present | Reference-only | Use generated prompt state grid for actions/death. |
| Archer | Front/back self-bow and crossbow references present | Reference-only | `quiver-edit.png` is a detail reference. |
| Knight | Multiple equipment-tier references present | Reference-only | Plate Helm and Iron Weapons remain independent visual axes in prompts. |
| Spearman / Pikeman | Front/back references present | Reference-only / future runtime coverage | Current generated specs include Spearman; Pikeman reference may feed future tier or unit work. |
| Scout | Front/back references present | Reference-only / future runtime coverage | Not currently part of the main generated unit prompt set. |
| Siege units | Missing | Reference-only gap | Add catapult, ram, and trebuchet references later. |
| Ships | Missing | Reference-only gap | Add fishing boat, warship, and transport references later. |

## Next Steps

1. Add a reusable stylized Realm unit prompt template once the desired style language is supplied.
2. Feed references to image generation as equipment/silhouette references, not style references.
3. Generate a first contact sheet for one unit and direction.
4. Review the contact sheet visually before cropping or regenerating production frames.
5. Copy accepted candidates into `art/tiles/candidates/units/...`.
6. Promote only reviewed runtime-ready frames into `assets/tiles/entities/...`.
