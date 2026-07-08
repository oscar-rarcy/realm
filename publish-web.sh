#!/bin/sh
# Publish the local web/ build to the gh-pages branch, which GitHub Pages
# serves at https://oscar-rarcy.github.io/realm/. The web artifacts are
# gitignored on sdl-frontend, so this is THE way the live site updates.
#
# relay.json on gh-pages — the live multiplayer relay pointer the page reads
# at load — is kept as-is unless --relay hands in a new URL:
#   ./publish-web.sh                             # publish current game build
#   ./publish-web.sh --relay wss://your.relay    # …and re-point the relay
set -e
cd "$(dirname "$0")"
make web
git fetch origin gh-pages
D="$(mktemp -d)/pages"
git worktree add "$D" gh-pages
cp web/index.html web/index.js web/index.wasm web/index.data "$D"/
if [ "$1" = "--relay" ] && [ -n "$2" ]; then
  printf '{ "relay": "%s" }\n' "$2" > "$D/relay.json"
fi
git -C "$D" add -A
git -C "$D" diff --cached --quiet || git -C "$D" commit -m "publish web build $(git rev-parse --short HEAD)"
git -C "$D" push origin gh-pages
git worktree remove --force "$D"
echo "Live at https://oscar-rarcy.github.io/realm/ (allow ~1 min for Pages to refresh)"
