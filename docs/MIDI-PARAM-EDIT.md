# Live param edits over SysEx: the slot rule

**Confirmed on hardware 2026-07-31.** Recorded here because this cost a very long
debugging session — an earlier one-line note said the same thing and was overruled.

## The rule

The MS-70CDR honours live `0x31` parameter edits **only for the early effect
slots**. For the later slots the identical message is accepted onto the wire and
silently ignored.

```
F0 52 00 61 31 <slot> <paramIdx> <lo7> <hi7> F7
```

* **slots 1-3** (`<slot>` = 0..2) — the value takes effect immediately.
* **slots 4-6** (`<slot>` = 3..5) — ignored. These need a buffer write plus a
  bypass bounce on that slot (see "the param cache" below). No store required.

All six slots were tested individually (2026-07-31), so the 1-3 / 4-6 boundary is
measured rather than inferred. The first clue was moving GenLoss from slot 4 to
slot 1: its knobs began responding instantly with no change to the effect at all
— same ZDL, same fxid, same descriptor.

## The asymmetry

Direction matters, and only one direction is limited:

| direction | slots 1-3 | slots 4-6 |
|---|---|---|
| editor → pedal (`0x31` in) | works | **ignored** |
| pedal → editor (`0x31` out) | works | works |

The pedal emits notifications for **every** slot, including the ones whose
inbound edits it discards — so the firmware clearly understands the address. The
limitation is in the receive handler alone, not in the addressing. Pinning down
why would mean disassembling the pedal firmware, which we have not done.

## What this is NOT

Every one of these was investigated and ruled out with evidence:

* **Not the param count.** Flower (2 params) and Spool (9 params) both live-edit
  fine; GenLoss (3) did not — until it moved slot.
* **Not the "last knob" / Mix / Hiss.** Once GenLoss was in slot 1, *all three*
  knobs worked, Hiss included.
* **Not the descriptor sentinel or flags.** Our 3-param layout (`flags=0x14`,
  `pedal_max=max` on the final entry) matches stock `AUTOWAH` and `BOTTOM_B`.
* **Not the DLL descriptor count.** Verified per effect at stub offset +142:
  Flower declares 4 (2 params + 2), GenLoss declares 5 (3 + 2). Both correct.
* **Not the bit tables.** Confirmed against the pedal's own dumps: setting Hiss
  to 100 on the pedal writes `byte 75 = 0x32`, and the table decodes
  `(0x32 & 0x7f) << 1 = 100`.
* **Not which slot the pedal is displaying.** Edits still fail on slot 4 with
  that effect on screen.

## The pedal talks back

Turning a knob **on the pedal** emits the same `0x31` form, so an editor can
follow the hardware live:

```
← F0 52 00 61 31 03 04 64 00 F7     slot 4, param idx 4 = 100
```

This is emit-only; sending it back for slots 4-6 does nothing.

## Why slots 4-6 ignore a written patch — the param cache

The DSP caches a slot's parameters **when the effect is instantiated**. Writing
the edit buffer updates the patch data but never touches that cache, so the slot
keeps playing its old values. Slots 1-3 escape this only because they accept live
`0x31`, which reaches the running effect by a different path.

The cure is to make the pedal re-instantiate the effect. Flip the slot's bypass
bit off and back on:

```
0x50            edit enable
0x28 <146B>     write buffer with the target slot's on/off bit = 0   (tear down)
0x28 <146B>     write buffer again with it = 1                       (rebuild, re-reads params)
```

Nothing is stored, no program change is sent, and only the edited slot is
disturbed — so no flash wear, no audible patch blip, and the pedal's display
stays where it was. The audible cost is a brief bypass gap on that one slot.

This is the automated form of the manual workaround "change the knob, switch the
slot off, switch it back on". It is also why **"wiggle the Mix knob on the
pedal"** has always worked: the wiggle forced a re-instantiation.

### Dead end: forcing a reload instead

Recorded so it is not retried. A **bare** program change to the program the
pedal is already on is a no-op, so `0xC0 n` alone reloads nothing — this had
silently broken every "recall" in the patch editor, including effect-type
changes. Bank Select MSB/LSB followed by `0xC0 n` (what a ToneLib capture shows)
does reload, but it needs a `0x32` store to survive, wears flash, blips through
the reloaded patch, and resets the pedal's display to the patch's stored `curfx`.
The bypass bounce above is better on every count.

### Writes must not be blind

The editor re-reads the patch and re-applies **only the fields the user changed**
(tracked as dirty `slot,field` pairs). Writing a whole cached model back pushes
stale values over slots the user never touched — including pedal-side knob moves
and live `0x31` edits that already landed.

## External references, and a coordinate-space trap

[shooking/ZoomPedalFun — "De re MS-70 CDR"](https://github.com/shooking/ZoomPedalFun/wiki/De-re-MS-70-CDR)
is the only other MS-70CDR-specific protocol write-up found. It independently
confirms every command this repo uses (`0x50`/`0x51` edit enable/disable, `0x29`
request → `0x28` dump, `0x31 <fx> <param+1> <lo> <hi>`, on/off as `0x31` with
param byte `00`, `0x32` store, `0xC0` load) and the 7-in-8 packing. It documents
**nothing** about per-slot live-edit limits or the param cache, so the findings
above are not recorded anywhere else.

Two corrections to it, both checked against hardware:

* It says the pedal has **5 FX slots** and that the `0x31` slot byte is "0-4 for
  FX 1-5". The MS-70CDR has **6** — confirmed on the device. `maxfx` is not
  evidence either way; it counts *populated* slots, not capacity.
* **Its byte offsets are in UNPACKED space; this repo's bit tables index the RAW
  PACKED message.** The two are not comparable. Convert with:

  ```
  raw = 5 + 8*(u // 7) + 1 + (u % 7)        # u = unpacked index
  ```

  So its "tempo is spread over bytes 109 and 110" means **raw bytes 130-131**.
  Mixing the two spaces made it look briefly as though `encodePatch` was
  clobbering the tempo. It is not: raw byte 130's `0x1c` bits decode as the
  effect count (read 5 on a patch with exactly 5 populated slots), raw 131 is
  never written, and PE touches only `0x1c` and bit 0 of 130. Tempo lives in
  bits PE leaves alone — but anything that starts writing raw 130/131 more
  broadly needs to account for it.

## Editor behaviour

`tools/patch_editor.html` (`LIVE_EDIT_SLOTS = 3`):

* slots 1-3 — live `0x31`, no flash write, nothing else disturbed
* slots 4-6 — labelled as unable to update live; **Apply → pedal** performs the
  read → merge-dirty → bypass-bounce sequence above. Optionally automatic,
  debounced to one apply per gesture (toolbar checkbox)
* incoming `0x31` mirrors the hardware into the editor for every slot, so the
  editor follows knobs turned on the pedal
