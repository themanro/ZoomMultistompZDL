#!/usr/bin/env python3
"""Build the Ceiling probe (FXID 918) to build/probes/ceiling/, never dist/.

    python3 -B src/hardware_probes/ceiling/build.py
    python3 -B src/hardware_probes/ceiling/validate.py   # calibrate in the emulator, rebuild, check

The emulated cycles per loop iteration come from calib.json (written by
validate.py), so kCyc x 1000 is the emulated cost of the burn.
"""
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
sys.path[:0] = [str(ROOT / "build"), str(ROOT / "src" / "airwindows" / "common"), str(ROOT / "src" / "custom" / "common")]

from linker import LinkerConfig, ObjFile, link, params_from_manifest  # noqa: E402
from manifest_params import write_param_header  # noqa: E402
from zvis_build import Canvas, dials, encode_zoom_rle, em_thumbnail  # noqa: E402
from zdl_size_guard import check as size_check  # noqa: E402
from stack_guard import check as stack_check  # noqa: E402

TI_ROOT = Path("/Applications/ti/ti-cgt-c6000_8.5.0.LTS")
OUT = ROOT / "build" / "probes" / "ceiling"
CFLAGS = ["--c99", "--opt_level=2", "--interrupt_threshold=1", "--disable_software_pipeline", "-mv6740", "--abi=eabi",
          "--mem_model:data=far", f"--include_path={TI_ROOT}/include", f"--include_path={ROOT}/src/airwindows/common"]


def cover():
    c = Canvas()
    c.draw_text("CEILING", 1, 0)
    c.draw_text("CPU PROBE: TURN KCYC UP", 1, 8)
    c.draw_text("UNTIL THE TONE CLICKS", 1, 16)
    c.hline(0, 127, 36)
    dials(c, ("KCYC", "MODE", "TONE"))
    return c


def main(out_dir: Path = OUT) -> Path:
    manifest = json.loads((HERE / "manifest.json").read_text())
    write_param_header(manifest, HERE / "ceiling_params.h", "CEILING")
    calib = json.loads((HERE / "calib.json").read_text()) if (HERE / "calib.json").exists() else {}
    defs = [f"--define=CEIL_ITER_ALU={calib.get('alu', 4.0)}f", f"--define=CEIL_ITER_MEM={calib.get('mem', 4.0)}f"]
    out_dir.mkdir(parents=True, exist_ok=True)
    name, func = manifest["effect_name"], manifest["audio_func_name"]
    obj = out_dir / "ceiling.obj"
    subprocess.run([str(TI_ROOT / "bin" / "cl6x"), *CFLAGS, *defs, f"--define=CEIL_AUDIO_FUNC={func}", "-c",
                    str(HERE / "ceiling.c"), f"--output_file={obj}"], check=True, cwd=out_dir)
    for junk in ("compiler.opt", "linker.cmd"):
        (out_dir / junk).unlink(missing_ok=True)
    stack_check(TI_ROOT / "bin" / "cl6x", [*CFLAGS, *defs, f"--define=CEIL_AUDIO_FUNC={func}"], HERE / "ceiling.c", out_dir)
    o = ObjFile(obj)
    externals = {s["name"] for s in o.symbols if s["name"] and s["shndx"] == 0}
    assert not externals, f"unexpected externals {externals}"
    for sec in o.sections:
        if sec["name"] in (".text", ".bss", ".far", ".fardata") or sec["name"].startswith(".switch"):
            assert sec["size"] == 0, f"unexpected {sec['name']} ({sec['size']} bytes)"
    for group in json.loads((ROOT / "tools" / "effects_db.json").read_text()).values():
        if isinstance(group, list):
            assert not any(e.get("gid") == 8 and e.get("fxid") == manifest["fxid"] for e in group), "FXID taken"
    zdl = out_dir / f"{name}.ZDL"
    link(LinkerConfig(
        materialize_init=True, effect_name=name, screen_image=encode_zoom_rle(cover()), audio_func_name=func,
        gid=manifest["gid"], fxid=manifest["fxid"], params=params_from_manifest(manifest["params"]), obj_path=obj,
        output_path=zdl, fxid_version=manifest["fxid_version"].encode("ascii"), flags_byte=manifest["flags_byte"],
        dsp_cost=manifest["dsp_cost"], drop_unused_divf=True, knob_positions=[(2, 14, 46), (3, 55, 46), (4, 96, 46)],
        use_object_edit_handlers=False, synthesize_linesel_edit_handlers=True, synth_edit_start_index=2,
        knob3_blob_path="/tmp/__nonexistent__"))
    size_check(zdl)
    em_thumbnail(zdl)
    print(f"[ceiling] -> {zdl}  (iteration: alu {calib.get('alu', '?')}, mem {calib.get('mem', '?')} emulated cycles)")
    return zdl


if __name__ == "__main__":
    main()
