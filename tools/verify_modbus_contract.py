#!/usr/bin/env python3
"""Validate the P5 Modbus candidate without claiming hardware support."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any, Callable


EXPECTED_MEASUREMENTS = [
    ("0x0101", "bme280_temperature", 18, "int32", "centi_deg_c", "bme280"),
    ("0x0102", "bme280_pressure", 20, "uint32", "pa", "bme280"),
    ("0x0103", "bme280_humidity", 22, "uint32", "milli_percent_rh", "bme280"),
    ("0x0201", "veml7700_illuminance", 24, "uint32", "millilux", "veml7700"),
    ("0x0301", "adxl345_acceleration_x", 26, "int32", "millig", "adxl345_sample"),
    ("0x0302", "adxl345_acceleration_y", 28, "int32", "millig", "adxl345_sample"),
    ("0x0303", "adxl345_acceleration_z", 30, "int32", "millig", "adxl345_sample"),
    ("0x0311", "adxl345_mean_x", 32, "int32", "millig", "adxl345_feature"),
    ("0x0312", "adxl345_mean_y", 34, "int32", "millig", "adxl345_feature"),
    ("0x0313", "adxl345_mean_z", 36, "int32", "millig", "adxl345_feature"),
    ("0x0321", "adxl345_rms_x", 38, "uint32", "millig", "adxl345_feature"),
    ("0x0322", "adxl345_rms_y", 40, "uint32", "millig", "adxl345_feature"),
    ("0x0323", "adxl345_rms_z", 42, "uint32", "millig", "adxl345_feature"),
    ("0x0331", "adxl345_peak_x", 44, "uint32", "millig", "adxl345_feature"),
    ("0x0332", "adxl345_peak_y", 46, "uint32", "millig", "adxl345_feature"),
    ("0x0333", "adxl345_peak_z", 48, "uint32", "millig", "adxl345_feature"),
    ("0x0341", "adxl345_resultant_rms", 50, "uint32", "millig", "adxl345_feature"),
]

EXPECTED_SOURCES = ("bme280", "veml7700", "adxl345_sample", "adxl345_feature")
EXPECTED_METADATA = (
    ("state", "uint16", 1),
    ("flags", "uint16", 1),
    ("quality", "uint32", 2),
    ("sequence", "uint32", 2),
    ("sample_monotonic_ms", "uint32", 2),
    ("age_ms", "uint32", 2),
)
EXPECTED_WIRE_CODES = {
    "measurement_state": {"invalid": 0, "fresh": 1, "stale": 2, "offline": 3},
    "metadata_flags": {"value_present": 0, "value_retained": 1},
    "quality_bits": {
        "uncalibrated": 0,
        "range_warning": 1,
        "saturated": 2,
        "gap": 3,
        "dropped": 4,
        "transport_error": 5,
        "configuration_error": 6,
        "recovery_active": 7,
    },
    "sensor_fault_class": {
        "none": 0,
        "not_present_or_identity": 1,
        "transport_busy": 2,
        "transport_timeout": 3,
        "transport_io": 4,
        "configuration": 5,
        "data_stalled": 6,
        "recovery_active": 7,
        "offline": 8,
    },
    "health_state": {
        "bootstrap": 0,
        "serviceable": 1,
        "degraded": 2,
        "recovery_required": 3,
        "reset_required": 4,
        "reset_loop_latched": 5,
    },
    "serial_profile": {"19200_8E1": 1},
}


def reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_contract(path: Path) -> dict[str, Any]:
    return json.loads(
        path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicate_keys
    )


def add_check(condition: bool, message: str, errors: list[str]) -> int:
    if not condition:
        errors.append(message)
    return 1


def validate_entries(
    entries: Any,
    data_types: dict[str, Any],
    space: str,
    errors: list[str],
) -> tuple[int, set[int]]:
    checked = 0
    occupied: set[int] = set()
    names: set[str] = set()
    if not isinstance(entries, list):
        errors.append(f"{space}: entries must be a list")
        return 1, occupied

    for index, entry in enumerate(entries):
        label = f"{space}[{index}]"
        checked += add_check(isinstance(entry, dict), f"{label}: entry must be object", errors)
        if not isinstance(entry, dict):
            continue
        name = entry.get("name")
        address = entry.get("address")
        count = entry.get("count")
        data_type = entry.get("type")
        checked += add_check(
            isinstance(name, str) and bool(name), f"{label}: invalid name", errors
        )
        if isinstance(name, str):
            checked += add_check(name not in names, f"{space}: duplicate name {name}", errors)
            names.add(name)
        checked += add_check(
            isinstance(address, int) and not isinstance(address, bool),
            f"{label}: address must be integer",
            errors,
        )
        checked += add_check(
            isinstance(count, int) and not isinstance(count, bool) and count >= 1,
            f"{label}: count must be positive integer",
            errors,
        )
        checked += add_check(data_type in data_types, f"{label}: unsupported type {data_type}", errors)
        if data_type in data_types and isinstance(count, int):
            checked += add_check(
                count == data_types[data_type].get("register_count"),
                f"{label}: count {count} disagrees with {data_type}",
                errors,
            )
        if isinstance(address, int) and isinstance(count, int) and count >= 1:
            end = address + count - 1
            checked += add_check(
                0 <= address <= 0xFFFF and end <= 0xFFFF,
                f"{label}: address span {address}..{end} is outside 0..65535",
                errors,
            )
            for register in range(max(address, 0), min(end, 0xFFFF) + 1):
                if register in occupied:
                    errors.append(f"{space}: overlapping register {register} at {name}")
                occupied.add(register)
                checked += 1
    return checked, occupied


def validate_contract(contract: dict[str, Any]) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    checked += add_check(contract.get("schema_version") == "1.0.0", "schema_version must be 1.0.0", errors)

    document = contract.get("document", {})
    checked += add_check(document.get("project") == "freertos_modbus_can_node", "wrong project", errors)
    checked += add_check(document.get("task_id") == "P5-S5-T01", "wrong task_id", errors)
    checked += add_check(document.get("status") == "candidate_validated", "status must be candidate_validated", errors)
    checked += add_check(document.get("runtime_status") == "candidate_implemented", "runtime must be candidate_implemented", errors)
    checked += add_check(document.get("hardware_status") == "waiting_for_hardware", "hardware must remain waiting", errors)

    protocol = contract.get("protocol", {})
    protocol_expectations = {
        "mode": "modbus_rtu",
        "role": "slave",
        "address_model": "zero_based",
        "default_slave_address": 4,
        "allowed_slave_address_min": 1,
        "allowed_slave_address_max": 247,
        "broadcast_supported": False,
        "serial_profile": "19200_8E1",
        "register_byte_order": "big_endian",
        "multi_register_word_order": "high_word_first",
        "crc_wire_order": "low_byte_first",
        "crc_runtime_status": "candidate_validated",
        "parser_runtime_status": "candidate_implemented",
        "handler_runtime_status": "candidate_implemented",
    }
    for key, value in protocol_expectations.items():
        checked += add_check(protocol.get(key) == value, f"protocol.{key} must be {value!r}", errors)
    checked += add_check(
        protocol.get("supported_functions") == ["0x03", "0x04", "0x06"],
        "supported_functions must be exactly 0x03/0x04/0x06",
        errors,
    )

    data_types = contract.get("data_types", {})
    checked += add_check(set(data_types) == {"uint16", "int32", "uint32"}, "types must exclude float and remain uint16/int32/uint32", errors)
    for name, expected_count in (("uint16", 1), ("int32", 2), ("uint32", 2)):
        checked += add_check(data_types.get(name, {}).get("register_count") == expected_count, f"{name} register_count mismatch", errors)

    holding = contract.get("holding_registers", {})
    checked += add_check(holding.get("read_function") == "0x03", "holding read function must be 0x03", errors)
    checked += add_check(holding.get("write_function") == "0x06", "holding write function must be 0x06", errors)
    checked += add_check(holding.get("write_whitelist") == ["active_slave_address"], "0x06 whitelist must contain only active_slave_address", errors)
    checked += add_check(holding.get("write_commit") == "after_normal_echo_tx_complete", "address write must commit after echo TX complete", errors)
    checked += add_check(holding.get("persistence") == "volatile_reset_to_default_4", "address persistence must remain volatile reset-to-4", errors)
    holding_entries = holding.get("entries", [])
    entry_checked, holding_occupied = validate_entries(holding_entries, data_types, "holding", errors)
    checked += entry_checked
    checked += add_check(holding_occupied == set(range(4)), "holding map must occupy 0..3", errors)
    if isinstance(holding_entries, list):
        access = {entry.get("name"): entry.get("access") for entry in holding_entries if isinstance(entry, dict)}
        checked += add_check(access.get("active_slave_address") == "read_write", "active address must be read_write", errors)
        checked += add_check(all(access.get(name) == "read" for name in ("default_slave_address", "serial_profile", "configuration_generation")), "only active address may be writable", errors)

    input_section = contract.get("input_registers", {})
    checked += add_check(input_section.get("read_function") == "0x04", "input read function must be 0x04", errors)
    input_entries = input_section.get("entries", [])
    entry_checked, input_occupied = validate_entries(input_entries, data_types, "input", errors)
    checked += entry_checked
    checked += add_check(input_occupied == set(range(122)), "input map must be contiguous 0..121", errors)
    if isinstance(input_entries, list):
        checked += add_check(all(entry.get("access", "read") == "read" for entry in input_entries if isinstance(entry, dict)), "input registers must be read-only", errors)

    atomicity = contract.get("atomicity", {})
    atomicity_expectations = {
        "input_region_start": 0,
        "input_region_count": 122,
        "input_region_end": 121,
        "maximum_read_registers": 125,
        "contiguous": True,
        "response_source": "one_request_local_fixed_image",
        "mutex_policy": "copy_then_unlock_before_lookup_crc_or_encoding",
        "multi_request_consistency": "compare_register_image_generation",
        "unavailable_snapshot_policy": "server_device_failure",
        "runtime_status": "candidate_implemented",
    }
    for key, value in atomicity_expectations.items():
        checked += add_check(atomicity.get(key) == value, f"atomicity.{key} mismatch", errors)
    count = atomicity.get("input_region_count")
    maximum = atomicity.get("maximum_read_registers")
    checked += add_check(isinstance(count, int) and isinstance(maximum, int) and count <= maximum, "full input image must fit one read", errors)

    measurements = []
    metadata: dict[str, set[tuple[str, str, int]]] = {source: set() for source in EXPECTED_SOURCES}
    if isinstance(input_entries, list):
        for entry in input_entries:
            if not isinstance(entry, dict):
                continue
            if entry.get("category") == "measurement":
                measurements.append((entry.get("field_id"), entry.get("name"), entry.get("address"), entry.get("type"), entry.get("unit"), entry.get("source")))
                checked += add_check(entry.get("scale") == 1, f"{entry.get('name')}: scale must be 1", errors)
            if entry.get("category") == "metadata" and entry.get("source") in metadata:
                metadata[entry["source"]].add((entry.get("member"), entry.get("type"), entry.get("count")))
    checked += add_check(measurements == EXPECTED_MEASUREMENTS, "17-field measurement map differs from frozen oracle", errors)
    expected_metadata = set(EXPECTED_METADATA)
    for source in EXPECTED_SOURCES:
        checked += add_check(metadata[source] == expected_metadata, f"{source}: metadata set is incomplete or changed", errors)

    invalid_policy = contract.get("invalid_value_policy", {})
    checked += add_check(invalid_policy.get("sentinel_used") is False, "sentinel values are forbidden", errors)
    checked += add_check(invalid_policy.get("consumer_must_check") == ["state", "value_present"], "consumer must check state and value_present", errors)
    checked += add_check(set(invalid_policy.get("retained_value_requires", [])) == {"state", "age_ms", "quality", "value_retained"}, "retained value requirements are incomplete", errors)

    wire_codes = contract.get("wire_codes", {})
    for group, expected in EXPECTED_WIRE_CODES.items():
        checked += add_check(wire_codes.get(group) == expected, f"wire code group {group} changed", errors)

    p3 = contract.get("project_three_compatibility", {})
    checked += add_check(p3.get("existing_slave_addresses") == [1, 2, 3], "Project Three addresses must remain 1/2/3", errors)
    checked += add_check(p3.get("project_five_slave_address") == 4, "Project Five address must be 4", errors)
    checked += add_check(p3.get("project_three_write_authorized") is False, "Project Three writes are not authorized", errors)
    checked += add_check(p3.get("profile_status") == "not_created", "Project Three profile must remain not_created", errors)

    mapped_semantics = json.dumps(
        {
            "holding": holding_entries,
            "input": input_entries,
        },
        sort_keys=True,
    ).lower()
    for forbidden in ("float32", "motor_actuator", "can_id", "production_fault_injection"):
        checked += add_check(forbidden not in mapped_semantics, f"forbidden mapped semantic: {forbidden}", errors)
    return errors, checked


def validate_documents(root: Path) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    checks = {
        Path("docs/modbus_contract.md"): (
            "CANDIDATE_VALIDATED",
            "Runtime: `CANDIDATE_IMPLEMENTED`",
            "HIL readiness: `PASS_SELF_TEST + PASS_DRY_RUN + SERIAL_NOT_OPENED`",
            "Hardware: `WAITING_FOR_HARDWARE`",
            "`0x0000..0x0079`",
            "122",
            "request-local",
            "Project Three uses addresses",
            "Project Three address-4 profile | `not_created`",
        ),
        Path("protocol/README.md"): (
            "register map contract",
            "CRC/parser/handler",
            "`CANDIDATE_IMPLEMENTED`",
        ),
        Path("README.md"): (
            "P5-S5-T01",
            "默认 Modbus slave address contract 为 `4`",
            "runtime 为 `CANDIDATE_IMPLEMENTED`",
            "modbus_hil_probe.py --self-test",
            "serial NOT_OPENED",
        ),
        Path("tools/modbus_hil_probe.py"): (
            "P5 MODBUS HIL SELF-TEST",
            "serial NOT_OPENED",
            "--allow-address-write",
            "--confirm-default-address",
            "serial.PARITY_EVEN",
            "never auto-scanned",
        ),
        Path("docs/modbus_hil_report.md"): (
            "`FAIL_HARDWARE_RETURN_PATH / WAITING_FOR_CROSS_CHECK`",
            "serial NOT_OPENED",
            "249 B",
            "H01",
            "H11",
            "`not_created`",
        ),
        Path("docs/acceptance_protocol.md"): (
            "S5-T05",
            "H01～H11",
            "不要求持续抓包或大量重复",
        ),
        Path("docs/learning/index.md"): (
            "P5-S5-T05",
            "p5_s5_t05_项目三联调与主从站证据.md",
            "READY_FOR_CONTENT_REVIEW",
        ),
    }
    for relative, markers in checks.items():
        path = root / relative
        checked += 1
        try:
            content = path.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as exc:
            errors.append(f"{relative}: cannot read: {exc}")
            continue
        for marker in markers:
            checked += add_check(marker in content, f"{relative}: missing {marker!r}", errors)
    return errors, checked


def run_self_test(contract: dict[str, Any]) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    Mutation = Callable[[dict[str, Any]], None]

    def mutate_default_address(value: dict[str, Any]) -> None:
        value["protocol"]["default_slave_address"] = 2

    def mutate_overlap(value: dict[str, Any]) -> None:
        value["input_registers"]["entries"][1]["address"] = 0

    def mutate_overflow(value: dict[str, Any]) -> None:
        value["input_registers"]["entries"][-1]["address"] = 65535

    def mutate_count(value: dict[str, Any]) -> None:
        value["input_registers"]["entries"][16]["count"] = 1

    def mutate_input_write(value: dict[str, Any]) -> None:
        value["input_registers"]["entries"][0]["access"] = "read_write"

    def mutate_whitelist(value: dict[str, Any]) -> None:
        value["holding_registers"]["write_whitelist"].append("configuration_generation")

    def mutate_float(value: dict[str, Any]) -> None:
        value["data_types"]["float32"] = {"register_count": 2, "signed": True}

    def mutate_metadata(value: dict[str, Any]) -> None:
        entries = value["input_registers"]["entries"]
        entries[:] = [entry for entry in entries if entry.get("name") != "bme280_age_ms"]

    def mutate_word_order(value: dict[str, Any]) -> None:
        value["protocol"]["multi_register_word_order"] = "low_word_first"

    def mutate_max_read(value: dict[str, Any]) -> None:
        value["atomicity"]["input_region_count"] = 126

    def mutate_motor(value: dict[str, Any]) -> None:
        value["input_registers"]["entries"][0]["name"] = "motor_actuator_target"

    def mutate_runtime(value: dict[str, Any]) -> None:
        value["document"]["runtime_status"] = "pass"

    mutations: list[tuple[str, Mutation]] = [
        ("default-address-2", mutate_default_address),
        ("overlap", mutate_overlap),
        ("overflow", mutate_overflow),
        ("wrong-count", mutate_count),
        ("input-write", mutate_input_write),
        ("extra-write-whitelist", mutate_whitelist),
        ("float", mutate_float),
        ("missing-metadata", mutate_metadata),
        ("word-order", mutate_word_order),
        ("over-125", mutate_max_read),
        ("motor-semantics", mutate_motor),
        ("runtime-pass", mutate_runtime),
    ]
    for label, mutate in mutations:
        candidate = copy.deepcopy(contract)
        mutate(candidate)
        mutant_errors, _ = validate_contract(candidate)
        checked += 1
        if not mutant_errors:
            errors.append(f"self-test mutant was accepted: {label}")
    return errors, checked


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "map_path",
        nargs="?",
        type=Path,
        default=Path("protocol/register_map.json"),
    )
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    try:
        contract = load_contract(args.map_path)
    except (OSError, UnicodeError, ValueError, json.JSONDecodeError) as exc:
        print(f"P5 MODBUS CONTRACT: FAIL: cannot load {args.map_path}: {exc}", file=sys.stderr)
        return 1

    errors, checked = validate_contract(contract)
    document_errors, document_checked = validate_documents(args.root)
    errors.extend(document_errors)
    checked += document_checked

    if args.self_test and not errors:
        self_errors, self_checked = run_self_test(contract)
        errors.extend(self_errors)
        checked += self_checked

    if errors:
        for error in errors:
            print(f"FAIL: {error}", file=sys.stderr)
        return 1

    if args.self_test:
        print(
            "P5 MODBUS CONTRACT SELF-TEST: PASS "
            "(address, overlap, overflow, width, access, whitelist, float, "
            "metadata, word-order, read-limit, foreign-semantics and runtime mutants rejected)"
        )
    else:
        print(
            f"P5 MODBUS CONTRACT: PASS ({checked} facts, 122 input + 4 holding, "
            "candidate implemented, hardware waiting)"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
