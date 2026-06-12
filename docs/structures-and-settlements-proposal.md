# Non-military structures & enterable buildings — proposal

Status: IMPLEMENTED 2026-06-12 (same-day batch), with these deviations:
- Monastery skipped (was conditional on monks proving fun; relics are a
  separate feature).
- Stonemason auto-repair consumes one T_STONE deposit per 200 hp of
  repairs (converts it to gravel) rather than a peasant-mined resource.
- Castle compound perimeter (walls/gates) spawns pre-built; only the keep
  needs construction — avoids builders being walled out of their own site.
- Road-wear seeding along claim routes not done (traffic does it anyway).

Original proposal follows. Two threads: (1) civic/economic structures, both
found-on-the-map and buildable, that make the realm feel inhabited;
(2) making large buildings physically large — walkable interiors,
starting with the Castle.

## 1. Found-on-the-map (neutral) structures

The capturable ruined keep (E_RUIN) proved the pattern: a neutral
structure that rewards map control without being a resource node.
Candidates, same garrison-to-claim or stand-adjacent-to-use grammar:

- **Hermit's shrine** (1x1). Units resting adjacent heal at double rate;
  a monk garrisoned inside projects the heal aura at radius 4. One per
  map third.
- **Old watermill** (2x2, riverside). Claim like a ruin; while garrisoned
  by a peasant it acts as a half-rate Mill (farm bonus + food dropoff).
  Rewards settling rivers beyond fishing.
- **Trading post** (2x2, on the long road crossings mapgen already
  draws). Garrisoned: passive +3 gold/50 ticks and market trade access
  without building a Market. Contested mid-map economy — a reason armies
  meet away from the bases.
- **Stone circle / barrow** (decorative cluster + 1 tile). Vision +6 while
  a unit stands on it (hilltop beacon grammar — pairs with elevation).
- **Abandoned village** (cluster of 3–5 ruined house footprints).
  Peasants can *repair* (half cost, half build time) instead of building
  new — a found expansion site. Mapgen already scatters T_RUINS; this
  upgrades some into entity shells with hp=1, underConstruction=true.
- **Wolf den / boar nest** (1x1). Spawns a wolf every N ticks until
  destroyed (melee can destroy it — it's Light, not Siege). PvE pressure
  early; a free XP/food farm if you clear it.

## 2. Buildable civic structures

Each should earn its place by touching an existing system, not adding a
parallel one:

- **Granary** (2x2, 80w). THE stockpile building — see the economy
  proposal. Stores food locally (like Mill.carrying but big), halves
  winter drain for units within radius 10. Burns: raiders torch the
  winter stores. This is the highest-value civic building by far.
- **Tavern/Brewery** (2x2, 60w 40g). Converts stored grain to ale (see
  food/alcohol proposal); units that pass adjacent get the ale buff;
  morale anchor radius (once morale lands). Also: idle peasants drift
  toward it — a visible village heart, pure charm.
- **Well** (1x1, 20w 10g). Small heal-rate aura for peasants only;
  buildings within radius 6 take −25% fire/raid damage (bucket line).
  Cheap early flavour.
- **Manor** (2x2, upgrade of House, 100w 50g). +10 supply, garrison 6,
  small tax: +1 gold/50 ticks per nearby worked farm. The "lord's house"
  step between House spam and Castle.
- **Monastery** (3x2, 120w 100g). Off-map church: trains monks, slowly
  generates "relics" (one-time morale/heal artifacts a monk can carry).
  Optional — only if monks prove fun.
- **Stonemason** (2x2). Unlocks stone walls (wall HP x2) and castle
  repair; consumes the T_STONE deposits that currently do nothing.
  Gives stone a purpose as a fourth soft resource without a full
  resource pipeline.

Priorities: Granary first (it IS the stockpile mechanic), then Tavern
(food/alcohol hook), then Stonemason (gives T_STONE meaning).

## 3. Large, enterable buildings — yes, and the Castle first

### Why it works here
The map already has the grammar: castle *ruins* are terrain
(T_CASTLE_WALL / T_CASTLE_FLOOR / T_CASTLE_GATE) that units walk
through. So "enterable building" doesn't need an interior-map system —
it needs player-built structures that are made of terrain-like cells
instead of an opaque footprint.

### Proposal: the Keep — a built castle you walk into
Replace (or upgrade) E_CASTLE with a **compound**: a 7x7 placement that
writes terrain + sub-entities:

```
# # # G # # #        # = castle wall cell (entity per cell, like E_WALL)
# . . . . . #        G = gate (existing E_GATE auto-open logic)
# . ┌───┐ . #        . = courtyard floor (T_CASTLE_FLOOR, fast move)
G . │KEEP│ . G        KEEP = 3x3 central keep building (the old E_CASTLE:
# . └───┘ . #               trains trebuchets, garrison 10, volley fire)
# . . . . . #
# # # G # # #
```

- Walls are individual E_WALL-grade entities (already breach-resistant,
  already block paths) so sieges open *a hole*, not delete the castle.
- The courtyard is real space: your units stand inside, protected by
  walls; peasants flee into it; food wagons (stockpile proposal) park
  there. Defenders on walls? Garrison individual wall cells (cap 1) for
  archers to shoot from the parapet — wall garrison already half-exists
  via tower code in tickTowers.
- The keep itself is the last stand: compound breached ≠ castle lost.
  checkWin keys on the keep.
- Construction: peasants build it like a wall-line order — the CMD_BUILD
  writes the whole compound plan as queued wall/gate/keep spawns.

Cost: this is the biggest single feature in this doc set. The pieces it
needs all exist (per-cell walls, gates, castle-floor terrain, garrison,
multi-entity spawn from one command), so it's integration work, not new
systems. Estimate: 2–3 sessions including AI placement (AI needs a
flat 7x7 — canPlace already enforces same-elevation footprints).

### Interiors beyond the castle
- **Town Hall plaza** (5x5 variant, same pattern, no walls — just floor
  + the 3x3 hall): visual upgrade only, cheap once the compound
  machinery exists.
- **True interiors** (separate interior map you "enter"): NOT
  recommended. It forks the sim (two maps per entity-position), breaks
  the single-map fog/path/hash invariants, and the compound pattern
  delivers 90% of the fantasy on the existing board.

## 4. Map-feel multipliers (cheap, do anytime)

- Smoke glyph above occupied houses at dusk/dawn (render-only).
- Peasants idle near the Tavern/TC at night instead of where they stood
  (already have day/night; pure behaviour sugar).
- Roads between claimed structures wear in automatically (tickPaving
  already does this — just seed wear along claim routes).
