# Tempo Sync

Last updated: 2026-05-19

This file documents how Zoom's stock delay effects receive the host tempo
(BPM) and convert it to delay-time samples. It is reconstructed by static
disassembly of `stock_zdls/TAPEECH3.ZDL`, which the corpus marks as the
cleanest reference implementation (one explicit sync-aware time handler
plus the `GetString_StompDelaySync` formatter). Older delays
(`DELAY`, `ANLGDLY`, `TAPEECHO`) follow the same pattern with different
naming.

This is a static doc. Hardware verification is the obvious next step; see
"Open" at the bottom for what still needs a probe.

## 1. Reference effect: TAPEECH3

```
$ python3 build/analyze_stock_corpus.py    # produces build/stock_corpus.csv

TAPEECH3:
  family=Delay  n_edit=5
  edit  = time, fb, mix, RecLvl, PreAmp
  getstr= GetString_StompDelaySync, GetString_offset_10
  audio = Fx_DLY_TapeEcho3
  helpers includes disp_prm_StompDly_BPM_sync
```

The interesting handler is `Fx_DLY_TapeEcho3_time_edit`. It is the only
edit handler in the corpus that is wired to BPM sync end-to-end (older
sync-aware effects use the same callback set, just with shorter
implementations).

## 2. Per-slot state fields newly observed

Two state fields beyond the ones in
[docs/EDIT-HANDLER-ABI.md](EDIT-HANDLER-ABI.md) are required for a
tempo-aware effect:

| field | template value | role observed in TAPEECH3 |
|---:|---|---|
| `state[24]` | `c00d4b40` (set by template writer at `c00c8ac0+`) | **CORRECTION (2026-05-29):** previously assumed to be the host BPM→samples helper based on TAPEECH3's `DLY_EP3_Calc_DelayTime` call shape. Reading `c00d4b40` directly shows it is a **float-math utility** — opens with `ABSSP A4,A7; ABSSP B4,B7; CMPEQSP A4,0,A1; CMPEQSP B4,0,B1; OR B1,A1,B2; [B2] B B3` (early-returns if either arg is float 0.0), then runs `RCPSP`/`MPYSP` polynomial math with constants `0x3FC90000` (~π/2) and `0x3F860000`. This looks like ATAN2 / RCPSP-based division or a trig approximation — not BPM. SyncProbe v1/v1.5/v2 results are all consistent with this: A4 = state[0] = 0.0 triggered the early-exit and state[7] received zero. The actual BPM mechanism is somewhere else in the per-slot state or firmware globals; this table will be updated when it is located. |
| `state[30]` | `0xc00c3a70` | sync-division/mode query. Return value `4` means "free time" (no sync); other values are division indices. |

These join the previously documented callbacks:

| field | role |
|---:|---|
| `state[7]`  | tail-call target — final "write param into table" dispatch |
| `state[21]` | mid-stage knob callback used by edit handlers |
| `state[31]` | first/primary host callback (read-knob, multi-command — see §3) |
| `state[34]` (`state+136`) | coefficient-table setup callback |
| `state[35]` (`state+140`) | second setup callback |

## 3. `state[31]` is a per-slot table lookup

**Reading the firmware target `c00b820c` directly (2026-05-29)** shows
`state[31]` is a much simpler primitive than the earlier "multi-command
dispatcher" framing suggested. Its full body is 5 instructions plus
return:

```
c00b820c: MV.L1X B4, A0              ; A0 = command code
c00b820e: MVK.S1 44, A1               ; per-slot stride (= 11 entries × 4 bytes)
c00b8210: MPY32 A1, A4, A1            ; A1 = 44 * slot_idx (A4 = slot index input)
c00b8214: MVK.S2 0xffffc1a0, B0
c00b8218: MVKH.S2 0xc0090000, B0      ; B0 = 0xc009c1a0 (table base, in RAM)
c00b8220: SHL.S1 A0, 0x2, A0           ; A0 = command code * 4
c00b8222: ADD.L1 A1, A0, A0            ; offset = 44*slot + 4*command
c00b8224: ADD.L1X A0, B0, A4           ; address = base + offset
c00b8226: LDW.D1T1 *A4[0], A4          ; A4 = *(table entry)
c00b8228: BNOP.S2 B3, 5                ; return
```

So `state[31](slot, B4)` returns the 32-bit word at
`0xc009c1a0 + 44*slot + 4*B4`. It's a fixed-shape per-slot table read
— not a dispatcher with command-specific code paths.

What the table holds, mapped from cross-references in stock effects:

| `B4` | observed semantic | how mapped |
|---:|---|---|
| 0  | pointer; `[!B0] BNOP` null-check pattern at `c00d2110` | likely the slot's audio function or per-slot context ptr |
| 2  | 0..255 raw value (read by LineSel knob1 handler) | knob 1 / `params[5]` source |
| 3  | 0..255 raw value (read by LineSel knob2 handler) | knob 2 / `params[6]` source |
| 4  | unknown — TAPEECH3 calls with B4=4 ("get raw time") in `DLY_EP3_Calc_DelayTime` free-time path | candidate for sync-division index |
| 6  | unknown — TAPEECH3 calls with B4=6 to choose sync-on/off branch | candidate for sync-mode flag |
| 9  | used in DSP setup (read at `c00d217e`, fed to `c00ddda0` = state[34]) | possibly a setup pointer or size |
| other | unmapped | reads return whatever's at the table position |

Out-of-range `B4` (> 10) reads past the slot's 44-byte row — TAPEECH3
appears to call with `B4 = 0x0f3c` in `DLY_EP3_Calc_DelayTime`'s
sync path, which would read at offset 15600 bytes from the slot
base; that suggests either (a) the value was meant for the
following `state[24]` call not `state[31]`, or (b) the table extends
further than the per-slot 44 bytes for special-purpose entries. Not
yet resolved.

For custom code, the practical rule is:
* `B4 = 2 + param_index_within_user_knobs` reads the current raw value
  of knob N — matches the LineSel byte tables in `build/linker.py`.
* Other `B4` values may return useful per-slot data, but each one
  needs verification before use.

**BPM is not in this table.** The table is per-slot and the BPM is a
global. So `state[31]` is *not* a BPM source — finding BPM needs a
different lead. Likely candidates: a separate firmware global near
`0xc009xxxx`, a slot of `ctx[N]` we haven't decoded, or a stored value
that the tap-tempo handler updates. Tracked as the open question
this file was previously chasing.

## 4. The TAPEECH3 sync algorithm

`Fx_DLY_TapeEcho3_time_edit` and its helper `DLY_EP3_Calc_DelayTime`
together implement:

```c
void time_edit(state) {
    int raw_param   = state[2];                  // per-slot host value
    int delay_calc  = DLY_EP3_Calc_DelayTime();  // see below
    int division    = state[30](state[0]);       // 4 == free; else index
    int param_base  = state[1] + 0x158;          // params + 344 (offset table)

    if (division == 4) {
        // Free time: state[31] with B4=2 reads raw knob
        delay_samples = compute_from_raw_knob();
    } else {
        // Sync mode: compute (sample_rate * 60 / BPM) * division / 10
        // 185 << 8 = 0xb900 is the sample-rate constant for 44.1kHz
        int bpm        = state[24](command=10);
        int delay      = (47360 * bpm) / 10;
        int q13_delay  = delay << 13;            // Q13 fixed-point conversion
        params[10] = q13_delay;
        params[18] = q13_delay;
    }
}

int DLY_EP3_Calc_DelayTime(state) {
    int sync_on = state[31](command=6);          // is sync on?
    if (sync_on) {
        int raw = state[31](command=4);          // get raw time index
        return state[24](command=raw + 1);       // BPM→samples for this division
    } else {
        return state[31](command=4) + 10;        // free-time path with offset
    }
}
```

Two locations in the params table receive the computed delay:
`params[10]` and `params[18]`. The audio function (`Fx_DLY_TapeEcho3`)
reads those, not the raw knob value.

## 5. `GetString_StompDelaySync` — the division label formatter

This is the UI value-to-string callback bound to the sync-aware param
slot. Its body is short:

```
A4 = index passed by host (0..N)
A1 = 0x80000000 + 0xcd8           ; base of .const string table
A0 = A4 * 2 (ADDAW.D1)            ; 2 bytes per entry? No — 5-byte stride below
load string starting at A1 + A0   ; SPLOOP byte-copy until NUL
```

The 5-byte-stride string table in `.const + 0xcd8` for TAPEECH3
decodes as:

| index | bytes | display |
|---:|---|---|
| 0 | `20 4F 46 46 00`              | " OFF" (sync disabled) |
| 1 | `17 00 00 00 00`              | sixteenth note |
| 2 | `19 20 33 00 00`              | "♪ 3" (eighth triplet) |
| 3 | `17 2E 00 00 00`              | "♬." (dotted sixteenth) |
| 4 | `18 00 00 00 00`              | eighth note |
| 5 | `1A 20 33 00 00`              | quarter triplet |
| 6 | `18 2E 00 00 00`              | dotted eighth |
| 7 | `19 00 00 00 00`              | half note |
| 8 | `19 2E 00 00 00`              | dotted half |
| 9 | `19 78 32 00 00`              | "♪x2" |
| 10 | `19 78 33 00 00`             | "♪x3" |
| 11 | `19 78 34 00 00`             | "♪x4" |
| … | further entries follow             | |

Bytes `0x17..0x1A` are the pedal's font-internal music symbols, not
ASCII. The pattern is `<note-symbol> [<modifier>]` packed into a
fixed-width 5-byte slot.

## 6. Descriptor side — the sync flag is `pedal_flags & 0x28`

Confirmed by reading SonicStomp descriptor entries directly. The relevant
field is `pedal_flags` at byte `+0x2C` of the 48-byte entry (see
[build/ABI.md §3](../build/ABI.md)). The corpus shows two distinct
sync-aware descriptor patterns:

**Pattern A — "Time-with-sync" (legacy):** one parameter slot holds both
the free-time and sync ranges. The same `Time` knob's interpretation
toggles between ms and division based on the user's sync-mode setting
(stored elsewhere, e.g. global pedal mode or a second descriptor entry).
Observed in `DELAY.ZDL`, `ANLGDLY.ZDL`, `TAPEECHO.ZDL`:

| effect | name  | max  | rsv_a (+0x18) | flags | getstr             |
|---|---|---:|---:|---:|---|
| `DELAY`    | `Time` | 4022 | 3999 | `0x0028` | `GetString_1_5000_Sync` |
| `ANLGDLY`  | `Time` | 4022 | 3999 | `0x0028` | (same)                  |
| `TAPEECHO` | `Time` | 2014 | 1999 | `0x0038` | `GetString_1_2000_Sync` |

`rsv_a` (the "reserved" word at +0x18 noted as
"non-zero in some delay/pitch effects" in `build/ABI.md`) carries the
upper bound for the sync-mode value range. `max` is the free-time max
and `rsv_a + 1` is the sync-mode max. `0x0038` = `0x28 | 0x10`
adds pedal/expression assignability on top of sync.

**Pattern B — "Separate SYNC slot" (modern):** the `Time` slot stays a
regular free-time knob (`flags=0x00`, no GetString), and a *separate*
parameter slot named `Sync` or `SYNC` carries `flags=0x0028` with
`max=15` and a sync-division GetString:

| effect | name  | max | rsv_a | flags | getstr                          |
|---|---|---:|---:|---:|---|
| `TAPEECH3` | `TIME` | 990 | 0 | `0x0000` | (free time, no formatter) |
| `TAPEECH3` | `SYNC` |  15 | 0 | `0x0028` | `GetString_StompDelaySync` |
| `STOMPDLY` | `Time` | 599 | 0 | `0x0000` | (free time)               |
| `STOMPDLY` | `Sync` |  15 | 0 | `0x0028` | `GetString_StompDelaySync` |

Pattern B is cleaner and matches the `state[31]` multi-command path in
§3-§4: the time_edit handler reads the SYNC slot via `state[31]` with
`B4=6` and switches modes accordingly. This is the pattern a custom
plugin should follow.

**Non-sync sanity check:** `CHORUS.ZDL` has no slot with `flags & 0x28`
anywhere in its descriptor — confirming the bit is not just universally
set.

So the SDK rule is: **set `pedal_flags = 0x28` on the descriptor entry
for the sync-mode parameter (Pattern B) — `max = 15`, `GetString` →
a sync-division formatter, and the time_edit handler reads sync state
via `state[31]` with `B4=6`.** The linker's existing handling of
`pedal_flags` already supports this; no schema change is needed in
`build/linker.py`.

## 7. Tap tempo (open)

Hardware tap tempo on the MS-series pedals updates a global BPM. The
stock effects observe the change automatically because they read BPM
via `state[24]` on each `_edit` call (and likely on each audio block —
to confirm via audio-function disassembly).

For a custom effect, this means: **no tap-tempo registration is
needed.** A sync-aware effect just queries `state[24]` and `state[30]`
the same way TAPEECH3 does, and BPM changes propagate through those
queries. The tap button itself is not visible to ZDLs.

What is still unresolved:

* Whether the audio function (`Fx_DLY_TapeEcho3`) also queries
  `state[24]` per block, or only relies on the values cached by
  `_edit` into `params[10]/[18]`.
* Whether there is a host-side notification for "BPM changed" that
  re-triggers edit handlers, or whether the audio function must
  re-derive on every block.
* What the firmware does when the user activates sync mode while
  TAPEECH3 is loaded — does it re-call the `_edit` handler?

## 8. SDK shape for a custom sync-aware effect

Putting the pieces together, a custom tempo-aware delay can be written
by following the TAPEECH3 pattern:

1. Allocate a parameter slot for the sync division (typically the same
   slot that displays time). Wire its `GetString` field to a custom
   formatter that produces division labels (the
   `0x80000000 + 0xcd8`-style table can be embedded in the plugin's
   `.const` rather than imported from firmware).
2. In the `time_edit` handler, call `state[31]` with `B4=6` to check
   sync mode. If non-zero, call `state[24]` to convert the chosen
   division to a delay-sample count and store in two
   params-table slots (`params[N]` and `params[N+2]` per the TAPEECH3
   convention).
3. Audio function reads the cached delay samples from `params[N]`.
4. Optionally bind `disp_prm_StompDly_BPM_sync` as the host-side display
   helper, once we have mapped the descriptor field that points to it.

This is the static plan; the build/linker side does not yet know how to
emit a sync-aware descriptor entry. That work is gated on the
descriptor diff in §6.

## 9. Where this changes other docs

* [docs/STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md) §"Init And
  Edit-Handler ABI Status": `state[24]` and `state[30]` need rows in
  the per-slot state map; `state[31]` should be re-tagged as a
  multi-command callback with `B4` as selector rather than the
  too-narrow "read knob value".
* [docs/EDIT-HANDLER-ABI.md](EDIT-HANDLER-ABI.md) §1 contract: the `B4`
  selector convention should be added; the LineSel knob1 shape becomes
  one specialization (`B4 = knob_id` constant) rather than the full
  story.
* [build/ABI.md](../build/ABI.md) §5.1: same.

The descriptor-side diff in §6 is now done; the canonical doc updates
above are partially applied (state[24]/state[30] rows added in
STATE-ABI-PROGRESS, `reserved_a` and `0x28` flag annotated in
build/ABI.md). Further edits to EDIT-HANDLER-ABI for the `B4` selector
remain deferred until we hardware-confirm the TAPEECH3 reading.
