# Desktop Audio Preview

Render host-side reference versions of custom effects against ordinary WAV
files before flashing a pedal. A `.ZDL` contains TI C674x machine code, so the
desktop cannot execute arbitrary release binaries directly. Each supported
effect has a small renderer that mirrors its pedal DSP formulas.

Install the Python dependencies:

```bash
python3 -m pip install numpy scipy soundfile
```

Show release-source effects and preview coverage:

```bash
python3 tools/audio_preview/preview.py list
```

Render TapeEcho4 using manifest defaults:

```bash
python3 tools/audio_preview/preview.py render tapeecho4 input.wav preview.wav
```

Override pedal-style parameter values:

```bash
python3 tools/audio_preview/preview.py render tapeecho4 input.wav preview.wav \
  --set tempo=105 --set feed=82 --set flutter=48 --set wow=42 \
  --set wear=66 --set drive=52 --set spring=0 --set mix=70
```

Render every effect that currently has a desktop adapter:

```bash
python3 tools/audio_preview/preview.py render-all input.wav preview-output/
```

The CLI deliberately lists effects without adapters as `adapter needed`.
Desktop renderers are listening tools, not substitutes for hardware tests:
they do not exercise the ZDL loader, runtime ABI, edit handlers, or pedal CPU
budget.

TapeEcho4's desktop spring path is intentionally a listening target rather
than a mirror of the current ZDL fallback. It combines the first 744 ms of the
measured Galaxy spring capture with an eight-line FDN tail using `0.92`
feedback, `0.55` damping, and a 320 ms equal-power handoff. The longer model
still needs a bounded pedal-safe implementation.
