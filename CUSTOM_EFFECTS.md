# Custom Effect Pack (themanro)

A curated **library of 22 effects** for the Zoom MS-70CDR (ZDL family),
built on the [repeat98/ZoomMultistompZDL](https://github.com/repeat98/ZoomMultistompZDL)
toolchain. Eighteen are originals (most with a full-quality Python **desktop
preview** in `tools/audio_preview/renderers/`); the rest are rebuilt/renamed
Airwindows-derived ports plus one contributed effect (Dustbox). All are grouped
under the **Delay** category with a custom on-device cover, and all compile
clean (`.fardata 0`, `0 relocations`).

> ⚠️ **Status:** curated after hardware listening, and the whole pack was rebuilt
> on 2026-09-10 with working load-time parameter initialization.

### Every effect ends with Mix

Levels across the pack used to be inconsistent, and the naming was most of the
reason. Galactic called it DryWet, Howl and Scorch called it Level, Oxide called
it Output, and GenLoss had no level control at all. Worse, the ones called Level
or Output were not crossfades: Howl's only scaled a wet send to a 0.45 ceiling,
and Scorch's was an output gain topping out at 0.5×, so Scorch could never reach
unity however far it was turned.

Now every effect ends with a real dry/wet **Mix**, default 50 — except Dustbox,
whose fourth knob is Power. Stasis uses a true Mix while a capture is held and
passes dry stereo unchanged when idle; its sixth control selects stomp or
explicit capture.

Effects that saturate lift RMS regardless of input, so a few carry a named wet
trim (`SC_WET_TRIM`, `TOTAPE9_WET_TRIM`, `GL_WET_TRIM`) applied before the
crossfade. Those are by-ear calibration points, not derived constants.

### Known gaps

* **Oxide `HeadBmp`.** In this reduced core it is a broadband
  drive + level lift, not a low-frequency resonance — there is no filter behind
  it, so it is not a bass control however it reads by ear. `HeadFrq` used to sit
  beside it and controlled no frequency either: it only scaled the same lift, so
  two knobs adjusted one quantity. Its slot became `Output`, a makeup gain the
  pack was otherwise missing.
* Some features are deferred on the originals (Klang's frequency-shifter modes,
  GenLoss dropouts, Scorch's full cab IR). Compare each effect's `manifest.json`
  against its `manifest_pedal.json` to see what was cut for the pedal build.
* Knobs 4 and beyond use edit handlers synthesised by the build rather than stock
  ones. These are now confirmed working in all six slots, including at patch load
  — see [docs/RELEASE-INIT-AUDIT.md](docs/RELEASE-INIT-AUDIT.md). Editing them
  live over MIDI is still limited to slots 1–3 by the firmware; the patch editor
  handles 4–6 for you ([docs/MIDI-PARAM-EDIT.md](docs/MIDI-PARAM-EDIT.md)).

## Sound previews

Rendered demos (click to play in GitHub's audio viewer) for the effects that
have a desktop renderer. These are the **desktop** renders — full-quality,
before the pedal knob reduction.

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
| Howl | [howl.wav](previews/audio/howl.wav) — self-oscillating feedback | [guitar](previews/audio/dry_guitar.wav) |
| Hydra | [hydra.wav](previews/audio/hydra.wav) — double-time ghost + half-time drag | [drums](previews/audio/dry_drums.wav) |
| Spiral | [spiral.wav](previews/audio/spiral.wav) — slow 8-second rising delay | [guitar](previews/audio/dry_guitar.wav) |

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

Pre-built `.ZDL` files for all 22 are in [`dist/`](dist/).

## Implementation notes

All pedal builds follow the repo's safe-DSP rules: no math library (polynomial
sines, cubic soft-clips, reciprocal approximations, baked filter coefficients),
no runtime divide, persistent state in the `ctx[3]` arena, the `ctx[11]/ctx[12]`
magic shuttle preserved, and **no static arrays** (scalar float literals compile
to immediates and stay relocation-free; arrays would force a code→data
relocation, a documented freeze risk).

Licensing follows the parent repo (MIT for repo code).
