# State ABI Progress

Last updated: 2026-05-18

This is the compact current-state map for the hardware probes. The older
probe-by-probe chronology is preserved in git history through commit
`dc5eedb`; keep this file focused on durable findings and next decisions.

## Current Conclusions

* Large Airwindows state must not live in `.fardata`. Stock ZDLs usually keep
  `.fardata` tiny, and large custom writable static data has frozen hardware.
* `ctx[3]` is the large host-managed descriptor arena. It is readable, writable,
  persistent across audio callbacks, and per instance in the tested duplicate-FX
  cases.
* The default `ctx[3]` allocation is large enough for `StereoChorus`: hardware
  probes reached at least 705,536 bytes. The exact upper byte is no longer
  needed for the current ports.
* `StereoChorus` is the best state reference: it uses `ctx[3]`, lazy clears its
  large buffers, and the current release was reported to sound like Airwindows
  `StereoChorus`.
* ToTape9's load crash was not a ctx[3] lazy-init failure. `T9InitOnly`
  clears/finalizes the default state and loads cleanly. Removing runtime
  `__c6xabi_divf` from the full DSP path produced a hardware-reported working
  `dist/ToTape9.ZDL`. The reload mute bug was addressed in the next build by
  treating zero user slots as unmaterialized defaults and by scaling the
  LineSel raw `0..0.14` handler range before caching critical controls.
* The C/asm `ZOOM_EDIT_HANDLER` macro is not a safe release path for multi-page
  controls. `T9NoAudio` loads with DSP NOPed, then freezes on knob/page
  interaction.
* Stock-style init setup is partly mapped. `InitProbe` stage 2 proved a
  custom `_init` can safely call the host's coefficient-table setup callback
  through `__c6xabi_call_stub`, once PCR_S21 external relocations are patched
  correctly. Stage 3, calling a cloned LineSel edit handler from that init
  context, froze on boot. The likely missing ABI detail is the init-time host
  state/callback table expected by stock edit handlers.
* Category visibility is partly host/browser state. On the MS-70CDR test
  pedal, Drive-category `ToTape9` could flash but stayed hidden until at least
  one stock Drive effect was also installed.

## Runtime Map

The minimal custom audio entry map is:

| Field | Status | Meaning |
|---:|---|---|
| `ctx[1]` | confirmed | parameter float table |
| `ctx[2]` | partial | stock state/scratch header; small derived blocks at `+0x10` and `+0x18` are writable and persistent |
| `ctx[3]` | confirmed | large descriptor: `[0] = base`, `[1] = end`, `[2] = span/length` |
| `ctx[4]` | confirmed | dry/input buffer |
| `ctx[5]` | confirmed | effect/wet buffer |
| `ctx[6]` | confirmed | output accumulator; add into this, do not overwrite |
| `ctx[11]` | confirmed-use, unknown-purpose | magic shuttle destination |
| `ctx[12]` | confirmed-use, unknown-purpose | magic shuttle source |
| `ctx[13]` / `ctx[14]` | stock-used, unresolved | modulation/stereo-adjacent candidates; custom meaning unknown |

Audio buffers are float32. The observed stock/custom-safe pattern processes
8 samples per channel per callback: `LLLLLLLL RRRRRRRR`.

## Hardware-Proven Milestones

| Probe / build | Finding |
|---|---|
| `ParamTap` / LineSel handlers | First two stock-derived knob handlers update `params[5]` and `params[6]`. |
| `StatePing` | `ctx[2] + 0x10` and `ctx[2] + 0x18` can be written without crashing. |
| `StateIso` | `ctx[2] + 0x18` behaves per instance in the tested two-slot case. |
| `StateComb` | `ctx[2] + 0x18` can hold small DSP history, not only stamps. |
| `DescComb` | `ctx[3]` descriptor memory can hold audible delay history. |
| `DescSize` | Default `ctx[3]` descriptor allocation is at least 705,536 bytes. |
| `DescIso` | Duplicate instances do not share observed `ctx[3]` descriptor memory. |
| `StChorus` | Large stateful Airwindows-style effect can run from `ctx[3]`. |
| `T9InitOnly` | ToTape9-sized lazy state init/clear is load-safe. |
| `T9NoState` | A simple ToTape9-shaped DSP path can run; Input audibly changes gain. |
| `ToTape9` | No-divide full DSP loads and runs on the test MS-70CDR. |
| `VerbTiny` | First Airwindows reverb candidate; builds with ctx[3] state, no `.fardata`, and no object relocations. Hardware result pending. |
| `Galactic` | Larger Airwindows reverb candidate; builds with about 528 KB of ctx[3] state, no `.fardata`, and no object relocations. Hardware result pending. |
| `TapeEcho4` | Custom Airwindows-inspired tape echo; builds with about 512 KB of ctx[3] delay state, no `.fardata`, no `.text`, and no object relocations. Tempo control is BPM+division with stock tempo descriptor flags; true host tap-tempo behavior is pending. |
| `InitProbe` | Object-defined init setup call loads; init-time cloned edit-handler call freezes on boot. |

## Init And Edit-Handler ABI Status

Known safe enough:

* NOP init.
* Stock-style coefficient-table setup from a custom `_init`: load callback
  pointer from state offset `+136`, pass `ctx[1]`/param table, pass
  `_<AudioFunc>_Coe`, and call `__c6xabi_call_stub`.
* Stock LineSel `onf`, knob 1, and knob 2 handlers when invoked by normal user
  interaction.
* AIR's third-knob blob in limited contexts, though it is not a general
  page-2/3 solution.
* Synthesized LineSel-cloned handlers as a linker strategy, but only after an
  isolated tiny-DSP hardware confirmation.

Known unsafe:

* Calling a cloned LineSel edit handler from custom `_init`; `InitProbe` stage
  3 froze on boot after setup plus one edit-handler call.
* The object-defined `ZOOM_EDIT_HANDLER` macro path for multi-page UI builds.
  It pulls in `__c6xabi_call_stub` and freezes on interaction in `T9NoAudio`.

## ToTape9 Split Status

| Variant | Result | Meaning |
|---|---|---|
| `T9Meta` | loads | descriptor/image/link shape can load when DSP is NOPed. |
| `T9NoAudio` | loads, then freezes on edit/page change | object-defined edit handlers are a separate UI-interaction failure. |
| `T9NoHand` | freezes on load | full DSP path is unsafe. |
| `T9NoInit` | loads | returning before state init is safe, but DSP never enters. |
| `T9InitOnly` | loads | lazy ctx[3] init/clear/finalization is safe. |
| `T9DspNoLoop` | old build froze; current no-divide split no longer the priority | parameter derivation / `computeHDB` builds without `__c6xabi_divf`. |
| `T9NoState` | loads | helper-light scalar DSP path is viable. |
| `ToTape9` | loads and runs | full ctx[3]-backed no-divide DSP is viable enough for listening/exactness work; Drive category needs a stock Drive effect present on MS-70CDR to show in the pedal browser. |

Next ToTape9 work:

1. Verify the zero-as-unmaterialized parameter fallback. ToTape9 still treats
   the host parameter table as read-only. Nonzero values in the LineSel
   `0..0.14` raw range are normalized before use/cache; zero/NaN values fall
   back to manifest defaults because reload can expose zeroed params before an
   edit interaction materializes the host table. This means true knob-at-zero
   cannot currently be distinguished from "not materialized yet".
2. Test whether saved presets and preset switches preserve edited values with
   the read-only fallback path.
3. Add a desktop comparison harness before calling ToTape9 source-equivalent.
4. Separately build a tiny-DSP page 2/3 parameter probe using synthesized
   LineSel-cloned handlers to prove `params[7..13]` updates independently from
   the tape kernel.

## Reverb Port Status

`VerbTiny` was chosen as the first reverb target because its Airwindows delay
network is much smaller than the older `Reverb`/plate families. The current
Zoom port keeps the VerbTiny delay constants, five source parameters, matrix
feedback structure, and bezier undersampling/filter reconstruction, but stores
the network in a rectangular `ctx[3]` state layout rather than source C++
member arrays. The Airwindows float dither tail is omitted like the other Zoom
ports.

`Galactic` is the next reverb candidate. It uses the original Galactic delay
network converted from double to float32 in `ctx[3]`, with about 528 KB of
delay/scalar state. The build assumes 44.1 kHz (`cycleEnd = 1`) and omits the
Airwindows dither tail.

Hardware status: both are untested. First tests should be basic load,
unbypass, page 2 parameter interaction, reload, and duplicate-instance
behavior.

## Custom Tape Echo Status

`TapeEcho4` is intentionally a custom effect rather than an exact Airwindows
port. It combines safe Airwindows-derived tape techniques: TapeHack-style soft
clipping, TapeDelay/TapeDelay2-inspired feedback filtering and modulation, and
a bounded stereo delay line in `ctx[3]`. The state size is below the proven
descriptor lower bound, and the first build has `.audio` only, no `.fardata`,
and no object relocations. Hardware testing still needs load, unbypass,
parameter paging, reload, and duplicate-instance checks. The `Tempo` descriptor
uses the stock tempo flag pattern, but custom-ZDL access to the pedal's global
tap tempo is still unresolved.

## Open Questions

* What declares or toggles stock stereo routing for custom ZDLs?
* Can stock tempo/tap-tempo descriptor flags feed meaningful host tempo values
  into custom ZDL parameters, or do custom effects need their own BPM control?
* What exactly do `ctx[13]` and `ctx[14]` represent in stock modulation effects?
* What are the lifecycle rules for ctx state during bypass, preset switch,
  reload, and duplicate instances beyond the cases already tested?
* Which runtime helpers are safe enough to bundle, and which must be avoided?
* Can a compact reloc-free page 2/3 edit handler be written or cloned safely?
* What init-time state fields make stock edit handlers safe to call during
  load, and are those fields available to custom effects?

## How To Record New Findings

Keep this file short:

* Add only durable conclusions or a small table row for a new hardware result.
* Move detailed flash instructions into `src/hardware_probes/` or the relevant
  effect directory.
* If a conclusion changes, replace the old claim instead of appending a long
  contradiction.
