# Re-ID probe: can an effect move to a new ID at install time?

If it can, an install tool resolves ID clashes on the pedal itself: a new
effect whose ID is taken gets a free one before it is written, and nobody has
to coordinate ranges. The ID is only in the ZDL header (group at bytes 60 and
67, ID at 64-65, no checksum), so moving it means rewriting those bytes.

`python3 -B src/hardware_probes/reid/make_reid.py` → `build/probes/reid/`:
copies of Scorch (Delay 457), changed only in the ID and the 12-byte
pedal name.

| File | Pedal name | ID | Tests |
|---|---|---|---|
| SCR917.ZDL | Scr917 | Delay 917 | a free ID inside 403–957 |
| SCR1000.ZDL | Scr1000 | Delay 1000 | the untried range 958–1023 |
| SCR1023.ZDL | Scr1023 | Delay 1023 | the highest ID a patch can store (10 bits) |
| SCRDUP.ZDL | ScrDup | Delay 917 | a clash with SCR917: the pedal page moves it to a local ID (958–1023). In `build/reid-clash/`, out of the editor's list. |
| CAPS1–CAPS16.ZDL | CapS1–CapS16 | capture slot 1–16's ID (900–907, 950–957) | the MultiStomp Editor skips a capture slot whose ID another effect has. In `build/reid-clash/capslot/`. |

Modwerk's pedal page installs added effects up to Delay 1023 and moves a
clashing one to a free local ID (958–1023).

## Steps

1. Install the three files with the pedal page and power-cycle.
2. **Listed?** Each appears in the Delay list under its own name.
3. **Runs?** Put each in a patch, raise Gain: it distorts like Scorch.
4. **Saves?** Store that patch, power-cycle, recall it: the same effect, with
   the same Gain.
5. **Beside the original?** If Scorch is installed, a patch with Scorch and
   Scr917 side by side: both run, each keeps its own settings.

## Results

MS-70CDR 2.10 (2026-11-02):

- **Installed:** all three, through the pedal page with its limit raised to 1023.
- **Runs:** all three play and sound identical to Scorch, at 917, 1000 and 1023.
- **Saves:** a patch with copies in two slots at different Gains, stored,
  power-cycled and recalled: the same effects in the same slots with the same
  settings.

So an install tool can give an effect any free Delay ID from 403 to 1023 by
rewriting its header; nothing needs rebuilding.

MS-70CDR 2.10 (2026-11-03), Modwerk pedal page with clash handling:

- **Clash:** SCRDUP.ZDL (917) installed with SCR917.ZDL already on the pedal.
  The page moved it to 973, the ID its name hashes to.
- **Runs:** a patch with ScrDup (973) and Scr917 (917) side by side: both
  distort.

MS-70CDR 2.10 (2026-11-03), MultiStomp Editor capture slots:

- A capture installed to slot 1 (nothing in the way). CAPS2.ZDL then installed
  on slot 2's ID (8/901). The next capture skipped slot 2 ("CAPS2.ZDL already
  has its ID 8/901") and slot 3 (used before in that browser) and went to slot 4.
- After a power cycle NAM-testC, CapS2 and NAM-testS are all listed and all play.
