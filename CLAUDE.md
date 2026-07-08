# Realm — instructions for any assistant (and future me)

Medieval ASCII RTS. C++17, one Makefile, no dependencies beyond
ncursesw/SDL2. Read `docs/ARCHITECTURE.md` before touching anything —
it maps every file and has cookbooks for the common changes.

## The golden rules (breaking these corrupts saves, replays, multiplayer)

1. **Determinism is sacred.** All gameplay randomness goes through
   `simRand()`. Never `rand()`, clocks, pointers, or unordered-container
   iteration in anything that affects game state. Render/UI may use
   anything — visuals can't desync.
2. **The command funnel is the only door into the sim.** Input and AI
   express every verb as a `Command` (realm.h). New player verbs need a
   new `CmdType`, never a direct order call from input.cpp.
3. **Enum order is storage format.** `EntityType`, `Terrain`, `Biome`,
   `Layout` values are saved as ints — only APPEND new values (or accept
   breaking old saves and bump versions). `STATS[]` must match
   `EntityType` row-for-row (a static_assert now enforces the count).
4. **Version-bump checklist** when you change formats or sim rules:
   - Save layout changed → `SAVE_VERSION` (save.cpp)
   - Replay header/commands changed → `REP_VERSION` (commands.cpp)
   - ANY sim-rules or wire change → `NET_PROTO_VERSION` (net.cpp)
   - New hashed state → add to `simStateHash()` (commands.cpp) AND
     to save read/write; expect the verify baseline hash to change.
5. **After ANY change: `make check`.** Two identical hashes = the sim
   still reproduces. CI runs the same on Linux/macOS/Windows per push.

## Commands

```sh
make && make check         # build + the post-change gate
make gui-build             # SDL window build (realm-gui)
make app && make share     # macOS Realm.app + friend zip on ~/Desktop
make web                   # browser build -> web/index.html (needs emcc);
                           #   serve: python3 -m http.server 8080 -d web
./publish-web.sh           # make web + push to GitHub Pages (live site);
                           #   --relay wss://… also re-points the MP relay
./realm --verify S T N B L # headless: seed/ticks/AIs/biome/layout + probe
./realm --test-raid        # AI plunder pipeline end-to-end (exit 0 = pass)
./realm --test-sow         # player farm pipeline (sow 2x2 field, tend, bank)
./realm --net-host 3000 1  # + --net-join <ip> 3000 elsewhere: lockstep test
REALM_NET_PAUSE_TEST=1 ./realm --net-host 3000 1  # 12s mid-match stall must survive
./realm --replay <file>    # watch a recording (also in-game REPLAYS menu)
```

## Hard-won gotchas

- **Never edit by slicing between two text markers without asserting the
  slice's contents** — one such edit silently deleted the entity
  colour-selection block and shipped (units wore the terrain's colour).
- **Never `make` while a `--net-*` test pair is running** — macOS kills
  processes whose binary is replaced, and the stale half poisons the
  port for the next run (`pkill -f 'realm --net'` first).
- Menu/PTY testing: arrow escape sequences don't survive automation —
  every menu takes `j/k`; feed those. Method: python `pty.fork()` with a
  set winsize, strip ANSI, grep frame titles.
- Any modal input path that swallows `KEY_MOUSE` must still `getmouse()`
  the event, or stale MEVENTs replay later (queue-desync bug class).
- The SDL shim synthesizes mouse-position reports every 30ms — never
  treat a bare position report as user intent (see the cursor-drift fix
  in input.cpp).
- Mixed-seat rule: anything only the local player should see is gated on
  `g.localPlayer` / `g.humanMask`, never literal `0` — grep for the
  patterns before adding messages.

## Workflow expectations (user preference)

Fix → build → `make check` → commit → push to BOTH remotes
(`origin` and `ascii` = Realm-ASCII mirror) → refresh `make share` when
gameplay changed. Small commits, real messages. CI must stay green on
all three platforms.
