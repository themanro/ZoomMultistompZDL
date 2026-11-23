#!/usr/bin/env python3
"""The README's main picture: the painted lab console, brought to life.

1. The painting (graphics/13-ivory-console-film.png) is edited: most of its
   Cyrillic labels are lifted off the metal and reprinted as short English or
   symbol marks in a condensed industrial face; the painted needles, the scope
   trace, the pedal's screen and the handwritten notes are removed.
2. It gets the phosphor colouring of graphics/make_phosphor_hero.py.
3. build/hero/paint.html draws what moves on top (the pedal's screen playing
   the editor's effect pictures, the needles, the scope trace, the LEDs), and
   build/hero_capture.cjs records one loop of it as graphics/hero.webp.

    python3 -B build/make_paint_hero.py            # base + animation
    python3 -B build/make_paint_hero.py --base     # just build/hero/paint-base.png

Needs numpy, scipy, Pillow, and Playwright for Node (see hero_capture.cjs).
"""
import argparse
import os
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont
from scipy import ndimage as ndi

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "graphics" / "13-ivory-console-film.png"
BASE = ROOT / "build" / "hero" / "paint-base.png"
OUT = ROOT / "graphics" / "hero.webp"
FONTS = "/System/Library/Fonts/Supplemental/"
COND, WIDE = FONTS + "DIN Condensed Bold.ttf", FONTS + "DIN Alternate Bold.ttf"

# (box x0, y0, x1, y1), ink: "dark" on light metal / "light" on dark, new text
# (lines split by |), font. Boxes are the old text's extent in the painting.
LABELS = [
    ((572, 22, 952, 60), "dark", "MULTISTOMP", COND),
    ((592, 63, 952, 100), "dark", "EFFECTS LABORATORY", COND),
    ((1312, 44, 1468, 64), "dark", "48 EFFECTS", COND),
    ((1318, 66, 1466, 86), "dark", "NAM · CAB · EDITOR", COND),
    ((300, 176, 340, 194), "dark", "PWR", COND),
    ((304, 315, 337, 334), "dark", "ON", COND),
    ((64, 391, 200, 412), "dark", "ZERO", COND),
    ((58, 612, 236, 633), "dark", "THRESHOLD", COND),
    ((57, 811, 119, 831), "dark", "REC", COND),
    ((161, 811, 204, 831), "dark", "PLAY", COND),
    ((251, 811, 295, 831), "dark", "STOP", COND),
    ((361, 824, 419, 845), "dark", "MODE", COND),
    ((626, 802, 764, 823), "dark", "EXT IN", COND),
    ((880, 799, 945, 819), "dark", "DATA", COND),
    ((1068, 519, 1234, 541), "dark", "SPECTRUM", COND),
    ((1058, 727, 1085, 748), "dark", "LO", COND),
    ((1168, 727, 1198, 748), "dark", "MID", COND),
    ((1278, 727, 1307, 748), "dark", "HI", COND),
    ((1391, 481, 1492, 501), "dark", "MOD", COND),
    ((1403, 699, 1477, 719), "dark", "DEPTH", COND),
    ((1101, 764, 1262, 786), "dark", "EXPANSION", COND),
    ((1246, 147, 1322, 169), "dark", "CH II", COND),
    ((1391, 147, 1447, 169), "dark", "LIMIT", COND),
    ((141, 258, 167, 277), "dark", "dB", COND),
    ((58, 151, 227, 171), "light", "SIGNAL", COND),
    ((406, 151, 547, 171), "light", "PHASE", COND),
    ((1033, 135, 1150, 153), "light", "X · Y", COND),
    ((630, 311, 666, 327), "dark", "MONO", COND),
    ((870, 311, 906, 327), "dark", "MONO", COND),
    ((646, 541, 713, 561), "dark", "MS-70CDR", WIDE),
    ((819, 542, 895, 557), "dark", "CHORUS", WIDE),
    ((819, 559, 895, 574), "dark", "DELAY", WIDE),
    ((819, 576, 895, 591), "dark", "REVERB", WIDE),
]
# Pedal name: "MULTI" underlined, as the painting's.
PEDAL_NAME = ((657, 512, 882, 538), "MULTISTOMP")
# Handwriting (blue ink) to lift off, nothing reprinted.
NOTES = [(392, 486, 516, 552), (136, 528, 240, 594), (1330, 878, 1506, 1004)]
# Painted needles: line segments to lift off the meter faces.
NEEDLES = [((70, 231), (131, 293)), ((402, 231), (455, 293))]
SCOPE = (992, 186, 1204, 360)                 # the trace's area on the scope screen
LCD = (677, 251, 859, 394)                    # the pedal's screen, redrawn live


def lum(a):
    return 0.299 * a[..., 0] + 0.587 * a[..., 1] + 0.114 * a[..., 2]


def fill(a, mask, donor_shift=None):
    """Fill `mask` from its surroundings (normalized convolution at growing
    scales), then put back fine grain copied from a nearby unmasked spot.
    Works on a crop around the mask."""
    ys, xs = np.nonzero(mask)
    if not len(ys):
        return a
    if donor_shift is None:
        donor_shift = quiet_shift(a, mask, ys, xs)
    dy, dx = donor_shift
    pad = 64
    y0, y1 = max(0, ys.min() - pad - abs(dy)), min(a.shape[0], ys.max() + pad + abs(dy))
    x0, x1 = max(0, xs.min() - pad - abs(dx)), min(a.shape[1], xs.max() + pad + abs(dx))
    out = a.copy()
    out[y0:y1, x0:x1] = _fill(a[y0:y1, x0:x1], mask[y0:y1, x0:x1], donor_shift)
    return out


def quiet_shift(a, mask, ys, xs):
    """Of a few nearby spots the mask's size away, the one with the least
    detail (no text or edges), to copy grain from."""
    h, w = ys.max() - ys.min() + 1, xs.max() - xs.min() + 1
    L = lum(a)
    best, pick = None, (h + 6, 0)
    for dy, dx in [(h + 6, 0), (-(h + 6), 0), (0, w + 6), (0, -(w + 6)), (2 * h + 10, 0), (-(2 * h + 10), 0),
                   (h + 6, w + 6), (-(h + 6), -(w + 6)), (h + 6, -(w + 6)), (-(h + 6), w + 6)]:
        y0, y1, x0, x1 = ys.min() + dy, ys.max() + dy + 1, xs.min() + dx, xs.max() + dx + 1
        if y0 < 0 or x0 < 0 or y1 > a.shape[0] or x1 > a.shape[1]:
            continue
        r = L[y0:y1, x0:x1]
        e = np.abs(r - ndi.gaussian_filter(r, 2)).mean() + 2 * np.abs(r - np.median(r)).mean()
        if best is None or e < best:
            best, pick = e, (dy, dx)
    # the grain is copied with np.roll: a shift of +d takes pixels from -d
    return (-pick[0], -pick[1])


def _fill(a, mask, donor_shift):
    keep = (~mask).astype(float)
    out = a.copy()
    todo = mask.copy()
    for s in (1.5, 3, 6, 12, 24):
        num = np.stack([ndi.gaussian_filter(a[..., k] * keep, s) for k in range(3)], -1)
        den = ndi.gaussian_filter(keep, s)
        ok = todo & (den > 0.25)
        out[ok] = num[ok] / den[ok, None]
        todo &= ~ok
    # grain: generated to match the surrounding metal's fine texture (a copy
    # from nearby would bring its marks along), plus a few rust specks
    grain = a - np.stack([ndi.gaussian_filter(a[..., k], 1.6) for k in range(3)], -1)
    ring = ndi.binary_dilation(mask, iterations=10) & ~mask
    sd = grain[ring].std(0) if ring.any() else np.full(3, 0.03)
    rng = np.random.default_rng(int(mask.sum()) * 7919 + mask.shape[0])
    n = ndi.gaussian_filter(rng.normal(size=mask.shape), 0.7)
    n /= n.std() + 1e-9
    spk = ndi.gaussian_filter((rng.random(mask.shape) < 0.003).astype(float), 1.1)
    spk /= spk.max() + 1e-9
    out[mask] += n[mask, None] * sd[None] * 0.95 - spk[mask, None] * 0.22
    return out


def ink_mask(a, box, kind, thr=0.10, grow=2):
    x0, y0, x1, y1 = box
    m = np.zeros(a.shape[:2], bool)
    P = 30
    cy0, cx0 = max(0, y0 - P), max(0, x0 - P)
    crop = a[cy0:y1 + P, cx0:x1 + P]
    Lc = lum(crop)
    medc = ndi.median_filter(Lc, size=25)
    L = lum(a)
    sub = (Lc - medc)[y0 - cy0:y1 - cy0, x0 - cx0:x1 - cx0]
    m[y0:y1, x0:x1] = sub < -thr if kind == "dark" else sub > thr
    if kind == "blue":
        r = a[y0:y1, x0:x1]
        b = r[..., 2] - (r[..., 0] + r[..., 1]) / 2
        m[:] = False
        m[y0:y1, x0:x1] = (b > 0.015) & (lum(r) < 0.66)
    return ndi.binary_dilation(m, iterations=grow)


def pad(box, p):
    return (box[0] - p, box[1] - p, box[2] + p, box[3] + p)


def ink_of(a, box, kind):
    """The old letters' ink: the darkest (or lightest) few percent of the box."""
    x0, y0, x1, y1 = box
    r = a[y0:y1, x0:x1].reshape(-1, 3)
    L = lum(r)
    sel = L <= np.percentile(L, 6) if kind == "dark" else L >= np.percentile(L, 94)
    return r[sel].mean(0)


def reprint(img, box, text, font_path, color, align="center"):
    """Text set to the old text's cap height, centred on its box, worn a little."""
    x0, y0, x1, y1 = box
    h = y1 - y0
    size = 10
    while True:
        f = ImageFont.truetype(font_path, size + 1)
        if f.getbbox("H")[3] - f.getbbox("H")[1] > h * 0.82:
            break
        size += 1
    f = ImageFont.truetype(font_path, size)
    layer = Image.new("L", img.size, 0)
    d = ImageDraw.Draw(layer)
    bb = d.textbbox((0, 0), text, font=f)
    hb = f.getbbox("H")
    tx = (x0 + x1) / 2 - (bb[2] - bb[0]) / 2 - bb[0] if align == "center" else x0 - bb[0]
    ty = (y0 + y1) / 2 - (hb[3] + hb[1]) / 2
    d.text((tx, ty), text, font=f, fill=255)
    layer = layer.filter(ImageFilter.GaussianBlur(0.45))
    rng = np.random.default_rng(abs(hash(text)) % 2**32)
    wear = ndi.gaussian_filter(rng.random(img.size[::-1]), 1.2)
    wear = np.clip((wear - wear.mean()) * 6 + 0.92, 0.55, 1)
    alpha = np.asarray(layer, float) / 255 * wear
    a = np.asarray(img, float) / 255
    out = a * (1 - alpha[..., None]) + np.array(color)[None, None] * alpha[..., None]
    return Image.fromarray(np.clip(out * 255, 0, 255).round().astype(np.uint8)), (tx, ty, f)


def edit(src):
    a = np.asarray(src.convert("RGB"), float) / 255
    jobs = []
    for box, kind, text, font in LABELS:
        jobs.append((box, text, font, ink_of(a, box, kind)))
        a = fill(a, ink_mask(a, pad(box, 4), kind, grow=2))
    # the pedal's name
    box, name = PEDAL_NAME
    pink = ink_of(a, box, "dark")
    a = fill(a, ink_mask(a, pad(box, 5), "dark", thr=0.10, grow=2))
    for box in NOTES:
        a = fill(a, ink_mask(a, box, "blue", grow=3))
    for (p, q) in NEEDLES:
        yy, xx = np.mgrid[0:a.shape[0], 0:a.shape[1]]  # cheap enough: two needles
        p, q = np.array(p, float), np.array(q, float)
        d = q - p
        t = np.clip(((xx - p[0]) * d[0] + (yy - p[1]) * d[1]) / (d @ d), 0, 1)
        dist = np.hypot(xx - (p[0] + t * d[0]), yy - (p[1] + t * d[1]))
        a = fill(a, dist < 7.5)
    # the scope trace: its bright strokes, lifted off the grid
    x0, y0, x1, y1 = SCOPE
    m = np.zeros(a.shape[:2], bool)
    Lc = lum(a[y0:y1, x0:x1])
    m[y0:y1, x0:x1] = (Lc - ndi.median_filter(Lc, size=15)) > 0.12
    a = fill(a, ndi.binary_dilation(m, iterations=3), (0, 0))
    img = Image.fromarray(np.clip(a * 255, 0, 255).round().astype(np.uint8))
    for box, text, font, ink in jobs:
        img, _ = reprint(img, box, text, font, ink)
    box, name = PEDAL_NAME
    img, (tx, ty, f) = reprint(img, box, name, WIDE, pink)
    d = ImageDraw.Draw(img)
    hb = f.getbbox("H")
    uy = ty + hb[3] + 3
    d.rectangle([tx + f.getbbox("M")[0], uy, tx + f.getlength("MULTI") - 1, uy + 2], fill=tuple(int(c * 255) for c in pink))
    return img


# The phosphor colouring of graphics/15-console-phosphor.jpg (make_phosphor_hero.py),
# with its ramp as it was then: make_thumb_png's LIT has since changed.
CURVE = 1.25
RAMP = [(0.0, (12, 13, 16)), (0.35, (34, 52, 66)), (0.75, (158, 192, 214)), (1.0, (226, 240, 248))]


def phosphor(img):
    a = np.asarray(img.convert("RGB"), float) / 255
    L = lum(a)
    lo, hi = np.percentile(L, 0.5), np.percentile(L, 99.7)
    t = np.clip((L - lo) / (hi - lo), 0, 1) ** CURVE
    xs = [s for s, _ in RAMP]
    cs = np.array([c for _, c in RAMP], float)
    out = np.stack([np.interp(t, xs, cs[:, k]) for k in range(3)], -1)
    sat = a.max(-1) - a.min(-1)
    accent = np.clip((sat - 0.35) * 3, 0, 1) * np.clip((a.max(-1) - 0.45) * 3, 0, 1)
    out = out * (1 - accent[..., None]) + a * 255 * accent[..., None]
    return Image.fromarray(np.clip(out, 0, 255).round().astype(np.uint8))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base", action="store_true", help="only write the still base")
    ap.add_argument("--fps", type=int, default=12)
    ap.add_argument("--quality", type=int, default=82)
    a = ap.parse_args()
    base = phosphor(edit(Image.open(SRC)))
    BASE.parent.mkdir(parents=True, exist_ok=True)
    base.save(BASE)
    print(f"-> {BASE.relative_to(ROOT)}")
    if a.base:
        return
    with tempfile.TemporaryDirectory() as td:
        subprocess.run(["node", str(ROOT / "build" / "hero_capture.cjs"), td, str(a.fps), "--page", "paint.html"],
                       check=True, env=os.environ)
        frames = [Image.open(p).convert("RGB") for p in sorted(Path(td).glob("*.png"))]
        # Only the screen, needles, scope and LEDs change: one full frame, then
        # just the changed areas (the painting's grain is costly to repeat).
        frames[0].save(OUT, save_all=True, append_images=frames[1:], duration=round(1000 / a.fps), loop=0,
                       quality=a.quality, method=4, minimize_size=True, kmin=len(frames) - 1, kmax=len(frames))
    print(f"-> {OUT.relative_to(ROOT)}: {len(frames)} frames, {OUT.stat().st_size / 1e6:.2f} MB")


if __name__ == "__main__":
    main()
