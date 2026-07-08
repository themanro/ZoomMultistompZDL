"""Build the patch-editor effect database from .ZDL binaries.

Parses every ZDL's SonicStomp descriptor table (0x30-byte entries: OnOff,
effect-name self-entry, then one entry per knob with name/max/default) plus
the header's gid/fxid, and computes the 32-bit patch effect ID used inside
MS-series patch dumps:

    patchID = ((fxid & 0x3F) << 17) | (((fxid >> 6) & 1) << 30)
            | (((fxid >> 7) & 7) << 8) | (gid << 1)

Verified against all 137 stock MS-70CDR ZDLs vs g200kg/zoom-ms-utility's
effect list (137/137 match).

Usage:
    python3 build/extract_effect_db.py            # writes tools/effects_db.json
"""

from __future__ import annotations

import json
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "build"))


GID_CATEGORY = {
    1: "Dynamics", 2: "Filter", 3: "Drive", 4: "Amp", 5: "Pedal",
    6: "Modulation", 7: "SFX", 8: "Delay", 9: "Reverb",
}


def patch_id(fxid: int, gid: int) -> int:
    return (((fxid & 0x3F) << 17) | (((fxid >> 6) & 1) << 30)
            | (((fxid >> 7) & 7) << 8) | (gid << 1))


def _walk_descriptor(data: bytes, off: int):
    """Walk 0x30-byte descriptor entries from `off` until the sentinel."""
    entries = []
    while True:
        e = data[off:off + 0x30]
        if len(e) < 0x30:
            return None
        raw = e[:10].split(b"\0")[0]
        if any(c < 0x20 or c > 0x7E for c in raw):
            return None                     # non-printable name: not a table
        nm = raw.decode("ascii")
        maxv, = struct.unpack_from("<I", e, 0x0C)
        defv, = struct.unpack_from("<I", e, 0x10)
        flags, = struct.unpack_from("<I", e, 0x2C)
        entries.append((nm, maxv, defv, flags))
        off += 0x30
        if flags & 0x04:                    # last-entry sentinel
            return entries
        if len(entries) > 12:
            return None


def parse_zdl(path: Path):
    """The descriptor table always begins with an 'OnOff' entry — find it by
    content (works for both our SonicStomp symbol and stock ZDLs, which name
    the table after the effect, plus files whose ELF headers parse oddly)."""
    data = path.read_bytes()
    gid = data[0x3C]
    fxid = data[0x40] | (data[0x41] << 8)

    entries = None
    i = 0
    while True:
        i = data.find(b"OnOff\x00", i)
        if i < 0:
            break
        entries = _walk_descriptor(data, i)
        if entries and len(entries) >= 2 and entries[1][1] == 0xFFFFFFFF:
            break                           # entry 1 = effect self-entry
        entries = None
        i += 1

    if not entries:
        print(f"  skip (no descriptor table): {path.name}")
        return None
    eff_name = entries[1][0]
    params = [
        {"name": nm, "max": maxv, "default": defv}
        for nm, maxv, defv, _fl in entries[2:]
        if nm and maxv != 0xFFFFFFFF
    ]
    return {
        "name": eff_name,
        "fxid": fxid,
        "gid": gid,
        "category": GID_CATEGORY.get(gid, f"gid{gid}"),
        "id": patch_id(fxid, gid),
        "params": params,
    }


def _cover_b64(path: Path):
    """Row-major MSB-first 1024-byte cover bitmap, base64 (None if undecodable)."""
    import base64
    try:
        from decode_picture import decode_picture
        px, _ = decode_picture(str(path))
        b = bytearray()
        for y in range(64):
            for xb in range(16):
                byte = 0
                for bit in range(8):
                    if px[y][xb * 8 + bit]:
                        byte |= 1 << (7 - bit)
                b.append(byte)
        return base64.b64encode(bytes(b)).decode()
    except Exception:
        return None


def _rank(path: Path) -> int:
    n = path.name
    if path.parent.name == "dist":
        return 0
    for i, pre in enumerate(
            ("MS-70CDR_",), start=1):
        if n.startswith(pre):
            return i
    for i, pre in enumerate(("MS-50G_", "MS-60B_", "G1on_", "G1Xon_", "B1Xon_"), start=3):
        if n.startswith(pre):
            return i
    return 2                              # bare-name files (MS-70CDR era)


def main() -> None:
    best = {}                             # patch id -> (rank, entry, is_custom)
    files = sorted((ROOT / "dist").glob("*.ZDL")) + sorted((ROOT / "stock_zdls").glob("*.ZDL"))
    for f in files:
        e = parse_zdl(f)
        if not e:
            continue
        r = _rank(f)
        cur = best.get(e["id"])
        if cur is None or r < cur[0]:
            e["cover"] = _cover_b64(f)
            best[e["id"]] = (r, e, r == 0)

    db = {"custom": [], "stock": []}
    for r, e, is_custom in sorted(best.values(), key=lambda x: x[1]["name"].upper()):
        db["custom" if is_custom else "stock"].append(e)

    # sanity: no patch-ID collisions
    ids = {}
    for kind in ("custom", "stock"):
        for e in db[kind]:
            if e["id"] in ids:
                print(f"WARNING: patch-ID collision {e['name']} vs {ids[e['id']]}")
            ids[e["id"]] = e["name"]

    out = ROOT / "tools" / "effects_db.json"
    out.write_text(json.dumps(db, indent=1))
    print(f"{len(db['custom'])} custom + {len(db['stock'])} stock effects -> {out}")


if __name__ == "__main__":
    main()
