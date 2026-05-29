# BpmHunt — memory-inspector probe (v4, per-byte isolation around the hit)

A diagnostic probe that turns the audio function into a memory
inspector. The single `Addr` knob (0..15) selects a (word, byte)
combination across 4 candidate words near `0xc009c080` and routes
ONE isolated byte into a 0..2 audible gain.

**v1 result (64-byte window at 4-byte step, low-byte gain):** no
tap-tempo correlation at any of 16 positions in `0xc009c1a0..0xc009c1dc`.
**v2 result (16 KB window at 1024-byte step, byte-sum gain):** no
correlation across `0xc009c000..0xc009ffff`, but positions 2, 8, 12,
13 read louder confirming the mechanism works.
**v3 result (512-byte window at 32-byte step covering the pre-table
gap):** **HIT** at knob 4 = `0xc009c080` — byte-sum varies with tap
tempo. At BPM 75 and BPM 250 the gain reads "a bit louder" than at
other BPMs. The non-monotonic 75/250 response suggests the byte-sum
is sensing a 32-bit value where different bytes dominate at different
BPM ranges (likely a period-in-samples or a fixed-point ratio).

v4 confirms-and-narrows: span 4 candidate words around the v3 hit
and isolate ONE BYTE per knob position to identify which exact
(word, byte) pair tracks BPM.

## Scan layout

Each knob position reads ONE WORD and isolates ONE BYTE of it:

| knob | word address | byte selected | bit range |
|---:|---|---:|---|
| 0 | `0xc009c080` | 0 | [ 7: 0] |
| 1 | `0xc009c080` | 1 | [15: 8] |
| 2 | `0xc009c080` | 2 | [23:16] |
| 3 | `0xc009c080` | 3 | [31:24] |
| 4 | `0xc009c084` | 0 | [ 7: 0] |
| 5 | `0xc009c084` | 1 | [15: 8] |
| 6 | `0xc009c084` | 2 | [23:16] |
| 7 | `0xc009c084` | 3 | [31:24] |
| 8 | `0xc009c088` | 0 | [ 7: 0] |
| 9 | `0xc009c088` | 1 | [15: 8] |
| 10 | `0xc009c088` | 2 | [23:16] |
| 11 | `0xc009c088` | 3 | [31:24] |
| 12 | `0xc009c08c` | 0 | [ 7: 0] |
| 13 | `0xc009c08c` | 1 | [15: 8] |
| 14 | `0xc009c08c` | 2 | [23:16] |
| 15 | `0xc009c08c` | 3 | [31:24] |

A byte that holds raw BPM (~40..240) will swing gain ~0.3..2.0
between BPM 75 and BPM 250 — much louder than v3's byte-sum
variation. A byte that holds a stable upper half of a pointer (e.g.,
`0xc0`) will be saturated-constant. A byte that's all-zero will be
silent.

## Audio interpretation

```c
int word_idx = idx >> 2;     /* 0..3 -> 0xc009c080, +4, +8, +c */
int byte_idx = idx & 3;      /* 0..3 -> low byte .. high byte */
unsigned int value = *(volatile unsigned int *)(0xc009c080 + word_idx * 4);
unsigned int byte = (value >> (byte_idx * 8)) & 0xFF;
float gain = (float)byte * (2.0f / 255.0f);
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

After sweeping, record what you observed at each knob setting (each
position isolates one byte of one of four candidate words):

| knob | (word, byte) | observed audio | tap-tempo response |
|---:|---|---|---|
| 0 | (0xc009c080, byte 0) | | |
| 1 | (0xc009c080, byte 1) | | |
| 2 | (0xc009c080, byte 2) | | |
| 3 | (0xc009c080, byte 3) | | |
| 4 | (0xc009c084, byte 0) | | |
| ... | ... | | |
| 15 | (0xc009c08c, byte 3) | | |

Recommended test cadence:
  1. Sweep through 0..15 at a baseline BPM (say 120). Note which
     positions are silent (byte = 0), constant-loud (byte ≈ 0xc0..0xff
     suggesting pointer high byte), or mid-range (byte 0x10..0x80,
     candidate BPM byte).
  2. At the candidate positions, tap BPM 75 → listen → tap BPM 250 →
     listen. The position whose gain SWINGS dramatically between the
     two BPMs is the BPM byte.
  3. If exactly one knob position has the swing, that single (word,
     byte) pair holds raw BPM. If two adjacent knob positions
     (i.e., two bytes of the same word) both swing, BPM is stored as
     a 16-bit value spanning those two bytes.
  4. Once identified, the BPM source address is fixed and we can
     design a sync handler that reads it directly without any of the
     state[31] indirection.

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
