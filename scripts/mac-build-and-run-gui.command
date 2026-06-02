#!/bin/bash

# Realm macOS GUI build/run script.
# Run it from Terminal or double-click it in Finder.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

# Keep the log outside build/, because `make clean` deletes build/.
mkdir -p "$REPO/logs"
LOG="$REPO/logs/mac-gui-build.log"

echo "Realm macOS GUI build"
echo "Repo: $REPO"
echo "Log: $LOG"
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: This script is for macOS."
    echo
    read -r -p "Press Return to close..."
    exit 1
fi

if ! command -v brew >/dev/null 2>&1; then
    echo "ERROR: Homebrew was not found."
    echo
    echo "Install Homebrew from https://brew.sh, then run this script again."
    echo
    read -r -p "Press Return to close..."
    exit 1
fi

cd "$REPO" || exit 1

echo "Installing/checking Homebrew build dependencies..."
echo "Cleaning and building GUI target..."
echo

{
    brew install pkg-config sdl2 sdl2_ttf
    make clean
    make gui
    test -x bin/realm-gfx
} > "$LOG" 2>&1

EXITCODE=$?

echo
echo "Finished build with exit code: $EXITCODE"
echo

if [[ "$EXITCODE" -ne 0 ]]; then
    echo "Build failed. Log output:"
    echo "----------------------------------------"
    cat "$LOG"
    echo "----------------------------------------"
    read -r -p "Press Return to close..."
    exit "$EXITCODE"
fi

echo "Build succeeded."
echo "Starting Realm GUI..."
echo

"$REPO/bin/realm-gfx"

EXITCODE=$?

echo
echo "Realm exited with code: $EXITCODE"
echo "Log saved to:"
echo "$LOG"
echo

read -r -p "Press Return to close..."
exit "$EXITCODE"
