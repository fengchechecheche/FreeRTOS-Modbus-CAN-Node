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
SOL_CAN_RAW = 101
CAN_RAW_ERR_FILTER = 2
CAN_ERR_MASK = 0x1FFFFFFF
CAN_FRAME = struct.Struct("=IB3x8s")
SCHEMA_REVISION = 1
PERIODIC_IDS = {0x240, 0x241, 0x340, 0x341, 0x342, 0x440}
DIAGNOSTIC_REQUEST_ID = 0x540
DIAGNOSTIC_RESPONSE_ID = 0x541
DIAGNOSTIC_OPCODE_PING = 0x01
DIAGNOSTIC_STATUS_OK = 0x00
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
    expected = {
        0x140,
        0x240,
        0x241,
        0x340,
        0x341,
        0x342,
        0x440,
        DIAGNOSTIC_REQUEST_ID,
        DIAGNOSTIC_RESPONSE_ID,
    }
    if identifiers != expected:
        raise ProbeError("MAP_CONTRACT", "message map does not contain the nine frozen IDs")
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
    elif name == "diagnostic_ping_request":
        if values["opcode"] != DIAGNOSTIC_OPCODE_PING:
            raise ProbeError("OUT_OF_RANGE", "diagnostic request opcode is unsupported")
        if values["reserved"] != 0:
            raise ProbeError("RESERVED_BITS", "diagnostic request reserved byte is nonzero")
    elif name == "diagnostic_ping_response":
        if values["status"] != DIAGNOSTIC_STATUS_OK:
            raise ProbeError("OUT_OF_RANGE", "diagnostic response status is not OK")
        if values["opcode_echo"] != DIAGNOSTIC_OPCODE_PING:
            raise ProbeError("OUT_OF_RANGE", "diagnostic response opcode is unsupported")

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

    diagnostic_request = build_diagnostic_request(0x2A, 0x12345678)
    if diagnostic_request.data != bytes.fromhex("01 2A 01 00 78 56 34 12"):
        raise ProbeError("SELF_TEST", "diagnostic request vector mismatch")
    diagnostic_response = CanFrame(
        DIAGNOSTIC_RESPONSE_ID,
        bytes.fromhex("01 2A 00 01 78 56 34 12"),
    )
    decoded_response = decode_frame(diagnostic_response, message_map)
    if decoded_response["fields"]["nonce"] != 0x12345678:
        raise ProbeError("SELF_TEST", "diagnostic response decode mismatch")

    class FakeClock:
        def __init__(self) -> None:
            self.now = 0.0

        def monotonic(self) -> float:
            return self.now

        def advance(self, duration_s: float) -> None:
            self.now += duration_s

    class FakeCanSocket:
        def __init__(self, clock: FakeClock) -> None:
            self.clock = clock
            self.timeout_s = 0.0
            self.pending: list[bytes] = []
            self.sent: list[CanFrame] = []
            self.error_filter: bytes | None = None
            self.closed = False

        def setsockopt(self, level: int, option: int, value: bytes) -> None:
            if level == SOL_CAN_RAW and option == CAN_RAW_ERR_FILTER:
                self.error_filter = value

        def settimeout(self, timeout_s: float) -> None:
            self.timeout_s = timeout_s

        def send(self, raw: bytes) -> int:
            request = unpack_socketcan(raw)
            self.sent.append(request)
            response = CanFrame(
                DIAGNOSTIC_RESPONSE_ID,
                bytes(
                    [
                        SCHEMA_REVISION,
                        request.data[1],
                        DIAGNOSTIC_STATUS_OK,
                        DIAGNOSTIC_OPCODE_PING,
                    ]
                )
                + request.data[4:8],
            )
            self.pending.append(pack_socketcan(response))
            return len(raw)

        def recv(self, _size: int) -> bytes:
            if self.pending:
                return self.pending.pop(0)
            self.clock.advance(self.timeout_s)
            raise TimeoutError

        def close(self) -> None:
            self.closed = True

    fake_clock = FakeClock()
    fake_socket = FakeCanSocket(fake_clock)
    socket_open_count = 0

    def fake_socket_factory(_interface: str) -> FakeCanSocket:
        nonlocal socket_open_count
        socket_open_count += 1
        return fake_socket

    series_result, _series_frames = run_diagnostic_series(
        "fake0",
        0x20,
        0x10203040,
        0.2,
        3,
        1.0,
        0.5,
        message_map,
        socket_factory=fake_socket_factory,
        monotonic=fake_clock.monotonic,
    )
    if (
        series_result["status"] != "PASS_HARDWARE_ROUND_TRIP_SERIES_CANDIDATE"
        or socket_open_count != 1
        or len(fake_socket.sent) != 3
        or series_result["matching_responses"] != 3
        or series_result["actual_duration_s"] < 3.5
        or fake_socket.error_filter != struct.pack("=I", CAN_ERR_MASK)
        or not fake_socket.closed
    ):
        raise ProbeError("SELF_TEST", "persistent diagnostic series mismatch")
    if [frame.data[1] for frame in fake_socket.sent] != [0x20, 0x21, 0x22]:
        raise ProbeError("SELF_TEST", "diagnostic series sequence mismatch")

    return {
        "mode": "self-test",
        "status": "PASS_HOST",
        "interface": "NOT_OPENED",
        "known_good_vectors": 7,
        "mutants_rejected": len(expected),
        "duplicate_policy": "COUNT_NOT_FAIL",
        "bme_pair_policy": "SAME_SEQUENCE_ONLY",
        "hardware": "NOT_RUN",
        "diagnostic_ping_vector": "PASS",
        "diagnostic_series_vector": "PASS_SINGLE_SOCKET",
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


def build_diagnostic_request(sequence: int, nonce: int) -> CanFrame:
    payload = bytes(
        [SCHEMA_REVISION, sequence, DIAGNOSTIC_OPCODE_PING, 0]
    ) + nonce.to_bytes(4, "little")
    return CanFrame(DIAGNOSTIC_REQUEST_ID, payload)


def run_diagnostic_ping(
    interface: str,
    sequence: int,
    nonce: int,
    response_timeout_s: float,
    message_map: dict[str, Any],
) -> tuple[dict[str, Any], list[CanFrame]]:
    request = build_diagnostic_request(sequence, nonce)
    can_socket = open_can_socket(interface)
    captured: list[CanFrame] = []
    try:
        sent = can_socket.send(pack_socketcan(request))
        if sent != CAN_FRAME.size:
            raise ProbeError("SOCKET_SEND", f"sent {sent} bytes, expected 16")
        captured = receive_bounded(can_socket, response_timeout_s)
    except OSError as exc:
        raise ProbeError("SOCKET_IO", str(exc)) from exc
    finally:
        can_socket.close()

    matching = 0
    mismatched = 0
    for frame in captured:
        if frame.identifier != DIAGNOSTIC_RESPONSE_ID:
            continue
        try:
            decoded = decode_frame(frame, message_map)
        except ProbeError:
            mismatched += 1
            continue
        fields = decoded["fields"]
        if (
            fields["sequence"] == sequence
            and fields["status"] == DIAGNOSTIC_STATUS_OK
            and fields["opcode_echo"] == DIAGNOSTIC_OPCODE_PING
            and fields["nonce"] == nonce
        ):
            matching += 1
        else:
            mismatched += 1

    if matching > 1:
        status = "FAIL_DUPLICATE"
    elif mismatched > 0:
        status = "FAIL_MISMATCH"
    elif matching == 0:
        status = "FAIL_TIMEOUT"
    else:
        status = "PASS_HARDWARE_ROUND_TRIP_CANDIDATE"
    return {
        "mode": "diagnostic-ping",
        "status": status,
        "interface": interface,
        "request_id": "0x540",
        "response_id": "0x541",
        "sequence": sequence,
        "nonce": f"0x{nonce:08X}",
        "request_frames_sent": 1,
        "matching_responses": matching,
        "mismatched_responses": mismatched,
        "captured_frames": len(captured),
        "response_timeout_s": response_timeout_s,
    }, captured


def diagnostic_response_matches(
    frame: CanFrame,
    sequence: int,
    nonce: int,
    message_map: dict[str, Any],
) -> bool:
    if frame.identifier != DIAGNOSTIC_RESPONSE_ID:
        return False
    try:
        decoded = decode_frame(frame, message_map)
    except ProbeError:
        return False
    fields = decoded["fields"]
    return (
        fields["sequence"] == sequence
        and fields["status"] == DIAGNOSTIC_STATUS_OK
        and fields["opcode_echo"] == DIAGNOSTIC_OPCODE_PING
        and fields["nonce"] == nonce
    )


def run_diagnostic_series(
    interface: str,
    sequence: int,
    nonce: int,
    response_timeout_s: float,
    count: int,
    interval_s: float,
    hold_after_s: float,
    message_map: dict[str, Any],
    *,
    socket_factory: Any = open_can_socket,
    monotonic: Any = time.monotonic,
) -> tuple[dict[str, Any], list[CanFrame]]:
    can_socket = socket_factory(interface)
    can_socket.setsockopt(
        SOL_CAN_RAW,
        CAN_RAW_ERR_FILTER,
        struct.pack("=I", CAN_ERR_MASK),
    )
    captured: list[CanFrame] = []
    captured_discarded = 0
    error_frames = 0
    unexpected_responses = 0
    exchanges: list[dict[str, Any]] = []
    session_started = monotonic()

    def receive_until(
        deadline: float,
        expected_sequence: int | None = None,
        expected_nonce: int | None = None,
    ) -> tuple[int, int]:
        nonlocal captured_discarded, error_frames, unexpected_responses
        matching = 0
        mismatched = 0
        while True:
            remaining = deadline - monotonic()
            if remaining <= 0.0:
                break
            can_socket.settimeout(min(0.2, remaining))
            try:
                frame = unpack_socketcan(can_socket.recv(CAN_FRAME.size))
            except TimeoutError:
                continue
            except OSError as exc:
                raise ProbeError("SOCKET_IO", str(exc)) from exc
            if len(captured) < MAX_CAPTURED_FRAMES:
                captured.append(frame)
            else:
                captured_discarded += 1
            if frame.error:
                error_frames += 1
                continue
            if frame.identifier != DIAGNOSTIC_RESPONSE_ID:
                continue
            if expected_sequence is None or expected_nonce is None:
                unexpected_responses += 1
            elif diagnostic_response_matches(
                frame,
                expected_sequence,
                expected_nonce,
                message_map,
            ):
                matching += 1
            else:
                mismatched += 1
        return matching, mismatched

    try:
        for index in range(count):
            scheduled_at = session_started + index * interval_s
            receive_until(scheduled_at)
            request_sequence = (sequence + index) & 0xFF
            request_nonce = (nonce + index) & 0xFFFFFFFF
            request = build_diagnostic_request(request_sequence, request_nonce)
            sent = can_socket.send(pack_socketcan(request))
            if sent != CAN_FRAME.size:
                raise ProbeError("SOCKET_SEND", f"sent {sent} bytes, expected 16")
            matching, mismatched = receive_until(
                scheduled_at + response_timeout_s,
                request_sequence,
                request_nonce,
            )
            if matching > 1:
                exchange_status = "FAIL_DUPLICATE"
            elif mismatched > 0:
                exchange_status = "FAIL_MISMATCH"
            elif matching == 0:
                exchange_status = "FAIL_TIMEOUT"
            else:
                exchange_status = "PASS"
            exchanges.append(
                {
                    "index": index + 1,
                    "sequence": request_sequence,
                    "nonce": f"0x{request_nonce:08X}",
                    "matching_responses": matching,
                    "mismatched_responses": mismatched,
                    "status": exchange_status,
                }
            )
        receive_until(session_started + count * interval_s + hold_after_s)
    except OSError as exc:
        raise ProbeError("SOCKET_IO", str(exc)) from exc
    finally:
        can_socket.close()

    failures = [item for item in exchanges if item["status"] != "PASS"]
    if error_frames > 0:
        status = "FAIL_ERROR_FRAME"
    elif unexpected_responses > 0:
        status = "FAIL_UNEXPECTED_RESPONSE"
    elif failures:
        status = failures[0]["status"]
    else:
        status = "PASS_HARDWARE_ROUND_TRIP_SERIES_CANDIDATE"
    return {
        "mode": "diagnostic-series",
        "status": status,
        "interface": interface,
        "request_id": "0x540",
        "response_id": "0x541",
        "socket_open_count": 1,
        "request_frames_sent": count,
        "matching_responses": sum(item["matching_responses"] for item in exchanges),
        "mismatched_responses": sum(
            item["mismatched_responses"] for item in exchanges
        ),
        "unexpected_responses": unexpected_responses,
        "error_frames": error_frames,
        "active_duration_s": count * interval_s,
        "hold_after_s": hold_after_s,
        "socket_planned_duration_s": count * interval_s + hold_after_s,
        "actual_duration_s": monotonic() - session_started,
        "captured_frames_bounded": len(captured),
        "captured_frames_discarded": captured_discarded,
        "exchanges": exchanges,
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
    mode.add_argument("--diagnostic-ping", action="store_true")
    mode.add_argument("--diagnostic-series", action="store_true")
    parser.add_argument("--interface")
    parser.add_argument("--observe-seconds", type=float, default=10.0)
    parser.add_argument("--response-timeout", type=float, default=2.0)
    parser.add_argument("--sequence", type=lambda value: int(value, 0), default=0x2A)
    parser.add_argument("--nonce", type=lambda value: int(value, 0), default=0x12345678)
    parser.add_argument("--diagnostic-count", type=int, default=5)
    parser.add_argument("--diagnostic-interval", type=float, default=60.0)
    parser.add_argument("--diagnostic-hold-after", type=float, default=0.0)
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
    if not 0.1 <= args.response_timeout <= 5.0:
        raise ProbeError("ARGUMENT", "--response-timeout must be in [0.1, 5]")
    if not 0 <= args.sequence <= 0xFF:
        raise ProbeError("ARGUMENT", "--sequence must be in [0, 255]")
    if not 0 <= args.nonce <= 0xFFFFFFFF:
        raise ProbeError("ARGUMENT", "--nonce must be in [0, 0xffffffff]")
    if not 1 <= args.diagnostic_count <= 16:
        raise ProbeError("ARGUMENT", "--diagnostic-count must be in [1, 16]")
    if not 1.0 <= args.diagnostic_interval <= 300.0:
        raise ProbeError("ARGUMENT", "--diagnostic-interval must be in [1, 300]")
    if not 0.0 <= args.diagnostic_hold_after <= 300.0:
        raise ProbeError("ARGUMENT", "--diagnostic-hold-after must be in [0, 300]")
    if args.diagnostic_series and args.response_timeout >= args.diagnostic_interval:
        raise ProbeError(
            "ARGUMENT",
            "--response-timeout must be shorter than --diagnostic-interval",
        )
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
                "use the fixed 0x540/0x541 diagnostic pair for one bounded round trip",
                "use one persistent SocketCAN socket for repeated timed round trips",
                "keep only bounded troubleshooting output",
            ],
        }, []
    if not args.interface:
        raise ProbeError("ARGUMENT", "interface mode requires --interface")
    if args.vcan_self_test:
        return run_vcan_test(args.interface, args.allow_send, message_map)
    if args.diagnostic_ping:
        return run_diagnostic_ping(
            args.interface,
            args.sequence,
            args.nonce,
            args.response_timeout,
            message_map,
        )
    if args.diagnostic_series:
        return run_diagnostic_series(
            args.interface,
            args.sequence,
            args.nonce,
            args.response_timeout,
            args.diagnostic_count,
            args.diagnostic_interval,
            args.diagnostic_hold_after,
            message_map,
        )
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
