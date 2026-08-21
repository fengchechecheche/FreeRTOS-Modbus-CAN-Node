#!/usr/bin/env python3
"""Low-intrusion Raspberry Pi collector for the P5 hardware soak."""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import math
import os
import pathlib
import select
import signal
import socket
import struct
import sys
import termios
import threading
import time
from typing import Any

SCHEMA_REVISION = 1
REGISTER_COUNT = 122
DEVICE_SIGNATURE = 20533
TASK_NAMES = ("protocol", "acquisition", "can", "health", "diagnostic")
SENSOR_NAMES = ("bme280", "veml7700", "adxl345_sample", "adxl345_feature")
CAN_IDS = (0x240, 0x241, 0x340, 0x341, 0x342, 0x440)
MAX_VCP_LINE_BYTES = 4096
MAX_EVIDENCE_BYTES = 64 * 1024 * 1024
CAN_ERR_FLAG = 0x20000000
CAN_ERR_BUSOFF = 0x00000040
CAN_SFF_MASK = 0x000007FF
SOL_CAN_RAW = getattr(socket, "SOL_CAN_RAW", 101)
CAN_RAW_FILTER = 1
CAN_RAW_ERR_FILTER = 2


class CollectorError(RuntimeError):
    """Raised for a bounded collector failure."""


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if (crc & 1) else crc >> 1
    return crc & 0xFFFF


def append_crc(payload: bytes) -> bytes:
    value = crc16(payload)
    return payload + bytes((value & 0xFF, value >> 8))


def parse_p5diag(line: str) -> dict[str, Any]:
    if not line.startswith("P5DIAG1 "):
        raise CollectorError("not a P5DIAG1 record")
    fields: dict[str, str] = {}
    for token in line.strip().split()[1:]:
        if "=" not in token:
            raise CollectorError("malformed P5DIAG1 token")
        key, value = token.split("=", 1)
        fields[key] = value
    required = {"v", "t", "boot", "tr", "tm", "td", "tb", "tc", "ts", "sm", "q", "h", "rst", "rs", "can", "sns"}
    if set(fields) != required or fields["v"] != "1":
        raise CollectorError("unsupported or incomplete P5DIAG1 record")

    def numbers(key: str, separator: str = "/", base: int = 10) -> list[int]:
        try:
            return [int(item, base) for item in fields[key].split(separator)]
        except ValueError as error:
            raise CollectorError(f"invalid P5DIAG1 field {key}") from error

    task_values = {key: numbers(key) for key in ("tr", "tm", "td", "tb", "tc", "ts")}
    if any(len(values) != 5 for values in task_values.values()):
        raise CollectorError("task vectors must contain five values")
    measured_mask = int(fields["sm"], 16)
    queue = numbers("q")
    health = fields["h"].split("/")
    reset = fields["rst"].split("/")
    rs485 = numbers("rs")
    can = numbers("can")
    sensor_groups = fields["sns"].split(";")
    if len(queue) != 5 or len(health) != 5 or len(reset) != 3 or len(rs485) != 2 or len(can) != 8 or len(sensor_groups) != 4:
        raise CollectorError("P5DIAG1 vector length mismatch")
    sensors = []
    for group in sensor_groups:
        values = [int(item) for item in group.split(",")]
        if len(values) != 4:
            raise CollectorError("sensor vector must contain four values")
        sensors.append(values)
    tasks = {}
    for index, name in enumerate(TASK_NAMES):
        tasks[name] = {
            "release": task_values["tr"][index],
            "missed": task_values["tm"][index],
            "deadline_miss": task_values["td"][index],
            "budget_overrun": task_values["tb"][index],
            "configured_words": task_values["tc"][index],
            "minimum_free_words": task_values["ts"][index],
            "measured": bool(measured_mask & (1 << index)),
        }
    state_tokens = ("INVALID", "FRESH", "STALE", "OFFLINE")
    parsed_sensors = {}
    for name, values in zip(SENSOR_NAMES, sensors):
        parsed_sensors[name] = {
            "state": state_tokens[values[0]] if values[0] < len(state_tokens) else f"UNKNOWN_{values[0]}",
            "sequence": values[1],
            "fault_count": values[2],
            "recovery_count": values[3],
        }
    return {
        "monotonic_ms": int(fields["t"]),
        "boot_count": int(fields["boot"]),
        "tasks": tasks,
        "queue": {"current": queue[0], "maximum": queue[1], "depth": queue[2], "dropped": queue[3], "drained": queue[4]},
        "health": {"state": health[0], "warning_mask": int(health[1], 16), "stalled_mask": int(health[2], 16), "feed": health[3], "fault_code": int(health[4])},
        "reset": {"primary": reset[0], "raw_flags": int(reset[1], 16), "loop": bool(int(reset[2]))},
        "rs485": {"accepted": rs485[0], "error_count": rs485[1]},
        "can": {"state": str(can[0]), "pending": can[1], "capacity": can[2], "maximum_pending": can[3], "event_dropped": can[4], "hal_busy": can[5], "bus_off": can[6], "recovery_attempts": can[7]},
        "sensors": parsed_sensors,
    }


def load_register_map(path: pathlib.Path) -> list[dict[str, Any]]:
    payload = json.loads(path.read_text(encoding="utf-8"))
    entries = payload["input_registers"]["entries"]
    if payload["atomicity"]["input_region_count"] != REGISTER_COUNT:
        raise CollectorError("register map is not the frozen 122-register image")
    return entries


def decode_registers(words: list[int], entries: list[dict[str, Any]]) -> dict[str, int]:
    if len(words) != REGISTER_COUNT:
        raise CollectorError("Modbus image must contain 122 registers")
    decoded: dict[str, int] = {}
    for entry in entries:
        address = int(entry["address"])
        kind = entry["type"]
        if kind == "uint16":
            value = words[address]
        elif kind in {"uint32", "int32"}:
            value = (words[address] << 16) | words[address + 1]
            if kind == "int32" and value & 0x80000000:
                value -= 1 << 32
        else:
            raise CollectorError(f"unsupported register type: {kind}")
        decoded[entry["name"]] = value
    return decoded


def parse_modbus_response(response: bytes) -> list[int]:
    if len(response) != 249 or response[:3] != bytes((4, 4, 244)):
        raise CollectorError("invalid full input-register response header or length")
    if crc16(response[:-2]) != int.from_bytes(response[-2:], "little"):
        raise CollectorError("Modbus response CRC mismatch")
    return list(struct.unpack(">" + "H" * REGISTER_COUNT, response[3:-2]))


class SerialPort:
    def __init__(self, path: str, baud: int, parity_even: bool) -> None:
        self.path = path
        self.fd = os.open(path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = termios.IGNPAR
        attrs[1] = 0
        attrs[2] = termios.CLOCAL | termios.CREAD | termios.CS8
        if parity_even:
            attrs[2] |= termios.PARENB
        attrs[3] = 0
        speed = termios.B115200 if baud == 115200 else termios.B19200
        attrs[4] = speed
        attrs[5] = speed
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)

    def close(self) -> None:
        if self.fd >= 0:
            os.close(self.fd)
            self.fd = -1


def modbus_snapshot(port: SerialPort, timeout_s: float = 1.0) -> tuple[bytes, list[int]]:
    request = append_crc(bytes((4, 4, 0, 0, 0, REGISTER_COUNT)))
    termios.tcflush(port.fd, termios.TCIFLUSH)
    os.write(port.fd, request)
    deadline = time.monotonic() + timeout_s
    response = bytearray()
    while len(response) < 249 and time.monotonic() < deadline:
        readable, _, _ = select.select([port.fd], [], [], max(0.0, deadline - time.monotonic()))
        if readable:
            response.extend(os.read(port.fd, 249 - len(response)))
    raw = bytes(response)
    return raw, parse_modbus_response(raw)


class EvidenceWriter:
    def __init__(self, path: pathlib.Path, limit: int = MAX_EVIDENCE_BYTES) -> None:
        self.path = path
        self.limit = limit
        self.size = 0
        self.stream = path.open("w", encoding="utf-8", newline="")
        self.lock = threading.Lock()

    def line(self, value: str) -> None:
        encoded = (value.rstrip("\r\n") + "\n").encode("utf-8")
        with self.lock:
            if self.size + len(encoded) > self.limit:
                raise CollectorError(f"evidence limit exceeded: {self.path.name}")
            self.stream.write(encoded.decode("utf-8"))
            self.stream.flush()
            self.size += len(encoded)

    def close(self) -> None:
        self.stream.close()


class CanMonitor(threading.Thread):
    def __init__(self, interface: str, frames: EvidenceWriter, errors: EvidenceWriter, stop: threading.Event) -> None:
        super().__init__(daemon=True)
        self.interface = interface
        self.frames = frames
        self.errors = errors
        self.stop = stop
        self.counts = {f"{can_id:03X}": 0 for can_id in CAN_IDS}
        self.error_frames = 0
        self.bus_off_frames = 0
        self.failure: str | None = None
        self.lock = threading.Lock()

    def snapshot(self) -> dict[str, Any]:
        with self.lock:
            return {"per_id": dict(self.counts), "error_frames": self.error_frames, "bus_off_frames": self.bus_off_frames}

    def run(self) -> None:
        try:
            can_socket = socket.socket(socket.PF_CAN, socket.SOCK_RAW, socket.CAN_RAW)
            filters = b"".join(struct.pack("=II", value, CAN_SFF_MASK) for value in CAN_IDS)
            can_socket.setsockopt(SOL_CAN_RAW, CAN_RAW_FILTER, filters)
            can_socket.setsockopt(SOL_CAN_RAW, CAN_RAW_ERR_FILTER, struct.pack("=I", 0x1FFFFFFF))
            can_socket.settimeout(0.25)
            can_socket.bind((self.interface,))
            while not self.stop.is_set():
                try:
                    frame = can_socket.recv(16)
                except socket.timeout:
                    continue
                can_id, dlc, data = struct.unpack("=IB3x8s", frame)
                now_ns = time.time_ns()
                if can_id & CAN_ERR_FLAG:
                    payload = {"utc_ns": now_ns, "can_id": f"{can_id:08X}", "data": data[:dlc].hex().upper()}
                    self.errors.line(json.dumps(payload, separators=(",", ":")))
                    with self.lock:
                        self.error_frames += 1
                        if can_id & CAN_ERR_BUSOFF:
                            self.bus_off_frames += 1
                else:
                    sid = can_id & CAN_SFF_MASK
                    key = f"{sid:03X}"
                    payload = {"utc_ns": now_ns, "id": key, "dlc": dlc, "data": data[:dlc].hex().upper()}
                    self.frames.line(json.dumps(payload, separators=(",", ":")))
                    if key in self.counts:
                        with self.lock:
                            self.counts[key] += 1
            can_socket.close()
        except Exception as error:  # bounded handoff to main thread
            self.failure = str(error)
            self.stop.set()


def host_metrics() -> dict[str, Any]:
    load1, load5, load15 = os.getloadavg()
    available = 0
    for line in pathlib.Path("/proc/meminfo").read_text(encoding="utf-8").splitlines():
        if line.startswith("MemAvailable:"):
            available = int(line.split()[1])
            break
    return {"load1": load1, "load5": load5, "load15": load15, "mem_available_kib": available}


def sensor_projection(decoded: dict[str, int], index: int, elapsed_ms: int) -> dict[str, Any]:
    names = (
        "bme280_temperature", "bme280_pressure", "bme280_humidity",
        "veml7700_illuminance", "adxl345_acceleration_x",
        "adxl345_acceleration_y", "adxl345_acceleration_z",
        "adxl345_rms_x", "adxl345_rms_y", "adxl345_rms_z",
        "adxl345_peak_x", "adxl345_peak_y", "adxl345_peak_z",
        "adxl345_resultant_rms",
    )
    return {"sample_index": index, "elapsed_ms": elapsed_ms, **{name: decoded[name] for name in names}}


def finalize_evidence(raw_dir: pathlib.Path, manifest: dict[str, Any]) -> None:
    manifest_path = raw_dir / "session_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    lines = []
    for path in sorted(raw_dir.iterdir()):
        if path.name == "SHA256SUMS.txt" or not path.is_file():
            continue
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"{digest}  {path.name}")
    (raw_dir / "SHA256SUMS.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")


def run(args: argparse.Namespace) -> int:
    raw_dir = args.raw_evidence_dir.resolve()
    raw_dir.mkdir(parents=True, exist_ok=True)
    if any(raw_dir.iterdir()):
        raise CollectorError("raw evidence directory must be empty")
    entries = load_register_map(args.register_map)
    stop = threading.Event()
    signal.signal(signal.SIGTERM, lambda *_: stop.set())
    signal.signal(signal.SIGINT, lambda *_: stop.set())
    vcp_writer = EvidenceWriter(raw_dir / "vcp_raw.log")
    modbus_writer = EvidenceWriter(raw_dir / "modbus_snapshots.jsonl")
    sensor_writer = EvidenceWriter(raw_dir / "sensor_timeseries.jsonl")
    can_writer = EvidenceWriter(raw_dir / "can_frames.jsonl")
    can_error_writer = EvidenceWriter(raw_dir / "can_errors.jsonl")
    host_writer = EvidenceWriter(raw_dir / "host_metrics.jsonl")
    event_writer = EvidenceWriter(raw_dir / "events.jsonl")
    writers = (vcp_writer, modbus_writer, sensor_writer, can_writer, can_error_writer, host_writer, event_writer)
    csv_path = raw_dir / "sensor_timeseries.csv"
    csv_stream = csv_path.open("w", encoding="utf-8", newline="")
    csv_writer: csv.DictWriter[str] | None = None
    vcp = SerialPort(args.vcp, 115200, False)
    rs485 = SerialPort(args.rs485, 19200, True)
    can_monitor = CanMonitor(args.can_interface, can_writer, can_error_writer, stop)
    can_monitor.start()
    started = time.monotonic()
    needed = math.ceil(args.duration_seconds / args.sample_seconds)
    index = 0
    buffer = bytearray()
    modbus_errors = 0
    manifest = {"schema_revision": 1, "session_id": args.session_id, "collector": "p5-hil-v1", "completed": False}
    try:
        while not stop.is_set() and index < needed:
            readable, _, _ = select.select([vcp.fd], [], [], 0.25)
            if not readable:
                if can_monitor.failure:
                    raise CollectorError(f"CAN monitor failed: {can_monitor.failure}")
                if time.monotonic() - started > args.duration_seconds + args.sample_seconds:
                    raise CollectorError("P5DIAG1 sample deadline exceeded")
                continue
            chunk = os.read(vcp.fd, 4096)
            buffer.extend(chunk)
            if len(buffer) > MAX_VCP_LINE_BYTES * 2:
                raise CollectorError("VCP input exceeded bounded line buffer")
            while b"\n" in buffer:
                raw, _, remainder = buffer.partition(b"\n")
                buffer = bytearray(remainder)
                line = raw.rstrip(b"\r").decode("utf-8", errors="replace")
                vcp_writer.line(line)
                if not line.startswith("P5DIAG1 "):
                    continue
                diagnostic = parse_p5diag(line)
                try:
                    response, words = modbus_snapshot(rs485)
                    decoded = decode_registers(words, entries)
                    snapshot_ok = decoded.get("device_signature") == DEVICE_SIGNATURE
                    if not snapshot_ok:
                        raise CollectorError("Modbus device signature mismatch")
                    modbus_writer.line(json.dumps({"sample_index": index, "request": "04040000007A71BC", "response": response.hex().upper(), "registers": words, "decoded": decoded}, separators=(",", ":")))
                except CollectorError as error:
                    modbus_errors += 1
                    event_writer.line(json.dumps({"sample_index": index, "event": "modbus_error", "detail": str(error)}, separators=(",", ":")))
                    raise
                elapsed_ms = index * args.sample_seconds * 1000
                sensors = sensor_projection(decoded, index, elapsed_ms)
                sensor_writer.line(json.dumps(sensors, separators=(",", ":")))
                if csv_writer is None:
                    csv_writer = csv.DictWriter(csv_stream, fieldnames=list(sensors))
                    csv_writer.writeheader()
                csv_writer.writerow(sensors)
                csv_stream.flush()
                host = host_metrics()
                host_writer.line(json.dumps({"sample_index": index, **host}, separators=(",", ":")))
                can_state = can_monitor.snapshot()
                sample = {
                    "kind": "sample", "schema_revision": SCHEMA_REVISION,
                    "session_id": args.session_id, "sample_index": index,
                    "elapsed_ms": elapsed_ms, "monotonic_ms": diagnostic["monotonic_ms"],
                    "boot_id": f"boot-{diagnostic['boot_count']}",
                    "tasks": diagnostic["tasks"], "queue": diagnostic["queue"],
                    "health": diagnostic["health"], "reset": diagnostic["reset"],
                    "rs485": diagnostic["rs485"], "can": diagnostic["can"],
                    "sensors": diagnostic["sensors"],
                    "collector": {"modbus_snapshot_ok": True, "modbus_register_count": len(words), "modbus_signature": decoded["device_signature"], "modbus_error_count": modbus_errors, "can": can_state, "host": host},
                }
                print(json.dumps(sample, separators=(",", ":")), flush=True)
                index += 1
                if index >= needed:
                    break
        if index < needed:
            raise CollectorError(f"collector stopped after {index}/{needed} samples")
        manifest.update({"completed": True, "sample_count": index, "can": can_monitor.snapshot(), "modbus_error_count": modbus_errors})
        return 0
    finally:
        stop.set()
        can_monitor.join(2.0)
        vcp.close()
        rs485.close()
        csv_stream.close()
        for writer in writers:
            writer.close()
        finalize_evidence(raw_dir, manifest)


def self_test(register_map: pathlib.Path) -> int:
    if CAN_IDS != (0x240, 0x241, 0x340, 0x341, 0x342, 0x440):
        raise AssertionError("periodic CAN capture set drifted")
    line = (
        "P5DIAG1 v=1 t=60000 boot=2 tr=1/2/3/4/5 tm=0/0/0/0/0 "
        "td=0/0/0/0/0 tb=0/0/0/0/0 tc=256/256/256/256/256 "
        "ts=128/127/126/125/124 sm=1F q=0/2/8/0/10 h=1/00000000/00000000/1/0 "
        "rst=1/00000001/0 rs=10/0 can=2/0/14/2/0/0/0/0 "
        "sns=1,10,0,0;1,5,0,0;1,500,0,0;1,20,0,0"
    )
    parsed = parse_p5diag(line)
    if parsed["tasks"]["diagnostic"]["release"] != 5 or not parsed["tasks"]["health"]["measured"]:
        raise AssertionError("P5DIAG1 task parse failed")
    entries = load_register_map(register_map)
    words = [0] * REGISTER_COUNT
    words[0] = DEVICE_SIGNATURE
    words[18:20] = [0xFFFF, 0xFF9C]
    decoded = decode_registers(words, entries)
    if decoded["device_signature"] != DEVICE_SIGNATURE or decoded["bme280_temperature"] != -100:
        raise AssertionError("register decode failed")
    payload = bytes((4, 4, 244)) + struct.pack(">" + "H" * REGISTER_COUNT, *words)
    if parse_modbus_response(append_crc(payload)) != words:
        raise AssertionError("Modbus response parse failed")
    try:
        parse_p5diag(line.replace(" sm=1F", ""))
        raise AssertionError("missing P5DIAG1 field accepted")
    except CollectorError:
        pass
    print("P5 SOAK HIL COLLECTOR SELF-TEST: PASS (4 bounded checks)")
    return 0


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(description=__doc__)
    result.add_argument("--self-test", action="store_true")
    result.add_argument("--session-id")
    result.add_argument("--duration-seconds", type=int, default=600)
    result.add_argument("--sample-seconds", type=int, default=60)
    result.add_argument("--vcp")
    result.add_argument("--rs485")
    result.add_argument("--can-interface", default="can0")
    result.add_argument("--raw-evidence-dir", type=pathlib.Path)
    result.add_argument("--register-map", type=pathlib.Path, default=pathlib.Path("protocol/register_map.json"))
    return result


def main() -> int:
    args = parser().parse_args()
    try:
        if args.self_test:
            return self_test(args.register_map)
        if not args.session_id or not args.vcp or not args.rs485 or args.raw_evidence_dir is None:
            raise CollectorError("runtime requires session-id, VCP, RS485 and raw evidence dir")
        if args.duration_seconds < 60 or args.duration_seconds > 8 * 60 * 60 or args.sample_seconds != 60:
            raise CollectorError("duration must be 60..28800 seconds and sample period must be 60 seconds")
        return run(args)
    except (CollectorError, OSError, ValueError, json.JSONDecodeError) as error:
        print(f"P5 SOAK HIL COLLECTOR: FAIL: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
