"""Reusable 128x64 Airwindows-style Zoom screen images."""

from __future__ import annotations

import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "build"))

from screen_image import Canvas, encode_zoom_rle  # noqa: E402


def _draw_reel(c: Canvas, cx: int, cy: int, r: int) -> None:
    c.circle(cx, cy, r)
    c.circle(cx, cy, 5)
    c.filled_circle(cx, cy, 2)
    for angle_deg in (90, 210, 330):
        a = math.radians(angle_deg)
        for rr in range(3, r - 1):
            c.px(int(cx + rr * math.cos(a) + 0.5), int(cy - rr * math.sin(a) + 0.5))


def _draw_wave(c: Canvas, x0: int, x1: int, cy: int, amp: float, periods: float, phase: float) -> None:
    prev_y = None
    for x in range(x0, x1 + 1):
        t = (x - x0) / float(x1 - x0)
        y = int(cy - (math.sin((t * periods * 2.0 * math.pi) + phase) * amp))
        if prev_y is None:
            c.px(x, y)
        else:
            y0, y1 = (prev_y, y) if prev_y <= y else (y, prev_y)
            c.vline(x, y0, y1)
        prev_y = y


def make_airwindows_tape_screen(name: str, number: str = "9") -> bytes:
    """Tape-machine themed Airwindows bitmap, adapted from the old ToTape9 port."""
    c = Canvas()

    c.rect(1, 4, 60, 59)
    _draw_reel(c, 15, 32, 14)
    _draw_reel(c, 47, 32, 14)
    c.hline(29, 29, 22)
    c.hline(33, 33, 22)
    c.rect(29, 19, 33, 27)
    c.hline(30, 32, 23)
    c.vline(63, 2, 61)

    c.draw_text(name[:2].upper(), 66, 4, scale=2, spacing=1)
    c.draw_text(name[2:6].upper(), 66, 18, scale=2, spacing=2)
    if number:
        c.draw_char(number[0], 108, 3, scale=3)

    base_y = 52
    prev_y = None
    for x in range(66, 127):
        t = (x - 66) / 60.0
        raw = math.sin(t * 2.0 * math.pi * 1.5) * 6.0
        clipped = math.copysign(min(abs(raw * 0.9), 5.0), raw)
        y = int(base_y - clipped)
        if prev_y is None:
            c.px(x, y)
        else:
            y0, y1 = (prev_y, y) if prev_y <= y else (y, prev_y)
            c.vline(x, y0, y1)
        prev_y = y

    c.draw_text("AIRWINDOWS", 66, 55, scale=1, spacing=1)
    return encode_zoom_rle(c)


def make_airwindows_chorus_screen() -> bytes:
    """Stereo chorus bitmap with clear Speed/Depth knob wells."""
    c = Canvas()

    c.rect(0, 0, 127, 63)
    c.draw_text("ST", 6, 5, scale=2, spacing=1)
    c.draw_text("CHORUS", 27, 5, scale=2, spacing=1)
    c.draw_text("AIRWINDOWS", 84, 7, scale=1, spacing=1)

    c.hline(5, 122, 21)
    _draw_wave(c, 8, 119, 29, 5.0, 2.0, 0.0)
    _draw_wave(c, 8, 119, 29, 5.0, 2.0, 1.57079632679)
    c.draw_text("L", 5, 24, scale=1, spacing=1)
    c.draw_text("R", 5, 31, scale=1, spacing=1)

    c.draw_text("SPEED", 24, 35, scale=1, spacing=1)
    c.draw_text("DEPTH", 81, 35, scale=1, spacing=1)

    c.rect(20, 42, 53, 60)
    c.rect(76, 42, 109, 60)
    c.hline(23, 50, 39)
    c.hline(79, 106, 39)
    c.vline(64, 39, 60)

    return encode_zoom_rle(c)


def make_airwindows_totape_screen() -> bytes:
    """ToTape9 bitmap using the shared Airwindows title/knob layout."""
    c = Canvas()

    c.rect(0, 0, 127, 63)
    c.draw_text("TO", 6, 5, scale=2, spacing=1)
    c.draw_text("TAPE9", 27, 5, scale=2, spacing=1)
    c.draw_text("AIRWINDOWS", 84, 7, scale=1, spacing=1)

    c.hline(5, 122, 21)
    _draw_reel(c, 25, 29, 7)
    _draw_reel(c, 53, 29, 7)
    c.rect(37, 25, 41, 33)
    c.hline(34, 44, 35)
    c.hline(34, 44, 37)
    _draw_wave(c, 78, 119, 30, 4.0, 1.25, 0.0)

    c.draw_text("INPUT", 11, 38, scale=1, spacing=1)
    c.draw_text("TILT", 56, 38, scale=1, spacing=1)
    c.draw_text("SHAPE", 92, 38, scale=1, spacing=1)

    c.rect(7, 45, 39, 61)
    c.rect(48, 45, 80, 61)
    c.rect(89, 45, 121, 61)
    c.hline(10, 36, 42)
    c.hline(51, 77, 42)
    c.hline(92, 118, 42)

    return encode_zoom_rle(c)


def make_airwindows_tape_echo_screen() -> bytes:
    """TapeEcho4 bitmap: tape transport plus delay repeats and paged knobs."""
    c = Canvas()

    c.rect(0, 0, 127, 63)
    c.draw_text("TAPE", 6, 5, scale=2, spacing=1)
    c.draw_text("ECHO4", 52, 5, scale=2, spacing=1)
    c.draw_text("AIRWINDOWS", 84, 7, scale=1, spacing=1)

    c.hline(5, 122, 21)
    _draw_reel(c, 17, 30, 6)
    _draw_reel(c, 41, 30, 6)
    c.rect(27, 26, 31, 34)
    c.hline(24, 34, 36)
    c.hline(24, 34, 38)

    for x0, amp in ((61, 5.0), (76, 3.0), (91, 2.0), (106, 1.0)):
        _draw_wave(c, x0, min(122, x0 + 18), 31, amp, 0.7, 0.0)

    c.draw_text("TEMPO", 10, 38, scale=1, spacing=1)
    c.draw_text("DIV", 58, 38, scale=1, spacing=1)
    c.draw_text("FEED", 95, 38, scale=1, spacing=1)

    c.rect(7, 45, 39, 61)
    c.rect(48, 45, 80, 61)
    c.rect(89, 45, 121, 61)
    c.hline(10, 36, 42)
    c.hline(51, 77, 42)
    c.hline(92, 118, 42)

    return encode_zoom_rle(c)


def make_airwindows_vinyl_screen() -> bytes:
    """ToVinyl4 bitmap: spinning record on the left, three paged knob wells."""
    c = Canvas()

    c.rect(0, 0, 127, 63)
    c.draw_text("TO", 6, 5, scale=2, spacing=1)
    c.draw_text("VINYL", 27, 5, scale=2, spacing=1)
    c.draw_text("AIRWINDOWS", 84, 7, scale=1, spacing=1)

    c.hline(5, 122, 21)

    cx, cy, r = 22, 32, 8
    c.circle(cx, cy, r)
    c.circle(cx, cy, r - 2)
    c.circle(cx, cy, r - 4)
    c.filled_circle(cx, cy, 1)

    _draw_wave(c, 40, 119, 32, 4.0, 2.5, 0.0)
    _draw_wave(c, 40, 119, 32, 2.0, 5.0, 1.57079632679)

    c.draw_text("BRIGHT", 8, 38, scale=1, spacing=1)
    c.draw_text("SIDE", 56, 38, scale=1, spacing=1)
    c.draw_text("HISS", 96, 38, scale=1, spacing=1)

    c.rect(7, 45, 39, 61)
    c.rect(48, 45, 80, 61)
    c.rect(89, 45, 121, 61)
    c.hline(10, 36, 42)
    c.hline(51, 77, 42)
    c.hline(92, 118, 42)

    return encode_zoom_rle(c)


def make_airwindows_reverb_screen(name: str = "VerbTiny") -> bytes:
    """Shared Airwindows reverb bitmap with three visible paged knob wells."""
    c = Canvas()

    c.rect(0, 0, 127, 63)
    title = name.upper()
    if title == "VERBTINY":
        left, right = "VERB", "TINY"
    elif title == "GALACTIC":
        left, right = "GAL", "ACTIC"
    else:
        left, right = title[:4], title[4:9]
    c.draw_text(left, 6, 5, scale=2, spacing=1)
    if right:
        c.draw_text(right, 53, 5, scale=2, spacing=1)
    c.draw_text("AIRWINDOWS", 84, 7, scale=1, spacing=1)

    c.hline(5, 122, 21)
    for x in range(9, 119, 11):
        c.vline(x, 25, 35)
        c.px(x + 1, 24)
        c.px(x + 2, 23)
        c.px(x + 3, 24)
        c.px(x + 4, 25)
    _draw_wave(c, 8, 119, 31, 4.0, 3.25, 0.0)
    _draw_wave(c, 8, 119, 32, 2.0, 5.0, 1.57079632679)

    c.draw_text(name[:8].upper(), 40, 24, scale=1, spacing=1)
    c.draw_text("REPL", 11, 38, scale=1, spacing=1)
    c.draw_text("DEREZ", 52, 38, scale=1, spacing=1)
    c.draw_text("FILT", 96, 38, scale=1, spacing=1)

    c.rect(7, 45, 39, 61)
    c.rect(48, 45, 80, 61)
    c.rect(89, 45, 121, 61)
    c.hline(10, 36, 42)
    c.hline(51, 77, 42)
    c.hline(92, 118, 42)

    return encode_zoom_rle(c)
