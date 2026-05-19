# Stereo Routing

Last updated: 2026-05-19

This file answers the recurring community question: *how does a ZDL declare
itself stereo so the pedal switches to stereo mode?* The static answer, from
the stock corpus and the ZDL header layout, is: **it doesn't. There is no
stereo flag in any ZDL.**

## 1. The header carries no stereo bit

The 76-byte `ZdlInfo` header was diffed across mono/stereo pairs:

| pair | only field that differs |
|---|---|
| `CHORUS.ZDL` vs `STCHO.ZDL`   | `sort_index` (browse order) |
| `DELAY.ZDL`  vs `STDELAY.ZDL` | `sort_index` (browse order) |

`real_type`, `knob_type`, `bass_flags`, `sort_fx_type`, `unknown1`, `unknown2`,
`fx_version`, and `version_string` are identical between mono and stereo
versions of the same effect family. Whatever distinguishes stereo is not in
the header.

## 2. The ELF carries no stereo flag either

Section layout for the same pairs is essentially identical: same sections,
same VAs, same alignments. The only structural deltas are size — extra DSP
code and extra const data — and the exported symbol names.

Examples:

* `CHORUS.ZDL` exports `Fx_MOD_Chorus*` symbols; `STCHO.ZDL` exports
  `Fx_MOD_StereoCho*` with the **same parameter set** (`depth`, `mix`,
  `outLv`, `rate`, `tone`). The `.audio` section is the same size in both
  (0x4a0 bytes). The stereo version is a separately compiled DSP that
  reads/writes both channel buffers; the host UI sees the same controls.
* `DELAY.ZDL` exports `Fx_DLY_Delay*` with `time/fb/mix/...`; `STDELAY.ZDL`
  exports `Fx_DLY_StereoDly*` with `timeL/timeR/timeLR/fbL/fbR/mixL/mixR`.
  Different parameter count, different `.audio` size. Stock "stereo Delay"
  is structurally a different effect, not a flag on the mono one.

## 3. Pedal hardware decides routing, not the ZDL

Same-named effect ZDLs are **byte-for-byte identical** across pedals with
different channel counts:

| MS-70CDR (stereo) | MS-50G (mono) | identical? |
|---|---|---|
| `CHORUS.ZDL`  | `MS-50G_CHORUS.ZDL`  | yes (12878 B) |
| `DELAY.ZDL`   | `MS-50G_DELAY.ZDL`   | yes (16557 B) |
| `AIR.ZDL`     | `MS-50G_AIR.ZDL`     | yes (13948 B) |
| `AUTOPAN.ZDL` | `MS-50G_AUTOPAN.ZDL` | yes (13834 B) |

The same compiled effect is shipped to both pedals. The pedal hardware /
firmware is what determines whether the right-channel buffer carries real
audio or is silent. Nothing in the ZDL adapts.

## 4. What this means for custom ZDLs

* Don't look for a stereo flag. There isn't one in the descriptor, the
  header, the SonicStomp entry, or the effectTypeImageInfo.
* If a custom effect wants to be stereo, write the audio function so it
  reads from and writes to **both** the L and R buffers. The
  `LLLLLLLL RRRRRRRR` 8-sample block layout (see
  [build/ABI.md §5.2](../build/ABI.md)) is the same on mono and stereo
  pedals.
* On a mono pedal the right-channel pointer may carry a silent or
  uninitialized buffer; the audio function still gets handed a buffer
  pointer and must not crash. Stock stereo effects ship to the MS-50G
  unchanged and are known to load there, so the firmware tolerates this.
* If a custom effect wants different parameters per channel (as in
  `STDELAY`'s `timeL`/`timeR`/`fbL`/`fbR`), that's just a parameter-count
  decision; it doesn't change the ABI.

## 5. Open questions deliberately not closed by this doc

* Whether the pedal exposes any *dynamic* stereo signal (e.g.
  `ctx[13]` / `ctx[14]`) to custom effects: still open, tracked in
  [docs/STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md). Stock modulation
  effects use those slots; their meaning for custom DSP is unresolved.
* Whether the firmware silently passes `ctx[5]_R` (right wet buffer) on
  mono pedals or actively masks it: would need a hardware probe writing
  a known constant into the R wet buffer and watching for crashes on a
  mono pedal.
* How preset routing (the global stereo/dual chain mode) affects which
  pedals are "in" the L vs R path: that is a pedal-level feature, not a
  ZDL-level one, and is out of scope here.

## 6. Where this finding lives in the toolchain

Nothing to change. The `build/linker.py` path already has no stereo bit
because stock builds don't have one either. A "make this effect stereo"
control would belong in the per-plugin `manifest.json` only as a hint to
the author about which buffers to touch in their C code, not as a flag
the host reads.
