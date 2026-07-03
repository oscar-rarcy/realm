# Building Realm

Two frontends from the same sources:

- **`make`** → `realm` — the terminal build (wide ncurses).
- **`make gui-build`** → `realm-gui` — the standalone SDL2 window
  (the game's small ncurses-shaped API reimplemented over SDL in
  `sdl_shim.cpp`; no ncurses needed).

Both are plain C++17 + a Makefile — no CMake, no submodules. CI
(`.github/workflows/build.yml`) builds every push on Linux, macOS and
Windows and checks the sim still hashes deterministically.

## macOS

```sh
brew install sdl2 sdl2_ttf sdl2_mixer pkg-config   # ncurses ships with macOS
make            # terminal build
make gui-build  # SDL build
make app        # self-contained Realm.app (Apple Silicon; bundles dylibs)
make share      # Realm.app + READ ME zipped to ~/Desktop/Realm-mac.zip
```

## Linux

```sh
sudo apt-get install libncursesw5-dev libsdl2-dev libsdl2-ttf-dev \
                     libsdl2-mixer-dev pkg-config
make            # terminal build
make gui-build  # SDL build
```

The SDL build looks for system monospace fonts at runtime (`REALM_FONT=`
`/path/to/font.ttf` overrides if none of the default candidates exist).

## Windows (MSYS2 / MinGW64)

Install [MSYS2](https://www.msys2.org), then in a **MINGW64** shell:

```sh
pacman -S make pkg-config mingw-w64-x86_64-gcc \
          mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_ttf \
          mingw-w64-x86_64-SDL2_mixer mingw-w64-x86_64-ncurses
make gui-build  # the recommended Windows frontend
make            # terminal build (runs best in Windows Terminal)
```

Winsock is linked automatically (`PLATFORM_LIBS` in the Makefile).
Set `REALM_FONT=C:/Windows/Fonts/consola.ttf` if the font probe misses.

## Checks

```sh
./realm --verify 12345 3000 2   # run twice: identical hash = deterministic
./realm --test-raid             # AI plunder pipeline end-to-end
./realm --net-host 3000 1       # + `./realm --net-join <ip> 3000` elsewhere:
                                #   identical hashes = lockstep holds
```

## Multiplayer compatibility

Both players need the **same build** (same commit, any platform is intended
but only same-platform is verified so far — the sim uses floats, so
cross-platform play should be confirmed with the hash check before trusting
it). The title screen shows the build date and protocol version.
