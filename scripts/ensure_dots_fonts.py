"""
Pre-build: make sure the Helvetica Neue sleep-screen font headers exist.

Helvetica Neue is a proprietary font, so its converted bitmap headers are not
committed (see .gitignore). On a machine that has run
lib/EpdFont/scripts/convert-year-fonts.sh they exist and this is a no-op;
elsewhere a fallback header is written that aliases each size to the matching
Geist Bold cut, so the firmware still builds and the "Helvetica Neue Bold"
option renders with Geist Bold.
"""
import os

try:
    Import("env")  # noqa: F821 - provided by PlatformIO/SCons
    PROJECT_DIR = env.subst("$PROJECT_DIR")  # noqa: F821
except NameError:
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

FONT_DIR = os.path.join(PROJECT_DIR, "lib", "EpdFont", "builtinFonts")
SIZES = (14, 24, 40)

for size in SIZES:
    path = os.path.join(FONT_DIR, f"helveticaneue_{size}_bold.h")
    if os.path.exists(path):
        continue
    with open(path, "w", encoding="utf-8") as f:
        f.write(
            "// Fallback written by scripts/ensure_dots_fonts.py: Helvetica Neue is a\n"
            "// proprietary font and its converted bitmaps are not committed. Run\n"
            "// lib/EpdFont/scripts/convert-year-fonts.sh on a Mac to build the real cut;\n"
            "// until then the \"Helvetica Neue Bold\" option renders with Geist Bold.\n"
            "#pragma once\n"
            f"#include <builtinFonts/geist_{size}_bold.h>\n"
            f"#define helveticaneue_{size}_bold geist_{size}_bold\n"
        )
    print(f"ensure_dots_fonts: wrote Geist fallback for helveticaneue_{size}_bold.h")
