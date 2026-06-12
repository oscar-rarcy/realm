# Stockpiles, stealable wealth, food types & ale — proposal

Status: PROPOSAL. Dwarf-Fortress-referenced rework of where resources
*live*, plus differentiated food and alcohol with tactical teeth.

## 1. Where Realm already is

The codebase has quietly grown most of a local-storage model:

- `Mill.carrying` tracks food physically stored at the mill — destroyed
  mill = forfeited food (killEntity already implements the loss).
- Farms ripen into `farm.carrying`; peasant couriers ferry it.
- Peasants carry resources in `Entity.carrying` and die holding them.
- Drop-off buildings (camps/dock/mill) are already *where* resources
  arrive.

What's missing is the DF idea: **the player's wealth is the sum of what
physically sits somewhere**, not a global int. Full DF item-level
simulation is wrong for an RTS — but per-building stockpiles are the
right altitude, and we're 40% there.

## 2. Proposal: per-depot stockpiles (DF lite)

### Model
Replace `Player.gold/wood/food` as the *source of truth* with per-depot
storage; keep the player totals as a cached sum (UI + cost checks stay
one-line).

- Every drop-off building gets `store[3]` (gold/wood/food), cap by type:
  TC 400 each, Castle 600, camps 250 of their type, Mill/Granary 300
  food, Dock 200 food.
- Deliveries go to the receiving depot. Spending drains *nearest-first*
  to the paying site (building a barracks pulls wood from the closest
  pile — and a depleted frontier camp means the next delivery walks
  further: logistics emerge without scripting).
- Cap full → peasants haul to the next depot (further walk = visible
  inefficiency, the DF "your stockpile layout matters" feeling).

### Raiding & theft (the fun part)
- **Destruction forfeits**: any depot's store is lost with it
  (generalises the existing mill rule). Burning the enemy's frontier
  lumber camp now *means* something.
- **Plunder**: enemy units adjacent to a destroyed depot's tile for the
  next ~100 ticks pick up loot (`carrying` + gatherType of the stolen
  kind, CARRY_MAX each) and auto-haul it to *their* nearest depot.
  Hussars become actual raiders: hit the granary, ride home with the
  harvest. Counterplay: kill the laden raider — loot drops again.
- **Wagons**: a slow civilian unit (trains at Mill/Granary, 40g) that
  rebalances 100 units between two depots per trip. The player's supply
  lines become visible, ambushable things — escort duty exists now.
- AI: teach aiPickTarget to weight depots by stored value (it already
  scores building types; add `+ store/4`).

### Why this stays RTS-sized
No item entities, no hauling jobs queue, no quality levels. Twelve ints
per building and a nearest-depot spend rule. Determinism unaffected.
Save bump, UI: depot panel shows "Stored: 120g 40w" (mill already does).

### Step 1 (one session, ships alone)
Food only: Granary building + food lives at Mill/Granary/TC + plunder
rule. Food is the seasonal resource with an existing local-storage
precedent — and "they burned the granary before winter" is the single
best story this game can generate. Gold/wood follow once it proves out.

## 3. Food types

Make food a 4-slot larder instead of one number. Each source already
exists in code; this differentiates what it yields:

| Type    | Source                  | Keeps?                  | Edge                                  |
|---------|-------------------------|--------------------------|---------------------------------------|
| Grain   | farms, wheat harvest    | forever (granary)        | brews ale; winter staple              |
| Meat    | hunting, sheep, boar    | spoils (~1 season) unless wintered/salted | units fed meat: +1 atk for a day (well-fed) |
| Fish    | fishing boats, shoals   | spoils (~1 season); salted at Dock for 20g | cheap mass food; coastal identity     |
| Berries | bushes                  | spoils fast (~half season) | early game only, foragers' food       |

- Consumption (winter drain, training food costs) takes spoilables
  first, grain last — automatic, no micromanagement.
- Spoilage is just a per-depot decay tick on meat/fish/berry slots:
  visible pressure to *use* the hunt now, store grain for the freeze.
- Tactical layer: a player you've cut off from grain (burned fields,
  stolen granary) can survive on fish — unless you blockade the docks
  with warships. Food becomes a war target with multiple axes instead
  of a bar that goes down.
- UI: top bar shows total Food, larder breakdown on TC/Granary panel.

## 4. Alcohol (the DF homage with a tactical edge)

**Ale**: brewed at the Tavern/Brewery from grain (2 grain → 1 ale,
stored like food, never spoils).

DF makes alcohol *required*; an RTS should make it *tempting*:

- **Well-drunk aura**: military units passing within radius 4 of a
  tavern holding ale consume 1 and gain "ale-warmed" for ~600 ticks:
  +morale regen (once morale lands; until then +1 atk), −1 hp frostbite
  immunity in winter. Winter campaigns want a wagon of ale along — the
  ale wagon is a target.
- **Dutch courage / the tradeoff**: ale-warmed units have −1 range on
  ranged attacks (archers shoot worse drunk) and rout *later* (morale
  floor raised). Melee armies love ale; archer lines should stay sober.
  One buff, one nerf, real composition decision.
- **Victory feast** (active ability on the Tavern, costs 10 ale, long
  cooldown): all units in radius 8 heal 20% and reset stamina. The
  post-battle rally point becomes a place on the map.
- **Raid value**: ale stores plunder like any stockpile, and burning a
  tavern hurts morale of everyone who's buffed (the men watch the beer
  burn). Cheap to implement, maximum medieval.

Implementation: ale is a 5th larder slot + tavern building + timed buff
int on Entity (like alertTicks). No new systems. The morale proposal
(combat doc 1.1) multiplies its value but isn't required for v1.

## 5. Suggested sequencing across both economy docs

1. Granary + food-only stockpiles + plunder (step 1 above).
2. Food types with spoilage (slots into the same larder struct).
3. Tavern + ale buff.
4. Gold/wood stockpiles + wagons.
5. AI raiding weights.

Each step is independently shippable and `--verify`-safe.
