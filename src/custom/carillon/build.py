#!/usr/bin/env python3
"""Build Carillon.ZDL from carillon.c + manifest_pedal.json (9 knobs, live screen).

Builds dist/Carillon.ZDL and its Effect Manager thumbnail (dist/Carillon.png).

Tables for carillon.c (carillon_art.h): the filters' f = 2 sin(pi F / fs) per
semitone from C2, the per-scale snap table, and the screen: the fixed bottom
half (frame line, knob labels and dials) and a 3x5 font with the Key / Scale
step names (from build/selector_metadata.py, the same names the knobs show).
carillon.c adds the header line, writes the title, tags and values as text,
and draws the bands. On other firmware the static cover drawn here stays.

    python3 src/custom/carillon/build.py
    ZVIS_EMU=<dir> python3 src/custom/carillon/build.py   # emulator copy: -DZV_HOST
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
LABELS = ("CUT", "RESO", "DECAY")
GW = 4                                       # scale-1 glyph advance
# Header text on a grid of 4-px cells, x = 1 + 4 * cell, two lines (y 0, 6).
# carillon.c fills the values (CR_CELL_* there must match).
TAGS = (("CARILLON", 0, 0), ("KEY", 0, 1), ("SCALE", 8, 1))
NAME_W = 5                                   # cells per step name


def background(cover=False):
    """cover: the still Effect Manager / other-firmware screen, complete.
    Otherwise the live background, of which only the bottom half (rows 32-63)
    is stored."""
    c = Canvas()
    c.hline(0, 127, 36)
    dials(c, LABELS)
    if cover:                                # a still of the live screen
        # sample values; no N (the shared font draws it as M)
        for text, cell, line in TAGS + (("C", 4, 1), ("MAJOR", 14, 1)):
            c.draw_text(text, 1 + GW * cell, 6 * line)
        c.hline(0, 127, 12)
        for k in range(32):                  # bands: levels under a low-pass shape
            x = 4 * k
            d = (k * 72 / 31 - 52) / 12
            shape = 1 / (1 + max(d, 0) ** 4 * 6.5) + 0.9 / (1 + d * d * 9)
            lvl = shape * (0.35 + 0.65 * abs(math.sin(k * 1.7))) / 1.9
            h = int(20 * lvl + 0.5)
            for xx in range(x, x + 3):
                for y in range(35 - h, 35):
                    c.pixels[y][xx] = 1
            y = 34 - int(20 * shape / 1.9 + 0.5)
            for xx in range(x, x + 3):
                c.pixels[max(y, 14)][xx] ^= 1
    return c


# The runtime font: index 0 = space, 1..10 = digits, then whatever the names
# and tags use. N is redrawn locally (the shared font's N is the same as its
# M); # isn't in the shared font.
FONT_N = ["110", "101", "101", "101", "101"]
FONT_SHARP = ["101", "111", "101", "111", "101"]
LOCAL = {"N": FONT_N, "#": FONT_SHARP}

# Scales as pitch classes; order = build/selector_metadata.py 'Carillon' Scale
# (Free and Chrom don't snap).
SCALES = ((), tuple(range(12)), (0, 2, 4, 5, 7, 9, 11), (0, 2, 3, 5, 7, 8, 10),
          (0, 2, 3, 5, 7, 9, 10), (0, 2, 4, 7, 9), (0, 3, 5, 7, 10), (0, 2, 4, 6, 8, 10),
          (0, 7), (0,))


def snap_table():
    """Per scale, per pitch class: the step to the nearest note in the scale."""
    out = []
    for sc in SCALES:
        for pc in range(12):
            if not sc:
                out.append(0)
                continue
            cands = [n + o for n in sc for o in (-12, 0, 12)]
            out.append(min(cands, key=lambda n: (abs(n - pc), n)) - pc)
    return out


def f_table():
    """f = 2 sin(pi F / fs) for each semitone from C2 (65.41 Hz), 0..86."""
    return [2 * math.sin(math.pi * 65.406392 * 2 ** (s / 12) / 44100) for s in range(87)]


def font_and_names():
    names = {k: [n.upper() for n in (g["label"] for g in SELECTORS["Carillon"][k])]
             for k in ("Key", "Scale")}
    assert len(names["Scale"]) == len(SCALES)
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
    tables = {"Key": b"".join(enc(n, 2) for n in names["Key"]),
              "Scale": b"".join(enc(n) for n in names["Scale"])}
    tags = bytearray(64)                     # the header's fixed words, as cells
    for text, cell, line in TAGS:
        tags[32 * line + cell:32 * line + cell + len(text)] = enc(text, len(text))
    tables["Tags"] = bytes(tags)
    return font, tables, enc


def main() -> None:
    manifest = json.loads((HERE / "manifest_pedal.json").read_text())
    write_param_header(manifest, HERE / "carillon_params.h", "CARILLON")

    font, tables, enc = font_and_names()
    (HERE / "carillon_art.h").write_text(
        "/* generated by build.py */\n"
        f"#define CAR_NAME_W {NAME_W}u\n"
        + c_bytes("CAR_BG", raw(background())[512:]) + c_bytes("CAR_FONT", font)
        + c_bytes("CAR_TAGS", tables["Tags"])
        + c_bytes("CAR_KEY_N", tables["Key"]) + c_bytes("CAR_SCALE_N", tables["Scale"])
        + "static const signed char CAR_SNAP[%d] = { %s };\n" % (12 * len(SCALES), ", ".join(map(str, snap_table())))
        + "static const float CAR_F[87] = {\n    %s };\n" % ",\n    ".join(
            ", ".join(f"{v:.9g}f" for v in f_table()[i:i + 6]) for i in range(0, 87, 6)))

    emu = os.environ.get("ZVIS_EMU")
    effect_name = manifest["effect_name"]
    audio_func = manifest["audio_func_name"]
    out_dir = Path(emu) if emu else ROOT / "dist"
    out_dir.mkdir(parents=True, exist_ok=True)
    obj = HERE / f"{effect_name.lower()}{'-emu' if emu else ''}.obj"
    out_zdl = out_dir / f"{effect_name}.ZDL"

    print(f"[carillon] compiling carillon.c -> {obj.name}")
    subprocess.run([str(TI_ROOT / "bin" / "cl6x"), *CFLAGS, *(["--define=ZV_HOST"] if emu else []),
                    f"--define=CARILLON_AUDIO_FUNC={audio_func}", "-c", str(HERE / "carillon.c"),
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
    # calls itself (ZDL_GetLabel_i, generated into carillon_params.h).
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
    print(f"\n[carillon] done -> {out_zdl}")


if __name__ == "__main__":
    main()
