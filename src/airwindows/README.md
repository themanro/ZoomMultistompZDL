# Airwindows Port Layout

Before adding or deepening a port, read
[AGENT_CONTEXT.md](AGENT_CONTEXT.md) and
[../../docs/SAFE-DSP-RULES.md](../../docs/SAFE-DSP-RULES.md). The short
version: exact source parameter metadata is good; full desktop DSP with large
state is not automatically safe on the pedal.

also [AIRWINDOWS-EXACT-PORTS.md]

For effects marketed as Airwindows ports, also read
[../../docs/AIRWINDOWS-EXACT-PORTS.md](../../docs/AIRWINDOWS-EXACT-PORTS.md).
If the DSP is an approximation or an ABI probe, say that plainly in the
manifest and comments. `StChorus` is now the first hardware-confirmed
`ctx[3]`-backed `StereoChorus` port; keep documenting remaining numerical
differences before calling it bit-for-bit equivalent.

Hardware-only ABI probes live in [../hardware_probes/](../hardware_probes/),
not in this Airwindows source tree. Their findings belong in
[../../docs/STATE-ABI-PROGRESS.md](../../docs/STATE-ABI-PROGRESS.md) before
they are used to justify a stateful port.

Each effect directory is meant to be buildable on its own and through
`python3 build_all.py`.

Required files:

- `manifest.json`: effect name, category, fxid, version, and 1-9 params.
- `build.py`: compiles the C file with TI `cl6x`, then calls
  `build/linker.py`.
- `<effect>.c`: exports the manifest's `audio_func_name` in `.audio`.

Manifest params can describe more than plain `0..100` knobs:

```json
{
  "name": "Mode",
  "type": "switch",
  "max": 1,
  "default": 0,
  "labels": ["Off", "On"],
  "audio_default": 0
}
```

For continuous controls, keep UI range and DSP range separate:

```json
{
  "name": "Freq",
  "type": "knob",
  "min": 0,
  "max": 100,
  "default": 50,
  "scale": "hz",
  "unit": "Hz",
  "audio_min": 25.0,
  "audio_max": 200.0,
  "audio_default": 0.5
}
```

The linker writes descriptor fields from `min/max/default/flags`. Build
scripts can call `write_param_header(...)` from
`common/manifest_params.py` so DSP code gets generated slot/default/range
defines from the same manifest.

For ports with more than two controls, do not use
`../common/zoom_edit_handlers.h` as the release path yet. The
`ZOOM_EDIT_HANDLER` macro is useful as a diagnostic object-defined handler, but
`T9NoAudio` hardware-tested as load-safe only until knob/page interaction, then
froze. The current safer path is to let the linker use the stock LineSel first
two handlers and, for experimental page 2/3 controls, set
`synthesize_linesel_edit_handlers=True` in a tiny-DSP probe before coupling
those controls to a full kernel.

Keep writable `.fardata` tiny. The linker rejects large writable images by
default because big static state has frozen real pedals during load. For large
stateful ports, use the proven `ctx[3]` descriptor arena and validate the
descriptor before touching memory. `StereoChorus`, `T9InitOnly`, and the
current no-divide `ToTape9` build prove this can work; `VerbTiny` is the first
reverb-sized candidate using the same strategy. New full-kernel ports still
need the same load-safety ladder: audio-NOP with the final UI shape, tiny
pass-through DSP, then helper-free DSP increments.
