# State ABI Progress

Last updated: 2026-05-17

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
  `dist/ToTape9.ZDL`; current follow-up is parameter/default lifecycle testing.
* The C/asm `ZOOM_EDIT_HANDLER` macro is not a safe release path for multi-page
  controls. `T9NoAudio` loads with DSP NOPed, then freezes on knob/page
  interaction.
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

1. Verify the parameter fallback change. Audio-side writes to `params[5..13]`
   were removed because the pedal can remember values across browsing/loading;
   ToTape9 now treats the host parameter table as read-only. Fallback is
   page-granular for the three visible UI pages, plus `ctx[3]` caches for the
   mute-capable controls (`Input`, `Bias`, `Output`). Valid nonzero host reads
   update those caches; empty/zero reload reads use the cached/default value so
   an unmaterialized slot cannot mute the effect. The audio on/off gate is also
   ignored while any mute-capable slot is empty, because reload can expose
   zeroed params before an edit interaction materializes the host table. The
   incomplete-reload `ctx[6]` writeback experiment did not fix the pedal
   report. A follow-up object-defined init shim that invoked the
   descriptor-selected on/off and all nine edit handlers crashed the pedal on
   boot, so release ToTape9 is back to a NOP init while the init-call ABI is
   investigated separately.
2. Test whether saved presets and preset switches preserve edited values with
   the read-only fallback path.
3. Add a desktop comparison harness before calling ToTape9 source-equivalent.
4. Separately build a tiny-DSP page 2/3 parameter probe using synthesized
   LineSel-cloned handlers to prove `params[7..13]` updates independently from
   the tape kernel.

## Edit-Handler ABI Status

Known safe enough:

* Stock LineSel `onf`, knob 1, and knob 2 handlers.
* AIR's third-knob blob in limited contexts, though it is not a general
  page-2/3 solution.
* Synthesized LineSel-cloned handlers as a linker strategy, but only after an
  isolated tiny-DSP hardware confirmation.

Known unsafe:

* The object-defined `ZOOM_EDIT_HANDLER` macro path for multi-page UI builds.
  It pulls in `__c6xabi_call_stub` and freezes on interaction in `T9NoAudio`.

## Open Questions

* What declares or toggles stock stereo routing for custom ZDLs?
* What exactly do `ctx[13]` and `ctx[14]` represent in stock modulation effects?
* What are the lifecycle rules for ctx state during bypass, preset switch,
  reload, and duplicate instances beyond the cases already tested?
* Which runtime helpers are safe enough to bundle, and which must be avoided?
* Can a compact reloc-free page 2/3 edit handler be written or cloned safely?

## How To Record New Findings

Keep this file short:

* Add only durable conclusions or a small table row for a new hardware result.
* Move detailed flash instructions into `src/hardware_probes/` or the relevant
  effect directory.
* If a conclusion changes, replace the old claim instead of appending a long
  contradiction.
