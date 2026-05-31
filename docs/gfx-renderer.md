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
    sudo apt install -y build-essential pkg-config libncursesw5-dev libsdl2-dev libsdl2-ttf-dev

Build/run:

    make clean
    make gfx
    ./bin/realm-gfx

Graphical build on native Windows with MSYS2 UCRT64
--------------------------------------------------

Open the "MSYS2 UCRT64" shell, not plain PowerShell and not the MSYS shell.
Then run:

    pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf

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

Runtime controls added by hardening
-----------------------------------

- `F5` saves to `realm-save.txt`.
- `F9` loads from `realm-save.txt`.
- `F8` toggles diagnostics.
- `?` toggles the shared help overlay.
- The right panel always shows cursor tile terrain, biome, resource amount, and
  visible stack information.
- The help overlay documents shared keyboard/mouse commands, SDL-only
  zoom/pan/projection controls, food sources, winter starvation, owner colours,
  neutral animals, and combat alerts.
- Temporary command markers appear on empty target tiles after move, gather,
  attack, build, or rally-style orders.
- Train mode stays open after queueing a unit; repeat unit keys to queue more,
  `Esc` cancels.

Emoji font fix
--------------

The previous SDL renderer was drawing tofu boxes because it usually loaded only
the monospace text font. This version explicitly searches for Segoe UI Emoji at:

    C:/Windows/Fonts/seguiemj.ttf
    /mnt/c/Windows/Fonts/seguiemj.ttf
    /c/Windows/Fonts/seguiemj.ttf

So it works in native Windows, MSYS2 and WSL. At startup, the GUI renderer prints
the text font and emoji font paths it actually loaded. If no emoji font is found, it
uses ASCII-style fallbacks instead of drawing boxes.

Build separation
----------------

The graphical target now compiles its own *_gfx.o object files with
-DUSE_SDL_RENDERER and does not link ncurses. The normal terminal target still
links ncursesw and should keep behaving as before.


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


Isometric projection update
---------------------------
The SDL GUI renderer now has a GUI-only projection option on the splash screen:

  [6] Top-down
  [7] Isometric

The ncurses terminal build is unchanged.  Isometric mode draws the terrain and
tile backgrounds as flat isometric diamonds while keeping entities, buildings,
resources, trees and text upright in the centre of each tile.  During the game
F6 switches back to top-down and F7 switches to isometric.
