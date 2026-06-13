# Combat feel — proposals

Status: PARTLY IMPLEMENTED (June 2026). A deep dive on why Realm fights feel
the way they do and a menu of changes, ordered roughly by (impact / effort).
The armour-class damage table, high-ground modifiers, and anti-bunching landed
earlier.

**Implemented June 2026 (SAVE_VERSION 8, REP_VERSION 5):**
- **1.1 Morale & routing** — per-unit `morale` 0–100, drains on wounds /
  nearby friendly deaths / being locally outnumbered; recovers out of combat,
  near a TC/Castle, or beside a veteran banner. At 0 the unit enters
  `S_ROUTING`: drops orders, flees to the nearest stronghold at +1 speed,
  unorderable for ~80 ticks, renders as a blinking `?`. Rallies (morale→40)
  on reaching safety or when the panic passes.
- **1.2 Charge impact** — Knight/Hussar with ≥4 consecutive strides into the
  foe deal ×2 on the first blow + knockback + stun; spearmen and buildings
  cancel the bonus. `chargeSteps` resets on any block/turn.
- **1.3 Death reads** — corpse `%` markers linger ~200 ticks (render-only
  `g.corpses`); "You broke their line!" status flash on 3 routs in 20 ticks.
- **2.2 Stamina** — `stamina` 0–100 drains marching/fighting, recovers at
  rest; <30 → −25% damage and +1 move cd.
- **2.4 Veteran banner** — a militia with 3+ kills gives +1 atk and +morale
  regen to friendlies within 3.
- **3.3 Capture & ransom** — a router cornered by an adjacent enemy footman
  for ~40 ticks becomes an inert prisoner (owner = captor); `tickPrisoners`
  marches him to the captor's hold to be ransomed (+25g) or frees him if a
  soldier of his old side reaches him.
- **3.4 (partial) Catapult entrenchment** — a catapult standing still ≥200
  ticks gains +1 range; rolling resets `entrenchTicks`.

**Still open:** 1.4 engagement-lock/parting hits, 2.1 combat collision,
2.3 facing/flanking, 3.1 battle pause/auto-slow, 3.2 weapon reach rows,
3.4 sortie tools (murder-holes / boiling oil). The text below is the original
proposal menu.

## Diagnosis: why fights blur

Watching current battles, five things flatten the drama:

1. **Fights resolve as HP races.** Two blobs meet, damage ticks down, the
   bigger blob wins. There is no *moment* — no charge impact, no line
   breaking, no rout. Every death is the same quiet disappearance.
2. **No positional commitment.** Units path freely through each other
   (tile-sharing), so there are no fronts, no flanks, no surrounds. A
   "formation" exists for one tick after arrival.
3. **Damage is continuous, morale doesn't exist.** Real medieval fights
   ended when one side *broke*, usually well before half were dead.
4. **No reads or feints.** All information is positional and instant;
   nothing rewards predicting the opponent.
5. **Sameness of pace.** Every engagement runs at the same tempo whether
   it's 4 militia vs a boar or 60 units at a river ford.

## Tier 1 — cheap, high impact

### 1.1 Morale and routing
Per-unit `morale` (0–100). Drains on: taking damage, nearby friendly
deaths (bigger drain if it's the same tile/adjacent), being outnumbered
in local radius, fighting uphill. Recovers near TC/Castle/banner units,
when enemies rout, slowly when out of combat. At 0 → `S_ROUTING`: unit
drops orders and flees toward the nearest friendly base at +1 speed,
cannot be re-ordered for ~80 ticks, renders with a distinct glyph (e.g.
reversed or `?`).

Why it's the single best change: battles get a *decision point*. Killing
the enemy's knights doesn't just remove DPS — it can crack their whole
line. Hussars get a real job (chase routers). Monks/banners get a defensive
role (morale anchor). And losses stop being symmetrical: the loser keeps
most of their routed units if the winner doesn't pursue — fights have
aftermaths instead of annihilation.

Sim cost: one int per entity, drains/recovers in `tickEntity`. Save bump.

### 1.2 Charge impact (cavalry)
If a Knight/Hussar's last N steps were consecutive moves toward its
target (track `chargeSteps`, reset when blocked/turning), the first hit
deals +100% and knocks the target back one tile (if free) with a brief
stun (atkCd maxed). Punishes standing still against cavalry, rewards
catching archers in the open, and makes spear-bracing visible: spearmen
*cancel* charge bonuses against themselves.

### 1.3 Death should read
- Corpse glyph `%`/`x` on the tile for ~200 ticks (render-only overlay
  list, not entities — no sim cost beyond a deque of (x,y,tick)).
- One-tick flash on the killing blow.
- Status line for big swings: "Your knights broke their line!" when 3+
  enemies rout within 20 ticks.

### 1.4 Engagement lock (soft ZoC)
A unit in melee contact that tries to *move away* takes one free parting
hit from each adjacent enemy melee unit ("attack of opportunity").
Suddenly committing the militia line means something, and pulling
wounded units out is a real tradeoff. ~20 lines in `moveAlongPath`.

## Tier 2 — moderate effort, changes the shape of battles

### 2.1 Real collision for combatants only
Full no-tile-sharing was tried (branch `unit-collision`) and made
pathing miserable. Narrower rule: a tile may hold at most 1 unit **per
side that is currently in combat** (alertTicks > 0). Peaceful stacking
(eco, marching) stays free; battle lines physically form because
attackers can't all occupy the defender's tile. Combined with 1.4 you
get fronts, flanking, and chokepoint defence at bridges/ramps/gates —
the elevation ramps from this week become Thermopylae.

### 2.2 Stamina
`stamina` 0–100: drains while fighting and force-marching, recovers
idle/garrisoned. Below 30%: −25% damage, +1 move cd. Armies that
marched across the map fight worse than the defenders who waited —
defender's advantage without any explicit bonus, and a reason for the
forward castles the AI already builds.

### 2.3 Facing & flanking (lightweight)
Don't model true facing — derive it: if a unit is attacked by 3+
enemies from tiles spanning >90° of arc, hits from "behind" (opposite
the unit's current target) deal +25%. Encourages envelopment without
any new player controls.

### 2.4 Banner/Sergeant unit, or banner on Militia veterancy
A cheap aura unit (or militia that survive 3 fights promote) giving
+morale regen and +1 atk in radius 3. Gives armies a heart to cut out —
focus-firing the banner is the new tactical read.

## Tier 3 — bigger swings

### 3.1 Battle pause / tactical time
At 80 ms/tick a 40-unit fight outpaces human orders. Options: (a)
SC-style: just let it ride (current); (b) auto-slow: TICK_MS 80→160
while ≥10 units have alertTicks (smooth ramp), reverts after; (c)
active pause (P already pauses — allow issuing commands while paused).
(b) is the sleeper hit: big battles *feel* big because time itself
thickens, and you get to actually use the new tools (focus banner,
pull wounded, commit cavalry).

### 3.2 Weapon reach / first-strike rows
Spears strike from range 2 over a friendly front-rank tile (pike
hedge). Two-row infantry fights: militia wall + spear second rank
becomes the classic anvil. Needs 2.1 to matter.

### 3.3 Capture & ransom
Routed units that are caught (adjacent enemy for N ticks) are captured,
not killed: they convert to prisoners (entity, owner = captor, inert)
and can be ransomed at the Market (gold per head) or freed by a rescue.
Medieval-authentic, gives raids a profit motive beyond arson.

### 3.4 Siege escalation arc
Sieges currently are "walk engines up, grind". Give defenders sortie
tools (gate murder-holes: gate tile deals damage to adjacent enemies;
boiling oil cooldown on castle) and attackers commitment tools
(catapults entrench after 200 ticks in place: +range +1, can't move
without un-entrenching). The siege becomes phases — invest, breach,
storm — instead of one long HP bar.

## Recommended order

1. Morale/rout (1.1) — transforms everything downstream.
2. Corpses + kill feedback (1.3) — one evening, pure feel.
3. Charge impact (1.2) + parting hits (1.4) — cavalry and lines get
   identities.
4. Combat-collision (2.1) — with the elevation ramps already in, this
   is where chokepoint warfare arrives.
5. Auto-slow big battles (3.1b).

All of it is sim-deterministic (no rand outside simRand, no wall-clock
inputs), so `--verify` and the replay/lockstep plan stay intact.
