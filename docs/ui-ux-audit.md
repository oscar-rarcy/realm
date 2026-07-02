# UI/UX audit — findings & status (2026-06-12)

A cleanliness pass over every surface the player touches. **Fixed** items
shipped in commit f36d5e6 (+ follow-ons); **Open** items are recommendations.

## Fixed this pass

| Finding | Fix |
|---|---|
| No way to leave a match without killing the app | Q Q now abandons to the splash; the splash owns app exit |
| No in-game key reference; bottom bar was the only teacher | `?` opens a full command sheet (sim pauses underneath) |
| Build bar crammed 16 buildings + keys into one truncated line | Build/Train menus render in the side panel with name + live cost per row; bottom bar just points there |
| Train costs hardcoded in three UI strings, drifting from orderTrain | `trainFoodCost()` is the single source; menus print STATS + it |
| Panel showed base ATK/RNG (wrong once research/shield-wall/ale apply) | Panel shows live `unitAtk`/`unitRange` + armour class/damage type |
| Hovering a bridge read past the terrain-name array (garbage/crash risk) | Table fixed + `static_assert` pins it to the Terrain enum |
| Status messages vanished in ~3 s, before long lines were readable | 4 s |
| "Rolling Hills" hid that hills are now the cliff ramps | "Hills (ramp)"; help sheet explains cliffs/ramps |
| Emoji entity table 4 entries out of alignment with the enum | Rebuilt + static_assert (commit 1957697) |

## Open recommendations — status (updated 2026-07-02)

1. **Message log — DONE** (earlier pass): `g.eventLog`, six-line rolling
   feed at the bottom of the side panel, newest highlighted.
2. **Idle-peasant button — DONE**: the top-bar `Idle:` readout is a gold
   reverse-video button while any peasant idles; clicking it jumps to the
   next idle peasant (same as `,`). Yields to the clock cluster on narrow
   terminals.
3. **Production rally visibility — DONE**: a bold gold `>` marks the rally
   tile while its building is selected (drawMapOverlays).
4. **Minimap viewport rectangle — DONE**: camera rect shown as an
   A_REVERSE border on the minimap.
5. **Confirmation grammar — DONE**: F9-F12 loads arm like Q-quit (second
   press within 40 ticks confirms).
6. **Selection panel scroll.** OPEN — long readouts on short windows are
   currently covered by the event log (drawn later); acceptable, revisit
   if it bites.
7. **Colour-blind build preview — DONE**: footprint glyphs are `+` (fits)
   / `x` (blocked), colour is now reinforcement not information.
8. **Mode strings / Esc.** Verified already unified since the audit: every
   mode's Esc exits to M_NORMAL keeping selection; Esc in M_NORMAL clears
   selection.

New this pass (multiplayer): `C` opens a chat line in network matches
(sent text lands in both event logs); network banners (waiting / paused /
connection lost / desync) render centre-top.
