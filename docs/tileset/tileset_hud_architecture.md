# Tileset HUD Architecture

## Goal

The tileset HUD is a separate desktop SDL HUD path for the graphical tileset mode. It must not change the ASCII desktop renderer, the terminal/ncurses renderer, or the mobile ASCII frame.

The desktop SDL tileset HUD is the default HUD whenever the game is in tileset visual mode. The existing top bar, right console panel, bottom command bar, and terminal-style views remain the HUD contract for ASCII and terminal rendering.

## Render Boundary

The switch point is in `src/render/sdl/gfx_renderer.cpp` after the map is drawn:

- `displayMode == DM_EMOJI` on desktop SDL: draw `drawTilesetHud(world)`.
- ASCII, mobile, terminal, and viewport-only paths: keep existing rendering.

The tileset HUD renderer lives in `src/render/sdl/tileset_hud_renderer.cpp`. It reads existing game state and input modes only. It does not introduce a new command model or a new game mechanic.

## Geometry Boundary

`src/render/sdl/camera.cpp` already exposes shared viewport and minimap geometry. When the tileset HUD is active:

- `mapRect()` returns the full window so the map can render underneath the right-side fade.
- `panelRect()` returns the floating right HUD overlay bounds.
- `miniMapRect()` returns the tileset HUD minimap rectangle so existing minimap input can be reused.

This keeps hit testing aligned with the tileset HUD while leaving ASCII and terminal geometry unchanged.

## Asset Boundary

Runtime HUD assets live under:

```text
assets/tiles/ui/hud/
```

Reference images stay under:

```text
art/reference/hud/
```

The asset loader has `tilesetLoadScreenUiTileScaled(...)`, which resolves screen UI assets from `assets/tiles/ui/` and uses the existing tileset PNG decode, scale, and cache path.

## Current Visual Contract

The visual target is the reference in `art/reference/hud/hud.png`:

- dark right-side fade, not a hard sidebar;
- compact icon-only system controls;
- resource counters without section headers;
- a small minimap embedded in the right-side overlay;
- action buttons as compact icon slots;
- selected unit information in a modest lower card;
- portrait/entity art allowed to break out of the card.

## Build Menu Scope

Build mode reuses the existing `M_BUILD_SELECT` and `M_BUILD_PLACE` modes. The current HUD shows a flat gallery from existing `BuildRule` entries and a resource before/after preview for gold and wood.

Skipped for now because it would require new UI mechanics:

- nested build categories;
- persistent hover/selection state independent of mouse position;
- repeated-placement policy changes;
- new economy resources such as stone.

The stone icon exists in the asset pack for visual completeness, but the current build service only spends gold and wood.

## Validation

Smallest useful checks after HUD work:

```sh
mingw32-make gfx
REALM_SMOKE_TEST=1 ./bin/realm.exe
```

Run ASCII comparison if the touched code is near shared geometry:

```sh
REALM_ASCII_COMPARE=1 ./bin/realm.exe
```
