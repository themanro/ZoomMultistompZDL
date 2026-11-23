# Changelog

What changed, newest first. The project has no version numbers; entries are
dated. Effects in [dist/](dist/) carry their own version in the file
(shown in Zoom Effect Manager).

## Unreleased

### Editor
- **Remove several of Zoom's effects at once** (Effects → Zoom's effects on the
  pedal): tick them (Shift-click ticks a range) and click Remove selected. One
  confirm, the effect list written once (nothing is deleted if that write
  fails), then the files deleted. It won't remove the last effect. For making
  room on a full pedal, for example after a factory reset puts all of Zoom's
  effects back.
- **Live edits in slots 4-6 on MIDISTOMP 2.0.4 and later.** Its SysEx knob
  message now reaches all six slots (identity `M204` and up), so the editor turns
  knobs in slots 4-6 live instead of rewriting the whole patch. Earlier firmware
  is unchanged. Not yet tried on a pedal (2.0.4 isn't released yet).
- **Installing on the MS-50G** (experimental): installing, updating and removing
  effects over USB, unlocked once "Enable writes (experimental)" passes its
  write test, with one extra confirm per session. Until now the MS-50G could
  only read and back up. Its storage is the MS-70CDR's (the 3.10 updater's file
  system has the same header: 700 data blocks, 200 file slots; Zoom's effects
  fill 400 of them). Not yet tried on an MS-50G.
- **The MS-50G's amps are read as effects.** Their files carry a cabinet chunk
  ('CABI') that moves the code from byte 0x4C to 0x14C. The editor and the
  updater reader assumed 0x4C, so the MS-50G updater wouldn't open ("not a
  ZDL") and its 8 amps were missing from the Zoom's effects list and the effect
  ID check. Both now take the header length from the header.
- **MIDISTOMP support** ([docs/MIDISTOMP-COMPAT.md](docs/MIDISTOMP-COMPAT.md), from
  reading MIDISTOMP 2.0.1's patches and sources; not yet tried on a pedal running it).
  The editor reads MIDISTOMP's tag in the identity reply (`M201`) as "MIDISTOMP 2.0.1"
  instead of warning that the live screens need firmware 2.10, and its log notes that
  renaming a patch or changing its effects resets that patch's MIDISTOMP mixer and
  PERFORM settings. A slot swap now moves MIDISTOMP's bus bits (byte 17, bits 0-1) with
  the effect and leaves the slot position's mixer bits (2-6) in place. The live screens
  need no change: MIDISTOMP leaves the firmware words they check alone, and its own
  screens set the UI mode they already test. Asked of Luca
  ([matujuice/zoom-ms-midistomp#72](https://github.com/matujuice/zoom-ms-midistomp/issues/72)):
  live `0x31` edits for slots 4-6.

### Probes
- **Ceiling** (`src/hardware_probes/ceiling`, FXID 918, never in `dist/`): burns
  kCyc x 1,000 emulated cycles per callback, in registers, through L2 or
  through DDR, until the pedal crackles, to measure the processing left to one
  effect and the cost of a memory access (sizing a bigger NAM model).
  Calibrated in the emulator (1,000.0 cycles per kCyc in all three modes); not
  yet run on a pedal.

### Effects
- **Loop 0.05**: spots MIDISTOMP by the magic numbers of its looper services
  and transport blocks before using the Long regions, instead of by one of
  MIDISTOMP's region addresses (one of which has moved since). Long was
  already off under MIDISTOMP 2.0.1; this keeps it off whatever MIDISTOMP's
  memory layout. Sound unchanged (emulator output identical to 0.04).
- **New effect: Cloud** (Delay 423, version 0.01, not yet tried on the
  pedal), a dense, diffuse ambient reverb. HorizonL's half-rate tank with an
  allpass inside each of its eight lines and five long input diffusers, so
  notes bloom into a wash (the reverb peaks about 300 ms after a note,
  Horizon's at about 115 ms) and the tail turns to fog instead of echoes.
  Knobs: Size, Decay (1 s to 90 s, 100 holds), Pre, Diff (the smear), Mod,
  Tone and Lo (how long the highs and the lows last, set from each line's
  gain so they don't shorten the tail as Horizon's cuts do), Swell, Mix.
  About 6,100 emulated cycles per callback (Horizon 7,400, HorizonL 5,250),
  8,900 at the screen's heaviest step; declared cost 45; 215 KB of the
  effect's memory. Live screen: the eight lines as eight puffs of a cloud.
- **Carillon and Droplets have new effect IDs**: Carillon Delay 421 (was
  497) as version 0.05, Droplets Delay 422 (was 492) as version 0.04. 497 is
  also Matujuice's Looper (GRDLOOP) and 492 Matujuice's Washout, and two effects with
  one ID can freeze a pedal at boot. Only the ID and version bytes changed;
  the old IDs stay as legacy names so old patches still show them. To swap:
  remove the old one, power-cycle, then install
  ([docs/INSTALLING-ZDLS.md](docs/INSTALLING-ZDLS.md)). The editor's install
  check now also knows other collections' effects (`others` in the effect
  DB: Matujuice's releases not in `dist/`, TEcho4, SilverSt, DrumBox,
  SYNX2), so it warns about clashes like these. `dist/index.json` also had
  Loop's previous size and checksum, so the editor wouldn't install Loop
  0.04; fixed.
- **Live screens on the G1on family** (contributed by Barsik-Barbosik,
  [#12](https://github.com/themanro/ZoomMultistompZDL/issues/12)): the
  firmware check and the on-screen test in `src/custom/common/zvis.h` are now
  a table with one entry per firmware, the MS-70CDR 2.10 (same values as
  before) and the G1on family (G1on and G1Xon 1.21, B1on and B1Xon 1.30, one
  firmware). Skull, Ring, Scope, Spectrum and Loop load and animate on a
  B1Xon. On the G1on, Loop's Long mode stays off (its regions are mapped on
  the MS-70CDR only). The linker can leave out the unused 640-byte float
  divide (`drop_unused_divf`, kept when the code calls it), which keeps
  Skull and Chopdly under the size limit. Rebuilt, with new versions: Skull
  0.06, Ring 0.03, Scope 0.04, Spectrum 0.03, Loop 0.04, Chimera 0.03,
  Horizon 0.03, HorizonL 0.02, Carillon 0.04, Droplets 0.03, Chopdly 1.04,
  Deform 0.04. Same sound (emulator output identical to the previous
  builds); on the MS-70CDR the screens should behave as before.
- **HorizonL** (new, Delay 420, version 0.01): Horizon with its reverb tank
  (diffusers, the eight lines, their damping) at half the sample rate, for
  chains where Horizon crackles: Carillon + Horizon crackled on the pedal
  although they emulate at about 19,000 cycles at peak, so the cost is
  Horizon's reads from its 330 KB of lines, which the emulator does not
  charge. HorizonL does half the reads into 165 KB, at about 5,300
  emulated cycles per callback (Horizon 7,400), declared cost 35. Same
  knobs, same screen; reverb times within about 5% of Horizon's; Hi follows
  Horizon's curve but stops at 10 kHz, and the tail is a little darker
  (the half-rate filters). Horizon itself is unchanged. Not yet tested on
  the pedal. [README](src/custom/horizonl/README.md).
- **Horizon** (new, Delay 419, version 0.02): a huge, spacey reverb.
  Pre-delay (to 370 ms), four diffusers, then eight long, slowly moving
  delay lines mixed in a Hadamard feedback matrix: Size from a room to a
  cathedral (turning it bends the tail's pitch), Decay from 0.8 s to
  a minute (measured within 10% of the setting) and a hold at 100, Mod,
  Lo and Hi cuts inside the tail, Swell (each note blooms in over up to
  2 s), Width, Mix. Left and right uncorrelated. On firmware 2.10 its
  screen shows the eight lines as orbits, each lit by its sound, and the
  reverb time. About 6,000 emulated cycles per callback, plus the memory
  traffic of its 330 KB of lines (declared cost 50). 0.01 used 650 KB and
  a 432-byte stack frame and was on the pedal when it froze; 0.02 halves
  the memory and keeps the frame at 260 bytes. 0.02 loads and runs on the
  pedal (2026-11-15), but next to Carillon the chain crackles: see HorizonL.
  [README](src/custom/horizon/README.md).
- **Chimera** (new, Delay 418, version 0.02): your guitar as the voice of a
  simulated throat and mouth, like a talkbox, named after and built on the
  Chimera physical-model voice synth (github.com/jamesondh/chimera-vocal-synth,
  MIT, by Jameson Hodge).
  A 16-section airway plus a 6-section nose at 44.1 kHz, with the guitar as
  the glottal source; vowel shapes fitted to measured formants (u o a e i).
  Knobs: Vowel, Open, Body, Nasal, Talk, Move (Pick, Slow, Fast, Chat),
  Buzz, Breath, Mix. On firmware 2.10 its screen shows the airway and the
  sound inside it. About 4,800 emulated cycles per callback (5,400 with the
  nose open). 0.01 froze the pedal (installed with Horizon 0.01): its audio
  function used a 900-byte stack frame; 0.02 uses 280. 0.02 loads and runs
  on the pedal (2026-11-15). [README](src/custom/chimera/README.md).
- **Lighter on the DSP, same sound: Deform 0.03, Carillon 0.03, Droplets
  0.02.** Their audio is bit-identical to the builds before (checked in the
  emulator on 12 knob settings).
  - **The screen no longer spikes.** Each drew most of a frame in one
    callback, 30 times a second, which put single callbacks far over their
    usual cost (Deform up to 19,300 emulated cycles against about 6,000).
    With the chain near the DSP limit that one long callback is a click.
    The drawing is now spread over more callbacks: Deform's worst is 8,900,
    Carillon's 9,400 (was 10,900).
  - **Deform**: its per-sample reciprocals use the pedal's reciprocal
    instruction instead of three software steps: 6,020 emulated cycles a
    callback at the defaults (was 7,430), 7,700 at the heaviest (was 8,870).
    Declared DSP cost 45 (was 25), in line with what it uses.
  - **Droplets**: up to six grains it plays as before; with seven or eight
    all voices run straight through without branching. 2,780 at the
    defaults (was 3,120), 5,740 with Rate and Size at maximum (was 7,030).
  - **Carillon**: 6,250 (was 6,510).
- **Galactic 1.02**: declared DSP cost 30, was the build default 20 (3,400
  emulated cycles plus the memory traffic of its 528 KB of delay lines).
- **Galactic 1.01**: knob turns no longer interrupt the sound. Knobs now
  glide instead of jumping (about 10 ms; Bigness about 60 ms, since it moves
  every delay length at once). An off shorter than 60 ms no longer cuts the
  tail, so the Patch Editor's off-and-on in slots 4–6 can't be heard; a real
  bypass takes effect 60 ms later. The added smoothing and holdover are the
  pedal's, not Airwindows'. Not yet tested on the pedal.
- **Deform** (new, Delay 417, version 0.01): drive, crush and compression
  after the Torso S-4's Deform, in its order: noise that follows your
  playing, a two-band drive (Tilt, Split), a crush made to stay soft (Mode:
  Bit with rounded steps, squeezed quiet signals and its error fed back;
  Redux with straight lines between held samples and a filter after; or
  Both), and a one-knob compressor. Level held about even across Drive and
  Crush. On firmware 2.10 its screen shows the drive and crush curve, the
  gain reduction and the wet signal. Not yet tested on the pedal.
  [README](src/custom/deform/README.md).
- **Carillon** (new, Delay 497, version 0.02): a resonant filter bank after
  the Torso S-4's. 32 band-pass filters from C2 to C8, snapped to a Key and
  Scale (named steps), with Cutoff, Reso, Slope (low-pass to high-pass),
  Decay (from one plain filter to bells ringing for seconds), Waves (a
  ripple across the bands) and Noise (bands struck on their own, then a
  hiss). On firmware 2.10 its screen shows a bar per band and the filter's
  shape. [README](src/custom/carillon/README.md). 0.02: Noise strikes no
  longer click (each struck band now starts its ring from silence instead
  of jumping), heard on the pedal at high Noise.
- **Droplets** (new, Delay 492, version 0.01): a granular delay. Up to 8
  grains at once from the last 2.5 s of your playing, with Pitch and Pattern
  as named steps, Rate, Size, Contour, Scan, Random (spray, pitch jumps,
  length/timing/level, reversed grains), Feedbk, and Lock (Feedbk 100)
  to freeze the buffer. On firmware 2.10 its screen shows the buffer and the
  grains. Not yet tested on the pedal.
  [README](src/custom/droplets/README.md).
- **Chopdly** (new, Delay 947, version 1.03): a delay that cuts the last
  cycle into chops (equal, or at note attacks, cut at the quiet point just
  before each) and plays them back
  rearranged, reversed, pitched and stretched; the repeats feed back and get
  re-chopped. Chops, Order, Pitch, Reverse and Stretch are named steps, shown
  by name on the pedal and as dropdowns in the Patch Editor. Reverse's Ping
  and Scatr steps pan the chops (alternating L/R, or a fixed place per
  chop); otherwise the chops are mono. Glide sets how chops join, from hard
  cuts to long overlapping crossfades. On firmware 2.10
  its screen shows the recording, the cuts, the chop playing and the current
  steps. [README](src/custom/chopdly/README.md).
- **Spool** has a new effect ID, 914 (was 409), as version 1.02; the sound is
  unchanged. TEcho4.ZDL from another collection used 409 too, and swapping
  the two in Zoom Effect Manager froze an MS-70CDR at boot until its ROM was
  reprogrammed. To update: remove the old Spool, power-cycle, then install
  the new one ([steps](docs/INSTALLING-ZDLS.md#spools-new-effect-id-29-october-2026)).
  Patches with the old Spool need it picked again. The Patch Editor still
  reads them (as "Spool (old 409)"), can remove the old file, and refuses to
  write the new ID over it in place.

### README and docs
- **All effects on one picture**: the README's eight sample pictures are
  replaced by [graphics/fx-all.webp](graphics/fx-all.webp), all 47 editor
  animations playing together, labelled, linking to EFFECTS.md
  (`build/make_fx_sheet.py`). `--video` on it and on
  `build/make_editor_preview.py` records the same sheet (3840x2160) and the
  editor (1920x2000) as 15-second H.264 MP4s for social media, encoded by
  macOS's own encoder (`build/encode_mp4.swift`); they go to `video/`, which
  is not committed.
- **README rewritten and shortened** (786 to 479 lines): a new title, a
  four-step Get started, an editor section with a moving preview, the
  effects in one table, NAM condensed, the Zoom Effect Manager steps without
  screenshots, one compatibility table. Dropped: out-of-date notes, the old
  effect counts, and "Not yet tested" on effects that now run.
- **[EFFECTS.md](EFFECTS.md)**, a new page with every effect: its screen on
  the pedal, its moving picture from the editor, what it does and its knobs
  (read from the effect database). Luca Matturro's Matujuice pack has its
  own section there and is no longer in the README's compilation picture.
  Generated by `build/make_effects_page.py`.
- **Pedal screens as the pedal shows them**: the compilation picture and
  each effect's picture are now dark ink on the light screen, not inverted
  (`build/make_thumb_png.py`, `build/make_gallery_png.py`). The live screens
  stay dark, as on the pedal.
- **Moving pictures**: `build/make_fx_previews.py` records each effect's
  editor picture as a looping WebP (`graphics/fx/`), and
  `build/make_editor_preview.py` records the editor itself with an example
  bank (`graphics/editor.webp`); both run the real page in headless Chrome.

### MultiStomp Editor (was Patch Editor)
- **Patches the editor can't read yet** ([#15](https://github.com/themanro/ZoomMultistompZDL/issues/15)):
  on a G1on-family pedal (134-byte patches) Reload bank said "no reply from
  slot(s) 1–50" although the pedal answered every slot, and it waited out
  the timeout on each, about a minute in all. It now says the pedal answered
  in a format the editor can't read yet, moves on as soon as the answer
  arrives (a few seconds), and Copy diag adds every such patch in full (the
  MIDI list cuts each message at 32 bytes): the data needed to map those
  pedals. The warning about the patch length is shown once, not once per slot.
- **Connect on a G1on** ([#13](https://github.com/themanro/ZoomMultistompZDL/issues/13)):
  Connect stopped with "Patch changed on the pedal during this operation"
  when the pedal sent a program change of its own while connecting; until
  Connect's first read is done, one now only says which patch the pedal is
  on. Connect also waits for the pedal's identity reply (up to 0.8 s, was a
  fixed 40 ms) before reading, since a pedal ignores reads addressed to
  another model, and finds the pedal's port by "ZOOM" rather than "ZOOM MS".
  When a pedal sends a patch in a layout the editor hasn't mapped (anything
  but 146 bytes), it says so and asks for Copy diag, instead of suggesting a
  power-cycle. Tested with a simulated G1on and MS-70CDR, not on a G1on.
- **Check effect files before installing** (Effects): choose effect files,
  or the folder you install from with Zoom Effect Manager, and it lists the
  ones that can't go on one pedal together: two files with one group and
  effect ID (which can freeze the pedal at boot), a file with the ID of one
  of Zoom's own effects or of this project's, file names that match in the 8
  characters the pedal keeps, two versions of one effect, and local IDs
  (Delay 958–1023). Works without a pedal; after Read the pedal, the effects
  on it are checked too. Read in the browser, nothing uploaded
  ([#14](https://github.com/themanro/ZoomMultistompZDL/issues/14)).
- **Renamed the MultiStomp Editor** in both editors, the launcher, the NAM
  loader, the issue forms and the docs. File names are unchanged
  (`tools/editor.html`, `tools/patch_editor.html`), so links keep working.
- **Every effect has its own picture.** Each of our effects and Luca
  Matturro's Matujuice pack now has a scene of its own in
  `tools/fx_visuals.js` that follows its knobs (Deform keeps its hatching;
  nothing else borrows it). Skull's picture is the pedal's own skull,
  rendered by `src/custom/skull/pe_frames.py`. `tools/fx_gallery.html` shows
  any of them at three knob settings (`?fx=Spiral,Hydra`).
- **The new design is the default editor** (`tools/editor.html`): the README,
  the launcher and the server open it. The classic editor is in Settings →
  Editor, and each editor links to the other there.
- **Drawers close on a click beside them**, not only with ✕ (or Esc):
  click the dimmed page to dismiss Library, Effects, NAM / Cab or Settings.
- **Remove Zoom's own effects** (Effects on the pedal → Zoom's effects on the
  pedal), listed apart from this project's: each with its group and the
  blocks it takes, and a Remove button. Zoom's effects fill about 683 of the
  pedal's 700 blocks, so removing ones you never use is how to make room.
  Only effects whose ID is in Zoom's stock list (IDs up to 1023, so another
  collection's larger, aliasing IDs never count) and that are in the pedal's
  effect list are offered; files the pedal keeps outside that list are not.
  The plain Remove still refuses them. Put them back from Zoom's updater or
  a backup. Not yet tried on the pedal.
- Installing or updating an effect now checks room the way the pedal
  stores files: whole 4096-byte blocks (4090 bytes of data each) and a
  limit on files. MS-70CDR 2.10: 700 blocks and 200 file slots, of which
  Zoom's own effects fill 683 blocks (read from the 2.10 updater with
  Ziddle's firmware tools). It refuses an install that would leave fewer
  than 3 blocks free. The Effects Library's scan line shows blocks and
  slots in use. The byte count the pedal reports hid the rounding: on
  2026-11-13 the pedal sat at 685 of 700 blocks, Chimera and Horizon need
  13, and installing both froze it (twice).
- Each slot shows an animation of its effect that follows the knobs: it
  moves on its own and reshapes as you turn (from the editor or on the
  pedal). Scenes by kind (waveform, filter-bank bars, hatched drive, delay
  rings and reverb box, modulation waves, filter curve, steps, pitch lines,
  dynamics curve), each of this project's effects with its own knobs wired
  in (Deform's crush steps its outline, Carillon's bars follow its Scale,
  Chopdly's waveform is cut into its chops). Zoom's effects get one by
  category. A Screen / Animation button switches every slot back to the
  pedal-screen picture; the choice is remembered. A slot that is off holds
  its picture still (dimmed). When the system asks for reduced motion the
  scenes hold still until you press Play, which overrides it and is
  remembered. `tools/fx_visuals.js`. Up to 60 frames a second, so knob
  turns glide.
- Deform's animation is drawn pixel by pixel, after the S-4's: two hatched
  circles (low and high band) that pulse with the playing. Drive eats holes
  and dashes into the lines, Noise breaks off fading fragments and dust, and
  Crush steps the picture: Redux across, Bit down, Both into blocks.
- The Pause/Play and Screen buttons on a slot's picture show only while
  the mouse is over it (or a button has keyboard focus; always on touch
  screens).
- Rewire's animation shows its chain: the five blocks as tiles in the order Route picks (sliding into place when it changes), each showing a test waveform as it is at that point in the chain, its knob as a bar under it, Tone at the end and the dry path over the top. Horizon's: its eight lines as orbits around the horizon, lit by each note after the pre-delay, fading as Decay says.
- Chimera's animation: its airway, the same vowel shapes as the pedal, with sound running from the glottis to the lips, the nose branching off when Nasal is up, and the mouth moving as Move moves it.
- New animations for Carillon, Droplets and Galactic, in the same style.
  Carillon: a tall dim bar per band is the filter's shape, and a bright bar
  inside it is what comes through, with struck bands ringing for as long as
  Decay says. Droplets: the 2.5 s buffer along the bottom (frozen and marked
  LOCK at Feedbk 100) and each grain above it at its pitch and read point,
  a little waveform shaped by its envelope (Contour), pale when reversed.
  Like Deform, they use two colours: the screen colour made vivid (the
  filter shape and ringing bands, the grains, the faint stars) and a pale
  one. Galactic: a turning
  galaxy, as big as Bigness, lit by a ring that runs out from each note,
  with trails as long as Replace leaves them; Detune makes the arms waver
  and Bright makes the stars sharp or soft.
- Clicking a slot in 1–3 no longer writes the patch when the pedal's screen
  has already followed the live nudge, so it no longer blips the sound.
  Slots 4–6 still need the write (a short blip).
- The update after a knob move in slots 4–6 runs in the background: the
  buttons no longer grey out and the knobs stay usable while it runs. The
  slot now stays off for 20 ms instead of about 150 ms (one write fewer),
  so the sound gap is much shorter. The length is in Settings > Diagnostics
  ("Slot 4–6 update gap"); raise it if knobs in slots 4–6 stop changing the
  sound.
- Clicking a slot's title or picture now writes the patch back with that
  slot as the current effect, which should move the pedal's screen in all
  six slots (the old nudge couldn't reach slots 4–6). Not yet confirmed on
  the pedal; the log shows what the pedal reports back.
- When an MS-70CDR reports firmware other than 2.10, the effects panel and
  a notice say that live screens need 2.10 and link to Modwerk for the
  update. The editor doesn't flash firmware itself.
- **Effects**: install, update and remove this project's effects over USB
  on the MS-70CDR, without Zoom Effect Manager. It shows the version on the
  pedal next to the one available, refuses Zoom's own effects and any name
  or effect-ID clash, checks downloads against `dist/index.json` (written by
  `build/dist_index.py`), reads back every write, writes the effect before
  the effect list, and backs up all the pedal's files on request. Tested on
  the owner's pedal (2026-10-23): remove, power-cycle, install, power-cycle.
- **Zoom's effects from Zoom's updater**: in Effects, choose the firmware
  updater you downloaded from Zoom (the Mac `.zip`, or its `FS.bin`). The
  page unpacks it and strips each effect as Effect Manager does
  (`tools/zdl_strip.js`, byte-identical to TI's strip6x on all 137 MS-70CDR
  2.10 effects), all in the browser, nothing uploaded. It lists the effects
  missing from the pedal or newer than its copy, and installs only those:
  never an older version over a newer one, never in place of another effect.
  Not yet tested on a pedal.
- **NAM captures** in Effects: choose a .nam and a short name, and the
  capture is converted (the NAM loader's engine, Full rate or Eco) and
  installed over USB in one step, as a new capture (the next unused slot ID,
  picked for you) or in place of one already on the pedal, which is removed
  first. Patches using that slot then play the new capture.
  Cabs the same way: choose a .wav IR and a name; it is fitted (the cab
  loader's fit, in a worker) and installed as a new cab or in place of one.
  A DSP estimate shows the capture's share of the pedal (194 or 181 of
  ~228), what still fits beside it, and whether it fits the open patch.
  Captures and cabs on the pedal are listed with Remove. Eco+, split outputs
  and cabs are still made in the NAM + Cab Loader. Not yet tested on a pedal.
- **NAM / Cab** is its own panel (a menu button in both editors): captures and
  cabs on the pedal, install from a file or from **TONE3000** (pick in
  TONE3000's own browser after signing in there; the capture or IR loads
  straight into the page, credited to its creator; once signed in, your
  TONE3000 favourites and pasted tone links load without the popup), the DSP
  estimate, and a
  link to the NAM + Cab Loader for making files. It replaces the NAM section
  in Settings and the loader panel in editor.html.
- **Packs** in NAM / Cab: a TONE3000 tone with several models (gain stages,
  mics), or several files chosen at once, installs as one pack: tick the
  variants, and they get family names (a pack code plus what sets each apart,
  e.g. MP3, MP5, MP10 for Gain 3/5/10, distinct on the pedal's screen). After
  every install the editor groups its captures and cabs in the pedal's effect
  list (pack by pack, variants in number order), leaving every other effect's
  place alone.
- **Variant switcher**: a slot holding a capture or cab from a pack shows
  ◀ name · 2/4 ▶ under its effect; stepping swaps in the next variant of the
  pack (only those on the pedal, once it has been read), keeping the slot's
  knob values, so gain stages can be auditioned in place.
- **[editor.html](tools/editor.html)**: the Patch Editor in Modwerk's design,
  running the same code. In testing; the classic editor stays the default.

### In dist/
- **Loop 0.03**: with Long turned on while a loop is recorded, the screen
  now says **CLEAR** (Long is available once the looper is cleared) instead
  of NO LONG, which now only means Long can't be used here (other firmware,
  or MIDISTOMP installed). Tested on the MS-70CDR 2.10 (2026-10-23).
- **Installing over USB without Effect Manager** works on the MS-70CDR 2.10
  (first tests, 2026-10-23): `tools/file_probe.html` backs up every file
  from the pedal, and replaced Loop 0.03 with 0.02 and back, each loaded
  after a power cycle. See docs/EFFECT-INSTALL-PROTOCOL.md.
- **Loop, Skull, Scope, Spectrum and Ring** now ship in `dist/`, tested on
  the MS-70CDR 2.10 (2026-10-20): Loop 0.02, Skull 0.05, Scope 0.03,
  Spectrum 0.02, Ring 0.02. Their screens need firmware 2.10; elsewhere they
  run as audio effects (Loop) or pass the sound through.

### NAM engine 0.26 (works on the pedal)
- **Eco+**: half-rate NAM, with a correction EQ the loader fits to each
  capture: 152 instead of 194, for amp-only captures into a cab. It sounds
  worse than full rate: use it when the DSP is needed for other effects.
- **Eco** resamples with cubic curves at evenly spaced positions, so its
  whistle-like spurs drop by 30-45 dB. It costs 181 instead of 177.
- **Soft output limiter** instead of the hard clip at full scale.
- **Split outputs**: the amp on one output, the clean guitar on the other
  (for a second amp or a DI). Pick the side in the loader. Knob 3 becomes
  **Dry**, the dry side's level, -24 to +24 dB with 50 = unchanged. Full
  rate only, same file size and DSP cost (194). Works on the pedal.

### Emulator
- Fixed a Ziddle decoding bug (local copy): a masked 16-bit stack store in a
  pipelined loop counted as unit D1 instead of D2, so it was replayed on every
  iteration and overwrote a saved pointer.
- Two more Ziddle fixes (local copy): the 16-bit shift-by-constant decoded
  "shift by 8" as 7 and "by 16" as 0 (45 such instructions in our builds),
  and SPLOOPW loops ran an iteration too many after their exit condition. The split build still disagrees with
  the host in the emulator but plays correctly on the pedal; see
  docs/NAM-RUNTIME-INVESTIGATION.md.

### README art
- New hero: the console painting with all its detail, in the screens'
  pale-blue phosphor, its red LEDs kept (`graphics/make_phosphor_hero.py`).
- The effects overview shows every effect: the pack, the experimental effects
  (live screens as they play), a NAM and a CAB example, and the Matujuice pack
  in a credited band. Thumbnails for all of them in the README.
- The Patch + Cover Editor image is re-shot from the current pages.
- One list of what the README shows, for both generators: `build/readme_art.py`.

### Library
- The **Matujuice effects pack v1.5** by Luca Matturro is included in
  `dist/`, credited, with its licence. The Patch Editor lists it under his
  name.

roughly 225 the pedal allows, Eco 181, Eco+ 152, a cab 10 (0.26).
### NAM + Cab Loader
- **Frequency response**: the capture's EQ curve at three playing levels,
  through any cab in your bank, or a cab's IR against its pedal filter; 48 kHz
  captures can be shown as trained. Measured in the browser
  (`tools/nam_response.js`, matched to the validated C kernel).

### Loop v0.02 (experimental)
- **Tempo quantize**: Tempo (BPM, 40-250) and Beats (per bar; Off = free)
  set before recording snap the loop to whole bars: an early closing press
  records on to the bar line, a late one drops the overshoot and plays on in
  time.
- **Click**: a metronome with a bar accent, in the output only (never
  recorded), with its own level knob.
- Screen shows the BPM, then bar and beat. Matches the host build in the
  emulator in every session tested, and works on the pedal (2026-10-20).

### Visual effects (experimental, MS-70CDR firmware 2.10)
- **Full screen**: Scope v0.03, Spectrum v0.02 and Ring v0.02 draw over the
  whole display, with no knob row; the knobs are on the knob pages.
- **Skull v0.05**: the knob boxes stay hidden. They came back on the pedal in
  v0.04 because the hide ran only once, at start-up; it now runs every callback.
- **Skull v0.04**: idle animations when it's quiet (looks around, spins, a
  snake through the eye sockets and mouth, rolls, melts into dust); a more
  violent explosion of shards cut from its own picture.
- **Spectrum** (FXID 468): a live spectrum analyzer, 1024-point FFT, 126 bars
  40 Hz - 16 kHz with peak hold; Range, Fall and Tilt.

### Loop
- **Loop** (FXID 467): a looper on stock firmware; works on the pedal. The
  footswitch records, plays and overdubs; a double press stops, and a double
  press when stopped clears. Undo and redo of the last overdub; mono, 12-bit,
  ~5 s (Long: ~23 s in RAM MIDISTOMP found unused on 2.10, experimental). Its
  screen shows the state, the time and the loop's waveform with a playhead.
  Guide: [src/custom/loop/README.md](src/custom/loop/README.md).
- README: a section for the looper and the live screen effects.

## 2026-10-18

### NAM + Cab Loader
- **216 capture slots and 216 cab slots** (was 16 each). Slots 1-16 keep
  their effect IDs; captures 17-216 are FXIDs 500-699, cabs 700-899. Slots
  past 16 are made in the browser from one base template per engine
  (`tools/slot_identity.js`); tests check them byte for byte against
  linker-built files.
- **Your name and an icon on the pedal screen.** Pick one of 21 icons (amp,
  cabinet, speaker, tube, mic, skull, flame, ghost…); the pedal-screen picture
  shows it next to the name you typed, and the loader gives you a matching
  Effect Manager thumbnail (`tools/cover_art.js`, `build/capture_icons.py`).
  Every template reserves 1,024 bytes for this picture. Re-export a capture
  to get its name on screen; older files keep working.
- Amp and cab cards now work the same way: name, icon, slot picker and
  preview, with the preview hidden until a file is chosen.
- New filename rules: `NAM<slot><name>` (4/3/2 name letters for slots
  1-9/10-99/100-216); cabs `C<slot><name>` (5 letters, 4 from slot 100).
  Cab names must start with a letter; digit-first names are migrated.

### Patch Editor
- Knows all 432 generated slots, and draws each capture's and cab's own
  name and icon on its card.

### Toolchain
- **Linker fix: calls between code sections.** A call from one code section
  to a function in another landed past its target (the instruction's offset
  was added twice). This froze the pedal when the Blink probe was selected,
  and probably explains an older report on another effect. The linker now
  handles it, or refuses to link when it can't be sure, following Leemuzhko's
  SDK notes. Tonic rebuilds byte-identical.
- The linker can hand an effect the addresses of its own picture and
  picture info (`_<audio>_Pic`, `_<audio>_Info`), so an effect can redraw
  itself.
- Effect DB: generated slots are listed by range rather than one by one.
- Builds of the animated effects and probes write their Effect Manager
  thumbnail too.

### Animated screens (experimental, MS-70CDR firmware 2.10 only)
An effect can now animate its own pedal screen on stock firmware: no
firmware flash. Research and hardware results:
[docs/pedal-research/animated-covers.md](docs/pedal-research/animated-covers.md),
with thanks to Alexander Lim (Leemuzhko) for the SDK notes that pointed the
way.
- **Probes** (`src/hardware_probes/redraw`, `coverrw`, `blink`): proved
  that the screen redraws only on events, that an effect can rewrite its
  own picture, and that an effect can ask for a redraw. All three were run
  on the pedal.
- **zvis kit** (`src/custom/common/`): draw a 128×64 frame, encode it a
  little per audio block, show it at ~30 fps, only while the effect's card is
  on screen. Firmware addresses are checked first; on any other firmware the
  effect is just a pass-through.
- **Scope** (FXID 414): a live oscilloscope of the input. Works on the pedal.
- **Ring** (FXID 415): a rotating ring that follows the signal. Works on
  the pedal.
- **Skull** (FXID 416): a shaded 3D skull, full screen, that opens its jaw,
  lights its eyes and shakes with the signal, and explodes at the loudest
  peaks (Boom knob). Works on the pedal.

These three are built locally and are not in `dist/` yet.

### Docs and graphics
- The README's main image is redrawn in the Skull's 1-bit dither
  (`graphics/14-console-dithered.png`, made by
  `graphics/make_dithered_hero.py`; 36 KB instead of 2.9 MB). Replaced on
  2026-10-19 by the phosphor hero (see Unreleased).
- NAM Loader guide, NAM template docs and the reserved effect ID list
  updated for 216 slots, names and icons.
- This changelog.

## 2026-10-14
- MS-50G firmware 3.10 confirmed: NAM (full rate and Eco), CabIR and the
  Patch Editor's writes all work (#5).
- Effect DB decodes the pedal's "Hz" glyph, so Graphic EQ is recognised.

## 2026-10-12
- Loader caps on-pedal names at 9 characters (`NAM-`/`CAB-` + 5 letters).
- Pedal research notes for every other Zoom pedal people ask about;
  MS-100BT works through third-party Bluetooth tools.

## 2026-10-09
- Docs: recovery button combo, "Before you start", MIDISTOMP and the
  Matujuice effects pack.
- README: the Four series (G1/B1/A1 Four) is ZD2 and not supported; related
  projects (MS StompCtrl).

## 2026-10-06
- **NAM + Cab Loader**: 16 capture and 16 cab slots, Eco mode, DSP meter.
- NAM beta: tester note, freeze recovery guide, report form.
- Patch Editor: opt-in experimental writes for MS-50G and MS-60B.

Earlier history: `git log`.
