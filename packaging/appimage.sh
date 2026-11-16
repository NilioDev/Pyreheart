#!/bin/sh
# Packs a built game into Pyreheart-x86_64.AppImage in the current folder:
#   packaging/appimage.sh <build dir> <path to linuxdeploy>
set -e
BUILD=$(cd "${1:-build}" && pwd)
LINUXDEPLOY=${2:-linuxdeploy}
HERE=$(cd "$(dirname "$0")" && pwd)
WORK=$(mktemp -d)
APPDIR="$WORK/AppDir"
mkdir -p "$APPDIR/usr/bin"
cp "$BUILD/pyreheart" "$APPDIR/usr/bin/"
# the game looks for its font next to itself
cp -r "$HERE/../assets" "$APPDIR/usr/bin/assets"
cp "$HERE/../assets/icon.png" "$WORK/pyreheart.png"
export APPIMAGE_EXTRACT_AND_RUN=1 OUTPUT=Pyreheart-x86_64.AppImage
"$LINUXDEPLOY" --appdir "$APPDIR" -e "$APPDIR/usr/bin/pyreheart" \
  -d "$HERE/pyreheart.desktop" -i "$WORK/pyreheart.png" --output appimage
