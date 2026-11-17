#!/usr/bin/env python3
"""Build Chimera.ZDL from chimera.c + manifest_pedal.json (9 knobs, live screen).

Builds dist/Chimera.ZDL and its Effect Manager thumbnail (dist/Chimera.png).
The vowel shapes come from design.py (chimera_vowels.h, checked in; rerun it
only to refit them).

The screen (zvis, MS-70CDR 2.10): this script draws the fixed bottom half
(frame line, knob labels and dials) and a 3x5 font with the Move step names
(from build/selector_metadata.py) and the vowel letters into chimera_art.h;
chimera.c writes the header and draws the airway. On other firmware the
static cover drawn here stays.

    python3 src/custom/chimera/build.py
    ZVIS_EMU=<dir> python3 src/custom/chimera/build.py   # emulator copy: -DZV_HOST
"""

from __future__ import annotations

import json
import math
import os
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent.parent
sys.path[:0] = [str(ROOT / "build"), str(ROOT / "src" / "airwindows" / "common"), str(HERE.parent / "common")]

from linker import LinkerConfig, ObjFile, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402
from zvis_build import Canvas, raw, c_bytes, dials, encode_zoom_rle, em_thumbnail  # noqa: E402
from zdl_size_guard import check as size_check  # noqa: E402
from stack_guard import check as stack_check  # noqa: E402
from selector_metadata import SELECTORS  # noqa: E402

TI_ROOT = Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS")
CFLAGS = ["--c99", "--opt_level=2", "--interrupt_threshold=1", "--disable_software_pipeline", "-mv6740", "--abi=eabi", "--mem_model:data=far",
          f"--include_path={TI_ROOT}/include", f"--include_path={ROOT}/src/airwindows/common",
          f"--include_path={HERE.parent / 'common'}"]
LABELS = ("VOWEL", "OPEN", "BODY")
GW = 4                                       # scale-1 glyph advance
# Header text on a grid of 4-px cells, x = 1 + 4 * cell, two lines (y 0, 6).
# chimera.c fills the values (TH_CELL_* there must match).
TAGS = (("CHIMERA", 0, 0), ("MOVE", 0, 1), ("VOWEL", 12, 1))
NAME_W = 4                                   # cells per step name
VOWEL_LETTERS = "UOAEI"


def vowel_areas():
    """chimera_vowels.h back into Python (cm^2 per section, per vowel)."""
    txt = (HERE / "chimera_vowels.h").read_text()
    rows = re.findall(r"^    \{ ([^}]*) \},", txt, flags=re.M)
    return [[math.exp(float(v.rstrip("f"))) for v in r.split(",")] for r in rows]


def draw_airway(c, areas, mid=24, x0=8, sw=7, hmax=10):
    hs = [max(1, min(hmax, round(math.sqrt(a) * 3.6))) for a in areas]
    prev = None
    for k, h in enumerate(hs):
        xa, xb = x0 + sw * k, x0 + sw * k + sw - 1
        for x in range(xa, xb + 1):
            c.pixels[mid - h][x] = c.pixels[mid + h][x] = 1
        if prev is not None:
            for y in range(min(prev, h), max(prev, h) + 1):
                c.pixels[mid - y][xa] = c.pixels[mid + y][xa] = 1
        prev = h
    for y in range(mid - 2, mid + 3):
        c.pixels[y][x0 - 1] = 1
    return hs


def background(cover=False):
    """cover: the still Effect Manager / other-firmware screen, complete.
    Otherwise the live background, of which only the bottom half (rows 32-63)
    is stored."""
    c = Canvas()
    c.hline(0, 127, 36)
    dials(c, LABELS)
    if cover:                                # a still of the live screen, on 'a'
        for text, cell, line in TAGS + (("PICK", 5, 1), ("A", 18, 1)):
            c.draw_text(text, 1 + GW * cell, 6 * line)
        c.hline(0, 127, 12)
        hs = draw_airway(c, vowel_areas()[2])
        for k, h in enumerate(hs):           # some sound inside
            f = int((h - 1) * math.sqrt(0.2 + 0.7 * abs(math.sin(k * 0.7))))
            xa = 8 + 7 * k + 2
            for y in range(24 - f + 1, 24 + f):
                for x in range(xa, xa + 3):
                    c.pixels[y][x] = 1
    return c


def font_and_names():
    names = {"Move": [g["label"].upper() for g in SELECTORS["Chimera"]["Move"]]}
    used = {ch for v in names.values() for n in v for ch in n} | set("".join(t for t, _, _ in TAGS)) | set(VOWEL_LETTERS)
    chars = " 0123456789" + "".join(sorted(used - set(" 0123456789")))
    font = bytearray()
    for ch in chars:
        rows = Canvas._FONT.get(ch)
        assert rows, f"no glyph for {ch!r}"
        for col in range(3):
            font.append(sum(1 << r for r in range(5) if rows[r][col] == "1"))
    def enc(text, w=NAME_W):
        assert len(text) <= w, text
        return bytes(chars.index(ch) for ch in text.ljust(w))
    tables = {"Move": b"".join(enc(n) for n in names["Move"]),
              "Vowel": bytes(chars.index(ch) for ch in VOWEL_LETTERS)}
    tags = bytearray(64)                     # the header's fixed words, as cells
    for text, cell, line in TAGS:
        tags[32 * line + cell:32 * line + cell + len(text)] = enc(text, len(text))
    tables["Tags"] = bytes(tags)
    return font, tables


def main() -> None:
    manifest = json.loads((HERE / "manifest_pedal.json").read_text())
    write_param_header(manifest, HERE / "chimera_params.h", "CHIMERA")

    font, tables = font_and_names()
    (HERE / "chimera_art.h").write_text(
        "/* generated by build.py */\n"
        f"#define THR_NAME_W {NAME_W}u\n"
        + c_bytes("THR_BG", raw(background())[512:]) + c_bytes("THR_FONT", font)
        + c_bytes("THR_TAGS", tables["Tags"])
        + c_bytes("THR_MOVE_N", tables["Move"]) + c_bytes("THR_VOWEL_N", tables["Vowel"]))

    emu = os.environ.get("ZVIS_EMU")
    effect_name = manifest["effect_name"]
    audio_func = manifest["audio_func_name"]
    out_dir = Path(emu) if emu else ROOT / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = HERE / f"{effect_name.lower()}{'-emu' if emu else ''}.obj"
    out_zdl = out_dir / f"{effect_name}.ZDL"

    print(f"[chimera] compiling chimera.c -> {obj.name}")
    subprocess.run([str(TI_ROOT / "bin" / "cl6x"), *CFLAGS, *(["--define=ZV_HOST"] if emu else []),
                    f"--define=CHIMERA_AUDIO_FUNC={audio_func}", "-c", str(HERE / "chimera.c"),
                    f"--output_file={obj}"], check=True, cwd=HERE)
    for junk in ("compiler.opt", "linker.cmd"):
        (HERE / junk).unlink(missing_ok=True)
    stack_check(TI_ROOT / "bin" / "cl6x", [*CFLAGS, f"--define=CHIMERA_AUDIO_FUNC={audio_func}"], HERE / "chimera.c", HERE)

    o = ObjFile(obj)                         # the zvis rules (src/custom/common/zvis_build.py)
    externals = {s["name"] for s in o.symbols if s["name"] and s["shndx"] == 0}
    assert externals <= {f"_{audio_func}_Info"}, f"unexpected externals {externals}"
    for sec in o.sections:
        if sec["name"] in (".bss", ".far", ".fardata") or sec["name"].startswith(".switch"):
            assert sec["size"] == 0, f"unexpected {sec['name']} ({sec['size']} bytes)"
    audio_idx = next(s["idx"] for s in o.sections if s["name"] == ".audio")
    stray = [s["name"] for s in o.symbols if s["type"] == 2 and s["shndx"] and s["size"]
             and s["shndx"] != audio_idx and not s["name"].startswith("ZDL_GetLabel_")]
    assert not stray, f"functions outside .audio (a cross-section call froze the pedal): {stray}"

    link(LinkerConfig(
        materialize_init=True,
        effect_name=effect_name,
        screen_image=encode_zoom_rle(background(cover=True)),
        audio_func_name=audio_func,
        gid=manifest["gid"],
        fxid=manifest["fxid"],
        params=params_from_manifest(manifest["params"]),
        obj_path=obj,
        output_path=out_zdl,
        fxid_version=manifest.get("fxid_version", "1.00").encode("ascii"),
        flags_byte=manifest.get("flags_byte", 0x01),
        audio_nop=manifest.get("audio_nop", False),
        dsp_cost=manifest.get("dsp_cost", 20.0),
        knob_positions=[(2, 14, 46), (3, 55, 46), (4, 96, 46)],
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__",
    ))
    size_check(out_zdl)
    if not emu:
        em_thumbnail(out_zdl)                # same-name PNG: Effect Manager's thumbnail
    print(f"\n[chimera] done -> {out_zdl}")


if __name__ == "__main__":
    main()
