"""Custom 128x64 on-device cover images for the effect pack.

Each cover = a border, the effect name (big), and a small per-effect emblem
echoing the repo's SVG icons. make_cover(name) picks the emblem by name.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))

from screen_image import Canvas, encode_zoom_rle  # noqa: E402

CX, EY = 64, 43          # emblem centre


def _line(c, x0, y0, x1, y1):
    dx, dy = abs(x1 - x0), -abs(y1 - y0)
    sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
    err = dx + dy
    while True:
        c.px(x0, y0)
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 >= dy:
            err += dy; x0 += sx
        if e2 <= dx:
            err += dx; y0 += sy


def _spark(c, x, y, s):
    c.hline(x - s, x + s, y)
    c.vline(x, y - s, y + s)


def _em_shimmer(c):                       # Microloom
    _spark(c, 44, 54, 3); _spark(c, 64, 43, 4); _spark(c, 84, 32, 3)


def _em_flower(c):                        # Flower
    c.filled_circle(CX, EY, 2)
    for k in range(6):
        a = k * math.pi / 3
        c.circle(int(CX + 9 * math.cos(a)), int(EY + 9 * math.sin(a)), 4)


def _em_bars(c):                          # Shatter
    hs = [4, 12, 7, 15, 5, 11, 8]
    for i, h in enumerate(hs):
        x = 46 + i * 5
        c.vline(x, 52 - h, 52)


def _em_dunes(c):                         # Arrakis
    c.circle(80, 30, 4)
    for off, yc in ((0.0, 46), (1.0, 52)):
        prev = None
        for x in range(44, 85):
            t = (x - 44) / 40.0
            y = int(yc - 5 * math.sin(t * 2 * math.pi * 1.5 + off))
            if prev is not None:
                _line(c, x - 1, prev, x, y)
            prev = y


def _em_square(c):                        # Corrupt
    pts = [(44, 50), (44, 36), (60, 36), (60, 50), (76, 50), (76, 36), (84, 36)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])


def _em_rings(c):                         # Klang
    c.circle(57, EY, 8); c.circle(71, EY, 8)
    _line(c, 60, EY - 5, 68, EY + 5); _line(c, 68, EY - 5, 60, EY + 5)


def _reel(c, cx, cy, r):
    c.circle(cx, cy, r); c.filled_circle(cx, cy, 2)
    for ad in (90, 210, 330):
        a = math.radians(ad)
        for rr in range(3, r - 1):
            c.px(int(cx + rr * math.cos(a)), int(cy - rr * math.sin(a)))


def _em_reels(c):                         # GenLoss / Reel / Spool / Oxide
    _reel(c, 52, EY, 9); _reel(c, 76, EY, 9)
    c.hline(52, 76, EY - 11)


def _em_resonance(c):                     # Howl
    for r in (4, 8, 12):
        c.circle(CX, EY, r)


def _em_bolt(c):                          # Scorch
    pts = [(70, 28), (58, 44), (66, 44), (60, 58)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])
        _line(c, pts[i][0] + 1, pts[i][1], pts[i + 1][0] + 1, pts[i + 1][1])


def _em_swirl(c):                         # Lush (chorus)
    for off in (0.0, 1.6):
        prev = None
        for x in range(44, 85):
            t = (x - 44) / 40.0
            y = int(EY - 7 * math.sin(t * 2 * math.pi * 2.0 + off))
            if prev is not None:
                _line(c, x - 1, prev, x, y)
            prev = y


def _em_room(c):                          # Room (reverb)
    c.rect(50, 30, 78, 56)
    _line(c, 50, 30, 58, 24); _line(c, 78, 30, 86, 24)
    _line(c, 58, 24, 86, 24); _line(c, 86, 24, 86, 50); _line(c, 78, 30, 86, 24)


def _em_stars(c):                         # Galactic (reverb)
    c.circle(CX, EY, 7)
    for k in range(8):
        a = k * math.pi / 4
        c.filled_circle(int(CX + 15 * math.cos(a)), int(EY + 15 * math.sin(a)), 1)
    _spark(c, 44, 30, 3); _spark(c, 84, 54, 3)


def _em_arrows(c):                        # OTT (compressor)
    _line(c, 58, EY - 8, 64, EY - 15); _line(c, 64, EY - 15, 70, EY - 8)
    c.vline(64, EY - 15, EY - 3)
    _line(c, 58, EY + 8, 64, EY + 15); _line(c, 64, EY + 15, 70, EY + 8)
    c.vline(64, EY + 3, EY + 15)


def _em_reel_single(c):                   # Reel — one clean big reel
    _reel(c, CX, EY, 17)


def _em_oxide(c):                         # Oxide — reels shedding oxide specks
    _reel(c, 50, 38, 10); _reel(c, 78, 38, 10)
    c.hline(50, 78, 27)
    for x, y in [(46, 51), (58, 57), (64, 52), (72, 58), (82, 54), (54, 60), (76, 50)]:
        c.filled_circle(x, y, 1)


def _em_spool(c):                         # Spool — reel feeding shrinking echo loops
    _reel(c, 46, EY, 12)
    c.hline(46, 64, EY)
    for cx, r in [(74, 9), (92, 6), (106, 4)]:
        c.circle(cx, EY, r)


def _em_genloss(c):                       # GenLoss — reels + tangled/spilled tape
    _reel(c, 50, 36, 10); _reel(c, 78, 36, 10)
    c.hline(50, 78, 26)
    pts = [(42, 52), (52, 59), (60, 50), (68, 60), (76, 51), (86, 58)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])


def _em_tapehack(c):                      # TapeHack — exploding tape
    _reel(c, CX, EY, 11)
    for k in range(8):
        a = k * math.pi / 4
        _line(c, int(CX + 13 * math.cos(a)), int(EY - 13 * math.sin(a)),
              int(CX + 20 * math.cos(a)), int(EY - 20 * math.sin(a)))
    for x, y in [(40, 26), (88, 26), (40, 60), (88, 60)]:
        c.filled_circle(x, y, 1)


EMBLEMS = {
    "Microlm": _em_shimmer, "Flower": _em_flower, "Shatter": _em_bars,
    "Arrakis": _em_dunes, "Corrupt": _em_square, "Klang": _em_rings,
    "GenLoss": _em_genloss, "Howl": _em_resonance, "Scorch": _em_bolt,
    "Reel": _em_reel_single, "Spool": _em_spool, "Oxide": _em_oxide,
    "Lush": _em_swirl, "Room": _em_room,
    "Galactic": _em_stars, "OTT": _em_arrows, "TapeHack": _em_tapehack,
}


def make_cover(name: str) -> bytes:
    c = Canvas()
    c.rect(1, 1, 126, 62)
    adv = 3 * 2 + 1                       # scale-2 glyph advance
    w = len(name) * adv - 1
    x = max(4, (128 - w) // 2)
    c.draw_text(name.upper(), x, 6, scale=2, spacing=1)
    fn = EMBLEMS.get(name)
    if fn:
        fn(c)
    return encode_zoom_rle(c)
