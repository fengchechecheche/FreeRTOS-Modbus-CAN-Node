#!/usr/bin/env python3
"""Check the bounded P5 software-source release policy."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
import tempfile
from typing import Iterable

PACKAGE_LICENSE_SHA256 = (
    "2fe52ca80ec3d84064631a051a914b418aa65b9126ae5131ea323213a8be5aa2"
)
HISTORICAL_REPLAY_SHA256 = (
    "b71181e434387a4f5a3c8b30348413d9d6027f63ed2e4e348e95348938d78634"
)
HISTORICAL_MANIFEST_SHA256 = (
    "72254162346c3925375d5ab322185f5216549e54f21aaa8daecb8aae3faf870e"
)
CURRENT_REPLAY_NAME = "p5_repro_002_replay.json"
CURRENT_MANIFEST_NAME = "p5_repro_002_candidate_manifest.sha256"
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
    "docs/reproduction_report.md",
    "docs/reproduction_report_repro_002.md",
    "docs/evidence_matrix.md",
    "tools/reproduce_release_candidate.py",
    "tools/check_evidence_matrix.py",
    "artifacts/release/p5_s7_t02_replay.json",
    "artifacts/release/p5_s7_t02_candidate_manifest.sha256",
    "artifacts/release/p5_repro_002_replay.json",
    "artifacts/release/p5_repro_002_candidate_manifest.sha256",
    "artifacts/release/p5_s7_t03_evidence_matrix.json",
    "docs/learning/README.md",
    "docs/learning/index.md",
    "docs/learning/problem_ledger.md",
    "docs/learning/p5_s1_t01_项目定位与能力边界.md",
    "docs/learning/p5_s1_t02_硬件组成与安全准入.md",
    "docs/learning/p5_s1_t03_仓库结构与嵌入式构建测试.md",
    "docs/learning/p5_s1_t04_引脚复用时钟与接口合同.md",
    "docs/learning/p5_s7_t04_初学者学习路线与问题复盘.md",
    "docs/learning/p5_s7_t05_v0_1_0发布与求职材料.md",
    "tools/check_learning_docs.py",
    "docs/v0_1_0_software_candidate.md",
    "docs/demo_guide.md",
    "docs/recruitment_claim_ledger.md",
    "tools/check_release_candidate.py",
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


def validate_readme(text: str) -> list[str]:
    errors: list[str] = []
    stale_markers = (
        "TBD_USER_REVIEW",
        "当前没有 `LICENSE`",
    )
    for marker in stale_markers:
        if marker in text:
            errors.append(f"README contains stale license claim: {marker}")
    required = (
        "P5-S7-T05 update",
        "docs/v0_1_0_software_candidate.md",
        "docs/demo_guide.md",
        "docs/recruitment_claim_ledger.md",
        "P5-S7-T04 update",
        "docs/learning/README.md",
        "docs/learning/problem_ledger.md",
        "P5-S7-T03 update",
        "docs/evidence_matrix.md",
        "P5-S7-T02 update",
        "docs/reproduction_report.md",
        "REPRO-002 update",
        "docs/reproduction_report_repro_002.md",
        "bit-for-bit reproducibility",
        "WAITING_FOR_HARDWARE",
    )
    for marker in required:
        if marker not in text:
            errors.append(f"README missing reproduction marker: {marker}")
    return errors


def validate_evidence_projection(root: pathlib.Path, ledger_text: str) -> list[str]:
    """Check the bounded T03 projection without duplicating its deep checker."""
    errors: list[str] = []
    matrix_path = root / "artifacts/release/p5_s7_t03_evidence_matrix.json"
    matrix_doc_path = root / "docs/evidence_matrix.md"
    try:
        matrix = json.loads(read_text(matrix_path))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        return [f"invalid evidence-matrix JSON: {exc}"]
    if not isinstance(matrix, dict):
        return ["evidence matrix root must be an object"]
    matrix_rows = matrix.get("rows")
    matrix_row_count = len(matrix_rows) if isinstance(matrix_rows, list) else None

    expected = (
        ("schema", matrix.get("schema"), "P5_EVIDENCE_MATRIX_V1"),
        (
            "matrix baseline",
            matrix.get("matrix_baseline"),
            "26411d2b627fd67654479f5a97a2066e47deafb5",
        ),
        (
            "clean-replay source",
            matrix.get("clean_replay_source_commit"),
            "26411d2b627fd67654479f5a97a2066e47deafb5",
        ),
        (
            "candidate Release BIN",
            matrix.get("candidate_release_bin_sha256"),
            "8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c",
        ),
        ("row count", matrix_row_count, 24),
        (
            "result summary",
            matrix.get("summary"),
            {
                "FAIL": 1,
                "NOT_CLAIMED": 1,
                "NOT_RUN": 4,
                "PASS": 18,
                "REVIEW_REQUIRED": 0,
            },
        ),
    )
    for label, actual, wanted in expected:
        if actual != wanted:
            errors.append(f"evidence matrix {label} mismatch: {actual!r} != {wanted!r}")
    rows_by_id = {
        row.get("id"): row
        for row in matrix_rows
        if isinstance(matrix_rows, list) and isinstance(row, dict)
    } if isinstance(matrix_rows, list) else {}
    row_expectations = {
        "REP-01": (
            "26411d2b627fd67654479f5a97a2066e47deafb5",
            "8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c",
        ),
        "FW-01": (
            "26411d2b627fd67654479f5a97a2066e47deafb5",
            "f2458f4040b749934874f2ad30881c28c35f81b80865d58a60339e5fe58cf4c1",
        ),
        "FW-02": (
            "26411d2b627fd67654479f5a97a2066e47deafb5",
            "8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c",
        ),
        "BSP-02": (
            "d409a8161669aae8ea4c01246df577d36212650a",
            "4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7",
        ),
        "WDG-01": (
            "96fa46b41c38a0a0bb249876d9a0736165ac1642",
            "d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca",
        ),
    }
    for row_id, (commit, firmware) in row_expectations.items():
        row = rows_by_id.get(row_id)
        if row is None:
            errors.append(f"evidence matrix missing current row: {row_id}")
        elif row.get("evidence_commit") != commit or row.get("firmware_sha256") != firmware:
            errors.append(f"evidence matrix current identity mismatch: {row_id}")

    matrix_doc = read_text(matrix_doc_path)
    for marker in (
        "Matrix status: `FROZEN_SCHEMA / UPDATED_EVIDENCE`",
        "Row summary: `24 = 18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`",
        "BLOCKED_WAITING_FOR_HARDWARE",
    ):
        if marker not in matrix_doc:
            errors.append(f"evidence matrix document missing marker: {marker}")
    if "Evidence matrix gate: `PASS_SCHEMA_REFERENCE_CHECK`" not in ledger_text:
        errors.append("release ledger evidence matrix gate is not PASS_SCHEMA_REFERENCE_CHECK")
    return errors


def validate_learning_projection(root: pathlib.Path, ledger_text: str) -> list[str]:
    """Check T05 learning markers without duplicating the deep learning checker."""
    errors: list[str] = []
    route_text = read_text(root / "docs/learning/README.md")
    index_text = read_text(root / "docs/learning/index.md")
    problem_text = read_text(root / "docs/learning/problem_ledger.md")
    tutorial_text = read_text(
        root / "docs/learning/p5_s7_t04_初学者学习路线与问题复盘.md"
    )
    t05_tutorial_text = read_text(
        root / "docs/learning/p5_s7_t05_v0_1_0发布与求职材料.md"
    )
    route_markers = (
        "路线状态：`FROZEN`",
        "路线总数：35",
        "当前可用教程：35",
        "当前候选：P5-S7-T05",
        "problem_ledger.md",
        "13 PASS + 10 NOT_RUN + 1 NOT_CLAIMED",
    )
    for marker in route_markers:
        if marker not in route_text:
            errors.append(f"learning route missing marker: {marker}")
    if not any(
        line.startswith("| S7 | P5-S7-T04 |") and line.endswith("| FROZEN |")
        for line in index_text.splitlines()
    ):
        errors.append("learning index does not identify T04 as frozen")
    if "教程状态：`FROZEN`" not in tutorial_text:
        errors.append("T04 tutorial is not frozen after content review")
    if not any(
        line.startswith("| S7 | P5-S7-T05 |")
        and line.endswith("| FROZEN |")
        for line in index_text.splitlines()
    ):
        errors.append("learning index does not identify T05 as review candidate")
    if "教程状态：`FROZEN`" not in t05_tutorial_text:
        errors.append("T05 tutorial is not frozen after content review")
    problem_rows = sum(
        line.startswith("| PRB-") for line in problem_text.splitlines()
    )
    if problem_rows != 12:
        errors.append(f"problem ledger row count mismatch: {problem_rows} != 12")
    if "Learning documentation gate: `PASS_35_FROZEN`" not in ledger_text:
        errors.append("release ledger learning documentation gate is not PASS")
    return errors


def validate_candidate_projection(root: pathlib.Path, ledger_text: str) -> list[str]:
    """Check only the top-level T05 collateral markers."""
    errors: list[str] = []
    candidate_text = read_text(root / "docs/v0_1_0_software_candidate.md")
    demo_text = read_text(root / "docs/demo_guide.md")
    claim_text = read_text(root / "docs/recruitment_claim_ledger.md")
    markers = (
        ("software candidate", candidate_text, "Release state: `UNRELEASED`"),
        (
            "software candidate",
            candidate_text,
            "Candidate state: `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE`",
        ),
        (
            "software candidate",
            candidate_text,
            "Hardware Release: `BLOCKED_WAITING_FOR_HARDWARE`",
        ),
        ("demo guide", demo_text, "Guide state: `SOFTWARE_DEMO_READY`"),
        (
            "demo guide",
            demo_text,
            "Hardware demo: `NOT_RUN / BLOCKED_WAITING_FOR_HARDWARE`",
        ),
        ("claim ledger", claim_text, "Publication: `NOT_PUBLISHED`"),
        (
            "claim ledger",
            claim_text,
            "Hardware claims: `INELIGIBLE_WHILE_NOT_RUN`",
        ),
    )
    for label, text, marker in markers:
        if marker not in text:
            errors.append(f"{label} missing marker: {marker}")
    if "Software candidate collateral gate: `PASS_READY_FOR_HARDWARE`" not in ledger_text:
        errors.append("release ledger software candidate collateral gate is not PASS")
    return errors


def parse_manifest(path: pathlib.Path) -> tuple[dict[str, str], list[str]]:
    errors: list[str] = []
    entries: dict[str, str] = {}
    try:
        lines = read_text(path).splitlines()
    except (OSError, UnicodeDecodeError) as exc:
        return entries, [f"cannot read manifest {path.name}: {exc}"]
    for line in lines:
        fields = line.split("  ", 1)
        if len(fields) != 2 or not re.fullmatch(r"[0-9a-f]{64}", fields[0]):
            errors.append(f"invalid manifest line: {line}")
            continue
        digest, label = fields
        if label in entries:
            errors.append(f"duplicate manifest label: {label}")
        entries[label] = digest
    return entries, errors


def validate_historical_replay_bundle(root: pathlib.Path) -> list[str]:
    """Validate the frozen [036] bundle without comparing it to current sources."""
    errors: list[str] = []
    replay_path = root / "artifacts/release/p5_s7_t02_replay.json"
    manifest_path = root / "artifacts/release/p5_s7_t02_candidate_manifest.sha256"
    if sha256_file(replay_path) != HISTORICAL_REPLAY_SHA256:
        errors.append("historical replay JSON hash mismatch")
    if sha256_file(manifest_path) != HISTORICAL_MANIFEST_SHA256:
        errors.append("historical replay manifest hash mismatch")
    try:
        replay = json.loads(read_text(replay_path))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        return errors + [f"invalid historical replay JSON: {exc}"]

    expected_values = (
        ("schema", replay.get("schema"), "P5_RELEASE_REPLAY_V1"),
        ("source commit", replay.get("source", {}).get("commit"), "15932a2ff7adecdfbe5355559926a95b0df25845"),
        ("clean reproduction", replay.get("software", {}).get("clean_reproduction"), "PASS"),
        ("network used", replay.get("environment", {}).get("network_used"), False),
        ("prior build cache used", replay.get("environment", {}).get("prior_build_cache_used"), False),
        ("hardware flash", replay.get("hardware", {}).get("flash"), "WAITING_FOR_HARDWARE"),
    )
    for label, actual, expected in expected_values:
        if actual != expected:
            errors.append(
                f"historical replay {label} mismatch: {actual!r} != {expected!r}"
            )

    entries, manifest_errors = parse_manifest(manifest_path)
    errors.extend(manifest_errors)

    required_labels = (
        "source/git_archive.tar",
        "source/CMakePresets.json",
        "source/freertos_modbus_can_node.ioc",
        "build/debug/freertos_modbus_can_node.bin",
        "build/release/freertos_modbus_can_node.bin",
        "evidence/p5_s7_t02_replay.json",
    )
    for label in required_labels:
        if label not in entries:
            errors.append(f"manifest missing label: {label}")

    replay_digest = sha256_file(replay_path)
    if entries.get("evidence/p5_s7_t02_replay.json") != replay_digest:
        errors.append("historical manifest replay JSON hash mismatch")
    artifacts = replay.get("artifacts")
    if not isinstance(artifacts, dict):
        errors.append("historical replay artifacts must be an object")
    else:
        for label, record in artifacts.items():
            digest = record.get("sha256") if isinstance(record, dict) else None
            if entries.get(label) != digest:
                errors.append(f"historical manifest artifact mismatch: {label}")
    return errors


def validate_current_replay_bundle(root: pathlib.Path, *, required: bool) -> list[str]:
    """Validate the REPRO-002 bundle when present, and require it after closure."""
    errors: list[str] = []
    replay_path = root / f"artifacts/release/{CURRENT_REPLAY_NAME}"
    manifest_path = root / f"artifacts/release/{CURRENT_MANIFEST_NAME}"
    replay_exists = replay_path.is_file()
    manifest_exists = manifest_path.is_file()
    if not replay_exists and not manifest_exists:
        return ["REPRO-002 bundle is required after closure"] if required else []
    if replay_exists != manifest_exists:
        return ["REPRO-002 bundle is incomplete"]

    try:
        replay = json.loads(read_text(replay_path))
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        return [f"invalid REPRO-002 replay JSON: {exc}"]

    expected_values = (
        ("schema", replay.get("schema"), "P5_RELEASE_REPLAY_V2"),
        ("evidence ID", replay.get("evidence_id"), "REPRO-002"),
        ("clean reproduction", replay.get("software", {}).get("clean_reproduction"), "PASS"),
        ("network used", replay.get("environment", {}).get("network_used"), False),
        ("prior build cache used", replay.get("environment", {}).get("prior_build_cache_used"), False),
        ("hardware flash", replay.get("hardware", {}).get("flash"), "NOT_RUN_IN_THIS_SOFTWARE_REPLAY"),
    )
    for label, actual, expected in expected_values:
        if actual != expected:
            errors.append(
                f"REPRO-002 replay {label} mismatch: {actual!r} != {expected!r}"
            )
    source = replay.get("source")
    source_commit = source.get("commit") if isinstance(source, dict) else None
    if not isinstance(source_commit, str) or not re.fullmatch(r"[0-9a-f]{40}", source_commit):
        errors.append("REPRO-002 source commit must be a full lowercase Git SHA")

    commands = replay.get("software", {}).get("commands")
    if not isinstance(commands, list) or not commands:
        errors.append("REPRO-002 command results are missing")
    else:
        for command in commands:
            if not isinstance(command, dict):
                errors.append("REPRO-002 command result must be an object")
                continue
            if command.get("exit_code") != 0 or command.get("timed_out") is not False:
                errors.append(f"REPRO-002 command did not pass: {command.get('name')}")

    entries, manifest_errors = parse_manifest(manifest_path)
    errors.extend(manifest_errors)
    artifact_labels = (
        "source/git_archive.tar",
        "source/CMakePresets.json",
        "source/freertos_modbus_can_node.ioc",
        "build/debug/freertos_modbus_can_node.elf",
        "build/debug/freertos_modbus_can_node.hex",
        "build/debug/freertos_modbus_can_node.bin",
        "build/release/freertos_modbus_can_node.elf",
        "build/release/freertos_modbus_can_node.hex",
        "build/release/freertos_modbus_can_node.bin",
    )
    replay_label = f"evidence/{CURRENT_REPLAY_NAME}"
    required_labels = set(artifact_labels) | {replay_label}
    for label in sorted(required_labels - set(entries)):
        errors.append(f"REPRO-002 manifest missing label: {label}")
    for label in sorted(set(entries) - required_labels):
        errors.append(f"REPRO-002 manifest has unexpected label: {label}")

    if entries.get(replay_label) != sha256_file(replay_path):
        errors.append("REPRO-002 manifest replay JSON hash mismatch")
    artifacts = replay.get("artifacts")
    if not isinstance(artifacts, dict):
        errors.append("REPRO-002 replay artifacts must be an object")
    else:
        for label in artifact_labels:
            record = artifacts.get(label)
            digest = record.get("sha256") if isinstance(record, dict) else None
            if entries.get(label) != digest:
                errors.append(f"REPRO-002 manifest artifact mismatch: {label}")
    archive_digest = source.get("archive_sha256") if isinstance(source, dict) else None
    if entries.get("source/git_archive.tar") != archive_digest:
        errors.append("REPRO-002 source archive hash mismatch")
    for label, relative in (
        ("source/CMakePresets.json", "CMakePresets.json"),
        ("source/freertos_modbus_can_node.ioc", "freertos_modbus_can_node.ioc"),
    ):
        if entries.get(label) != sha256_file(root / relative):
            errors.append(f"REPRO-002 current source hash mismatch: {label}")
    return errors


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
            if path.name in {
                "check_release_readiness.py",
                "check_evidence_matrix.py",
                "check_learning_docs.py",
                "check_release_candidate.py",
            }:
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
    errors.extend(validate_readme(read_text(root / "README.md")))
    errors.extend(validate_historical_replay_bundle(root))

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
    errors.extend(validate_evidence_projection(root, ledger_text))
    errors.extend(validate_learning_projection(root, ledger_text))
    errors.extend(validate_candidate_projection(root, ledger_text))
    rows, ledger_errors = parse_ledger(ledger_text)
    errors.extend(ledger_errors)
    errors.extend(source_gate_errors(rows))
    repro_rows = [row for row in rows if row["id"] == "REPRO-002"]
    if len(repro_rows) != 1:
        errors.append("release ledger must contain exactly one REPRO-002 row")
    else:
        repro_status = repro_rows[0]["status"]
        errors.extend(
            validate_current_replay_bundle(root, required=repro_status == "CLOSED")
        )
        if repro_status == "OPEN":
            marker = (
                "Binary reproduction gate: "
                "`REPLAY_HARNESS_READY_CURRENT_REPLAY_NOT_RUN`"
            )
            if marker not in ledger_text:
                errors.append("open REPRO-002 ledger has incorrect binary gate")
        elif repro_status == "CLOSED":
            marker = "Binary reproduction gate: `PASS_CURRENT_CLEAN_REPRODUCTION`"
            if marker not in ledger_text:
                errors.append("closed REPRO-002 ledger has incorrect binary gate")
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

    valid_readme = (
        "P5-S7-T05 update\n"
        "docs/v0_1_0_software_candidate.md\n"
        "docs/demo_guide.md\n"
        "docs/recruitment_claim_ledger.md\n"
        "P5-S7-T04 update\n"
        "docs/learning/README.md\n"
        "docs/learning/problem_ledger.md\n"
        "P5-S7-T03 update\n"
        "docs/evidence_matrix.md\n"
        "P5-S7-T02 update\n"
        "docs/reproduction_report.md\n"
        "REPRO-002 update\n"
        "docs/reproduction_report_repro_002.md\n"
        "bit-for-bit reproducibility\n"
        "WAITING_FOR_HARDWARE\n"
    )
    assert not validate_readme(valid_readme)
    checks += 1

    valid_learning_route = (
        "路线状态：`FROZEN`\n"
        "路线总数：35\n当前可用教程：35\n当前候选：P5-S7-T05\n"
        "problem_ledger.md\n13 PASS + 10 NOT_RUN + 1 NOT_CLAIMED\n"
    )
    assert all(
        marker in valid_learning_route
        for marker in (
            "路线状态：`FROZEN`",
            "路线总数：35",
            "当前可用教程：35",
            "当前候选：P5-S7-T05",
            "problem_ledger.md",
            "13 PASS + 10 NOT_RUN + 1 NOT_CLAIMED",
        )
    )
    checks += 1
    assert validate_readme(valid_readme + "TBD_USER_REVIEW\n")
    checks += 1

    valid_projection_doc = (
        "Matrix status: `FROZEN_SCHEMA / UPDATED_EVIDENCE`\n"
        "Row summary: `24 = 18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`\n"
        "BLOCKED_WAITING_FOR_HARDWARE\n"
    )
    assert all(
        marker in valid_projection_doc
        for marker in (
            "Matrix status: `FROZEN_SCHEMA / UPDATED_EVIDENCE`",
            "Row summary: `24 = 18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`",
            "BLOCKED_WAITING_FOR_HARDWARE",
        )
    )
    checks += 1

    assert privacy_findings([("doc.md", "C:" + "\\Users\\person\\file")])
    assert privacy_findings([("doc.md", "/home/person/project")])
    checks += 2

    repository_root = pathlib.Path(__file__).resolve().parents[1]
    assert not validate_historical_replay_bundle(repository_root)
    checks += 1

    with tempfile.TemporaryDirectory(prefix="p5-release-self-test-") as directory:
        root = pathlib.Path(directory)
        release_dir = root / "artifacts/release"
        release_dir.mkdir(parents=True)
        historical_replay = repository_root / "artifacts/release/p5_s7_t02_replay.json"
        historical_manifest = (
            repository_root
            / "artifacts/release/p5_s7_t02_candidate_manifest.sha256"
        )
        (release_dir / historical_replay.name).write_bytes(historical_replay.read_bytes())
        manifest_copy = release_dir / historical_manifest.name
        manifest_copy.write_bytes(historical_manifest.read_bytes())
        manifest_text = read_text(manifest_copy)
        replacement = "0" if manifest_text[0] != "0" else "1"
        manifest_copy.write_text(
            replacement + manifest_text[1:], encoding="utf-8", newline="\n"
        )
        assert any(
            "historical replay manifest hash mismatch" in item
            for item in validate_historical_replay_bundle(root)
        )
        checks += 1

    with tempfile.TemporaryDirectory(prefix="p5-repro-002-self-test-") as directory:
        root = pathlib.Path(directory)
        release_dir = root / "artifacts/release"
        release_dir.mkdir(parents=True)
        (root / "CMakePresets.json").write_text(
            "presets\n", encoding="utf-8", newline="\n"
        )
        ioc_path = root / "freertos_modbus_can_node.ioc"
        ioc_path.write_text("ioc\n", encoding="utf-8", newline="\n")
        assert not validate_current_replay_bundle(root, required=False)
        assert validate_current_replay_bundle(root, required=True)
        checks += 2

        artifact_labels = (
            "source/git_archive.tar",
            "source/CMakePresets.json",
            "source/freertos_modbus_can_node.ioc",
            "build/debug/freertos_modbus_can_node.elf",
            "build/debug/freertos_modbus_can_node.hex",
            "build/debug/freertos_modbus_can_node.bin",
            "build/release/freertos_modbus_can_node.elf",
            "build/release/freertos_modbus_can_node.hex",
            "build/release/freertos_modbus_can_node.bin",
        )
        digests = {
            label: hashlib.sha256(label.encode("utf-8")).hexdigest()
            for label in artifact_labels
        }
        digests["source/CMakePresets.json"] = sha256_file(
            root / "CMakePresets.json"
        )
        digests["source/freertos_modbus_can_node.ioc"] = sha256_file(ioc_path)
        replay = {
            "schema": "P5_RELEASE_REPLAY_V2",
            "evidence_id": "REPRO-002",
            "source": {
                "commit": "1" * 40,
                "archive_sha256": digests["source/git_archive.tar"],
            },
            "environment": {
                "network_used": False,
                "prior_build_cache_used": False,
            },
            "software": {
                "clean_reproduction": "PASS",
                "commands": [
                    {"name": "gate", "exit_code": 0, "timed_out": False}
                ],
            },
            "hardware": {"flash": "NOT_RUN_IN_THIS_SOFTWARE_REPLAY"},
            "artifacts": {
                label: {"sha256": digest, "bytes": 1}
                for label, digest in digests.items()
            },
        }
        replay_path = release_dir / CURRENT_REPLAY_NAME
        replay_path.write_text(
            json.dumps(replay, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
            newline="\n",
        )
        manifest_entries = dict(digests)
        manifest_entries[f"evidence/{CURRENT_REPLAY_NAME}"] = sha256_file(
            replay_path
        )
        manifest_path = release_dir / CURRENT_MANIFEST_NAME
        manifest_path.write_text(
            "\n".join(
                f"{manifest_entries[label]}  {label}"
                for label in sorted(manifest_entries)
            )
            + "\n",
            encoding="utf-8",
            newline="\n",
        )
        assert not validate_current_replay_bundle(root, required=True)
        checks += 1

        ioc_path.write_text("mutated\n", encoding="utf-8", newline="\n")
        assert any(
            "current source hash mismatch" in item
            for item in validate_current_replay_bundle(root, required=True)
        )
        checks += 1
        ioc_path.write_text("ioc\n", encoding="utf-8", newline="\n")

        manifest_lines = read_text(manifest_path).splitlines()
        manifest_path.write_text(
            "\n".join(
                line
                for line in manifest_lines
                if not line.endswith("  build/release/freertos_modbus_can_node.bin")
            )
            + "\n",
            encoding="utf-8",
            newline="\n",
        )
        assert any(
            "manifest missing label" in item
            for item in validate_current_replay_bundle(root, required=True)
        )
        checks += 1

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
    ledger_text = read_text(args.root.resolve() / "docs/release_readiness.md")
    rows, _ = parse_ledger(ledger_text)
    closed = sum(row["status"] == "CLOSED" for row in rows)
    opened = sum(row["status"] == "OPEN" for row in rows)
    print(
        "P5 RELEASE READINESS: PASS_SOFTWARE_CANDIDATE "
        f"({closed} closed, {opened} open for later gates)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
