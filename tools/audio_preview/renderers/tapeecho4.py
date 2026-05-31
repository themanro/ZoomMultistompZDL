"""Desktop reference renderer for the TapeEcho4 pedal DSP."""

from __future__ import annotations

from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.linalg import hadamard
from scipy.signal import fftconvolve, resample_poly

PEDAL_FS = 44100
TWO_PI = 6.2831853
PI = 3.1415927
HALF_PI = 1.5707963
CLIP_LIMIT = 2.305929
SPRING_HANDOFF_SECONDS = 0.744
SPRING_CROSSFADE_SECONDS = 0.320
SPRING_FEEDBACK = 0.92
SPRING_DAMPING = 0.55
SPRING_DELAYS = np.array([1499, 1877, 2203, 2663, 3163, 3571, 4001, 4481])
SPRING_INPUT = np.array([1, -1, 1, 1, -1, 1, -1, -1]) / np.sqrt(8.0)
SPRING_OUTPUT = np.array([1, 1, -1, 1, -1, -1, 1, -1]) / np.sqrt(8.0)
SPRING_MATRIX = hadamard(8).astype(np.float64) / np.sqrt(8.0)


def _resample(audio: np.ndarray, source_fs: int, target_fs: int) -> np.ndarray:
    if source_fs == target_fs:
        return audio
    gcd = int(np.gcd(source_fs, target_fs))
    return resample_poly(audio, target_fs // gcd, source_fs // gcd, axis=0)


def _load_galaxy_kernel(root: Path) -> np.ndarray:
    path = root / "tools" / "measure" / "Tape" / "Tape_IR.wav"
    ir, sample_rate = sf.read(str(path), always_2d=True, dtype="float64")
    causal = ir.mean(axis=1)
    causal = causal[int(np.argmax(np.abs(causal))):]
    causal = _resample(causal, sample_rate, PEDAL_FS)
    kernel = causal[:64].copy()
    n_fft = 1 << 17
    freqs = np.fft.rfftfreq(n_fft, d=1.0 / PEDAL_FS)
    magnitude = np.abs(np.fft.rfft(kernel, n=n_fft))
    kernel /= float(np.interp(1000.0, freqs, magnitude))
    return kernel


def _load_spring_kernel(root: Path, frames: int) -> np.ndarray:
    path = root / "tools" / "measure" / "Tape" / "Tape Spring.wav"
    ir, sample_rate = sf.read(str(path), always_2d=True, dtype="float64")
    measured = ir.mean(axis=1)
    measured = measured[int(np.argmax(np.abs(measured))):]
    measured = _resample(measured, sample_rate, PEDAL_FS)
    measured /= float(np.max(np.abs(measured)))

    handoff = int(SPRING_HANDOFF_SECONDS * PEDAL_FS)
    synthetic = np.zeros(frames)
    buffers = [np.zeros(delay) for delay in SPRING_DELAYS]
    positions = np.zeros(len(SPRING_DELAYS), dtype=np.int64)
    damped = np.zeros(len(SPRING_DELAYS))

    for frame in range(frames):
        taps = np.array([
            buffer[position]
            for buffer, position in zip(buffers, positions)
        ])
        synthetic[frame] = float(SPRING_OUTPUT @ taps)
        mixed = SPRING_MATRIX @ taps
        damped += SPRING_DAMPING * (mixed - damped)
        excitation = measured[frame] if frame < min(handoff, len(measured)) else 0.0
        writes = SPRING_FEEDBACK * damped + SPRING_INPUT * excitation
        for line, buffer in enumerate(buffers):
            buffer[positions[line]] = writes[line]
            positions[line] = (positions[line] + 1) % len(buffer)

    match_start = max(0, handoff - int(0.160 * PEDAL_FS))
    reference_rms = np.sqrt(np.mean(measured[match_start:handoff] ** 2))
    synthetic_rms = np.sqrt(np.mean(synthetic[match_start:handoff] ** 2))
    synthetic *= reference_rms / max(synthetic_rms, 1.0e-30)

    kernel = np.zeros(frames)
    copied = min(frames, len(measured))
    kernel[:copied] = measured[:copied]
    fade = int(SPRING_CROSSFADE_SECONDS * PEDAL_FS)
    fade_end = min(frames, handoff + fade)
    phase = np.linspace(0.0, 1.0, fade_end - handoff, endpoint=False)
    kernel[handoff:fade_end] = (
        np.cos(phase * HALF_PI) * kernel[handoff:fade_end] +
        np.sin(phase * HALF_PI) * synthetic[handoff:fade_end]
    )
    kernel[fade_end:] = synthetic[fade_end:]
    return kernel


def _sin_approx(x: float) -> float:
    x -= TWO_PI * int(x * 0.15915494)
    if x < 0.0:
        x += TWO_PI
    sign = 1.0
    if x > PI:
        x -= PI
        sign = -1.0
    if x > HALF_PI:
        x = PI - x
    x2 = x * x
    y = x - x * x2 * 0.16666667
    x *= x2
    y += x * x2 * 0.0083333310
    x *= x2
    y -= x * x2 * 0.0001984090
    return y * sign


def _saturate(x: float) -> float:
    x = float(np.clip(x, -CLIP_LIMIT, CLIP_LIMIT))
    xx = x * x
    p = x * xx
    y = x - p * 0.16666667
    p *= xx
    y += p * 0.014492754
    p *= xx
    y -= p * 0.000395244
    p *= xx
    y += p * 0.00000444473
    p *= xx
    return y - p * 0.000000100208


def _division(value: float) -> float:
    if value < 16.666667:
        return 0.25
    if value < 33.333334:
        return 0.33333334
    if value < 50.0:
        return 0.5
    if value < 66.66667:
        return 0.75
    if value < 83.33333:
        return 1.0
    return 1.5


def render(audio: np.ndarray, sample_rate: int, params: dict[str, float],
           tail_seconds: float, root: Path) -> np.ndarray:
    source_frames = len(audio)
    source_fs = sample_rate
    audio = _resample(audio, source_fs, PEDAL_FS)
    if audio.shape[1] == 1:
        audio = np.repeat(audio, 2, axis=1)
    elif audio.shape[1] > 2:
        audio = audio[:, :2]

    kernel = _load_galaxy_kernel(root)
    n_frames = len(audio) + int(PEDAL_FS * tail_seconds)
    out = np.zeros((n_frames, 2), dtype=np.float64)
    delay = np.zeros((2, 65536), dtype=np.float64)
    fir = np.zeros((2, 64), dtype=np.float64)
    hp = np.zeros(2)
    lp = np.zeros(2)
    write_index = 0
    fir_index = 0
    flutter_l, flutter_r = 0.0, 2.0943951
    wow_l, wow_r = 0.0, 3.1415927

    tempo = float(np.clip(params["tempo"], 40.0, 200.0))
    base_delay = float(np.clip((2646000.0 / tempo) * _division(params["div"]),
                               48.0, 65500.0))
    feedback = (params["feed"] * 0.01) ** 2 * 0.92
    flutter = params["flutter"] * 0.01
    wow = params["wow"] * 0.01
    wear = params["wear"] * 0.01
    drive = params["drive"] * 0.01
    spring = params["spring"] * 0.01
    mix = params["mix"] * 0.01
    hp_coef = 0.0005 + wear * wear * 0.0065
    lp_coef = 1.0 - wear * wear * 0.38
    record_gain = 1.0 + drive * 2.4

    for i in range(n_frames):
        dry = audio[i] if i < len(audio) else np.zeros(2)
        flutter_l = (flutter_l + 0.001096) % TWO_PI
        flutter_r = (flutter_r + 0.000860) % TWO_PI
        wow_l = (wow_l + 0.000213) % TWO_PI
        wow_r = (wow_r + 0.000546) % TWO_PI
        travel = float(np.clip((base_delay - 3043.0) * 0.00005425, 0.0, 1.0))
        flutter_depth = flutter * flutter * (
            0.35 + np.clip(base_delay, 0.0, 21477.0) * 0.000007
        )
        wow_depth = wow * wow * base_delay * 0.00118
        modulation = (_sin_approx(flutter_l) * 0.7 +
                      _sin_approx(flutter_r) * 0.3) * flutter_depth
        modulation += (
            _sin_approx(wow_l) * (0.35 + travel * 0.65) +
            _sin_approx(wow_r) * (0.75 - travel * 0.55)
        ) * wow_depth

        delay_samples = float(np.clip(base_delay + modulation, 48.0, 65500.0))
        whole = int(delay_samples)
        frac = delay_samples - whole
        wet = (delay[:, (write_index - whole) & 65535] * (1.0 - frac) +
               delay[:, (write_index - whole - 1) & 65535] * frac)
        record = dry + wet * feedback
        record = np.array([_saturate(x * record_gain) / record_gain for x in record])
        fir[:, fir_index] = record
        indices = (fir_index - np.arange(64)) & 63
        filtered = np.array([np.dot(fir[ch, indices], kernel) for ch in range(2)])
        fir_index = (fir_index + 1) & 63
        hp += (filtered - hp) * hp_coef
        filtered -= hp
        lp += (filtered - lp) * lp_coef
        delay[:, write_index] = lp
        write_index = (write_index + 1) & 65535

        out[i] = dry * (1.0 - mix) + wet * mix

    if spring > 0.0:
        # Desktop listening target. The pedal build still uses its compact
        # fallback until a bounded long-convolution strategy is validated.
        spring_kernel = _load_spring_kernel(root, n_frames)
        mono = audio.mean(axis=1)
        spring_return = fftconvolve(mono, spring_kernel)[:n_frames]
        out[:, 0] += spring_return * spring * mix
        out[:, 1] += spring_return * spring * mix

    peak = float(np.max(np.abs(out)))
    if peak > 0.99:
        out *= 0.99 / peak
    rendered = _resample(out, PEDAL_FS, source_fs)
    expected_frames = source_frames + int(source_fs * tail_seconds)
    return rendered[:expected_frames]
