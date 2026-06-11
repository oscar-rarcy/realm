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
- Sim already ticks at a fixed 80ms cadence (`TICK_MS`), input is
  decoupled from the tick, and combat/entity logic was already rand-free.

## Rules to keep the sim deterministic (enforce in review)

1. No `rand()`, `random()`, `arc4random`, clocks, or pointer values in
   anything that affects game state. Only `simRand()`.
2. Don't iterate unordered containers in sim code where order affects
   outcomes (entity vector iteration order is part of the sim).
3. UI/render must never mutate game state directly — today some input
   paths do (acceptable single-player; see step 1 below).
4. Floats in the sim (`dayPhase`, `seasonPhase`, projectile positions,
   `getBrightness()`): fine while every player runs the same binary on
   the same arch; cross-platform play would need fixed-point or
   tick-derived integers. Defer until it matters.

## Remaining steps, in order

1. **Command funnel**: input.cpp currently calls `orderMove()` etc.
   directly. Introduce a small `Command {type, playerId, args}` struct;
   input *emits* commands, the tick *applies* them. Single-player becomes
   "local player's commands applied with zero delay".
2. **Apply AI through the same funnel** with its own playerId — AI then
   runs identically on all machines (it's part of the sim).
3. **State hash** (e.g., FNV over entity positions/hp/gold per N ticks)
   behind a debug flag — replay the same seed twice, compare hashes:
   instant desync detector, useful long before networking exists.
4. **Replays for free**: log seed + command stream to disk; playback is
   the same funnel. Ship this before networking — it debugs everything.
5. **Networking last**: exchange commands scheduled for tick T+delay
   (classic lockstep), pause when a peer's commands haven't arrived.
