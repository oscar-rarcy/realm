# Refactor Roadmap (condensed)

The full, authoritative plan lives in `docs/implementation/refactor-plan.md`. This file is the
short working checklist and records the established conventions/safety net.

## Safety net (Phase 0)
- Build + tests: `make test` (Linux/macOS/WSL) or `mingw32-make test` (Windows/MSYS2 UCRT64).
- Deterministic harness: `initGameWithSeed(numAI, seed, humanCorner)` in
  `tests/realm_headless_tests.cpp`; covers spawn, resources, commands, ticking, save/load,
  `validateGameState()`, and mapgen invariants across seeds.
- Warnings: `-Wall -Wextra` always on (see `Makefile`).

## Architectural rules
- Platform code reads devices. Input code creates intents/commands. Command/domain code applies
  player + AI actions. Simulation advances time. Render observes. AI plans (does not mutate
  directly). Save serializes + migrates.
- No layer mutates global `g` except through its assigned boundary.

## Input policy (Phase 1.1)
- `X` = hold position during gameplay.
- `Q` = resign / return to main menu.
- `X` exits the application only on the game-over screen; otherwise exit via the menu.

## Progress
- [x] Phase 0 safety net (pre-existing harness) + roadmap.
- [x] Phase 1.1 `x`/`X` input conflict.
- [ ] Phase 1.2 wall-line build ownership/cost/validation.
- [ ] Phase 1.3 / 3.3 shared research service.
- [ ] Phase 3.1 production food cost in definitions.
- [ ] Phase 3.4 market trade service.
- [ ] Phases 2, 4–14 (larger structural work).
