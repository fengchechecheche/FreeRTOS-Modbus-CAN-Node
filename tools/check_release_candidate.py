#!/usr/bin/env python3
"""Validate the bounded P5 software candidate and recruitment claims."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import sys
from collections import Counter
from urllib.parse import urlsplit


MATRIX_PATH = pathlib.Path("artifacts/release/p5_s7_t03_evidence_matrix.json")
CANDIDATE_PATH = pathlib.Path("docs/v0_1_0_software_candidate.md")
DEMO_PATH = pathlib.Path("docs/demo_guide.md")
CLAIM_PATH = pathlib.Path("docs/recruitment_claim_ledger.md")
TUTORIAL_PATH = pathlib.Path("docs/learning/p5_s7_t05_v0_1_0发布与求职材料.md")
RELEASE_LEDGER_PATH = pathlib.Path("docs/release_readiness.md")

EXPECTED_ROLES = {"EMBEDDED", "IOT", "ROBOTICS_LOW_LEVEL"}
ROLE_PREFIX = {
    "EMBEDDED": "CLM-EMB-",
    "IOT": "CLM-IOT-",
    "ROBOTICS_LOW_LEVEL": "CLM-ROB-",
}
ALLOWED_QUALIFICATIONS = {
    "HOST+CROSS_BUILD",
    "SOFTWARE_CANDIDATE",
    "HOST+VIRTUAL_BUS",
}
PRIVATE_PATTERNS = (
    ("Windows user path", re.compile(r"[A-Za-z]:\\Users\\", re.IGNORECASE)),
    ("WSL user mount", re.compile(r"/mnt/[a-z]/Users/", re.IGNORECASE)),
    ("Linux user home", re.compile(r"/home/[A-Za-z0-9._-]+/")),
    ("WSL UNC path", re.compile(r"\\\\wsl(?:\.localhost)?\\", re.IGNORECASE)),
)
FORBIDDEN_CLAIM_MARKERS = (
    "工业级",
    "量产级",
    "功能安全",
    "实板验证通过",
    "项目三联调完成",
    "物理 CAN 通过",
    "8 小时长稳通过",
)
UNQUALIFIED_METRIC = re.compile(
    r"(?i)(?<![A-Za-z0-9])\d+(?:\.\d+)?\s*(?:ms|hz|kbit/s|分钟|小时)"
)


def read_text(path: pathlib.Path) -> str:
    return path.read_text(encoding="utf-8", errors="strict")


def parse_rows(text: str, prefix: str, columns: int) -> tuple[list[list[str]], list[str]]:
    rows: list[list[str]] = []
    errors: list[str] = []
    for line in text.splitlines():
        if not line.startswith(f"| {prefix}"):
            continue
        fields = [field.strip() for field in line.strip().strip("|").split("|")]
        if len(fields) != columns:
            errors.append(f"{prefix} row has {len(fields)} columns, expected {columns}: {line}")
            continue
        rows.append(fields)
    return rows, errors


def matrix_rows(matrix: object) -> tuple[dict[str, dict[str, object]], list[str]]:
    errors: list[str] = []
    if not isinstance(matrix, dict):
        return {}, ["evidence matrix root must be an object"]
    if matrix.get("schema") != "P5_EVIDENCE_MATRIX_V1":
        errors.append("evidence matrix schema mismatch")
    if matrix.get("summary") != {
        "FAIL": 1,
        "NOT_CLAIMED": 1,
        "NOT_RUN": 4,
        "PASS": 18,
        "REVIEW_REQUIRED": 0,
    }:
        errors.append("evidence matrix summary mismatch")
    raw_rows = matrix.get("rows")
    if not isinstance(raw_rows, list) or len(raw_rows) != 24:
        return {}, errors + ["evidence matrix must contain 24 rows"]
    result: dict[str, dict[str, object]] = {}
    for row in raw_rows:
        if not isinstance(row, dict) or not isinstance(row.get("id"), str):
            errors.append("evidence matrix contains an invalid row")
            continue
        row_id = row["id"]
        if row_id in result:
            errors.append(f"duplicate evidence row: {row_id}")
        result[row_id] = row
    return result, errors


def evidence_ids(field: str) -> list[str]:
    return [item.strip().strip("`") for item in field.split(",") if item.strip()]


def validate_claim_rows(
    rows: list[list[str]], evidence: dict[str, dict[str, object]]
) -> list[str]:
    errors: list[str] = []
    ids = [row[0] for row in rows]
    if len(rows) != 6:
        errors.append(f"candidate claim count must be 6, got {len(rows)}")
    duplicates = sorted({item for item in ids if ids.count(item) > 1})
    if duplicates:
        errors.append(f"duplicate claim IDs: {duplicates}")

    role_counts: Counter[str] = Counter()
    for row in rows:
        claim_id, role, wording, refs, qualification, publication = row
        if role not in EXPECTED_ROLES:
            errors.append(f"{claim_id}: unknown role {role}")
            continue
        role_counts[role] += 1
        if not re.fullmatch(re.escape(ROLE_PREFIX[role]) + r"[0-9]{2}", claim_id):
            errors.append(f"{claim_id}: ID does not match role {role}")
        if qualification not in ALLOWED_QUALIFICATIONS:
            errors.append(f"{claim_id}: invalid qualification {qualification}")
        if publication != "NOT_PUBLISHED":
            errors.append(f"{claim_id}: publication must remain NOT_PUBLISHED")
        refs_list = evidence_ids(refs)
        if not refs_list:
            errors.append(f"{claim_id}: evidence rows are required")
        for row_id in refs_list:
            matrix_row = evidence.get(row_id)
            if matrix_row is None:
                errors.append(f"{claim_id}: unknown evidence row {row_id}")
            elif matrix_row.get("result") != "PASS":
                errors.append(f"{claim_id}: evidence row {row_id} is not PASS")
        for marker in FORBIDDEN_CLAIM_MARKERS:
            if marker in wording:
                errors.append(f"{claim_id}: forbidden wording {marker!r}")
        if UNQUALIFIED_METRIC.search(wording):
            errors.append(f"{claim_id}: unqualified time/rate metric in candidate wording")

    if role_counts != Counter({role: 2 for role in EXPECTED_ROLES}):
        errors.append(f"role claim counts mismatch: {dict(role_counts)}")
    return errors


def validate_restrictions(
    rows: list[list[str]], evidence: dict[str, dict[str, object]]
) -> list[str]:
    errors: list[str] = []
    expected = {
        row_id
        for row_id, row in evidence.items()
        if row.get("result") in {"FAIL", "NOT_RUN", "NOT_CLAIMED"}
    }
    covered: set[str] = set()
    for restriction_id, refs, _boundary, eligibility in rows:
        if not re.fullmatch(r"LIM-[A-Z0-9]+-[0-9]{2}", restriction_id):
            errors.append(f"invalid restriction ID: {restriction_id}")
        if eligibility != "NOT_ELIGIBLE":
            errors.append(f"{restriction_id}: eligibility must be NOT_ELIGIBLE")
        row_has_restricted_evidence = False
        for row_id in evidence_ids(refs):
            matrix_row = evidence.get(row_id)
            if matrix_row is None:
                errors.append(f"{restriction_id}: unknown evidence row {row_id}")
            elif matrix_row.get("result") in {"FAIL", "NOT_RUN", "NOT_CLAIMED"}:
                covered.add(row_id)
                row_has_restricted_evidence = True
            elif matrix_row.get("result") != "PASS":
                errors.append(f"{restriction_id}: {row_id} has an invalid result")
        if not row_has_restricted_evidence:
            errors.append(f"{restriction_id}: restriction has no incomplete evidence")
    if covered != expected:
        errors.append(
            f"restricted evidence coverage mismatch: missing={sorted(expected - covered)}, "
            f"extra={sorted(covered - expected)}"
        )
    return errors


def marker_errors(label: str, text: str, markers: tuple[str, ...]) -> list[str]:
    return [f"{label} missing marker: {marker}" for marker in markers if marker not in text]


def privacy_findings(items: list[tuple[str, str]]) -> list[str]:
    findings: list[str] = []
    for label, text in items:
        for kind, pattern in PRIVATE_PATTERNS:
            if pattern.search(text):
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


def check_repository(root: pathlib.Path) -> tuple[list[str], int, int]:
    errors: list[str] = []
    required = (
        MATRIX_PATH,
        CANDIDATE_PATH,
        DEMO_PATH,
        CLAIM_PATH,
        TUTORIAL_PATH,
        RELEASE_LEDGER_PATH,
    )
    for relative in required:
        path = root / relative
        if not path.is_file() or path.stat().st_size == 0:
            errors.append(f"required candidate file missing or empty: {relative.as_posix()}")
    if errors:
        return errors, 0, 0

    try:
        matrix = json.loads(read_text(root / MATRIX_PATH))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        return [f"invalid evidence matrix JSON: {exc}"], 0, 0
    evidence, matrix_errors = matrix_rows(matrix)
    errors.extend(matrix_errors)

    candidate_text = read_text(root / CANDIDATE_PATH)
    demo_text = read_text(root / DEMO_PATH)
    claim_text = read_text(root / CLAIM_PATH)
    tutorial_text = read_text(root / TUTORIAL_PATH)
    release_text = read_text(root / RELEASE_LEDGER_PATH)

    errors.extend(marker_errors("software candidate", candidate_text, (
        "Intended version: `v0.1.0`",
        "Release state: `UNRELEASED`",
        "Candidate state: `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE`",
        "Original T05 documentation baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`",
        "Current software-test baseline: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`",
        "Clean replay source: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`",
        "Evidence matrix: `24 = 18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`",
        "Hardware Release: `BLOCKED_WAITING_FOR_HARDWARE`",
        "Tag / remote Release: `ABSENT / NOT_RUN`",
    )))
    errors.extend(marker_errors("demo guide", demo_text, (
        "Guide state: `SOFTWARE_DEMO_READY`",
        "Hardware demo: `NOT_RUN / BLOCKED_WAITING_FOR_HARDWARE`",
        "Current executable software demo",
        "Future hardware demo（NOT_RUN）",
        "does not represent a transceiver, physical ACK or MCU frame exchange",
    )))
    errors.extend(marker_errors("claim ledger", claim_text, (
        "Ledger schema: `P5_RECRUITMENT_CLAIMS_V1`",
        "Publication: `NOT_PUBLISHED`",
        "Hardware claims: `INELIGIBLE_WHILE_NOT_RUN`",
    )))
    errors.extend(marker_errors("T05 tutorial", tutorial_text, (
        "教程状态：`FROZEN`",
        "UNRELEASED + SOFTWARE_CANDIDATE_READY_FOR_HARDWARE",
        "BLOCKED_WAITING_FOR_HARDWARE",
    )))
    errors.extend(marker_errors("release ledger", release_text, (
        "Learning documentation gate: `PASS_35_FROZEN`",
        "Software candidate collateral gate: `PASS_READY_FOR_HARDWARE`",
        "Binary reproduction gate: `PASS_CURRENT_CLEAN_REPRODUCTION`",
        "Hardware Release gate: `BLOCKED_WAITING_FOR_HARDWARE`",
        "Tag / remote Release: `NOT_AUTHORIZED / NOT_RUN`",
    )))

    claims, parse_errors = parse_rows(claim_text, "CLM-", 6)
    errors.extend(parse_errors)
    errors.extend(validate_claim_rows(claims, evidence))
    restrictions, parse_errors = parse_rows(claim_text, "LIM-", 4)
    errors.extend(parse_errors)
    errors.extend(validate_restrictions(restrictions, evidence))

    public_docs = [CANDIDATE_PATH, DEMO_PATH, CLAIM_PATH, TUTORIAL_PATH]
    errors.extend(privacy_findings([
        (path.as_posix(), read_text(root / path)) for path in public_docs
    ]))
    for relative in public_docs:
        path = root / relative
        errors.extend(markdown_link_errors(root, path, read_text(path)))

    eligible = sum(row.get("result") == "PASS" for row in evidence.values())
    restricted = sum(
        row.get("result") in {"FAIL", "NOT_RUN", "NOT_CLAIMED"}
        for row in evidence.values()
    )
    return errors, eligible, restricted


def run_self_test() -> int:
    checks = 0
    pass_row = {"result": "PASS"}
    restricted_row = {"result": "NOT_RUN"}
    not_claimed_row = {"result": "NOT_CLAIMED"}
    failed_row = {"result": "FAIL"}
    evidence = {
        "SW-01": pass_row,
        "FW-01": pass_row,
        "BSP-02": restricted_row,
        "REP-02": not_claimed_row,
        "RS485-03": failed_row,
    }
    valid = [
        ["CLM-EMB-01", "EMBEDDED", "Host evidence", "SW-01", "HOST+CROSS_BUILD", "NOT_PUBLISHED"],
        ["CLM-EMB-02", "EMBEDDED", "Build evidence", "FW-01", "SOFTWARE_CANDIDATE", "NOT_PUBLISHED"],
        ["CLM-IOT-01", "IOT", "Protocol evidence", "SW-01", "HOST+CROSS_BUILD", "NOT_PUBLISHED"],
        ["CLM-IOT-02", "IOT", "Virtual evidence", "FW-01", "HOST+VIRTUAL_BUS", "NOT_PUBLISHED"],
        ["CLM-ROB-01", "ROBOTICS_LOW_LEVEL", "Runtime evidence", "SW-01", "HOST+CROSS_BUILD", "NOT_PUBLISHED"],
        ["CLM-ROB-02", "ROBOTICS_LOW_LEVEL", "Bounded evidence", "FW-01", "SOFTWARE_CANDIDATE", "NOT_PUBLISHED"],
    ]
    assert not validate_claim_rows(valid, evidence)
    checks += 1
    missing = [row.copy() for row in valid]
    missing[0][3] = "UNKNOWN-01"
    assert any("unknown evidence row" in item for item in validate_claim_rows(missing, evidence))
    checks += 1
    non_pass = [row.copy() for row in valid]
    non_pass[0][3] = "BSP-02"
    assert any("is not PASS" in item for item in validate_claim_rows(non_pass, evidence))
    checks += 1
    published = [row.copy() for row in valid]
    published[0][5] = "PUBLISHED"
    assert any("NOT_PUBLISHED" in item for item in validate_claim_rows(published, evidence))
    checks += 1
    forbidden = [row.copy() for row in valid]
    forbidden[0][2] = "工业级 firmware"
    assert any("forbidden wording" in item for item in validate_claim_rows(forbidden, evidence))
    checks += 1
    metric = [row.copy() for row in valid]
    metric[0][2] = "20 ms measured timing"
    assert any("unqualified" in item for item in validate_claim_rows(metric, evidence))
    checks += 1
    short = valid[:-1]
    assert validate_claim_rows(short, evidence)
    checks += 1
    duplicate = [row.copy() for row in valid]
    duplicate[1][0] = duplicate[0][0]
    assert any("duplicate claim" in item for item in validate_claim_rows(duplicate, evidence))
    checks += 1
    restrictions = [
        ["LIM-HW-01", "BSP-02", "not run", "NOT_ELIGIBLE"],
        ["LIM-REPRO-01", "REP-02", "not claimed", "NOT_ELIGIBLE"],
        ["LIM-RS485-01", "RS485-03", "failed", "NOT_ELIGIBLE"],
    ]
    assert not validate_restrictions(restrictions, evidence)
    checks += 1
    assert validate_restrictions(restrictions[:-1], evidence)
    checks += 1
    contextual = {
        "BSP-02": pass_row,
        "SNS-01": restricted_row,
    }
    contextual_restrictions = [
        [
            "LIM-HW-01",
            "BSP-02,SNS-01",
            "Board passed; sensor remains incomplete.",
            "NOT_ELIGIBLE",
        ]
    ]
    assert not validate_restrictions(contextual_restrictions, contextual)
    checks += 1
    contextual_restrictions[0][1] = "BSP-02"
    assert any(
        "no incomplete evidence" in item
        for item in validate_restrictions(contextual_restrictions, contextual)
    )
    checks += 1
    print(f"P5 RELEASE CANDIDATE SELF-TEST: PASS ({checks} bounded checks)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()
    errors, eligible, restricted = check_repository(args.root.resolve())
    if errors:
        for error in errors:
            print(f"P5 RELEASE CANDIDATE: FAIL: {error}", file=sys.stderr)
        return 1
    print(
        "P5 RELEASE CANDIDATE: PASS "
        f"(6 candidate claims, {eligible} eligible evidence rows, "
        f"{restricted} restricted rows)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
