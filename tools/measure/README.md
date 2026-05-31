# Measurement Tools

Scripts for two characterization workflows: **frequency response** (via Farina
log sweeps) and **wow/flutter** (via steady tone + Hilbert pitch tracking).

The deps (`numpy`, `scipy`, `soundfile`, `matplotlib`) live in
`~/coding/airwindowsZoom/.venv` — call its interpreter directly:

```
PY=~/coding/airwindowsZoom/.venv/bin/python3
```

## Frequency response (sweep)

```
$PY generate_sweep.py
$PY analyze_sweep.py recorded.wav \
   --reference "passthrough without pedal.wav" \
   --label "MS-70CDR" --ref-label "Interface"
```

## Wow / flutter (steady tone, pitch tracking)

1. Generate the carrier:

   ```
   $PY generate_tone.py                  # writes ./out/tone.wav (1 kHz, 30 s)
   ```

2. Play `tone.wav` through the plugin under test with **feedback = 0**, **mix
   = 100 % wet**, tape sim active. Record the output.

3. Analyze. Run the script once per recording:

   ```
   $PY analyze_wow_flutter.py tone_uad_69ms.wav  --label "UAD 69 ms"
   $PY analyze_wow_flutter.py tone_uad_487ms.wav --label "UAD 487 ms"
   ```

   Each run prints `WOW peak / FLUTTER peak` rates in Hz and depths in cents.

### Delay-coupled measurement protocol

Real tape echoes share **one** transport modulator between wow and flutter —
UAD Galaxy is no different. The output pitch deviation observed at the
playback head depends on how much tape is between record and playback, so
depth scales with delay time. The rates do not.

Capture the two endpoints of the UAD's delay range and we have enough to fit
a depth-vs-delay function:

| recording          | UAD delay | what we extract     |
| ------------------ | --------: | ------------------- |
| `tone_uad_69ms.wav`  |  69 ms   | rate + depth (both bands) |
| `tone_uad_487ms.wav` | 487 ms   | rate + depth (both bands) |

Expected outcome:

- `wow_rate_hz` and `flutter_rate_hz` ≈ constant across the two captures.
- `wow_depth_cents` and `flutter_depth_cents` proportional to delay
  (give or take an offset). We fit `depth(d) = k·d` first; if there's a
  meaningful intercept we move to `depth(d) = k·d + b`.

The fit lands in `tapeecho4.c` where the wow/flutter LFO outputs are scaled
per sample.

## Compact FIR kernel from a clean IR

`TapeEcho4` uses a short causal FIR as its baseline tape/head fingerprint.
Generate a C coefficient table from a clean IR capture with:

```
$PY extract_fir_kernel.py Tape/Tape_IR.wav --taps 64 --target-fs 44100
```

The script finds the peak, discards the pre-impulse samples, resamples the
causal tail to the pedal rate, truncates it, and normalizes the kernel at
1 kHz. The current 64-tap Galaxy-derived kernel tracks the full clean IR
within roughly 0.34 dB RMS across 30 Hz to 18 kHz.
