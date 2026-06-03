# Save Format

Saves are text files beginning with `REALM_SAVE <version>`. Current saves use the latest schema version, and loading supports the configured minimum through `migrateLoadedGame()`.

Save/load commands route through `saveGameService()` and `loadGameService()`, which return structured results and emit `SaveCompleted` or `LoadCompleted` events. The serializer and parser remain in `src/sim/save_load.cpp`.

## Allowed dependencies

Save/load services may depend on `GameContext`, event sinks, save migration helpers, and serializer functions. Tests may load fixture paths and assert round trips or unsupported-version failures.

## Forbidden dependencies

The dispatcher should not own file I/O details. Load failures must not leave partially hydrated game state active. Future schema changes should use migration code instead of ad-hoc parser branches without comments.
