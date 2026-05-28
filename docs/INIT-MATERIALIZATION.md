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

## 7. Where this changes other docs

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
