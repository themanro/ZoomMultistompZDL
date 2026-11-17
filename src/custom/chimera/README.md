# Chimera

Your guitar as the voice of a simulated throat and mouth. Like a talkbox,
the guitar takes the place of the vocal folds, so chords come out sung on
vowels; nothing tracks your pitch, so everything you play works.

MS-70CDR, Delay group, effect ID 418. File: [dist/Chimera.ZDL](../../../dist/Chimera.ZDL).

![Chimera's screen](../../../graphics/chimera.png)

## Named after Chimera

This effect is named after, and built on,
[Chimera](https://github.com/jamesondh/chimera-vocal-synth), a
physical-modelling voice synthesizer by Jameson Hodge (MIT licence,
copyright (c) 2026 Jameson Hodge). It is not affiliated with that project.

The synth simulates about a hundred points of airway (lungs, vocal folds, a
branched throat and mouth, nose, yielding walls, turbulence) at 96 kHz,
which is several times what the whole pedal can do. The pedal effect keeps
its acoustic core: the same staggered-grid transmission line in cgs units,
each section's inertance built from the half-sections either side, viscous
loss on the walls, and a Flanagan radiation load at the lips and nostrils,
at 16 sections of throat and mouth plus 6 of nose, at the pedal's 44.1 kHz.

The synth shapes its vowels from a 2D head (tongue, jaw, lips). That doesn't
fit either, so the vowel shapes here are fitted instead: for each of u, o,
a, e and i, a smooth tube shape whose first three resonances land on
measured formants of an adult voice (Peterson and Barney, 1952).
`design.py` fits them and writes `chimera_vowels.h`.

## Knobs

| Knob | Range | What it does |
|---|---|---|
| Vowel | 0–100 | The mouth's shape: u, o, a, e, i from 0 to 100, morphing between them |
| Open | 0–100 | Jaw and lips, closed to wide open. Near 0 the mouth almost closes: with Nasal up, a hum |
| Body | 0–100 | The throat's length, 19 cm (big) to 13.5 cm (small). 30 is an adult man; turn it up for a smaller, higher voice |
| Nasal | 0–100 | Opens the soft palate into the nose: a nasal, "ng" colour |
| Talk | 0–100 | How far Move moves the mouth |
| Move | Pick, Slow, Fast, Chat | **Pick:** each note opens the mouth and moves the vowel on, like a "wow". **Slow** / **Fast:** the vowel sways, about every 3 s or twice a second. **Chat:** babbled syllables, the mouth closing between them |
| Buzz | 0–100 | Drives the guitar into a soft clipper first. More harmonics give the mouth more to shape; a clean guitar with the tone rolled off has little above its fundamental |
| Breath | 0–100 | Air noise through the throat, following your playing: a whisper |
| Mix | 0–100 | Dry/wet |

The guitar is mixed to mono for the throat; the dry signal keeps its stereo.
The level is evened out across the vowels, so sweeping Vowel doesn't jump in
volume (a closed 'u' stays a little softer, as a voice does).

## Screen

On firmware 2.10 the screen shows the airway from the glottis (left) to the
lips (right), as wide as each part is, lit inside by the sound in each
section. When Nasal is up, the soft palate's opening and the way up to the
nose show above. The header shows Move and the nearest vowel. On other
firmware the still cover stays and the audio works the same.

## DSP

Version 0.01 froze the pedal (installed together with Horizon 0.01); its
audio function used a 900-byte stack frame, three times anything proven on
the pedal. 0.02 keeps the frame at 280 bytes (build/stack_guard.py now
fails anything over 300).


About 4,800 emulated cycles per callback, 5,400 with the nose open, near
Deform's. The screen's drawing is spread over six callbacks so no single
one runs long.

## Building

    python3 src/custom/chimera/build.py

Writes `dist/Chimera.ZDL` and its Effect Manager thumbnail. `host.c` runs the
same DSP on a computer (WAV in, WAV out, optional screen frames, one knob
turned mid-run); see its header. `design.py` refits the vowel shapes (a few
minutes).
