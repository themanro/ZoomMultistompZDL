#!/usr/bin/env python3
"""Record the README's editor preview, graphics/editor.webp: the MultiStomp
Editor with an example bank and patch, its effect pictures moving, as a
looping animated WebP.

Serve the repo first (python3 -m http.server 8000 from the repo root), then:

    python3 build/make_editor_preview.py
    python3 build/make_editor_preview.py --video   # video/editor.mp4 (not committed)

The video is 15 seconds at 30 fps, recorded at 1.6x pixel density (1920x2000)
and encoded as H.264 by build/encode_mp4.swift (macOS).
Needs Playwright for Node (see build/fx_capture.cjs) and Pillow. The bank and
the patch are in build/editor_capture.cjs.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
URL = os.environ.get("EDITOR_URL", "http://localhost:8000/tools/editor.html")
FPS, SECONDS, W, H = 12, 4, 1200, 1250
VIDEO_FPS, VIDEO_SECONDS, VIDEO_SCALE, VIDEO_MBIT = 30, 15, 1.6, 25


def video() -> int:
    sys.path.insert(0, str(ROOT / "build"))
    from make_fx_sheet import encoder
    n = VIDEO_FPS * VIDEO_SECONDS
    dest = ROOT / "video" / "editor.mp4"
    dest.parent.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        subprocess.run(["node", str(ROOT / "build" / "editor_capture.cjs"), URL, td, str(n), str(VIDEO_FPS),
                        str(W), str(H), str(VIDEO_SCALE)], check=True, env=os.environ)
        paths = sorted(Path(td).glob("*.png"))
        w, h = Image.open(paths[0]).size
        w, h = w - w % 2, h - h % 2                   # H.264 wants even sizes
        enc = subprocess.Popen([str(encoder(td)), str(dest), str(w), str(h), str(VIDEO_FPS), str(VIDEO_MBIT)],
                               stdin=subprocess.PIPE)
        for p in paths:
            enc.stdin.write(Image.open(p).convert("RGB").crop((0, 0, w, h)).tobytes("raw", "BGRX"))
        enc.stdin.close()
        if enc.wait():
            return 1
    print(f"  {dest.relative_to(ROOT)}: {w}x{h}, {VIDEO_SECONDS} s at {VIDEO_FPS} fps, "
          f"{dest.stat().st_size // (1 << 20)} MB")
    return 0


def main() -> int:
    if "--video" in sys.argv[1:]:
        return video()
    with tempfile.TemporaryDirectory() as td:
        subprocess.run(["node", str(ROOT / "build" / "editor_capture.cjs"), URL, td, str(FPS * SECONDS), str(FPS),
                        str(W), str(H)], check=True, env=os.environ)
        frames = [Image.open(p).convert("RGB") for p in sorted(Path(td).glob("*.png"))]
    dest = ROOT / "graphics" / "editor.webp"
    frames[0].save(dest, save_all=True, append_images=frames[1:], duration=round(1000 / FPS), loop=0,
                   quality=72, method=6)
    print(f"  {len(frames)} frames -> {dest.relative_to(ROOT)}  ({dest.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
