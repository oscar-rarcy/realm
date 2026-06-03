Audit and document the items that are acceptable to keep temporarily, making sure they are clearly isolated and cannot be mistaken for stale legacy paths.

Do not delete these unless you find they are genuinely unused or unsafe. Instead, quarantine and document them.

Acceptable temporary items:

1. `dispatchCommandForLocalGame`
   - Keep this as the single app-boundary command runner.
   - It should remain the only helper that builds a `WorldIndex`, creates `GameContext`, and calls `dispatchCommand` for local app/input use.
   - Do not create parallel SDL/input/mobile/gfx dispatch runners.

2. Global `gameEvents()` queue
   - Acceptable only as an app/UI boundary queue.
   - It must not be treated as a domain/service escape hatch.
   - Gameplay logic should use explicit `EventSink`/`GameContext`.

3. Save-format migration and old-save compatibility
   - Keep intentional save migration/backward compatibility.
   - Clearly separate old save-schema translation from live runtime architecture.
   - Old fields may be read during load/migration, but should not remain live authoritative runtime paths unless explicitly justified.

4. Separate SDL and ASCII renderers
   - Acceptable to keep both renderers.
   - Both should converge on the same `RenderModel`.
   - Do not allow each renderer to maintain separate gameplay/query/rule logic.

5. AI `aiIssue*` wrappers
   - Acceptable if they remain private AI command-builder helpers.
   - They should only queue typed `Command` payloads.
   - They must not mutate game state directly or bypass `dispatchCommand`.

6. Indexed query façade helpers
   - Helpers like `entityById`, `entitiesAt`, or other `WorldIndex`-based query functions are acceptable if they are the intentional query façade.
   - Avoid duplicate query helpers that rebuild `WorldIndex` internally or scan `game.entities` directly.

Required output:
- Add comments or architecture notes explaining why each temporary item is allowed.
- Add grep/CI checks preventing these temporary exceptions from expanding.
- For each item, state whether it is:
  - permanent boundary API,
  - temporary migration bridge,
  - dev/test-only helper,
  - or save-compatibility-only code.

Acceptance criteria:
- Temporary items are documented and isolated.
- No temporary item creates a second live implementation path.
- No save-compatibility code is used as live runtime architecture.
- Future coding agents can tell which paths are canonical and which are explicitly temporary.