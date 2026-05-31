# Realm

Small C++ RTS-style game with two frontends:

- SDL2 graphical renderer, built by default.
- ncurses terminal renderer for Linux/macOS/WSL.

## Project Layout

```text
src/        C++ implementation files
include/    Public project headers
docs/       Design notes, renderer notes, and manual test plans
scripts/    Convenience launch/build scripts
build/      Generated object files and logs, ignored by git
bin/        Generated executables and runtime DLLs, ignored by git
```

## Windows Build

Use MSYS2 UCRT64:

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-SDL2_ttf
mingw32-make clean
mingw32-make gfx
./bin/realm.exe
```

The Windows build copies the required runtime DLLs into `bin/` beside `realm.exe`,
so the executable can also be launched from PowerShell or Explorer.

## Runtime Smoke Test

```powershell
$env:REALM_SMOKE_TEST = "1"
Start-Process .\bin\realm.exe -Wait -PassThru
Remove-Item Env:\REALM_SMOKE_TEST
```

A successful smoke writes `realm-run.log` with `realm: main screen ready` and
exits with code `0`.

To smoke a real deterministic match startup, simulation ticks, and SDL frame
rendering:

```powershell
$env:REALM_SMOKE_TEST = "match"
$env:REALM_SEED = "2468"
$env:REALM_HUMAN_CORNER = "1"
$env:REALM_BIOME = "0"
Start-Process .\bin\realm.exe -Wait -PassThru
Remove-Item Env:\REALM_SMOKE_TEST
Remove-Item Env:\REALM_SEED
Remove-Item Env:\REALM_HUMAN_CORNER
Remove-Item Env:\REALM_BIOME
```

A successful match smoke logs `realm: match smoke complete tick=60`.

## Headless Tests

Run from an MSYS2 UCRT64 shell on Windows:

```sh
mingw32-make test
```

The test target builds `bin/realm_headless_tests.exe` without SDL or ncurses and
checks placement bounds, state names, entity traits, command help bindings,
two-games-in-one-process reset, deterministic startup, supply reservation, town
hall cost, berry gathering/depletion, mapgen reachability across deterministic
seeds, hostile wildlife start safety, exact save/resume, recoverable validation,
and a 10,000 tick AI progression run.

For debug assertions and symbols:

```sh
mingw32-make clean
mingw32-make debug
```

`mingw32-make sanitize` is intentionally disabled on native Windows/MSYS2 in this
Makefile. Use WSL/Linux/macOS for sanitizer runs:

```sh
make sanitize
```

The sanitizer target runs the same headless suite with ASan/UBSan and sets
`REALM_TEST_LONG_TICKS=2000` so sanitizer verification remains practical. Plain
`make test` / `mingw32-make test` still default to the 10,000 tick long run.

When switching between WSL/Linux `make` and Windows/MSYS2 `mingw32-make`, run
`make clean` or `mingw32-make clean` first. Both toolchains use `build/obj`, so
mixed object files can cause confusing link errors.

## Reproducible Startup

Normal games stay random by default. For reproducible reports, set these
environment variables before launching:

```powershell
$env:REALM_SEED = "12345"
$env:REALM_HUMAN_CORNER = "0"   # 0..3, or unset for random
$env:REALM_BIOME = "0"          # -1 random, 0 temperate, 1 desert, 2 snow, 3 swamp, 4 forest, 5 volcanic, 6 coastal
$env:REALM_DIAGNOSTICS = "1"
.\bin\realm.exe
```

Match startup logs the seed, AI count, human corner, biome, entity count, and
projectile count to `realm-run.log`.

## Save / Load

During a match:

- SDL: `F5` saves, `F9` loads, `F8` toggles diagnostics, `?` opens help.
- Terminal: `V` saves, `L` loads, `D` toggles diagnostics, `?` opens help.

The default save file is `realm-save.txt`, which is ignored by git.

## Food Economy

Peasants gather food from berries and hunted animals, fishing boats gather fish
for docks, and completed farms produce food when worked, especially around
mills. Carried resources return to a town hall or the matching drop-off
building. Winter applies periodic food pressure to living units; if the
stockpile is empty, starvation damages units instead.

## Packaging

After a successful Windows GUI build:

```sh
mingw32-make package
```

This creates `bin/realm-windows.zip` from `realm.exe` and the copied runtime
DLLs. Verify a package by extracting it away from the repo and running
`REALM_SMOKE_TEST=match realm.exe`; logs are generated at runtime.

## Unix-like Builds

GUI:

```sh
make clean
make gfx
./bin/realm-gfx
```

Terminal:

```sh
make clean
make terminal
./bin/realm
```
