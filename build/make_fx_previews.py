#!/usr/bin/env python3
"""Render each effect's editor picture as a short looping image for the docs.

The editor draws every effect as a live, knob-driven scene (tools/fx_visuals.js).
GitHub pages cannot run it, so this records each scene at its default knob
settings (build/fx_capture.cjs, headless Chromium) and writes an animated WebP
per effect to graphics/fx/<name>.webp.

    python3 build/make_fx_previews.py            # every effect on the effects page
    python3 build/make_fx_previews.py Spiral     # just one

Needs Playwright for Node (see fx_capture.cjs) and Pillow.
"""
from __future__ import annotations

import os
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build"))
import readme_art as A  # noqa: E402

FPS, SECONDS, WIDTH = 12, 3, 320


def main() -> int:
    every = A.PACK + A.EXPERIMENTAL + A.MATUJUICE
    names = [n for n in every if not sys.argv[1:] or n.lower() in {a.lower() for a in sys.argv[1:]}]
    db_name = {"SGNL": "S.GN_L", "Microlm": "Microlm"}
    out_dir = ROOT / "graphics" / "fx"
    out_dir.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as td:
        subprocess.run(["node", str(ROOT / "build" / "fx_capture.cjs"), td, str(FPS * SECONDS), str(FPS), str(WIDTH),
                        *[db_name.get(n, n) for n in names]], check=True, env=os.environ)
        for n in names:
            src = Path(td) / db_name.get(n, n)
            frames = [Image.open(p).convert("RGB") for p in sorted(src.glob("*.png"))]
            if not frames:
                continue
            dest = out_dir / f"{n.lower()}.webp"
            frames[0].save(dest, save_all=True, append_images=frames[1:], duration=round(1000 / FPS), loop=0,
                           quality=70, method=6)
            print(f"  {n:<10} -> {dest.relative_to(ROOT)}  ({dest.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
