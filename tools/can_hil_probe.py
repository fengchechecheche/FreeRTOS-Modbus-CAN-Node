#!/usr/bin/env python3
"""Bounded SocketCAN preflight and HIL probe for P5-S6-T03."""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any

CAN_EFF_FLAG = 0x80000000
CAN_RTR_FLAG = 0x40000000
CAN_ERR_FLAG = 0x20000000
CAN_SFF_MASK = 0x000007FF
CAN_FRAME = struct.Struct("=IB3x8s")
SCHEMA_REVISION = 1
PERIODIC_IDS = {0x240, 0x241, 0x340, 0x341, 0x342, 0x440}
MAX_CAPTURED_FRAMES = 128


class ProbeError(ValueError):
    """A classified frame or environment error."""

    def __init__(self, code: str, detail: str) -> None:
        super().__init__(detail)
        self.code = code
        self.detail = detail


@dataclass(frozen=True)
class CanFrame:
    identifier: int
    data: bytes
    dlc: int = 8
    extended: bool = False
    rtr: bool = False
    error: bool = False


def repository_root() -> Path:
    return Path(__file__).resolve().parents[1]


def load_message_map(path: Path) -> dict[str, Any]:
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise ProbeError("MAP_LOAD", f"cannot load {path}: {exc}") from exc

    physical = document.get("physical_layer", {})
    if physical.get("nominal_bitrate_bit_s") != 500000:
        raise ProbeError("MAP_CONTRACT", "nominal bitrate is not 500000 bit/s")
    if physical.get("dlc") != 8:
        raise ProbeError("MAP_CONTRACT", "DLC is not 8")
    if physical.get("identifier_format") != "standard_11_bit":
        raise ProbeError("MAP_CONTRACT", "identifier format is not standard_11_bit")

    frames = document.get("frames", [])
    identifiers = {frame.get("identifier") for frame in frames}
    expected = {0x140, 0x240, 0x241, 0x340, 0x341, 0x342, 0x440}
    if identifiers != expected:
        raise ProbeError("MAP_CONTRACT", "message map does not contain the seven frozen IDs")
    return document


def frame_definitions(message_map: dict[str, Any]) -> dict[int, dict[str, Any]]:
    return {int(frame["identifier"]): frame for frame in message_map["frames"]}


def integer_from(data: bytes, offset: int, width: int, signed: bool) -> int:
    return int.from_bytes(data[offset : offset + width], "little", signed=signed)


def decode_frame(
    frame: CanFrame,
    message_map: dict[str, Any],
) -> dict[str, Any]:
    if frame.error:
        raise ProbeError("ERROR_FRAME", "SocketCAN error frame")
    if frame.extended:
        raise ProbeError("EXTENDED_ID", "extended identifier is outside revision 1")
    if frame.rtr:
        raise ProbeError("RTR_FRAME", "remote frames are outside revision 1")
    if frame.dlc != 8:
        raise ProbeError("INVALID_DLC", f"DLC {frame.dlc}, expected 8")
    if len(frame.data) < frame.dlc:
        raise ProbeError("TRUNCATED", "payload is shorter than DLC")

    definitions = frame_definitions(message_map)
    definition = definitions.get(frame.identifier)
    if definition is None:
        raise ProbeError("UNKNOWN_ID", f"unknown standard ID 0x{frame.identifier:03X}")
    if frame.data[0] != SCHEMA_REVISION:
        raise ProbeError(
            "INVALID_SCHEMA",
            f"schema revision {frame.data[0]}, expected {SCHEMA_REVISION}",
        )

    values: dict[str, int] = {}
    for field in definition["fields"]:
        field_type = field["type"]
        signed = field_type.startswith("int")
        value = integer_from(frame.data, field["offset"], field["width"], signed)
        valid_max = field.get("valid_max")
        special = {field.get("invalid"), field.get("unknown")}
        special.discard(None)
        if valid_max is not None and value > valid_max and value not in special:
            raise ProbeError(
                "OUT_OF_RANGE",
                f"{definition['name']}.{field['name']}={value} exceeds {valid_max}",
            )
        values[field["name"]] = value

    name = definition["name"]
    if name == "status_event":
        if values["severity"] > max(message_map["wire_codes"]["severity"].values()):
            raise ProbeError("OUT_OF_RANGE", "status_event.severity is out of range")
        if values["source"] > max(message_map["wire_codes"]["event_source"].values()):
            raise ProbeError("OUT_OF_RANGE", "status_event.source is out of range")
    elif name == "health_summary":
        if values["health_state"] > max(
            message_map["wire_codes"]["health_state"].values()
        ):
            raise ProbeError("OUT_OF_RANGE", "health_summary.health_state is out of range")
    elif name == "climate_primary":
        if values["data_flags"] & message_map["wire_codes"]["data_flags"][
            "reserved_mask"
        ]:
            raise ProbeError("RESERVED_BITS", "climate_primary data_flags use reserved bits")
    elif name in {"illuminance", "vibration_summary"}:
        if values["data_flags"] & message_map["wire_codes"]["data_flags"][
            "reserved_mask"
        ]:
            raise ProbeError("RESERVED_BITS", f"{name} data_flags use reserved bits")

    return {
        "identifier": f"0x{frame.identifier:03X}",
        "name": name,
        "sequence": values["sequence"],
        "fields": values,
    }


def known_good_frames() -> list[CanFrame]:
    return [
        CanFrame(0x140, bytes.fromhex("01 01 00 01 01 05 00 00")),
        CanFrame(0x240, bytes.fromhex("01 02 7B 00 00 00 01 00")),
        CanFrame(0x241, bytes.fromhex("01 03 01 55 00 00 00 00")),
        CanFrame(0x340, bytes.fromhex("01 04 C4 09 CD 8B 01 01")),
        CanFrame(0x341, bytes.fromhex("01 04 50 C3 00 00 0A 00")),
        CanFrame(0x342, bytes.fromhex("01 05 40 42 0F 00 01 0A")),
        CanFrame(0x440, bytes.fromhex("01 06 E8 03 00 00 01 0A")),
    ]


def exercise_frames() -> list[CanFrame]:
    good = known_good_frames()
    heartbeat = good[1]
    bad_schema = bytearray(heartbeat.data)
    bad_schema[0] = 2
    bad_flags = bytearray(good[5].data)
    bad_flags[6] = 0xF1
    return good + [
        good[1],
        CanFrame(0x123, bytes.fromhex("01 00 00 00 00 00 00 00")),
        CanFrame(0x240, heartbeat.data[:7], dlc=7),
        CanFrame(0x240, bytes(bad_schema)),
        CanFrame(0x342, bytes(bad_flags)),
    ]


def summarize_frames(
    frames: list[CanFrame],
    message_map: dict[str, Any],
) -> dict[str, Any]:
    accepted: list[dict[str, Any]] = []
    rejected: Counter[str] = Counter()
    counts: Counter[str] = Counter()
    seen_payloads: Counter[tuple[int, bytes]] = Counter()
    representative: dict[str, dict[str, Any]] = {}
    primary_sequences: Counter[int] = Counter()
    pair_count = 0
    pair_mismatch = 0

    for frame in frames:
        try:
            decoded = decode_frame(frame, message_map)
        except ProbeError as exc:
            rejected[exc.code] += 1
            continue
        accepted.append(decoded)
        counts[decoded["identifier"]] += 1
        representative.setdefault(decoded["identifier"], decoded)
        seen_payloads[(frame.identifier, frame.data[: frame.dlc])] += 1
        if frame.identifier == 0x340:
            primary_sequences[decoded["sequence"]] += 1
        elif frame.identifier == 0x341:
            sequence = decoded["sequence"]
            if primary_sequences[sequence] > 0:
                primary_sequences[sequence] -= 1
                pair_count += 1
            else:
                pair_mismatch += 1

    duplicates = sum(max(0, count - 1) for count in seen_payloads.values())
    observed = {int(identifier, 16) for identifier in counts}
    return {
        "accepted": len(accepted),
        "rejected": sum(rejected.values()),
        "rejection_categories": dict(sorted(rejected.items())),
        "counts_by_id": dict(sorted(counts.items())),
        "periodic_ids_observed": [f"0x{value:03X}" for value in sorted(observed & PERIODIC_IDS)],
        "missing_periodic_ids": [
            f"0x{value:03X}" for value in sorted(PERIODIC_IDS - observed)
        ],
        "status_event_observed": 0x140 in observed,
        "duplicates": duplicates,
        "bme_pairs": pair_count,
        "bme_pair_mismatches": pair_mismatch,
        "representative": representative,
    }


def run_self_test(message_map: dict[str, Any]) -> dict[str, Any]:
    good = known_good_frames()
    decoded = [decode_frame(frame, message_map) for frame in good]
    if len(decoded) != 7:
        raise ProbeError("SELF_TEST", "known-good vector count mismatch")

    mutations = exercise_frames()[8:]
    expected = ["UNKNOWN_ID", "INVALID_DLC", "INVALID_SCHEMA", "RESERVED_BITS"]
    observed: list[str] = []
    for frame in mutations:
        try:
            decode_frame(frame, message_map)
        except ProbeError as exc:
            observed.append(exc.code)
    if observed != expected:
        raise ProbeError("SELF_TEST", f"mutation result mismatch: {observed}")

    mismatched_secondary = bytearray(good[4].data)
    mismatched_secondary[1] = 0x7F
    summary = summarize_frames(
        good + [good[1], CanFrame(0x341, bytes(mismatched_secondary))],
        message_map,
    )
    if summary["duplicates"] != 1 or summary["bme_pairs"] != 1:
        raise ProbeError("SELF_TEST", "duplicate or BME pair policy mismatch")
    if summary["bme_pair_mismatches"] != 1:
        raise ProbeError("SELF_TEST", "BME mismatch was not classified")

    packed = pack_socketcan(good[1])
    if unpack_socketcan(packed) != good[1]:
        raise ProbeError("SELF_TEST", "SocketCAN frame packing mismatch")

    return {
        "mode": "self-test",
        "status": "PASS_HOST",
        "interface": "NOT_OPENED",
        "known_good_vectors": 7,
        "mutants_rejected": len(expected),
        "duplicate_policy": "COUNT_NOT_FAIL",
        "bme_pair_policy": "SAME_SEQUENCE_ONLY",
        "hardware": "NOT_RUN",
    }


def pack_socketcan(frame: CanFrame) -> bytes:
    can_id = frame.identifier & CAN_SFF_MASK
    if frame.extended:
        can_id |= CAN_EFF_FLAG
    if frame.rtr:
        can_id |= CAN_RTR_FLAG
    if frame.error:
        can_id |= CAN_ERR_FLAG
    return CAN_FRAME.pack(can_id, frame.dlc, frame.data.ljust(8, b"\x00")[:8])


def unpack_socketcan(raw: bytes) -> CanFrame:
    if len(raw) != CAN_FRAME.size:
        raise ProbeError("SOCKET_FRAME", f"received {len(raw)} bytes, expected 16")
    can_id, dlc, payload = CAN_FRAME.unpack(raw)
    return CanFrame(
        identifier=can_id & CAN_SFF_MASK,
        data=payload[:dlc],
        dlc=dlc,
        extended=bool(can_id & CAN_EFF_FLAG),
        rtr=bool(can_id & CAN_RTR_FLAG),
        error=bool(can_id & CAN_ERR_FLAG),
    )


def open_can_socket(interface: str) -> socket.socket:
    if not hasattr(socket, "AF_CAN"):
        raise ProbeError("SOCKETCAN_UNAVAILABLE", "Python has no AF_CAN support")
    try:
        can_socket = socket.socket(socket.AF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
        can_socket.bind((interface,))
    except OSError as exc:
        raise ProbeError("INTERFACE_OPEN", f"cannot open {interface}: {exc}") from exc
    return can_socket


def receive_bounded(
    can_socket: socket.socket,
    duration_s: float,
    expected_count: int | None = None,
) -> list[CanFrame]:
    frames: list[CanFrame] = []
    deadline = time.monotonic() + duration_s
    can_socket.settimeout(min(0.2, duration_s))
    while time.monotonic() < deadline and len(frames) < MAX_CAPTURED_FRAMES:
        try:
            frames.append(unpack_socketcan(can_socket.recv(CAN_FRAME.size)))
        except TimeoutError:
            continue
        except OSError as exc:
            raise ProbeError("SOCKET_RECV", str(exc)) from exc
        if expected_count is not None and len(frames) >= expected_count:
            break
    return frames


def run_vcan_test(
    interface: str,
    allow_send: bool,
    message_map: dict[str, Any],
) -> tuple[dict[str, Any], list[CanFrame]]:
    if not allow_send:
        raise ProbeError("SEND_LOCKED", "--vcan-self-test requires --allow-send")
    if not interface.startswith("vcan"):
        raise ProbeError("VCAN_GUARD", "vcan self-test requires an interface named vcan*")

    vectors = exercise_frames()
    rx_socket = open_can_socket(interface)
    tx_socket = open_can_socket(interface)
    try:
        for frame in vectors:
            tx_socket.send(pack_socketcan(frame))
        captured = receive_bounded(rx_socket, 2.0, expected_count=len(vectors))
    except OSError as exc:
        raise ProbeError("SOCKET_SEND", str(exc)) from exc
    finally:
        tx_socket.close()
        rx_socket.close()

    summary = summarize_frames(captured, message_map)
    expected_rejections = {
        "INVALID_DLC": 1,
        "INVALID_SCHEMA": 1,
        "RESERVED_BITS": 1,
        "UNKNOWN_ID": 1,
    }
    passed = (
        len(captured) == len(vectors)
        and summary["missing_periodic_ids"] == []
        and summary["rejection_categories"] == expected_rejections
        and summary["duplicates"] == 1
        and summary["bme_pairs"] == 1
        and summary["bme_pair_mismatches"] == 0
    )
    result = {
        "mode": "vcan-self-test",
        "status": "PASS_HOST" if passed else "FAIL_HOST",
        "interface": interface,
        "sent": len(vectors),
        "captured": len(captured),
        "summary": summary,
        "physical_can": "NOT_RUN",
    }
    return result, captured


def run_observe(
    interface: str,
    observe_seconds: float,
    message_map: dict[str, Any],
) -> tuple[dict[str, Any], list[CanFrame]]:
    can_socket = open_can_socket(interface)
    try:
        captured = receive_bounded(can_socket, observe_seconds)
    except OSError as exc:
        raise ProbeError("SOCKET_IO", str(exc)) from exc
    finally:
        can_socket.close()

    summary = summarize_frames(captured, message_map)
    receive_pass = not summary["missing_periodic_ids"] and summary["bme_pairs"] >= 1
    status = "PASS_HARDWARE_RX_CANDIDATE" if receive_pass else "FAIL_OBSERVE"
    return {
        "mode": "observe",
        "status": status,
        "interface": interface,
        "observe_seconds": observe_seconds,
        "captured": len(captured),
        "host_send_attempted": False,
        "host_send_evidence_boundary": (
            "physical observe is receive-only; use the reviewed RX_ONLY diagnostic "
            "for bounded host-to-device ACK evidence"
        ),
        "summary": summary,
    }, captured


def write_optional_output(
    output_dir: Path | None,
    result: dict[str, Any],
    frames: list[CanFrame],
) -> None:
    if output_dir is None:
        return
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "summary.json").write_text(
        json.dumps(result, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    bounded = frames[:MAX_CAPTURED_FRAMES]
    lines = [
        f"{frame.identifier:03X}#{frame.data[:frame.dlc].hex().upper()}"
        for frame in bounded
    ]
    (output_dir / "frames.log").write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--self-test", action="store_true")
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--vcan-self-test", action="store_true")
    mode.add_argument("--observe", action="store_true")
    parser.add_argument("--interface")
    parser.add_argument("--observe-seconds", type=float, default=10.0)
    parser.add_argument("--allow-send", action="store_true")
    parser.add_argument("--output-dir", type=Path)
    parser.add_argument(
        "--map",
        dest="map_path",
        type=Path,
        default=repository_root() / "protocol" / "can_message_map.json",
    )
    return parser


def run(args: argparse.Namespace) -> tuple[dict[str, Any], list[CanFrame]]:
    message_map = load_message_map(args.map_path)
    if not 0.1 <= args.observe_seconds <= 60.0:
        raise ProbeError("ARGUMENT", "--observe-seconds must be in [0.1, 60]")
    if args.allow_send and not args.vcan_self_test:
        raise ProbeError(
            "SEND_OWNERSHIP",
            "--allow-send is reserved for --vcan-self-test; physical observe is receive-only",
        )

    if args.self_test:
        return run_self_test(message_map), []
    if args.dry_run:
        return {
            "mode": "dry-run",
            "status": "READY_FOR_HOST",
            "interface": "NOT_OPENED",
            "hardware": "NOT_RUN",
            "planned_ids": [f"0x{value:03X}" for value in sorted(frame_definitions(message_map))],
            "planned_steps": [
                "create or select an explicitly named SocketCAN interface",
                "observe physical CAN without transmitting node-owned telemetry IDs",
                "decode revision 1 standard DLC-8 frames",
                "keep only bounded troubleshooting output",
            ],
        }, []
    if not args.interface:
        raise ProbeError("ARGUMENT", "interface mode requires --interface")
    if args.vcan_self_test:
        return run_vcan_test(args.interface, args.allow_send, message_map)
    return run_observe(args.interface, args.observe_seconds, message_map)


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    try:
        result, frames = run(args)
        write_optional_output(args.output_dir, result, frames)
    except ProbeError as exc:
        print(
            json.dumps(
                {"status": "ERROR", "error": exc.code, "detail": exc.detail},
                ensure_ascii=False,
            ),
            file=sys.stderr,
        )
        return 2
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return 0 if not result.get("status", "").startswith("FAIL") else 1


if __name__ == "__main__":
    raise SystemExit(main())
