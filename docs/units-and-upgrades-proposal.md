# Units & upgrades — proposal (Brood War-referenced)

Status: PROPOSAL — nothing here is implemented. Numbers reference current
STATS (militia 70hp/8atk, archer 45hp/6atk r5, knight 110hp/14atk,
spearman 55hp/5atk, catapult 70hp/25atk r8) and Brood War's design
grammar: damage types vs unit sizes, +1/+2/+3 smith tiers, and one
signature ability per advanced unit.

## 1. Formalize the counter triangle (BW: damage type × unit size)

Three armor classes on every unit (replaces ad-hoc damageVs rules):

| Class   | Units                                          |
|---------|------------------------------------------------|
| Light   | Peasant, Archer, Militia, Monk, Hussar          |
| Armored | Knight, Crossbowman, Champion, Ram              |
| Siege   | Catapult, Trebuchet, all buildings              |

Four damage types (BW: normal/concussive/explosive):

| Type   | vs Light | vs Armored | vs Siege | Carriers              |
|--------|----------|------------|----------|------------------------|
| Slash  | 100%     | 100%       | 75%      | Militia, Knight, Champion |
| Pierce | 100%     | 60%        | 40%      | Archer (BW concussive: shreds light, pings armor) |
| Thrust | 75%      | 150%       | 50%      | Spearman/Pikeman, Crossbowman (BW explosive vs large) |
| Crush  | 75%      | 100%       | 150%     | Catapult, Trebuchet, Ram |

This makes every existing relationship legible and tunable in ONE table,
and creates the BW property: army composition beats army size.

## 2. New units (5)

**Crossbowman** — Barracks, requires Blacksmith. 70g 30w. 60hp Armored,
9 Thrust, range 4, slow reload. *BW Dragoon.* The ranged answer to
knights; cost-inefficient against militia floods. Fills the hole where
knights currently run over archer lines for free.

**Hussar** — Stable. 80g 20f. 80hp Light, 7 Slash, fastest unit in the
game. *BW Vulture.* Raider: kills peasants/archers/monks, dies to
spears and knights. Exists to attack the NEW economy (autumn granaries,
corn-meadow farms) and to scout. Upgrade unlock: **Caltrops** — drops 3
spike traps (30 dmg, single use, invisible to enemies without a nearby
tower) — spider mines, medievalized.

**Monk** — Church. 60g. 45hp Light, no attack. Heals adjacent friendly
unit 1hp/8 ticks while idle near it. *BW Medic.* Makes small skirmish
armies sustainable and gives the Church a field role. Counterplay:
focus the monk (Light, squishy).

**Sapper** — Barracks, requires Blacksmith. 60g 20w. 50hp Light,
suicide attack: 120 Crush to a building, 30 splash. *BW Infested
Terran, budget edition.* Early-mid siege option before catapults;
melts if anything shoots it on the way in.

**Champion** — Castle. 150g 60f. 200hp Armored, 16 Slash, knight speed
minus one. Passive: arrows deal half damage (plate). *BW Ultralisk
(HP scale: between Dragoon 180 and Ulti 400).* The lategame anvil that
walks through archer fire; answered by Crush splash and massed Thrust.

## 3. Upgrades (Blacksmith becomes a real tech tree)

BW's +1/+2/+3 economy works because base numbers are small — ours are
too (8 atk militia): every +1 is ~12% and flips specific breakpoints.

**Forge tiers** (each tier costs more, requires previous):
- Sharpened Steel I/II/III: +1/+2/+3 melee attack (100/175/250 g+w)
- Fletching I/II/III: +1/+2/+3 ranged attack
- Mail I/II/III: −1/−2/−3 damage taken, floor 1

Breakpoint examples (the BW fun): Fletching I lets archers 3-shot
peasants instead of 4; Mail I makes militia survive one extra archer
volley; Sharpened II lets knights 2-shot monks.

**Specialist tech** (one per building, BW-style single abilities):
- Stable: Horseshoes (cavalry −1 move cd — BW ling speed), Barding
  (cavalry +2 armor vs Pierce only — anti-archer, not anti-spear)
- Church: Litany (monk heal rate ×2), Bell Towers (towers +1 range)
- Castle: Caltrops (unlocks Hussar ability), Plate (Champion arrow
  resist 50%→66%)
- Existing R_IRON_WEAPONS/R_CROSSBOWS/R_PIKES fold into this tree:
  Iron = Sharpened I, Crossbows = unlocks Crossbowman, Pikes =
  Spearman→Pikeman transform (Thrust 150%→200% vs Armored).

## 4. Counter map (after the above)

- Militia flood → beats Crossbowmen, Sappers; loses to Archers, Champion
- Archers → beat Light (militia/hussars/monks); ping off Knights/Champion
- Spearmen/Pikemen → beat Knights, Hussars, Champion (massed); lose to Archers, Militia
- Knights → beat Archers, Catapults, Monks; lose to Spears, Crossbowmen
- Hussars → beat economy + Archers/Monks; lose to Spears, Knights, towers
- Crossbowmen → beat Knights, Champion; lose to Militia, Hussars
- Champion → beats Militia/Archer cores; loses to Catapult splash, Pikemen
- Catapult/Trebuchet → beat buildings, clumps, Champion; lose to anything fast
- Sapper → beats buildings; loses to everything that attacks

## 5. Implementation order (when approved)

1. Armor class + damage table in damageVs (pure refactor, no new units —
   verify with --verify before/after intent change).
2. Forge tiers (player + AI difficulty-aware purchase order).
3. Crossbowman + Hussar (core counter loop).
4. Monk, Champion, Sapper.
5. Caltrops last (new entity kind: trap).
Each step: enum append at the END of unit range (save compat), STATS row,
train menu key, AI build-order entry, determinism verify.
