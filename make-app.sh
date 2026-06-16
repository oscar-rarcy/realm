#!/usr/bin/env bash
# Build a self-contained Realm.app (Apple Silicon) from realm-gui, bundling the
# SDL2 / freetype / harfbuzz (and their) dylibs inside the app so it runs on a
# Mac that has never seen Homebrew. Fonts are loaded from macOS system paths
# (Menlo/SF Mono/Monaco/Luminari), present on every Mac, so none are bundled.
#
# Usage:  ./make-app.sh      (or: make app)
# Result: Realm.app — zip it and send it to a friend. First launch on their
#         Mac: right-click the app -> Open (Gatekeeper prompt for ad-hoc sign).
set -euo pipefail
cd "$(dirname "$0")"

APP="Realm.app"
EXE_NAME="Realm"
FW="$APP/Contents/Frameworks"
EXE="$APP/Contents/MacOS/$EXE_NAME"

echo "==> Building realm-gui ..."
make gui-build

echo "==> Assembling $APP ..."
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$FW" "$APP/Contents/Resources"
cp realm-gui "$EXE"
chmod +x "$EXE"

# Recursively gather every non-system dylib the binary needs. (Plain arrays +
# a newline-delimited "seen" string, so this works on macOS's stock bash 3.2.)
queue=()
seen=$'\n'
collect_deps() {
  while IFS= read -r line; do
    dep=$(echo "$line" | awk '{print $1}')
    case "$dep" in
      /opt/homebrew/*|/usr/local/*)
        case "$seen" in
          *$'\n'"$dep"$'\n'*) : ;;                 # already collected
          *) seen="$seen$dep"$'\n'; queue+=("$dep"); collect_deps "$dep" ;;
        esac ;;
    esac
  done < <(otool -L "$1" | tail -n +2)
}
collect_deps "$EXE"

echo "==> Copying ${#queue[@]} dylibs into the bundle ..."
for src in "${queue[@]}"; do
  cp -f "$src" "$FW/$(basename "$src")"
  chmod u+w "$FW/$(basename "$src")"
done

# Rewrite install names: each bundled dylib's id -> @rpath/<base>, and every
# reference (in the exe and in each dylib) to a bundled lib -> @rpath/<base>.
fix_refs() {
  for src in "${queue[@]}"; do
    install_name_tool -change "$src" "@rpath/$(basename "$src")" "$1" 2>/dev/null || true
  done
}
for src in "${queue[@]}"; do
  base=$(basename "$src")
  install_name_tool -id "@rpath/$base" "$FW/$base"
  fix_refs "$FW/$base"
done
fix_refs "$EXE"
install_name_tool -add_rpath "@executable_path/../Frameworks" "$EXE" 2>/dev/null || true

# --- Optional app icon -------------------------------------------------------
# To set the icon, drop ONE of these in the repo root and re-run (checked in
# this order):
#   * *.icon      — an Icon Composer document; compiled with actool so its
#                   gradient/glass/shadow effects are baked in (and a layered
#                   Assets.car is emitted for Liquid Glass on macOS 26+).
#   * Realm.icns  — a ready-made macOS icon, used as-is
#   * icon.png    — a square PNG (1024x1024 ideal); it's converted to .icns
# With none present the app keeps the generic icon.
ICON_SET=0          # 1 once an icon is installed
ICON_NAME=""        # basename used for CFBundleIconFile / CFBundleIconName
ICON_HAS_CAR=0      # 1 if an Assets.car was produced (needs CFBundleIconName)
if ls *.icon >/dev/null 2>&1; then
  SRC_ICON="$(ls -1d *.icon | head -1)"
  ICON_NAME="$(basename "${SRC_ICON%.icon}")"
  echo "==> Compiling $SRC_ICON with actool ..."
  ICONOUT="$(mktemp -d)"
  if xcrun actool "$SRC_ICON" --compile "$ICONOUT" --platform macosx \
       --minimum-deployment-target 11.0 --app-icon "$ICON_NAME" \
       --output-partial-info-plist "$ICONOUT/icon.plist" >/dev/null 2>&1 \
     && [ -f "$ICONOUT/$ICON_NAME.icns" ]; then
    cp "$ICONOUT/$ICON_NAME.icns" "$APP/Contents/Resources/$ICON_NAME.icns"
    if [ -f "$ICONOUT/Assets.car" ]; then
      cp "$ICONOUT/Assets.car" "$APP/Contents/Resources/Assets.car"; ICON_HAS_CAR=1
    fi
    ICON_SET=1
  else
    echo "   actool failed; falling back to Realm.icns / icon.png if present."
    ICON_NAME=""
  fi
fi
if [ "$ICON_SET" = 0 ] && [ -f Realm.icns ]; then
  cp Realm.icns "$APP/Contents/Resources/Realm.icns"; ICON_NAME="Realm"; ICON_SET=1
elif [ "$ICON_SET" = 0 ] && [ -f icon.png ]; then
  echo "==> Building Realm.icns from icon.png ..."
  ICONSET="$(mktemp -d)/Realm.iconset"; mkdir -p "$ICONSET"
  for sz in 16 32 128 256 512; do
    sips -z $sz $sz           icon.png --out "$ICONSET/icon_${sz}x${sz}.png"    >/dev/null
    sips -z $((sz*2)) $((sz*2)) icon.png --out "$ICONSET/icon_${sz}x${sz}@2x.png" >/dev/null
  done
  iconutil -c icns "$ICONSET" -o "$APP/Contents/Resources/Realm.icns"
  ICON_NAME="Realm"; ICON_SET=1
fi

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleName</key><string>Realm</string>
  <key>CFBundleDisplayName</key><string>Realm</string>
  <key>CFBundleIdentifier</key><string>com.oscar.realm</string>
  <key>CFBundleVersion</key><string>1.0</string>
  <key>CFBundleShortVersionString</key><string>1.0</string>
  <key>CFBundleExecutable</key><string>Realm</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHighResolutionCapable</key><true/>
</dict>
</plist>
PLIST

# Point the bundle at the icon (only if one was produced). Signing happens
# after this, so the icon is covered by the signature. CFBundleIconFile names
# the .icns; CFBundleIconName points at the Assets.car icon (preferred by the
# system when present).
if [ "$ICON_SET" = 1 ]; then
  ICON_KEYS="  <key>CFBundleIconFile</key><string>$ICON_NAME</string>"
  if [ "$ICON_HAS_CAR" = 1 ]; then
    ICON_KEYS="$ICON_KEYS\\
  <key>CFBundleIconName</key><string>$ICON_NAME</string>"
  fi
  sed -i '' "s#<key>CFBundleExecutable</key><string>Realm</string>#&\\
$ICON_KEYS#" "$APP/Contents/Info.plist"
  echo "==> App icon set ($ICON_NAME$( [ "$ICON_HAS_CAR" = 1 ] && echo ", with Assets.car" ))."
fi

# Ad-hoc sign the dylibs, then the whole bundle. This lets a friend right-click
# -> Open (a paid Developer ID + notarization would remove the prompt entirely).
echo "==> Ad-hoc signing ..."
for dylib in "$FW"/*.dylib; do codesign --force -s - "$dylib"; done
codesign --force --deep -s - "$APP"

echo "==> Checking the bundle is self-contained ..."
if otool -L "$EXE" "$FW"/*.dylib | grep -E '/opt/homebrew|/usr/local'; then
  echo "WARNING: residual Homebrew references listed above — not fully portable."
  exit 1
fi
echo "OK: no external Homebrew deps (only @rpath + /usr/lib + system frameworks)."
echo "==> Done: $APP   (zip it to share: ditto -c -k --keepParent Realm.app Realm.zip)"
