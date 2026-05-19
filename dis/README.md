# Disassembly Work Files

This directory holds small, explicit stock-effect extraction targets for
interactive reverse engineering.

Current target:

* `MS-70CDR_TAPEECH3.ZDL.out` - embedded C6000 ELF extracted from
  `stock_zdls/MS-70CDR_TAPEECH3.ZDL`; open this in IDA first.
* `MS-70CDR_TAPEECH3.ZDL.asm` - TI `dis6x` output for quick text search.

Regenerate with:

```bash
python3 -B build/disassemble_zdl.py stock_zdls/MS-70CDR_TAPEECH3.ZDL --out-dir dis
```

High-value function:

* `Fx_DLY_TapeEcho3_time_edit` at `0x000007e0`. The descriptor binds both
  `TIME` and `SYNC` to this handler, so it is the best current stock reference
  for tempo-sync parameter materialization.
