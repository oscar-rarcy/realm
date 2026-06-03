#!/bin/bash

# Realm macOS terminal build/run script.
# Run it from Terminal or double-click it in Finder.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

# Keep the log outside build/, because `make clean` deletes build/.
mkdir -p "$REPO/logs"
LOG="$REPO/logs/mac-terminal-build.log"

echo "Realm macOS terminal build"
echo "Repo: $REPO"
echo "Log: $LOG"
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: This script is for macOS."
    echo
    read -r -p "Press Return to close..."
    exit 1
fi

cd "$REPO" || exit 1

echo "Cleaning and building terminal target..."
echo

{
    make clean
    make terminal
    test -x bin/realm
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
echo "Starting Realm terminal renderer..."
echo

"$REPO/bin/realm"

EXITCODE=$?

echo
echo "Realm exited with code: $EXITCODE"
echo "Log saved to:"
echo "$LOG"
echo

read -r -p "Press Return to close..."
exit "$EXITCODE"
