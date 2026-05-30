# Realm manual test plan

Run after material code changes to catch regressions. Each section names the
behaviour the test guards and the bug it would have caught.

This plan is **not** a substitute for the headless harness called out in
[ascii-rts-hardening-plan.md](../implementation/ascii-rts-hardening-plan.md)
Phase 5. It's the floor: what to walk through in a terminal before pushing.

## Status key

* `[ ]` Not run
* `[x]` Passed
* `[!]` Failing / regression — file an issue

---

## Phase 0: Build smoke

### 0.1 Clean build

* [ ] `make clean && make` completes with no errors.
* [ ] No new `-Wall -Wextra` warnings appear (clang's
      `decomposition-declarations` warnings are pre-existing and may be
      ignored).
* [ ] `./realm` launches and shows the opponent-count menu without crashing.
* [ ] `Q` from the menu exits cleanly.

---

## Phase 1: Cursor + selection + navigation

### 1.1 Cursor stays put when keyboard is used

#### Problem
A previous mouse handler reset the in-game cursor to the OS mouse position on
*every* mouse motion event. Keyboard arrows looked dead because stale mouse
events kept yanking the cursor back, and a selected peasant appeared to
"vanish" when the visual cursor jumped away.

#### Tests

* [ ] With the mouse parked anywhere in the map area but not moving, press
      arrow keys. The cursor moves one tile per press and stays where the
      arrows put it.
* [ ] Move the mouse to a new tile. Cursor jumps there exactly once.
* [ ] Press `Shift+arrows` or `PgUp` / `PgDn` / `Home` / `End`. Cursor jumps
      ~10 tiles.

### 1.2 Box-select keeps the count honest

#### Problem
`g.selectedIds` used to keep IDs of dead units forever. The multi-select
header read the raw vector size, so the "Group: N units" count never dropped
as casualties piled up.

#### Tests

* [ ] Drag-select 5 peasants. Side panel reads "Group: 5 units".
* [ ] Send them into enemy archers, let 2 die.
* [ ] Header now reads "Group: 3 units" within one tick of each death.
* [ ] Assign a group to control group `1`. Lose half the group. Press `1`.
      Selection only contains live units.

### 1.3 Cursor renders entity glyph when over a unit

* [ ] Hover over a peasant — cursor shows `p` on a gold background.
* [ ] Hover over a town hall — cursor shows `H`.

---

## Phase 2: Placement + construction

### 2.1 `canPlace` rejects out-of-bounds coordinates

#### Problem
`canPlace` used to read `g.map[y][x]` for the farm terrain check before
running any bounds check. AI placement near the map edge could feed `-1` or
`MAP_W`/`MAP_H` into the lookup.

#### Tests

* [ ] Move the cursor to row 0, then enter build mode (`B`) and try to place
      a 3x3 town hall — the second/third row of the footprint pushes off-map
      and placement is rejected without a crash.
* [ ] Let an AI run for 5+ minutes; no segfaults from edge-of-map
      construction (no console crash on quit either).

### 2.2 Forests are non-buildable

#### Problem
`canPlace` did not reject forest tiles, so a building dropped on
`T_FOREST` / `T_PINE` / `T_PALM` / `T_DEAD_TREE` silently erased the wood
deposit. Edward's plan flagged this as confusing terrain semantics.

#### Tests

* [ ] Position a peasant on a forest tile. Open build mode and try every
      building type. Each is rejected.
* [ ] Chop the forest tile to depletion (`T_DIRT`). Build now succeeds.
* [ ] Units still walk *through* forest — no pathing regression.

### 2.3 Town Hall costs resources

#### Problem
`STATS[E_TOWNHALL]` used to have cost 0g/0w. AI expansion got free town
halls.

#### Tests

* [ ] Starting town halls spawn even though the player has 300g/200w (free
      via setup-time `spawnEntity`).
* [ ] Watch an AI for ~3 minutes — when it tries to expand it has to wait
      until it can afford 200g+150w.
* [ ] The AI's gold/wood actually drops when it starts a forward town hall.

### 2.4 Build menu rejects unaffordable orders

* [ ] Spend down to 0 wood. `B` then `B` (barracks) shows
      "Not enough resources!" and does not deduct.
* [ ] Same for farm, mill, dock, etc.

---

## Phase 3: Resource economy

### 3.1 Berries gather as food

#### Problem
`T_BERRY` existed visually but `orderGather` ignored it. Players got
confusing "no response" feedback right-clicking a berry tile.

#### Tests

* [ ] Find a `*` berry tile in a forest biome. Right-click with a peasant —
      status reads "Picking berries...".
* [ ] Peasant carries food, returns to TC. Food increases by 8 per trip
      (`GATHER_RATE`).
* [ ] Tile depletes to grass (`.` on green).
* [ ] AI peasants also gather berries when other resources are scarce.

### 3.2 Gold / wood / fish still gather

* [ ] Right-click `$` gold tile → "Mining gold...". Peasant drops at TC or
      mining camp.
* [ ] Right-click forest → "Chopping wood..." → TC or lumber camp.
* [ ] Train a fishing boat. It auto-fishes `T_FISH` and drops at dock.
* [ ] Resource node depletes; peasant auto-finds nearby same-type resource
      via `findNearbyResource`.

### 3.3 Farm cycle

* [ ] Sow a farm on grass (`B` → `F` or right-click wheat). Farm shows
      under construction.
* [ ] Peasant builds it. Once complete and adjacent to a mill, food
      accumulates.
* [ ] Winter arrives → farms die (`T_DEAD`), wheat icons disappear.

---

## Phase 4: Combat + damage

### 4.1 Building damage modifier

#### Problem
Pre-modifier, militia killed town halls in ~10 seconds. Catapults felt
underwhelming because nothing was tuned around them.

#### Tests

* [ ] Send a militia to attack an enemy house. House HP drops at ~0.5×
      militia atk. House survives a long time.
* [ ] Send a catapult instead. Damage per hit is ~1.5× raw — house dies
      noticeably faster than under militia fire.
* [ ] Tower fire on enemy walls also goes through the 0.5× rule.

### 4.2 Catapult splash

* [ ] Catapult fires at an enemy in a 2x2 building. Building takes full
      damage. Adjacent units take ~1/3 raw catapult damage.
* [ ] Splash works on multi-tile buildings (footprint overlap distance,
      not top-left only).
* [ ] Friendly fire: parking your own peasant 1 tile from the impact damages
      them too.

### 4.3 Peasant flee-to-shelter

* [ ] Build a tower near a gathering point. Send an AI raid (or attack a
      neutral animal that fights back).
* [ ] When a peasant is hit, it abandons its task and paths to the tower.
* [ ] If the tower is full, it picks the next free Tower / TC / Castle /
      House within ~10 tiles.
* [ ] If no shelter exists in range, peasant keeps doing whatever it was
      doing.

### 4.4 Military auto-engage

* [ ] Park 3 idle militia inside their own town hall vision. Spawn an
      enemy 6 tiles away. Militia move toward it without an explicit order.
* [ ] Press `X` (hold position) on selected units. They stop and do NOT
      auto-engage even if enemies come into vision.

### 4.5 Garrisoned targets are untouchable

* [ ] Order a militia to attack a peasant. As the militia approaches, send
      the peasant into a tower.
* [ ] Militia stops swinging and idles — does not kill the peasant inside
      the tower.

### 4.6 Splash + stacks

#### Problem
Knights converging on the same target stack on one tile and only the first
rendered. Looked like the others disappeared.

#### Tests

* [ ] Select 3 knights, attack one enemy. They converge.
* [ ] When 2+ knights share a tile, the glyph renders **uppercase** (`K`).
* [ ] When a peasant and a knight share a tile, the knight glyph wins.

---

## Phase 5: AI behaviour

### 5.1 AI doesn't deadlock at 10/10 population

#### Problem
`aiGather()` consumed every idle peasant before the build pass ran, so
`aiIdle(o, E_PEASANT)` returned null and the AI never built houses.

#### Tests

* [ ] Start a 3-AI game. Let it run 5+ minutes without playing.
* [ ] Each AI builds at least one house and at least one barracks.
* [ ] Each AI trains some military (visible via Shift+S debug reveal).
* [ ] No AI sits at 10/10 pop for more than ~60 seconds.

### 5.2 Grace period before attacks

#### Problem
AI was attacking within 20 seconds, which felt brutal for a keyboard-driven
RTS.

#### Tests

* [ ] Start a 1-AI game. Time how long until the first wave reaches your
      base.
* [ ] First contact is **no earlier than 2 minutes in** (1500 ticks at
      80ms).
* [ ] When a wave does arrive it's at least 6 units, not a lone scout.

### 5.3 Partial waves, not all-in

* [ ] After the grace period, when an AI sends a wave, count the attackers.
* [ ] Wave size is ~60% of their army with a floor of 3.
* [ ] The AI keeps some military at home for defense.

### 5.4 Owners 2 and 3 render as enemies

#### Problem
Render only treated `owner == 1` as enemy. Owners 2 and 3 fell through to
the wildlife palette and looked like deer.

#### Tests

* [ ] Start a 3-AI game. Use Shift+S to reveal map.
* [ ] All three AI factions render in the enemy red, not animal colours.
* [ ] Minimap matches.

### 5.5 AI doesn't have permanent omniscience

#### Problem
`updateFog` only reset `visible[0]` and `visible[1]` each tick, so AIs 2
and 3 accumulated permanent visible tiles and ignored fog/cloak.

#### Tests

* [ ] Start a 3-AI game. Run 10 minutes.
* [ ] AI 2 and AI 3 lose vision of a tile after their units leave it (hard
      to verify by eye — confirmed by code review, but watch for AI units
      attacking through fog as a red flag).

---

## Phase 6: Visual + weather + season

### 6.1 Rain is foreground-only

#### Problem
Rain used `CP_WATER_SHIMMER` which painted a deep-blue background tile —
each rain dot replaced the terrain colour beneath it.

#### Tests

* [ ] Wait for rain (or cycle weather via long play). The pulsing `.`
      drops appear as bright blue dots **on top of** terrain, not as
      filled blue cells.
* [ ] Drops never land on a tile occupied by a unit/building/projectile.

### 6.2 Tundra seasonal cycle

* [ ] Find a `B_SNOW` biome patch.
* [ ] Winter: full snow.
* [ ] Spring: partial — bare grass shows through.
* [ ] Summer: mostly bare grass, no snow.
* [ ] Autumn: frost ramps back up.

### 6.3 Mud freezes in winter

* [ ] Make it rain. Wait for mud (`,` brown) to form.
* [ ] Winter arrives. Mud tiles turn to snow (`.`) like the rest of the
      ground.

### 6.4 Spring thaw completes by mid-season

* [ ] Spring at progress > 0.4 — snow patches are nearly gone, world looks
      green.
* [ ] At progress > 0.6, no winter ground colour visible (except B_SNOW
      biome's own seasonal cycle).

### 6.5 Ships have wooden decks

* [ ] Train a fishing boat, warship, transport. Each renders with a brown
      background tile (CP_SHIP_PLAYER) rather than just a single coloured
      glyph against water.

### 6.6 Towers fire bolts

* [ ] Build a tower, lure an enemy into range. Projectile glyph is `-` in
      tower colour (bright red), not `*`.

---

## Phase 7: Naval

### 7.1 Dock + fishing boats

* [ ] Build a dock on the shoreline.
* [ ] Train a fishing boat (`T` → `B`). It auto-fishes nearest `T_FISH`.
* [ ] Boat drops fish at the dock.

### 7.2 Warship

* [ ] Train a warship (`T` → `W` at the dock).
* [ ] Warship auto-attacks any enemy within range 5, firing arrows.
* [ ] Warship doesn't try to walk onto land.

### 7.3 Transport load/unload

* [ ] Train a transport (`T` → `T` at the dock).
* [ ] Select 3 land units, right-click the transport.
* [ ] Units path to the shore adjacent to the transport and board (`Cargo:
      3/4` shows in panel).
* [ ] Move the transport across water.
* [ ] Press `U` with the transport selected. Units eject onto adjacent
      passable land.
* [ ] Sink the transport while loaded — units bail out onto adjacent land
      if any, drown if not.

### 7.4 Shallows + reeds

* [ ] Land units can wade through `T_SHALLOWS` and `T_REEDS` (slower).
* [ ] Land units cannot enter `T_WATER`.
* [ ] Boats can traverse all three.

---

## Phase 8: Pre-match + win/loss

### 8.1 Player-count menu

* [ ] Game launches into a centered menu.
* [ ] Pressing `1`, `2`, `3` starts a game with that many AI opponents.
* [ ] Pressing `Q` exits.
* [ ] The chosen number of corners is occupied; the rest are empty
      (visible via Shift+S).

### 8.2 Human defeat ends the match

#### Problem
Previously, when the human's last TC/Castle died, the match continued and
AIs kept fighting each other. The player just watched a screensaver.

#### Tests

* [ ] Sacrifice your TC. Within 100 ticks, the screen flips to "DEFEAT!
      Your kingdom has fallen. [Q] Quit".
* [ ] Pressing `Q` exits.

### 8.3 Victory still works

* [ ] In a 1-AI game, destroy the AI's TC. "VICTORY!" appears.
* [ ] `Q` exits.

---

## Phase 9: Edge cases

### 9.1 Train when blocked

#### Problem
When a building completed training but every adjacent tile was blocked,
the code cleared `e.producing` and popped the next queued unit — the
trained unit was silently consumed.

#### Tests

* [ ] Wall off all 4-ring tiles around a barracks (use the wall drag).
* [ ] Queue 3 militia. They should NOT spawn (no space). Resources stay
      committed.
* [ ] Delete a wall (kill it with another unit). Spawn happens on the
      next tick; queue advances.

### 9.2 Supply cap respected

* [ ] At 9/10 supply with one peasant queued, try to queue another
      1-supply unit. Status reads "Need more houses!".
* [ ] Build a house. Queue clears.

### 9.3 Long-game stability

* [ ] Run a 3-AI game for 30+ minutes (pause, walk away, come back).
* [ ] No crash. No heap corruption symptoms (units randomly disappearing
      en masse, town halls vanishing without combat).
* [ ] Entity count stays well under `g.entities.reserve(8192)`.

### 9.4 Gate behaviour

* [ ] Build a gate. By default it auto-opens when an ally is within 2
      tiles, closes otherwise.
* [ ] Press `O` while gate is selected — gate locks in current state
      ("Mode: Locked").
* [ ] Press `O` again — toggles open/closed while staying locked.
* [ ] Press `O` a third time —... currently no path back to auto. (Known
      gap.)

### 9.5 Right-click symmetry

#### Problem
Keyboard `Enter` and mouse right-click each had their own ~50 line block
of command-routing logic. The two drifted out of sync historically.

#### Tests

* [ ] Select a peasant. Right-click a forest tile → "Chopping wood...".
* [ ] Same selection. Move cursor with arrows over a forest tile, press
      `Enter` → same status message and same action.
* [ ] Repeat for: berry, gold, wheat field, build target, garrisonable
      building, enemy unit, empty tile.
* [ ] Group right-click and group `Enter` produce identical behaviour
      (group move, group attack, group garrison).

---

## Definition of "session ready"

A change can ship when:

* [ ] Phase 0 build smoke passes.
* [ ] Phases relevant to the change pass.
* [ ] If a change touches AI, run Phase 5 in full.
* [ ] If a change touches input, run Phase 1 and Phase 9.5.
* [ ] If a change touches combat, run Phase 4.

---

## Deferred / not yet covered

These are explicit gaps for the next pass.

* No headless test harness yet — every test here is manual. See
  [hardening plan Phase 5](../implementation/ascii-rts-hardening-plan.md).
* No deterministic-seed mode, so reproducing AI-progression tests across
  runs is approximate.
* No screenshots / golden frames for visual regressions (terrain rendering,
  weather overlay, season transitions).
* No load-testing for the entity vector (would catch a future regression
  around the `reserve(8192)` safety net).
