# ZDL and Pedal Reverse-Engineering Status

Last updated: 2026-05-18

This is the current map of the Zoom `.ZDL` wrapper, the embedded TI C6000 ELF,
and the runtime ABI details that matter for custom effects. Keep experiment
transcripts out of this file; put durable state/edit-handler findings in
[STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md).

## Current Snapshot

What is working:

* The repo can build loadable custom `.ZDL` effects without Zoom's SDK.
* `dist/StChorus.ZDL` is the current best Airwindows reference: it uses the
  host-managed `ctx[3]` arena for large state and was reported to sound like
  Airwindows `StereoChorus`.
* `dist/` is kept release-focused. Hardware probe ZDLs are buildable, but are
  not shipped by default.
* The 830-file stock ZDL corpus in `stock_zdls/` round-trips through the parser,
  aside from five stock files with stale embedded SIZE fields that canonicalize
  to the actual ELF length.

What is still experimental:

* `dist/ToTape9.ZDL` is not a finished port, but the no-divide full DSP now
  loads and runs on the test MS-70CDR. Current validation work is parameter
  initialization, preset behavior, and source-equivalence testing.
* `dist/VerbTiny.ZDL` is the first reverb candidate. It builds with `ctx[3]`
  state, no `.fardata`, and no object relocations; hardware result is pending.
* `dist/Galactic.ZDL` is a larger reverb candidate. It builds with about
  528 KB of `ctx[3]` state, no `.fardata`, and no object relocations; hardware
  result is pending.
* `dist/TEcho4.ZDL` is a custom Airwindows-inspired Delay-category tape
  echo. It builds with a bounded `ctx[3]` stereo delay line, no `.fardata`, no
  object relocations, and a BPM+division tempo workflow. Hardware result is
  pending; true host tap-tempo plumbing for custom ZDLs is still unresolved.
* `dist/OTT.ZDL` is a custom Dynamics-category OTT-style 3-band compressor,
  not an Ableton port. It uses a small `ctx[3]` state block for crossover,
  envelope, and gain history, and builds with no `.fardata`, no `.text`, and no
  object relocations. Hardware result is pending.
* `src/airwindows/tovinyl4/` is a source-only ToVinyl4 candidate. It builds
  manually or via `build_all.py tovinyl4`, but is not in release `dist/` until
  hardware testing says it should be.
* Multi-page parameter editing is not a clean SDK contract yet. Stock LineSel
  handlers are useful for the first two knobs; object-defined
  `ZOOM_EDIT_HANDLER` freezes on interaction in `T9NoAudio`; synthesized
  LineSel clones still need an isolated page 2/3 tiny-DSP proof.
* Init-time parameter materialization is still unsolved. `InitProbe` proved the
  stock-style coefficient setup callback can be called from custom `_init`, but
  invoking a cloned LineSel edit handler from that custom init froze on boot.
* The custom stereo routing declaration remains unknown. Custom code can process
  stereo buffers once running, but the stock effect-level stereo mechanism has
  not been mapped.
* Category exposure has a host/browser dependency. On the MS-70CDR test pedal,
  Drive-category `ToTape9` flashed successfully but only appeared in the
  on-device FX browser after a stock Drive effect was also installed.

## ZDL File Layers

| Layer | Current understanding |
|---|---|
| Wrapper | Starts with four zero bytes, `SIZE`, then `INFO`. |
| Header size | Standard files place ELF at offset `0x4c`; extended files must honor the `SIZE` header-size field. |
| Extended payloads | Some stock files carry `BCAB` or `CABI` payloads between `INFO` and ELF. Their fields are preserved, not fully decoded. |
| Embedded executable | TI C6000 ELF32, little-endian, `ET_DYN`. |
| Relocations | `.rela.dyn` is used for descriptor pointers, image pointers, and code-address materialization. Object `PCR_S21` relocations need different addend handling for section-local calls versus external symbols. |
| Exported symbols | Firmware finds names such as `Dll_<Name>`, `Fx_<GID>_<Name>`, init/onf/edit handlers, `SonicStomp`, `effectTypeImageInfo`, picture, and knob info. |

Stock corpus observations:

| Observation | Result |
|---|---:|
| Stock files in corpus | 830 |
| Header payload 56 bytes | 754 |
| Header payload 232 bytes | 24 |
| Header payload 312 bytes | 52 |
| PT_LOAD segments with `memsz > filesz` | 0 |
| Stock effects with 9 user parameters | 114 |
| Largest observed executable PT_LOAD | 18,016 bytes |
| Largest observed `.fardata` | 220 bytes |

Loader rule for custom builds: keep `memsz == filesz`, keep `.fardata` tiny,
and avoid assuming a normal C runtime zero-fill/startup path.

## Runtime Model

The audio function receives a `ctx` pointer in the first C6000 argument
register. The useful custom map is:

| Field | Meaning |
|---:|---|
| `ctx[1]` | parameter float table |
| `ctx[2]` | stock state/scratch header; small derived blocks are writable |
| `ctx[3]` | large descriptor: base, end, span/length |
| `ctx[4]` | dry/input buffer |
| `ctx[5]` | effect/wet buffer |
| `ctx[6]` | output accumulator |
| `ctx[11]` / `ctx[12]` | preserve the stock magic shuttle |
| `ctx[13]` / `ctx[14]` | stock-used, still unresolved |

Audio data is float32. The known processing shape is 8 samples per channel per
callback: `LLLLLLLL RRRRRRRR`.

For details on `ctx[2]`, `ctx[3]`, descriptor-size probes, and ToTape9 split
results, see [STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md).

## Parameter Model

| Slot | Meaning |
|---:|---|
| `params[0]` | on/off as float |
| `params[4]` | normalizer, commonly `1 / max` |
| `params[5]` | user knob 1 raw value |
| `params[6]` | user knob 2 raw value |
| `params[7]..params[13]` | user knobs 3..9 |

Parameter-scaling caution: different handler/descriptor combinations can expose
different raw ranges. Do not claim source-equivalent control laws until the raw
knob scale for that effect's handler path has been confirmed on hardware.

## Airwindows Port Boundary

A build is not a 1:1 Airwindows port until all of these are true:

1. The manifest matches source parameters: names, order, defaults, labels, and
   control laws.
2. The DSP kernel is the source algorithm, with only documented mechanical
   changes for the C674x toolchain.
3. Persistent state survives across audio calls like the source plugin instance
   state.
4. Hardware has tested load, bypass, preset switching, parameter edits, and
   audio output.

`StereoChorus` is the best current reference. `ToTape9` shows the next boundary:
state and helper-light DSP can run, but source-equivalent claims still need
parameter lifecycle tests and a desktop comparison harness. `VerbTiny` and
`Galactic` start the same process for reverb-sized state.

## Highest-Priority Open Questions

* Does the current ToTape9 zero-as-unmaterialized fallback and corrected
  `0..0.14` raw handler scaling stop first-touch Bias/Output and zero-output
  startup issues? The first edit-calling object-defined init shim crashed on
  boot; setup-only `InitProbe` is safe, so the next step is mapping the missing
  edit-handler init context.
* Do the new `VerbTiny` and `Galactic` reverb candidates load and pass audio on
  the test MS-70CDR, and do synthesized page 2 controls survive interaction?
* Do synthesized LineSel-cloned page 2/3 edit handlers update `params[7..13]`
  correctly in an isolated tiny-DSP probe?
* What declares stock-style stereo routing for custom effects?
* Do stock tempo/tap-tempo descriptor flags work for custom effects beyond UI
  presentation?
* What are the lifecycle rules for ctx state during bypass, preset switch,
  reload, and duplicate instances?
* What exactly are `ctx[11]` and `ctx[12]`, and how dangerous is skipping the
  shuttle in complex effects?
* What are the semantics of extended `BCAB` and `CABI` header payloads?

## Useful References

* [build/ABI.md](../build/ABI.md) - low-level linker/runtime ABI reference.
* [SAFE-DSP-RULES.md](SAFE-DSP-RULES.md) - hardware-safe DSP constraints.
* [AIRWINDOWS-EXACT-PORTS.md](AIRWINDOWS-EXACT-PORTS.md) - exact-port rules.
* [AIRWINDOWS-1TO1-PORT-ROADMAP.md](AIRWINDOWS-1TO1-PORT-ROADMAP.md) - current
  porting plan.
* [TI-PDF-NOTES.md](TI-PDF-NOTES.md) - distilled TI C6000 manual notes.
