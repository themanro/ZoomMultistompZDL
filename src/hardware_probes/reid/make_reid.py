#!/usr/bin/env python3
"""Re-ID probe: copies of Scorch with a new effect ID and pedal name.

The question is whether an effect can be moved to a free ID at install time
by rewriting its header, with nothing rebuilt: group at bytes 60 and 67, ID
at 64-65 (little-endian), no checksum over them. The pedal name is the
12-byte field in the descriptor table, which the NAM loader already rewrites.

    python3 -B src/hardware_probes/reid/make_reid.py  ->  build/probes/reid/
"""
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SRC = ROOT / "dist" / "Scorch.ZDL"
OUT = ROOT / "build" / "probes" / "reid"   # not dist/: dist/ ships

# (file, pedal name, fxid): one free ID inside today's 403-957, two above it.
COPIES = [("SCR917.ZDL", "Scr917", 917),
          ("SCR1000.ZDL", "Scr1000", 1000),
          ("SCR1023.ZDL", "Scr1023", 1023)]
# Same ID as SCR917 on purpose: the pedal page should move it to a local ID.
# Outside build/probes/, so the editor's effect list doesn't get two effects on 917.
CLASH = ("SCRDUP.ZDL", "ScrDup", 917)
CLASH_OUT = ROOT / "build" / "reid-clash"
# What the pedal page should turn SCRDUP into (hash of the name -> 973), so the
# editor lists it where it landed. The editor doesn't follow moves yet.
MOVED = ("SCRDUP.ZDL", "ScrDup", 973)
# Blockers on the IDs of capture slots 1-16 (nam_loader.js namFxid): install the
# one for the slot the Patch Editor would use next, then install a capture; it
# should skip that slot. Not 'NAM<digit>' names, so the editor doesn't take
# them for captures.
BLOCKERS = [(f"CAPS{s}.ZDL", f"CapS{s}", 899 + s if s <= 8 else 941 + s) for s in range(1, 17)]
BLOCK_OUT = CLASH_OUT / "capslot"


def reid(data: bytes, gid: int, fxid: int, old_name: str, new_name: str) -> bytes:
    b = bytearray(data)
    assert b[4:8] == b"SIZE" and b[76:80] == b"\x7fELF", "not a ZDL"
    assert 0 <= fxid <= 1023, "a patch stores 10 bits of effect ID"
    b[60] = b[67] = gid
    struct.pack_into("<H", b, 64, fxid)
    field = b"\x00" * 8 + old_name.encode().ljust(12, b"\x00")
    assert b.count(field) == 1, "name field not found exactly once"
    at = b.index(field) + 8
    assert len(new_name) <= 9, "stock names are at most 9 characters"
    b[at:at + 12] = new_name.encode().ljust(12, b"\x00")
    return bytes(b)


def main():
    src = SRC.read_bytes()
    OUT.mkdir(parents=True, exist_ok=True)
    BLOCK_OUT.mkdir(parents=True, exist_ok=True)
    for out, (name, label, fxid) in ([(OUT, c) for c in COPIES + [MOVED]] + [(CLASH_OUT, CLASH)]
                                     + [(BLOCK_OUT, b) for b in BLOCKERS]):
        data = reid(src, 8, fxid, "Scorch", label)
        diff = [i for i in range(len(src)) if src[i] != data[i]]
        (out / name).write_bytes(data)
        print(f"{name:12} Delay {fxid:4}  '{label}'  {len(diff)} bytes changed at "
              + ", ".join(hex(i) for i in diff))


if __name__ == "__main__":
    main()
