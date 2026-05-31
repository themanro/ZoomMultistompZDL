#!/usr/bin/env python3
"""Convert a recorded impulse response into a compact C FIR coefficient table."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import resample_poly


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("ir", type=Path, help="mono or stereo impulse-response WAV")
    ap.add_argument("--taps", type=int, default=64)
    ap.add_argument("--target-fs", type=int, default=44100)
    ap.add_argument("--normalize-hz", type=float, default=1000.0)
    args = ap.parse_args()

    data, source_fs = sf.read(str(args.ir), always_2d=True)
    mono = data.mean(axis=1).astype(np.float64)
    peak = int(np.argmax(np.abs(mono)))
    causal = mono[peak:]

    if source_fs != args.target_fs:
        gcd = int(np.gcd(source_fs, args.target_fs))
        causal = resample_poly(causal, args.target_fs // gcd, source_fs // gcd)

    kernel = causal[: args.taps].copy()
    n_fft = 1 << 17
    freqs = np.fft.rfftfreq(n_fft, d=1.0 / args.target_fs)
    mag = np.abs(np.fft.rfft(kernel, n=n_fft))
    reference = float(np.interp(args.normalize_hz, freqs, mag))
    if reference <= 0.0:
        raise ValueError("normalization frequency has zero magnitude")
    kernel /= reference

    print(f"/* source={args.ir} peak={peak} source_fs={source_fs} "
          f"target_fs={args.target_fs} taps={args.taps} */")
    for i, value in enumerate(kernel):
        end = "\n" if (i + 1) % 4 == 0 else " "
        print(f"{value: .9e}f,", end=end)
    if len(kernel) % 4:
        print()


if __name__ == "__main__":
    main()
