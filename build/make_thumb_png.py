#!/usr/bin/env python3
"""Render graphics/thumb_effects.png -- the README hero -- as a VHS screengrab.

The old hero was made by hand in June 2026 and said "x16" long after the pack
passed sixteen effects; it was already three effects stale when the count reached
21. This generates it, so the count and the artwork can never drift again.

Every card is the effect's REAL on-device cover, decoded straight out of the
built .ZDL -- the same 128x64 bitmap the pedal puts on its screen, title bar,
emblem, knob names and all. Nothing here is a mock-up of the pedal, which is why
the hero cannot show a knob name the firmware does not have.

Palette is the editor's P4 "television white blend" phosphor theme, read from the
same values patch_editor.html uses, so the hero and the editor look like the same
machine.

The VHS pass models the tape rather than sprinkling noise. In order:

  * chroma smeared horizontally ~20x harder than luma, because VHS carries colour
    at a fraction of luma bandwidth -- this is the artefact that actually reads as
    "tape" rather than "old TV";
  * per-line horizontal jitter, correlated down the frame, from the capstan;
  * chroma misregistration, R and B pulled opposite ways;
  * dropouts as short bright dashes where oxide has shed;
  * head-switching tear in the bottom ~14 lines, the giveaway of a helical scan;
  * scanlines, bloom into the dark, tape grain, vignette.

Seeded, so a rebuild produces the same frame and the file does not churn in git.

Usage:
    python3 build/make_thumb_png.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT / "src" / "airwindows" / "common"))

from decode_picture import decode_picture  # noqa: E402
from screen_image import Canvas  # noqa: E402

try:
    from PIL import Image, ImageFilter
except ImportError:
    raise SystemExit("Pillow is required: pip install pillow")

# ---- P4 "television white blend", straight from patch_editor.html -----------
BG = (8, 9, 11)          # --lcd
CARD_BG = (4, 4, 5)      # --screen-bg
LIT = (157, 193, 216)    # --ink-bright / --screen-fg
DIM = (108, 134, 150)    # --ink-soft

CANVAS = (1600, 900)
COLS, ROWS = 6, 4
CARD = (244, 156)        # 128x64 stretched the way the device's non-square pixels want
GAP = 12
SEED = 20260827


def draw_text(grid, text, x0, y0, scale=1, spacing=1):
    """Blit the cover font into a bool array. Same 3x5 glyphs the covers use."""
    x = x0
    for ch in text:
        rows = Canvas._FONT.get(ch.upper(), Canvas._FONT[" "])
        for ry, row in enumerate(rows):
            for rx, bit in enumerate(row):
                if bit == "1":
                    ys = y0 + ry * scale
                    xs = x + rx * scale
                    grid[ys:ys + scale, xs:xs + scale] = True
        x += 3 * scale + spacing
    return x


def title_card(n_effects: int, w: int, h: int) -> Image.Image:
    """The one card that is not a cover: pack name and a count that is computed."""
    gw, gh = 256, 64
    g = np.zeros((gh, gw), dtype=bool)
    draw_text(g, "CUSTOM", 8, 6, scale=4, spacing=3)
    draw_text(g, "EFFECTS", 8, 26, scale=4, spacing=3)
    # Scale 2, not 1: a 5px-tall glyph does not survive the chroma smear and the
    # line jitter -- at scale 1 this line came out of the tape pass as texture.
    draw_text(g, "ZOOM MS-70CDR - ZDL PACK", 8, 50, scale=2, spacing=2)

    # Count badge: filled box with the number knocked OUT of it. Both the box and
    # the knock-out must be built from the same glyph metrics or the text lands
    # off the box and reads as a smudge, which is exactly what it did.
    label = f"X{n_effects}"
    sc, sp, pad = 4, 3, 7
    tw = len(label) * (3 * sc + sp) - sp
    bx1, by0, by1 = gw - 8, 6, 6 + 5 * sc + 2 * pad
    bx0 = bx1 - (tw + 2 * pad)
    g[by0:by1, bx0:bx1] = True
    knock = np.zeros((gh, gw), dtype=bool)
    draw_text(knock, label, bx0 + pad, by0 + pad, scale=sc, spacing=sp)
    g[knock] = False

    img = Image.new("RGB", (gw, gh), CARD_BG)
    px = img.load()
    for y in range(gh):
        for x in range(gw):
            if g[y, x]:
                px[x, y] = LIT
    return img.resize((w, h), Image.NEAREST)


def cover_card(zdl: Path, w: int, h: int) -> Image.Image:
    px_grid, _ = decode_picture(str(zdl))
    ch = len(px_grid)
    cw = len(px_grid[0]) if ch else 0
    img = Image.new("RGB", (cw, ch), CARD_BG)
    load = img.load()
    for y in range(ch):
        row = px_grid[y]
        for x in range(cw):
            if row[x]:
                load[x, y] = LIT
    return img.resize((w, h), Image.NEAREST)


def compose(cards: list[Image.Image], title: Image.Image) -> Image.Image:
    canvas = Image.new("RGB", CANVAS, BG)
    gw = COLS * CARD[0] + (COLS - 1) * GAP
    gh = ROWS * CARD[1] + (ROWS - 1) * GAP
    ox = (CANVAS[0] - gw) // 2
    oy = (CANVAS[1] - gh) // 2

    def cell(i):
        r, c = divmod(i, COLS)
        return ox + c * (CARD[0] + GAP), oy + r * (CARD[1] + GAP)

    canvas.paste(title, cell(0))          # spans cells 0 and 1
    for i, card in enumerate(cards):
        canvas.paste(card, cell(i + 2))
    return canvas


# ---------------------------------------------------------------------------
# VHS
# ---------------------------------------------------------------------------

def rgb_to_yiq(a):
    m = np.array([[0.299, 0.587, 0.114],
                  [0.596, -0.274, -0.322],
                  [0.211, -0.523, 0.312]])
    return a @ m.T


def yiq_to_rgb(a):
    m = np.array([[1.0, 0.956, 0.621],
                  [1.0, -0.272, -0.647],
                  [1.0, -1.106, 1.703]])
    return a @ m.T


def hblur(chan, radius):
    """Horizontal-only box blur -- the axis tape actually loses bandwidth on."""
    if radius < 1:
        return chan
    k = 2 * radius + 1
    pad = np.pad(chan, ((0, 0), (radius, radius)), mode="edge")
    c = np.cumsum(pad, axis=1)
    c = np.pad(c, ((0, 0), (1, 0)))
    return (c[:, k:] - c[:, :-k]) / k


def vhs(img: Image.Image, strength: float = 1.0, seed: int = SEED) -> Image.Image:
    """strength < 1 for the small per-effect covers: at 220px wide the full
    treatment eats the knob labels entirely, and a cover nobody can read is a
    worse cover however good the texture looks."""
    rng = np.random.default_rng(seed)
    k = strength
    a = np.asarray(img).astype(np.float64) / 255.0
    h, w, _ = a.shape

    # 1. Chroma bandwidth. Luma stays sharp; I/Q get smeared hard. This one step
    #    does most of the work of reading as tape instead of as a CRT photo.
    yiq = rgb_to_yiq(a)
    yiq[:, :, 0] = hblur(yiq[:, :, 0], 1)
    yiq[:, :, 1] = hblur(yiq[:, :, 1], max(1, int(16 * k)))
    yiq[:, :, 2] = hblur(yiq[:, :, 2], max(1, int(16 * k)))
    a = np.clip(yiq_to_rgb(yiq), 0, 1)

    # 2. Per-line jitter, correlated vertically so it waves rather than fizzes.
    noise = rng.normal(0, 1, h)
    kern = np.exp(-np.linspace(-2, 2, 9) ** 2)
    kern /= kern.sum()
    wob = np.convolve(noise, kern, mode="same")
    shifts = np.round(wob * 1.1 * k).astype(int)
    for y in range(h):
        s = int(shifts[y])
        if s:
            a[y] = np.roll(a[y], s, axis=0)

    # 3. Chroma misregistration: R and B pulled opposite ways.
    a[:, :, 0] = np.roll(a[:, :, 0], max(1, int(2 * k)), axis=1)
    a[:, :, 2] = np.roll(a[:, :, 2], -max(1, int(2 * k)), axis=1)

    # 4. Dropouts -- oxide shed from the tape, bright dashes on a scanline.
    for _ in range(max(1, int(18 * k))):
        y = rng.integers(0, h)
        x = rng.integers(0, w - 60)
        ln = int(rng.integers(8, 55))
        a[y, x:x + ln] = np.clip(a[y, x:x + ln] + rng.uniform(0.35, 0.8), 0, 1)

    # 5. Head-switching tear: the bottom lines are written by a head that is
    #    leaving the tape, so they skew and break up. No CRT does this.
    tear = max(3, int(14 * k))
    for i, y in enumerate(range(h - tear, h)):
        s = int((tear - i) * rng.uniform(2.5, 5.0))
        a[y] = np.roll(a[y], s, axis=0)
        a[y] = np.clip(a[y] * 0.55 + rng.uniform(0, 0.5, (w, 1)) * 0.45, 0, 1)

    # 6. Bloom: bright phosphor bleeding into the dark around it.
    bright = np.clip(a - 0.45, 0, 1)
    bimg = Image.fromarray((bright * 255).astype(np.uint8))
    bimg = bimg.filter(ImageFilter.GaussianBlur(7))
    a = np.clip(a + np.asarray(bimg).astype(np.float64) / 255.0 * 0.55, 0, 1)

    # 7. Scanlines.
    a[0::2] *= 0.82

    # 8. Tape grain, heavier in the darks where the signal is weakest.
    lum = a.mean(axis=2, keepdims=True)
    a = np.clip(a + rng.normal(0, 0.028 * k, a.shape) * (1.25 - lum), 0, 1)

    # 9. Vignette.
    yy, xx = np.mgrid[0:h, 0:w]
    r = np.sqrt(((xx - w / 2) / (w / 2)) ** 2 + ((yy - h / 2) / (h / 2)) ** 2)
    a *= np.clip(1.06 - 0.30 * r ** 2.2, 0, 1)[:, :, None]

    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))


def main() -> int:
    dist = ROOT / "dist"
    zdls = sorted(dist.glob("*.ZDL"), key=lambda p: p.stem.lower())
    if not zdls:
        print("no .ZDL in dist/ -- build first", file=sys.stderr)
        return 1

    slots = COLS * ROWS - 2          # two cells go to the title card
    if len(zdls) > slots:
        print(f"{len(zdls)} effects but only {slots} cells -- widen the grid",
              file=sys.stderr)
        return 1

    cards = [cover_card(z, *CARD) for z in zdls]
    title = title_card(len(zdls), CARD[0] * 2 + GAP, CARD[1])
    out = vhs(compose(cards, title))

    dest = ROOT / "graphics" / "thumb_effects.png"
    dest.parent.mkdir(parents=True, exist_ok=True)
    out.save(dest)
    print(f"  {len(zdls)} effects -> {dest.relative_to(ROOT)}  ({CANVAS[0]}x{CANVAS[1]})")
    print("  " + ", ".join(z.stem for z in zdls))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
