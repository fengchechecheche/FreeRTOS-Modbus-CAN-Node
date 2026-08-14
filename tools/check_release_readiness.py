#!/usr/bin/env python3
"""Check the bounded P5 software-source release policy."""

from __future__ import annotations

import argparse
import hashlib
import pathlib
import re
import sys
from typing import Iterable

PACKAGE_LICENSE_SHA256 = (
    "2fe52ca80ec3d84064631a051a914b418aa65b9126ae5131ea323213a8be5aa2"
)
REQUIRED_FILES = (
    "LICENSE",
    "THIRD_PARTY_NOTICES.md",
    "LICENSES/STM32CubeF4-1.28.3-Package_license.md",
    "Drivers/CMSIS/LICENSE.txt",
    "Drivers/CMSIS/Device/ST/STM32F4xx/LICENSE.txt",
    "Drivers/STM32F4xx_HAL_Driver/LICENSE.txt",
    "Middlewares/Third_Party/FreeRTOS/Source/LICENSE",
    "Middlewares/Third_Party/FreeRTOS/Source/st_readme.txt",
    "docs/release_readiness.md",
)
NOTICE_MARKERS = (
    "CMSIS Core | 5.9.0",
    "STM32F4 CMSIS Device | 2.6.11",
    "STM32F4 HAL Driver | 1.8.5",
    "FreeRTOS Kernel | 10.3.1",
    "ST FreeRTOS integration notes",
    "CubeMX-generated application material",
)
ALLOWED_CATEGORIES = {"LIC", "PRIV", "SW", "REPRO", "HW", "DOC"}
ALLOWED_STATUSES = {"OPEN", "CLOSED", "ACCEPTED_LIMITATION", "NOT_APPLICABLE"}
ALLOWED_BLOCKS = {"SOURCE", "BINARY", "HARDWARE"}
SOURCE_BLOCKING_CATEGORIES = {"LIC", "PRIV", "SW"}
LEDGER_HEADER = "| ID | Category | Status | Blocks | Summary | Evidence or next action |"
TEXT_SUFFIXES = {".c", ".h", ".json", ".md", ".py", ".sh", ".txt", ".yaml", ".yml"}


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


def validate_project_license(text: str) -> list[str]:
    errors: list[str] = []
    required = (
        "Project License Scope",
        "MIT License",
        "Copyright (c) 2026 fengchechecheche",
        "THIRD_PARTY_NOTICES.md",
        "does not relicense Core/, Drivers/, Middlewares/Third_Party/",
    )
    for marker in required:
        if marker not in text:
            errors.append(f"LICENSE missing marker: {marker}")
    if "TBD_USER_REVIEW" in text or "<copyright" in text.lower():
        errors.append("LICENSE still contains a decision placeholder")
    return errors


def validate_notice(text: str) -> list[str]:
    return [
        f"THIRD_PARTY_NOTICES.md missing component marker: {marker}"
        for marker in NOTICE_MARKERS
        if marker not in text
    ]


def parse_ledger(text: str) -> tuple[list[dict[str, object]], list[str]]:
    errors: list[str] = []
    rows: list[dict[str, object]] = []
    seen: set[str] = set()
    if LEDGER_HEADER not in text:
        return rows, ["release ledger header is missing or changed"]
    for raw in text.splitlines():
        if not re.match(r"^\| [A-Z]+-[0-9]{3} \|", raw):
            continue
        fields = [field.strip() for field in raw.strip().strip("|").split("|")]
        if len(fields) != 6:
            errors.append(f"ledger row must have six fields: {raw}")
            continue
        blocker_id, category, status, blocks_text, summary, evidence = fields
        if blocker_id in seen:
            errors.append(f"duplicate blocker ID: {blocker_id}")
        seen.add(blocker_id)
        if category not in ALLOWED_CATEGORIES:
            errors.append(f"{blocker_id}: unknown category {category}")
        if status not in ALLOWED_STATUSES:
            errors.append(f"{blocker_id}: unknown status {status}")
        blocks = {item.strip() for item in blocks_text.split(",") if item.strip()}
        unknown_blocks = blocks - ALLOWED_BLOCKS
        if not blocks or unknown_blocks:
            errors.append(f"{blocker_id}: invalid blocks {blocks_text}")
        if not summary or not evidence:
            errors.append(f"{blocker_id}: summary and evidence are required")
        rows.append(
            {
                "id": blocker_id,
                "category": category,
                "status": status,
                "blocks": blocks,
            }
        )
    if not rows:
        errors.append("release ledger has no blocker rows")
    return rows, errors


def source_gate_errors(rows: Iterable[dict[str, object]]) -> list[str]:
    errors: list[str] = []
    for row in rows:
        if (
            row["status"] == "OPEN"
            and row["category"] in SOURCE_BLOCKING_CATEGORIES
        ):
            errors.append(f"open software-source blocker: {row['id']}")
    return errors


def privacy_findings(items: Iterable[tuple[str, str]]) -> list[str]:
    patterns = (
        ("Windows user profile", re.compile(r"[A-Za-z]:\\Users\\[^\\\s]+")),
        ("Linux user home", re.compile(r"/home/[A-Za-z0-9._-]+(?:/|\\b)")),
        ("Windows local drive", re.compile(r"\b[FfEe]:\\")),
        ("WSL UNC path", re.compile(r"\\\\wsl(?:\.localhost|\$)\\", re.IGNORECASE)),
    )
    findings: list[str] = []
    for label, text in items:
        for kind, pattern in patterns:
            if pattern.search(text):
                findings.append(f"{label}: contains {kind}")
    return findings


def public_text_files(root: pathlib.Path) -> Iterable[pathlib.Path]:
    directory_roots = (
        "app",
        "artifacts",
        "bsp",
        "config",
        "docs",
        "protocol",
        "sensors",
        "test",
        "tools",
    )
    top_files = (
        ".gitignore",
        "AGENTS.md",
        "CMakeLists.txt",
        "CMakePresets.json",
        "README.md",
    )
    for relative in top_files:
        path = root / relative
        if path.is_file():
            yield path
    for relative in directory_roots:
        base = root / relative
        if not base.is_dir():
            continue
        for path in sorted(base.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in TEXT_SUFFIXES:
                continue
            if path == pathlib.Path(__file__).resolve():
                continue
            yield path


def sha256_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def check_repository(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    for relative in REQUIRED_FILES:
        path = root / relative
        if not path.is_file() or path.stat().st_size == 0:
            errors.append(f"required release file missing or empty: {relative}")
    if errors:
        return errors

    errors.extend(validate_project_license(read_text(root / "LICENSE")))
    errors.extend(validate_notice(read_text(root / "THIRD_PARTY_NOTICES.md")))

    package_path = root / "LICENSES/STM32CubeF4-1.28.3-Package_license.md"
    package_hash = sha256_file(package_path)
    if package_hash != PACKAGE_LICENSE_SHA256:
        errors.append(
            "STM32CubeF4 package license hash mismatch: "
            f"{package_hash} != {PACKAGE_LICENSE_SHA256}"
        )
    package_text = read_text(package_path)
    for marker in ("STM32CubeF4", "CMSIS Device", "Apache-2.0", "STM32F4 HAL", "BSD-3-Clause", "FreeRTOS kernel", "MIT"):
        if marker not in package_text:
            errors.append(f"package license missing marker: {marker}")

    ledger_text = read_text(root / "docs/release_readiness.md")
    rows, ledger_errors = parse_ledger(ledger_text)
    errors.extend(ledger_errors)
    errors.extend(source_gate_errors(rows))
    if "Software source candidate gate: `PASS`" not in ledger_text:
        errors.append("ledger software source candidate gate is not PASS")
    if "Hardware Release gate: `BLOCKED_WAITING_FOR_HARDWARE`" not in ledger_text:
        errors.append("ledger hardware Release gate is not blocked for hardware")

    public_items = (
        (str(path.relative_to(root)), read_text(path))
        for path in public_text_files(root)
    )
    errors.extend(privacy_findings(public_items))
    return errors


def run_self_test() -> int:
    checks = 0

    valid_license = (
        "Project License Scope\nMIT License\nCopyright (c) 2026 fengchechecheche\n"
        "THIRD_PARTY_NOTICES.md\ndoes not relicense Core/, Drivers/, "
        "Middlewares/Third_Party/\n"
    )
    assert not validate_project_license(valid_license)
    checks += 1
    assert validate_project_license(valid_license.replace("fengchechecheche", "missing"))
    checks += 1
    assert validate_project_license(valid_license + "<copyright holder>\n")
    checks += 1

    valid_notice = "\n".join(NOTICE_MARKERS)
    assert not validate_notice(valid_notice)
    assert validate_notice(valid_notice.replace("FreeRTOS Kernel | 10.3.1", ""))
    checks += 1

    assert privacy_findings([("doc.md", "C:" + "\\Users\\person\\file")])
    assert privacy_findings([("doc.md", "/home/person/project")])
    checks += 2

    header = LEDGER_HEADER + "\n|---|---|---|---|---|---|\n"
    hardware_row = "| HW-001 | HW | OPEN | HARDWARE | waiting | run later |\n"
    rows, errors = parse_ledger(header + hardware_row)
    assert not errors and not source_gate_errors(rows)
    checks += 1

    license_row = "| LIC-001 | LIC | OPEN | SOURCE | missing | decide |\n"
    rows, errors = parse_ledger(header + license_row)
    assert not errors and source_gate_errors(rows)
    checks += 1

    duplicate = hardware_row + hardware_row
    _, errors = parse_ledger(header + duplicate)
    assert any("duplicate blocker ID" in item for item in errors)
    checks += 1

    bad_status = hardware_row.replace("OPEN", "UNKNOWN")
    _, errors = parse_ledger(header + bad_status)
    assert any("unknown status" in item for item in errors)
    checks += 1

    print(f"P5 RELEASE READINESS SELF-TEST: PASS ({checks} bounded checks)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()
    errors = check_repository(args.root.resolve())
    if errors:
        for error in errors:
            print(f"P5 RELEASE READINESS: FAIL: {error}", file=sys.stderr)
        return 1
    print("P5 RELEASE READINESS: PASS_SOFTWARE_CANDIDATE (5 closed, 5 open for later gates)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
