"""Fail a build whose audio function needs more stack than the pedal has shown it gives.

The firmware calls an effect's audio function on its own stack, of a size we
have not measured. Effects proven on the MS-70CDR use frames up to 272 bytes
(Carillon); Chimera 0.01 used 900 and was installed when the pedal froze
(2026-11-13). Until the real limit is known, keep frames at or under LIMIT.

    from stack_guard import check as stack_check
    stack_check(cl6x, cflags + defines, source_c, workdir)

Compiles the source once more with -k (keeping the assembly) into a temporary
directory and reads the compiler's own "Local Frame Size" comments.
"""
from __future__ import annotations

import re
import subprocess
import tempfile
from pathlib import Path

LIMIT = 300                                   # bytes; proven: 272 (Carillon)


def frame_sizes(cl6x: Path, flags: list[str], source: Path, cwd: Path) -> list[int]:
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run([str(cl6x), *flags, "-k", f"--asm_directory={tmp}", f"--obj_directory={tmp}", "-c", str(source)],
                       check=True, cwd=cwd, capture_output=True)
        for junk in ("compiler.opt", "linker.cmd"):
            (cwd / junk).unlink(missing_ok=True)
        asm = (Path(tmp) / (source.stem + ".asm")).read_text(errors="replace")
    return [int(m) for m in re.findall(r"Local Frame Size\s*:.*?=\s*(\d+)\s*byte", asm)]


def check(cl6x: Path, flags: list[str], source: Path, cwd: Path, limit: int = LIMIT) -> int:
    sizes = frame_sizes(cl6x, flags, source, cwd)
    worst = max(sizes) if sizes else 0
    print(f"  stack frame (largest): {worst} bytes (limit {limit})")
    if worst > limit:
        raise SystemExit(f"{source.name}: a {worst}-byte stack frame is over the {limit} bytes proven on the pedal "
                         "(build/stack_guard.py). Move large local arrays into the ctx[3] state.")
    return worst
