# Realm architecture map + cookbooks

One header (`realm.h`) declares everything; each `.cpp` owns one concern.
The sim is deterministic lockstep (see `docs/networking-plan.md` and
`docs/multiplayer-roadmap.md`): same seed + same command stream = the same
game, bit for bit, on every machine. That single property powers saves,
replays, multiplayer and `make check` — protect it above all else.

## File map

| File | Owns |
|---|---|
| `realm.h` | Constants, every enum, `Entity`/`Tile`/`Player`/`Game`, `Command`, all prototypes. Read this first, always. |
| `globals.cpp` | `Game g`, `simRand` (splitmix64), `STATS[]` (one row per EntityType — static_assert enforced), `CIVS[]`, era costs/names. |
| `main.cpp` | `initGame` pipeline (reset → civ/persona rolls → mapgen → spawns → wildlife), `simTick` (THE sim step), `runMatch` loop + lockstep gating, headless harnesses (`--verify`, `--test-raid`, `--net-*`, `--replay`), `main`. |
| `menu.cpp` | Everything before/between matches: splash, skirmish setup, battlefield picker, save/replay browsers, controls screen, MP host/join lobbies, `realm-config.txt` persistence. Never touches the sim. |
| `mapgen.cpp` | Map name generator, noise, climate skin/ecotones, per-climate feature passes (desert/steppe/moor), one generator per Layout, `generateMap()` dispatch. |
| `entity.cpp` | Time-of-day/season helpers, passability, spawn, fog (+cliff occlusion), depots/stockpiles, `tickEntity` state machine (incl. S_RAIDING), morale/rout/capture. |
| `pathfind.cpp` | A* + `moveAlongPath` (re-validates every step). |
| `combat.cpp` | Era/civ gates (`makeGate`, costs, train times), damage grammar (`DMG_TABLE`), orders, garrison, `killEntity`. |
| `world.cpp` | Passive ticks (towers/farms/markets/taverns/prisoners), seasons/winter/thaw, weather, `checkWin` (annihilation + sacred-site domination). |
| `ai.cpp` | Fog-honest intel (`aiKnows`), scouting rides, era-up, table-driven research, personas (Raider/Builder/Warlord), waves + plunder squads + site play, per-seat orchestration. |
| `commands.cpp` | The funnel (`applyCommand`), the shared codec (`encode/decodeCommand` — replay file AND network wire), replays, `simStateHash`, the `RESEARCH[]` table. |
| `net.cpp` | TCP lockstep transport, UDP LAN discovery, lobby handshake, D=3 scheduler, hash alarm, chat/pause/bye. Winsock port behind `_WIN32`. |
| `render.cpp` | Colours (incl. team + torchlight pairs), `getTerrainVisual` (seasons/night/glow), `renderMap` (+`rmPreparePass` masks: litMask intensity, wallGrid connectivity), overlays. |
| `ui.cpp` | Top bar, side panel + menus, minimap, help sheet, save/load overlay, net banners, post-match Chronicle. |
| `input.cpp` | All keys + mouse; every verb becomes a `pushCmd`. Modal handlers first, then the big switch. |
| `save.cpp` | Versioned binary save/load + `peekSave` for the slot browsers. |
| `sdl_shim.{h,cpp}` | The ncurses-shaped API over SDL2 for the standalone app: fonts, mouse synthesis, ACS glyph set. |

## Cookbooks

### Add a unit or building
1. `realm.h`: add to `EntityType` **in the right block** (units before
   `E_TOWNHALL`, buildings before `E_BRIDGE` — the `isUnit`/`isBuilding`
   range checks depend on it). Appending mid-enum breaks old saves: bump
   `SAVE_VERSION` + `REP_VERSION` + `NET_PROTO_VERSION`.
2. `globals.cpp`: insert the `STATS[]` row at the SAME position (the
   static_assert catches a miscount, not a misplacement — count rows!).
3. `combat.cpp`: era gate in `eraOf()`, any civ denial in `makeGate()`,
   food cost in `trainFoodCost()`, armour/damage class if it fights.
4. Train/build wiring: `input.cpp` (M_TRAIN_SELECT / M_BUILD_SELECT key)
   + `ui.cpp` (`menuRow` in the matching panel).
5. AI: give it a shopping line in `ai.cpp` if the AI should use it.
6. `make check`, then a PTY smoke test.

### Add a research
One row in `RESEARCH[]` (commands.cpp) — building, era, costs, key, name,
effect string — then implement the effect where it bites (`unitAtk`,
`damageVs`, `tickFarms`, `spawnEntity`...). Menus/keys/charges/AI all read
the table; nothing else to wire.

### Add a civilisation
`CIVS[]` row (globals.cpp) + `NUM_CIVS`… the enum in realm.h; then hooks:
`makeGate` (the denial), `costGoldOf`/`costWoodOf`/`trainTimeOf`,
gather-rate in entity.cpp, farm yield in world.cpp, muster HP in
`spawnEntity`. Civ picks travel in lobby CONFIG and the replay header —
`NET_PROTO_VERSION` + `REP_VERSION` bump if you change what's carried.

### Add a climate
Append to `Biome` (values are stored — append only). Then: skin cases in
`applyClimateSkin`, band in BOTH `pickClimate` and the continental inline
copy, a feature pass beside `applyDesertFeatures` (bail cheaply BEFORE
consuming simRand when absent!), name pools in `makeMapName`, UI slot in
`kClimBiome`/`kClimateNames` (menu.cpp), `biomeName` (ui.cpp), preview
colours. New terrain? Append to `Terrain` + `terrName` (static_assert) +
render pair + winter behaviour (`applyWinter`) + passability check.

### Add a layout
Append to `Layout`, write `generateXxxMap()` in mapgen.cpp (paint a
neutral template, end with `finishLayout()`), dispatch in `generateMap`,
add to `kLayoutNames` (menu.cpp) + `layName` (picker). Guarantee spawn
viability: open ground + never seal regions (carve lanes like Canyons).

### Change sim rules / hashed state
Expect the `make check` baseline hash to change — that's fine, it only
has to be IDENTICAL across two runs. Add new sim state to
`simStateHash()`, `saveGame`/`loadGame`, and reset in `resetMatchState`;
bump `SAVE_VERSION` and `NET_PROTO_VERSION`.

## Testing map

- `make check` — determinism + raid pipeline (run after everything).
- `--verify <seed> <ticks> <ais> <biome> <layout>` prints a per-seat
  probe (era/persona/army/raids/sites) — the balance-tuning tool.
- Lockstep: two processes, `--net-host` / `--net-join`, compare hashes.
- Replays: play a match, `--verify-replay <file>` twice.
- Menus: python pty scripts (see CLAUDE.md gotchas; use j/k).
- Sanitizers: `make clean && make CXXFLAGS="-std=c++17 -O1 -g
  -fsanitize=address,undefined ..."` then run the suite — this caught a
  real misaligned-read in the wire decoder.

## Design-history docs

`eras-civs-stockyards.md` (the big gameplay wave), `networking-plan.md` +
`multiplayer-roadmap.md` (lockstep design, all shipped), `ui-ux-audit.md`,
`combat-feel-proposals.md`, and the proposal docs for units/structures/
economy — deviations are recorded in each file's header.
