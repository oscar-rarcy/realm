#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

EMSDK_VERSION="${REALM_EMSDK_VERSION:-3.1.74}"
BUILD_DIR="$ROOT_DIR/build/web"
DIST_DIR="$ROOT_DIR/dist/netlify"
ASSET_DIR="$BUILD_DIR/assets"
FONT_DIR="$ASSET_DIR/fonts"
EMSDK_DIR="${REALM_EMSDK_DIR:-$ROOT_DIR/.emsdk}"

if ! command -v em++ >/dev/null 2>&1; then
  if [[ "${REALM_INSTALL_EMSDK:-0}" != "1" ]]; then
    echo "em++ was not found. Set REALM_INSTALL_EMSDK=1 to install pinned emsdk $EMSDK_VERSION into $EMSDK_DIR." >&2
    exit 127
  fi
  if [[ ! -d "$EMSDK_DIR/.git" ]]; then
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  fi
  "$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
  "$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh"
fi

mkdir -p "$BUILD_DIR" "$DIST_DIR" "$ASSET_DIR" "$FONT_DIR"
find "$DIST_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +

copy_first_font() {
  local dest="$1"
  shift
  local src
  for src in "$@"; do
    if [[ -f "$src" ]]; then
      cp "$src" "$dest"
      return 0
    fi
  done
  return 1
}

if [[ ! -f "$FONT_DIR/DejaVuSansMono.ttf" ]]; then
  copy_first_font "$FONT_DIR/DejaVuSansMono.ttf" \
    /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf \
    /usr/share/fonts/truetype/liberation2/LiberationMono-Regular.ttf \
    /mnt/c/Windows/Fonts/consola.ttf \
    /c/Windows/Fonts/consola.ttf \
    || { echo "No usable monospace font found for the web bundle." >&2; exit 1; }
fi

if [[ ! -f "$FONT_DIR/RealmSymbols.ttf" ]]; then
  copy_first_font "$FONT_DIR/RealmSymbols.ttf" \
    /usr/share/fonts/truetype/noto/NotoEmoji-Regular.ttf \
    /usr/share/fonts/truetype/ancient-scripts/Symbola_hint.ttf \
    /mnt/c/Windows/Fonts/seguisym.ttf \
    /c/Windows/Fonts/seguisym.ttf \
    /mnt/c/Windows/Fonts/seguiemj.ttf \
    /c/Windows/Fonts/seguiemj.ttf \
    || cp "$FONT_DIR/DejaVuSansMono.ttf" "$FONT_DIR/RealmSymbols.ttf"
fi

if [[ -f assets/app-icon.svg ]]; then
  mkdir -p "$ASSET_DIR"
  cp assets/app-icon.svg "$ASSET_DIR/app-icon.svg"
fi

COMMON_SOURCES=(
  src/main_web.cpp
  src/main.cpp
  src/globals.cpp
  src/mapgen.cpp
  src/entity.cpp
  src/orders.cpp
  src/simulation.cpp
  src/ai.cpp
  src/input.cpp
  src/display.cpp
  src/gfx_renderer.cpp
)

em++ "${COMMON_SOURCES[@]}" \
  -std=c++17 -O2 -Wall -Wextra \
  -DREALM_WEB -DUSE_SDL_RENDERER \
  -Iinclude \
  -sUSE_SDL=2 \
  -sUSE_SDL_TTF=2 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sEXIT_RUNTIME=0 \
  -sASSERTIONS=1 \
  -sEXPORTED_FUNCTIONS='["_main","_realm_web_tick","_realm_web_entity_count","_realm_web_selected_id"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  --preload-file "$ASSET_DIR@/assets" \
  --preload-file "$FONT_DIR/DejaVuSansMono.ttf@/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf" \
  --preload-file "$FONT_DIR/RealmSymbols.ttf@/assets/fonts/RealmSymbols.ttf" \
  --shell-file web/shell.html \
  -o "$DIST_DIR/index.html"

perl -0pi -e 's#<script async src=index\.js></script>#<script>(function(){var base=location.pathname.indexOf("/apps/realm")===0?"/apps/realm/":"/";var script=document.createElement("script");script.async=true;script.src=base+"index.js";document.currentScript.after(script);}());</script>#' "$DIST_DIR/index.html"

cat > "$DIST_DIR/_headers" <<'HEADERS'
/*.wasm
  Content-Type: application/wasm

/*
  X-Content-Type-Options: nosniff
HEADERS

cat > "$DIST_DIR/_redirects" <<'REDIRECTS'
/* /index.html 200
REDIRECTS

if [[ -f "$ASSET_DIR/app-icon.svg" ]]; then
  mkdir -p "$DIST_DIR/assets"
  cp "$ASSET_DIR/app-icon.svg" "$DIST_DIR/assets/app-icon.svg"
fi

echo "Realm web build complete:"
find "$DIST_DIR" -maxdepth 2 -type f | sort
