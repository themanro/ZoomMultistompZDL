#!/usr/bin/env python3
"""Render dist/*.ZDL on-device covers to the graphics/ gallery PNGs.

The gallery images had no generator -- they were made by hand in June 2026 and
then went stale the moment any effect's knobs were renamed. Hydra's still read
WINDOW / FAST / SLOW long after those knobs became Div / Tempo / Fast.

Geometry and palette are matched to the existing files rather than invented.
Output is not byte-identical to the June set: those were rendered from an older
cover whose border bled to the frame edge, while make_cover now draws it inset
by a pixel. The framing is the same; the art is simply current.

The cover is decoded from the ZDL, then scaled with nearest-neighbor pixels.
The physical pixel aspect comes from lcd_geometry.PIXEL_ASPECT (about 1.4),
so a 128x64 bitmap is displayed at approximately 10:7, matching PE. No VHS
filter is applied to the release gallery.

Usage:
    python3 build/make_gallery_png.py            # every effect in dist/
    python3 build/make_gallery_png.py Stasis     # just one
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build"))

from decode_picture import decode_picture  # noqa: E402
from make_thumb_png import vhs  # noqa: E402

try:
    from PIL import Image
except ImportError:
    raise SystemExit("Pillow is required: pip install pillow")

# P4 "television white blend" -- the editor's TV phosphor theme, so the gallery,
# the hero and the editor all look like the same machine. The June palette was the
# light-blue LCD skin and clashed the moment the hero went to tape.
BG = (8, 9, 11)
LIT = (157, 193, 216)
from lcd_geometry import PIXEL_ASPECT
INNER = (630, round(630 * 64 / 128 * PIXEL_ASPECT))
CANVAS = (640, INNER[1] + 12)
ORIGIN = (5, 6)


def render(zdl: Path, out: Path) -> None:
    px, _name = decode_picture(str(zdl))
    h = len(px)
    w = len(px[0]) if h else 0
    cover = Image.new("RGB", (w, h), BG)
    load = cover.load()
    for y in range(h):
        row = px[y]
        for x in range(w):
            if row[x]:
                load[x, y] = LIT
    cover = cover.resize(INNER, Image.NEAREST)
    canvas = Image.new("RGB", CANVAS, BG)
    canvas.paste(cover, ORIGIN)
    # Gentler than the hero: these render at 220px in the README table, where the
    # full tape pass turns knob labels into texture. Seeded per effect so each one
    # gets its own dropouts instead of 21 identically-damaged frames.
    # Native artwork: no texture that obscures pixels at pedal size.
    out.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(out)


def main() -> int:
    wanted = sys.argv[1:]
    dist = ROOT / "dist"
    gfx = ROOT / "graphics"
    zdls = sorted(dist.glob("*.ZDL"))
    if wanted:
        keep = {w.lower() for w in wanted}
        zdls = [z for z in zdls if z.stem.lower() in keep]
        if not zdls:
            print(f"no match in dist/ for {wanted}", file=sys.stderr)
            return 1
    for z in zdls:
        out = gfx / f"{z.stem.lower()}.png"
        render(z, out)
        print(f"  {z.stem:<10} -> {out.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
