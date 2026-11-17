# Hardware Probes

This directory contains diagnostic ZDLs used to reverse-engineer the Zoom
runtime ABI. They are not Airwindows ports and are not part of the normal
release set in `dist/`.

Build one probe by name:

```bash
python3 -B build_all.py initprobe
python3 -B build_all.py ctxwatch
python3 -B build_all.py matprobe
```

Build release effects plus every probe:

```bash
python3 -B build_all.py --all
```

Probe artifacts built through `build_all.py` are moved to `build/probes/`.
Keep `dist/` for working release effects only.

Each probe keeps its own `manifest.json`, `build.py`, and DSP source. Probe
build scripts reuse the shared linker and helper code from `build/` and
`src/airwindows/common/`.

## Writing a probe that is worth flashing

Two lessons from the `_init` materialization work, where four "candidate fix"
flashes each froze the pedal and taught nothing, and one probe that did nothing
at all located the fault immediately.

**Make it fail safe.** `matprobe` exists to answer "did params materialize at
load", so its Gain knob **defaults to 0**. An unmaterialized load is therefore
*silent* — an unambiguous signal — rather than some sound you have to judge.
Design the failure to be legible before you design the probe.

**Flash the question, not the answer.** The probe that settled the `_init`
question was the frame with **zero** handler calls in it. It could not fix
anything; it only asked "can our `_init` frame execute at load?". It booted,
which put the fault squarely in the handler calls after three fixes had guessed
wrong. Bisect first:

```bash
# the frame only -- no handler calls at all
ZDL_MATERIALIZE_INIT=1 ZDL_MATERIALIZE_INIT_MAX_CALLS=0 \
  python3 -B src/hardware_probes/matprobe/build.py

# ...then 1, 2, ... to find which call is fatal
ZDL_MATERIALIZE_INIT=1 ZDL_MATERIALIZE_INIT_MAX_CALLS=1 \
  python3 -B src/hardware_probes/matprobe/build.py
```

**Verify what was actually emitted.** `python3 -B build/disassemble_zdl.py
build/probes/MatProb.ZDL` — check the bytes that will be on the pedal, not the
template you think you assembled.

**A probe in `dist/` is a shipping bug.** The install docs tell people to point
Effect Manager at `dist/`, so a stray diagnostic ends up on a stranger's pedal.
`build_all.py` checks for this; a hand-run per-effect `build.py` writes to
`dist/` and does **not**, so move the file yourself. EdgeWatch and MatProb have
each escaped that way.

## Current probes

| Probe | Question it answers |
|---|---|
| `matprobe` (fxid 496) | Did `_init` materialize params at load? Gain defaults to 0, so no = silence. |
| `initprobe` | `_init` entry conditions ([INIT-MATERIALIZATION](../../docs/INIT-MATERIALIZATION.md)). |
| `ctxwatch` | Whether any `ctx[]` region tracks a patch write when `params[]` does not ([MIDI-PARAM-EDIT](../../docs/MIDI-PARAM-EDIT.md)). |
| `blink`, `redraw`, `coverrw` | How an effect animates its own screen ([animated covers](../../docs/pedal-research/animated-covers.md)). |
| `bpmhunt` | A blind memory scan for the patch tempo; found nothing ([TEMPO-SYNC](../../docs/TEMPO-SYNC.md)). |
| `reid` | Whether an effect runs, saves and recalls under a moved effect ID ([README](reid/README.md)). |
| `namtest`, `namlite`, `nam_memory`, `nam_profile` | The NAM capture runtime ([NAM-A2-LITE-PROTOTYPE](../../docs/NAM-A2-LITE-PROTOTYPE.md), [NAM-RUNTIME-INVESTIGATION](../../docs/NAM-RUNTIME-INVESTIGATION.md)); `namlite` also feeds the loader templates. |
| `cab_ir` | The cab IR loader ([NAM-LOADER](../../docs/NAM-LOADER.md)). |
| `slo_ir` | The withdrawn SloIR prototype ([SLO-IR-PROTOTYPE](../../docs/SLO-IR-PROTOTYPE.md)). |

Removed on 2026-11-09, because nothing linked to them any more: the `ctx[]` map
probes (`ctxmap`, `ctxnib`, `ctxgate`, `paramtap`), the state block probes
(`stateping`, `stateiso`, `statecomb`), the arena descriptor probes (`desccomb`,
`descsize`, `desciso`), `edgewatch`, `nam_timing` and `syncprobe` v1–v3. Their
findings are in [STATE-ABI-PROGRESS](../../docs/STATE-ABI-PROGRESS.md),
[ABI](../../build/ABI.md), [MIDI-PARAM-EDIT](../../docs/MIDI-PARAM-EDIT.md) and
[TEMPO-SYNC](../../docs/TEMPO-SYNC.md); their sources are in git history (commit
afb3371 and earlier).
