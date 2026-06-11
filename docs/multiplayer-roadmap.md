# Multiplayer future-proofing

Goal: lockstep multiplayer (the AoE/StarCraft model). Nobody sends game
state over the wire — every machine runs the identical simulation and
only player *commands* are exchanged. That works iff the sim is
deterministic: same seed + same command stream ⇒ same game, bit for bit.

## Done

- **Deterministic sim RNG** (`simRand()` / `seedSimRng()`, splitmix64,
  state lives in `Game.rngState` and is saved/loaded — save v4).
  All gameplay randomness goes through it; `rand()` is banned from sim
  code. render/ui may use anything — visuals can't desync.
- Sim ticks at a fixed 80ms cadence (`TICK_MS`); the whole step lives in
  `simTick()` (main.cpp), shared verbatim by the game loop, replay
  playback, and the headless verifier.
- **Command funnel** (`commands.cpp`): input never mutates the sim.
  Every verb — orders, build, train, research, trades, rally, gates,
  trebuchet pack, even the debug reveal — becomes a
  `Command {type, player, x/y, target, arg, units[]}` pushed onto
  `g.pendingCmds`; the tick applies the queue. Commands carry explicit
  unit ids: selection/camera are UI-local and never reach the sim.
- **AI through the same funnel**: ai.cpp issues Commands with its own
  playerId via `applyCommand()` (immediate — the AI runs *inside* the
  sim, so its commands are re-derived on every machine and never need
  recording). Direct field writes that remain in ai.cpp (research cheat,
  trebuchet pack micro) are sim-internal behaviour, not orders.
- **State hash** (`simStateHash()`, FNV-1a over entities + players +
  RNG state). `REALM_HASH=1` logs tick/hash to realm-hash.log every 100
  ticks. Headless checks, no terminal needed:
  - `./realm --verify <seed> [ticks] [ais]` — run sim, print final hash.
    Run twice, compare: instant desync detector.
  - Verified: 13,000 ticks (full seasonal year, 3 AIs at war) replays
    hash-identical.
- **Replays**: every interactive match records header
  (seed/AIs/biome) + the human command stream to
  `replays/realm-<timestamp>.rep` (AI re-derives, so only human commands
  are stored). Loading a save mid-match stops the recording (the stream
  would no longer reproduce from the seed).
  - `./realm --replay <file>` — watch a recording; camera/selection are
    live, your orders are inert.
  - `./realm --verify-replay <file> [ticks]` — headless playback hash.

## Rules to keep the sim deterministic (enforce in review)

1. No `rand()`, `random()`, `arc4random`, clocks, or pointer values in
   anything that affects game state. Only `simRand()`.
2. Don't iterate unordered containers in sim code where order affects
   outcomes (entity vector iteration order is part of the sim).
3. UI/render must never mutate game state — everything goes through the
   command funnel. `pushCommand` from input, `applyCommand` from sim
   code. New player verbs need a new CmdType, not a direct call.
4. Floats in the sim (`dayPhase`, `seasonPhase`, projectile positions,
   `getBrightness()`): fine while every player runs the same binary on
   the same arch; cross-platform play would need fixed-point or
   tick-derived integers. Defer until it matters.

## Remaining

5. **Networking**: exchange commands scheduled for tick T+delay
   (classic lockstep), pause when a peer's commands haven't arrived.
   Everything it needs now exists: commands are serialisable (replay
   format), the sim is verified deterministic, and `simStateHash()` is
   the in-game desync alarm. Open decisions before building it:
   host/join UX on the splash screen, TCP vs ENet-style UDP, command
   delay (2-4 ticks), and how the lobby shares seed + biome + AI count
   (same fields as the replay header).
