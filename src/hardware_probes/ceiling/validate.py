#!/usr/bin/env python3
"""Calibrate Ceiling in the Ziddle emulator, rebuild, and check kCyc is linear.

1. Builds with the current calib.json (or a guess), runs the probe at a few
   kCyc values per mode and fits callback cycles = base + slope x kCyc.
2. Corrects each loop's cycles per iteration by slope / 1000, writes
   calib.json and rebuilds.
3. Checks the rebuilt probe: every mode should cost base + 1000 x kCyc
   emulated cycles (within 1%), the tone plays, the shuttle is kept.

    python3 -B src/hardware_probes/ceiling/validate.py
"""
from __future__ import annotations

import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import build  # noqa: E402

NAMCHECK = Path(os.environ.get("NAMCHECK", Path.home() / "ziddle/ziddle-source-1787802520/target/release/namcheck"))
POINTS = (0, 20, 60, 100)


def run(zdl: Path, td: Path, knobs: str) -> tuple[int, np.ndarray]:
    inp = td / "in.f32"
    if not inp.exists():
        np.zeros(44100, np.float32).tofile(inp)
    out = subprocess.run([str(NAMCHECK), str(zdl), str(inp), str(td / "out.f32"), knobs],
                         capture_output=True, text=True, check=True).stdout
    med = int(re.search(r"median=(\d+)", out).group(1))
    return med, np.fromfile(td / "out.f32", "<f4")


def fit(zdl: Path, td: Path, mode: int) -> tuple[float, float]:
    c = [run(zdl, td, f"{k},{mode},1")[0] for k in POINTS]
    slope, base = np.polyfit(POINTS, c, 1)
    return base, slope


def main() -> int:
    calib_path = HERE / "calib.json"
    with tempfile.TemporaryDirectory() as t:
        td = Path(t)
        calib = json.loads(calib_path.read_text()) if calib_path.exists() else {"alu": 4.0, "mem": 4.0}
        calib_path.write_text(json.dumps(calib) + "\n")
        zdl = build.main(td / "first")
        for key, mode in (("alu", 0), ("mem", 2)):
            base, slope = fit(zdl, td, mode)
            calib[key] = round(calib[key] * slope / 1000.0, 4)
            print(f"mode {mode}: base {base:.0f}, {slope:.1f} cycles per kCyc -> {key} iteration {calib[key]} cycles")
        calib_path.write_text(json.dumps(calib) + "\n")
        zdl = build.main()
        ok = True
        for mode in (0, 1, 2):
            base, slope = fit(zdl, td, mode)
            good = abs(slope - 1000.0) < 10.0
            ok &= good
            print(f"check mode {mode}: base {base:.0f}, {slope:.1f} cycles per kCyc ({'ok' if good else 'OFF'})")
        cyc, y = run(zdl, td, "0,0,1")
        rms = float(np.sqrt((y[4410:] ** 2).mean()))
        print(f"tone: rms {rms:.3f} (a -12 dB sine is ~0.18)")
        ok &= 0.15 < rms < 0.2
        cyc, y = run(zdl, td, "0,0,0")
        print(f"pass-through of silence: peak {np.abs(y).max():.2e}")
        ok &= float(np.abs(y).max()) < 1e-6
    print("OK" if ok else "FAILED")
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
