# Realm

Realm is a small real-time strategy game written in C++.

You build a base, control peasants and soldiers, gather resources, and fight AI opponents. It can run in a normal graphical window, in a text terminal, or in a browser.

This README is written for someone who is new to coding. The short version is:

1. If you just want to play, use the web version or a helper script.
2. If you want to change the game, edit files in `src/` and `include/`.
3. If you want to work on art, keep final game assets in `assets/` and art-making material in `art/`.

## Play Online

Current playable web versions:

- ASCII version: `https://edwardcoventry.com/apps/realm-ascii`
- Main Realm app: `https://edwardcoventry.com/apps/realm`

The ASCII version is the safest one for simple playtesting because it avoids optional visual tileset work.

## Run On Windows

The easiest Windows path is to use the included scripts.

You need MSYS2 installed at:

```text
C:\msys64
```

Then double-click or run:

```text
scripts\windows-gui-build-and-run.bat
```

That script installs/checks the needed MSYS2 packages, builds the graphical game, and starts it.
It uses an incremental rebuild by default. If you edited headers, switched build environments,
or the build is acting strange, run a clean build with:

```text
scripts\windows-gui-build-and-run.bat --clean
```

The same `--clean` flag works with the other build-and-run helper scripts.

For the local tileset lab, use:

```text
scripts\windows-build-and-run-lab.bat
```

Build logs are written to `logs/`. Build output goes in `build/` and `bin/`. These folders are generated and are ignored by git.

## Basic Controls

On the title screen:

- `Enter`: start a match.
- `1`, `2`, `3`: choose opponent count.
- `T`, `D`, `S`, `W`, `F`, `V`, `C`, `0`: choose biome.

In a match:

- Arrow keys: move the map cursor.
- `Space`: select the unit or building under the cursor.
- `Enter`: command the selected unit.
- `B`: build with a selected peasant.
- `T`: train from a selected building.
- `A`: select military, or attack-move when military is selected.
- `.` or `,`: cycle to an idle peasant.
- `F5`: save in the graphical version.
- `F9`: load in the graphical version.
- `?`: show in-game help.

More playtest instructions live in `docs/tests/agent-playtest.md`.

## Project Folders

```text
src/        C++ implementation files
include/    C++ header files
assets/     Final game assets that the game can load
art/        Art references, prompts, generated candidates, and workbench files
docs/       Notes, plans, test guides, and design docs
scripts/    Helper scripts for building and running
tests/      Automated tests
web/        Browser shell files
build/      Generated build/test output, ignored by git
bin/        Generated executables and DLLs, ignored by git
logs/       Generated wrapper logs, ignored by git
dist/       Generated web deploy output, ignored by git
```

Keep this rule in mind:

- `assets/` is for final files the game actually loads.
- `art/` is for things used to create assets.

For example, final peasant PNGs belong in `assets/tiles/entities/...`, but generated image prompts, source sheets, and candidate images belong in `art/tiles/...`.

## Local Visual Mode

The repo has a default `.env` file:

```text
REALM_VISUAL_MODE=ascii-only
```

That keeps local builds in ASCII mode by default.

To opt in to the tileset menu on your machine only, copy `.env.local.example` to `.env.local` and set:

```text
REALM_VISUAL_MODE=tileset-menu
```

`.env.local` is ignored by git, so personal settings do not get committed.

## Build From A Terminal

Most people should use the helper scripts above. These commands are for developers who are already comfortable with a terminal.

Windows MSYS2 UCRT64 graphical build:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf mingw-w64-ucrt-x86_64-libpng
mingw32-make gfx
./bin/realm.exe
```

Linux/macOS graphical build:

```sh
make gfx
./bin/realm-gfx
```

Linux/macOS terminal build:

```sh
make terminal
./bin/realm
```

When switching between Windows/MSYS2 and WSL/Linux/macOS builds, run `make clean` or `mingw32-make clean` first. They share `build/obj`, and mixed object files can cause confusing errors.
Also run clean after editing headers or changing compiler flags, because this simple Makefile
does not track header dependencies.

## Web Build

Realm can be built for the browser with Emscripten:

```sh
bash scripts/build-web.sh
```

If you already have a repo-local `./.emsdk`, the build script will activate it automatically.

To install the pinned SDK locally on demand:

```sh
bash scripts/setup-web.sh
```

Or, to install and build in one step:

```sh
bash scripts/build-web.sh --install-emsdk
```

The web output is written to `dist/netlify/`.

For the default validation flow:

```sh
bash scripts/validate.sh
```

That always runs the native architecture/test/GUI checks. It only runs the web build automatically when Emscripten is already available, or when you opt in with:

```sh
bash scripts/validate.sh --include-web --install-emsdk
```

To run the web output locally:

```sh
cd dist/netlify
python3 -m http.server 4173
```

Then open:

```text
http://127.0.0.1:4173/
```

More web details are in `docs/web-build.md`.

## Tests

From MSYS2 UCRT64 on Windows:

```sh
mingw32-make test
```

From Linux/macOS/WSL:

```sh
make test
```

Browser smoke test after building and serving `dist/netlify/`:

```sh
npm install
REALM_WEB_URL=http://127.0.0.1:4173/ npm run test:web
```

The automated tests check game rules, deterministic startup, save/load behavior, AI progression, and browser startup.

## Save And Load

During a match:

- Graphical version: `F5` saves, `F9` loads.
- Terminal version: `V` saves, `L` loads.

The default save file is `realm-save.txt`, which is ignored by git.

## User Settings

Realm keeps player preferences separate from match saves.

For example, the player colour and ASCII map-cell shape chosen on the main menu
are remembered for the next session. Player colour also applies when loading an
older save. On desktop this is stored in your user profile, such as
`%APPDATA%\Realm\settings.txt` on Windows. In the browser it is stored in the
browser's local storage for the Realm site.

## Packaging

After a successful Windows graphical build:

```sh
mingw32-make package
```

This creates:

```text
bin/realm-windows.zip
```

The zip contains `realm.exe` and the DLLs it needs.

## If Something Goes Wrong

Check these first:

- Build logs: `logs/`
- Generated executables: `bin/`
- Generated build output: `build/`
- Web output: `dist/netlify/`

Common fixes:

- If the build acts strange, run `mingw32-make clean` or `make clean`.
- If Windows cannot find MSYS2, check that `C:\msys64` exists.
- If the web build cannot find Emscripten, run `bash scripts/setup-web.sh` or `bash scripts/build-web.sh --install-emsdk`.
- If git shows files in `build/`, `bin/`, `logs/`, `dist/`, or `node_modules/`, they should normally be ignored generated files.

## Deeper Docs

- `docs/tests/manual-test-plan.md`: manual testing checklist.
- `docs/tests/agent-playtest.md`: simple repeatable playtest guide.
- `docs/web-build.md`: browser build and Netlify notes.
- `docs/gfx-renderer.md`: graphical renderer notes.
- `docs/tileset/realm_visual_asset_architecture.md`: art and tileset architecture.
