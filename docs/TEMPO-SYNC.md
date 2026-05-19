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
| `state[24]` | firmware ptr (set by template writer at `c00c8ac0+`) | host BPM→samples helper. Called via `__c6xabi_call_stub` after `state[31]` says sync is on. |
| `state[30]` | `0xc00c3a70` | sync-division/mode query. Return value `4` means "free time" (no sync); other values are division indices. |

These join the previously documented callbacks:

| field | role |
|---:|---|
| `state[7]`  | tail-call target — final "write param into table" dispatch |
| `state[21]` | mid-stage knob callback used by edit handlers |
| `state[31]` | first/primary host callback (read-knob, multi-command — see §3) |
| `state[34]` (`state+136`) | coefficient-table setup callback |
| `state[35]` (`state+140`) | second setup callback |

## 3. `state[31]` is a multi-command query, not just "read knob"

This is the most important reframing. The
[EDIT-HANDLER-ABI.md](EDIT-HANDLER-ABI.md) LineSel knob1 shape uses
`state[31]` with `B4 = knob_id` (`B4=2` for knob 1, `B4=3` for knob 2)
and gets back a 0..255 raw knob value. TAPEECH3 calls `state[31]` with
**different `B4` values to query different host data**:

| `B4` | observed return | meaning |
|---:|---|---|
| 2 | 0..255 raw knob value | knob 1 (params[5]) — LineSel pattern |
| 3 | 0..255 raw knob value | knob 2 (params[6]) |
| 4 | free-time raw value | used by `DLY_EP3_Calc_DelayTime` when sync is off |
| 6 | boolean | "is the global sync flag on?" |
| 7+ | undocumented; see open questions | other queries used by `Fx_DLY_TapeEcho3_Booster_onf` etc. |

`B4` is a command selector. The callback at `state[31]`'s template
target `0xc00b820c` is the multi-command dispatcher in firmware. For
custom code, the practical rule is:

* For a plain knob, set `B4 = 2 + param_index_within_user_knobs` (matches
  the LineSel byte tables in `build/linker.py`).
* For a sync-mode query, set `B4 = 6` and check whether the return is
  non-zero before going down the BPM-sync path.

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

## 6. Descriptor side (open)

`disp_prm_StompDly_BPM_sync` is a firmware-side helper symbol
referenced by 192 stock effects (per
[STOCK-EFFECT-CORPUS.md §8](STOCK-EFFECT-CORPUS.md)). It is bound by
the SonicStomp descriptor entry for the sync-aware parameter slot.
The exact descriptor field that says "this slot uses BPM sync display"
has not been mapped yet — a comparison between the `time` descriptor
entry in `CHORUS.ZDL` (no sync) vs `DELAY.ZDL` (sync via
`GetString_1_5000_Sync`) vs `TAPEECH3.ZDL` (sync via
`GetString_StompDelaySync`) should isolate it. Not done in this pass.

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

Those edits are deferred until we either (a) hardware-confirm the
TAPEECH3 reading or (b) finish the descriptor-side diff in §6, so the
canonical docs don't grow stale guesses.
