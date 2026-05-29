# BpmHunt — memory-inspector probe (v3, narrowed onto pre-table gap)

A diagnostic probe that turns the audio function into a memory
inspector. The single `Addr` knob (0..15) selects one of 16 firmware-RAM
words across a **512-byte window** starting at `0xc009c000`. The audio
function reads `*(0xc009c000 + 32 * knob_index)` directly and turns
the **sum of all four bytes** of the read value into a 0..2 audible
gain.

**v1 result (64-byte window at 4-byte step, low-byte gain):** no
tap-tempo correlation at any of 16 positions in `0xc009c1a0..0xc009c1dc`.
**v2 result (16 KB window at 1024-byte step, byte-sum gain):** still
no tap-tempo correlation across `0xc009c000..0xc009ffff`. User reported
positions 2 (`0xc009c800`), 8 (`0xc009e000`), 12 (`0xc009f000`), 13
(`0xc009f400`) read louder but none changed with BPM — confirming
the read mechanism works and that those positions land in code or
pointer-shaped data.

**The unscanned gap:** v2 sampled exactly one word at `0xc009c000`,
then jumped 1 KB past the state[31] per-slot table to `0xc009c400`.
That left `0xc009c004..0xc009c19c` (≈410 bytes immediately BEFORE the
state[31] table) untouched. Global firmware data tends to cluster
adjacent to related per-slot tables, so v3 focuses there.

No handler patches — LineSel handler does its normal job (read knob,
normalize, post to UI via state[7]), and the audio function does the
inspection. The `Addr` slot's descriptor carries `pedal_flags = 0x28`
to expose the **TAP UI** so we can drive BPM changes.

## Scan layout

| knob | address | region |
|---:|---|---|
| 0 | `0xc009c000` | start of unexplored gap |
| 1 | `0xc009c020` | |
| 2 | `0xc009c040` | |
| 3 | `0xc009c060` | |
| 4 | `0xc009c080` | |
| 5 | `0xc009c0a0` | |
| 6 | `0xc009c0c0` | |
| 7 | `0xc009c0e0` | |
| 8 | `0xc009c100` | |
| 9 | `0xc009c120` | |
| 10 | `0xc009c140` | |
| 11 | `0xc009c160` | |
| 12 | `0xc009c180` | last 32 B before the state[31] table |
| 13 | `0xc009c1a0` | state[31] table[0][0] (cross-check vs v1) |
| 14 | `0xc009c1c0` | state[31] table inside slot 0/1 |
| 15 | `0xc009c1e0` | state[31] table inside slot 1 |

## Audio interpretation

```c
unsigned int value = *(volatile unsigned int *)(0xc009c000 + 32 * idx);
unsigned int byte_sum =
    ((value >> 24) & 0xFF) +
    ((value >> 16) & 0xFF) +
    ((value >>  8) & 0xFF) +
    ( value        & 0xFF);
float gain = (float)byte_sum / 510.0f;  /* clamped 0..2 */
outBuf[i] += fxBuf[i] * gain;
```

So:
- `value == 0` → silent
- byte_sum 0..255 → quiet to unity (max for a single byte: 1.0)
- byte_sum 256..510 → unity to ~2× gain
- byte_sum > 510 → clamped at 2× gain (loud, near constant)
- Pointer-shaped values (like `0xc00bxxxx`) sum to `0xc0+0x0b+xx+xx`
  ≈ 200..450 → loud, near constant
- A word containing a 16-bit BPM (`0x00xxxxxx` or `0xxxxx0000`) where
  the BPM occupies the active bytes will modulate as BPM changes

## Flash and test plan

| step | action | expected if probe is alive |
|------|--------|----------------------------|
| 1 | Load `BpmHunt.ZDL`, browse to Filter, select it | shows up, opens without freeze |
| 2 | Unbypass with `Addr` at default (knob = 0) | some gain — should hear input passing through, possibly amplified or attenuated |
| 3 | Sweep `Addr` slowly from min to max | gain changes at different knob settings; note any settings where audio cuts out (`value & 0xFF == 0`) or saturates |
| 4 | Pick a knob setting that produced **non-trivial** audio (not all silent, not constant loud) | the read address holds some changing per-slot or global value |
| 5 | At that setting, tap the left-knob (tempo) at a **steady ~120 BPM** | listen for periodic gain pulses synchronized with the taps |
| 6 | If step 5 shows BPM-correlated changes, change to **~180 BPM** taps | the rate of gain change should follow |
| 7 | Try several knob settings — the one(s) where step 5/6 show clear BPM tracking reveal the BPM-storage address |

## Interpretation table

After sweeping, record what you observed at each knob setting (v3 scan
covers 512 B at 32-byte step starting from `0xc009c000`):

| knob | address | observed audio | tap-tempo response |
|---:|---|---|---|
| 0 | `0xc009c000` | | |
| 1 | `0xc009c020` | | |
| ... | ... | | |
| 15 | `0xc009c1e0` | | |

If **no setting** shows tap-tempo response, BPM is stored outside
both v1's 64-byte window AND this 512-byte gap — meaning it's not
clustered with the state[31] table at all. Next probe should pivot to
a completely different memory region (e.g., the per-slot handler state
at `0x11f03000..0x11f037ff`, globals near `0xc00fxxxx`, or scan the
DDR-ish region around `0xc00a0000..0xc00b0000`).

If **a setting tracks BPM**, that address is within a 32-byte block; a
follow-up probe narrows from 32-byte step to 4-byte step within that
block to identify the exact word.

## Risks

* **Direct firmware-RAM read from audio context**: the widened scan
  extends well past the state[31] table. Some pages (e.g., uncached
  regions or unmapped firmware addresses) may fault. If a particular
  knob position freezes the pedal, that page is unreadable from the
  audio context — skip it and try the next.
* **Reading code pages**: knobs 7..15 likely land in firmware code
  rather than data. The byte values there are instruction opcodes,
  which are constant across tap-tempo changes (instructions don't
  change), so those positions act as no-op slots from a BPM-finding
  standpoint.
* **Volatile read of a changing word**: tap tempo or other firmware
  threads may write to the read address concurrently. We use
  `volatile` to defeat optimizer caching, but tear-free reads of
  multi-word BPM values are not guaranteed.

If the probe loads and the knob sweep produces audible variation,
the inspection mechanism works. If any setting tracks tap tempo,
the BPM storage is found.

## How it was built

```
python3 src/hardware_probes/bpmhunt/build.py
```

Uses the **unpatched** `linesel_handlers.bin` (state[31] table
lookup, no state[24] patches). All the probe logic is in the audio
function.
