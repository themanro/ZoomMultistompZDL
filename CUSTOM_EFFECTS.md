# Custom Effect Pack (themanro)

Eight original custom effects for the Zoom MS-70CDR (ZDL family), built on the
[repeat98/ZoomMultistompZDL](https://github.com/repeat98/ZoomMultistompZDL)
toolchain. Each effect has a full-quality **desktop preview** (Python, in
`tools/audio_preview/renderers/`) and a **2-knob pedal build** (`src/custom/<name>/`).

> ⚠️ **Status: not yet hardware-tested.** Every effect compiles clean with the
> repo's ideal safe-build markers (`.fardata 0 bytes`, `0 relocations`), but
> none have been verified on a real pedal. Experimental builds can freeze a
> pedal until power-cycled — **back up your effect list and flash one at a
> time.**

## The effects

| Effect | `.ZDL` | Category | Pedal knobs | What it is |
|---|---|---|---|---|
| Microloom | `Microlm` | Delay | Regen, Mix | Microcosm-style granular pitch-shimmer cloud |
| Flower | `Flower` | Modulation | Rate, Mix | Korg "Random" S&H step filter (Deftones "Digital Bath") |
| Shatter | `Shatter` | Delay | Chance, Mix | Stutter / beat-repeat glitch for drums |
| Arrakis | `Arrakis` | Modulation | Detune, Mix | Dune-style detuned sub-octave beating drone |
| Corrupt | `Corrupt` | Drive | Sub, Mix | EQD Data Corrupter-style PLL square synth |
| Klang | `Klang` | Modulation | Freq, Mix | Ring modulator |
| GenLoss | `GenLoss` | Modulation | Wow, Tone | Tape/VHS generation-loss degradation |
| Scorch | `Scorch` | Drive | Gain, Level | Aggressive high-gain amp + cab |

The pedal builds are intentionally **2-knob** (the hardware-proven UI shape);
the desktop previews expose the full control set. Some features are deferred on
the pedal (Klang's frequency-shifter modes, GenLoss dropouts, Scorch's full
cab IR) — see each effect's `manifest.json` vs `manifest_pedal.json`.

## Sound previews

Rendered demos (click to play in GitHub's audio viewer). These are the
**desktop** renders — full-quality, before the 2-knob pedal reduction.

| Effect | Demo | Dry source |
|---|---|---|
| Microloom | [microloom.wav](previews/audio/microloom.wav) — lush shimmer wash | [chord](previews/audio/dry_chord.wav) |
| Flower | [flower.wav](previews/audio/flower.wav) — Digital Bath random filter | [chord](previews/audio/dry_chord.wav) |
| Shatter | [shatter.wav](previews/audio/shatter.wav) — machine-gun stutter | [drums](previews/audio/dry_drums.wav) |
| Arrakis | [arrakis.wav](previews/audio/arrakis.wav) — −2 oct beating drone | [drone](previews/audio/dry_drone.wav) |
| Corrupt | [corrupt.wav](previews/audio/corrupt.wav) — PLL square synth | [guitar](previews/audio/dry_guitar.wav) |
| Klang | [klang.wav](previews/audio/klang.wav) — metallic ring mod | [chord](previews/audio/dry_chord.wav) |
| GenLoss | [genloss.wav](previews/audio/genloss.wav) — wrecked tape | [chord](previews/audio/dry_chord.wav) |
| Scorch | [scorch.wav](previews/audio/scorch.wav) — djent high-gain amp+cab | [riff](previews/audio/dry_riff.wav) |

Regenerate or explore other presets with the preview tool below.

## Hearing them on desktop (no compiler, no pedal)

The renderers mirror each effect's DSP so you can audition before flashing:

```bash
pip install numpy scipy soundfile
python3 tools/audio_preview/make_test_signal.py input.wav        # or make_drum_loop.py
python3 tools/audio_preview/preview.py render scorch input.wav out.wav --set gain=85
python3 tools/audio_preview/preview.py list                      # all effects + coverage
```

## Building the pedal `.ZDL`

Requires the TI C6000 compiler (see the main README). The build scripts here
point `TI_ROOT` at `/Applications/ti/ti-cgt-c6000_8.5.0.LTS` — edit if yours
differs.

```bash
python3 build_all.py flower          # one effect -> dist/Flower.ZDL
python3 build_all.py scorch          # etc.
```

Pre-built `.ZDL` files for all eight are in [`dist/`](dist/).

## Implementation notes

All pedal builds follow the repo's safe-DSP rules: no math library (polynomial
sines, cubic soft-clips, reciprocal approximations, baked filter coefficients),
no runtime divide, persistent state in the `ctx[3]` arena, the `ctx[11]/ctx[12]`
magic shuttle preserved, and **no static arrays** (scalar float literals compile
to immediates and stay relocation-free; arrays would force a code→data
relocation, a documented freeze risk).

Licensing follows the parent repo (MIT for repo code).
