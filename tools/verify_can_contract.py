#!/usr/bin/env python3
"""Validate the bounded P5-S6-T01 wire and P5-S6-T02 runtime contract."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "protocol" / "can_message_map.json"

EXPECTED_IDS = {
    "status_event": 0x140,
    "heartbeat": 0x240,
    "health_summary": 0x241,
    "climate_primary": 0x340,
    "climate_secondary": 0x341,
    "illuminance": 0x342,
    "vibration_summary": 0x440,
    "diagnostic_ping_request": 0x540,
    "diagnostic_ping_response": 0x541,
}

CAN_TO_MODBUS_UNITS = {
    ("climate_primary", "temperature"): ("bme280_temperature", "centi_deg_c"),
    ("climate_primary", "pressure"): ("bme280_pressure", "pa"),
    ("climate_secondary", "humidity"): ("bme280_humidity", "milli_percent_rh"),
    ("illuminance", "illuminance"): ("veml7700_illuminance", "millilux"),
    ("vibration_summary", "resultant_rms"): ("adxl345_resultant_rms", "millig"),
}

DOCUMENT_MARKERS = {
    Path("README.md"): [
        "P5-S6-T02",
        "CAN_CONTRACT_CANDIDATE_VALIDATED",
        "CAN_RUNTIME_CANDIDATE_IMPLEMENTED",
    ],
    Path("protocol/README.md"): [
        "can_message_map.json",
        "CANDIDATE_VALIDATED",
        "CANDIDATE_IMPLEMENTED",
    ],
    Path("docs/can_contract.md"): [
        "Contract status: `CANDIDATE_VALIDATED`",
        "Runtime status: `CANDIDATE_IMPLEMENTED`",
        "Hardware status: `WAITING_FOR_HARDWARE`",
        "0x140",
        "0x440",
        "0x540",
        "0x541",
        "0.78%",
    ],
    Path("docs/learning/index.md"): [
        "p5_s6_t02_bxcan过滤器中断与发送队列.md",
        "READY_FOR_CONTENT_REVIEW",
    ],
    Path("docs/learning/p5_s6_t02_bxcan过滤器中断与发送队列.md"): [
        "P5-S6-T02",
        "实际问题与修复",
        "WAITING_FOR_HARDWARE",
    ],
    Path("docs/can_runtime.md"): [
        "CAN_RUNTIME_CANDIDATE_IMPLEMENTED",
        "RECOVERY_LATCHED",
        "WAITING_FOR_HARDWARE",
    ],
}

RUNTIME_SOURCE_MARKERS = {
    Path("freertos_modbus_can_node.ioc"): [
        "NVIC.CAN1_TX_IRQn=true\\:6\\:0",
        "NVIC.CAN1_RX0_IRQn=true\\:6\\:0",
        "NVIC.CAN1_SCE_IRQn=true\\:6\\:0",
    ],
    Path("Core/Src/stm32f4xx_it.c"): [
        "void CAN1_TX_IRQHandler(void)",
        "void CAN1_RX0_IRQHandler(void)",
        "void CAN1_SCE_IRQHandler(void)",
    ],
    Path("app/include/app_can_runtime.h"): [
        "APP_CAN_TX_DRAIN_BUDGET",
        "APP_CAN_RECOVERY_ATTEMPT_LIMIT",
    ],
    Path("app/src/app_rtos.c"): [
        "app_rtos_run_can",
        "app_rtos_can_notify_from_isr",
    ],
    Path("bsp/src/bsp_can.c"): [
        "HAL_CAN_RxFifo0MsgPendingCallback",
        "HAL_CAN_ErrorCallback",
    ],
}


def add_check(condition: bool, message: str, errors: list[str]) -> int:
    if not condition:
        errors.append(message)
    return 1


def frame_by_name(document: dict[str, Any]) -> dict[str, dict[str, Any]]:
    frames = document.get("frames", [])
    return {
        frame.get("name", ""): frame
        for frame in frames
        if isinstance(frame, dict)
    }


def verify_document(document: dict[str, Any]) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    metadata = document.get("document", {})
    physical = document.get("physical_layer", {})
    addressing = document.get("addressing", {})
    wire = document.get("wire", {})
    codes = document.get("wire_codes", {})
    scheduling = document.get("scheduling", {})
    runtime = document.get("runtime", {})
    traceability = document.get("traceability", {})
    frames = document.get("frames", [])
    named = frame_by_name(document)

    checked += add_check(
        metadata.get("project") == "freertos_modbus_can_node",
        "wrong project",
        errors,
    )
    checked += add_check(
        metadata.get("task_id") == "P5-S6-T01",
        "wrong task id",
        errors,
    )
    checked += add_check(
        metadata.get("status") == "candidate_validated",
        "contract must be candidate_validated",
        errors,
    )
    checked += add_check(
        metadata.get("runtime_task_id") == "P5-S6-T02",
        "wrong runtime task id",
        errors,
    )
    checked += add_check(
        metadata.get("runtime_status") == "candidate_implemented",
        "runtime boundary changed",
        errors,
    )
    checked += add_check(
        metadata.get("hardware_status") == "waiting_for_hardware",
        "hardware boundary changed",
        errors,
    )

    expected_physical = {
        "protocol": "classical_can_2_0a",
        "identifier_format": "standard_11_bit",
        "frame_kind": "data_only",
        "nominal_bitrate_bit_s": 500000,
        "dlc": 8,
        "can_fd": False,
        "extended_frames": False,
        "remote_frames": False,
        "hardware_validation": "not_run",
    }
    for key, value in expected_physical.items():
        checked += add_check(
            physical.get(key) == value,
            f"physical_layer.{key} mismatch",
            errors,
        )

    checked += add_check(addressing.get("node_id") == 4, "node id must be 4", errors)
    checked += add_check(
        addressing.get("modbus_address_space") == "independent",
        "CAN and Modbus address spaces must remain independent",
        errors,
    )
    checked += add_check(
        addressing.get("protocol_claim")
        == "project_specific_not_canopen_j1939_uds",
        "foreign protocol claim changed",
        errors,
    )
    checked += add_check(wire.get("schema_revision") == 1, "wrong revision", errors)
    checked += add_check(wire.get("byte_order") == "little_endian", "wrong endian", errors)
    checked += add_check(wire.get("sequence_bits") == 8, "wrong sequence width", errors)
    checked += add_check(
        wire.get("sequence_wrap") == "modulo_256",
        "wrong sequence wrap",
        errors,
    )
    checked += add_check(
        wire.get("structure_copy_allowed") is False,
        "C structure copy must remain forbidden",
        errors,
    )

    checked += add_check(len(frames) == len(EXPECTED_IDS), "wrong frame count", errors)
    checked += add_check(set(named) == set(EXPECTED_IDS), "frame names changed", errors)
    identifiers: list[int] = []
    for name, expected_id in EXPECTED_IDS.items():
        frame = named.get(name, {})
        identifier = frame.get("identifier")
        identifiers.append(identifier if isinstance(identifier, int) else -1)
        checked += add_check(identifier == expected_id, f"{name}: wrong id", errors)
        checked += add_check(
            isinstance(identifier, int) and 0 <= identifier <= 0x7FF,
            f"{name}: identifier is not standard 11-bit",
            errors,
        )
        checked += add_check(
            frame.get("identifier_hex") == f"0x{expected_id:03X}",
            f"{name}: hex id mismatch",
            errors,
        )
        fields = frame.get("fields", [])
        occupied: list[int] = []
        for field in fields:
            offset = field.get("offset")
            width = field.get("width")
            if isinstance(offset, int) and isinstance(width, int):
                occupied.extend(range(offset, offset + width))
            else:
                errors.append(f"{name}: invalid field offset/width")
        checked += 1
        if sorted(occupied) != list(range(8)):
            errors.append(f"{name}: fields must cover each payload byte once")
        checked += add_check(
            fields[:2]
            and fields[0].get("name") == "schema_revision"
            and fields[1].get("name") == "sequence",
            f"{name}: common prefix mismatch",
            errors,
        )

    checked += add_check(
        len(set(identifiers)) == len(identifiers),
        "CAN identifiers must be unique",
        errors,
    )
    checked += add_check(
        identifiers == sorted(identifiers),
        "message map must remain in arbitration order",
        errors,
    )

    data_flags = codes.get("data_flags", {})
    checked += add_check(
        data_flags.get("reserved_mask") == 0xF0,
        "data flags reserved mask changed",
        errors,
    )
    checked += add_check(
        codes.get("measurement_state")
        == {"invalid": 0, "fresh": 1, "stale": 2, "offline": 3},
        "measurement wire states changed",
        errors,
    )
    checked += add_check(
        codes.get("health_state", {}).get("reset_loop_latched") == 5,
        "health state range changed",
        errors,
    )

    periodic = scheduling.get("periodic_frames_per_second")
    event_budget = scheduling.get("event_budget_frames_per_second")
    diagnostic_budget = scheduling.get(
        "diagnostic_response_budget_frames_per_second"
    )
    bits_per_frame = scheduling.get("conservative_bits_per_frame")
    bitrate = physical.get("nominal_bitrate_bit_s")
    calculated = None
    load_inputs = (
        periodic,
        event_budget,
        diagnostic_budget,
        bits_per_frame,
        bitrate,
    )
    if all(isinstance(value, (int, float)) for value in load_inputs) and bitrate:
        calculated = (
            (periodic + event_budget + diagnostic_budget)
            * bits_per_frame
            * 100.0
            / bitrate
        )
    checked += add_check(
        calculated is not None and abs(calculated - 0.78) < 1e-9,
        "bus load calculation mismatch",
        errors,
    )
    checked += add_check(
        calculated is not None
        and calculated <= scheduling.get("contract_bus_load_limit_percent", 0),
        "bus load exceeds contract limit",
        errors,
    )
    expected_runtime = {
        "task": "can_task",
        "task_period_ms": 100,
        "notification_wait_ms_max": 100,
        "tx_drain_budget": 3,
        "rx_drain_budget": 2,
        "event_fifo_depth": 8,
        "diagnostic_response_slots": 1,
        "diagnostic_min_interval_ms": 100,
        "diagnostic_default_enabled": True,
        "rx_ring_depth": 4,
        "recovery_delay_ms": 1000,
        "recovery_attempt_limit": 3,
        "dynamic_allocation": False,
        "hardware_validation": "not_run",
    }
    checked += add_check(
        runtime == expected_runtime,
        "bounded runtime policy changed",
        errors,
    )
    checked += add_check(
        traceability.get("state_codes_match_unified_measurement") is True,
        "measurement state traceability missing",
        errors,
    )
    checked += add_check(
        traceability.get("modbus_wire_layout_may_differ") is True,
        "cross-interface layout boundary missing",
        errors,
    )

    serialized = json.dumps(document, ensure_ascii=False).lower()
    for forbidden in (
        '"canopen"',
        '"j1939"',
        '"uds"',
        '"register_address"',
        '"extended_frames": true',
        '"remote_frames": true',
    ):
        checked += add_check(
            forbidden not in serialized,
            f"forbidden CAN contract content: {forbidden}",
            errors,
        )

    return errors, checked


def verify_cross_interface(
    can_document: dict[str, Any],
    modbus_document: dict[str, Any],
) -> tuple[list[str], int]:
    errors: list[str] = []
    checked = 0
    can_states = can_document.get("wire_codes", {}).get("measurement_state")
    modbus_states = modbus_document.get("wire_codes", {}).get("measurement_state")
    checked += add_check(
        can_states == modbus_states,
        "CAN and Modbus measurement state codes differ",
        errors,
    )

    can_frames = frame_by_name(can_document)
    modbus_entries = {
        entry.get("name"): entry
        for entry in modbus_document.get("input_registers", {}).get("entries", [])
        if isinstance(entry, dict)
    }
    for (frame_name, field_name), (register_name, expected_unit) in CAN_TO_MODBUS_UNITS.items():
        fields = {
            field.get("name"): field
            for field in can_frames.get(frame_name, {}).get("fields", [])
            if isinstance(field, dict)
        }
        can_unit = str(fields.get(field_name, {}).get("unit", "")).lower()
        modbus_unit = str(modbus_entries.get(register_name, {}).get("unit", "")).lower()
        checked += add_check(
            can_unit == expected_unit and modbus_unit == expected_unit,
            f"cross-interface unit mismatch: {frame_name}.{field_name}",
            errors,
        )
    return errors, checked


def verify_repository(root: Path) -> tuple[list[str], int]:
    try:
        document = json.loads((root / "protocol/can_message_map.json").read_text(encoding="utf-8"))
        modbus_document = json.loads(
            (root / "protocol/register_map.json").read_text(encoding="utf-8")
        )
    except (OSError, json.JSONDecodeError) as exc:
        return [f"cannot load CAN/Modbus map: {exc}"], 0

    errors, checked = verify_document(document)
    cross_errors, cross_checked = verify_cross_interface(document, modbus_document)
    errors.extend(cross_errors)
    checked += cross_checked
    for relative_path, markers in DOCUMENT_MARKERS.items():
        path = root / relative_path
        try:
            content = path.read_text(encoding="utf-8")
        except OSError as exc:
            errors.append(f"{relative_path}: cannot read: {exc}")
            continue
        for marker in markers:
            checked += add_check(
                marker in content,
                f"{relative_path}: missing marker: {marker}",
                errors,
            )
    for relative_path, markers in RUNTIME_SOURCE_MARKERS.items():
        path = root / relative_path
        try:
            content = path.read_text(encoding="utf-8")
        except OSError as exc:
            errors.append(f"{relative_path}: cannot read: {exc}")
            continue
        for marker in markers:
            checked += add_check(
                marker in content,
                f"{relative_path}: missing runtime marker: {marker}",
                errors,
            )
    return errors, checked


def run_self_test(document: dict[str, Any]) -> int:
    mutants: list[tuple[str, dict[str, Any]]] = []

    duplicate = copy.deepcopy(document)
    duplicate["frames"][1]["identifier"] = duplicate["frames"][0]["identifier"]
    mutants.append(("duplicate id", duplicate))

    extended = copy.deepcopy(document)
    extended["frames"][0]["identifier"] = 0x800
    mutants.append(("extended id", extended))

    gap = copy.deepcopy(document)
    gap["frames"][2]["fields"][2]["offset"] = 3
    mutants.append(("payload overlap", gap))

    budget = copy.deepcopy(document)
    budget["scheduling"]["event_budget_frames_per_second"] = 100
    mutants.append(("budget overflow", budget))

    runtime = copy.deepcopy(document)
    runtime["document"]["runtime_status"] = "not_implemented"
    mutants.append(("runtime promotion", runtime))

    for label, mutant in mutants:
        errors, _ = verify_document(mutant)
        if not errors:
            print(f"CAN CONTRACT SELF-TEST: FAIL ({label} accepted)")
            return 1

    print(f"P5 CAN CONTRACT SELF-TEST: PASS ({len(mutants)} mutants rejected)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    try:
        document = json.loads(MAP_PATH.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        print(f"P5 CAN CONTRACT: FAIL (cannot load map: {exc})")
        return 1

    if args.self_test:
        return run_self_test(document)

    errors, checked = verify_repository(ROOT)
    if errors:
        print(f"P5 CAN CONTRACT: FAIL ({len(errors)} errors after {checked} checks)")
        for error in errors:
            print(f"- {error}")
        return 1

    print(
        "P5 CAN CONTRACT: PASS "
        f"({checked} facts, 9 standard IDs, 500 kbit/s, runtime candidate implemented, hardware waiting)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
