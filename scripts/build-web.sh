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

configure_local_emsdk_python() {
  local candidate
  for candidate in "$EMSDK_DIR"/python/*/python.exe "$EMSDK_DIR"/python/*/bin/python3; do
    [[ -x "$candidate" ]] || continue
    export PYTHON="$candidate"
    if command -v cygpath >/dev/null 2>&1 && [[ "$candidate" == *.exe ]]; then
      export EMSDK_PYTHON="$(cygpath -w "$candidate")"
    else
      export EMSDK_PYTHON="$candidate"
    fi
    export PATH="$(dirname "$candidate"):$PATH"
    return 0
  done
  return 1
}

activate_local_emsdk() {
  [[ -f "$EMSDK_DIR/emsdk_env.sh" ]] || return 1
  configure_local_emsdk_python || true
  # shellcheck disable=SC1091
  source "$EMSDK_DIR/emsdk_env.sh" >/dev/null 2>&1
  configure_local_emsdk_python || true
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
  src/platform/user_settings.cpp
  src/platform/view_state.cpp
  src/render/display_model.cpp
  src/render/entity_visual_defs.cpp
  src/render/ground_shader.cpp
  src/render/render_model.cpp
)

SDL_BASE_SOURCES=()
for source in src/render/sdl/*.cpp; do
  case "$source" in
    src/render/sdl/tileset_assets.cpp|src/render/sdl/tileset_hud_renderer.cpp|src/render/sdl/tileset_lab.cpp|src/render/sdl/tileset_disabled.cpp)
      ;;
    *)
      SDL_BASE_SOURCES+=("$source")
      ;;
  esac
done

visual_mode_is_ascii_only() {
  local mode="${1,,}"
  mode="${mode//_/-}"
  case "$mode" in
    ascii-only|ascii|terminal|console) return 0 ;;
    *) return 1 ;;
  esac
}

patch_shell_loader() {
  local html="$1"
  perl -0pi -e 's#<script async src=index\.js></script>#<script>(function(){var base=window.realmAssetBase||"/";var script=document.createElement("script");script.async=true;script.src=base+"index.js";document.currentScript.after(script);}());</script>#' "$html"
}

build_web_app() {
  local output_html="$1"
  local visual_mode="$2"
  local tileset_enabled="$3"
  local output_dir
  output_dir="$(dirname "$output_html")"
  mkdir -p "$output_dir"

  local -a sources=("${COMMON_SOURCES[@]}" "${SDL_BASE_SOURCES[@]}")
  local -a png_args=()
  local -a preload_args=(
    --preload-file "$FONT_DIR/DejaVuSansMono.ttf@/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf"
    --preload-file "$FONT_DIR/RealmSymbols.ttf@/assets/fonts/RealmSymbols.ttf"
  )

  if [[ "$tileset_enabled" == "1" ]]; then
    sources+=(src/render/sdl/tileset_assets.cpp src/render/sdl/tileset_hud_renderer.cpp)
    png_args=(-sUSE_LIBPNG=1)
    if [[ -d assets/tiles ]]; then
      rm -rf "$ASSET_DIR/tiles"
      cp -R assets/tiles "$ASSET_DIR/tiles"
      preload_args+=(--preload-file "$ASSET_DIR/tiles@/assets/tiles")
    fi
  else
    sources+=(src/render/sdl/tileset_disabled.cpp)
  fi

  em++ "${sources[@]}" \
    -std=c++17 -O2 -Wall -Wextra \
    -DREALM_WEB -DUSE_SDL_RENDERER "-DREALM_ENABLE_TILESET=$tileset_enabled" \
    "-DREALM_VISUAL_MODE_DEFAULT=\"$visual_mode\"" \
    -Iinclude \
    -Isrc \
    -sUSE_SDL=2 \
    -sUSE_SDL_TTF=2 \
    "${png_args[@]}" \
    -sALLOW_MEMORY_GROWTH=1 \
    -sSTACK_SIZE=8388608 \
    -sEXIT_RUNTIME=0 \
    -sASSERTIONS=1 \
    -sEXPORTED_FUNCTIONS='["_main","_realm_web_tick","_realm_web_entity_count","_realm_web_selected_id","_realm_web_selected_count","_realm_web_view_x","_realm_web_view_y","_realm_web_view_w","_realm_web_view_h","_realm_web_cursor_x","_realm_web_cursor_y","_realm_web_first_owned_unit_x","_realm_web_first_owned_unit_y","_realm_web_screen_x_for_tile","_realm_web_screen_y_for_tile","_realm_web_screen","_realm_web_ascii_only","_realm_web_display_mode","_realm_web_context_menu_open","_realm_web_context_menu_option_count","_realm_web_test_force_loss"]' \
    -sEXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
    "${preload_args[@]}" \
    --shell-file web/shell.html \
    -o "$output_html"

  patch_shell_loader "$output_html"
}

MAIN_TILESET_ENABLED=1
if visual_mode_is_ascii_only "$REALM_VISUAL_MODE"; then
  MAIN_TILESET_ENABLED=0
fi

build_web_app "$DIST_DIR/index.html" "$REALM_VISUAL_MODE" "$MAIN_TILESET_ENABLED"
build_web_app "$DIST_DIR/ascii/index.html" "ascii-only" 0

mkdir -p "$DIST_DIR/embed" "$DIST_DIR/ascii/embed"
cp "$DIST_DIR/index.html" "$DIST_DIR/embed/index.html"
cp "$DIST_DIR/ascii/index.html" "$DIST_DIR/ascii/embed/index.html"

cat > "$DIST_DIR/_headers" <<'HEADERS'
/*.wasm
  Content-Type: application/wasm

/*
  X-Content-Type-Options: nosniff
HEADERS

cat > "$DIST_DIR/_redirects" <<'REDIRECTS'
/ascii /ascii/index.html 200
/ascii/* /ascii/index.html 200
/* /index.html 200
REDIRECTS

if [[ -f assets/app-icon.svg ]]; then
  mkdir -p "$DIST_DIR/assets" "$DIST_DIR/ascii/assets"
  cp assets/app-icon.svg "$DIST_DIR/assets/app-icon.svg"
  cp assets/app-icon.svg "$DIST_DIR/ascii/assets/app-icon.svg"
fi

echo "Realm web build complete:"
find "$DIST_DIR" -maxdepth 2 -type f | sort
