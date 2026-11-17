# HorizonL

[Horizon](../horizon/README.md) with its reverb running at half the sample
rate. The same space, the same knobs and screen, for about half the DSP and a
quarter of the memory traffic: use it in a chain where Horizon crackles
(Horizon next to Carillon does). Horizon stays as it is.

MS-70CDR, Delay group, effect ID 420. File: [dist/HorizonL.ZDL](../../../dist/HorizonL.ZDL).

## What's different from Horizon

* The diffusers, the eight lines and their damping run once per pair of
  samples (22.05 kHz). In: a [1 2 1]/4 filter, then every other sample.
  Out: linear upsampling back to 44.1 kHz. The dry signal is untouched.
* **Hi** follows Horizon's curve but stops at 10 kHz (Horizon: 16 kHz),
  and the half-rate filters darken the tail a little more at the top.
* Reverb times are within about 5% of Horizon's at the same Decay.
* Memory: about 165 KB of the effect's memory (Horizon 330 KB), half of it
  read per second. Clearing it at load takes about 4 ms.

## Knobs

As Horizon: Size, Decay (100 holds), Pre (0–370 ms), Mod, Lo (20–600 Hz),
Hi (1.5–10 kHz), Swell, Width, Mix.

## DSP

About 5,300 emulated cycles per callback (Horizon 7,400), 8,300 at the
screen's heaviest step; declared cost 35. Stack frame 264 bytes.

## Building

    python3 src/custom/horizonl/build.py

`host.c` runs the same DSP on a computer, as Horizon's.
