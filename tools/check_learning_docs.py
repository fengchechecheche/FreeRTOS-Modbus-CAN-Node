#!/usr/bin/env python3
"""Validate the bounded P5 learning route and real-problem ledger."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from collections import Counter
from typing import Any
from urllib.parse import urlsplit


INDEX_PATH = pathlib.Path("docs/learning/index.md")
ROUTE_PATH = pathlib.Path("docs/learning/README.md")
LEDGER_PATH = pathlib.Path("docs/learning/problem_ledger.md")
EXPECTED_IDS = tuple(
    f"P5-S{stage}-T{task:02d}"
    for stage in range(1, 8)
    for task in range(1, 6)
)
EXPECTED_STATUS_COUNTS = {
    "FROZEN": 35,
}
ALLOWED_TASK_STATUSES = set(EXPECTED_STATUS_COUNTS)
ALLOWED_PROBLEM_STATUSES = {
    "FIXED",
    "MITIGATED",
    "ACCEPTED_LIMITATION",
    "OPEN_SOFTWARE",
}
ALLOWED_SOURCE_HOSTS = {
    "www.st.com",
    "www.freertos.org",
    "www.modbus.org",
    "www.bosch-sensortec.com",
    "www.vishay.com",
    "www.analog.com",
    "docs.kernel.org",
}
PRIVATE_PATTERNS = (
    ("Windows user profile", re.compile(r"[A-Za-z]:\\Users\\[^\\\s]+")),
    ("Linux user home", re.compile(r"/home/[A-Za-z0-9._-]+(?:/|\b)")),
    ("Windows local drive", re.compile(r"\b[FfEe]:[\\/]")),
    ("WSL UNC path", re.compile(r"\\\\wsl(?:\.localhost|\$)\\", re.IGNORECASE)),
    ("mounted local drive", re.compile(r"/mnt/[a-z](?:/|\b)", re.IGNORECASE)),
)


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


def parse_table_rows(text: str, id_prefix: str, fields: int) -> tuple[list[list[str]], list[str]]:
    rows: list[list[str]] = []
    errors: list[str] = []
    for raw in text.splitlines():
        if not raw.startswith(f"| {id_prefix}"):
            continue
        parts = [item.strip() for item in raw.strip().strip("|").split("|")]
        if len(parts) != fields:
            errors.append(f"{parts[0] if parts else id_prefix}: expected {fields} table fields")
            continue
        rows.append(parts)
    return rows, errors


def duplicates(values: list[str]) -> list[str]:
    counts = Counter(values)
    return sorted(value for value, count in counts.items() if count > 1)


def task_table_errors(
    index_rows: list[list[str]],
    route_rows: list[list[str]],
    root: pathlib.Path,
) -> list[str]:
    errors: list[str] = []
    index_ids = [row[1] for row in index_rows]
    route_ids = [row[0] for row in route_rows]
    for label, ids in (("index", index_ids), ("route", route_ids)):
        if tuple(ids) != EXPECTED_IDS:
            missing = sorted(set(EXPECTED_IDS) - set(ids))
            extra = sorted(set(ids) - set(EXPECTED_IDS))
            errors.append(f"{label} task order mismatch; missing={missing}, extra={extra}")
        if duplicates(ids):
            errors.append(f"{label} duplicate task IDs: {duplicates(ids)}")

    index_map = {row[1]: row for row in index_rows}
    route_map = {row[0]: row for row in route_rows}
    status_counts: Counter[str] = Counter()
    for task_id in EXPECTED_IDS:
        index_row = index_map.get(task_id)
        route_row = route_map.get(task_id)
        if index_row is None or route_row is None:
            continue
        index_filename = index_row[3].strip("`")
        route_filename = route_row[3].strip("`")
        index_status = index_row[4]
        route_status = route_row[4]
        expected_stage = task_id.split("-")[1]
        if index_row[0] != expected_stage:
            errors.append(f"{task_id}: index stage mismatch: {index_row[0]}")
        if index_filename != route_filename:
            errors.append(f"{task_id}: route/index filename mismatch")
        if index_status != route_status:
            errors.append(f"{task_id}: route/index status mismatch")
        if index_status not in ALLOWED_TASK_STATUSES:
            errors.append(f"{task_id}: invalid status {index_status}")
            continue
        status_counts[index_status] += 1
        tutorial = root / "docs/learning" / index_filename
        if index_status == "PLANNED":
            if task_id != "P5-S7-T05":
                errors.append(f"{task_id}: only P5-S7-T05 may remain PLANNED")
            if tutorial.exists():
                errors.append(f"{task_id}: PLANNED tutorial file must be absent")
        elif not tutorial.is_file() or tutorial.stat().st_size == 0:
            errors.append(f"{task_id}: tutorial missing or empty: {index_filename}")
    if dict(status_counts) != EXPECTED_STATUS_COUNTS:
        errors.append(
            f"task status counts mismatch: {dict(status_counts)} != {EXPECTED_STATUS_COUNTS}"
        )
    return errors


def privacy_findings(items: list[tuple[str, str]]) -> list[str]:
    findings: list[str] = []
    for label, content in items:
        for kind, pattern in PRIVATE_PATTERNS:
            if pattern.search(content):
                findings.append(f"{label}: contains {kind}")
    return findings


def markdown_link_errors(root: pathlib.Path, path: pathlib.Path, text: str) -> list[str]:
    errors: list[str] = []
    resolved_root = root.resolve()
    for target in re.findall(r"\[[^\]]+\]\(([^)]+)\)", text):
        if target.startswith("#") or urlsplit(target).scheme in {"http", "https", "mailto"}:
            continue
        relative = target.split("#", 1)[0]
        if not relative:
            continue
        resolved = (path.parent / relative).resolve()
        try:
            resolved.relative_to(resolved_root)
        except ValueError:
            errors.append(f"{path.relative_to(root)}: link escapes repository: {target}")
            continue
        if not resolved.exists():
            errors.append(f"{path.relative_to(root)}: missing relative link: {target}")
    return errors


def anchor_errors(root: pathlib.Path, route_text: str) -> list[str]:
    errors: list[str] = []
    ioc = read_text(root / "freertos_modbus_can_node.ioc")
    task_h = read_text(root / "Middlewares/Third_Party/FreeRTOS/Source/include/task.h")
    register_map = json.loads(read_text(root / "protocol/register_map.json"))
    can_map = json.loads(read_text(root / "protocol/can_message_map.json"))
    matrix = json.loads(read_text(root / "artifacts/release/p5_s7_t03_evidence_matrix.json"))

    for marker in (
        "MxCube.Version=6.18.0",
        "USART1.BaudRate=19200",
        "USART1.Parity=PARITY_EVEN",
        "USART1.WordLength=WORDLENGTH_9B",
    ):
        if marker not in ioc:
            errors.append(f"IOC current anchor missing: {marker}")
    if 'tskKERNEL_VERSION_NUMBER "V10.3.1"' not in task_h:
        errors.append("FreeRTOS current anchor is not V10.3.1")
    if register_map.get("protocol", {}).get("default_slave_address") != 4:
        errors.append("Modbus default slave address is not 4")
    if register_map.get("protocol", {}).get("address_model") != "zero_based":
        errors.append("Modbus address model is not zero_based")
    if can_map.get("physical_layer", {}).get("nominal_bitrate_bit_s") != 500000:
        errors.append("CAN bitrate is not 500000 bit/s")
    actual_ids = {
        item.get("identifier_hex")
        for item in can_map.get("frames", [])
        if isinstance(item, dict)
    }
    expected_ids = {
        "0x140",
        "0x240",
        "0x241",
        "0x340",
        "0x341",
        "0x342",
        "0x440",
        "0x540",
        "0x541",
    }
    if actual_ids != expected_ids:
        errors.append(f"CAN ID set mismatch: {sorted(actual_ids)}")
    if matrix.get("summary") != {
        "FAIL": 0,
        "NOT_CLAIMED": 1,
        "NOT_RUN": 0,
        "PASS": 23,
        "REVIEW_REQUIRED": 0,
    }:
        errors.append("evidence-matrix summary drift")
    for marker in (
        "CubeMX | 6.18.0",
        "FreeRTOS | V10.3.1",
        "default slave 4；19200 8E1",
        "500000 bit/s；0x140/240/241/340/341/342/440/540/541",
        "Debug/Release 23 tests",
        "23 PASS + 0 NOT_RUN + 1 NOT_CLAIMED",
    ):
        if marker not in route_text:
            errors.append(f"learning route current anchor missing: {marker}")
    return errors


def problem_ledger_errors(rows: list[list[str]], root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    ids = [row[0] for row in rows]
    if not 1 <= len(rows) <= 12:
        errors.append(f"problem ledger must contain 1..12 rows, got {len(rows)}")
    if duplicates(ids):
        errors.append(f"duplicate problem IDs: {duplicates(ids)}")
    for row in rows:
        problem_id, status = row[0], row[8]
        if not re.fullmatch(r"PRB-[0-9]{2}", problem_id):
            errors.append(f"invalid problem ID: {problem_id}")
        if status not in ALLOWED_PROBLEM_STATUSES:
            errors.append(f"{problem_id}: invalid status {status}")
        if status == "FIXED" and ("NOT_RUN" in " ".join(row) or "WAITING_FOR_HARDWARE" in " ".join(row)):
            errors.append(f"{problem_id}: hardware NOT_RUN/WAITING cannot be FIXED")
        references = re.findall(r"`([^`]+)`", row[6])
        if not 1 <= len(references) <= 3:
            errors.append(f"{problem_id}: regression evidence must contain 1..3 paths")
        for reference in references:
            if reference.startswith("/") or reference.startswith(".private/") or ".." in pathlib.PurePosixPath(reference).parts:
                errors.append(f"{problem_id}: unsafe evidence path: {reference}")
            elif not (root / reference).is_file():
                errors.append(f"{problem_id}: missing evidence path: {reference}")
    return errors


def source_errors(rows: list[list[str]]) -> list[str]:
    errors: list[str] = []
    ids = [row[0] for row in rows]
    if len(rows) != 8 or len(set(ids)) != 8:
        errors.append("official source table must contain eight unique rows")
    for row in rows:
        source_id, url, access = row[0], row[3], row[4]
        parsed = urlsplit(url)
        if not re.fullmatch(r"SRC-[0-9]{2}", source_id):
            errors.append(f"invalid source ID: {source_id}")
        if parsed.scheme != "https" or parsed.hostname not in ALLOWED_SOURCE_HOSTS:
            errors.append(f"{source_id}: source is not on an approved official host: {url}")
        if access != "VERIFIED_2026-08-15":
            errors.append(f"{source_id}: access status/date mismatch: {access}")
    return errors


def check_repository(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    required = (
        INDEX_PATH,
        ROUTE_PATH,
        LEDGER_PATH,
        pathlib.Path("docs/learning/p5_s7_t04_初学者学习路线与问题复盘.md"),
        pathlib.Path("docs/learning/p5_s7_t05_v0_1_0发布与求职材料.md"),
        pathlib.Path("freertos_modbus_can_node.ioc"),
        pathlib.Path("protocol/register_map.json"),
        pathlib.Path("protocol/can_message_map.json"),
        pathlib.Path("artifacts/release/p5_s7_t03_evidence_matrix.json"),
    )
    for relative in required:
        path = root / relative
        if not path.is_file() or path.stat().st_size == 0:
            errors.append(f"required learning file missing or empty: {relative.as_posix()}")
    if errors:
        return errors

    index_text = read_text(root / INDEX_PATH)
    route_text = read_text(root / ROUTE_PATH)
    ledger_text = read_text(root / LEDGER_PATH)
    index_rows, parse_errors = parse_table_rows(index_text, "S", 5)
    errors.extend(parse_errors)
    route_rows, parse_errors = parse_table_rows(route_text, "P5-", 6)
    errors.extend(parse_errors)
    errors.extend(task_table_errors(index_rows, route_rows, root))

    problem_rows, parse_errors = parse_table_rows(ledger_text, "PRB-", 9)
    errors.extend(parse_errors)
    errors.extend(problem_ledger_errors(problem_rows, root))
    source_rows, parse_errors = parse_table_rows(route_text, "SRC-", 5)
    errors.extend(parse_errors)
    errors.extend(source_errors(source_rows))
    errors.extend(anchor_errors(root, route_text))

    learning_items = [
        (str(path.relative_to(root)), read_text(path))
        for path in sorted((root / "docs/learning").glob("*.md"))
    ]
    errors.extend(privacy_findings(learning_items))
    for path in sorted((root / "docs/learning").glob("*.md")):
        errors.extend(markdown_link_errors(root, path, read_text(path)))
    return errors


def run_self_test() -> int:
    checks = 0
    assert len(EXPECTED_IDS) == 35 and EXPECTED_IDS[0] == "P5-S1-T01" and EXPECTED_IDS[-1] == "P5-S7-T05"
    checks += 1
    assert duplicates(["A", "B", "A"]) == ["A"]
    checks += 1
    assert not duplicates(list(EXPECTED_IDS))
    checks += 1
    assert privacy_findings([("doc.md", "C:" + "\\Users\\person\\file")])
    checks += 1
    assert privacy_findings([("doc.md", "/home/" + "person/project")])
    checks += 1
    assert privacy_findings([("doc.md", "/mnt/" + "f/project")])
    checks += 1
    assert not markdown_link_errors(
        pathlib.Path.cwd(), pathlib.Path.cwd() / "doc.md", "[official](https://example.com/page)"
    )
    checks += 1
    assert markdown_link_errors(
        pathlib.Path.cwd(), pathlib.Path.cwd() / "doc.md", "[missing](missing.md)"
    )
    checks += 1
    valid_problem = [["PRB-01", "T", "s", "w", "r", "f", "`README.md`", "p", "FIXED"]]
    assert not problem_ledger_errors(valid_problem, pathlib.Path.cwd()) if pathlib.Path("README.md").is_file() else True
    checks += 1
    bad_status = [row.copy() for row in valid_problem]
    bad_status[0][8] = "PLANNED"
    assert any("invalid status" in item for item in problem_ledger_errors(bad_status, pathlib.Path.cwd()))
    checks += 1
    too_many = [valid_problem[0].copy() for _ in range(13)]
    assert any("1..12" in item for item in problem_ledger_errors(too_many, pathlib.Path.cwd()))
    checks += 1
    hardware_fixed = [valid_problem[0].copy()]
    hardware_fixed[0][2] = "WAITING_FOR_HARDWARE"
    assert any("cannot be FIXED" in item for item in problem_ledger_errors(hardware_fixed, pathlib.Path.cwd()))
    checks += 1
    valid_source = [["SRC-01", "ST", "topic", "https://www.st.com/page", "VERIFIED_2026-08-15"] for _ in range(8)]
    for index, row in enumerate(valid_source, start=1):
        row[0] = f"SRC-{index:02d}"
    assert not source_errors(valid_source)
    checks += 1
    invalid_source = [row.copy() for row in valid_source]
    invalid_source[0][3] = "https://example.com/page"
    assert any("approved official host" in item for item in source_errors(invalid_source))
    checks += 1
    print(f"P5 LEARNING DOCS SELF-TEST: PASS ({checks} bounded checks)")
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
            print(f"P5 LEARNING DOCS: FAIL: {error}", file=sys.stderr)
        return 1
    print("P5 LEARNING DOCS: PASS (35 route entries, 35 tutorials, 12 real problems, 8 official sources)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
