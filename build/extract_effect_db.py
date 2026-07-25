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

    # ---- legacy fxid aliases -------------------------------------------------
    # Every fxid an effect has EVER shipped under. Old patches (and pedals still
    # running an old build) reference these ids; without aliases the editor
    # shows "unknown effect 0x…" for effects that are installed and playing
    # fine. Alias entries reuse the current build's params/cover.
    LEGACY_FXIDS = {
        "Microlm":  [474],
        "Flower":   [475],
        "Shatter":  [476],
        "Arrakis":  [453],
        "Corrupt":  [477],
        "Klang":    [478],
        "Scorch":   [479],
        "Howl":     [480],
        "Taffy":    [460, 482],
        "Dissolve": [481],
        "Mangle":   [462, 463, 464, 465, 467, 468, 469, 483],
        "Rooms":    [471],
    }
    current_ids = {e["id"] for e in db["custom"]} | {e["id"] for e in db["stock"]}
    db["legacy"] = []
    for e in db["custom"]:
        for old in LEGACY_FXIDS.get(e["name"], []):
            if old == e["fxid"]:
                continue
            lid = patch_id(old, e["gid"])
            if lid in current_ids:
                continue
            alias = dict(e)
            alias["fxid"] = old
            alias["id"] = lid
            alias["legacy"] = True
            alias["name"] = f"{e['name']} (old {old})"
            db["legacy"].append(alias)
    db["legacy"].sort(key=lambda x: x["name"].upper())
    print(f"legacy aliases: {len(db['legacy'])}")

    # sanity: no patch-ID collisions
    ids = {}
    for kind in ("custom", "stock"):
        for e in db[kind]:
            if e["id"] in ids:
                print(f"WARNING: patch-ID collision {e['name']} vs {ids[e['id']]}")
            ids[e["id"]] = e["name"]

    # sanity: no fxid collision with a stock effect. The pedal keys effects by
    # fxid ALONE (independent of gid/category), so a custom fxid equal to any
    # stock fxid loads the wrong DSP on hardware -> cracking / dead effect. (This
    # is what fxids 474-483 hit, e.g. Howl 480 vs stock CoronaCho 480.)
    stock_fx = {}
    for e in db["stock"]:
        if "fxid" in e:
            stock_fx.setdefault(e["fxid"], e["name"])
    for e in db["custom"]:
        if e.get("fxid") in stock_fx:
            print(f"*** fxid COLLISION: custom {e['name']} fxid {e['fxid']} == stock "
                  f"{stock_fx[e['fxid']]} — PICK A DIFFERENT fxid, this breaks on hardware ***")

    db_json = json.dumps(db, indent=1)

    out = ROOT / "tools" / "effects_db.json"
    out.write_text(db_json)
    print(f"{len(db['custom'])} custom + {len(db['stock'])} stock effects -> {out}")

    # The patch editor embeds the DB INLINE (const DB={...};) so it works over
    # file:// without a fetch. Keep that inline copy in sync — otherwise the
    # editor silently runs a stale DB and shows freshly-built effects as
    # "unknown effect 0x…". Splice the same JSON into the HTML.
    import re
    editor = ROOT / "tools" / "patch_editor.html"
    if editor.exists():
        html = editor.read_text()
        new_html, n = re.subn(
            r"const DB=\{[\s\S]*?\n\};",
            lambda _m: "const DB=" + db_json + ";",
            html,
            count=1,
        )
        if n == 1:
            editor.write_text(new_html)
            print(f"synced inline DB -> {editor}")
        else:
            print(f"WARNING: could not find inline 'const DB={{...}};' block in {editor}")


if __name__ == "__main__":
    main()
