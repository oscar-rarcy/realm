Realm graphical renderer
========================

This keeps the existing ncurses terminal game available on Unix-like platforms
and adds an SDL2/SDL_ttf graphical renderer that still looks like a console/grid UI.

Terminal build on Linux/macOS/WSL
---------------------------------

    make clean
    make terminal
    ./bin/realm

Graphical build on WSL/WSLg
---------------------------

One-time dependency install:

    sudo apt update
    sudo apt install -y build-essential pkg-config libncursesw5-dev libsdl2-dev libsdl2-ttf-dev libpng-dev

Build/run:

    make clean
    make gfx
    ./bin/realm-gfx

Graphical build on native Windows with MSYS2 UCRT64
--------------------------------------------------

Open the "MSYS2 UCRT64" shell, not plain PowerShell and not the MSYS shell.
Then run:

    pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-libpng

Build/run:

    cd /c/Users/Edward/Code/oscar/realm
    mingw32-make clean
    mingw32-make gfx
    ./bin/realm.exe

On native Windows, the build copies the required MSYS2 runtime DLLs into the
`bin/` folder beside `realm.exe`. That lets `bin/realm.exe` run from PowerShell or
Explorer without requiring the MSYS2 UCRT64 shell on PATH.

Runtime smoke log
-----------------

The GUI executable writes `realm-run.log` in the working directory. A successful
launch to the splash screen includes:

    realm: process started
    realm: gfxInit ok
    realm: entering main screen
    realm: main screen ready

For an automated runtime check that exits after the splash screen renders:

    $env:REALM_SMOKE_TEST = "1"
    .\bin\realm.exe

For an automated match smoke that starts a deterministic match, runs simulation
for 60 ticks, renders SDL frames, and exits:

    $env:REALM_SMOKE_TEST = "match"
    $env:REALM_SEED = "2468"
    $env:REALM_HUMAN_CORNER = "1"
    $env:REALM_BIOME = "0"
    .\bin\realm.exe

Reproducibility and diagnostics
-------------------------------

Normal startup remains random. For a reproducible match, set:

    REALM_SEED=12345
    REALM_HUMAN_CORNER=0
    REALM_BIOME=0
    REALM_DIAGNOSTICS=1

Match start logs the seed, AI count, human corner, biome, entity count, and
projectile count to `realm-run.log`. In the SDL renderer, `F8` toggles the
diagnostics panel during play.

ASCII comparison captures
-------------------------

To compare the GUI ASCII renderer with the terminal-style reference on the same
deterministic game states:

    make ascii-compare

or run the GUI binary with:

    REALM_ASCII_COMPARE=1 ./bin/realm.exe

The flow writes paired screenshots and reference text dumps to
`build/ascii-compare`. Each `*-gui-ascii.bmp` is generated through the GUI path;
each matching `*-terminal-reference.bmp` and `.txt` is generated from the same
terminal grid model, seed, cursor, selection, and HUD state.

Runtime controls added by hardening
-----------------------------------

- `F5` saves to `realm-save.txt`.
- `F9` loads from `realm-save.txt`.
- `F8` toggles diagnostics.
- `F11` or `Alt+Enter` toggles fullscreen in the SDL GUI.
- `Q` resigns and returns to the main menu during a match. `X` exits the native
  SDL app; web builds ignore `X` because the browser tab owns exit.
- `?` toggles the shared help overlay.
- The right panel always shows cursor tile terrain, biome, resource amount, and
  visible stack information.
- GUI text options that mirror keyboard choices can be clicked. They underline
  on mouse hover in the splash screen, side panel, bottom command bar, and ASCII
  terminal HUD. Mobile command buttons use the same hover treatment when driven
  with a mouse and remain tap targets on touch devices.
- The help overlay documents shared keyboard/mouse commands, SDL-only
  zoom/pan controls, food sources, winter starvation, owner colours,
  neutral animals, and combat alerts.
- Temporary command markers appear on empty target tiles after move, gather,
  attack, build, or rally-style orders.
- Train mode stays open after queueing a unit; repeat unit keys to queue more,
  `Esc` cancels.

Tileset symbol fallback
-----------------------

The GUI exposes two visual modes: ASCII and Tileset. ASCII uses the terminal-style
text grid. Tileset is always isometric. Until real image tiles are added, Tileset
uses the existing symbol/emoji placeholders where defined and falls back to the
same one-character ASCII glyph used by the terminal renderer when no tile symbol
is defined.

The current placeholder symbol path explicitly searches for Segoe UI Emoji at:

    C:/Windows/Fonts/seguiemj.ttf
    /mnt/c/Windows/Fonts/seguiemj.ttf
    /c/Windows/Fonts/seguiemj.ttf

So it works in native Windows, MSYS2 and WSL. At startup, the GUI renderer prints
the text font and tileset symbol font paths it actually loaded. If no symbol font
is found, it uses ASCII glyph fallbacks instead of drawing boxes.

Missing tileset audit
---------------------

Local Tileset runs write a missing-tile manifest for future asset generation.
Native/local GUI runs write:

    build/missing-tiles.log

Local browser runs on `localhost`, `127.0.0.1`, or `::1` mirror the same entries
to browser local storage under:

    realm.missingTilesLog

Each line includes the tile kind, stable key, display name, current fallback
glyph, and suggested future asset path. Deployed web builds do not emit this
audit log. Set `REALM_TILESET_AUDIT=0` to disable it locally.

Local tileset lab
-----------------

The native SDL build has a local-only tileset lab:

    mingw32-make lab
    ./bin/realm-lab.exe

or, on Unix-like systems:

    make lab
    ./bin/realm-lab

The lab renders a controlled single-tile preview through the same SDL renderer
helpers used by the game: isometric diamond projection, terrain tinting,
season/weather/time-of-day, fog visibility, torch/candle light, entity glyph
fallbacks, and the ASCII terminal cell model. It also enables native PNG sprite
loading for the preview.

The lab starts with no entity selected. Use the mouse on the left panel to open
dropdowns for terrain, biome, season, time, weather, fog, light, resources,
entity, action, direction, frame, owner, and animation speed. Drag the hue wheel
to change the team colour. Keyboard shortcuts remain available for fast
iteration.

Entity sprites are discovered by convention:

    assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_base.png
    assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_teammask.png

The base layer is drawn as neutral art. The optional team mask is composited with
the chosen lab team colour, using the mask luminance as shade. Missing sprites
show a checker placeholder and the lab panel reports the expected base/mask
paths instead of failing.

Run the non-interactive smoke with:

    REALM_LAB_SMOKE=1 ./bin/realm-lab.exe

It writes verification captures to `build/lab-screenshots`, including the
default no-entity state, tile-only, combined peasant, team-colour variants,
missing-placeholder, night/candle, and ASCII preview scenarios.

To open the repeated grass-map preview in the lab:

    REALM_LAB_GRASS_MAP=1 ./bin/realm-lab.exe

That mode draws the accepted runtime grass tile across an unbounded isometric
field for pan/zoom inspection. Use the mouse wheel or `+`/`-` to zoom, drag to
pan, `S` to write `build/lab-screenshots/grass-map-preview.bmp`, and `Esc` to
quit. For a non-interactive capture:

    REALM_LAB_GRASS_MAP=1 REALM_LAB_GRASS_MAP_SMOKE=1 ./bin/realm-lab.exe

PNG image tiles in the native normal game are enabled automatically whenever the
GUI is in Tileset visual mode. `REALM_IMAGE_TILESET=1` is still accepted as a
legacy/test override, but it is no longer required for native Tileset mode.

Texture size handling is automatic by default. Keep one reviewed source PNG per
runtime asset unless a sprite has explicitly gone through the zoom-stop pipeline.
The SDL tileset loader decodes each PNG once, then builds and caches draw-size
texture variants on demand. This is Realm's SDL2 equivalent of using mipmaps:
zoomed-out ground tiles are area-resampled before projection, entity sprites can
be cached at the actual draw size, and the renderer reuses those textures until
the tileset cache is cleared.

Runtime PNGs should remain source-quality images, so close zoom draws down from
real source detail instead of enlarging tiny draw-size crops. The range policy is
defined in `scripts/tileset_resolution_policy.py`:

- Grounds: 512 px minimum, 1024 px preferred target.
- Sprite-like generated sources target about 256 px per contact-sheet slot or
  standalone pre-crop source. This covers units, animals, decals, projectiles,
  effects, UI markers, and many feature sprites.
- Buildings target about 256 px per footprint tile on the largest footprint
  axis before crop, capped at 1536 px. A 2 by 2 House therefore has a 512 px
  pre-crop target, while a 3 by 3 Town Hall has a 768 px pre-crop target.

Generation size is not meant to force every sheet to one exact canvas. For
example, a 4 by 4 actor contact sheet can be 1024 px, 1254 px, or another clean
size if each slot has enough source detail. Runtime promotion may crop
transparent margins, so cropped sprites are checked with a longest-side floor
rather than an exact 256 by 256 canvas requirement. Do not promote ordinary
runtime art by shrinking it to 32, 48, or another tiny draw-size PNG first; that
makes close zoom enlarge an already-lossy sprite while neighbouring ground tiles
still draw from source-quality images.

Zoom-stop entity sprites are the exception for tiny AI-redrawn actor art. They
are optional runtime overrides that live beside the normal frame:

    assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_zoom_NNN_base.png
    assets/tiles/entities/<entity>/<action>/<direction>/frame_XX_zoom_NNN_teammask.png

`NNN` is the square draw size in pixels. The sheet/status helper reads the
renderer zoom ladder from `src/render/sdl/camera.cpp`; the current tileset
ladder has 16 stops from tile size 14 to 288, producing entity sprite sizes
21 to 446 with the default `entity_tile_zoom_1_55` scale. The loader uses an
exact stop for the requested draw size, then falls back to the normal
`frame_XX_base.png` and
`frame_XX_teammask.png` path if no stop exists. This keeps zoom-stop work
incremental: missing stops never blank gameplay.

The helper for this workflow is:

    python scripts/prepare_zoom_stop_sprite_sheet.py status --entity peasant --action idle --direction front --frame 0

It reports whether the base and team-mask zoom-stop files exist for a frame.

For focused visual QA of newly added ground tiles, run with:

    REALM_TILESET_TEST_MAP=1 ./bin/realm.exe

This creates a paused, non-playable test map made from the currently accepted
grass tile, surrounded by never-explored unknown tiles. It is intended for
checking ground/decal/sprite wiring without unrelated mapgen terrain.

For focused night lighting QA around the starting town hall, run with:

    REALM_UI_NIGHT_LIGHT_TEST=1 ./bin/realm.exe

It starts a deterministic Tileset-mode match at midnight, centers the camera on
the player town hall, and writes `build/night-light-screenshots/01-night-townhall-light.bmp`.
Use `REALM_SEED`, `REALM_HUMAN_CORNER`, `REALM_BIOME`, and
`REALM_UI_TEST_ZOOM` to vary the fixture.

To start a normal manual match at a specific time, set `REALM_START_DAY_PHASE`.
For example, `REALM_START_DAY_PHASE=0 ./bin/realm.exe` starts new matches at
midnight.

Build separation
----------------

The graphical target now compiles its own *_gfx.o object files with
-DUSE_SDL_RENDERER and does not link ncurses. The normal terminal target still
links ncursesw and should keep behaving as before.


Mobile GUI mode
---------------

The SDL GUI renderer switches to a touch-first mobile layout on narrow portrait
windows, short landscape windows, or when `REALM_MOBILE_GUI=1` is set. The
terminal/ncurses renderer is unchanged.

Mobile mode uses two panels only:

- portrait: game viewport above the HUD
- landscape: game viewport left of the HUD

The mobile HUD contains resources, selection status, minimap, command buttons,
and Menu/Pause/Full/Idle controls. Keyboard shortcut labels are hidden in this mode.
Touch-style input maps to existing game commands: tap selects or commands, drag
pans the map, long press inspects, minimap tap/drag pans the camera, and Build
uses an explicit placement preview with Cancel.

ASCII mobile keeps those touch controls but renders them as terminal-style
options, with a text-grid map, console HUD, and console-styled splash/settings/help
screens. Tileset mobile keeps the shaded isometric mobile HUD. The UI screenshot
suite writes the ASCII mobile captures as `19-mobile-ascii-menu.bmp`,
`20-mobile-ascii-portrait-hud.bmp`, `21-mobile-ascii-portrait-build-menu.bmp`,
and `22-mobile-ascii-landscape-hud.bmp`.

The GUI test harness writes mobile layout captures in addition to desktop
captures:

- `build/ui-screenshots/16-mobile-portrait-hud.bmp`
- `build/ui-screenshots/17-mobile-portrait-build-menu.bmp`
- `build/ui-screenshots/18-mobile-landscape-hud.bmp`


Cross-platform build behaviour
==============================

The Makefile is now frontend-oriented rather than platform-oriented:

- `make` builds the GUI renderer by default on every platform.
- `make gui` or `make gfx` explicitly builds the GUI renderer.
- `make run` builds and runs the GUI renderer.
- `make terminal` builds the ncurses terminal renderer on Linux/macOS/WSL only.
- Native Windows intentionally builds GUI only. The terminal renderer is still available through WSL.

Expected output binaries:

- Windows native/MSYS2: `bin/realm.exe`
- Linux/macOS/WSL GUI: `bin/realm-gfx`
- Linux/macOS/WSL terminal: `bin/realm`

Native Windows/MSYS2 UCRT64:

    mingw32-make clean
    mingw32-make
    ./bin/realm.exe

Linux/macOS/WSL GUI:

    make clean
    make
    ./bin/realm-gfx

Linux/macOS/WSL terminal:

    make clean
    make terminal
    ./bin/realm


Visual mode update
------------------
The SDL GUI renderer now has two visual choices on the splash screen:

  [4] ASCII
  [5] Tileset

ASCII draws the terminal-style text grid. Tileset draws the terrain and tile
backgrounds as flat isometric diamonds while keeping entities, buildings,
resources, trees, and text upright in the centre of each tile. The old top-down
Tileset/emoji projection is no longer offered.
