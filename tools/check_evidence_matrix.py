#!/usr/bin/env python3
"""Validate the bounded public P5 evidence matrix."""

from __future__ import annotations

import argparse
import copy
import json
import pathlib
import re
import sys
import tempfile
from collections import Counter
from typing import Any


SCHEMA = "P5_EVIDENCE_MATRIX_V1"
MATRIX_PATH = pathlib.Path("artifacts/release/p5_s7_t03_evidence_matrix.json")
REPRO_PATH = pathlib.Path("artifacts/release/p5_repro_002_replay.json")
MAX_ROWS = 24
RESULTS = {"PASS", "FAIL", "NOT_RUN", "NOT_CLAIMED", "REVIEW_REQUIRED"}
LAYERS = {
    "WINDOWS_CONFIG",
    "HOST",
    "CLEAN_BUILD",
    "CROSS_BUILD",
    "VIRTUAL_BUS",
    "HARDWARE",
    "INTEGRATION",
    "SOAK",
}
DOMAINS = {
    "RELEASE",
    "SOFTWARE",
    "FIRMWARE",
    "BSP",
    "SENSOR",
    "RS485",
    "CAN",
    "DUAL_BUS",
    "SOAK",
}
FULL_SHA = re.compile(r"^[0-9a-f]{40}$")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
ROW_ID = re.compile(r"^[A-Z0-9]+-[0-9]{2}$")
PRIVATE_OR_ABSOLUTE = re.compile(
    r"(?:^/|^[A-Za-z]:[\\/]|^\\\\|(?:^|/)\.private(?:/|$)|\.\.(?:/|$))"
)
PUBLIC_PRIVACY = (
    re.compile(r"[A-Za-z]:\\Users\\[^\\\s]+"),
    re.compile(r"/home/[A-Za-z0-9._-]+(?:/|\b)"),
    re.compile(r"\\\\wsl(?:\.localhost|\$)\\", re.IGNORECASE),
    re.compile(r"\b(?:password|access[_-]?token|private[_-]?key)\s*[:=]", re.IGNORECASE),
)
CLAIM_PREFIX = {
    "FAIL": "已执行但失败",
    "NOT_RUN": "未执行",
    "NOT_CLAIMED": "不声明",
    "REVIEW_REQUIRED": "需要复核",
}


def read_json(path: pathlib.Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8", errors="strict"))


def is_safe_relative(value: str) -> bool:
    return bool(value) and not PRIVATE_OR_ABSOLUTE.search(value.replace("\\", "/"))


def validate_references(
    root: pathlib.Path,
    row_id: str,
    field: str,
    values: Any,
) -> list[str]:
    errors: list[str] = []
    if not isinstance(values, list) or not 1 <= len(values) <= 3:
        return [f"{row_id}: {field} must contain 1..3 paths"]
    if len(values) != len(set(values)):
        errors.append(f"{row_id}: duplicate path in {field}")
    for value in values:
        if not isinstance(value, str) or not is_safe_relative(value):
            errors.append(f"{row_id}: unsafe {field} path: {value!r}")
            continue
        if not (root / value).is_file():
            errors.append(f"{row_id}: missing {field} path: {value}")
    return errors


def validate_matrix(matrix: dict[str, Any], root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    if matrix.get("schema") != SCHEMA:
        errors.append(f"schema must be {SCHEMA}")
    for field in ("matrix_baseline", "clean_replay_source_commit"):
        value = matrix.get(field)
        if not isinstance(value, str) or not FULL_SHA.fullmatch(value):
            errors.append(f"{field} must be a full Git SHA")
    candidate_hash = matrix.get("candidate_release_bin_sha256")
    if not isinstance(candidate_hash, str) or not SHA256.fullmatch(candidate_hash):
        errors.append("candidate_release_bin_sha256 must be SHA-256")
    errors.extend(
        validate_references(root, "matrix", "generated_from", matrix.get("generated_from"))
    )

    rows = matrix.get("rows")
    if not isinstance(rows, list) or not 1 <= len(rows) <= MAX_ROWS:
        return errors + [f"rows must contain 1..{MAX_ROWS} entries"]

    seen: set[str] = set()
    counts: Counter[str] = Counter()
    required = {
        "id",
        "domain",
        "capability",
        "execution_layer",
        "result",
        "evidence_commit",
        "regression_anchor",
        "firmware_sha256",
        "hardware_identity",
        "config_refs",
        "evidence_refs",
        "allowed_claim",
    }
    for index, row in enumerate(rows):
        if not isinstance(row, dict):
            errors.append(f"row {index}: must be an object")
            continue
        missing = required - set(row)
        if missing:
            errors.append(f"row {index}: missing fields: {', '.join(sorted(missing))}")
            continue
        row_id = row["id"]
        if not isinstance(row_id, str) or not ROW_ID.fullmatch(row_id):
            errors.append(f"row {index}: invalid id {row_id!r}")
            row_id = f"row-{index}"
        elif row_id in seen:
            errors.append(f"duplicate row id: {row_id}")
        seen.add(row_id)

        domain = row["domain"]
        layer = row["execution_layer"]
        result = row["result"]
        if domain not in DOMAINS:
            errors.append(f"{row_id}: unknown domain {domain!r}")
        if layer not in LAYERS:
            errors.append(f"{row_id}: unknown execution layer {layer!r}")
        if result not in RESULTS:
            errors.append(f"{row_id}: unknown result {result!r}")
        else:
            counts[result] += 1

        if not isinstance(row["capability"], str) or not row["capability"].strip():
            errors.append(f"{row_id}: capability is required")
        if not isinstance(row["evidence_commit"], str) or not FULL_SHA.fullmatch(row["evidence_commit"]):
            errors.append(f"{row_id}: evidence_commit must be a full Git SHA")
        anchor = row["regression_anchor"]
        if anchor is not None and (not isinstance(anchor, str) or not FULL_SHA.fullmatch(anchor)):
            errors.append(f"{row_id}: regression_anchor must be null or full Git SHA")
        firmware = row["firmware_sha256"]
        if firmware is not None and (not isinstance(firmware, str) or not SHA256.fullmatch(firmware)):
            errors.append(f"{row_id}: firmware_sha256 must be null or SHA-256")

        identity = row["hardware_identity"]
        if identity is not None and not isinstance(identity, str):
            errors.append(f"{row_id}: hardware_identity must be null or string")
        if result == "NOT_RUN" and firmware is not None:
            errors.append(f"{row_id}: NOT_RUN cannot carry a flashed firmware hash")
        if layer in {"HARDWARE", "INTEGRATION", "SOAK"}:
            if result == "PASS":
                if not firmware or identity in {None, "NOT_ADMITTED"}:
                    errors.append(f"{row_id}: physical PASS requires firmware and admitted hardware identity")
            elif result == "NOT_RUN" and identity != "NOT_ADMITTED":
                errors.append(f"{row_id}: physical NOT_RUN must use NOT_ADMITTED")

        claim = row["allowed_claim"]
        if not isinstance(claim, str) or not claim.strip():
            errors.append(f"{row_id}: allowed_claim is required")
        elif result in CLAIM_PREFIX and not claim.startswith(CLAIM_PREFIX[result]):
            errors.append(
                f"{row_id}: {result} claim must start with {CLAIM_PREFIX[result]!r}"
            )

        errors.extend(validate_references(root, row_id, "config_refs", row["config_refs"]))
        errors.extend(validate_references(root, row_id, "evidence_refs", row["evidence_refs"]))

    expected_summary = {result: counts.get(result, 0) for result in sorted(RESULTS)}
    if matrix.get("summary") != expected_summary:
        errors.append(f"summary mismatch: {matrix.get('summary')!r} != {expected_summary!r}")

    serialized = json.dumps(matrix, ensure_ascii=False)
    for pattern in PUBLIC_PRIVACY:
        if pattern.search(serialized):
            errors.append(f"public matrix contains forbidden private pattern: {pattern.pattern}")
    return errors


def validate_repro_projection(matrix: dict[str, Any], root: pathlib.Path) -> list[str]:
    """Tie the current clean-build rows to the admitted REPRO-002 bundle."""
    errors: list[str] = []
    try:
        replay = read_json(root / REPRO_PATH)
    except (OSError, json.JSONDecodeError, UnicodeDecodeError) as exc:
        return [f"cannot read REPRO-002 projection source: {exc}"]
    source_commit = replay.get("source", {}).get("commit")
    artifacts = replay.get("artifacts")
    if not isinstance(artifacts, dict):
        return ["REPRO-002 artifacts must be an object"]
    debug_record = artifacts.get("build/debug/freertos_modbus_can_node.bin")
    release_record = artifacts.get("build/release/freertos_modbus_can_node.bin")
    debug_hash = debug_record.get("sha256") if isinstance(debug_record, dict) else None
    release_hash = (
        release_record.get("sha256") if isinstance(release_record, dict) else None
    )
    expected_top = (
        ("matrix_baseline", source_commit),
        ("clean_replay_source_commit", source_commit),
        ("candidate_release_bin_sha256", release_hash),
    )
    for field, wanted in expected_top:
        if matrix.get(field) != wanted:
            errors.append(f"{field} does not match REPRO-002")
    generated_from = matrix.get("generated_from")
    if not isinstance(generated_from, list) or REPRO_PATH.as_posix() not in generated_from:
        errors.append("generated_from does not include REPRO-002 replay")

    rows = matrix.get("rows")
    row_map = {
        row.get("id"): row
        for row in rows
        if isinstance(rows, list) and isinstance(row, dict)
    } if isinstance(rows, list) else {}
    for row_id, firmware_hash in (
        ("REP-01", release_hash),
        ("SW-01", None),
        ("FW-01", debug_hash),
        ("FW-02", release_hash),
        ("REP-02", release_hash),
    ):
        row = row_map.get(row_id)
        if row is None:
            errors.append(f"missing REPRO-002 projection row: {row_id}")
            continue
        if row.get("evidence_commit") != source_commit:
            errors.append(f"{row_id}: evidence commit does not match REPRO-002")
        if row.get("regression_anchor") != source_commit:
            errors.append(f"{row_id}: regression anchor does not match REPRO-002")
        if row.get("firmware_sha256") != firmware_hash:
            errors.append(f"{row_id}: firmware hash does not match REPRO-002")
    return errors


def fixture(root: pathlib.Path) -> dict[str, Any]:
    (root / "config.txt").write_text("config\n", encoding="utf-8")
    (root / "evidence.md").write_text("evidence\n", encoding="utf-8")
    row = {
        "id": "SW-01",
        "domain": "SOFTWARE",
        "capability": "bounded host test",
        "execution_layer": "HOST",
        "result": "PASS",
        "evidence_commit": "1" * 40,
        "regression_anchor": "2" * 40,
        "firmware_sha256": None,
        "hardware_identity": None,
        "config_refs": ["config.txt"],
        "evidence_refs": ["evidence.md"],
        "allowed_claim": "指定 Host 测试通过。",
    }
    return {
        "schema": SCHEMA,
        "matrix_baseline": "3" * 40,
        "clean_replay_source_commit": "2" * 40,
        "candidate_release_bin_sha256": "4" * 64,
        "generated_from": ["evidence.md"],
        "summary": {
            "FAIL": 0,
            "NOT_CLAIMED": 0,
            "NOT_RUN": 0,
            "PASS": 1,
            "REVIEW_REQUIRED": 0,
        },
        "rows": [row],
    }


def run_self_test() -> int:
    checks = 0
    with tempfile.TemporaryDirectory(prefix="p5-evidence-matrix-") as directory:
        root = pathlib.Path(directory)
        valid = fixture(root)
        assert not validate_matrix(valid, root)
        checks += 1

        duplicate = copy.deepcopy(valid)
        duplicate["rows"].append(copy.deepcopy(duplicate["rows"][0]))
        duplicate["summary"]["PASS"] = 2
        assert any("duplicate row id" in item for item in validate_matrix(duplicate, root))
        checks += 1

        bad_status = copy.deepcopy(valid)
        bad_status["rows"][0]["result"] = "UNKNOWN"
        assert any("unknown result" in item for item in validate_matrix(bad_status, root))
        checks += 1

        absolute = copy.deepcopy(valid)
        absolute["rows"][0]["evidence_refs"] = ["/home/person/raw.log"]
        assert any("unsafe evidence_refs" in item for item in validate_matrix(absolute, root))
        checks += 1

        missing = copy.deepcopy(valid)
        missing["rows"][0]["evidence_refs"] = ["missing.md"]
        assert any("missing evidence_refs" in item for item in validate_matrix(missing, root))
        checks += 1

        hardware = copy.deepcopy(valid)
        hardware["rows"][0].update(
            {"execution_layer": "HARDWARE", "result": "PASS", "hardware_identity": "NOT_ADMITTED"}
        )
        assert any("physical PASS requires" in item for item in validate_matrix(hardware, root))
        checks += 1

        not_run = copy.deepcopy(valid)
        not_run["rows"][0].update(
            {
                "execution_layer": "HARDWARE",
                "result": "NOT_RUN",
                "firmware_sha256": "5" * 64,
                "hardware_identity": "NOT_ADMITTED",
                "allowed_claim": "未执行实物检查。",
            }
        )
        not_run["summary"].update({"PASS": 0, "NOT_RUN": 1})
        assert any("NOT_RUN cannot carry" in item for item in validate_matrix(not_run, root))
        checks += 1

        oversized = copy.deepcopy(valid)
        oversized["rows"] = [copy.deepcopy(valid["rows"][0]) for _ in range(MAX_ROWS + 1)]
        assert any("rows must contain" in item for item in validate_matrix(oversized, root))
        checks += 1

        summary = copy.deepcopy(valid)
        summary["summary"]["PASS"] = 0
        assert any("summary mismatch" in item for item in validate_matrix(summary, root))
        checks += 1

        claim = copy.deepcopy(valid)
        claim["rows"][0].update({"result": "NOT_CLAIMED", "allowed_claim": "已经完成。"})
        claim["summary"].update({"PASS": 0, "NOT_CLAIMED": 1})
        assert any("claim must start" in item for item in validate_matrix(claim, root))
        checks += 1

        replay_path = root / REPRO_PATH
        replay_path.parent.mkdir(parents=True, exist_ok=True)
        replay_path.write_text(
            json.dumps(
                {
                    "source": {"commit": "2" * 40},
                    "artifacts": {
                        "build/debug/freertos_modbus_can_node.bin": {
                            "sha256": "5" * 64
                        },
                        "build/release/freertos_modbus_can_node.bin": {
                            "sha256": "4" * 64
                        },
                    },
                }
            )
            + "\n",
            encoding="utf-8",
            newline="\n",
        )
        projection = {
            "matrix_baseline": "2" * 40,
            "clean_replay_source_commit": "2" * 40,
            "candidate_release_bin_sha256": "4" * 64,
            "generated_from": [REPRO_PATH.as_posix()],
            "rows": [
                {
                    "id": row_id,
                    "evidence_commit": "2" * 40,
                    "regression_anchor": "2" * 40,
                    "firmware_sha256": firmware,
                }
                for row_id, firmware in (
                    ("REP-01", "4" * 64),
                    ("SW-01", None),
                    ("FW-01", "5" * 64),
                    ("FW-02", "4" * 64),
                    ("REP-02", "4" * 64),
                )
            ],
        }
        assert not validate_repro_projection(projection, root)
        checks += 1
        projection["candidate_release_bin_sha256"] = "6" * 64
        assert validate_repro_projection(projection, root)
        checks += 1

    print(f"P5 EVIDENCE MATRIX SELF-TEST: PASS ({checks} bounded checks)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    if args.self_test:
        return run_self_test()

    root = args.root.resolve()
    path = root / MATRIX_PATH
    if not path.is_file():
        print(f"P5 EVIDENCE MATRIX: FAIL: missing {MATRIX_PATH}", file=sys.stderr)
        return 1
    try:
        matrix = read_json(path)
    except (json.JSONDecodeError, UnicodeDecodeError) as exc:
        print(f"P5 EVIDENCE MATRIX: FAIL: invalid JSON: {exc}", file=sys.stderr)
        return 1
    errors = validate_matrix(matrix, root)
    errors.extend(validate_repro_projection(matrix, root))
    if errors:
        for error in errors:
            print(f"P5 EVIDENCE MATRIX: FAIL: {error}", file=sys.stderr)
        return 1
    summary = matrix["summary"]
    print(
        "P5 EVIDENCE MATRIX: PASS "
        f"({len(matrix['rows'])} rows: {summary['PASS']} PASS, "
        f"{summary['NOT_RUN']} NOT_RUN, {summary['NOT_CLAIMED']} NOT_CLAIMED)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
