"""Desktop reference renderer for Howl — Death By Audio Total Sonic
Annihilation-style self-oscillating feedback.

A high-resonance bandpass (Chamberlin SVF) sits in a feedback loop with a fuzz
clipper. The input plus the fed-back, clipped output drives the filter; once
the loop gain (Annihil) crosses unity the resonance runs away into sustained
self-oscillation at the Tune frequency, bounded by the clipper so it screams
instead of exploding. Two slightly detuned resonators (L/R) give a beating
stereo howl. Low Annihil = a resonant filter; high = endless feedback drone;
max = chaos.

Knobs (0..100): Tune, Annihil, Drive, Tone, Mix.
"""

from __future__ import annotations

import numpy as np

PI = 3.14159265358979
TWO_PI = 6.28318530717959
Q = 0.45  # SVF damping — high enough that oscillation depends on Annihil


def _channel(mono, sr, fc, fbgain, drv, lpc):
    n = mono.shape[0]
    out = np.zeros(n)
    f1 = 2.0 * np.sin(PI * fc / sr)
    low = band = fb = lp = 0.0
    for i in range(n):
        x = mono[i] + fbgain * fb
        low += f1 * band
        hp = x - low - Q * band
        band += f1 * hp
        y = np.tanh(drv * band)      # fuzz clipper in the loop
        fb = y
        lp += lpc * (y - lp)         # output tone low-pass
        out[i] = lp
    return out


def render(audio, sample_rate, params, tail, root):
    sr = sample_rate
    tune = float(params.get("tune", 45.0)) / 100.0
    annih = float(params.get("annihil", 60.0)) / 100.0
    drive = float(params.get("drive", 55.0)) / 100.0
    tone = float(params.get("tone", 45.0)) / 100.0
    mix = float(params.get("mix", 70.0)) / 100.0

    fc = 80.0 * (2.0 ** (tune * 4.6))      # ~80 .. ~1950 Hz
    fbgain = annih * annih * 2.2           # quadratic: gentle low, runaway high
    drv = 1.0 + drive * 9.0
    lpc = 0.02 + tone * 0.5

    x = audio.astype(np.float64)
    mono = x.mean(axis=1)
    tail_samps = int(tail * sr)
    mono = np.concatenate([mono, np.zeros(tail_samps)])

    wetL = _channel(mono, sr, fc, fbgain, drv, lpc)
    wetR = _channel(mono, sr, fc * 1.012, fbgain, drv, lpc)
    # makeup + final safety clip
    wetL = np.tanh(wetL * 1.1)
    wetR = np.tanh(wetR * 1.1)

    dry = np.concatenate([x, np.zeros((tail_samps, x.shape[1]))], axis=0)
    n = wetL.shape[0]
    out = np.empty((n, 2))
    out[:, 0] = dry[:, 0] * (1.0 - mix) + wetL * mix
    out[:, 1] = dry[:, min(1, dry.shape[1] - 1)] * (1.0 - mix) + wetR * mix
    return out
