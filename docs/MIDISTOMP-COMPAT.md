# This project's effects and editor on MIDISTOMP

[MIDISTOMP](https://github.com/matujuice/zoom-ms-midistomp) (Luca Matturro, MIT) replaces
the MultiStomp's OS with a patched one. On the MS-70CDR it is built on firmware 2.10 only.
This page records what was checked for running this project's effects, their live
screens and the MultiStomp Editor on it. Checked against MIDISTOMP at commit `7c77c91`
(2026-11-21, v2.0.1), by reading its patches and sources. **Not yet tried on a pedal
running MIDISTOMP.**

## Live screens

Expected to work unchanged.

- **The firmware check passes.** `zvis.h` checks ten code words of the 2.10 OS before it
  reads or calls anything (Event_post's prologue and three other sites). None of
  MIDISTOMP's MS-70CDR `write:` or `section:` entries (all 186 in `patches/`) overlaps
  them, or any of the state words the screen test reads.
- **MIDISTOMP's own screens don't clash.** The screen test requires B14+320 = 0. That
  word is the OS's UI mode (MIDISTOMP calls it MODE: 0 = play, 11 = menu screens).
  MIDISTOMP's menus, MASTER, KALEIDO (including IDLE KALEIDO, `chain_keys.c idle_ev`),
  PERFORM and the effect chain screen all set it to 11, so our effects stop asking for
  redraws while they are up.
- **The GRDLOOP bar** is drawn over the cover only while the cursor is on GRDLOOP's own
  slot (`asm/looper/lbox.c`), so it never covers our effects' screens.
- **Memory:** MIDISTOMP's state lives in DDR the RAM test found unused (`0xC00F2000` up,
  `0xC00F3000` services block, `0xC010A020` MASTER) and in L2 (`0x1181FF00` transport
  block). Our effects use only their `ctx[3]` arena, except Loop's Long mode, which
  overlaps that DDR. Loop turns Long off when MIDISTOMP is present: from 0.05 it checks
  the magic of MIDISTOMP's looper services block (`0xC00F3000`, `0x5A4C5301`) or of its
  transport block (`0x1181FF00`, `0x5A4D5401`), so it doesn't depend on MIDISTOMP's
  memory layout (0.04 looked for one of the region bases in the services block; region
  1 has since moved to `0xC0118C00`, region 3 still matched).

## The editor

- **Identity.** MIDISTOMP puts its tag in the version bytes of the identity reply: `M201`
  for 2.0.1 in place of `2.10`. The editor now reads that as "MIDISTOMP 2.0.1" (heading
  and log) and doesn't warn that the live screens need 2.10.
- **Routing byte.** MIDISTOMP stores routing in byte 17 of each 18-byte slot block of the
  packed patch (stock leaves it 0). Bits 0-1 are the slot's bus and belong to the effect;
  bits 2-6 belong to the slot position; bit 7 stays 0 (MIDISTOMP
  `docs/dev/modwerk-research.md`). The editor writes patches on top of the pedal's own
  dump, so byte 17 is kept, and a slot swap now moves bits 0-1 with the effect and leaves
  bits 2-6 in place (test in `tools/tests/patch_editor.test.cjs`).
- **Mixer and PERFORM settings** are not in the patch: MIDISTOMP keeps them in its global
  record 50, matched to a patch by a hash of its six effect IDs and its name. Renaming a
  patch or changing, adding, removing or reordering its effects resets them for that
  patch. The editor says so in its log when it connects to a MIDISTOMP pedal. Knob values
  and on/off don't affect the hash.
- **Records 50 and 51** must never be written or deleted. The editor's file commands only
  touch effect files and `FLST_SEQ.ZDT`, and its patch commands only patches 1-50.
- **Live knob edits in slots 4-6** go through a buffer write as on stock firmware: the
  stock OS's SysEx `0x31` handler only edits slots 1-3. MIDISTOMP's CC handler reaches all
  six slots through `fx_set_param`; doing the same for `0x31` would make the editor's
  slot 4-6 edits live and gapless (asked of Luca).
