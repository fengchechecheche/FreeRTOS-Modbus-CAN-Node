#!/usr/bin/env python3
"""Check the bounded STM32F446RE resource contract from ELF and MAP files."""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys
from dataclasses import dataclass

FLASH_BYTES = 512 * 1024
RAM_BYTES = 128 * 1024
FLASH_LIMIT_BYTES = 384 * 1024
STATIC_RAM_LIMIT_BYTES = 96 * 1024
EXPECTED_HEAP_BYTES = 0
EXPECTED_MSP_BYTES = 1024
BANNED_SOURCE_CALLS = re.compile(
    r"\b(?:malloc|calloc|realloc|free|pvPortMalloc|vPortFree)\s*\("
)
BANNED_ELF_SYMBOLS = {
    "malloc",
    "calloc",
    "realloc",
    "free",
    "pvPortMalloc",
    "vPortFree",
}


@dataclass(frozen=True)
class SizeSummary:
    text: int
    data: int
    bss: int

    @property
    def flash(self) -> int:
        return self.text + self.data

    @property
    def static_ram(self) -> int:
        # GNU size includes the NOLOAD ._user_heap_stack section in bss.
        return self.data + self.bss


def run_text(command: list[str]) -> str:
    result = subprocess.run(command, check=True, text=True, capture_output=True)
    return result.stdout


def parse_size(elf: pathlib.Path) -> SizeSummary:
    output = run_text(["arm-none-eabi-size", str(elf)])
    lines = [line.split() for line in output.splitlines() if line.strip()]
    if len(lines) < 2 or len(lines[-1]) < 3:
        raise ValueError(f"cannot parse arm-none-eabi-size output for {elf}")
    return SizeSummary(*(int(value) for value in lines[-1][:3]))


def parse_linker_value(map_path: pathlib.Path, symbol: str) -> int:
    pattern = re.compile(
        rf"^\s*0x[0-9a-fA-F]+\s+{re.escape(symbol)}\s*=\s*"
        rf"(0x[0-9a-fA-F]+|[0-9]+)\s*$"
    )
    for line in map_path.read_text(encoding="utf-8", errors="replace").splitlines():
        match = pattern.match(line)
        if match:
            return int(match.group(1), 0)
    raise ValueError(f"{symbol} not found in {map_path}")


def scan_project_sources(source_root: pathlib.Path) -> list[str]:
    failures: list[str] = []
    for relative_root in ("app", "bsp", "config", "Core"):
        root = source_root / relative_root
        for path in sorted(root.rglob("*")):
            if path.suffix not in {".c", ".h"}:
                continue
            for line_number, line in enumerate(
                path.read_text(encoding="utf-8", errors="replace").splitlines(), 1
            ):
                if BANNED_SOURCE_CALLS.search(line):
                    failures.append(f"{path.relative_to(source_root)}:{line_number}")
    return failures


def linked_banned_symbols(elf: pathlib.Path) -> list[str]:
    output = run_text(["arm-none-eabi-nm", "--defined-only", str(elf)])
    found: set[str] = set()
    for line in output.splitlines():
        fields = line.split()
        if fields and fields[-1] in BANNED_ELF_SYMBOLS:
            found.add(fields[-1])
    return sorted(found)


def percent(value: int, total: int) -> float:
    return (100.0 * value) / total


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=pathlib.Path, required=True)
    parser.add_argument("--debug-elf", type=pathlib.Path, required=True)
    parser.add_argument("--debug-map", type=pathlib.Path, required=True)
    parser.add_argument("--release-elf", type=pathlib.Path, required=True)
    parser.add_argument("--release-map", type=pathlib.Path, required=True)
    args = parser.parse_args()

    failures: list[str] = []
    summaries: dict[str, SizeSummary] = {}
    for name, elf, map_path in (
        ("Debug", args.debug_elf, args.debug_map),
        ("Release", args.release_elf, args.release_map),
    ):
        summary = parse_size(elf)
        summaries[name] = summary
        heap_bytes = parse_linker_value(map_path, "_Min_Heap_Size")
        msp_bytes = parse_linker_value(map_path, "_Min_Stack_Size")
        if heap_bytes != EXPECTED_HEAP_BYTES:
            failures.append(f"{name}: linker heap is {heap_bytes} B, expected 0 B")
        if msp_bytes != EXPECTED_MSP_BYTES:
            failures.append(f"{name}: MSP reserve is {msp_bytes} B, expected 1024 B")
        if summary.flash > FLASH_LIMIT_BYTES:
            failures.append(f"{name}: Flash {summary.flash} B exceeds 384 KiB")
        if summary.static_ram > STATIC_RAM_LIMIT_BYTES:
            failures.append(f"{name}: static RAM {summary.static_ram} B exceeds 96 KiB")
        symbols = linked_banned_symbols(elf)
        if symbols:
            failures.append(
                f"{name}: unexpected allocator symbols: {', '.join(symbols)}"
            )

    calls = scan_project_sources(args.source_root)
    if calls:
        failures.append("project allocator calls: " + ", ".join(calls))

    for name in ("Debug", "Release"):
        summary = summaries[name]
        print(
            f"{name}: text={summary.text} data={summary.data} bss={summary.bss} "
            f"flash={summary.flash}/{FLASH_BYTES} "
            f"({percent(summary.flash, FLASH_BYTES):.2f}%) "
            f"static_ram={summary.static_ram}/{RAM_BYTES} "
            f"({percent(summary.static_ram, RAM_BYTES):.2f}%)"
        )
    debug = summaries["Debug"]
    release = summaries["Release"]
    print(
        "Debug-Release: "
        f"text={debug.text - release.text:+d} data={debug.data - release.data:+d} "
        f"bss={debug.bss - release.bss:+d} flash={debug.flash - release.flash:+d} "
        f"static_ram={debug.static_ram - release.static_ram:+d}"
    )

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}", file=sys.stderr)
        return 1

    print("PASS: static-only linker and resource budget contract")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
