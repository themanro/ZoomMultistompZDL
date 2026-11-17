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

## Hardware results: StProbe (2026-11-01)

StProbe was a stereo and cable-loop probe (Delay 916; never published, removed
2026-11-09). It asked, for a feedback-loop effect (R Out patched back into
L In, the instrument on R In):

1. Does an effect get L In and R In separately?
2. Can it send different signals to L Out and R Out?
3. Is `ctx[4]` the raw input, before the chain, with the same L/R layout?
4. How does the cable loop behave: where it starts to ring, and at what pitch?

Its modes, and what each answer meant:

| Mode | Setup | What it means |
|---|---|---|
| **Tone** | No input. Listen to L Out, then R Out. | Low single beep on L, high double beep on R: the outputs are separate. Both beeps on one output: they are summed. |
| **InFx** | Guitar in L In, then in R In. Reads `ctx[5]`. | L: steady. R: chopped to silence 6 times a second, if R In is separate. Pulsing but never silent: one input is copied to both sides. Silent: R In doesn't reach the effect. |
| **InDry** | As InFx. Reads `ctx[4]`. Repeat with a stock delay in slot 1 and the probe in slot 2. | Same answers for `ctx[4]`. If the delay is absent here but present in InFx, `ctx[4]` is the raw input. |
| **Loop** | Cable R Out → L In, amp on L Out, guitar in R In. Gain and Inst at 0. | The guitar passes clean to L Out. Raise Gain slowly: note the value where it starts to ring, and the pitch (set by the round trip through the cable). Inst feeds the guitar into the loop. R Out is capped at -6 dBFS. |

Unplug the R Out → L In cable before leaving Loop: InFx and InDry send the
input to both outputs, so with the cable in place they howl at once.

Also try Tone with a stock mono effect after the probe: does the separation
survive it?

MS-70CDR, firmware 2.10, StProbe 0.01 alone in slot 1 (2026-11-01):

- **Tone, both outputs plugged:** L Out plays only the low single beep, R Out
  only the high double beep. The outputs are separate.
- **Tone, only L Out plugged:** L Out still plays only the single beep. The
  pedal does not sum R into L when one output is unplugged.

- **InFx, guitar in L In (R In plugged or empty):** steady on L Out.
- **InFx, guitar in R In, L In plugged (free cable):** chopped. R In reaches
  the effect separately from L In.
- **InFx, guitar in R In, L In empty:** silent. R In only works while L In is
  plugged, which the loop always is (R Out cabled to L In).

- **Loop, R Out cabled to L In, Inst 0:** starts to ring at Gain 41. In the
  code, Gain 41 gives 8 × 0.41² = 1.34, and R Out's small-signal gain is
  0.5 × 1.5 = 0.75, so the loop inside the effect is 1.34 × 0.75 = 1.0. The
  round trip out of R Out, through the cable and back into L In is therefore
  unity (0 dB): full scale out comes back as full scale in.

  It rings at one pitch, a whine: the round trip through the converters is
  short. A tuner reads A5 (880 Hz): a 1.14 ms (50-sample) period. The loop
  can also ring at a multiple of its basic frequency, so the round trip is
  about 50 samples or a whole fraction of that.
- **Loop, guitar in R In, Gain 0, Inst 0:** silent on L Out, although the
  code passes R In straight to L Out. At full volume the guitar is just
  audible, very faint, with or without R Out plugged.
- **InFx, guitar in R In, R Out unplugged, compared with L In:** R In is also
  very faint (a little louder than in Loop). The pedal itself delivers R In
  far below L In; the code is not the cause. Not yet ruled out: the cable or
  jack contact on R In.

InDry: not yet run (only matters for an effect placed after others).
