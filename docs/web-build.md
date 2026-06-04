# Realm Web Build

Realm's web build compiles the shared C++ simulation and SDL renderer to
WebAssembly with Emscripten. The browser uses `src/main_web.cpp`, which opens on
the main menu for normal routes and advances either the menu or the active match
one frame at a time through Emscripten's main loop.

## Local build

From WSL, Linux, or a shell with Emscripten available:

```sh
bash scripts/build-web.sh
```

If `./.emsdk` already exists, `build-web.sh` will activate it automatically.

To install the pinned SDK locally without building yet:

```sh
bash scripts/setup-web.sh
```

Or, to install and build in one step:

```sh
bash scripts/build-web.sh --install-emsdk
```

The output is written to `dist/netlify/` and includes `index.html`, JavaScript,
WebAssembly, Emscripten data assets, `_headers`, and `_redirects`.

## Default validation

```sh
bash scripts/validate.sh
```

That always runs the architecture check plus native clean/test/GUI builds. It
only runs the web build automatically when Emscripten is already available, or
when you opt in with:

```sh
bash scripts/validate.sh --include-web --install-emsdk
```

If an AI assistant hits a missing-Emscripten error, it should ask before
running the install step.

The web build reads `REALM_VISUAL_MODE` from the shell, then `.env.local`, then
`.env`. The committed default is `ascii-only`, which makes the normal web build
ASCII-only and hides the visual-mode selector. Use `.env.local` with
`REALM_VISUAL_MODE=tileset-menu` for a personal tileset-first build that still
keeps ASCII available in the menu.

## Local run

```sh
cd dist/netlify
python3 -m http.server 4173
```

Open `http://127.0.0.1:4173/`.

## Browser smoke test

With the static server running:

```sh
npm install
REALM_WEB_URL=http://127.0.0.1:4173/ npm run test:web
```

The smoke test waits for the canvas, checks that the WebAssembly game reports
ready, confirms ticks advance, and verifies the canvas is nonblank.

## Netlify deploy

This pass enables the `edward` branch only:

```sh
npx netlify status
npx netlify deploy --build
npx netlify deploy --build --prod
```

Netlify runs:

```sh
bash scripts/netlify-build-web.sh edward
```

The production branch for this temporary site should be `edward`. Main and
stable branch deploys are TODO until those branches have the GUI/web build.

## URL model

Current pass:

```text
edward branch Netlify site -> playable Realm web build
edward branch Netlify site /ascii -> ASCII-only Realm web build
edwardcoventry.com/apps/realm -> proxy to the Realm Netlify site
edwardcoventry.com/apps/realm-ascii -> proxy to the Realm Netlify site /ascii surface
```

The web entrypoint also treats an `ascii.*` host as the ASCII-only surface, so a
custom domain assigned to the same Netlify deploy can serve the same build
without exposing the visual-mode selector.

Future TODO:

```text
play.<domain>              stable production branch
main.play.<domain>         main branch
edward.play.<domain>       edward branch
```

## Assets and storage

The build script copies a local monospace font into `build/web/assets/fonts` and
packs it into Emscripten's virtual filesystem at `/assets/fonts`. Save/load uses
Emscripten's in-memory filesystem for now, so browser saves are not yet
persistent after refresh.

User preferences are separate from match saves. Browser preferences, such as the
main-menu player colour and ASCII map-cell shape, use `localStorage` under
`realm.settings.v1`.

## Web controls

- `Q` resigns the active match and returns to the web main menu.
- `X` is ignored on web; the browser tab/window owns exit.
- `F11`, `Alt+Enter`, and the mobile HUD Full button request browser fullscreen.

## Known limitations

- The browser entrypoint uses route-based startup:
  non-`/embed` pages show the main menu, while `/embed` and `?embed` start a
  deterministic playable match immediately for site embeds.
- Browser save persistence is deferred.
- Only the `edward` branch is configured for Netlify in this pass.
