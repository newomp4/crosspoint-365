#!/bin/bash
# Sleep-screen display fonts for the Year Progress sleep screen.
# ASCII-only (plus a few typographic symbols), 2-bit antialiased, uncompressed
# so they render without the glyph decompressor. Sources live in
# lib/EpdFont/fontsrc/ (gitignored): HelveticaNeue-Bold.ttf extracted from the
# macOS system TTC, Geist-{Medium,Bold}.ttf instanced from Geist's variable font.

set -e

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python}"
SIZES=(14 24 40)
INTERVALS=(
  --no-default-intervals
  --additional-intervals 0x0020,0x007E  # printable ASCII
  --additional-intervals 0x00B0,0x00B0  # degree sign
  --additional-intervals 0x00B7,0x00B7  # middle dot
  --additional-intervals 0x2013,0x2014  # en / em dash
  --additional-intervals 0x2022,0x2022  # bullet
  --additional-intervals 0x2026,0x2026  # ellipsis
)

convert() {
  local name="$1" src="$2" size="$3"
  "$PYTHON" fontconvert.py "$name" "$size" "$src" --2bit "${INTERVALS[@]}" > "../builtinFonts/${name}.h"
  echo "Generated ../builtinFonts/${name}.h"
}

for size in "${SIZES[@]}"; do
  convert "helveticaneue_${size}_bold" ../fontsrc/HelveticaNeue-Bold.ttf "$size"
  convert "geist_${size}_bold" ../fontsrc/Geist-Bold.ttf "$size"
  convert "geist_${size}_medium" ../fontsrc/Geist-Medium.ttf "$size"
done
