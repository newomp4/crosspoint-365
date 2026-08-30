#!/bin/bash
# Geist as the UI typeface: 1-bit, auto-hinted cuts with the same script
# coverage as the Ubuntu UI fonts. Geist carries Latin; Noto Sans fills
# Cyrillic, Greek and Vietnamese; the Noto UI cuts add Hebrew and Arabic
# (Presentation Forms, as the Ubuntu stack does). Sources: Geist static
# instances in lib/EpdFont/fontsrc/ (built from the variable font, OFL).

set -e

cd "$(dirname "$0")"

PYTHON="${PYTHON:-python}"
SRC=../builtinFonts/source

ARABIC_INTERVALS=(
  --additional-intervals 0x060C,0x060C
  --additional-intervals 0x061B,0x061B
  --additional-intervals 0x061F,0x061F
  --additional-intervals 0x0621,0x0621
  --additional-intervals 0x0640,0x0640
  --additional-intervals 0x0660,0x0669
  --additional-intervals 0x06BA,0x06BA
  --additional-intervals 0x06D4,0x06D4
  --additional-intervals 0x06F0,0x06F9
  --additional-intervals 0xFB56,0xFB59
  --additional-intervals 0xFB66,0xFB69
  --additional-intervals 0xFB7A,0xFB7D
  --additional-intervals 0xFB88,0xFB95
  --additional-intervals 0xFB9E,0xFB9F
  --additional-intervals 0xFBA6,0xFBB1
  --additional-intervals 0xFBFC,0xFBFF
  --additional-intervals 0xFE80,0xFEFC
)

gen() {
  local name="$1" size="$2" geist="$3" noto="$4" hebrew="$5" arabic="$6"
  # Every face here is unhinted or variable-instanced: the auto-hinter gives
  # them the even stems the Ubuntu cuts get from their own bytecode.
  "$PYTHON" fontconvert.py "$name" "$size" "$geist" "$noto" "$hebrew" "$arabic" --mono \
    --autohint-font "$geist" --autohint-font "$noto" --autohint-font "$hebrew" \
    --additional-intervals 0x05D0,0x05EA "${ARABIC_INTERVALS[@]}" > "../builtinFonts/${name}.h"
  echo "Generated ../builtinFonts/${name}.h"
}

MED=(../fontsrc/Geist-Medium.ttf $SRC/NotoSans/NotoSans-Regular.ttf $SRC/NotoSansHebrew/NotoSansHebrew-UIMedium.ttf $SRC/NotoSansArabic/NotoSansArabic-UIMedium.ttf)
SEMI=(../fontsrc/Geist-SemiBold.ttf $SRC/NotoSans/NotoSans-Bold.ttf $SRC/NotoSansHebrew/NotoSansHebrew-UIMedium.ttf $SRC/NotoSansArabic/NotoSansArabic-UIMedium.ttf)
BOLD=(../fontsrc/Geist-Bold.ttf $SRC/NotoSans/NotoSans-Bold.ttf $SRC/NotoSansHebrew/NotoSansHebrew-UIBold.ttf $SRC/NotoSansArabic/NotoSansArabic-UIBold.ttf)

gen geist_ui_8_medium 8 "${MED[@]}"
gen geist_ui_10_semibold 10 "${SEMI[@]}"
gen geist_ui_10_bold 10 "${BOLD[@]}"
gen geist_ui_12_semibold 12 "${SEMI[@]}"
gen geist_ui_12_bold 12 "${BOLD[@]}"
gen geist_ui_18_bold 18 "${BOLD[@]}"
