#!/usr/bin/env python3
"""Summarize stock ZDL init/edit handler callback patterns.

This is a lightweight reverse-engineering helper. It disassembles the requested
stock ZDLs, then scans the text for:

* `Fx_*_init` functions
* stock setup callback slots such as `state + 136`, `state + 140`, ...
* init-time `__call_stub` invocations
* init-time calls into edit/onf handlers
* handler references to callback-ish state fields 7, 21, and 31

The output is intentionally heuristic. It is meant to spot repeated stock
patterns before hand annotation, not to prove a full ABI by itself.
"""

from __future__ import annotations

import argparse
import contextlib
import io
import re
from pathlib import Path

from disassemble_zdl import DEFAULT_DIS6X, disassemble_one


LABEL_RE = re.compile(r"^[0-9a-f]{8}\s+(Fx_\S+):$")
CALL_RE = re.compile(r"\bCALLP\.[LS]\d\s+([^ ]+)")
MVK_OFFSET_RE = re.compile(r"\bMVK\.[SL]\d\s+(\d+),([AB]\d+)")
MV_RE = re.compile(r"\bMV\.[A-Z0-9]+(?:X)?\s+([AB]\d+),([AB]\d+)")
ADD_RE = re.compile(r"\bADD\.[A-Z0-9]+(?:X)?\s+([^,]+),([^,]+),([AB]\d+)")
FIELD_RE = re.compile(r"\*\+([AB]\d+)\[(7|21|31)\]")


def _function_blocks(asm_text: str) -> dict[str, list[str]]:
    blocks: dict[str, list[str]] = {}
    current: str | None = None

    for line in asm_text.splitlines():
        label = LABEL_RE.match(line)
        if label:
            current = label.group(1)
            blocks[current] = [line]
            continue
        if current is not None:
            if re.match(r"^(TEXT|DATA|BSS) Section ", line):
                current = None
                continue
            blocks[current].append(line)

    return blocks


def _setup_offsets(lines: list[str]) -> list[int]:
    state_aliases = {"A4"}
    const_regs: dict[str, int] = {}
    offsets: set[int] = set()

    for line in lines:
        move = MV_RE.search(line)
        if move and move.group(1) in state_aliases:
            state_aliases.add(move.group(2))

        const = MVK_OFFSET_RE.search(line)
        if const:
            value = int(const.group(1))
            reg = const.group(2)
            if 128 <= value <= 160 and value % 4 == 0:
                const_regs[reg] = value

        add = ADD_RE.search(line)
        if add:
            left, right, dest = add.groups()
            left = left.strip()
            right = right.strip()
            if left in state_aliases and right in const_regs:
                offsets.add(const_regs[right])
                state_aliases.add(dest)
            elif right in state_aliases and left in const_regs:
                offsets.add(const_regs[left])
                state_aliases.add(dest)

    return sorted(offsets)


def _call_targets(lines: list[str]) -> list[str]:
    targets: list[str] = []
    for line in lines:
        match = CALL_RE.search(line)
        if match:
            targets.append(match.group(1))
    return targets


def _handler_fields(lines: list[str]) -> list[int]:
    fields: set[int] = set()
    for line in lines:
        for _, field_s in FIELD_RE.findall(line):
            fields.add(int(field_s))
    return sorted(fields)


def analyze(zdl_path: Path, out_dir: Path, dis6x: Path) -> str:
    with contextlib.redirect_stdout(io.StringIO()):
        disassemble_one(zdl_path, out_dir, dis6x)
    asm_path = out_dir / f"{zdl_path.name}.asm"
    asm_text = asm_path.read_text(encoding="utf-8", errors="replace")
    blocks = _function_blocks(asm_text)

    init_blocks = {name: lines for name, lines in blocks.items() if name.endswith("_init")}
    handler_blocks = {
        name: lines
        for name, lines in blocks.items()
        if name.endswith("_edit") or name.endswith("_onf")
    }

    lines: list[str] = [f"{zdl_path.name}:"]
    if not init_blocks:
        lines.append("  init: none found")
    for name, body in sorted(init_blocks.items()):
        calls = _call_targets(body)
        setup_calls = [target for target in calls if "__call_stub" in target]
        handler_calls = [
            target
            for target in calls
            if target.startswith("Fx_") and (target.endswith("_edit") or target.endswith("_onf"))
        ]
        offsets = _setup_offsets(body)
        lines.append(f"  init {name}:")
        lines.append(f"    setup offsets: {offsets or 'none found'}")
        lines.append(f"    __call_stub calls: {len(setup_calls)}")
        lines.append(f"    handler calls: {', '.join(handler_calls) or 'none found'}")

    if not handler_blocks:
        lines.append("  handlers: none found")
    else:
        field_summary = {
            name: _handler_fields(body)
            for name, body in sorted(handler_blocks.items())
        }
        for name, fields in field_summary.items():
            if fields:
                lines.append(f"  handler {name}: state fields {fields}")

    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zdl", nargs="+", type=Path)
    parser.add_argument("--out-dir", type=Path, default=Path("/tmp/zoom-zdl-dis"))
    parser.add_argument("--dis6x", type=Path, default=DEFAULT_DIS6X)
    args = parser.parse_args()

    for zdl_path in args.zdl:
        print(analyze(zdl_path, args.out_dir, args.dis6x))


if __name__ == "__main__":
    main()
