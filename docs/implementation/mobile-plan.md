# Realm Mobile GUI Specification

## 1. Scope

This specification covers the **mobile version of the GUI renderer** for Realm.

Mobile mode must **not affect the console version** of the game. The terminal/console build should continue to behave as it currently does, including keyboard input, ASCII display, ncurses-style rendering, and any existing terminal-specific UI.

The mobile layout, touch controls, responsive panels, and mobile menu changes apply only to the **custom GUI renderer** and any future web/native GUI frontend that uses that renderer.

## 2. Core Design Principle

Mobile should use a strict **two-panel layout**:

1. **Game viewport**
2. **HUD/control panel**

There should not be separate top bars, bottom shortcut bars, or floating overlays that eat into the game viewport. On mobile, space is limited, so the layout should remain clean and predictable.

The game area and the controls should be clearly separated.

## 3. Orientation Layouts

### 3.1 Portrait Layout

In portrait mode, the screen is split vertically:

```text
┌────────────────────┐
│                    │
│    Game viewport    │
│                    │
├────────────────────┤
│      Mobile HUD     │
│ Resources           │
│ Selection info      │
│ Minimap             │
│ Command buttons     │
│ Menu / utilities    │
└────────────────────┘
```

The game viewport occupies the upper area. The HUD occupies the bottom area.

The HUD should be tall enough to contain the essential controls without requiring tiny buttons. If necessary, less important HUD sections may become collapsible or swipeable.

### 3.2 Landscape Layout

In landscape mode, the screen is split horizontally:

```text
┌────────────────────┬──────────────┐
│                    │ Resources    │
│                    │ Minimap      │
│   Game viewport     │ Selection    │
│                    │ Commands     │
│                    │ Utilities    │
└────────────────────┴──────────────┘
```

The game viewport occupies the left side. The HUD occupies the right side.

There should be no separate top bar or bottom bar in landscape. Resources, minimap, selected unit information, command buttons, pause/menu controls, and alerts all belong inside the right-side HUD column.

## 4. Responsive Behaviour

The GUI renderer should choose the layout based on available screen dimensions:

* **Portrait / narrow screen:** game on top, HUD on bottom.
* **Landscape / wide screen:** game on left, HUD on right.
* **Desktop GUI:** existing desktop layout may remain separate unless it makes sense to share the same responsive layout system.

Orientation changes should preserve current game interaction state:

* camera position
* selected units/buildings
* current command mode
* current build placement mode
* minimap viewport
* zoom level
* paused/menu state

Rotating the device should reflow the UI, not reset the game state.

## 5. HUD Contents

The mobile HUD should contain the following sections.

### 5.1 Resources Section

A compact resources row or grid showing:

```text
Food 120
Wood 80
Gold 40
Stone 20
Pop 8/15
```

This replaces any desktop-style top resource bar on mobile.

The resource display should be compact, readable, and always visible.

### 5.2 Selection Summary

The HUD should show the current selection.

Examples:

```text
No selection
Tap a unit or building
```

```text
4 Peasants selected
3 idle, 1 gathering
```

```text
Town Center
Training: Peasant 40%
```

For multiple selected units, show a summary rather than trying to display every unit individually.

The selection summary should show essential state only:

* unit/building type
* count
* health if useful
* current task
* carried resource for workers
* production queue for buildings

### 5.3 Command Grid

Keyboard shortcuts should not be shown on mobile.

All commands must be exposed through large tappable buttons.

Example command grid:

```text
Move     Gather     Build
Attack   Stop       Patrol
```

For workers, the Build button opens a build submenu.

Example build submenu:

```text
House    Farm       Barracks
```

For buildings, the command area should show production and building-specific commands.

Example Town Center commands:

```text
Train Peasant
Set Rally Point
Cancel Queue
```

Buttons should be large enough for touch input. As a target, use a minimum touch size of about **44–48 px**, preferably larger where space allows.

### 5.4 Minimap

The minimap belongs inside the HUD panel.

In portrait mode, it may sit beside the selection card or command grid if space allows. Otherwise, it can be a collapsible HUD section.

In landscape mode, it should sit near the top of the right-side HUD column.

Minimap behaviour:

* tap minimap: move camera to that area
* drag on minimap: pan camera
* show current viewport rectangle
* show player base, resources, terrain, and alerts in simplified form

### 5.5 Utility Buttons

The HUD should include persistent utility controls:

```text
Menu
Pause
Idle Villagers
```

Optional additional utility buttons:

```text
Idle Military
Select Army
Cycle Town Centers
Last Alert
```

These replace common keyboard shortcuts that are unavailable on mobile.

## 6. Touch Control Model

Mobile should not try to copy desktop mouse controls directly.

There is no hover, no right-click, and no keyboard shortcut layer. The touch model should be explicit and forgiving.

### 6.1 Recommended Gesture Table

| Gesture                                     | Behaviour                                              |
| ------------------------------------------- | ------------------------------------------------------ |
| Tap own unit/building with nothing selected | Select                                                 |
| Tap own unit/building with units selected   | Select target, or interact if command mode requires it |
| Tap ground with controllable units selected | Move                                                   |
| Tap enemy with combat units selected        | Attack                                                 |
| Tap resource with worker selected           | Gather                                                 |
| Tap command button                          | Enter that command mode                                |
| Tap map during command mode                 | Execute selected command                               |
| Long press unit/tile/button                 | Inspect or show tooltip                                |
| Drag empty map                              | Pan camera                                             |
| Pinch                                       | Zoom, if supported                                     |
| Double tap unit                             | Select nearby units of same type                       |
| Tap empty ground with nothing selected      | Do nothing or clear selection                          |

### 6.2 Single Tap

Single tap is the primary interaction.

When nothing is selected:

* tapping an owned unit selects it
* tapping an owned building selects it
* tapping neutral or enemy objects may inspect/select them if supported
* tapping empty terrain does nothing

When controllable units are selected:

* tapping empty terrain issues a move command
* tapping an enemy issues an attack command
* tapping a resource with workers selected issues a gather command
* tapping a valid structure target issues the relevant contextual command

### 6.3 Avoid Double Tap as Core Command Input

Double tap should not be the main command gesture.

Reasons:

* it adds delay
* it is easy to misfire
* it is less discoverable
* it conflicts with normal mobile expectations
* it is worse for accessibility

Double tap is acceptable as a convenience gesture, especially for selecting nearby units of the same type.

Example:

```text
Double tap a peasant → select nearby peasants
```

### 6.4 Long Press

Long press should be used for secondary information, not essential gameplay.

Good uses:

* inspect tile
* inspect unit/building/resource
* show tooltip for command button
* preview range/path
* open contextual details

Long press should not be required for basic moving, attacking, gathering, or building.

### 6.5 Drag and Zoom

Drag should pan the camera.

Pinch zoom should be supported if the GUI renderer can support zoom. If smooth pinch zoom is too large a first implementation, support discrete zoom levels instead.

Zoom should affect the game viewport only, not the HUD.

## 7. Command Modes

Some actions should use explicit temporary command modes.

Examples:

```text
Select worker → tap Build → tap House → choose location
```

```text
Select soldiers → tap Attack → tap enemy
```

```text
Select Town Center → tap Set Rally Point → tap map location
```

Command modes should have visible feedback in the HUD and/or viewport.

Example:

```text
Placing House
Tap a valid tile to place
Cancel
```

Every command mode must have a clear cancel path.

Cancel options:

* tap Cancel in HUD
* tap selected command again
* tap menu/back button
* use Escape on desktop GUI if available

## 8. Ambiguous Tap Rules

Mobile input needs deterministic priority rules.

Recommended rule:

> If controllable units are selected, tapping the map issues the most likely contextual command. To inspect something instead, long press it.

Examples:

* worker selected + tap tree = gather wood
* soldier selected + tap enemy = attack
* unit selected + tap ground = move
* no unit selected + tap object = select or inspect
* no unit selected + tap ground = no action

When several entities are near a tap, target priority should be:

1. selected command target, if in command mode
2. owned controllable unit
3. owned building
4. enemy unit/building
5. resource
6. neutral animal/object
7. terrain

The GUI should use a tap tolerance radius so small units remain tappable on mobile.

## 9. Build Placement

Build placement needs special handling because accidental taps are common on touch screens.

Recommended flow:

1. Select worker.
2. Tap Build.
3. Tap building type, such as House.
4. Enter placement mode.
5. Show building ghost/preview on the map.
6. Valid tiles show a positive highlight.
7. Invalid tiles show blocked/red feedback.
8. Tap a valid location to place.
9. HUD offers Cancel while in placement mode.

Do not instantly place a building on the first accidental map tap after selecting the building type unless the preview state is very clear.

## 10. Selection and Feedback

Mobile needs strong visual feedback because there is no hover state.

Required feedback:

* clear selection outline around selected units/buildings
* selected group indicator
* destination marker after move command
* dotted path or brief move confirmation
* command mode label
* invalid command feedback
* build placement preview
* resource target feedback for gather commands
* attack target feedback for attack commands

Example HUD confirmation:

```text
Moving 4 Peasants
```

```text
Gathering Wood
```

```text
Cannot build there
```

## 11. Main Menu on Mobile GUI

The mobile GUI main menu should be fully tappable.

Do not show keyboard shortcut hints on mobile.

Desktop-style menu:

```text
[N] New Game
[L] Load Game
[Q] Quit
```

Mobile GUI menu:

```text
New Game
Load Game
Settings
Help
Quit
```

Buttons should be large and touch-friendly.

The menu should support both portrait and landscape.

Suggested mobile settings:

* orientation: auto / portrait / landscape
* UI scale
* zoom level
* touch controls help
* minimap tap-to-pan on/off
* command confirmation on/off
* edge scrolling off/on, default off on mobile

## 12. Alerts

Mobile should have a compact alert system because keyboard shortcuts are unavailable.

Examples:

```text
Under attack
House complete
Idle peasant
Not enough wood
```

Alerts should appear inside the HUD, not as intrusive overlays over the game viewport.

Tapping an alert should move the camera to the relevant location where applicable.

## 13. Mobile Web / GUI Runtime Considerations

If the GUI renderer is compiled to web or used in a browser, it should handle mobile browser behaviour properly.

Requirements:

* use pointer events rather than mouse-only events
* disable unwanted page scrolling while interacting with the game
* prevent accidental double-tap page zoom
* prevent long-press text selection/context menus inside the game area
* respect safe areas, notches, rounded corners, and gesture bars
* handle dynamic browser viewport height changes
* pause or handle focus loss when the app is backgrounded

For single-player mobile GUI, the game should probably pause when the app loses focus.

## 14. Architecture Requirements

Mobile mode should be implemented as a GUI-layer feature.

It should not change:

* core simulation logic
* unit behaviour
* AI behaviour
* map generation
* console rendering
* terminal input handling
* keyboard command behaviour in the console version

Recommended separation:

```text
Core game simulation
    shared by console and GUI

Console renderer/input
    unchanged

GUI renderer/input
    gains responsive layout and touch support

Mobile GUI layout
    portrait and landscape two-panel layouts

GUI input abstraction
    maps pointer/touch events into game commands
```

The mobile GUI should translate taps, long presses, drags, command buttons, and minimap interactions into the same underlying command system used by the rest of the game where possible.

## 15. Implementation Phases

### Phase 1 — Layout Foundation

* Add GUI-only mobile layout mode.
* Detect portrait versus landscape.
* Implement two-panel responsive layout.
* Move mobile resources/status into the HUD panel.
* Ensure console build is unaffected.

Acceptance criteria:

* portrait shows game above HUD
* landscape shows game beside HUD
* no mobile-specific code affects console mode
* resizing/orientation change reflows UI without crashing

### Phase 2 — HUD Components

* Add resources section.
* Add selection summary card.
* Add command grid.
* Add minimap inside HUD.
* Add utility buttons: Menu, Pause, Idle Villagers.
* Hide keyboard shortcut labels in mobile GUI.

Acceptance criteria:

* selected units/buildings show correct mobile HUD state
* command buttons are tappable
* minimap displays current viewport
* keyboard shortcuts are not displayed in mobile mode

### Phase 3 — Touch Input

* Add pointer/touch input abstraction for GUI.
* Implement tap-to-select.
* Implement tap-to-command for move, attack, gather.
* Implement drag-to-pan.
* Implement long-press inspect/tooltips.
* Add double-tap same-type selection as optional convenience.

Acceptance criteria:

* user can select units by tapping
* selected units can be moved by tapping ground
* workers can gather by tapping resources
* soldiers can attack by tapping enemies
* camera can be panned with touch drag
* long press does not trigger accidental commands

### Phase 4 — Command Modes

* Add explicit command mode state.
* Implement Build mode.
* Implement Attack mode if needed.
* Implement Set Rally Point mode for production buildings.
* Add Cancel behaviour.
* Add visible command-mode feedback.

Acceptance criteria:

* build placement is possible using touch only
* invalid placement gives clear feedback
* command mode can always be cancelled
* rotating device preserves command mode

### Phase 5 — Mobile Menu and Settings

* Add touch-first main menu for GUI mobile.
* Remove keyboard shortcut labels from mobile menu.
* Add mobile-specific settings.
* Add touch controls help screen.

Acceptance criteria:

* all mobile menus are usable without keyboard
* menu buttons are large enough for touch
* settings persist where appropriate

### Phase 6 — Polish and Edge Cases

* Add safe-area padding.
* Improve tap target tolerance.
* Improve selection highlights.
* Add alert stack.
* Add focus-loss pause behaviour.
* Test on narrow portrait, wide portrait, phone landscape, tablet landscape, and desktop GUI.

Acceptance criteria:

* game is playable on phone portrait
* game is playable on phone landscape
* HUD does not cover the game viewport
* no important UI is hidden behind notches or gesture bars
* console version still builds and behaves as before

## 16. Testing Checklist

### Console Regression

* console build still compiles
* console input still works
* console rendering unchanged
* keyboard shortcuts still work in console
* no mobile HUD appears in console mode

### GUI Desktop Regression

* desktop GUI still works
* existing mouse controls still work
* existing desktop layout still works, unless intentionally replaced
* keyboard shortcuts may remain visible on desktop GUI if currently supported

### Mobile Portrait GUI

* game viewport appears above HUD
* HUD contains resources, selection, commands, minimap, utilities
* command buttons are touch-sized
* no keyboard shortcut labels shown
* tap selection works
* tap movement works
* tap gather works
* tap attack works
* build placement works
* minimap tap/drag works
* menu is tappable

### Mobile Landscape GUI

* game viewport appears on left
* HUD appears on right
* no separate top/bottom bars
* resources are inside HUD
* minimap is inside HUD
* selection and command buttons are inside HUD
* utility buttons are inside HUD
* controls remain touch-sized

### Orientation Change

* selected units persist
* camera persists
* zoom persists
* command mode persists
* build placement preview persists or cancels cleanly
* no UI elements overlap incorrectly

### Touch Edge Cases

* tapping near several entities selects the correct priority target
* long press shows inspect/tooltip without issuing command
* double tap selection does not interfere with normal tap commands
* drag panning does not accidentally issue move commands
* invalid commands give clear feedback

## 17. Non-Goals

The first mobile GUI pass does not need to:

* redesign the console version
* remove or replace terminal controls
* implement multiplayer mobile UX
* implement every desktop hotkey as a mobile button
* create a completely new game simulation
* redesign AI or game balance
* create a final art style

The purpose is to make the existing GUI version playable on mobile through a clean responsive layout and touch-first controls.

## 18. Summary

Mobile GUI mode should be a responsive, touch-first version of the existing GUI renderer.

The key rule is:

> Mobile has two panels only: game viewport and HUD. Portrait splits vertically. Landscape splits horizontally.

The key control rule is:

> Tap selects or issues contextual commands. Command buttons create explicit command modes. Long press is for inspect/tooltips. Double tap is optional convenience, not core input.

The key architecture rule is:

> Mobile mode is GUI-only. The console version must remain unchanged.
