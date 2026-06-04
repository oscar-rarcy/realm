#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

show_help() {
  cat <<EOF
Usage: bash scripts/build-web.sh [--install-emsdk] [--setup-only]

Build Realm's web output in dist/netlify.

Options:
  --install-emsdk  Install the pinned emsdk locally in $ROOT_DIR/.emsdk if needed.
  --setup-only     Ensure the web toolchain is ready, then exit without building.
  -h, --help       Show this help text.
EOF
}

INSTALL_EMSDK="${REALM_INSTALL_EMSDK:-0}"
SETUP_ONLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install-emsdk)
      INSTALL_EMSDK=1
      ;;
    --setup-only)
      SETUP_ONLY=1
      ;;
    -h|--help)
      show_help
      exit 0
      ;;
    *)
      echo "Unknown option: $1" >&2
      show_help >&2
      exit 2
      ;;
  esac
  shift
done

read_env_value() {
  local file="$1"
  local key="$2"
  local line value
  [[ -f "$file" ]] || return 1
  while IFS= read -r line || [[ -n "$line" ]]; do
    line="${line#"${line%%[![:space:]]*}"}"
    [[ "$line" == "$key="* || "$line" == "export $key="* ]] || continue
    value="${line#*=}"
    if [[ "$value" != \"* && "$value" != \'* ]]; then
      value="${value%%#*}"
    fi
    value="${value%"${value##*[![:space:]]}"}"
    value="${value%\"}"
    value="${value#\"}"
    value="${value%\'}"
    value="${value#\'}"
    printf '%s' "$value"
    return 0
  done < "$file"
  return 1
}

if [[ -z "${REALM_VISUAL_MODE+x}" ]]; then
  if value="$(read_env_value .env REALM_VISUAL_MODE)"; then
    export REALM_VISUAL_MODE="$value"
  fi
  if value="$(read_env_value .env.local REALM_VISUAL_MODE)"; then
    export REALM_VISUAL_MODE="$value"
  fi
fi
REALM_VISUAL_MODE="${REALM_VISUAL_MODE:-ascii-only}"

EMSDK_VERSION="${REALM_EMSDK_VERSION:-3.1.74}"
BUILD_DIR="$ROOT_DIR/build/web"
DIST_DIR="$ROOT_DIR/dist/netlify"
ASSET_DIR="$BUILD_DIR/assets"
FONT_DIR="$ASSET_DIR/fonts"
EMSDK_DIR="${REALM_EMSDK_DIR:-$ROOT_DIR/.emsdk}"

activate_local_emsdk() {
  [[ -f "$EMSDK_DIR/emsdk_env.sh" ]] || return 1
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
  command -v em++ >/dev/null 2>&1
}

install_local_emsdk() {
  if [[ -e "$EMSDK_DIR" && ! -d "$EMSDK_DIR/.git" ]]; then
    echo "Local emsdk path exists but is not a git checkout: $EMSDK_DIR" >&2
    echo "Remove it or set REALM_EMSDK_DIR to a clean location before retrying." >&2
    exit 1
  fi

  if [[ ! -d "$EMSDK_DIR/.git" ]]; then
    git clone https://github.com/emscripten-core/emsdk.git "$EMSDK_DIR"
  fi
  "$EMSDK_DIR/emsdk" install "$EMSDK_VERSION"
  "$EMSDK_DIR/emsdk" activate "$EMSDK_VERSION"
  activate_local_emsdk || {
    echo "Installed emsdk $EMSDK_VERSION but could not activate em++ from $EMSDK_DIR." >&2
    exit 1
  }
}

ensure_web_toolchain() {
  if command -v em++ >/dev/null 2>&1; then
    return 0
  fi

  if activate_local_emsdk; then
    return 0
  fi

  if [[ "$INSTALL_EMSDK" == "1" ]]; then
    install_local_emsdk
    return 0
  fi

  echo "em++ was not found, and no usable local emsdk was activated from $EMSDK_DIR." >&2
  echo "Install the pinned toolchain with 'bash scripts/setup-web.sh' or 'bash scripts/build-web.sh --install-emsdk'." >&2
  echo "If you are an AI assistant, ask the user before running that install step." >&2
  exit 127
}

ensure_web_toolchain

if [[ "$SETUP_ONLY" == "1" ]]; then
  echo "Realm web toolchain ready: $(command -v em++)"
  exit 0
fi

mkdir -p "$BUILD_DIR" "$DIST_DIR" "$ASSET_DIR" "$FONT_DIR"
find "$DIST_DIR" -mindepth 1 -maxdepth 1 -exec rm -rf {} +
rm -rf "$ASSET_DIR/tiles"

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

if [[ -d assets/tiles ]]; then
  mkdir -p "$ASSET_DIR"
  cp -R assets/tiles "$ASSET_DIR/tiles"
fi

COMMON_SOURCES=(
  src/platform/main_web.cpp
  src/core/*.cpp
  src/sim/*.cpp
  src/sim/migrations/*.cpp
  src/commands/*.cpp
  src/ai/*.cpp
  src/map/*.cpp
  src/platform/app_config.cpp
  src/platform/game_init.cpp
  src/platform/view_state.cpp
  src/render/display_model.cpp
  src/render/entity_visual_defs.cpp
  src/render/render_model.cpp
  src/render/sdl/*.cpp
)

em++ "${COMMON_SOURCES[@]}" \
  -std=c++17 -O2 -Wall -Wextra \
  -DREALM_WEB -DUSE_SDL_RENDERER \
  "-DREALM_VISUAL_MODE_DEFAULT=\"$REALM_VISUAL_MODE\"" \
  -Iinclude \
  -Isrc \
  -sUSE_SDL=2 \
  -sUSE_SDL_TTF=2 \
  -sUSE_LIBPNG=1 \
  -sALLOW_MEMORY_GROWTH=1 \
  -sSTACK_SIZE=8388608 \
  -sEXIT_RUNTIME=0 \
  -sASSERTIONS=1 \
  -sEXPORTED_FUNCTIONS='["_main","_realm_web_tick","_realm_web_entity_count","_realm_web_selected_id","_realm_web_selected_count","_realm_web_view_x","_realm_web_view_y","_realm_web_view_w","_realm_web_view_h","_realm_web_cursor_x","_realm_web_cursor_y","_realm_web_first_owned_unit_x","_realm_web_first_owned_unit_y","_realm_web_screen_x_for_tile","_realm_web_screen_y_for_tile","_realm_web_screen","_realm_web_ascii_only","_realm_web_display_mode"]' \
  -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
  --preload-file "$ASSET_DIR@/assets" \
  --preload-file "$FONT_DIR/DejaVuSansMono.ttf@/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf" \
  --preload-file "$FONT_DIR/RealmSymbols.ttf@/assets/fonts/RealmSymbols.ttf" \
  --shell-file web/shell.html \
  -o "$DIST_DIR/index.html"

perl -0pi -e 's#<script async src=index\.js></script>#<script>(function(){var base=window.realmAssetBase||"/";var script=document.createElement("script");script.async=true;script.src=base+"index.js";document.currentScript.after(script);}());</script>#' "$DIST_DIR/index.html"

mkdir -p "$DIST_DIR/embed" "$DIST_DIR/ascii" "$DIST_DIR/ascii/embed"
cp "$DIST_DIR/index.html" "$DIST_DIR/embed/index.html"
cp "$DIST_DIR/index.html" "$DIST_DIR/ascii/index.html"
cp "$DIST_DIR/index.html" "$DIST_DIR/ascii/embed/index.html"

cat > "$DIST_DIR/_headers" <<'HEADERS'
/*.wasm
  Content-Type: application/wasm

/*
  X-Content-Type-Options: nosniff
HEADERS

cat > "$DIST_DIR/_redirects" <<'REDIRECTS'
/ascii /index.html 200
/ascii/* /index.html 200
/* /index.html 200
REDIRECTS

if [[ -f "$ASSET_DIR/app-icon.svg" ]]; then
  mkdir -p "$DIST_DIR/assets"
  cp "$ASSET_DIR/app-icon.svg" "$DIST_DIR/assets/app-icon.svg"
fi

echo "Realm web build complete:"
find "$DIST_DIR" -maxdepth 2 -type f | sort
