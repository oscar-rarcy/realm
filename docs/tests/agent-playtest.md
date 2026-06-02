# Realm Agent Playtest Guide

Use this guide when an automated agent needs to play Realm, not just verify that
the canvas loads.

## Preferred Routes

- Production ASCII-only route: `https://edwardcoventry.com/apps/realm-ascii`
- Deterministic ASCII playtest route:
  `https://edwardcoventry.com/apps/realm-ascii/embed?seed=2468&corner=1&ais=1&biome=0`
- Direct Netlify ASCII route:
  `https://realm-edward.netlify.app/ascii/`

Prefer the deterministic embed route for repeatable agent playtests. It starts a
match immediately with a stable seed and stays in ASCII mode because the URL path
contains `realm-ascii`.

## Desktop ASCII Controls

On the title screen:

- `Enter`: start a match.
- `1`, `2`, `3`: choose opponent count.
- `T`, `D`, `S`, `W`, `F`, `V`, `C`, `0`: choose biome.

In a match:

- Arrow keys: move the map cursor.
- `Space`: select the unit or building under the cursor.
- `Enter`: command the selected unit at the cursor tile.
- `B`: build with a selected peasant.
- `T`: train from a selected production building.
- `A`: select military, or attack-move when military is selected.
- `.` or `,`: cycle to an idle peasant.
- `Q`: return to the main menu.
- `X`: exit or hold position depending on frontend context.

## Useful Smoke Actions

### Start A Match

1. Open `https://edwardcoventry.com/apps/realm-ascii`.
2. Wait for the title screen.
3. Press `Enter`.
4. Pass condition: the top HUD shows resources and population, and the map shows
   player `H` Town Hall tiles plus `p` peasant tiles.

### Select A Peasant

1. Start from a fresh match.
2. Use `.` or `,` to cycle to an idle peasant.
3. If the shortcut is not delivered by the browser automation layer, use the
   arrow cursor plus `Space` instead.
4. Pass condition: the right panel says `Peasant`, shows HP, and includes
   `[B] Build` plus `[Enter] Move/Gather`.

### Make A Peasant Walk

1. Select a peasant.
2. Move the cursor to a visible empty grass tile.
3. Press `Enter`.
4. Wait a few seconds.
5. Pass condition: the status line says `Moving...`, and the `p` glyph moves
   away from its original tile.

### Make A Peasant Gather

Use terrain resources, not sheep, for the first gathering smoke test.

1. Select a peasant.
2. Move the cursor to one of these visible resource tiles:
   - `$` for gold
   - `T` or `Y` for trees
   - `:` for berries
   - `%` for wheat
3. Press `Enter`.
4. Wait several seconds.
5. Pass condition: the status line reports a gathering verb such as
   `Mining gold...`, `Chopping wood...`, `Picking berries...`, or
   `Working wheat field...`; the peasant should move toward the target.

Do not use sheep as the primary gathering smoke test. Sheep and other animals
are entities, so command resolution treats them as visible non-player targets
first. A peasant may attack or chase the animal before any meat-gathering loop
can be observed.

### Train A Peasant

1. Move the cursor to the Town Hall and press `Space`.
2. Press `T`.
3. Press `P`.
4. Pass condition: the right panel shows Peasant training progress, resources
   decrease, and a new peasant appears after training completes.

## Evidence To Capture

For a useful playtest report, capture screenshots at these points:

1. Fresh match with Town Hall and peasants visible.
2. Peasant selected.
3. Peasant after a movement or gathering command.
4. Optional: training progress or completed new peasant.

Also record browser console errors. The known benign log is:

`emscripten_set_main_loop_timing: Cannot set timing mode for main loop since a main loop does not exist!`

Unexpected failures include missing WASM/assets, CSP errors, blank canvas, no
resource HUD after starting, or commands that leave the status line and glyph
positions unchanged.
