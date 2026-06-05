#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT_DIR"

show_help() {
  cat <<'EOF'
Usage: bash scripts/validate.sh [--include-web] [--install-emsdk] [--native-only]

Runs Realm's default validation flow:
  - architecture check
  - clean native test build
  - native GUI build

Web validation is optional by default. It runs automatically when em++ is
available (including a repo-local ./.emsdk install), or can be forced with
--include-web / REALM_VALIDATE_WEB=1. Use --install-emsdk to bootstrap the
repo-local toolchain first.
EOF
}

WEB_MODE="${REALM_VALIDATE_WEB:-auto}"
INSTALL_EMSDK=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --include-web)
      WEB_MODE=1
      ;;
    --install-emsdk)
      INSTALL_EMSDK=1
      WEB_MODE=1
      ;;
    --native-only)
      WEB_MODE=0
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

has_web_toolchain() {
  command -v em++ >/dev/null 2>&1 || [[ -f "$ROOT_DIR/.emsdk/emsdk_env.sh" ]]
}

pick_make_cmd() {
  if command -v mingw32-make >/dev/null 2>&1; then
    printf '%s\n' "mingw32-make"
    return 0
  fi
  if command -v make >/dev/null 2>&1; then
    printf '%s\n' "make"
    return 0
  fi
  echo "Could not find 'mingw32-make' or 'make' in PATH." >&2
  exit 127
}

pick_python_cmd() {
  if command -v python >/dev/null 2>&1; then
    printf '%s\n' "python"
    return 0
  fi
  if command -v python3 >/dev/null 2>&1; then
    printf '%s\n' "python3"
    return 0
  fi
  if command -v py >/dev/null 2>&1; then
    printf '%s\n' "py"
    return 0
  fi
  echo "Could not find 'python', 'python3', or 'py' in PATH." >&2
  exit 127
}

MAKE_CMD="$(pick_make_cmd)"
PYTHON_CMD="$(pick_python_cmd)"
GUI_TARGET="gfx"
GUI_BINARY="./bin/realm-gfx"
if [[ "${OS:-}" == "Windows_NT" ]]; then
  GUI_TARGET="bin/realm.exe"
  GUI_BINARY="./bin/realm.exe"
fi

echo "Running architecture check..."
"$PYTHON_CMD" scripts/check_architecture.py

echo "Running clean native validation with $MAKE_CMD..."
"$MAKE_CMD" clean
"$MAKE_CMD" test
"$MAKE_CMD" "$GUI_TARGET"
echo "Checking tileset asset registry..."
if [[ -d "$ROOT_DIR/assets/tiles" ]] && [[ -n "$(find "$ROOT_DIR/assets/tiles" -name manifest.json -print -quit)" ]]; then
  "$GUI_BINARY" --dump-missing-tileset-assets
else
  echo "Skipping tileset asset registry: assets/tiles is absent or has no manifests."
fi

case "$WEB_MODE" in
  1|true|TRUE|yes|YES)
    RUN_WEB=1
    ;;
  0|false|FALSE|no|NO)
    RUN_WEB=0
    ;;
  auto|AUTO)
    if has_web_toolchain; then
      RUN_WEB=1
    else
      RUN_WEB=0
    fi
    ;;
  *)
    echo "Unsupported REALM_VALIDATE_WEB value: $WEB_MODE" >&2
    exit 2
    ;;
esac

if [[ "$RUN_WEB" == "1" ]]; then
  echo "Running web build validation..."
  if [[ "$INSTALL_EMSDK" == "1" ]]; then
    bash scripts/build-web.sh --install-emsdk
  else
    bash scripts/build-web.sh
  fi
else
  echo "Skipping web build: no active Emscripten toolchain was detected."
  echo "Run 'bash scripts/setup-web.sh' once, or rerun with '--include-web --install-emsdk' to opt in."
fi
