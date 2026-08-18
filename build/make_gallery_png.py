#!/usr/bin/env python3
"""Render dist/*.ZDL on-device covers to the graphics/ gallery PNGs.

The gallery images had no generator -- they were made by hand in June 2026 and
then went stale the moment any effect's knobs were renamed. Hydra's still read
WINDOW / FAST / SLOW long after those knobs became Div / Tempo / Fast.

Geometry and palette are matched to the existing files rather than invented.
Output is not byte-identical to the June set: those were rendered from an older
cover whose border bled to the frame edge, while make_cover now draws it inset
by a pixel. The framing is the same; the art is simply current.

  * canvas 640x416, background (18, 20, 24)
  * cover drawn at 630x404 inset at (5, 6), matching the original framing
  * lit pixels (180, 230, 255)

The 128x64 cover is stretched NON-UNIFORMLY (4.92x across, 6.30x down). That is
deliberate: covers are authored pre-squashed vertically because the device's
pixels are not square, so the gallery has to un-squash them to show what the
screen actually looks like.

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

try:
    from PIL import Image
except ImportError:
    raise SystemExit("Pillow is required: pip install pillow")

BG = (18, 20, 24)
LIT = (180, 230, 255)
CANVAS = (640, 416)
INNER = (630, 404)
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
