# ZDL Loader Safety

Last updated: 2026-05-19

What makes a custom ZDL load on the MS-70CDR without freezing the
pedal? This file documents the firmware's ZDL-load path, the
load-bearing safety constraints derived from it, and the load-time
size envelope measured from the stock corpus. The intent is to turn
"don't do that or it freezes" folklore into a documented contract.

The firmware data here comes from static disassembly of
`firmware/extracted/main_os.dis`. Corpus numbers come from
`build/analyze_stock_corpus.py`.

## 1. Loader entry shape

The ZDL loader entry is a multi-step function around `c00a6b68` that
runs through 8+ phases, each returning success/fail in `A4`. On any
phase failure, control branches to one of three error handlers
(`c00a6cac`, `c00a6cb4`, `c00a6cba`) which call the firmware error
reporter at `c00df6a0` with error codes 2 or 3:

```
c00a6b68   CALLP __push_rts
   ...register setup...
c00a6b80   CALLP 0xc00a5400              ; phase 1 (header / SIZE block)
c00a6b90   [!A0] BNOP error               ; → error if phase 1 fails

c00a6b98   CALLP 0xc00a5660              ; phase 2
c00a6ba4   [!A0] BNOP error

c00a6ba8   CALLP 0xc00a5b8c              ; phase 3
c00a6bb2   [!A0] BNOP error

c00a6bb4   CALLP 0xc00a5be0              ; phase 4
c00a6bc2   [!A0] BNOP error

c00a6bc4   CALLP 0xc00a5d24              ; phase 5
c00a6bce   [!A0] BNOP error

c00a6bd8   CALLP 0xc00dc168              ; phase 6 (with B4 = state[27] = ELF base)
c00a6be8   [!A0] BNOP error

c00a6bec   CALLP 0xc00a62fc              ; phase 7 — *** section walker / dynamic-table dispatcher ***
c00a6bf6   [!A0] BNOP error

c00a6bf8   CALLP 0xc00a65c4              ; phase 8
c00a6c0a   CALLP 0xc00a6630              ; conditional (only if state[12] == 2)
c00a6c10   CALLP 0xc00a4ba0              ; finalization
c00a6c38   CALLP 0xc00a66ac              ; final hook
```

The loader passes a per-ZDL "load state" struct around in `A4`. Its
fields seen in the disassembly:

| state offset | seen at | meaning |
|---:|---|---|
| `state[27]` | `c00a630c LDW *+A4[27],A4` | ELF base pointer |
| `state[30]` | `c00a6be4 LDW *+A4[30],A0` | dynamic table presence flag |
| `state[19]` | `c00a61c4 LDW *+B10[19],B4` | dynamic table pointer |
| `state[12]` | `c00a6c02 LDHU *A4[12],A0` | section/halfword count |

This is the loader's internal struct, not the per-slot handler state
at `0x11f03000 + slot*0xD4`.

## 2. ZDL header: not version-checked

`firmware/Main.bin` does **not** contain the string
`"ZOOM EFFECT DLL SYSTEM VER 1.00"`. The 76-byte ZDL header is parsed
structurally (`NULL` prefix, `SIZE` block, `INFO` block, optional
`BCAB`/`CABI` extended payload, then ELF), but the version string
inside the INFO payload is not compared against anything in the
firmware.

That has two consequences for custom builds:

* The version string can be anything 32 bytes long; the loader doesn't
  read it.
* The `SIZE` block must be structurally valid: `size_payload = 8`,
  followed by `header_size` and `elf_size` as 4-byte little-endian
  words. `build/zdl.py` already enforces this on writes.

## 3. The dynamic-table dispatcher

`c00a61b8` is the dynamic-table parser. It walks the `.dynamic` table
8 bytes per entry (tag + value) until it sees `DT_NULL` (tag 0):

| DT tag | name | loader behavior |
|---:|---|---|
| 0  | DT_NULL                | end of table |
| 1  | DT_NEEDED              | silently skipped (`[!B1] BNOP error_exit` after `CMPLTU 0x1, B1, B1`) |
| 4..11 | DT_HASH/STRTAB/SYMTAB/etc | range check `CMPLTU 0x7, B0, B0` triggers loader exit if invalid |
| 12 | DT_INIT                | calls handler at `c00a6268` (then `c00a6108`) |
| 13 | DT_FINI                | calls handler at `c00a6274` (then `c00a612c`) |
| 14..23 | DT_SONAME, DT_PLTREL, DT_TEXTREL, etc. | dispatched via function table at `0xc00ef334` |
| 25..28 | DT_INIT_ARRAY, DT_FINI_ARRAY, …_ARRAYSZ | dispatched via the same `0xc00ef334` table |
| 32 | DT_PREINIT_ARRAY       | jumps to `0xc00a6274` (shared with DT_FINI handler) |
| 33 | DT_PREINIT_ARRAYSZ     | silently skipped |

For any DT tag the loader doesn't recognize (anything in 29..31, or
above 33), it skips to the next entry. So **a ZDL can include DT
entries the loader doesn't process and still load** — but those
entries do nothing.

Corpus evidence ([STOCK-EFFECT-CORPUS.md §1](STOCK-EFFECT-CORPUS.md)
and STATE-ABI-PROGRESS.md §"ELF dynamic-table correction"): all 825
parseable stock ZDLs have `PT_DYNAMIC`, and **none** of them include
`DT_INIT`, `DT_FINI`, `DT_INIT_ARRAY`, `DT_FINI_ARRAY`, or
`DT_PREINIT_ARRAY`. The standard tags they do use are
`DT_NEEDED`/`DT_PLTRELSZ`/`DT_PLTGOT`/`DT_HASH`/`DT_STRTAB`/
`DT_SYMTAB`/`DT_STRSZ`/`DT_SYMENT`. So custom ZDLs are best served
following the same minimal-DT pattern that `build/linker.py` already
produces — relying on `Dll_<Name>` for entry rather than DT_INIT.

## 4. Symbol resolution

Stock ZDLs use the standard ELF hash table (`.hash`, indexed by
`DT_HASH`) plus `.dynsym` and `.dynstr` to expose `Dll_<Name>` and
other entry points. The loader uses this path (not `DT_INIT`) to find
the effect's registration function. Every stock ZDL exports
`Dll_<EffectName>` — the linker already does this for custom builds.

The descriptor symbol `SonicStomp` (or in stock effects, the effect's
own name like `Chorus`, `Delay`, `TapeEcho3`) is also exported via
`.dynsym` and resolved by the loader.

`__TI_STATIC_BASE` and the three `__TI_pprof_*` symbols are present in
all 825 parseable stock ZDLs. The current linker preserves these.

## 5. Sections and placement

### 5.1 `.audio` is optional

[STOCK-EFFECT-CORPUS.md §4](STOCK-EFFECT-CORPUS.md): 65% of stock
effects (540/825) put DSP code in `.text`, not `.audio`. The current
`build/linker.py` places the audio function at VA `0x7800` in
`.audio`. Both layouts load on hardware (`StereoChorus`, `OTT`,
`TapeEcho4`, `SyncProbe` all confirmed). The loader doesn't require a
section named `.audio` to load.

### 5.2 Code-size envelope (observed)

From 825 parseable stock ZDLs, combined `.audio + .text` sizes:

| stat | bytes |
|---|---:|
| min   |    736 |
| p50   |  3,456 |
| p90   |  8,992 |
| max   | 18,016 |

Custom builds today (SyncProbe ≈ 288 bytes, TapeEcho4 ≈ several KB,
OTT/Galactic ≈ tens of KB) sit well within this envelope. The 18 KB
max comes from the dual-engine `DUAL_REV` family.

### 5.3 `.fardata` is overwhelmingly 24 bytes

89% of stock effects ship exactly 24 bytes of `.fardata` — the
6-word `KNOB_INFO` struct the linker already produces. The 11% with
larger `.fardata` mostly carry coefficient tables or mode-switch
data. **No load-time `.fardata` size limit has been observed**; the
v1-era hypothesis that "large `.fardata` freezes" remains untested.

### 5.4 Relocation types

Stock ZDLs use the standard C6000 ELF relocation types that the
loader applies:

| type | name | use |
|---:|---|---|
| 0x09 | `R_C6000_ABS_L16` | low 16-bit half of absolute address |
| 0x0A | `R_C6000_ABS_H16` | high 16-bit half of absolute address |
| 0x07 | `R_C6000_PCR_S21` | PC-relative branch displacement |
| 0x06 | `R_C6000_ABS32`   | 32-bit absolute pointer (descriptor entries) |

The linker handles these. The previous Codex pass found
([STATE-ABI-PROGRESS.md feedback_linker_sht_rel](../docs/STATE-ABI-PROGRESS.md))
that `cl6x` may emit `SHT_REL` instead of `SHT_RELA` when all addends
are zero, and the linker has to handle both — that was a real bug fixed
earlier.

## 6. Known freeze causes (catalogued)

Hardware-observed freezes mapped to their causes:

| Probe / build | Freeze trigger | Cause | Mitigation |
|---|---|---|---|
| `T9NoAudio` | Knob/page interaction | `cl6x`-compiled `ZOOM_EDIT_HANDLER` macro outputs freeze on UI interaction in 9-param plugins | Use stock-derived LineSel-cloned handlers; see [EDIT-HANDLER-ABI.md](EDIT-HANDLER-ABI.md) |
| `InitProbe` Stage 3 | Boot | Cloned edit handler called from custom `_init` without `A10` save / delay-slot `A4` restore | Follow LineSel `_init` recipe; see [INIT-MATERIALIZATION.md](INIT-MATERIALIZATION.md) |
| `Galactic`-era `.fardata` experiments | Boot | Large writable static data in `.fardata` froze hardware in v1 | Keep large state in `ctx[3]`, not `.fardata`; see [STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md) |
| `--opt_for_space=3` cl6x output | Boot | cl6x at level 3 emits unfillable indirect-dispatch thunks | Drop the flag — `feedback_linker_obj_const` |
| Descriptor entry +0x28 = 0 | Effect drops from edit-mode UI | Entry 1's `+0x28` is a CPU/memory cost estimate; writing 0 silently drops params | Linker writes 20.0f; see `build/ABI.md` |

## 7. Loader's optional-feature handling (failure modes that aren't freezes)

The loader is permissive in several places — failures here cause the
ZDL to load but not work fully, rather than freezing:

* Out-of-range DT tags: silently skipped.
* DT_INIT/FINI present but unused: handled by the loader's separate
  init dispatcher; stock effects don't ship them and don't need to.
* `__TI_pprof_*` missing: not confirmed load-safe, but the linker
  always emits them so this isn't tested.
* `effectTypeImageInfo` missing: 7 of 825 stock effects omit it. So
  the loader tolerates its absence in some path, but the
  3-PARAM-LINKER-BUG history shows that `+0x28` of the imageInfo
  matters for UI display.

## 8. Practical safety checklist for custom builds

For a new custom ZDL to load cleanly:

* [ ] Header: 76 bytes (or 232/312 with `BCAB`/`CABI` extended
  payload). `SIZE` block must claim `size_payload = 8` and the
  declared `elf_size` must match the actual ELF bytes.
* [ ] ELF: little-endian, ELFCLASS32, EM_TI_C6000, ET_DYN.
* [ ] `.dynsym` + `.dynstr` + `.hash` (DT_HASH'd) present, exporting
  at least `Dll_<EffectName>`.
* [ ] `SonicStomp` descriptor table in `.const` with at least
  OnOff + name + at least one param entry, last entry marked by
  `pedal_flags & 0x04`.
* [ ] Name entry's `+0x28` (CPU-cost float) must be non-zero —
  linker writes `20.0f`.
* [ ] `effectTypeImageInfo` present (1 of 818 stock have it; treat
  it as required by the linker even though 7 stock effects skip it).
* [ ] DSP code in `.audio` *or* `.text` — both work.
* [ ] `.fardata` carries `KNOB_INFO` (24 bytes) plus any small
  effect-local statics. Keep large state in `ctx[3]`.
* [ ] No `DT_INIT` / `DT_FINI` / `DT_INIT_ARRAY` etc. — stock pattern
  uses `Dll_<Name>` for entry instead.
* [ ] Relocations limited to standard `R_C6000_*` types the linker
  applies.
* [ ] Avoid `cl6x --opt_for_space=3`. Default `-O2` is safe; SyncProbe,
  StereoChorus, TapeEcho4, OTT all built with `-O2 --abi=eabi`
  `--mem_model:data=far`.
* [ ] Edit handlers: borrowed `linesel_handlers.bin` (proven) or
  synthesized via the linker's LineSel-clone path. Avoid the
  `ZOOM_EDIT_HANDLER` macro path for multi-page UIs.

## 9. Open static-RE leads (not yet pinned down)

* Exact firmware call site that invokes `_init` after slot setup. We
  know `_init` runs after the template writer (`c00ab614`), entered
  with `A4 = state ptr`, but the actual CALLP target inside
  `c00ab620..c00ab690` isn't isolated. See
  [INIT-MATERIALIZATION.md §7.3](INIT-MATERIALIZATION.md).
* The exact failure mode in each of the 8 loader phases. We have
  function addresses (`c00a5400`, `c00a5660`, etc.) but not yet a
  per-phase description of what each checks.
* Whether the firmware's loader applies relocations itself or whether
  the linker's `.rela.dyn` is consumed at runtime by some other path.
* Whether section name strings (`.audio`, `.text`, `.const`) are
  matched anywhere, or whether the loader works purely from section
  type/flags/address.

## 10. Where this changes other docs

* [build/ABI.md](../build/ABI.md): once the per-phase loader is
  mapped, ABI.md can grow a "loader contract" section that the linker
  validates against. For now, the existing "Required sections /
  symbols" list is consistent with this doc's §8 checklist.
* [STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md): the freeze-cause
  catalog in §6 here mirrors the "Known unsafe" list there; keeping
  them aligned as new probes find new freeze modes.
