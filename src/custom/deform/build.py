#!/usr/bin/env python3
"""Build Deform.ZDL from deform.c + manifest_pedal.json (9 knobs, live screen).

Builds dist/Deform.ZDL and its Effect Manager thumbnail (dist/Deform.png).

The screen (zvis, MS-70CDR 2.10): this script draws the fixed bottom half
(frame line, knob labels and dials) and a 3x5 font with the Split / Mode
step names (from build/selector_metadata.py, the same names the knobs show)
into deform_art.h; deform.c adds the header line, writes the title, tags and
values as text, and draws the curve, the gain reduction and the scope. On
other firmware the static cover drawn here stays.

    python3 src/custom/deform/build.py
    ZVIS_EMU=<dir> python3 src/custom/deform/build.py   # emulator copy: -DZV_HOST
"""

from __future__ import annotations

import json
import math
import os
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
from selector_metadata import SELECTORS  # noqa: E402

TI_ROOT = Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS")
CFLAGS = ["--c99", "--opt_level=2", "--interrupt_threshold=1", "--disable_software_pipeline", "-mv6740", "--abi=eabi", "--mem_model:data=far",
          f"--include_path={TI_ROOT}/include", f"--include_path={ROOT}/src/airwindows/common",
          f"--include_path={HERE.parent / 'common'}"]
LABELS = ("DRIVE", "CRUSH", "COMP")
GW = 4                                       # scale-1 glyph advance
# Header text on a grid of 4-px cells, x = 1 + 4 * cell, two lines (y 0, 6).
# deform.c fills the values (DF_CELL_* there must match).
TAGS = (("DEFORM", 0, 0), ("SPLIT", 0, 1), ("MODE", 12, 1))
NAME_W = 5                                   # cells per step name


def soft_bits(x, bits):
    """deform.c's crush on one value: squeeze, rounded steps, expand."""
    lv = 2 ** (bits - 1)
    y = 5 * x / (1 + 4 * abs(x))
    v = y * lv + 0.5
    n = math.floor(v)
    fr = v - n - 0.5
    q = max(-1.2, min(1.2, (n + 4 * fr ** 3) / lv))
    return q / (5 - 4 * abs(q))


def sat(x):
    x = max(-3, min(3, x))
    return x * (27 + x * x) / (27 + 9 * x * x)


def background(cover=False):
    """cover: the still Effect Manager / other-firmware screen, complete.
    Otherwise the live background, of which only the bottom half (rows 32-63)
    is stored."""
    c = Canvas()
    c.hline(0, 127, 36)
    dials(c, LABELS)
    if cover:                                # a still of the live screen
        for text, cell, line in TAGS + (("MID", 6, 1), ("BIT", 17, 1)):
            c.draw_text(text, 1 + GW * cell, 6 * line)
        c.hline(0, 127, 12)
        g = 2 ** (6 * 0.35)                  # the curve at the defaults
        ys = [soft_bits(sat(g * (k - 15) / 30 + 0.1) - sat(0.1), 3.5) for k in range(31)]
        m = max(abs(v) for v in ys)
        pts = [24 - round(v * 10 / m) for v in ys]
        for k in range(31):
            lo, hi = sorted((pts[k], pts[k - 1] if k else pts[k]))
            for y in range(lo, hi + 1):
                c.pixels[y][k] = 1
        for y in range(14, 20):              # gain reduction
            c.pixels[y][34] = c.pixels[y][35] = 1
        prev = None
        for k in range(90):                  # a crushed cycle
            kk = k - k % 3                   # held, then put on a few levels
            v = sat(1.4 * math.sin(2 * math.pi * kk / 45) + 0.4 * math.sin(2 * math.pi * kk / 15))
            v = round(v * 5) / 5
            y = 24 - round(v * 10)
            lo, hi = sorted((y, prev if prev is not None else y))
            for yy in range(lo, hi + 1):
                c.pixels[yy][38 + k] = 1
            prev = y
    return c


# The runtime font: index 0 = space, 1..10 = digits, then whatever the names
# and tags use. N is redrawn locally (the shared font's N is the same as its M).
FONT_N = ["110", "101", "101", "101", "101"]
LOCAL = {"N": FONT_N}

def font_and_names():
    names = {k: [n.upper() for n in (g["label"] for g in SELECTORS["Deform"][k])]
             for k in ("Split", "Mode")}
    used = {ch for v in names.values() for n in v for ch in n} | set("".join(t for t, _, _ in TAGS))
    chars = " 0123456789" + "".join(sorted(used - set(" 0123456789")))
    font = bytearray()
    for ch in chars:
        rows = LOCAL.get(ch) or Canvas._FONT.get(ch)
        assert rows, f"no glyph for {ch!r}"
        for col in range(3):
            font.append(sum(1 << r for r in range(5) if rows[r][col] == "1"))
    def enc(text, w=NAME_W):
        assert len(text) <= w, text
        return bytes(chars.index(ch) for ch in text.ljust(w))
    tables = {k: b"".join(enc(n) for n in v) for k, v in names.items()}
    tags = bytearray(64)                     # the header's fixed words, as cells
    for text, cell, line in TAGS:
        tags[32 * line + cell:32 * line + cell + len(text)] = enc(text, len(text))
    tables["Tags"] = bytes(tags)
    return font, tables, enc


def main() -> None:
    manifest = json.loads((HERE / "manifest_pedal.json").read_text())
    write_param_header(manifest, HERE / "deform_params.h", "DEFORM")

    font, tables, enc = font_and_names()
    (HERE / "deform_art.h").write_text(
        "/* generated by build.py */\n"
        f"#define DEF_NAME_W {NAME_W}u\n"
        + c_bytes("DEF_BG", raw(background())[512:]) + c_bytes("DEF_FONT", font)
        + c_bytes("DEF_TAGS", tables["Tags"])
        + c_bytes("DEF_SPLIT_N", tables["Split"]) + c_bytes("DEF_MODE_N", tables["Mode"]))

    emu = os.environ.get("ZVIS_EMU")
    effect_name = manifest["effect_name"]
    audio_func = manifest["audio_func_name"]
    out_dir = Path(emu) if emu else ROOT / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = HERE / f"{effect_name.lower()}{'-emu' if emu else ''}.obj"
    out_zdl = out_dir / f"{effect_name}.ZDL"

    print(f"[deform] compiling deform.c -> {obj.name}")
    subprocess.run([str(TI_ROOT / "bin" / "cl6x"), *CFLAGS, *(["--define=ZV_HOST"] if emu else []),
                    f"--define=DEFORM_AUDIO_FUNC={audio_func}", "-c", str(HERE / "deform.c"),
                    f"--output_file={obj}"], check=True, cwd=HERE)
    for junk in ("compiler.opt", "linker.cmd"):
        (HERE / junk).unlink(missing_ok=True)

    o = ObjFile(obj)                         # the zvis rules (src/custom/common/zvis_build.py)
    externals = {s["name"] for s in o.symbols if s["name"] and s["shndx"] == 0}
    assert externals <= {f"_{audio_func}_Info"}, f"unexpected externals {externals}"
    for sec in o.sections:
        if sec["name"] in (".bss", ".far", ".fardata") or sec["name"].startswith(".switch"):
            assert sec["size"] == 0, f"unexpected {sec['name']} ({sec['size']} bytes)"
    audio_idx = next(s["idx"] for s in o.sections if s["name"] == ".audio")
    # The only code allowed outside .audio: the knob-name callbacks the firmware
    # calls itself (ZDL_GetLabel_i, generated into deform_params.h).
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
        drop_unused_divf=True,             # the linker keeps divf if the .obj calls it
        knob_positions=[(2, 14, 46), (3, 55, 46), (4, 96, 46)],
        use_object_edit_handlers=False,
        synthesize_linesel_edit_handlers=True,
        synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__",
    ))
    size_check(out_zdl)
    if not emu:
        em_thumbnail(out_zdl)                # same-name PNG: Effect Manager's thumbnail
    print(f"\n[deform] done -> {out_zdl}")


if __name__ == "__main__":
    main()
