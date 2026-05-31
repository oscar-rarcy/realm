# Specification: Add a WebAssembly / Netlify Web Build for Realm

## Goal

Add a web-playable version of the C++ Realm game that can be deployed to Netlify and played in the browser.

The web build should preserve the existing C++ game core and compile it to WebAssembly using Emscripten. Do not rewrite the game in JavaScript or TypeScript unless a specific frontend wrapper is needed. The preferred architecture is:

```text
shared C++ game core
  ├─ existing terminal/headless backend
  ├─ existing Windows GUI backend
  └─ new web/Emscripten backend
```

The web build should be deployable as a static Netlify site.

## High-level requirements

1. Keep the simulation, map generation, entities, AI, commands, and game rules in shared C++ code.
2. Add a new web platform backend using Emscripten/WebAssembly.
3. Avoid coupling the game core to the Windows GUI renderer.
4. Avoid blocking console input or native-only APIs in the web path.
5. Produce static deploy output containing HTML, JavaScript, WebAssembly, and any required assets.
6. Support Netlify branch deploys so different Git branches can have stable public URLs.
7. Keep the existing Windows GUI build working.
8. Keep terminal/headless mode working if already present, because it remains useful for tests and AI-agent play.

## Recommended public URL strategy

Use a dedicated play subdomain as the canonical public web game URL:

```text
play.<domain>
```

For example:

```text
play.example.com
```

Use Netlify branch subdomains for long-lived branch builds:

```text
play.example.com              -> stable / production branch
main.play.example.com         -> main branch
edward.play.example.com       -> edward branch
```

Also support this optional convenience URL:

```text
example.com/play              -> redirect or rewrite to play.example.com
```

Do not make `/play` the only canonical deployment path unless there is already a larger website at the root. WebAssembly games are simpler to host when their assets live relative to the site root. If `/play` is supported, the build must handle a configurable base path correctly.

## Alternative URL strategy if the game must live under `/play`

If the same Netlify site also hosts other content and the game must live under `/play`, use:

```text
example.com/play              -> stable / production branch
main.example.com/play         -> main branch
edward.example.com/play       -> edward branch
play.example.com              -> optional redirect to example.com/play
```

If this route is chosen, the web build must explicitly support a `BASE_PATH=/play` setting so that HTML, JS, WASM, assets, save files, and service worker paths do not break.

## Netlify branch model

Configure Netlify as follows:

```text
production branch: stable
branch deploys: main, edward, and optionally all non-production branches
production URL: play.<domain>
branch URLs: main.play.<domain>, edward.play.<domain>
```

If the current stable branch is called something else, use that as the production branch instead.

Do not rely on Netlify CLI alias deploys for branch environments. Use connected Git branch deploys so Netlify treats them as real branch deploys.

## Netlify DNS/domain requirements

Use Netlify DNS for the domain or delegate the relevant subdomain to Netlify DNS.

Preferred setup:

```text
play.<domain> is managed by Netlify DNS
branch subdomains are generated under play.<domain>
```

This should allow:

```text
main.play.<domain>
edward.play.<domain>
```

If the apex domain is not managed by Netlify DNS, investigate delegating only the `play.<domain>` subdomain to Netlify DNS.

## Build output

The web build should produce a static directory such as:

```text
dist/netlify/
  index.html
  realm.js
  realm.wasm
  assets/
  _headers
  _redirects
```

Or, if the game is served under `/play`:

```text
dist/netlify/
  index.html
  play/
    index.html
    realm.js
    realm.wasm
    assets/
  _headers
  _redirects
```

Prefer the root-hosted version first unless the repo already has a wider website.

## Required repository changes

Add or update the following files as appropriate:

```text
netlify.toml
scripts/build-web.sh
scripts/netlify-build-web.sh
web/index.html
web/shell.html, if using a custom Emscripten shell
src/platform_web/, if a dedicated web backend is added
docs/web-build.md
```

The exact paths can be adjusted to match the existing repository structure.

## Netlify configuration

Add a `netlify.toml` file similar to this:

```toml
[build]
  command = "bash scripts/netlify-build-web.sh"
  publish = "dist/netlify"

[context.production]
  command = "bash scripts/netlify-build-web.sh stable"

[context.main]
  command = "bash scripts/netlify-build-web.sh main"

[context.edward]
  command = "bash scripts/netlify-build-web.sh edward"

[[headers]]
  for = "/*.wasm"
  [headers.values]
    Content-Type = "application/wasm"

[[headers]]
  for = "/*"
  [headers.values]
    X-Content-Type-Options = "nosniff"
```

If pthreads or SharedArrayBuffer are used later, add cross-origin isolation headers:

```toml
[[headers]]
  for = "/*"
  [headers.values]
    Cross-Origin-Opener-Policy = "same-origin"
    Cross-Origin-Embedder-Policy = "require-corp"
```

Do not enable pthreads in the first pass unless the game already requires threads.

If supporting `/play` as a redirect to the canonical play subdomain, add an appropriate redirect in `_redirects` or `netlify.toml`.

Example if the canonical game is `play.<domain>`:

```text
/play/*  https://play.<domain>/:splat  301!
/play    https://play.<domain>/        301!
```

If the game is hosted under `/play` on the same site, do not redirect it away. Instead, build the app with `BASE_PATH=/play`.

## Emscripten build requirements

Add a web build target that uses Emscripten.

The build should be reproducible and pinned to a known Emscripten SDK version. The Netlify build script may install `emsdk` during the build, but it should be scripted and deterministic.

The build script should roughly do this:

```bash
#!/usr/bin/env bash
set -euo pipefail

# 1. Install or activate pinned Emscripten SDK.
# 2. Build the C++ web target.
# 3. Copy generated HTML/JS/WASM/assets into dist/netlify.
# 4. Copy _headers and _redirects.
# 5. Print the final file tree.
```

The local command should also work on Windows via WSL/MSYS2 if practical, but Netlify’s build will run in a Linux environment.

## Game loop changes

Browser games cannot use a blocking infinite loop that owns the process forever.

Refactor the main loop so the platform layer can call one frame at a time:

```text
game_init()
game_tick(input_snapshot)
game_render(render_target)
game_shutdown()
```

The web backend should use Emscripten’s browser-compatible main loop mechanism.

Do not put browser-specific logic into the core simulation.

## Platform abstraction

Introduce or clarify these boundaries:

```text
GameCore
  owns rules, state, map, AI, units, resources, commands

Renderer
  draws tiles, entities, HUD, selection, overlays, messages

Input
  translates keyboard/mouse/browser events into game commands

Platform
  owns window/canvas lifecycle, timing, filesystem, deployment-specific setup
```

The web backend should be another platform implementation, not a fork of the game.

## Rendering approach

Start with the simplest reliable web renderer.

Preferred first pass:

```text
Emscripten + Canvas/SDL-style rendering
```

Acceptable alternatives:

```text
C++ core compiled to WASM + small JavaScript canvas frontend
Emscripten SDL2 renderer
Emscripten WebGL renderer
```

Do not start with WebGPU. It is unnecessary for the first web version.

The first web version only needs to prove:

1. The game loads.
2. The map renders.
3. Units/entities render.
4. The user can select and command units.
5. The game advances normally.
6. The HUD is usable.

Visual polish can come later.

## Input requirements

The web backend should support:

```text
mouse move
mouse click
right click or command click, if used by the game
keyboard shortcuts
escape/cancel
resize handling
focus/blur handling
```

The browser must not scroll the page when game-critical keys are pressed.

## Files/assets/saves

Abstract file access.

Native builds may use local files. Web builds must use browser-safe storage.

First pass:

```text
assets: packaged into the Netlify static output
settings: localStorage or IDBFS
save games: optional; can be deferred if not currently needed
```

If the game currently assumes relative filesystem paths, make a small asset-loading abstraction instead of scattering web-specific path hacks through the codebase.

## Threading

First pass should be single-threaded.

Do not enable Emscripten pthreads unless required. Pthreads add deployment complexity because browsers require SharedArrayBuffer support and cross-origin isolation headers.

## Build modes

Keep these modes distinct:

```text
native-terminal
native-headless
windows-gui
web
```

The web build should not break the existing desktop builds.

Suggested commands:

```bash
make
make windows-gui
make headless
make web
bash scripts/build-web.sh
bash scripts/netlify-build-web.sh
```

Adjust names to match the repository’s existing Makefile style.

## Testing requirements

Add or preserve tests for:

```text
native C++ build still compiles
Windows GUI build still compiles
headless/simulation tests still pass
web build compiles successfully with Emscripten
generated Netlify output contains index.html, JS, WASM, and assets
local static server can load the game
browser smoke test confirms the canvas appears and at least one frame renders
```

If Playwright is already acceptable in the repo, add a minimal browser smoke test:

```text
start static server from dist/netlify
open page
wait for canvas
assert no fatal console errors
assert game reports initialized
```

Do not block the first implementation on full browser gameplay tests.

## Manual verification checklist

After implementation, verify:

```text
Local native build works.
Local Windows GUI build works.
Local web build creates dist/netlify.
Local web build runs in a browser.
No blocking console input is used in the web build.
No Windows-only APIs are compiled into the web target.
Netlify build command succeeds.
Stable branch deploys to play.<domain>.
Main branch deploys to main.play.<domain>.
Edward branch deploys to edward.play.<domain>.
Optional /play route redirects or works correctly.
```

## Documentation requirements

Create `docs/web-build.md` covering:

```text
what the web build is
how to build locally
how to run locally
how to deploy to Netlify
which branch maps to which URL
how assets are loaded
known limitations
future improvements
```

Also update the README with a short section pointing to `docs/web-build.md`.

## Implementation phases

### Phase 1: Audit and plan

Inspect the current repo and identify:

```text
current entry points
current main loop
current renderer boundary
current Windows-specific code
current asset loading
current input handling
current build files
```

Write a short implementation note before changing code.

### Phase 2: Build skeleton

Add:

```text
web build target
Emscripten build script
Netlify output directory
minimal web HTML shell
netlify.toml
```

Success condition:

```text
Netlify/static output can show a blank or placeholder canvas.
```

### Phase 3: Web main loop

Refactor the game loop so web can call one frame at a time.

Success condition:

```text
Web build initializes the game and runs frames without freezing the browser.
```

### Phase 4: Web renderer/input

Implement enough renderer/input support to play locally in the browser.

Success condition:

```text
The game can be interacted with in the browser.
```

### Phase 5: Asset and storage cleanup

Make asset loading and any save/config logic browser-safe.

Success condition:

```text
The web build does not depend on native filesystem assumptions.
```

### Phase 6: Netlify deployment

Configure Netlify and document domain/branch setup.

Success condition:

```text
stable/main/edward branches deploy to their expected Netlify branch URLs.
```

### Phase 7: Tests and report

Run all relevant builds/tests and write a short final report.

The final report should include:

```text
files changed
commands run
what passed
what failed or remains limited
exact local run instructions
exact Netlify setup instructions
```

## Non-goals for the first pass

Do not add online multiplayer.
Do not rewrite the game in TypeScript.
Do not redesign the whole UI.
Do not require WebGPU.
Do not require pthreads unless the existing game cannot run without them.
Do not remove terminal/headless support.
Do not remove the Windows GUI build.
Do not make the browser version the only supported platform.

## Desired final outcome

After this work, the repository should support:

```text
local desktop/native development
local Windows GUI development
headless simulation/testing
browser play through WebAssembly
Netlify static deployment
branch-specific public preview URLs
```

The preferred public URL setup is:

```text
play.<domain>              stable production branch
main.play.<domain>         main branch
edward.play.<domain>       edward branch
<domain>/play              optional redirect to play.<domain>
```
