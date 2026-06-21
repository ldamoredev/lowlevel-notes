#!/usr/bin/env bash
# Rasterize the SVG brand sources into every PNG the site references.
# Run once after editing static/favicon.svg or static/assets/og-image.svg.
#
# Rasterizer preference (first one found wins):
#   rsvg-convert   (brew install librsvg)   — fastest, recommended
#   magick         (brew install imagemagick)
#   inkscape       (brew install --cask inkscape)
#   qlmanage+sips  (macOS built-in fallback, no install required)
#
# Outputs:
#   static/apple-touch-icon.png   180x180   (iOS home screen)
#   static/assets/icon-192.png    192x192   (PWA standard)
#   static/assets/icon-512.png    512x512   (PWA hi-res / splash)
#   static/assets/og-image.png    1200x630  (social link previews)
set -euo pipefail
cd "$(dirname "$0")/.."

SRC_ICON="static/favicon.svg"
SRC_OG="static/assets/og-image.svg"

raster_tool=""
for t in rsvg-convert magick inkscape qlmanage; do
  if command -v "$t" >/dev/null 2>&1; then raster_tool="$t"; break; fi
done
if [ -z "$raster_tool" ]; then
  echo "No SVG rasterizer found. Install librsvg/imagemagick/inkscape, or run on macOS (qlmanage)." >&2
  exit 1
fi
echo "Using rasterizer: $raster_tool"

# render <src> <width> <height> <out>
render() {
  local src="$1" w="$2" h="$3" out="$4"
  case "$raster_tool" in
    rsvg-convert) rsvg-convert -w "$w" -h "$h" "$src" -o "$out" ;;
    magick)       magick -background none "$src" -resize "${w}x${h}" "$out" ;;
    inkscape)     inkscape "$src" -w "$w" -h "$h" -o "$out" >/dev/null 2>&1 ;;
    qlmanage)
      local tmp; tmp="$(mktemp -d)"
      local box="$w"; [ "$h" -gt "$w" ] && box="$h"
      qlmanage -t -s "$box" -o "$tmp" "$src" >/dev/null 2>&1
      if [ "$w" -eq "$h" ]; then
        mv "$tmp/$(basename "$src").png" "$out"
      else
        # qlmanage pads to a square box; crop the centered content to w x h.
        sips -c "$h" "$w" "$tmp/$(basename "$src").png" --out "$out" >/dev/null
      fi
      rm -rf "$tmp"
      ;;
  esac
  echo "  wrote $out (${w}x${h})"
}

render "$SRC_ICON" 180 180   static/apple-touch-icon.png
render "$SRC_ICON" 192 192   static/assets/icon-192.png
render "$SRC_ICON" 512 512   static/assets/icon-512.png
render "$SRC_OG"   1200 630  static/assets/og-image.png

echo "Done."
