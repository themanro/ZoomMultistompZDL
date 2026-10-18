# Custom Effect Layout

This folder is for original effects that are not source-equivalent Airwindows
ports. They still use the same ZDL linker, safe DSP rules, and hardware-test
expectations as [../airwindows/](../airwindows/).

Current custom effects:

| Effect | Notes |
|---|---|
| [ott/](ott/) | OTT-style 3-band upward/downward compressor with `DryWet`, `Time`, `Output`, and `SplitFrq`. Not an Ableton port. |
| [beatrep/](beatrep/) | Experimental BPM-synced beat repeat with continuous, manual one-shot, and four-beat automatic modes. Hardware validation pending. |
| [beatscat/](beatscat/) | Experimental BPM-grid beat scatter with stutter, reverse, or slice-shift treatments and every/alternate/syncopated trigger patterns. Hardware validation pending. |
| [bloom/](bloom/) | Experimental Midiverb II Bloom-patch-inspired diffuse reverb with a transient-swelling wet attack. Original approximation; hardware validation pending. |
| [corrupt/](corrupt/) | Data Corrupter-inspired PLL synth-fuzz with selectable tracked master octave, subharmonic level, harmony interval, and FM Rate/Depth. Uses the Unison root mode; the independent Square voice and eight subharmonic programs are not modeled. |
| [shallow/](shallow/) | Experimental Shallow Water-inspired random-delay modulation with Rate, Depth, Sensitivity, Noise, and Mix controls. Original approximation; hardware validation pending. |
| [rainbow/](rainbow/) | Experimental Rainbow Machine-inspired dual-voice pitch-warping effect with Pitch, Primary, Tracking, Tone, Secondary, and Magic controls. Original approximation; hardware validation pending. |
| [spacerev/](spacerev/) | Experimental Space Station Reverse-mode-inspired reverse grains with LFO-modulated Expression and Rate, Depth, and Mix controls. Original approximation; hardware validation pending. |
| [space2/](space2/) | Experimental Space Station patch-2-inspired fifth-up shimmer reverb with Fifth, Decay, Tone, and Mix controls. Original approximation; hardware validation pending. |
| [particle/](particle/) | Experimental Particle 2-inspired granular delay with eight delay/pitch modes, Size, Density, Delay, Feedback, and Mix controls. Original approximation; hardware validation pending. |

Before adding a new custom effect, read
[../../docs/SAFE-DSP-RULES.md](../../docs/SAFE-DSP-RULES.md). Keep the first
build helper-free, avoid large `.fardata`, and document any product-inspired
behavior as an approximation unless it is truly source-compatible.
