"""Custom 128x64 on-device cover images for the effect pack.

Stock MS-70CDR convention (verified by decoding ~130 stock ZDLs with
build/decode_picture.py): the cover is the FULL 128x64 frame, and the firmware
paints live knob-value boxes (20x15 px, up to 3) ON TOP at the (x,y) in
effectTypeImageInfo. So a good cover is laid out in two bands:

    rows  1..12   effect name (big)
    rows 14..34   effect emblem
    rows 36..62   a knob row: each visible param's label + a dial, positioned
                  to sit exactly under the firmware's value box.

`knob_layout(n)` is the single source of truth for the box/dial x,y — the
linker's default knob_positions and this drawing code both use it, so the
drawn dials line up with the firmware boxes.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))

from screen_image import Canvas, encode_zoom_rle  # noqa: E402

CX, CY = 64, 23          # emblem centre (top band)
KNOB_Y = 46              # firmware value boxes sit at y=46 (rows 46..61)


def knob_layout(n: int):
    """Box positions (knob_id, x, y) for n visible knobs (n = min(3, params)).

    MUST stay in sync with the linker's defaults_by_count so the drawn dials
    line up under the firmware-painted value boxes.
    """
    Y = KNOB_Y
    return {
        0: [],
        1: [(2, 54, Y)],
        2: [(2, 26, Y), (3, 82, Y)],
    }.get(n, [(2, 14, Y), (3, 55, Y), (4, 96, Y)])


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


def _reel(c, cx, cy, r):
    c.circle(cx, cy, r)
    c.filled_circle(cx, cy, 2)
    for ad in (90, 210, 330):
        a = math.radians(ad)
        for rr in range(3, r - 1):
            c.px(int(cx + rr * math.cos(a)), int(cy - rr * math.sin(a)))


# ---- emblems (compact, centred on the top band rows ~14..34) --------------

def _em_shimmer(c):                       # Microloom
    _spark(c, 52, 30, 2); _spark(c, 64, 23, 3); _spark(c, 76, 17, 2)


def _em_flower(c):                        # Flower
    c.filled_circle(CX, CY, 2)
    for k in range(6):
        a = k * math.pi / 3
        c.circle(int(CX + 8 * math.cos(a)), int(CY + 8 * math.sin(a)), 3)


def _em_bars(c):                          # Shatter
    hs = [5, 11, 7, 13, 6, 10, 8]
    for i, h in enumerate(hs):
        c.vline(46 + i * 5, 33 - h, 33)


def _em_dunes(c):                         # Arrakis
    c.circle(82, 15, 3)
    for off, yc in ((0.0, 27), (1.0, 32)):
        prev = None
        for x in range(44, 85):
            t = (x - 44) / 40.0
            y = int(yc - 4 * math.sin(t * 2 * math.pi * 1.5 + off))
            if prev is not None:
                _line(c, x - 1, prev, x, y)
            prev = y


def _em_square(c):                        # Corrupt
    pts = [(46, 32), (46, 16), (60, 16), (60, 32), (74, 32), (74, 16), (84, 16)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])


def _em_rings(c):                         # Klang
    c.circle(57, CY, 7); c.circle(71, CY, 7)
    _line(c, 60, CY - 5, 68, CY + 5); _line(c, 68, CY - 5, 60, CY + 5)


def _em_resonance(c):                     # Howl
    for r in (3, 6, 9):
        c.circle(CX, CY, r)


def _em_bolt(c):                          # Scorch
    pts = [(70, 13), (58, 25), (66, 25), (56, 35)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])
        _line(c, pts[i][0] + 1, pts[i][1], pts[i + 1][0] + 1, pts[i + 1][1])


def _em_swirl(c):                         # Lush (chorus)
    for off in (0.0, 1.6):
        prev = None
        for x in range(44, 85):
            t = (x - 44) / 40.0
            y = int(CY - 6 * math.sin(t * 2 * math.pi * 2.0 + off))
            if prev is not None:
                _line(c, x - 1, prev, x, y)
            prev = y


def _em_room(c):                          # Room (reverb)
    c.rect(52, 18, 76, 34)
    _line(c, 52, 18, 60, 13); _line(c, 60, 13, 84, 13)
    _line(c, 84, 13, 84, 29); _line(c, 76, 18, 84, 13)


def _em_stars(c):                         # Galactic (reverb)
    c.circle(CX, CY, 5)
    for k in range(8):
        a = k * math.pi / 4
        c.filled_circle(int(CX + 11 * math.cos(a)), int(CY + 11 * math.sin(a)), 1)
    _spark(c, 46, 15, 2); _spark(c, 84, 33, 2)


def _em_arrows(c):                        # OTT (compressor)
    _line(c, 58, CY - 6, 64, CY - 12); _line(c, 64, CY - 12, 70, CY - 6)
    c.vline(64, CY - 12, CY - 2)
    _line(c, 58, CY + 6, 64, CY + 12); _line(c, 64, CY + 12, 70, CY + 6)
    c.vline(64, CY + 2, CY + 12)


def _em_reel_single(c):                   # Reel — one clean reel
    _reel(c, CX, CY, 10)


def _em_oxide(c):                         # Oxide — reels shedding oxide specks
    _reel(c, 54, 21, 6); _reel(c, 74, 21, 6)
    c.hline(54, 74, 14)
    for x, y in [(48, 30), (58, 33), (64, 30), (72, 33), (80, 31)]:
        c.filled_circle(x, y, 1)


def _em_spool(c):                         # Spool — reel feeding shrinking echo loops
    _reel(c, 50, CY, 8)
    c.hline(50, 66, CY)
    for cx, r in [(74, 6), (88, 4), (99, 3)]:
        c.circle(cx, CY, r)


def _em_genloss(c):                       # GenLoss — reels + tangled tape
    _reel(c, 54, 20, 6); _reel(c, 74, 20, 6)
    c.hline(54, 74, 13)
    pts = [(44, 31), (52, 35), (60, 29), (68, 35), (76, 30), (84, 34)]
    for i in range(len(pts) - 1):
        _line(c, pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1])


def _em_tapehack(c):                      # TapeHack — exploding tape
    _reel(c, CX, CY, 7)
    for k in range(8):
        a = k * math.pi / 4
        _line(c, int(CX + 9 * math.cos(a)), int(CY - 9 * math.sin(a)),
              int(CX + 13 * math.cos(a)), int(CY - 13 * math.sin(a)))


EMBLEMS = {
    "Microlm": _em_shimmer, "Flower": _em_flower, "Shatter": _em_bars,
    "Arrakis": _em_dunes, "Corrupt": _em_square, "Klang": _em_rings,
    "GenLoss": _em_genloss, "Howl": _em_resonance, "Scorch": _em_bolt,
    "Reel": _em_reel_single, "Spool": _em_spool, "Oxide": _em_oxide,
    "Lush": _em_swirl, "Room": _em_room,
    "Galactic": _em_stars, "OTT": _em_arrows, "TapeHack": _em_tapehack,
}


def _draw_knob_row(c, param_names):
    """A label + dial under each firmware value box, aligned to knob_layout."""
    n = min(3, len(param_names))
    for (kid, kx, ky), label in zip(knob_layout(n), param_names[:n]):
        cx = kx + 10                      # box is 20 wide -> centre
        adv = 3 + 1                       # scale-1 glyph advance
        w = len(label) * adv - 1
        lx = max(2, cx - w // 2)
        c.draw_text(label.upper(), lx, 37, scale=1, spacing=1)
        cyd = ky + 7                      # box is 15 tall -> centre
        c.circle(cx, cyd, 6)
        c.vline(cx, cyd - 5, cyd)         # pointer


def make_cover(name: str, param_names=None) -> bytes:
    c = Canvas()
    c.rect(1, 1, 126, 62)
    adv = 3 * 2 + 1                       # scale-2 glyph advance
    w = len(name) * adv - 1
    x = max(3, (128 - w) // 2)
    c.draw_text(name.upper(), x, 3, scale=2, spacing=1)
    fn = EMBLEMS.get(name)
    if fn:
        fn(c)
    if param_names:
        _draw_knob_row(c, param_names)
    return encode_zoom_rle(c)
