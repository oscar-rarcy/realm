# Realm Unit Reference Catalogue

This catalogue records what the user-supplied unit references are for. The references should be shown to the image generator only as visual evidence for gear and equipment, then redrawn into the Realm style described by the generated unit prompts.

## Reference Role

- Authoritative for: equipment identity, visible weapon tier, armour tier, shield style, front/back readability, and broad silhouette intent.
- Not authoritative for: final Realm art style, exact pose, exact proportions, lighting, background, crop, pixel finish, or runtime sprite canvas.
- Generation rule: use references as a guide, then create stylized Realm small-RTS sprites that follow the current `art/tiles/image-spec/units/*.md` prompt.

## Generation Order

Start with player-owned human units before vehicles:

1. Peasant
2. Militia
3. Archer
4. Knight
5. Spearman / Pikeman references, once the runtime/exported unit set needs them
6. Scout references, once the runtime/exported unit set needs them
7. Siege units, after references are supplied
8. Ships, after references are supplied

This mirrors the production priority in `docs/tileset/realm_tileset_visual_audit.md`: core readability first, then combat kit, then biome/naval/siege expansion.

## Curated Files

| Unit or concept | Reference files | Use |
|---|---|---|
| Peasant | `peasant-front.png`, `peasant-rear.png` | Worker clothing, simple labourer silhouette, front/back read. |
| Militia basic | `militia-front.png`, `militia-rear.png` | Basic infantry gear, shield/body relationship, front/back read. |
| Militia Iron Weapons | `militia-iron-weapons-front.png`, `militia-iron-weapons-rear.png` | Upgraded weapon/shield hardware and rear equipment read. |
| Archer self bow | `archer-self-bow-front.png`, `archer-self-bow-rear.png`, `quiver-edit.png` | Hood/quiver/bow setup for the starting archer tier. |
| Archer crossbow | `archer-crossbow-front.png`, `archer-crossbow-rear.png`, `quiver-edit.png` | Crossbow tier equipment and bolt/quiver treatment. |
| Knight basic weapons / base armour | `knight-base-weapons-base-armour-front.png`, `knight-base-weapons-base-armour-rear.png`, `knight-base-weapons-base-armour-spear.png` | Mounted early-cavalry identity, basic spear, horse/rider read. |
| Knight Plate Helm | `knight-base-weapons-plate-helm-front.png` | Plate Helm armour and saddle/horse-armour cues without Iron Weapons. |
| Knight Iron Weapons | `knight-base-armour-iron-weapons-front.png`, `knight-plate-helm-iron-weapons-front.png`, `knight-plate-helm-iron-weapons-rear.png` | Lance, upgraded shield, iron weapon tier, and combined Plate Helm + Iron Weapons cues. |
| Spearman | `spearman-front.png`, `spearman-rear.png` | Long spear infantry gear and front/back read. |
| Pikeman | `pikeman-front.png`, `pikeman-rear.png` | Longer pike tier silhouette and bracing-equipment reference. |
| Scout | `scout-blue.png`, `scout-blue-rear.png` | Mounted scout/horse gear reference for future unit coverage. |

## Work-In-Progress Material

`units-wip/` contains experiments, paint files, intermediate edits, and alternative generation attempts. Use it only when a curated file above is missing a needed equipment detail. Do not treat WIP material as accepted reference unless a future catalogue entry promotes it.

## Known Gaps

- Siege unit references are not supplied yet.
- Ship references are not supplied yet.
- Current references are idle/front/back oriented; full action states still need generation from the prompt state grid.
- Future references may fill every runtime unit, vehicle, and direction, but the reference-only rule stays the same.
