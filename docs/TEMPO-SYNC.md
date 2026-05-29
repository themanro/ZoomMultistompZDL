# Tempo Sync

Last updated: 2026-05-30

This file documents how Zoom's stock delay effects receive the host tempo
(BPM) and convert it to delay-time samples. It is reconstructed by static
disassembly of `stock_zdls/TAPEECH3.ZDL`, which the corpus marks as the
cleanest reference implementation (one explicit sync-aware time handler
plus the `GetString_StompDelaySync` formatter). Older delays
(`DELAY`, `ANLGDLY`, `TAPEECHO`) follow the same pattern with different
naming.

## TL;DR — what we actually do for custom effects (2026-05-30)

After two rounds of blind-scan BpmHunt probes (v1..v4) found no
BPM-tracking firmware address near `0xc009c1a0`, the static trace
through TapeEcho3 reveals **why we never found it**: TapeEcho3 never
reads BPM directly. The host firmware owns BPM internally and exposes
only "compute a sync-adjusted delay value for this slot" via the
state callbacks.

**Practical rule:** stop hunting for a global BPM address. To add
sync to a custom effect, replicate TapeEcho3's `DLY_EP3_Calc_DelayTime`
call pattern exactly and use the value the host returns. Details
below in §4.

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

**BPM is not in this table** (and not anywhere a custom effect can
reach by direct read — see §4). The state[31] table is per-slot and
BPM is a host-internal global. `state[31]` is *not* a BPM source,
but with the `B4=0x0f3c` trick it does reach a **secondary firmware
table at `0xc009fe90`** that the host uses internally for tempo-sync
computation. See §4 for the full algorithm.

### Update: BpmHunt scan results (closed 2026-05-30)

A series of `src/hardware_probes/bpmhunt/` probes used the audio
function as a memory inspector to search for any firmware-RAM word
that varied with tap-tempo:

| ver | window | step | gain mapping | result |
|---:|---|---:|---|---|
| v1 | `0xc009c1a0..0xc009c1dc` | 4 B | low byte | no tap-tempo correlation |
| v2 | `0xc009c000..0xc009ffff` | 1024 B | byte-sum | no correlation; mechanism confirmed working |
| v3 | `0xc009c000..0xc009c1e0` (pre-state[31]-table gap) | 32 B | byte-sum | weak hit at `0xc009c080` (louder at BPM 75 and 250) |
| v4 | `0xc009c080..0xc009c08c` (4 words around v3 hit), per-byte isolation | byte index | per-byte | no clear single byte tracking BPM |

The v3 weak hit at `0xc009c080` does not survive v4's per-byte
narrowing, suggesting `0xc009c080` is a host scratch word that
indirectly correlates with BPM (perhaps a phase accumulator updated
by the tempo task) rather than the BPM value itself. **No probe
found a firmware-RAM address that holds BPM as a directly-readable
value.** This is consistent with the §4 finding: the host never
exposes BPM as a global to ZDL code — it folds tempo into
state[24]'s output and into the `0xc009fe90` secondary table.

Conclusion: stop looking for a BPM address. Use the algorithm in §4.

## 4. The TAPEECH3 sync algorithm (re-traced 2026-05-30)

After the v3 BpmHunt result confirmed BPM is **not** at any 32-byte
granularity in `0xc009c000..0xc009c1e0`, a re-trace of the actual
TapeEcho3 disassembly (`/tmp/zoom-zdl-dis/TapeEcho3.ZDL.asm` lines
553-704) shows the algorithm is more abstract than the earlier
pseudocode implied. The host firmware owns BPM; the effect just
asks the host for "the right delay value for my current SYNC setting".

### The handler call pattern (verbatim from the asm)

```c
int DLY_EP3_Calc_DelayTime(ctx) {
    // First state[31] call — read the SYNC knob value
    int sync_value = state[31](
        A4 = ctx[0],         // slot index
        B4 = 6               // command 6 = "read SYNC slot"
    );  // returns 0..15 (0 = OFF, 1..15 = division index)

    if (sync_value == 0) {
        // Free-time path — read raw TIME knob and add 10
        int raw = state[31](
            A4 = ctx[0],     // slot
            B4 = 4           // command 4 = "read TIME slot"
        );
        return raw + 10;
    } else {
        // Sync-on path — two consecutive host calls, weird arg shapes
        int byte = (ctx[0] - 1) & 0xff;
        int x = state[31](
            A4 = byte,
            B4 = 0x0f3c      // = 3900; reads 0xc009fe90 + 44*byte
        );                   // = secondary firmware table, NOT the per-slot table
        int y = state[24](
            A4 = x,
            B4 = 100         // explicit MVK B4,0x0064 in B-branch delay slot
        );
        return y / 100;      // integer divide via __divu
    }
}
```

### Why the second `state[31]` call uses `B4 = 0x0f3c`

`state[31]`'s body (5 instructions at `c00b820c`) computes:

```
target = 0xc009c1a0 + 44 * A4 + 4 * B4
```

With `B4 = 0x0f3c`, the term `4 * B4 = 0x3cf0`, so:

```
target = 0xc009c1a0 + 0x3cf0 + 44 * byte
       = 0xc009fe90 + 44 * byte
```

This is a **second 44-byte-stride table at `0xc009fe90`**, not the
per-slot table at `0xc009c1a0`. Same arithmetic, different base.
Each row appears to hold pre-computed sync-division constants that
state[24] turns into a delay value with the current BPM folded in.

### Why state[24] works as "BPM-aware math" here despite being float-shaped

Reading `c00d4b40` (state[24]'s template value) shows ATAN2-like
float math. With this call's `A4 = x` (some int from the secondary
table) and `B4 = 100` (the explicit `MVK B4,0x0064` in the
`B A1` delay slot at `0x7a8`), the function does its float math
on whatever the table value happens to be. The host-built table at
`0xc009fe90` evidently contains values pre-arranged so that
state[24]'s math + the `/100` post-step yields a tempo-tracked
delay in the effect's expected fixed-point format.

That's the key insight: **we don't have to understand the math —
we just have to call it the same way TapeEcho3 does**. The host
arranged the table and the function to be self-consistent.

### What time_edit does with the returned delay

```c
void time_edit(ctx) {
    ctx_local = ctx;                              // A10
    params    = ctx[1];                           // A11
    int v2    = ctx[2];                           // A12
    int delay = DLY_EP3_Calc_DelayTime(ctx);

    // Stash delay value at params + 0x1FC (= params[127])
    *((int *)(params + 0x158 + 164)) = delay;     // params[127] = delay

    int sync_mode = state[30](ctx);
    if (sync_mode == 0) goto L4;                  // no-sync path

    v2 += 0x230;                                  // A12 += 560

    if (state[30](ctx) != 4) {                    // sync_mode != "free time"
        // Re-check SYNC slot via state[31]; if non-zero, goto L4
        if (state[31](ctx[0], 6) != 0) goto L4;
    }

    // L3: tape-mute close, then store delay-derived value
    tapmuteClose(ctx);
    int delay_q13 = ((441 * params[127]) / 10) << 13;  // Q13 fixed-point
    params[(0x158/4) + 10] = delay_q13;                // params + 0x180
    ctx[18]                = delay_q13;
    // ... downstream uses ctx[18] in audio function
}
```

### Practical recipe for a custom sync-aware effect

In a TapeEcho4-style custom effect:

1. **Declare a SYNC knob slot** with `max=15`, `pedal_flags=0x28`.
   (Same as TapeEcho3's `SYNC` param, same as `SyncProbe v1` did.)
2. **In the SYNC knob's edit handler**, replicate `DLY_EP3_Calc_DelayTime`
   verbatim — the two `state[31]` calls plus the conditional
   `state[24]` call. Use the returned value as your delay-in-samples.
3. **Store the result** at a fixed `params[N]` slot. The audio function
   reads from there each block.

There is no BPM number passed to the effect at any point. The
host's tempo state is folded into the value returned by `state[24]`
in the sync-on path.

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
