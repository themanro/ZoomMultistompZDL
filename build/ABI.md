# ZDL runtime ABI — what a Zoom DSP plugin looks like, end to end

Written before our first ground-up port, so we go in with eyes open
instead of mystery-flashing. Everything below is derived from:

* `ofd_zdl.txt` — `ofd6x` dump of `MS-70CDR_EXCITER.ZDL` (full symbol
  table, relocation table, section addresses).
* `stock_zdls/MS-70CDR_EXCITER.ZDL` — the on-disk bytes, cross-checked
  against the relocations.
* `stock_zdls/MS-70CDR_LINESEL.ZDL` — second data point.
* **The v1 project at `~/coding/airwindowsZoom/`** — most importantly
  `ZDL_Findings.md` (descriptor format reverse-engineered from all 128
  stock MS-70CDR ZDLs) and `build/link_zdl.py` (the linker that
  successfully produced `TOTAPE9_AUDIO.ZDL`, which booted on real
  hardware and was selectable in the FX menu). Every load-bearing
  constant in v1's linker has a comment explaining the experiment that
  pinned it down. Items below marked **[v1-empirical]** were verified
  on hardware via bisection — change them only with proof.
* `zoom-fx-modding-ref/library/CH_2.md` — conversational disassembly
  of LineSel; provides DSP-loop semantics.
* `ZoomPedalFun-main/MS70CDR/DerivedData/2.10/checkme.py` — independent
  RE of the SonicStomp entry layout (corroborates `OnOffblockSize=0x30`).

Items still inferred (not directly observed) are flagged **[ASSUMPTION]**.

---

## 1. Toolchain target

| Property         | Value                                  |
|------------------|----------------------------------------|
| ISA              | TI C6740 (`Tag_ISA = 8`)               |
| ABI              | EABI (`Tag_ABI = 2`)                   |
| Endianness       | Little                                 |
| `long` width     | 32-bit (`Tag_Long_Precision_Bits = 2`) |
| `wchar_t`        | 16-bit                                 |
| Output           | ELF32 shared object (`ET_DYN`)         |
| Compiler         | TI C6000 v8.3.x (CCS 8.x)              |
| Linker version   | 7.3.7 (per the factory ELFs)           |

The ELF program headers Exciter ships with:

```
PH0  PT_LOAD  vaddr 0x00000000  filesz 0x6c0  flags r-x   (.text + .audio)
PH1  PT_LOAD  vaddr 0x80000000  filesz 0x458  flags r--   (.const)
PH2  PT_LOAD  vaddr 0x80000458  filesz 0x18   flags rw-   (.fardata)
PH3  PT_DYNAMIC                 size  0xa8                 (.dynamic)
```

A linker command file targeting this layout is the foundation of any
ground-up port. `.text` and `.const` are read-only at runtime; only
`.fardata` is writable, and it's used for *static* knob bitmaps and the
like — not for per-instance state.

---

## 2. The exported symbol contract

The host firmware finds your DLL's entrypoints **by name** in the
`.dynsym` table. A complete plugin must export exactly:

| Symbol                          | Kind     | Where it lives | Purpose                                         |
|---------------------------------|----------|----------------|-------------------------------------------------|
| `Dll_<Name>`                    | function | `.text`        | DLL load entry. Returns/registers the structs.  |
| `Fx_FLT_<Name>`                 | function | `.text`        | Per-buffer audio loop (the DSP).                |
| `Fx_FLT_<Name>_init`            | function | `.text`        | One-shot per-instance init.                     |
| `Fx_FLT_<Name>_onf`             | function | `.text`        | On/Off (bypass) handler.                        |
| `Fx_FLT_<Name>_<param>_edit`    | function | `.text`        | One per knob; runs when the user turns it.      |
| `picEffectType_<Name>`          | object   | `.const`       | RLE-compressed 128×40 1-bpp picture.            |
| `effectTypeImageInfo`           | object   | `.const`       | UI layout (image dims + per-knob xy + bitmap).  |
| `_infoEffectTypeKnob_A_2`       | object   | `.fardata`     | Knob bitmap descriptor (24 bytes; can share).   |
| `SonicStomp`                    | object   | `.const`       | **The** plugin descriptor — table of pointers.  |
| `_Fx_FLT_<Name>_Coe`            | object   | `.const`       | Coefficient/lookup table (effect-specific).     |

`<Name>` is a free identifier (PascalCase per Zoom's convention).
`<param>` is the lowerCamelCase knob name (e.g. `loContour`, `outlv`).

---

## 3. `SonicStomp` — the plugin descriptor

This is the centerpiece. It's a variable-length array in `.const` of
48-byte (`0x30`) entries. The **last entry is marked by setting bit
`0x04` in `pedal_flags` at offset `+0x2C`** — the firmware walks entries
from the table start until it sees this sentinel. There is **no fixed
entry count**: stock LO-FI Dly has 11 entries (OnOff + name + 9 knobs);
HELLO/LineSel have 4. [v1-empirical, verified across all 128 stock ZDLs]

```
SonicStomp ::=  [OnOff entry]
                [<Name> entry]
                [knob entry]+        // one per parameter
                                     // last knob has pedal_flags & 0x04
```

Each entry layout (corrected against v1's full survey of 128 ZDLs):

```c
struct SonicStompEntry {                  // 48 bytes
    char     name[12];        // +0x00  visible label, NUL-padded
                              //        up to 12 chars on the name entry,
                              //        up to 8 on parameter entries
    uint32_t max_val;         // +0x0C  max integer value
                              //        (0xFFFFFFFF on the name entry)
    uint32_t default_val;     // +0x10  default integer
    uint32_t pedal_max;       // +0x14  same as max_val if pedal-assignable,
                              //        0 otherwise
    uint32_t reserved_a;      // +0x18  sync-mode upper bound (used by
                              //        Pattern A "time-with-sync" delays).
                              //        On free-time entries it is 0.
                              //        For `DELAY`/`ANLGDLY` it is 3999
                              //        (vs `max=4022` for free time); for
                              //        `TAPEECHO` it is 1999 (vs 2014).
                              //        See docs/TEMPO-SYNC.md §6.
    uint32_t func_ptr;        // +0x1C  PRIMARY handler (relocated, ABS32):
                              //          OnOff entry  → onf
                              //          name entry   → init
                              //          knob entry   → <param>_edit
    uint32_t audio_ptr;       // +0x20  audio loop ptr (relocated, ABS32);
                              //        non-zero ONLY on the name entry,
                              //        0 elsewhere
    uint32_t getstr_ptr;      // +0x24  optional value-to-string formatter
                              //        for display; 0 if none
    uint32_t reserved_b;      // +0x28  usually 0
    uint32_t pedal_flags;     // +0x2C  bitmask, see below
};
```

`pedal_flags` bitmask (`+0x2C`) — cross-checked against the stock corpus:

| Mask   | Bit | Meaning                                                   |
|--------|-----|-----------------------------------------------------------|
| `0x04` | 2   | **End-of-table sentinel** — last parameter entry          |
| `0x10` | 4   | Pedal/expression-assignable parameter marker              |
| `0x28` | 3+5 | Tempo-synced (both bits required) — see [docs/TEMPO-SYNC.md](../docs/TEMPO-SYNC.md) for the two stock patterns (legacy "time-with-sync" using `reserved_a` vs modern "separate SYNC slot") |

Common observed values: `0x00` (regular knob), `0x04` (last-param,
not pedal), `0x10` (pedal/expression assignable), `0x14` (last +
pedal/expression assignable), `0x28` (tempo), `0x38` (tempo +
pedal/expression assignable).

Important correction: `0x10` is not the missing "make this effect stereo"
switch. It appears on mono stock effects such as `CHORUS`, `DELAY`, and many
amp/drive parameters whenever those parameters are expression-assignable, and
stock effects with explicit Mono/Stereo mode parameters use ordinary descriptor
entries plus `GetString_MonoStereo` display helpers. Treat stereo routing as
still unmapped.

**The Exciter values, observed on disk** (note +0x14 = 1 on the name
entry — that's `pedal_max = 1`, common but its purpose is unclear):

| Entry   | name     | max        | default | pedal_max | func_ptr        | audio_ptr | flags   |
|---------|----------|------------|---------|-----------|-----------------|-----------|---------|
| 0       | "OnOff"  | 1          | 0       | 0         | onf             | 0         | 0x00    |
| 1       | "Exciter"| 0xFFFFFFFF | 0       | 1         | init            | audio     | 0x00    |
| 2       | "Bass"   | 100        | 0       | 0         | loContour_edit  | 0         | 0x00    |
| 3       | "Trebl"  | 100        | 0       | 0         | process_edit    | 0         | 0x00    |
| 4       | "Level"  | 150        | 100     | 150       | outlv_edit      | 0         | 0x14    |

**On disk, `func_ptr` and `audio_ptr` are zero** — the dynamic linker
resolves them at load time from `.rela.dyn` ABS32 entries. Same for the
descriptor symbol's address everywhere it's referenced (e.g. inside
`Dll_<Name>`).

---

### 3.1 Pagination — how >3 knobs work [hardware-confirmed]

`effectTypeImageInfo` carries the total user-parameter count, but only
the first three coordinate blocks are meaningful. The firmware paginates
edit mode by walking the descriptor table from the name entry until the
`pedal_flags & 0x04` sentinel, overlaying entries onto 3 fixed visible
slots three-at-a-time.

The earlier "nknobs=9 breaks paging" reading was incomplete: the real
bug was the DLL entry stub still declaring NoiseGate's descriptor count
of `4`. Once `Dll_<Name>` declares `2 + len(params)`, hardware renders
and edits 3, 5, 7, and 9 parameter builds.

## 4. `effectTypeImageInfo` — UI layout (212 bytes)

A second `.const` struct, parallel to SonicStomp, that tells the
firmware where to *paint* things:

```
offset  type    field
------  ------  -----
0x00    u32     0
0x04    u32     1
0x08    u32     0
0x0C    u32     image width   (= 128)
0x10    u32     image height  (= 64)
0x14    u32*    picEffectType_<Name>  ← ABS32 reloc
0x18    u32     unknown (0x1C or 0x20)
0x1C    u32     unknown (0x18 or 0x19)
0x20    u32     user parameter count (3 for single-page, 4-9 for paginated)
  -- per-knob block (16 bytes), first 3 visible slots populated --
  +0    u32     knob_id  (1-based parameter index;
                          1 = OnOff, 2 = first knob, …)
  +4    u32     x  (top-left, in pixels)
  +8    u32     y
  +12   u32*    -> _infoEffectTypeKnob_A_2  ← ABS32 reloc
  ...
  zero-padded to exactly 212 bytes total       [v1-empirical]
```

Notes:
* **The struct must be padded to exactly 212 bytes.** Smaller breaks
  paging on hardware. With 3 knob entries the populated portion is
  `0x20 + 4 + 3·16 = 84` bytes; the rest is zeros. [v1-empirical]
* The picture is *not* embedded — it's a pointer to a separate `.const`
  blob. Editing artwork = editing `picEffectType_<Name>` and leaving
  this struct alone.
* The knob_id is what gets passed to the runtime "get knob value"
  callback (see §5).
* All three knobs in Exciter point to the *same* `_infoEffectTypeKnob_A_2`
  bitmap — knob shapes are shared.

`_infoEffectTypeKnob_A_2` is a 24-byte struct in `.fardata`,
**always exactly `{20, 15, 11, 0, 2, 0}` as six little-endian u32s**.
[v1-empirical: field4=5 was tried once and froze the unit at FX-select
time; field4=2 ships in all 128 stock ZDLs.]

---

## 5. The C6000 calling convention (what the firmware passes)

TI C6000 EABI scalar conventions:

| Register | Role                                      |
|----------|-------------------------------------------|
| `A4`     | arg 0 / return value                      |
| `B4`     | arg 1                                     |
| `A6`     | arg 2                                     |
| `B6`     | arg 3                                     |
| `B3`     | return address                            |
| `B15`    | stack pointer                             |
| `A14`/`B14` | preserved (data-page pointer)         |

All handlers seen in CH_2.md follow stock CCS prologue/epilogue
(save B3, allocate stack via B15, restore on exit).

### 5.1 Knob/OnOff `_edit` and `_onf` handlers

From CH_2.md's walkthrough of LineSel, all "edit" handlers and `onf`
share a shape:

```c
void Fx_FLT_<Name>_<param>_edit(SonicStompState *state, /* B4: arg1 */ ...);
```

Inside the handler: it reads `A6[?]` (= some host-state pointer), then
calls a host callback (also reached via that pointer) to fetch the
current integer knob value. The callback's **2nd argument is the
knob_id** — this is why CH_2.md notes "Efx gives 2, and Out gives 3":
each handler hardcodes the knob ID it represents, matching what
`effectTypeImageInfo` advertised.

The handler converts the integer (0..max) into a normalized float
0..1.0 and stores it at a fixed location for the audio loop to read.
LineSel uses a bias of `1.0 / max` (= `1.0/150` for OUT_L); Exciter
uses similar normalizers in its `_Coe` table.

**[ASSUMPTION]** The SonicStomp itself (or a parallel runtime state)
is what's passed in `A6`. CH_2.md describes the indirection generically
("a function pointer at offset 31 words in the 1st argument struct")
without naming the struct — but since SonicStomp is the only struct
the DLL exports that has a callback-shaped offset like this, that's
the most likely candidate. Verify on first ground-up build.

### 5.2 The audio loop `Fx_FLT_<Name>`

```c
void Fx_FLT_<Name>(BufferState *bs);
```

Where `BufferState` (called via `A4`/`A6`) holds:

* `Effect L` / `Effect R` buffer pointers — wet path.
* `Guitar L` / `Guitar R` buffer pointers — dry path.
* `Output L` / `Output R` buffer pointers — accumulator.
* Block size (sample count per channel).

CH_2.md's LineSel walkthrough confirms the buffer-pointer-loading idiom
(`A7[0]..A7[7]` → `A6[0]`) and the stride. **Three** logical buffer
pairs (Effect/Guitar/Output) — this is the LineSel "trick book" that
gives effects access to both the wet and dry signal independently.

#### Sample format

* **`float32`, IEEE-754, mono per channel.** CH_2.md is explicit:
  binary `01111111 << 23 = 0x3F800000` is "1.0 in float", and the
  effect coefficients (`__k0`..`__k6`) are floats throughout.
* Channel layout in memory: blocks of 8 samples per channel, then
  the other channel — `LLLLLLLL RRRRRRRR LLLLLLLL RRRRRRRR ...`.
  This is what enables the compiler to unroll the inner loop 8× cleanly.

#### Output is *added*, not assigned

Critical: the audio loop **adds** its contribution to `Output`, never
overwrites. Downstream effects in the chain still receive the input
signal independently — that's why a reverb's tail survives a switch-off
later in the chain. From CH_2.md:

> "Notice addition to output buffer, not overriding. To me this seems
> to be made to preserve trails of any effects that have them."

So a clean pass-through is `Output += Effect` (when on) or `Output += 0`
(when off). A new effect like gain is `Output += Effect * gain`.

#### Sample rate

* **44.1 kHz**, 24-bit codec (MS-70CDR datasheet). DSP samples are
  float32 internally regardless of codec width.

#### Block size

* **[ASSUMPTION]** A multiple of 8 samples per channel; exact value
  not directly observed. Typical embedded DSP block sizes are 32 or
  64 samples. We'll instrument this on the first ground-up plugin
  by writing a known-period sine-from-counter and measuring how it
  steps across calls.

### 5.3.a Buffer-state struct field map [v1-confirmed]

From `zoom-fx-modding-ref/diy/{rainsel,rtfm,div0}.asm` (three independent
from-scratch effects, all with consistent A4-field accesses):

```
A4 = ctx (state pointer; arg 0)
  ctx[1]   →  parameters table (an array of float values, see §5.4)
  ctx[4]   →  Dry buffer    (float*, raw guitar-input signal)
  ctx[5]   →  Fx  buffer    (float*, signal modified by upstream chain)
  ctx[6]   →  Output buffer (float*, accumulator — ADD into this)
  ctx[11]  →  "magic dest"  (must shuttle bytes from ctx[12])
  ctx[12]  →  "magic src"
```

`ctx[11]` / `ctx[12]` are a side-channel the original effects all
read-and-rewrite once per inner-loop iteration. Skipping the shuttle
may break downstream effects; the safe pattern is to copy verbatim. The
purpose is only partially mapped.

New ZD2 clue: a hand-decoded `Fx_SFX_LineSel` uses a field described as a
pointer to a "current sample" store and writes the current input sample there
inside the sample loop. The ZDL `Fx_FLT_LineSel` disassembly performs the same
kind of per-sample copy through the `ctx[11]`/`ctx[12]` path. Treat these fields
as a current-sample/magic-shuttle path required by host callbacks until a
hardware probe proves a narrower contract.

#### ZD2 ↔ ZDL ctx-field mapping (LineSel cross-reference)

A user contributed a hand-decoded ZD2 (Zoom G5-family format)
`Fx_SFX_LineSel` audio body. ZD2 uses different ctx indices than ZDL, but
the algorithm is the same — that lets us back-map ZD2's commented field
names onto our ZDL ctx slots. The two bodies process the same K0/K4/K5/K6
coefficient table out of `ctx[1]`, read input from one buffer, write to
two buffers (effect bus + pedal bus), and both shuttle a "current sample"
through `ctx[11]`/`ctx[12]` (ZDL) ≡ `ctx[7][0]` (ZD2).

| Role                                  | ZDL ctx   | ZD2 ctx               |
|---------------------------------------|-----------|-----------------------|
| coefficient / params table base       | `ctx[1]`  | `*A4[1]` (via A4)     |
| dry / input buffer                    | `ctx[4]`  | `ctx[8]`              |
| effect output (wet bus)               | `ctx[5]`  | `ctx[1]`              |
| pedal output (accumulator)            | `ctx[6]`  | `ctx[2]`              |
| current-sample shuttle dst (indirect) | `*ctx[11]`| `ctx[7][0]`           |
| current-sample shuttle src            | `ctx[12]` | input buffer reads    |

ZD2 buffers are processed as 16 samples × 2 outer iterations with a
64-byte stride between iterations (32 samples × 4-byte float total per
channel pair). ZDL buffers are 8 samples × 2 outer iterations (the
`LLLLLLLL RRRRRRRR` layout, 16 floats per call). Same algorithm,
different block sizes — useful when porting effects between formats.

#### LineSel coefficient model — what `ctx[1]` actually means

The ZD2 trace plus the ZDL disassembly of `Fx_FLT_LineSel` together show
that LineSel's audio body reads **four named coefficients K0, K4, K5, K6**
from `ctx[1]` and computes:

```
sample × K0 × K4 × K5         → written to ctx[5] (effect bus)
(1 - K0) × sample × K4 × K6   → added to ctx[6] (pedal bus accumulator)
```

So K0 acts as the wet/dry mix (effect-bus enable), and K6 is the pedal-bus
output level. K5 is the effect-bus level. K4 is a fixed gain (in stock
LineSel it stays at the default the host sets up).

Mapping the K-positions onto the param/coefficient table indices that
`ctx[1]` indexes:

| coefficient | `ctx[1][N]` byte offset | corresponds to                |
|-------------|------------------------:|-------------------------------|
| K0          | `+0x00` (=`params[0]`)  | OnOff state (bypass fade)     |
| K4          | `+0x10` (=`params[4]`)  | system gain (default-only)    |
| K5          | `+0x14` (=`params[5]`)  | `EfxLvl_edit` writes here     |
| K6          | `+0x18` (=`params[6]`)  | `OutLvl_edit` writes here     |

That places the two LineSel user knobs at `params[5]` and `params[6]` —
exactly what `build/linker.py` already produces. **The first user knob
lands at `params[5]` because positions `params[0..4]` are taken by
host-managed coefficients (K0, K4, plus three system slots) that the
audio function may read.** Custom plugins should treat the
`params[0..4]` range as reserved for system fields, not freely writable.

`_Fx_FLT_LineSel_Coe` (the 28-byte table that `_init` registers via
`state[34]`) is all zeros on disk; the host populates K0 and K4
dynamically. This means **state[34]'s job is to *register* the
coefficient region with the host, not to copy real data into it** — the
real coefficient updates flow through the edit handlers and the state[7]
tail-call.

#### `state[7]`'s firmware target is not a simple param store

The edit handler's final tail-call goes to `state[7]` (template value
`0xc00cc94c`). Reading that firmware function shows it is **not** a
"write the float to `params[A0+N]`" routine. Instead it:

* loads `state[0]` (= 0 from template), computes `0 - B10` as a
  single-precision float (B10 = the value the handler passed in);
* converts to double, takes `ABSDP`, compares against `0` with
  `CMPEQSP` — early-exits if the input was exactly zero;
* otherwise multiplies the absolute value by a constant
  `0x3eb00000_a0b50000` (a double-precision scaling factor near
  `5.36e-7`) and compares with another constant for clamping;
* finally calls into `c00cc8c8` (further normalization) and writes to a
  global UI state region at `0x11f03b08`/`0x11f03b18` (past the
  per-slot 212-byte block at `0x11f03000 + slot*0xD4`).

Practical implications:

* The state[7] tail-call is **not safe to skip** in a borrowed handler
  — it does denormal/zero sanitization and UI synchronization, not just
  storage.
* The "params slot offset" the edit handler computes (`MV A0, A4;
  MVK 20, A4; ADD A0, A4, A4`) probably tells state[7] *which* param
  to update; the actual write happens inside the firmware after
  normalization.
* Writing to `params[N]` from a custom audio function (bypassing
  state[7]) is one-way — it lands as a raw float without UI sync.

This is why the SyncProbe patched-handler experiment produced silent
audio: even when `state[24]` returned a non-zero value, the SHL-22
shift plus the state[7] normalize-and-clamp logic mapped most return
values to zero or a constant near zero. A correct sync probe needs to
bypass state[7] entirely, or supply state[7] with a value already in
the post-SHL range it expects (`< ~0.14` raw).

#### state[7] → c00cc8c8 → the audio↔UI postbox at 0x11f03b00

Tracing state[7]'s call chain past the float-normalize phase lands at
`c00cc8c8`, which is a **postbox-style IPC writer** between the
audio-context handler and the UI-context updater. The postbox lives
at `0x11f03b00..0x11f03b1f` — a shared region, not per-slot — and
the template writer points **`state[11]`** at it for every slot
(template value `0x11f03b08`, same for all slots).

`c00cc8c8`'s actual work:

```
*(0x11f03b04) = 0x11f03b18      ; next-message link?
*(0x11f03b0c) = caller's A6     ; the param-slot offset/value
*(0x11f03b14) = caller's A4     ; the params address being updated
*(0x11f03b1c) = 1               ; sentinel: "message pending"
CALLP c00cebc0                  ; wake UI / yield
loop: while (*(0x11f03b1c) != 0) CALLP c00cebc0  ; spin until UI processes
```

So when the LineSel edit handler tail-calls state[7], the firmware:

1. Normalizes the float value (denormal/clamp/scale).
2. Posts a message in the UI postbox describing which param slot was
   touched and what value.
3. **Synchronously waits** for the UI thread to ack the message before
   returning.

That's why a borrowed-handler probe like SyncProbe v1 sees its return
value silently dropped: even when the patched LDW reads `state[24]`
instead of `state[31]`, the value still flows through state[7]'s
sanitize-then-postbox path, which expects post-SHL-22 raw knob values
(`< ~0.14`). Larger or oddly-shaped uint32 returns from `state[24]`
get clamped to zero before they reach the params array.

A "bypass-state[7]" probe writes the raw `state[24]` return value
**directly** to `params[5]` from the custom handler (no SHL, no tail
call). The trade-off: the UI postbox is not updated, so the displayed
knob position is wrong. For diagnostic purposes that is fine.

**Hardware finding (SyncPrV2, 2026-05-29):** bypassing state[7]
**freezes the pedal** on knob interaction. The handler returns
cleanly (load and unbypass are fine) but the very first knob touch
hangs the pedal — the dispatcher or UI thread is blocking on the
postbox sentinel at `0x11f03b1c` that the bypassed handler never
sets. So state[7] is **not optional** for user-interaction handlers:
the postbox IPC is part of the contract, not a sanitization
convenience.

The corrected design for a sync-aware probe is therefore: keep the
state[7] tail-call intact, but **shape the value it receives** so it
survives the `SHL.S1 A4, 0x16` + state[7] normalize chain. The
simplest shaping is `A4 = state[24]_return & 0xFF` — mask to the
0..255 raw-knob range state[7]'s normalizer expects. The resulting
`params[5]` then carries a value in the LineSel `0..~0.14` float
range that varies with the low 8 bits of state[24]'s output, while
the UI postbox stays correctly synced.

**Second correction (2026-05-29):** the shape-the-value-before-state[7]
design above is sound for getting through the normalize chain without
freezing, but reading state[24]'s firmware target `c00d4b40` directly
shows it is **not a BPM helper at all** — it is a float-math utility
(ATAN2 / RCPSP / polynomial). Its opening early-exits on `A4 == 0.0`
or `B4 == 0.0`, then runs trig-style polynomial math with constants
`0x3FC90000` (~π/2) and `0x3F860000`. Every SyncProbe call that
passed `A4 = state[0] = 0.0` hit the early-exit and returned zero.
Even with a perfect `& 0xFF` mask before state[7], the input to mask
is zero, so the mask gives zero, so params[5] gets zero.

The takeaway: **TAPEECH3 uses state[24] as one math helper in a
larger BPM-derivation chain**; the BPM value itself comes from
state[31] called earlier with `B4=4` ("get raw time"), and state[24]
is then used to normalize/divide it. A custom sync probe needs to
follow the full TAPEECH3 chain (multiple state[31] calls then
state[24] with proper float-shaped args), not patch a single LDW
offset. v3 with `& 0xFF` mask was abandoned for this reason — see
[TEMPO-SYNC.md §3](../docs/TEMPO-SYNC.md) for the corrected
mental model.

#### LineSel coefficient population — the full chain

Cross-referencing the LineSel edit handlers, onf, and state[34]
copy/init routine establishes the complete K-coefficient update path:

| coefficient | populated by | through |
|-------------|--------------|---------|
| K0 (`params[0]`) | `Fx_FLT_LineSel_onf`, on bypass-toggle | calls state[7] with `B4 = 0.0f` (off) or `B4 = 1.0f` (on); state[7] normalizes and posts to UI |
| K4 (`params[4]`) | `state[34]`'s post-copy normalizer at `c00dde40+` | copies 28 zeros from `_Fx_FLT_LineSel_Coe`, then the post-copy block at `c00dde40+` reshapes/defaults the floats (likely K4 = 1.0) |
| K5 (`params[5]`) | `Fx_FLT_LineSel_EfxLvl_edit`, on knob turn | sets `A4 = params + 20` and tail-calls state[7] with the new knob value as `B4` |
| K6 (`params[6]`) | `Fx_FLT_LineSel_OutLvl_edit`, on knob turn | sets `A4 = params + 24` and tail-calls state[7] with the new knob value as `B4` |

`state[34]` (= template value `0xc00ddda0`) is the load-bearing piece.
The first part of `c00ddda0..c00dde38` does a byte-level `memcpy` from
`B4` (source) to `A5` (= `A4` = `state[1]` = params base) of size `A6`
bytes. For LineSel that's 28 zero bytes. The block at
`c00dde40..c00dded4` then runs additional work on the destination —
loading a 0x3FE constant, comparing exponents, conditional shifts
that look like a single-precision range normalization. This is
probably where K4 gets its non-zero default; without it, the audio
formula collapses to zero (`pedal_bus = (1-K0) × sample × 0 × K6 = 0`).

The full normalizer hasn't been hand-decoded yet, but the practical
takeaway is: **the `_Coe` table on disk does not need real
coefficient values — `state[34]` does the work to make them safe
defaults.** A custom plugin that calls `state[34]` from its `_init`
gets the same treatment, even when the `_Coe` table is all zeros.

The audio loop processes **8 samples per channel × 2 channels = 16
floats per call**, channel-interleaved as `LLLLLLLL RRRRRRRR`.
Implementations use a 2-iteration outer loop over channels (`MVK 2,B0`)
and 8 inline samples per inner block.

#### Provisional host state fields [hardware + stock-disassembly]

Custom hardware probes prove that `ctx[2] + 0x10` and `ctx[2] + 0x18` are
writable, persistent, and likely per-instance for at least words 0, 12, 18, and
19. `StateComb` used `ctx[2] + 0x18` words 0..15 plus word 18 as a tiny comb
history, so this block can hold small DSP state.

Stock delay/modulation disassembly and `DescComb` hardware testing confirm
`ctx[3]` as the large host-managed buffer descriptor:

```
ctx[3][0]  base pointer
ctx[3][1]  end pointer
ctx[3][2]  wrap span / byte length
```

Stock `DELAY`, `ANLGDLY`, `TAPEECHO`, and `STCHO` form sample-history addresses
from `ctx[3][0]`, compare against `ctx[3][1]`, and subtract/reload
`ctx[3][2]` when wrapping. Custom `DescComb.ZDL` first proved the descriptor is
readable/plausible (`Arm=1`, `UseBuf=0` stereo wobble), then proved descriptor
base memory is writable audio history (`UseBuf=1` sounded like a delay effect).
`DescSize` then proved the default descriptor allocation is at least 524288
bytes (`Dsz512K` wobbles), enough for the raw two-array memory requirement of
Airwindows `StereoChorus`.
`DescIso` showed two duplicate instances in separate FX slots do not see each
other's descriptor-memory stamps, so `ctx[3]` is currently treated as
per-instance.
`Dsz689K` wobbles. If the "works up to 689K" report means `Dsz690K` and higher
were silent, the default descriptor allocation is bracketed at `>= 705536` and
`< 706560` bytes. The exact byte count is no longer required for the first
`StereoChorus` exact-port attempt; the important ABI result is a per-instance
large descriptor arena of at least 705536 bytes.

### 5.3 Init `Fx_FLT_<Name>_init`

CH_2.md observed for LineSel: init calls `_onf`, `_edit_efx`, `_edit_out`
in sequence. Current disassembly corrects one older assumption:
`0x80000378` is not the host state pointer; it is `_Fx_FLT_LineSel_Coe`, the
effect-local coefficient table passed to a setup callback. Init is called with
a host-provided state pointer in `A4`, registers the coefficient table, then
invokes the per-param handlers to push initial values into the runtime.

LineSel's stock init sequence is:

```
A4 = state
A0/A10/A5 = state
A4 = state + 136
A0 = *(state + 136)          ; host setup callback
B4 = _Fx_FLT_LineSel_Coe     ; 28-byte coefficient table
A4 = state[1]                ; parameter table / materialized value area
A6 = 28
B31 = A0
CALLP __c6xabi_call_stub
CALLP Fx_FLT_LineSel_EfxLvl_edit with A4 = state
CALLP Fx_FLT_LineSel_OutLvl_edit with A4 = state
```

LineSel edit handlers use a second layer of callbacks:

| State field | Observed use in LineSel handlers | Status |
|---:|---|---|
| `state[0]` | passed as `A4` into the first edit/onf callback | partial |
| `state[1]` | base pointer used by handlers and init setup | partial |
| `state[7]` | tail-called after materialization callback setup | partial |
| `state[21]` | callback pointer used by knob edit handlers after the first callback | partial |
| `state[31]` | callback pointer used by on/off and knob edit handlers first | partial |
| `state + 136` | setup callback pointer used by stock init for coefficient-table registration | hardware-safe in `InitProbe` stage 2 |

`build/analyze_stock_init_handlers.py` generalizes this across stock ZDLs. A
sample scan of LineSel, Exciter, OptComp, ZNR, BottomB, Air, Delay, StereoCho,
TapeEcho, Hall, AutoPan, and Phaser shows the same broad shape:

* Init functions first call one or more setup callbacks through
  `__c6xabi_call_stub`, usually from `state + 136` and `state + 140`.
* Init functions then call each stock edit handler with the original
  host-provided state pointer.
* Most knob edit handlers read `state[31]`; value/output-style handlers often
  also read `state[21]`; on/off or time/rate-style handlers often tail through
  `state[7]`.

This makes the parameter bug look like a general host-materialization ABI
problem, not a ToTape9-specific DSP issue. Custom builds can copy stock handler
bytes, but they cannot safely call those handlers during custom init until the
complete init-time callback environment is understood.

Firmware confirms the state pointer passed to these handlers. `c00b056c` is
the SonicStomp entry lookup: given `(slot, entry_index)`, it returns
`descriptor_base + entry_index * 0x30`. The generic handler path at `c00bb460`
then loads word 7 of that entry (`+0x1C`, the `func_ptr`), calls
`c00c8e6c(slot)`, and branches to the handler pointer. `c00c8e6c` is just:

```
state = 0x11f03000 + slot * 0xD4
```

The handler receives that 212-byte per-slot runtime state in `A4`. Entry index
`0` is the on/off entry, entry index `1` is the effect-name entry and stock
`_init`, and later entries are user edit handlers. This is now the primary
anchor for mapping `state[7]`, `state[21]`, `state[31]`, `state + 136`, and
`state + 140`.

The state template writer at `c00c8ac0..c00c8e64` initializes all six per-slot
state blocks. It starts at `0x11f03000`, uses a stride of `0xD4`, writes a
53-word template, then repeats six times. The callback fields that stock
LineSel/Exciter handlers use have these initial values:

| Field | Byte offset | Initial template value | Known use |
|---:|---:|---|---|
| `state[7]` | `+0x1c` | `c00cc94c` | tail-called by on/off and some edit handlers |
| `state[21]` | `+0x54` | `c00c8c80` | second materialization callback in value handlers |
| `state[31]` | `+0x7c` | `c00b820c` | first materialization callback in on/off/edit handlers |
| `state[34]` | `+0x88` | `c00ddda0` | coefficient-table setup callback (`state + 136`) |
| `state[35]` | `+0x8c` | `c00dbae0` | second setup callback (`state + 140`) |

The same template writer also seeds non-callback per-slot fields:

| Field | Template source | Current read |
|---:|---|---|
| `state[0]` | literal `0` | source for the first callback's `A4`; likely patched later or phase-dependent |
| `state[1]` | `*(c00ee8e8 + 4*slot)` | likely setup/materialization base |
| `state[2]` | `*(c00ee900 + 4*slot)` | unresolved per-slot pointer/value |
| `state[3]` | `c00ee430 + 12*slot` | unresolved 12-byte per-slot record |
| `state[29]` | `c00ee9f0 + 4*slot` | unresolved per-slot pointer/value |

The currently extracted firmware chunks do not cover `c00ee430`, `c00ee8e8`,
`c00ee900`, or `c00ee9f0`, so their contents still need a missing data-segment
map or runtime dump. This is now the most direct lead for the ToTape9
first-touch/reload parameter bug.

These values mean the init/edit callback table is not absent when custom init
runs. The remaining init-handler failure likely involves some other required
state field or phase/argument condition.

Firmware static RE gives this a nearby loader-state lead. In
`firmware/extracted/main_os.dis`, the ZDL/ELF load path around `c00a5406`
allocates 164 bytes, initializes words 0, 1, 15..31, and initializes byte
offsets 128..156 before checking the ELF magic at `c00a54b8`. It sets word 31
to `1` initially and later code near `c00a63e4` tests/clears word 31 while
adjusting a size/allocation field. Because LineSel's edit handler later treats
`state[31]` as a callable pointer, this 164-byte block is not proven to be the
exact handler state passed to stock init; it may be adjacent loader bookkeeping
or a pre-patched form. The later `c00bb460`/`c00c8e6c` dispatch path confirms
that the main handler state object is instead the 212-byte per-slot block at
`0x11f03000 + slot * 0xD4`.

`build/find_firmware_state_offsets.py` scans firmware disassembly for these
suspected fields. The first pass did **not** find a simple "write function
pointer to `state + 136`, then call stock init" pattern. Instead, it found
several loader-side uses that make the lifecycle look more phase-specific:

* The allocation/init block at `c00a5406..c00a54a8` fills byte offsets
  `+128`, `+136`, `+140`, `+148`, and `+152` with `-1`, and fills `+132`,
  `+144`, and `+156` with `0`.
* The region around `c00a5ae4..c00a5b02` writes a runtime value to `+140` and
  another table-derived value to `+156`.
* The region around `c00a65f4..c00a6678` resets `+140`/`+152`/`+156` and then
  reads `+140` as an index into a table rooted at `state[19]`.
* The region around `c00a6f56..c00a6f72` reads `+136` as an index into the
  same kind of table-rooted lookup and writes the resulting value to an
  output pointer.

So the current firmware-side hypothesis is: the offsets stock init sees as
callable setup entries are populated in the 212-byte per-slot state block,
while similarly numbered fields in nearby loader bookkeeping may still be
indices/sentinels. Do not synthesize this state from the `c00a5406` allocation
alone. The next static search should map the non-callback fields consumed by
stock edit handlers, especially `state[0]`, `state[1]`, and any descriptor-
derived per-parameter fields.

Firmware dispatch lead: `c00d3bec` is a tiny generic function-pointer caller:

```
c00d3bec  save B3
c00d3bf0  branch to A4
c00d3bf4  set return PC
c00d3bf8  restore B3
```

It is called from loader/runtime regions at `c00a4e14`, `c00a4e34`,
`c00a6ad8`, and `c00a6b38`. The surrounding code is not yet proven to be ZDL
`_init`, but it is important:

* `c00a4db0..c00a4e38` checks `word22`, iterates over an array rooted at
  `word21`, and calls each non-null entry through `c00d3bec`. It also calls
  `word20` if present.
* `c00a6ab0..c00a6b64` does a similar walk through a linked/list-like object:
  call `object[0]` if `object[1]` is zero, otherwise iterate entries rooted at
  `object[0]`, then call via `c00d3bec`.

These regions are better candidates for the late-bound callback dispatch
environment than the raw 164-byte loader allocation, but the first attempt to
connect their list fields back to descriptor records reclassified the nearby
parser as ELF dynamic-table handling rather than normal parameter
materialization.

Correction on the suspected descriptor/list connection: the parser at
`c00a61b8..c00a62e0` is walking ELF dynamic-table entries (`Elf32_Dyn`), not
SonicStomp/UI descriptor entries. The type values are the giveaway: `12` is
`DT_INIT`, `13` is `DT_FINI`, `14` is `DT_SONAME`, `20` is `DT_PLTREL`, `22` is
`DT_TEXTREL`, `23` is `DT_JMPREL`, `25..29` are init/fini-array/runpath tags,
and `32..33` are preinit-array tags.

A `PT_DYNAMIC` scan over the stock corpus found 825 normal dynamic tables out
of 830 ZDLs. Those 825 use the common relocation/symbol/string tags, but no
stock file in the scan uses `DT_INIT`, `DT_FINI`, `DT_INIT_ARRAY`,
`DT_FINI_ARRAY`, `RUNPATH`, or `PREINIT_ARRAY`. The current custom linker also
emits none of those tags. Therefore the type-13 branch is a loader-general
`DT_FINI` path for uncommon ELF features, not the likely parameter
materialization path for normal effects. Keep the parameter bug investigation
centered on the SonicStomp init function entry and the edit-handler state
callbacks (`state + 136/+140`, `state[31]`, `state[21]`, `state[7]`).

For Exciter, init at `.text+0x5c0` (per `Fx_FLT_Exciter_init` symbol)
should follow the same pattern — invoke onf, then each edit handler.

Current linker support: `LinkerConfig(use_object_init_handler=True)` can use an
object-defined `<audio_func>_init` symbol when present and resolves calls from
that shim to the exact on/off/edit handler VAs selected for the descriptor.
Hardware caution: `InitProbe` stage 2 showed that a custom init shim can safely
perform the stock-style coefficient setup call through `__c6xabi_call_stub`.
The next stage, which called a cloned LineSel edit handler from init after that
setup, froze the pedal on boot. Release builds currently keep a NOP init until
the edit-handler init context is understood. The current best explanation is
that setup uses only `state + 136`, while edit handlers additionally require
valid `state[31]`, `state[21]`, and `state[7]` callback fields for that exact
init phase.

Relocation caution: object-file `PCR_S21` relocations do not all use the same
addend interpretation. For section-local calls, cl6x's placeholder displacement
is measured from the section start, so the linker adds the instruction's
section offset. For external symbols such as `__c6xabi_call_stub`, the resolved
symbol address is already the final VA; adding the instruction offset again
lands past the stub and caused a boot freeze during `InitProbe` testing.

### 5.3.b Parameter table layout [hardware-confirmed through 9 params]

`ctx[1]` points to a flat float array. Verified slots, used by
all three diy/*.asm reference effects:

```
params[0]   on/off multiplier   (1.0 when on, 0.0 when off)
params[4]   level multiplier    (= 1/max, e.g. 0.01 for max=100)
params[5]   knob 1 raw value    (0..max as float)
params[6]   knob 2 raw value
params[7]   knob 3 raw value
...
params[13]  knob 9 raw value
```

Audio code typically computes a per-knob coefficient as
`params[5] * params[4] * params[0]` once at the top of the function
(producing a normalized `0..1` scaled by on/off), then applies it
inside the sample loop.

Slots `params[5..13]` are contiguous for the 1-9 user-param range.
Generated edit handlers must write these slots; NOP handlers render UI
but do not update the audio parameters. The reusable macro lives in
`src/airwindows/common/zoom_edit_handlers.h`.

### 5.4 DLL entry `Dll_<Name>` [v1-empirical]

The ELF `e_entry` is `Dll_<Name>`. It returns:

* `B0` = address of the descriptor table **start (the OnOff entry)**,
  not the name entry.
* `A1` = address of `effectTypeImageInfo`.

Both addresses are loaded via MVK/MVKH instruction pairs that are
patched by `.rela.dyn` ABS_L16 + ABS_H16 relocations at load time.

Body length matters. v1 tried an 8-instruction (32-byte) Dll function
patterned after LOFIDLY — the unit booted but froze inconsistently on
each FX-select event. Switching to a **verbatim 200-byte (50-instruction)
copy of NoiseGate's Dll function**, with the 4 reloc points re-patched
for the new descriptor + imageInfo addresses, produced a stable boot.
The simplest working approach is therefore: splice in NoiseGate's body,
patch the relocation targets, and patch its compact `MVK A0` immediate
from NoiseGate's hardcoded descriptor count `4` to `2 + len(params)`.

### 5.5 Compile flags — the `--mem_model:data=far` trap [v1-empirical]

Critical compiler flag for any C source file used in a ZDL:

```
cl6x --c99 --opt_level=2 --opt_for_space=3 -mv6740 --abi=eabi \
     --mem_model:data=far -c your.c -o your.obj
```

Without `--mem_model:data=far`, the C compiler places small statics
in `.bss` and addresses them via **B14-relative** loads (a.k.a. SBR /
DP-relative). The Zoom firmware does *not* set B14 to a valid base
before invoking your code, so any such load reads garbage and freezes
the unit.

With `--mem_model:data=far`, every static lives in its own `.far:<name>`
section and is addressed via absolute MVKL / MVKH pairs (`R_C6000_ABS_L16`
+ `R_C6000_ABS_H16` relocations). Those *are* resolvable by the runtime
linker.

Putting the audio function into the `.audio` section is one
`#pragma CODE_SECTION` away:

```c
#pragma CODE_SECTION(Fx_FLT_<Name>, ".audio")
void Fx_FLT_<Name>(unsigned int *ctx) { ... }
```

---

## 6. Memory map and constraints

| Region     | VA                            | Flags | Notes                                        |
|------------|-------------------------------|-------|----------------------------------------------|
| `.text`    | `0x00000000` upward           | r-x   | Firmware remaps to IRAM at load time.        |
| `.const`   | `0x80000000` upward           | r--   | RO data: descriptor, image, coefficients.    |
| `.fardata` | immediately after `.const`    | rw-   | Tiny writable data. **memsz must equal filesz**. |
| Stack      | `B15` provided by host        | rw-   | Don't overflow — no MMU, no guard page.      |

* **`.fardata` must have `memsz == filesz`** [v1-empirical]. Setting
  `memsz > filesz` (i.e. requesting BSS zero-fill) overflows into
  firmware-managed memory and corrupts the parameter array, breaking
  paging. Stock effects all set `memsz = filesz` and put their
  initialised state in `.const` lookup tables, accumulating runtime
  state in a firmware-provided per-effect scratch buffer.
* The `.fardata` section's leading 24 bytes are *always*
  `_infoEffectTypeKnob_A_2 = {20, 15, 11, 0, 2, 0}`. Any user state
  follows from offset 24.
* Keep writable `.fardata` small. Stock MS-70CDR effects observed so far
  stay within a few hundred bytes; large custom static state has frozen
  hardware on effect load. The linker rejects `.fardata` images above
  512 bytes unless `allow_large_fardata=True` is set for an explicit
  hardware probe.
* **No malloc.** All state is statically allocated.
* **No FPU exceptions** worth catching — the C6740 has hardware float;
  treat NaN/Inf the same as Airwindows desktop builds do.
* **No `sinf`/`cosf`/`tanf`/`logf` in the runtime** — none of the
  stock effects use them, so they aren't in firmware's RTS. Either
  inline approximations (v1's `totape9_zoom.c` has `zoom_sinf`,
  `zoom_logf`, `zoom_tanf` examples) or table-lookups.
* **`__c6xabi_divf` (float divide) IS available** — extracted from a
  stock ZDL, ships as `build/divf_rts.bin`, gets spliced into `.text`.

## 6.1 SONAME — the `gid` trap [v1-empirical]

The `.dynamic` `DT_SONAME` string **must follow the pattern**
`ZDL_<GID_PREFIX>_<Name>.out`, where `<GID_PREFIX>` matches the
3-letter category code for the `gid` byte in the ZDL header:

| gid  | Category    | SONAME prefix |
|------|-------------|---------------|
| 0x01 | Dynamics    | `ZDL_DYN_`    |
| 0x02 | Filter      | `ZDL_FLT_`    |
| 0x06 | Modulation  | `ZDL_MOD_`    |
| 0x07 | SFX         | `ZDL_SFX_`    |
| 0x08 | Delay       | `ZDL_DLY_`    |
| 0x09 | Reverb      | `ZDL_REV_`    |

Mismatching prefix and gid causes the firmware to fall back to a
2-knob no-page render mode regardless of what the descriptor says.

---

## 7. The LineSel "trick book" (relevant for any port)

LineSel teaches the cleanest mental model for the host's signal flow:

1. The audio function gets three buffer pairs: **Effect (wet)**,
   **Guitar (dry)**, **Output (accumulator)**.
2. **Effect** is the upstream signal *as modified so far* by previous
   effects in the chain.
3. **Guitar** is always the original raw input.
4. **Output** accumulates whatever each effect adds; final
   speaker-bound signal is `Output` after the last effect runs.
5. Most factory effects compute `wet_out = process(Effect)`, write
   that back into `Effect` (so the next effect sees it), and don't
   touch `Output` (so trails decay cleanly when this effect is bypassed).

The ZD2 `Fx_SFX_LineSel` body posted by mungewell/ELynx is not a drop-in match
for the ZDL ABI: it uses `Fx_SFX_`, a different context field layout, and a
looped body, while MS-70CDR ZDL LineSel exports `Fx_FLT_LineSel` and uses an
8-sample unrolled `.audio` body. The signal-flow and coefficient roles line up,
though: coefficient table values equivalent to `K0`, `K4`, `K5`, and `K6`
drive the effect-output and pedal-output gains. That supports the current
parameter-bug diagnosis: stock init/edit handlers materialize coefficient-table
values before audio runs, while custom builds with cloned edit handlers may see
zero/unmaterialized parameter slots until the user touches a knob.

This means a port like Airwindows `Console` channel — which is itself
a clean sum stage — maps directly onto reading `Effect`, summing, and
writing back `Effect` (or directly into `Output` if it's the last
effect we care about). Most Airwindows kernels are sample-by-sample
and don't care about the L/R interleave subtlety.

---

## 8. Summary checklist for our first ground-up plugin

To produce a loadable ELF, we need:

- [ ] CCS 8.x project, generic C674x, C6000 v8.3.x compiler, ABIv2.
- [ ] Linker command file with three segments at the addresses in §1.
- [ ] One C/asm source file exporting the §2 symbols, with `Dll_<Name>`
      as the ELF entry point.
- [ ] A correctly-shaped `SonicStomp` (§3) and `effectTypeImageInfo` (§4).
- [ ] `Fx_FLT_<Name>` operating on float32 in/out, *adding* to Output
      (§5.2).
- [ ] `_init`, `_onf`, and per-param `_edit` handlers (§5.1).
- [ ] A `picEffectType_<Name>` blob — can stub with all-zeros to start;
      icon will look blank but the unit will boot.
- [ ] A picture pointer + knob layout in `effectTypeImageInfo`.

When the linked `.out` is small (under a few KB) and exports exactly
the §2 symbol set, our existing `build/zdl.py` can wrap it into a ZDL
unchanged — the SIZE field is recomputed from `len(elf)` on save.

---

## 9. Open questions to settle empirically

After incorporating v1's findings and the 2026-05-13 hardware probes, the list
shrinks to:

1. **Load-safe shape for complex ports.** `ctx[3]` is proven enough for large
   per-instance state, and `StereoChorus` uses it successfully. The current
   `ToTape9` build still crashes on load, so the open problem is now the whole
   load-time shape: 9 parameters, synthesized page 2/3 edit handlers, helper
   symbols, and a larger `.audio` image.
2. The two unknown words at `effectTypeImageInfo` offsets 0x18 / 0x1C
   (Exciter has 32 / 17; LineSel has different values). Stock ZDLs all
   work with these as observed; we copy them. Their semantic role is
   irrelevant for now.
3. The `+0x18` reserved word in each SonicStomp entry — non-zero in
   delay/pitch effects only. May encode a sub-range or sub-tick value.
4. The `ctx[11]` / `ctx[12]` "magic shuttle" in the audio loop — what
   bytes are these, and what breaks if we skip the read-and-rewrite?

Lower priority — already resolved well enough for v2:

* Sample format: float32 ✓
* Sample rate: 44.1 kHz ✓
* Per-call block size: 8 samples per channel × 2 channels ✓
* C6000 calling convention: standard EABI ✓
* SONAME pattern: `ZDL_<GID>_<Name>.out` ✓
* `--mem_model:data=far` requirement ✓
* `.fardata` `memsz == filesz` ✓
* `effectTypeImageInfo` exactly 212 bytes, exactly 3 knob slots ✓
* `ctx[3][0..2]` large host-managed state descriptor ✓
* `Dll_<Name>` body: NoiseGate verbatim, 200 bytes ✓
* `KNOB_INFO = {20, 15, 11, 0, 2, 0}` ✓
