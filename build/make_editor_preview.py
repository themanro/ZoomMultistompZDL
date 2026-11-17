#!/usr/bin/env python3
"""Record the README's editor preview, graphics/editor.webp: the MultiStomp
Editor with an example bank and patch, its effect pictures moving, as a
looping animated WebP.

Serve the repo first (python3 -m http.server 8000 from the repo root), then:

    python3 build/make_editor_preview.py

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


def main() -> int:
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
