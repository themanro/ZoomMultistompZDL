#!/usr/bin/env python3
"""Write EFFECTS.md: every effect with its pedal screen, its editor picture,
what it does and its knobs.

The knobs are read from tools/effects_db.json (built from the dist/ binaries,
so they are what the pedal shows); the pictures are graphics/<name>.png
(build/make_gallery_png.py, the pedal's screen) and graphics/fx/<name>.webp
(build/make_fx_previews.py, the editor's animated picture). The words are
below. Run it after adding an effect or changing one's knobs:

    python3 build/make_effects_page.py
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build"))
import readme_art as A  # noqa: E402

# name -> (what it is, its own README or None, note or None)
OURS = {
    # Delays and loopers
    "Spiral": ("A tempo-locked delay whose repeats climb or sink in pitch, compounding with every repeat; Glide adds a rise that starts at each note.", None, None),
    "Hydra": ("Two extra playheads on one delay: a double-time ghost an octave up and a half-time drag an octave down, locked to tempo. Built for drums.", None, None),
    "Mangle": ("A delay with three blendable mutations on every repeat: crush, tremolo and an octave-down shimmer. All three at 0 is a clean delay.", None, None),
    "Spool": ("A tempo-locked tape echo with flutter, wow, head wear, drive and a spring tail. Feed self-oscillates above about 90.", None, None),
    "Dissolve": ("A glitch delay after the OBNE Parting: chunks of what you play are dropped, smeared into a wash, reversed or dropped an octave.", None, None),
    "Chopdly": ("A delay that chops what it recorded: every repeat replays the last cycle cut into 1-16 chops (or at your notes), rearranged, reversed, pitched and stretched, and the repeats get re-chopped.", "src/custom/chopdly/README.md", None),
    "Droplets": ("A granular delay: short grains sprayed from the last 2.5 s, with pitch steps and patterns, scan, randomness and feedback. Feedback at 100 freezes the buffer.", "src/custom/droplets/README.md", None),
    "Gyre": ("A loop you hold that keeps spiralling: two grains half a window apart make it unity-gain, so Decay at 0 holds for ever and Rise climbs or sinks it each pass.", None, None),
    "Stasis": ("A footswitch freeze: the note you just played is held while you play over it.", None, None),
    "Loop": ("A looper on the footswitch: record, play, overdub, stop, clear, undo and redo, with tempo quantize and a click. About 5 s, or about 23 s in the experimental Long mode.", "src/custom/loop/README.md", None),
    # Reverbs
    "Galactic": ("Airwindows' lush Galactic reverb, its wet path boosted for pedal levels.", None, None),
    "Rooms": ("A multi-mode reverb after the DBA Rooms: Room, Digit, Peak, Gate, Wave and Gong, with Freq and Depth reused per mode.", None, None),
    "Horizon": ("A huge, spacey reverb: eight long, slowly moving lines, from a room to a cathedral, a minute-long tail and a hold, a swell that blooms each note in.", "src/custom/horizon/README.md", None),
    "HorizonL": ("Horizon at half the sample rate inside: the same space for about half the DSP, for chains where Horizon crackles. Hi tops out at 10 kHz.", "src/custom/horizonl/README.md", "Not yet tried on the pedal."),
    # Pitch, modulation and texture
    "Microlm": ("Microloom: a granular shimmer cloud. Grains are pitched and fed back, so it builds rather than repeats.", None, None),
    "Arrakis": ("A Dune-style detuned sub-octave drone: two subs beating against each other.", None, None),
    "Taffy": ("A tape warp after the Red Panda Tensor: reverse, stop, double speed and random slips.", None, None),
    "GenLoss": ("Tape and VHS generation loss: wow, a head-bump tone shift and hiss.", None, None),
    "Shatter": ("A stutter and beat-repeat glitch for drums.", None, None),
    # Drive and lo-fi
    "Scorch": ("An aggressive high-gain amp with a cab-style filter.", None, None),
    "Dustbox": ("A Buzz Box-style fuzz with a flip-flop sub-octave. Contributed by Marcelo Fuentes (MIT).", None, None),
    "Corrupt": ("A Data Corrupter-style square synth: the note you play, a tracked sub and a divided harmony.", None, None),
    "Oxide": ("Airwindows ToTape9: tape saturation with Dolby-style tilt, bias, flutter and head bump.", None, None),
    "Deform": ("Drive, crush and compression in one, after the Torso S-4: two-band drive, a soft crush and a one-knob compressor, with noise that follows your playing.", "src/custom/deform/README.md", None),
    "Rewire": ("Five lo-fi blocks (crush, drive, ring mod, comb, frequency shift) in a chain you can reorder.", None, None),
    "Tonic": ("A Bitters-inspired multi-effect: drive, a phaser and one of decimate, crush, FM or ring mod, in either order.", "src/custom/tonic/README.md", "Experimental: not yet tried on the pedal."),
    # Filters, resonators and voices
    "Flower": ("A Korg-style random sample-and-hold step filter: the \"Digital Bath\" sound.", None, None),
    "Klang": ("A swept-carrier ring modulator: sirens, dive bombs and clanging metal.", None, None),
    "Howl": ("A Total Sonic Annihilation-style feedback loop that self-oscillates on demand.", None, None),
    "Carillon": ("A resonant filter bank: 32 narrow filters from C2 to C8, snapped to a key and scale. A filter at low Decay, ringing bells turned up.", "src/custom/carillon/README.md", None),
    "Chimera": ("Your guitar as the voice of a simulated throat and mouth, like a talkbox: vowels, jaw, body size, nose, and a mouth that can follow your notes or babble.", "src/custom/chimera/README.md", None),
    # Live screens
    "Scope": ("A live oscilloscope of your signal, full screen: waveform or XY. Passes the sound through.", None, None),
    "Spectrum": ("A live spectrum analyser, 40 Hz to 16 kHz, with peak hold. Passes the sound through.", None, None),
    "Ring": ("A spinning ring that follows your signal, full screen. Passes the sound through.", None, None),
    "Skull": ("A shaded 3D skull that reacts to your playing: the jaw opens, the eyes light, it jolts on attacks and explodes at the loudest. Passes the sound through.", None, None),
}
GROUPS = [
    ("Delays and loopers", ["Spiral", "Hydra", "Mangle", "Spool", "Dissolve", "Chopdly", "Droplets", "Gyre", "Stasis", "Loop"]),
    ("Reverbs", ["Galactic", "Rooms", "Horizon", "HorizonL"]),
    ("Pitch, modulation and texture", ["Microlm", "Arrakis", "Taffy", "GenLoss", "Shatter"]),
    ("Drive and lo-fi", ["Scorch", "Dustbox", "Corrupt", "Oxide", "Deform", "Rewire", "Tonic"]),
    ("Filters, resonators and voices", ["Flower", "Klang", "Howl", "Carillon", "Chimera"]),
    ("Live screens", ["Scope", "Spectrum", "Ring", "Skull"]),
]
MATUJUICE = {
    "WaveFold": "A wavefolder on a Buchla 259 fold curve.",
    "DualShft": "A tempo-synced dual pitch shifter: two voices, each with its own echo time, bent apart by one LFO.",
    "Choral": "A choir that sings the note your synth plays: sections, sizes, chords and vowels.",
    "EuGate": "A Euclidean rhythm gate.",
    "DubSiren": "A dub siren with its own tape echo, played from the footswitch.",
    "SGNL": "S.GN_L, a broken digital line: lost packets, dropouts and codec damage.",
    "Scrub": "Scrub through the last seconds and freeze where you stop.",
    "GridDly": "One tempo-synced delay with six engines.",
    "SyncEQ": "An EQ that passes the bar sync on to the effects after it.",
    "Breather": "A tempo-synced pump with a reverb.",
    "DirtBox": "A distortion box for acid lines: seven classic pedal models, EQ and a noise reducer.",
    "Sweep": "A phaser, flanger and filter in one, on a tempo-locked LFO.",
}
DB_NAME = {"SGNL": "S.GN_L"}


def knobs(db, name):
    e = next((x for x in db["custom"] if x["name"] == DB_NAME.get(name, name)), None)
    return ", ".join(p["name"] for p in e["params"]) if e else ""


def card(db, name, text, readme=None, note=None):
    low = name.lower()
    title = f"[{name}]({readme})" if readme else name
    extra = f"<br><em>{note}</em>" if note else ""
    return (f'<tr>\n<td width="300"><img src="graphics/{low}.png" width="300" alt="{name} on the pedal\'s screen"><br>'
            f'<img src="graphics/fx/{low}.webp" width="300" alt="{name} in the editor"></td>\n'
            f'<td>\n\n### {title}\n\n{text}{extra}\n\n<sub>{knobs(db, name)}</sub>\n\n</td>\n</tr>')


def main() -> int:
    db = json.loads((ROOT / "tools" / "effects_db.json").read_text())
    missing = [n for n in A.PACK + A.EXPERIMENTAL if n not in OURS]
    assert not missing, f"no words for {missing}"
    out = ["<!-- generated by build/make_effects_page.py: edit the words there, then rerun -->",
           "# The effects", "",
           f"{len(OURS)} custom effects for the Zoom MultiStomp, plus Luca Matturro's "
           f"{len(MATUJUICE)} [below](#matujuice-pack). Each shows its screen on the pedal and, "
           "under it, its picture in the "
           "[MultiStomp Editor](https://themanro.github.io/ZoomMultistompZDL/tools/editor.html), "
           "which plays out what the effect does and redraws as you turn its knobs. Knob names "
           "are listed as the pedal shows them. Install them from the editor's "
           "**Effects Library**, or from [dist/](dist/) with Zoom Effect Manager.", "",
           "Every effect ends with a **Mix** knob (dry/wet), except Dustbox (Power) and the "
           "live screens (they pass the sound through). Sound demos, desktop previews and "
           "build notes are in [CUSTOM_EFFECTS.md](CUSTOM_EFFECTS.md).", "",
           "**Jump to:** " + " · ".join(f"[{g}](#{g.lower().replace(',', '').replace(' ', '-')})" for g, _ in GROUPS)
           + " · [Matujuice pack](#matujuice-pack)", ""]
    for g, names in GROUPS:
        out += [f"## {g}", "", "<table>"]
        out += [card(db, n, *OURS[n]) for n in names]
        out += ["</table>", ""]
    out += ["## Matujuice pack", "",
            "Twelve effects by **Luca Matturro** ([matujuice](https://github.com/matujuice)), built on this "
            "project's toolchain and included unmodified under his MIT licence: "
            "[zoom-ms-zdl-effects-pack](https://github.com/matujuice/zoom-ms-zdl-effects-pack) v1.5, whose "
            "Tempo knobs run on stock firmware (later releases take their tempo from "
            "[MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp)). Made for synths and drum machines "
            "as much as guitar. The pedal screens are his artwork; his full guide and licence are in "
            "[dist/matujuice/](dist/matujuice/README.md). Their IDs are 480 and 485-496.", "", "<table>"]
    out += [card(db, n, t) for n, t in MATUJUICE.items()]
    out += ["</table>", ""]
    (ROOT / "EFFECTS.md").write_text("\n".join(out))
    print(f"  {len(OURS)} + {len(MATUJUICE)} effects -> EFFECTS.md")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
