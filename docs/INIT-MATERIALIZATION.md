# Parameter Materialization on Load

Last updated: 2026-05-19

This file documents the stock `_init` recipe that materializes parameter
values at load time. The recipe is reconstructed by reading
`Fx_FLT_LineSel_init` (2 params), `Fx_FLT_Exciter_init` (3 params),
`Fx_FLT_A_Filter_init` (multi-param) from their stock ZDLs side by side.

It is a static-RE doc. Hardware verification is the
[MatProbe](#matprobe) experiment.

## 1. The bug

On preset reload or effect swap, the audio function reads
`params[5..N]` as zeros until the user touches a knob. This is the
"parameter persistency" or "params-zeroed-on-reload" bug tracked across
ToTape9, TapeEcho4, and OTT. Stock effects don't have this bug.

[InitProbe](../src/hardware_probes/initprobe/) Stage 2 confirmed the
stock-style **coefficient-table setup call** is load-safe from a custom
`_init`. Stage 3 attempted to add a cloned edit-handler call after the
setup and froze on boot. That stalled the investigation until now.

## 2. What stock `_init` does

All three reference effects share the same body shape:

```
Fx_FLT_<Name>_init:
    CALLP __push_rts                       ; save callee-saved regs

    MV A4, A0                              ; A0 = state ptr
    MV A4, A10                             ; A10 = state ptr (saved across calls)
    MV A4, A5                              ; A5 = state ptr (saved for state[1] load)

    MVK 136, A4
    ADD A0, A4, A4                         ; A4 = state + 136
    LDW *A4[0], A0                         ; A0 = state[34]  (setup callback)

    MVK <Coe_addr_low>, B4
    MVKH <Coe_addr_high>, B4               ; B4 = address of _Fx_..._Coe (.const data)

    LDW *A5[1], A4                         ; A4 = state[1]  (per-slot params/data base)
    MVK <Coe_size_bytes>, A6               ; A6 = size of Coe table

    CALLP __call_stub                      ; call state[34]
        MV.L2X A0, B31                     ; (delay slot) B31 = A0 = state[34]

    ; --- optional second setup call (effects with state[2] data) ---
    ; LineSel does NOT do this. Exciter and A_FILTER do.
    ; MVK 140, B0
    ; ADD B0, A1, B4                        ; B4 = state + 140
    ; LDW *B4[0], B0                        ; B0 = state[35] (second setup callback)
    ; ...load A0, A4, A6 with second-call args...
    ; CALLP __call_stub                     ; call state[35]
    ;     MV B0, B31                         ; (delay slot)

    ; --- materialization phase: call EACH edit handler ---
    CALLP Fx_FLT_<Name>_<param1>_edit
        MV A10, A4                         ; (delay slot) A4 = state ptr
    CALLP Fx_FLT_<Name>_<param2>_edit
        MV A10, A4                         ; (delay slot) A4 = state ptr
    CALLP Fx_FLT_<Name>_<param3>_edit
        MV A10, A4                         ; (delay slot)
    ; ...one CALLP per knob...

    CALLP __c6xabi_pop_rts                 ; restore callee-saved regs
```

The two halves do two distinct jobs:

* **Setup phase** registers a per-effect coefficient table with the host
  runtime. `state[34]` (and `state[35]` for multi-coefficient effects)
  is the callback. The table itself (`_Fx_FLT_<Name>_Coe` in `.const`)
  is allowed to be all-zero — LineSel's 28-byte Coe is exactly that.
  What matters is that the size (`A6`) and base address (`B4`) are
  passed correctly so the host knows what range to manage.
* **Materialization phase** explicitly calls each `_<param>_edit`
  handler with `A4 = state ptr`. Each handler runs its normal flow —
  read knob via `state[31](B4=knob_id)`, finalize via state[7]
  tail-call — and writes to `params[5+i]`. So when the audio function
  runs immediately after `_init`, every param slot already holds its
  saved value.

## 3. Why InitProbe Stage 3 froze (probable cause)

[InitProbe](../src/hardware_probes/initprobe/) Stage 3 added a single
`CALLP Fx_FLT_InitProbe_Knob1_edit` after the setup call. From the
asm in `initprobe.c`, it did **not** save the state pointer to `A10`
before the setup call and did **not** restore `A4 = state ptr` in the
edit handler's CALLP delay slot.

Compare:

```
; LineSel init (works):
    MV A4, A10                           ; <-- save state
    ...setup call...
    CALLP EfxLvl_edit
        MV A10, A4                       ; <-- restore A4 = state in delay slot

; InitProbe Stage 3 (froze):
    ...setup call without A10 save...
    CALLP Knob1_edit                     ; A4 was clobbered by setup return
        (no delay-slot fix)
```

When the edit handler started with the wrong `A4`, its
`LDW *+A7[31],B31` read garbage as the state[31] callback, then
indirect-called through it. That's a classic freeze cause.

This is a hypothesis based on static reading. Verifying it is exactly
what MatProbe is for.

## 4. MatProbe — the minimum experiment

Goal: prove that a custom `_init` following the LineSel recipe
**exactly** (including the A10 save and delay-slot A4 restore)
materializes params on reload.

Probe shape:

* Two user knobs (matches LineSel — simplest case in stock corpus).
* `_init` follows the LineSel recipe byte-for-byte, with our own
  zero-filled `_Coe` table (28 bytes) and our two LineSel-cloned edit
  handlers as the materialization targets.
* Audio function reads `params[5]` and `params[6]` and produces audible
  gain on each channel, so the test is:

  1. Set the two knobs to non-default values.
  2. Switch to another effect, switch back.
  3. Audio should immediately reflect the saved values **before**
     touching either knob. If it does, the recipe works.

Variants to keep on the shelf in case v1 freezes:

* MatProbe-v1.1 — drop the `__call_stub` setup call entirely, only call
  the edit handlers. If this works, the setup call wasn't required at
  all and we save complexity.
* MatProbe-v1.2 — add the second `state[35]` setup call. Required if
  state[1] alone isn't sufficient to bound the params slot.

## 5. Critical implementation details

The asm must be written with care:

* **`A10` must hold the state pointer** across the entire `_init` body.
  C6x callee-save convention preserves A10..A15, B10..B15 across calls
  via `__push_rts`/`__pop_rts`, so saving once at the top and reading
  in each delay slot is safe.
* **The `MV A10, A4` in CALLP delay slots is not optional.** Each
  `CALLP <edit_handler>` consumes 5 delay-slot cycles before the branch
  resolves. The edit handler reads `A4` as its state pointer, so `A4`
  must be set to `A10` before the branch fires.
* **Don't intermix the setup call's data setup with the edit handler
  call's register restore.** Stock `_init` carefully orders the
  instructions so the setup call's `A4 = state[1]` value is used by
  state[34], not by the subsequent edit handlers.
* **`_Fx_FLT_<Name>_Coe` size in A6 must match the `.const` allocation.**
  LineSel uses 28 bytes. Exciter uses 68 bytes for the first setup
  call. The size is per-effect; a custom plugin can use 28 if it has no
  real coefficients.

## 6. What this does **not** solve

* The audio function still sees `params[5..N]` as 0 between `_init`
  finishing and the first audio block. If `_init` does correctly
  materialize, that gap is zero-length. If it doesn't, the gap is
  permanent until the user touches a knob.
* Preset *creation* (first-time load with no saved values) — the firmware
  likely uses descriptor `default_val` to seed values before `_init`
  runs. This recipe handles the load case but the new-preset case
  needs its own check.
* Multi-page (page 2/3) edit handlers. LineSel has 2 knobs on page 1
  only. Once MatProbe v1 works, ToTape9 (9 knobs across 3 pages) is the
  next milestone — its `_init` will need to call all 9 handlers.

## 7. Firmware-side observations (when does `_init` get called?)

A static-RE pass through `firmware/extracted/main_os.dis` pinned down
three load-bearing facts about the dispatch path for `_init`. The
exact call site that invokes `_init` is not yet identified — the
firmware path runs through several layers of indirection — but the
shape of that call is now constrained enough that the recipe in §2
is the right thing to try first.

### 7.1 The generic dispatcher explicitly skips `entry_index = 1`

The generic handler dispatcher (entry around `c00bb288`, main body
`c00bb420..c00bb47c`) is what fires `_onf` and `_<param>_edit` when
the user interacts with the UI. It calls `get_entry(slot, entry_index)`,
loads `entry[7]` (= `func_ptr` at `+0x1C`), then branches to it with
`A4 = state ptr`. **But before doing that it checks
`entry_index == 1` and skips the dispatch entirely:**

```
c00bb420   MV A12, B0            ; B0 = entry_index
c00bb422   CMPEQ 1, B0, B0       ; B0 = (entry_index == 1)
c00bb424   [B0] BNOP 0xc00bb480  ; if entry_index == 1, jump past dispatch to return
```

So `_init` is **not** called through this path. It has its own
firmware dispatch route — which is consistent with `_init` being
called at slot-load time (one-shot, part of the load sequence) rather
than in response to UI interaction.

### 7.2 The template writer runs before `_init`

The 53-word handler-state template writer at `c00c8ac0..c00c8e64`
(documented in
[STATE-ABI-PROGRESS.md §"Per-slot state template"](STATE-ABI-PROGRESS.md))
has exactly one caller in the firmware: `c00ab614`. That call lives in
the big slot-initialization sequence at `c00ab610..c00ab6e0`:

```
c00ab610   CALLP 0xc00cc698        ; setup
c00ab614   CALLP 0xc00c8ac0        ; *** template writer ***
c00ab618   CALLP 0xc00ba7b0        ; buffer comparison
c00ab620   CALLP 0xc00d1410
c00ab624   CALLP 0xc00c6ccc
c00ab628   CALLP 0xc00c518c
c00ab62c   CALLP 0xc00ccda4
c00ab630   CALLP 0xc00c9da0        ; with B14[194], B4=10
c00ab63c   CALLP 0xc00c9da0        ; with B14[185], B4=7
c00ab648   CALLP 0xc00d6008
c00ab64c   CALLP 0xc00c9da0        ; with B14[169], B4=6
c00ab660   CALLP 0xc00cbf40
c00ab668   CALLP 0xc00dee20        ; with B14[164]
c00ab674   CALLP 0xc00ab540
c00ab678   CALLP 0xc00b93a8
c00ab688   CALLP 0xc00b8e64
c00ab68c   CALLP 0xc00b93b0
c00ab694   CALLP 0xc00b8e64
c00ab6a0   CALLP 0xc00ce4c4
c00ab6b8   CALLP 0xc00c8320        ; A12 = 1 here — likely the entry-1 walker
```

Since the template writer is what installs `state[7]`, `state[21]`,
`state[24]`, `state[30]`, `state[31]`, `state[34]`, `state[35]`
template values into the slot's 212-byte runtime block, and it is
called early in this setup sequence, **the slot's state-callback
table is fully populated before any user-effect code (`_init` or
`_onf` or `_<param>_edit`) runs.** That is why the SyncProbe state[24]
patch worked from edit-handler context: by the time the user-effect
gets control, `state[24]` already holds `0xc00d4b40` or whatever the
firmware put there.

### 7.3 One of the later CALLPs invokes `_init`

`c00c8320` is called at `c00ab6b8` with `A12 = 1`. The function is a
small wrapper that calls `c00c82e0` then `c00bbeb8` and returns —
`A12 = 1` survives across both calls, so whichever of those callees
checks `A12` is the one that ultimately invokes `_init`. `c00bbeb8`
is large and writes to a UI screen buffer (lots of `STH` of pixel-ish
values); the actual init entry may live in `c00c82e0` or one of the
many CALLPs `c00ab620..c00ab690` makes.

Pinning down the exact call site needs another pass and probably a
hardware breakpoint setup (out of scope here). For now, the safe
operating assumption is:

* `_init` is invoked at slot-load time by firmware code that runs
  **after** the template writer at `c00ab614`.
* The firmware enters `_init` with `A4 = state ptr` and `B3 = return
  address`, matching the generic handler convention even though
  the generic dispatcher itself does not handle entry_index=1.
* `state[1]` (the per-slot RAM-table value used as the params-table
  base) is initialized somewhere in the
  `c00ab620..c00ab690` setup chain before `_init` runs — otherwise
  LineSel's `LDW *A5[1],A4` followed by `state[34]` call would
  store its 28-byte Coe data into garbage RAM.

### 7.4 What this means for MatProbe

The recipe in §2 is consistent with what the firmware does:

* `_init` runs after the template writer, so all `state[7..35]`
  callbacks are available.
* `state[1]` is populated by the time `_init` runs, so `LDW *A5[1],A4`
  is safe.
* `_init` is called with `A4 = state ptr`, `B3 = return address` —
  same convention as user-interaction handlers — so the LineSel-style
  body (push_rts, save state in A10, do the setup call, materialize via
  edit handlers, pop_rts) is the right shape.

The remaining unknown (exact firmware call site) does not change the
recipe; it only affects how we would debug a freeze. If MatProbe v1
freezes, the next probe should isolate whether the freeze is in the
setup call or in the edit-handler call, not in the firmware entry
sequence (which we now know runs cleanly through to `_init` for the
NOP-stub init that SyncProbe ships).

## 8. Where this changes other docs

* [docs/STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md) §"Init And
  Edit-Handler ABI Status": the "calling a cloned LineSel edit handler
  from custom `_init`" entry can be re-tagged as "froze because A10
  save and delay-slot A4 restore were missing — corrected in
  [INIT-MATERIALIZATION.md](INIT-MATERIALIZATION.md)".
* [src/hardware_probes/initprobe/](../src/hardware_probes/initprobe/)
  comments should reference this doc once MatProbe confirms the recipe.
* If MatProbe works, [build/linker.py](../build/linker.py) can add a
  `materialize_init` flag that the linker emits automatically for any
  effect with LineSel-cloned handlers.
