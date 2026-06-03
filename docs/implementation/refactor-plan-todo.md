Do a second cleanup pass for remaining SHOULD-REMOVE architecture debt.

Goal: finish converging the codebase around one render/query/rule/include architecture so future agents cannot edit stale duplicate paths.

Tasks:

1. Expand `RenderModel` so SDL and ASCII renderers no longer need direct `Game` entity queries for normal drawing.
   Include enough data for:
   - selected entities and primary selection,
   - hover/cursor tile stack,
   - build preview and build legality,
   - range rings,
   - minimap data,
   - action markers,
   - observer/local player visibility,
   - entity animation/facing/state display data.

2. Move mobile HUD command handling out of renderer code.
   - Renderer should report button IDs or UI intents only.
   - App/input/controller layer should translate those into typed commands.
   - Renderer should not own gameplay command construction beyond presentation-only UI.

3. Remove renderer-side query helpers once render-model coverage is sufficient.
   - Target helpers include patterns like `renderFindEntity`, `renderEntityAt`, `renderCanPlace`, and similar direct `Game + WorldIndex` render queries.
   - Replace with `RenderModel` fields or precomputed view data.

4. Replace broad `#include "realm.h"` dependencies with narrow module headers.
   - Start with core/domain/sim/AI/commands.
   - Keep umbrella includes only at platform/app boundary if still needed.
   - Remove any include that exists only because old globals or giant shared declarations were convenient.

5. Add architecture-enforcement checks.
   Add CI/grep/lint checks that fail on:
   - direct `g` access in `src/core`, `src/sim`, `src/ai`, and gameplay command modules,
   - direct gameplay entity mutation from render/mobile UI files,
   - new no-context overloads that build `WorldIndex` internally,
   - new duplicate command dispatch wrappers,
   - `#include "realm.h"` in forbidden layers,
   - global UI-status/event shims outside UI/platform glue.

6. Add a world-index performance regression test.
   - Track or instrument `WorldIndex` build count per tick.
   - Assert normal simulation ticks do not rebuild the index unexpectedly in hot loops.

Acceptance criteria:
- Renderers consume `RenderModel` for normal frame rendering.
- Mobile HUD no longer contains gameplay logic.
- Renderer query helpers are deleted or explicitly marked as short-lived transitional code with a removal target.
- Include boundaries are visibly narrower.
- CI prevents reintroducing direct globals, renderer mutations, no-context overloads, and duplicate dispatch paths.