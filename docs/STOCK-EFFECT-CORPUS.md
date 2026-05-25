# Stock-Effect Corpus

Last updated: 2026-05-19

This file is the running summary of `build/analyze_stock_corpus.py`. The
extractor walks every stock ZDL under `stock_zdls/` and emits one
machine-readable row per effect to `build/stock_corpus.csv`. The point is
to turn "what do stock effects look like" from folklore into a queryable
table.

Rerun any time the corpus changes:

```
python3 build/analyze_stock_corpus.py
```

Dump the actual SonicStomp descriptor entries for one or more ZDLs:

```
python3 build/dump_zdl_descriptor.py stock_zdls/MS-70CDR_LINESEL.ZDL
```

This is now the preferred way to inspect user-facing parameters, because it
reads the 0x30-byte entries the firmware handler dispatcher actually walks.
Counting exported `_edit` symbols is still useful, but it overcounts shared or
conditional edit handlers.

Schema (columns, current revision):

`name, file_size, header_size, extra_header_len, truncated, real_type,
family_name, sort_fx_type, knob_type, bass_flags, sort_index, sort_sub,
fx_version, audio_size, text_size, const_size, fardata_size, rela_dyn,
rela_plt, dynsym, dynstr, hash, dynamic, debug_total, fam_prefix,
effect_root, audio_main, n_audio_candidates, n_edit, n_onf, n_init,
n_dll, n_coe, n_getstr, n_helpers, edit_handlers, getstr_names, helpers`

## 1. Corpus shape

* 830 ZDL files. 825 parse cleanly; 5 are **truncated** (declared
  `elf_size` larger than the file actually holds). All five are reverb
  variants: `MS-60B_GATE_REV`, `MS-60B_HD_REV`, `MS-70CDR_CHURCH`,
  `MS-70CDR_DUAL_REV`, `MS-70CDR_TREMDLY`. They appear to be shipped
  partial; do not use them as references.

* Extended-header distribution (`extra_header_len`):

  | extra | count | meaning |
  |---:|---:|---|
  | 0   | 754 | standard 76-byte header (the case `build/linker.py` produces) |
  | 176 | 24  | `BCAB`-style extended header |
  | 256 | 52  | `CABI`-style extended header |

  91% of stock effects use the standard 76-byte header. Custom builds
  don't need extended headers to load. See
  [docs/STATE-ABI-PROGRESS.md](STATE-ABI-PROGRESS.md) for the
  reverse-engineering history of the extended-header parser.

## 2. Family distribution

| family | count | notes |
|---|---:|---|
| Modulation    | 189 |  |
| Reverb        | 124 |  |
| Delay         | 118 |  |
| Filter        | 109 |  |
| Dynamics      |  66 |  |
| Drive         |  57 |  |
| GuitarAmp     |  52 |  |
| SFX           |  49 |  |
| BassAmp       |  24 |  |
| BassDrive     |  16 | `real_type` 12 or 20; symbols `Fx_BASSDRV_*` |
| BassPreamp    |  12 | `real_type` 13 or 22; symbols `Fx_BASSPREAMP_*` |
| PedalOperated |  10 | `real_type` 11; symbols `Fx_PDL_*` |
| ExtraDLL      |   4 |  |

`real_type` 12/13 and 22 had been missing or mis-labeled in
`build/zdl.py:FX_TYPES`; this revision corrects them.

## 3. Edit-handler counts (= visible knobs)

| `n_edit` | count |
|---:|---:|
| 0   |  71 |
| 1   |  26 |
| 2   |  22 |
| 3   | 114 |
| 4   | 118 |
| 5   | **226** |
| 6   | 112 |
| 7   |  53 |
| 8   |  57 |
| 9   |  24 |
| 10  |   7 |

Notable:

* **5 knobs is the modal stock shape** (226/825 = 27%).
* **66 effects have zero edit handlers.** Most are bass-amp models
  (`Fx_BASSAMP_*`) with no user-adjustable knobs; they ship fixed
  parameters and rely on the OnOff path only.
* **The "10 edit handlers" rows are a counting artifact, not a
  real ceiling break.** `DUALDIGD`, `DUAL_REV`, and `FLTERPPD`
  (plus their MS-60B and MS-70CDR variants) each export 10 `_edit`
  symbols, but reading the SonicStomp descriptor directly shows only
  **9 user-knob entries**. The extra symbols are link-mode handlers
  (e.g. `Fx_DLY_DualDigiD_timeLR_edit` controls both `TimeA` and
  `TimeB` together) that are exported but only bound conditionally,
  not as independent descriptor entries. The 9-knob ceiling in
  [build/ABI.md §3.1](../build/ABI.md) is correct.

Direct descriptor parsing gives this actual user-knob distribution across the
818 stock ZDLs where the `OnOff` descriptor table is found:

| descriptor user knobs | count |
|---:|---:|
| 2 | 25 |
| 3 | 109 |
| 4 | 88 |
| 5 | 158 |
| 6 | 187 |
| 7 | 80 |
| 8 | 57 |
| 9 | 114 |

This explains why symbol counting and UI behavior sometimes disagreed: stock
ZDLs often export helper edit handlers that are not separate descriptor
entries, and some descriptor entries intentionally share the same handler.

## 4. Audio-section placement (`.audio` vs `.text`)

| placement | count |
|---|---:|
| `.audio` non-empty                            | 285 |
| `.audio` empty, audio body lives in `.text`   | 540 |
| both empty                                    |   5 (the truncated files) |

**65% of stock effects do not use a dedicated `.audio` section**; the
audio body lives in `.text` alongside the handlers. The current custom
toolchain (`build/linker.py`) places the audio function at VA `0x7800`
in `.audio`. Both layouts load on hardware (LineSel, Galactic, OTT have
all built with `.audio`). The corpus tells us **`.audio` is optional**;
it is not a contract the firmware relies on for placement.

The ZD2-side hand decode of `Fx_SFX_LineSel` points the same way: code
placement differs from the ZDL LineSel build, but the audio semantics still
match the same coefficient-table split between effect buffer and pedal output
accumulator. The loader contract appears to be the descriptor/symbol/runtime
ABI, not the section name that happens to hold the audio body.

## 5. `.fardata` size distribution

`.fardata` exists in 821/825 parseable effects, and almost all of them
ship exactly 24 bytes — the `KNOB_INFO` struct that `build/linker.py`
already produces:

| `.fardata` size | count |
|---:|---:|
| 24  | 741 |
| 40  |  30 |
| 48  |  25 |
| 0   |   9 |
| 220 |   7 |
| 156 |   5 |
| 76  |   5 |
| 72  |   4 |
| other | small |

The 24-byte case is `KNOB_INFO {20, 15, 11, 0, 2, 0}` from the linker.
Larger `.fardata` values correspond to effects that ship additional
small static state (coefficient tables, mode tables). 89% of stock
effects ship only the 24-byte `KNOB_INFO`.

## 6. Code-size envelope

`.audio + .text` combined size across the 825 parseable effects:

| stat | bytes |
|---|---:|
| min   |    736 |
| p50   |  3,456 |
| p90   |  8,992 |
| max   | 18,016 |

The max (~18 KB) belongs to the dual-engine `DUAL_REV` family.
Custom builds today land well under p50. This is the working envelope
for "how large can a stock-style audio body be on this hardware."

## 7. GetString callbacks — top names

`GetString_*` symbols are value-to-string formatters that the UI calls
to display knob values. Top names by use across the corpus:

| count | symbol | meaning |
|---:|---|---|
| 255 | `GetString_offset_1`         | display raw value with `+1` offset |
| 201 | `GetString_Tail`             | reverb/delay tail toggle display |
|  66 | `GetString_offset_minus10`   | `-10` offset |
|  48 | `GetString_0_50_Sync`        | tempo-sync 0..50 |
|  45 | `GetString_ofst_1_50_Sync`   | tempo-sync 1..50 |
|  40 | `GetString_1_2000_Sync`      | tempo-sync 1..2000 (ms-style) |
|  36 | `GetString_AwaSens`          | "awa" sensitivity (auto-wah) |
|  26 | `GetString_FreqTable8`       | 8-step frequency selector |
|  25 | `GetString_Dry`              | dry/wet display |
|  23 | `GetString_offset_minus25`   | `-25` offset |
|  22 | `GetString_MonoStereo`       | **mono/stereo toggle as a knob value** |
|  19 | `GetString_offset_minus12`   | `-12` offset (semitone-ish) |
|  18 | `GetString_Cabi`             | cabinet selector |
|  16 | `GetString_PitSft`           | pitch-shift display |

Two consequences worth following up:

* **Tempo-sync is widespread.** At least 150 stock effects use a
  `GetString_*_Sync` callback — i.e. they expose a user param that
  cycles through tempo-sync divisions instead of a free continuous time
  knob. That is the static evidence base for the "Tempo sync" item in
  the plan; the next step is mapping which descriptor flags the
  audio/edit code reads to actually receive the host BPM.

* **`GetString_MonoStereo` is the actual stereo answer at the
  user-facing level.** It is used by exactly the effects with a
  `_mode_edit` knob: `STOMPDLY`, `ANA234CH`, `CE_CHO5`, `SUPERCHO`,
  `SLAPBKD` (plus the per-pedal-variant copies, 22 in total). So
  *some* stock effects DO let the player toggle mono/stereo, but it is
  a parameter value with a string formatter — not a ZDL-level flag.
  This sharpens [docs/STEREO-ROUTING.md](STEREO-ROUTING.md): the ZDL
  has no stereo bit, but a custom effect can expose an in-UI toggle
  the same way stock STOMPDLY does, simply by shipping a knob whose
  display callback is `GetString_MonoStereo`.

## 8. Most-used shared symbols

The corpus reveals a small set of symbols that nearly every stock
effect imports. The Top 20 (excluding C6x runtime helpers
`__c6xabi_*`):

| count | symbol | role |
|---:|---|---|
| 825 | `__TI_STATIC_BASE`                 | TI runtime base pointer |
| 825 | `__TI_pprof_out_hndl`              | TI profiler handles |
| 825 | `__TI_prof_data_size`              | TI profiler size |
| 825 | `__TI_prof_data_start`             | TI profiler start |
| 818 | `effectTypeImageInfo`              | the 212-byte UI struct |
| 224 | `_infoEffectTypeKnob_A_2`          | shared knob-info struct |
| 201 | `disp_prm_Tail`                    | tail-knob display helper |
| 192 | `disp_prm_BPM_sync`                | BPM-sync display helper |
| 177 | `infoEffectTypeKnob_A_2`            | shared knob-info struct (non-underscored) |
| 176 | `_infoEffectTypeKnob_A_2_Reverse`  | reverse direction knob-info |
| 102 | `infoEffectTypeKnob_A_2_Reverse`    | reverse direction knob-info |
|  60 | `SUB_Drive_KawaOD`                  | drive helper |
|  52 | `AmpEqSetting`                      | amp-EQ helper |
|  52 | `iir2p_ComboFront_*`                | shared IIR filter blocks |
|  52 | `iir2p_StackFront_*`                | shared IIR filter blocks |
|  52 | `iir2p_thru`                        | pass-through IIR slot |

* The four `__TI_*` symbols are TI compiler runtime; effectively
  invariant.
* `effectTypeImageInfo` is present in 818/825 — that confirms the
  current linker's assumption that it is part of the standard ZDL
  shape.
* `disp_prm_BPM_sync` appearing in 192 effects is more concrete evidence
  for the tempo-sync follow-up: there is a single shared display routine
  that all sync-enabled effects bind against.

## 9. Open questions surfaced by this pass

* ~~**10-knob effects.**~~ Closed: the 10-edit count was a counting
  artifact from link-mode handlers (see §3). 9 knobs is the actual
  descriptor ceiling.
* **BPM/tempo wiring.** The `GetString_*_Sync` and `disp_prm_BPM_sync`
  callbacks are stock-shared; the path that delivers the host BPM into
  the audio loop is not yet mapped. Next static step: disassemble
  `disp_prm_BPM_sync` and see what `ctx[]` field or host callback the
  audio function reads.
* **Descriptor flags.** Direct descriptor parsing found these parameter-entry
  flag values: `0x00`, `0x10`, `0x04`, `0x14`, `0x28`, `0x38`, `0x50`, and
  rare `0x2c`. The `0x04` bit is the final-entry sentinel. `0x28`/`0x38`
  are tempo/sync-shaped entries; e.g. `MS-70CDR_TAPEECH3` has `TIME` and
  `SYNC` entries sharing `Fx_DLY_TapeEcho3_time_edit`, with `SYNC` using
  `max=15`, `default=0`, `flags=0x28`.
* **Shared edit handlers.** At least 145 stock ZDLs have multiple descriptor
  entries bound to the same handler. Examples include `Time`/`Sync` pairs,
  dual-rate controls, and grouped EQ bands. This is important for custom
  effects: the descriptor table, not the exported symbol list, defines the UI.
* **Truncated reverb files.** Five stock reverbs ship as truncated
  blobs. Whether they are placeholders (factory firmware bug) or
  loaded via a fallback path is open. They are excluded from all
  analysis above except header-level stats.
* **`Fx_BASSAMP_*` zero-edit effects.** 66 effects expose no edit
  handlers; they look like fixed-character amp models. The
  parameter-materialization story is different for these — there is
  no edit-handler dispatch involved. Worth a separate look if the
  open-source platform wants to support amp-model-style effects.
* **DWARF-labeled data tables.** The ZNR `ZNR_gate_depth_tbl` sample shows
  that some labels emitted as `DW_TAG_subprogram` are actually raw data tables
  (`1.0`, `0.95`, `0.9`, ...). Corpus tooling should not assume every
  function-like DWARF label is executable code.

## 10. How to extend this

* Add a column: edit the schema in `build/analyze_stock_corpus.py`'s
  `extract()` return dict, rerun.
* Add a derived cut: keep the analysis in this doc, not in the
  extractor. Use the CSV from Python/pandas/sqlite.
* Add a corpus integrity check: the truncated-file detection is the
  template — surface other shape-level anomalies as new columns rather
  than silent failures.
