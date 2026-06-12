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

## Open recommendations (rough priority)

1. **Message log.** One status line still loses information under message
   bursts (combat + season + weather in the same minute). A 3-line fading
   log above the bottom bar, or `L` to open recent history.
2. **Idle-peasant button parity.** `,`/`.` cycles idle peasants but nothing
   on screen advertises the idle count is clickable — make the top-bar
   `Idle:` readout a click target.
3. **Production rally visibility.** Rally points are invisible after
   setting; draw a small flag glyph on the rally tile while the building
   is selected.
4. **Minimap viewport rectangle.** The minimap shows units but not the
   current camera rectangle; a one-cell border makes orientation instant.
5. **Consistent confirmation grammar.** Q-quit needs a double press but
   F9 load (equally destructive to the current match) is instant —
   consider arming F-loads the same way.
6. **Selection panel scroll.** With the new depot/larder readouts a busy
   selection can exceed panel height on short windows; clamp + "…more".
7. **Colour audit for the colour-blind.** Player cyan vs P3 purple holds
   up, but red/green build-preview (OK/blocked) is the classic trap —
   add a glyph difference (`+`/`x`) not just colour.
8. **Mode strings.** M_BUILD_PLACE/M_RALLY_SET/M_PATROL_SET all explain
   themselves in the bar — good — but Esc behaviour differs subtly
   (some clear selection, some keep it). Unify: Esc exits the mode,
   second Esc clears selection.
