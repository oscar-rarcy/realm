# Realm Web Build

Realm's web build compiles the shared C++ simulation and SDL renderer to
WebAssembly with Emscripten. The browser uses `src/main_web.cpp`, which starts a
deterministic playable match and advances the existing game one frame at a time
through Emscripten's main loop.

## Local build

From WSL, Linux, or a shell with Emscripten available:

```sh
bash scripts/build-web.sh
```

If `em++` is not installed, allow the script to install the pinned SDK locally:

```sh
REALM_INSTALL_EMSDK=1 bash scripts/build-web.sh
```

The output is written to `dist/netlify/` and includes `index.html`, JavaScript,
WebAssembly, Emscripten data assets, `_headers`, and `_redirects`.

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
edwardcoventry.com/apps/realm -> proxy to the Realm Netlify site
```

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

## Known limitations

- The browser entrypoint uses route-based startup:
  non-`/embed` pages show the main menu, while `/embed` starts a deterministic
  mid-game immediately.
- Browser save persistence is deferred.
- Only the `edward` branch is configured for Netlify in this pass.
