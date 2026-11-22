# Ceiling — how many cycles one effect can use

FXID 918, Delay, version 0.01. A probe, never in `dist/`. Built to
`build/probes/ceiling/Ceiling.ZDL` (+ `.png`).

It answers the question a bigger NAM model depends on: how much processing
the stock firmware leaves to one effect, and what a memory access really
costs. The estimate so far (51–62k cycles per 8-sample callback, from where
full-rate NAM crackled and 6/8 rate didn't) is a fit, not a measurement:
[NAM-RUNTIME-INVESTIGATION](../../../docs/NAM-RUNTIME-INVESTIGATION.md).

## What it does

It plays a 1 kHz test sine at -12 dB (Tone 1, the default) or passes the
input through (Tone 0), and each callback burns **kCyc × 1,000 emulated
cycles**:

| Mode | The burn | Real cost |
|---|---|---|
| 0 | a float multiply-add chain in registers | = emulated (the C674x is statically scheduled, nothing waits on memory) |
| 1 | one load per 128-byte line over 64 KB of the arena | emulated + L2 hits (L1 misses) |
| 2 | the same over the whole arena (~690 KB, more than the 128 KB L2) | emulated + a miss to DDR on every load |

kCyc goes to 120 (120,000 cycles) and **defaults to 0**, so installing it
costs nothing. It declares a DSP cost of 5 so the pedal admits it anywhere;
the declared cost has nothing to do with what it burns.

## Running it (the spare pedal)

Back up first. Monitor at low volume: the sine is loud.

1. Install `Ceiling.ZDL` with Effect Manager; close Effect Manager.
2. A patch with **Ceiling alone**, Mode 0, Tone 1. Raise kCyc a step at a
   time. Note the **first value where the sine clicks or crackles**, and the
   last clean one. Wait a few seconds at each step: overruns can be rare.
3. The same with Mode 1, then Mode 2.
4. **Next to NAM:** add the full-rate NAM loader effect to the same patch
   (any capture, Mix 100, playing guitar so the model runs), Ceiling after
   it with Tone 0. Raise kCyc until it crackles. That is the room left
   beside today's NAM, i.e. how much bigger a model could be on stock
   firmware.
5. Optional, with a screen effect: one of the live-screen effects in the
   chain as well. The screen redraw costs time too.

Report the numbers in a table:

| Test | Last clean kCyc | First crackle kCyc |
|---|---|---|
| Mode 0 alone | | |
| Mode 1 alone | | |
| Mode 2 alone | | |
| Mode 0 next to NAM (full rate) | | |

## What the numbers mean

- **Mode 0 alone** × 1,000 + ~470 (the probe's own base cost) = the cycles per
  callback the firmware leaves to effects. ÷ (8 samples / 44,100) gives the
  clock left to effects in Hz.
- **Mode 1 and 2** against Mode 0: each kCyc in Mode 1 or 2 is 1,000 / 7 =
  ~143 loads. If Mode 2 crackles at a kCyc of M2 and Mode 0 at M0, a DDR miss
  costs about 7 × (M0 / M2 − 1) cycles (the same for Mode 1 with L2).
- **Next to NAM:** today's NAM is ~15,300 emulated cycles per callback plus
  its memory stalls; the room left in Mode 0 cycles sizes a bigger model
  (about 2 multiply-adds per cycle at best).

## Checking a build

    python3 -B src/hardware_probes/ceiling/validate.py

Measures each loop's emulated cycles per iteration in Ziddle, writes
`calib.json`, rebuilds, and checks that every mode costs base + 1,000 × kCyc
(it does: base 472, 1,000.0 per kCyc in all three), that the tone plays and
that pass-through is clean. Stack frame 0 bytes; no `.switch`, `.bss` or
`.text`; no external symbols.
