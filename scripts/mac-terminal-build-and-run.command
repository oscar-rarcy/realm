#!/bin/bash

# Realm macOS terminal build/run script.
# Run it from Terminal or double-click it in Finder.
# Pass "clean" or "--clean" to remove build outputs before rebuilding.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$SCRIPT_DIR/.." && pwd)"

# Keep the log outside build/, because optional clean builds delete build/.
mkdir -p "$REPO/logs"
LOG="$REPO/logs/mac-terminal-build.log"

CLEAN=0
for arg in "$@"; do
    case "$arg" in
        clean|--clean)
            CLEAN=1
            ;;
    esac
done

echo "Realm macOS terminal build"
echo "Repo: $REPO"
echo "Log: $LOG"
if [[ "$CLEAN" -eq 1 ]]; then
    echo "Build mode: clean"
else
    echo "Build mode: incremental"
fi
echo

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "ERROR: This script is for macOS."
    echo
    read -r -p "Press Return to close..."
    exit 1
fi

cd "$REPO" || exit 1

if [[ "$CLEAN" -eq 1 ]]; then
    echo "Cleaning and building terminal target..."
else
    echo "Building terminal target..."
fi
echo

{
    if [[ "$CLEAN" -eq 1 ]]; then
        make clean
    fi
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
