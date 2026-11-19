#!/usr/bin/env python3
"""Build Chopdly.ZDL from chopdly.c + manifest_pedal.json (9 knobs, live screen).

Builds dist/Chopdly.ZDL and its Effect Manager thumbnail (dist/Chopdly.png).

The screen (zvis, MS-70CDR 2.10): this script draws the fixed bottom half
(knob labels and dials; chopdly.c adds the header line and writes the title
and ORDER / PITCH / REV / STR tags as text) and a 3x5 font with the step names (from build/selector_metadata.py,
the same names the knobs show) into chopdly_art.h; chopdly.c writes the live
values into the header and draws the waveform, cuts and playhead. On other
firmware the static cover drawn here stays.

    python3 src/custom/chopdly/build.py
    ZVIS_EMU=<dir> python3 src/custom/chopdly/build.py   # emulator copy: -DZV_HOST
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
LABELS = ("TIME", "CHOPS", "ORDER")
GW = 4                                       # scale-1 glyph advance
# Header text sits on a grid of 4-px cells, x = 1 + 4 * cell, two lines (y 0, 6).
# Static tags here; chopdly.c fills the values (CD_COL_* there must match).
TAGS = (("CHOPDLY", 0, 0), ("ORDER", 21, 0), ("PITCH", 0, 1), ("REV", 12, 1), ("STR", 22, 1))
NAME_W = 5                                   # cells per step name


def background(cover=False):
    """cover: the still Effect Manager / other-firmware screen, complete.
    Otherwise the live background, of which only the bottom half (rows 32-63)
    is stored: chopdly.c draws the header line and writes the tags as text."""
    c = Canvas()
    if cover:
        for text, cell, line in TAGS:
            c.draw_text(text, 1 + GW * cell, 6 * line)
        c.hline(0, 127, 12)
    c.hline(0, 127, 36)
    dials(c, LABELS)
    if cover:                                # a still of the live screen
        cuts = [0, 21, 37, 66, 90, 128]
        for x in range(128):
            h = int(7 * abs(math.sin(x * 0.37)) * math.exp(-((x - max(k for k in cuts if k <= x)) / 14.0)) + 1)
            c.vline(x, 23 - h, 23 + h)
        for k in cuts[1:-1]:
            for y in range(15, 32, 2):
                c.pixels[y][k] = 1
        for x in range(37, 66):              # the chop playing, inverted
            for y in range(15, 32):
                c.pixels[y][x] ^= 1
        for x in range(90, 128):             # the slot it plays in
            c.pixels[33][x] = c.pixels[34][x] = 1
        for text, cell, line in (("5 CHOPS", 13, 0), ("SWAP", 27, 0), ("OCT+", 6, 1),
                                 ("HALF", 16, 1), ("1/2", 26, 1)):   # sample values, no N/M
            c.draw_text(text, 1 + GW * cell, 6 * line)
    return c


# The runtime font: index 0 = space, 1..10 = digits (chopdly.c relies on both),
# then whatever the step names use. N is redrawn locally: the shared font's N
# is the same as its M ("TRAMS", "UMITY").
FONT_N = ["110", "101", "101", "101", "101"]


def font_and_names():
    names = {k: [n.upper() for n in (g["label"] for g in SELECTORS["Chopdly"][k])]
             for k in ("Order", "Pitch", "Reverse", "Stretch")}
    used = {ch for v in names.values() for n in v for ch in n} | set("TRANSCHOP") | set("".join(t for t, _, _ in TAGS))
    chars = " 0123456789" + "".join(sorted(used - set(" 0123456789")))
    font = bytearray()
    for ch in chars:
        rows = FONT_N if ch == "N" else Canvas._FONT.get(ch)
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
    write_param_header(manifest, HERE / "chopdly_params.h", "CHOPDLY")

    font, tables, enc = font_and_names()
    (HERE / "chopdly_art.h").write_text(
        "/* generated by build.py */\n"
        f"#define CHOP_NAME_W {NAME_W}u\n"
        + c_bytes("CHOP_BG", raw(background())[512:]) + c_bytes("CHOP_FONT", font)
        + c_bytes("CHOP_TAGS", tables["Tags"])
        + c_bytes("CHOP_ORDER_N", tables["Order"]) + c_bytes("CHOP_PITCH_N", tables["Pitch"])
        + c_bytes("CHOP_REV_N", tables["Reverse"]) + c_bytes("CHOP_STR_N", tables["Stretch"])
        + c_bytes("CHOP_W_TRANS", enc("TRANS")) + c_bytes("CHOP_W_CHOPS", enc("CHOPS")))

    emu = os.environ.get("ZVIS_EMU")
    effect_name = manifest["effect_name"]
    audio_func = manifest["audio_func_name"]
    out_dir = Path(emu) if emu else ROOT / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = HERE / f"{effect_name.lower()}{'-emu' if emu else ''}.obj"
    out_zdl = out_dir / f"{effect_name}.ZDL"

    print(f"[chopdly] compiling chopdly.c -> {obj.name}")
    subprocess.run([str(TI_ROOT / "bin" / "cl6x"), *CFLAGS, *(["--define=ZV_HOST"] if emu else []),
                    f"--define=CHOPDLY_AUDIO_FUNC={audio_func}", "-c", str(HERE / "chopdly.c"),
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
    # calls itself (ZDL_GetLabel_i, generated into chopdly_params.h, as in Tonic).
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
    print(f"\n[chopdly] done -> {out_zdl}")


if __name__ == "__main__":
    main()
