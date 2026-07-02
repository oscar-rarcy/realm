# Eras, civilisations, stockyards, and the antagonist AI (2026-07-02)

The "make it compelling" batch. Four systems, one goal: give every match an
arc, an opponent with intentions, and something worth stealing.

## Eras — the match arc

Three eras, advanced at the Town Hall/Castle (`[E]`, or the gold row in the
panel). `Player.era`, sim state, hashed, saved, in the replay header via the
seat's civ/era config.

| | Cost | Time | Unlocks |
|---|---|---|---|
| **Hamlet** (start) | — | — | Peasant, Militia, Fishing Boat; TC, House, Farm, Mill, camps, Well, Dock, Barracks, **Walls + Gates** (a village can always defend itself), Bridge |
| **Township** | 175f 100g | 900t | Archer, Spearman, Hussar, Monk, Wagon, Transport, Warship; Stable, Blacksmith, Market, Church, Tavern, Granary, Manor, Tower, **Stockyard**; techs: Iron Weapons, Fletching, Crossbows, Pikes, Heavy Plough |
| **Stronghold** | 450f 300g 150w | 1300t | Knight, Crossbowman, Sapper, Catapult, Ram, Trebuchet; Castle, Stonemason; techs: Plate Helm, Counterweight, Horse Breeding, Masonry |

- Gate authority: `eraOf()` / `makeGate()` in combat.cpp — orderTrain/orderBuild
  enforce (sim-side), menus dim locked rows with the era name, AI checks
  `aiCan()` before shopping.
- Era-up rides the building `researching` machinery with the `R_ERA_ADVANCE`
  sentinel; completion announces to everyone ("The Hillfolk (P3) enter the
  Stronghold era!").
- One advance at a time realm-wide; costs charged up front.

## Tech tree — spread across buildings

`ResearchDef RESEARCH[]` in commands.cpp is the single authority (menus,
input keys, charges, AI shopping all read it). New techs: **Fletching**
(smith, ranged +1 atk), **Heavy Plough** (mill, farms +1), **Horse Breeding**
(stable, new cavalry +15 HP), **Masonry** (stonemason, buildings take −20%).
`R` opens research at Blacksmith / Mill / Stable / Stonemason.

## Civilisations — bonus + denial

`CIVS[]` in globals.cpp; `Player.civ`; picked on the skirmish/lobby screens
(client picks its own in the MP lobby, travels as MSG_CIVPICK, host echoes in
CONFIG v2). AI civs + personas roll from the seed in initGame with **constant
simRand consumption** (two rolls per seat, always) so choices can never shift
the RNG stream. Replay header (v8) stores the per-seat `civChoice` config.

- **Freeholders** — peasants train 15% faster. No denial.
- **Fenlanders** — farms +1, boats −25%, archers train fast. *No stables.*
- **Hillfolk** — miners +25%, walls/towers/gates +50% HP. *Military trains
  slower, no war fleet.*
- **Marcher Lords** — stable units −20% and train 25% faster. *No archers.*

Hooks: `costGoldOf/costWoodOf/trainTimeOf` (combat.cpp), gather-rate in
entity.cpp S_GATHERING, farm yield in tickFarms, muster HP in spawnEntity.

## Stockyard — the raidable hoard (the DF itch)

`E_STOCKYARD`: 3×3, 60w, Township. Caps 300 gold + 300 wood + 300 food — the
biggest storage in the game, and it renders its contents as **visible piles,
one tile each** (top row gold `$`, middle wood `=`, bottom food `%`; glyph
grows with the pile). Integrates with the normal depot economy
(drainStores/depositToNearest/findDepot).

**Raiding**: right-click an enemy stockyard (or CMD_RAID) sends land units to
steal: `S_RAIDING` walks to the piles, takes up to 30 of the tallest stack
(the victim's totals drop immediately), then rides home through the standard
courier flow — kill the raider and the goods scatter as loot. Victim gets
"Raiders are plundering your Stockyard!". `--test-raid` is the headless
harness for the whole pipeline (AI must rob a staged yard within 4000 ticks).

## AI — an antagonist now

- **Fair intel**: `aiScout`/target pickers only count entities on tiles the
  AI has `explored[]`. No more omniscience — and therefore:
- **Scouting**: until the enemy hall is found, a fast unit rides at
  unexplored quarters on a deterministic cadence; afterwards occasional sweeps.
- **Personas** (`Player.aiPersona`, rolled per seat): **Raider** (restless,
  night attacks at −2 threshold, stables full of hussars, raids every ~45 AI
  ticks), **Builder** (booms, +4 peasant cap, patient), **Warlord** (fewer,
  bigger waves, +4 military cap).
- **Seasonal intent**: winter +3 attack threshold (defensive), summer −1;
  autumn: raid interval ×⅔ and target scores +120 for granaries, mills,
  stockyards, taverns — the harvest is the season to strike.
- **Plunder squads**: 2-3 hussars/militia dispatched between waves at
  scouted stockyards (steal) or gatherers/farms (harass).
- **Era-up**: all personas advance when the economy allows (Builder
  earliest); training/build shopping is era- and civ-gated so composition
  adapts (a Fenlander AI never wants knights; a Marcher AI fields no archers).

## Balance notes (from `--verify` probes, which now print per-seat summaries)

FFA between AIs resolves decisively by ~12-15k ticks — one seat snowballs to
Stronghold with near-full research; weak seats can die in Hamlet. Matches
END now, which was the goal, but early-game snowball pressure is worth
watching in human playtests. Walls/gates were moved down to Hamlet
specifically so a pressured player is never locked out of defence.

## Compatibility

SAVE_VERSION 12, REP_VERSION 8, NET_PROTO_VERSION 2 — old saves/replays are
rejected cleanly; both players need the same build (as ever).
