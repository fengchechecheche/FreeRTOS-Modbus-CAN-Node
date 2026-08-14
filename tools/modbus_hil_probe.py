#!/usr/bin/env python3
"""Bounded Modbus RTU bench probe for the Project Five address-4 node.

Self-test and dry-run use only the Python standard library and never open a
serial port. Real serial access is explicit and imports pyserial lazily.
"""

from __future__ import annotations

import argparse
import sys
import time
from dataclasses import dataclass
from typing import Iterable, Optional, Sequence

DEFAULT_SLAVE_ADDRESS = 4
MIGRATION_SLAVE_ADDRESS = 5
BAUD_RATE = 19_200
FRAME_GAP_SECONDS = 0.005

READ_HOLDING_REGISTERS = 0x03
READ_INPUT_REGISTERS = 0x04
WRITE_SINGLE_REGISTER = 0x06

ILLEGAL_FUNCTION = 0x01
ILLEGAL_DATA_ADDRESS = 0x02
ILLEGAL_DATA_VALUE = 0x03


class ProbeError(RuntimeError):
    """A bounded probe or response-validation failure."""


def crc16_modbus(data: bytes) -> int:
    crc = 0xFFFF
    for value in data:
        crc ^= value
        for _ in range(8):
            if crc & 1:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc & 0xFFFF


def append_crc(payload: bytes) -> bytes:
    crc = crc16_modbus(payload)
    return payload + bytes((crc & 0xFF, crc >> 8))


def has_valid_crc(frame: bytes) -> bool:
    return len(frame) >= 4 and crc16_modbus(frame[:-2]) == int.from_bytes(
        frame[-2:], "little"
    )


def build_request(slave: int, function: int, data: bytes) -> bytes:
    if not 0 <= slave <= 247:
        raise ProbeError(f"slave address out of range: {slave}")
    if not 0 <= function <= 0x7F:
        raise ProbeError(f"request function out of range: {function}")
    return append_crc(bytes((slave, function)) + data)


def build_read_request(slave: int, function: int, start: int, quantity: int) -> bytes:
    if function not in (READ_HOLDING_REGISTERS, READ_INPUT_REGISTERS):
        raise ProbeError(f"unsupported read function: 0x{function:02X}")
    if not 0 <= start <= 0xFFFF or not 0 <= quantity <= 0xFFFF:
        raise ProbeError("read start/quantity must fit uint16")
    return build_request(
        slave,
        function,
        start.to_bytes(2, "big") + quantity.to_bytes(2, "big"),
    )


def build_write_request(slave: int, register: int, value: int) -> bytes:
    if not 0 <= register <= 0xFFFF or not 0 <= value <= 0xFFFF:
        raise ProbeError("write register/value must fit uint16")
    return build_request(
        slave,
        WRITE_SINGLE_REGISTER,
        register.to_bytes(2, "big") + value.to_bytes(2, "big"),
    )


def corrupt_crc(frame: bytes) -> bytes:
    if len(frame) < 2:
        raise ProbeError("cannot corrupt a frame without CRC bytes")
    return frame[:-1] + bytes((frame[-1] ^ 0x01,))


def hex_bytes(data: bytes) -> str:
    return data.hex(" ").upper() if data else "<silence>"


@dataclass(frozen=True)
class ProbeCase:
    case_id: str
    description: str
    request: bytes
    expected_kind: str
    expected_address: int
    expected_function: int
    expected_byte_count: Optional[int] = None
    expected_exception: Optional[int] = None
    expected_echo: Optional[bytes] = None

    def expected_length(self) -> int:
        if self.expected_kind == "silence":
            return 1
        if self.expected_kind == "exception":
            return 5
        if self.expected_echo is not None:
            return len(self.expected_echo)
        if self.expected_byte_count is None:
            raise ProbeError(f"{self.case_id}: missing expected byte count")
        return 3 + self.expected_byte_count + 2


def read_only_cases() -> list[ProbeCase]:
    h01 = build_read_request(DEFAULT_SLAVE_ADDRESS, READ_HOLDING_REGISTERS, 0, 4)
    h02 = build_read_request(DEFAULT_SLAVE_ADDRESS, READ_INPUT_REGISTERS, 0, 18)
    h03 = build_read_request(DEFAULT_SLAVE_ADDRESS, READ_INPUT_REGISTERS, 0, 122)
    return [
        ProbeCase(
            "H01",
            "read holding registers 0..3",
            h01,
            "normal",
            DEFAULT_SLAVE_ADDRESS,
            READ_HOLDING_REGISTERS,
            expected_byte_count=8,
        ),
        ProbeCase(
            "H02",
            "read input identity/generation/masks 0..17",
            h02,
            "normal",
            DEFAULT_SLAVE_ADDRESS,
            READ_INPUT_REGISTERS,
            expected_byte_count=36,
        ),
        ProbeCase(
            "H03",
            "read all 122 input registers (249-byte response)",
            h03,
            "normal",
            DEFAULT_SLAVE_ADDRESS,
            READ_INPUT_REGISTERS,
            expected_byte_count=244,
        ),
        ProbeCase(
            "H04",
            "unsupported function returns illegal-function exception",
            build_request(DEFAULT_SLAVE_ADDRESS, 0x05, b"\x00\x00\xFF\x00"),
            "exception",
            DEFAULT_SLAVE_ADDRESS,
            0x05,
            expected_exception=ILLEGAL_FUNCTION,
        ),
        ProbeCase(
            "H05",
            "input span beyond register 121 returns illegal-address exception",
            build_read_request(DEFAULT_SLAVE_ADDRESS, READ_INPUT_REGISTERS, 121, 2),
            "exception",
            DEFAULT_SLAVE_ADDRESS,
            READ_INPUT_REGISTERS,
            expected_exception=ILLEGAL_DATA_ADDRESS,
        ),
        ProbeCase(
            "H06A",
            "zero read quantity returns illegal-value exception",
            build_read_request(DEFAULT_SLAVE_ADDRESS, READ_INPUT_REGISTERS, 0, 0),
            "exception",
            DEFAULT_SLAVE_ADDRESS,
            READ_INPUT_REGISTERS,
            expected_exception=ILLEGAL_DATA_VALUE,
        ),
        ProbeCase(
            "H06B",
            "address value zero returns illegal-value exception",
            build_write_request(DEFAULT_SLAVE_ADDRESS, 0, 0),
            "exception",
            DEFAULT_SLAVE_ADDRESS,
            WRITE_SINGLE_REGISTER,
            expected_exception=ILLEGAL_DATA_VALUE,
        ),
        ProbeCase(
            "H07A",
            "bad CRC remains silent",
            corrupt_crc(h01),
            "silence",
            DEFAULT_SLAVE_ADDRESS,
            READ_HOLDING_REGISTERS,
        ),
        ProbeCase(
            "H07B",
            "broadcast address remains silent",
            build_read_request(0, READ_HOLDING_REGISTERS, 0, 1),
            "silence",
            0,
            READ_HOLDING_REGISTERS,
        ),
        ProbeCase(
            "H07C",
            "foreign address remains silent",
            build_read_request(MIGRATION_SLAVE_ADDRESS, READ_HOLDING_REGISTERS, 0, 1),
            "silence",
            MIGRATION_SLAVE_ADDRESS,
            READ_HOLDING_REGISTERS,
        ),
    ]


def validate_response(case: ProbeCase, response: bytes) -> None:
    if case.expected_kind == "silence":
        if response:
            raise ProbeError(f"expected silence, received {hex_bytes(response)}")
        return

    if not response:
        raise ProbeError("response timeout")
    if not has_valid_crc(response):
        raise ProbeError(f"invalid response CRC: {hex_bytes(response)}")
    if response[0] != case.expected_address:
        raise ProbeError(
            f"response address {response[0]} != expected {case.expected_address}"
        )

    if case.expected_kind == "exception":
        if len(response) != 5:
            raise ProbeError(f"exception length {len(response)} != 5")
        expected_function = case.expected_function | 0x80
        if response[1] != expected_function:
            raise ProbeError(
                f"exception function 0x{response[1]:02X} != 0x{expected_function:02X}"
            )
        if response[2] != case.expected_exception:
            raise ProbeError(
                f"exception code 0x{response[2]:02X} != 0x{case.expected_exception:02X}"
            )
        return

    if response[1] != case.expected_function:
        raise ProbeError(
            f"response function 0x{response[1]:02X} != 0x{case.expected_function:02X}"
        )
    if case.expected_echo is not None:
        if response != case.expected_echo:
            raise ProbeError(
                f"write echo mismatch: expected {hex_bytes(case.expected_echo)}"
            )
        return
    if len(response) < 5:
        raise ProbeError(f"read response too short: {len(response)}")
    if response[2] != case.expected_byte_count:
        raise ProbeError(
            f"byte count {response[2]} != expected {case.expected_byte_count}"
        )
    expected_length = 3 + response[2] + 2
    if len(response) != expected_length:
        raise ProbeError(
            f"response length {len(response)} != byte-count length {expected_length}"
        )


def self_test() -> int:
    checks = 0

    def require(condition: bool, message: str) -> None:
        nonlocal checks
        checks += 1
        if not condition:
            raise ProbeError(message)

    require(crc16_modbus(b"123456789") == 0x4B37, "official CRC check failed")
    cases = read_only_cases()
    require(len(cases) == 10, "read-only matrix size changed")
    require(has_valid_crc(cases[0].request), "H01 request CRC invalid")
    require(len(cases[2].request) == 8, "H03 request length changed")

    normal = append_crc(bytes((4, 3, 8, 0, 4, 0, 4, 0, 1, 0, 0)))
    validate_response(cases[0], normal)
    checks += 1
    exception = append_crc(bytes((4, 0x84, ILLEGAL_DATA_ADDRESS)))
    validate_response(cases[4], exception)
    checks += 1

    write = build_write_request(4, 0, 5)
    write_case = ProbeCase(
        "H08",
        "write address 4 to 5",
        write,
        "normal",
        4,
        WRITE_SINGLE_REGISTER,
        expected_echo=write,
    )
    validate_response(write_case, write)
    checks += 1

    bad_crc = normal[:-1] + bytes((normal[-1] ^ 1,))
    for response, label in (
        (bad_crc, "bad CRC accepted"),
        (normal[1:], "truncated response accepted"),
        (append_crc(bytes((5, 3, 8, 0, 4, 0, 4, 0, 1, 0, 0))), "foreign response accepted"),
        (append_crc(bytes((4, 4, 8, 0, 4, 0, 4, 0, 1, 0, 0))), "wrong function accepted"),
        (append_crc(bytes((4, 3, 6, 0, 4, 0, 4, 0, 1))), "wrong byte count accepted"),
    ):
        try:
            validate_response(cases[0], response)
        except ProbeError:
            checks += 1
        else:
            raise ProbeError(label)

    validate_response(cases[7], b"")
    checks += 1
    try:
        validate_response(cases[7], b"\x04")
    except ProbeError:
        checks += 1
    else:
        raise ProbeError("silence case accepted a response byte")

    require(checks == 14, f"self-test count changed: {checks}")
    print(f"P5 MODBUS HIL SELF-TEST: PASS ({checks} checks, serial NOT_OPENED)")
    return 0


def dry_run() -> int:
    print("P5 MODBUS HIL DRY-RUN: serial NOT_OPENED")
    for case in read_only_cases():
        print(
            f"{case.case_id} request={hex_bytes(case.request)} "
            f"expected={case.expected_kind} description={case.description}"
        )
    print("H08/H09 address migration: LOCKED (requires --allow-address-write)")
    print("H10 reset default and H11 physical reconnect: MANUAL_NOT_RUN")
    return 0


def open_serial(port_name: str, timeout_seconds: float):
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as error:
        raise ProbeError(
            "pyserial is required only for --port mode; use a project virtual "
            "environment, or run --self-test/--dry-run without it"
        ) from error

    return serial.Serial(
        port=port_name,
        baudrate=BAUD_RATE,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_EVEN,
        stopbits=serial.STOPBITS_ONE,
        timeout=timeout_seconds,
        write_timeout=timeout_seconds,
    )


def exchange(serial_port, case: ProbeCase) -> bytes:
    serial_port.reset_input_buffer()
    time.sleep(FRAME_GAP_SECONDS)
    written = serial_port.write(case.request)
    serial_port.flush()
    if written != len(case.request):
        raise ProbeError(f"short serial write: {written}/{len(case.request)}")
    if case.expected_kind == "silence":
        response = serial_port.read(1)
    else:
        response = serial_port.read(case.expected_length())
    time.sleep(FRAME_GAP_SECONDS)
    return bytes(response)


def run_cases(serial_port, cases: Iterable[ProbeCase]) -> tuple[int, int]:
    passed = 0
    failed = 0
    for case in cases:
        response = b""
        try:
            response = exchange(serial_port, case)
            validate_response(case, response)
        except (ProbeError, OSError) as error:
            failed += 1
            print(
                f"{case.case_id} FAIL reason={error} "
                f"request={hex_bytes(case.request)} response={hex_bytes(response)}"
            )
            break
        else:
            passed += 1
            print(f"{case.case_id} PASS")
    return passed, failed


def address_write_cases() -> Sequence[Sequence[ProbeCase]]:
    to_five = build_write_request(4, 0, 5)
    to_four = build_write_request(5, 0, 4)
    return (
        (
            ProbeCase(
                "H08",
                "write address 4 to 5",
                to_five,
                "normal",
                4,
                6,
                expected_echo=to_five,
            ),
            ProbeCase(
                "H08-OLD",
                "old address 4 becomes silent",
                build_read_request(4, 3, 0, 1),
                "silence",
                4,
                3,
            ),
            ProbeCase(
                "H08-NEW",
                "new address 5 responds",
                build_read_request(5, 3, 0, 1),
                "normal",
                5,
                3,
                expected_byte_count=2,
            ),
        ),
        (
            ProbeCase(
                "H09",
                "restore address 5 to 4",
                to_four,
                "normal",
                5,
                6,
                expected_echo=to_four,
            ),
            ProbeCase(
                "H09-RESTORED",
                "restored address 4 responds",
                build_read_request(4, 3, 0, 1),
                "normal",
                4,
                3,
                expected_byte_count=2,
            ),
        ),
    )


def run_port(args: argparse.Namespace) -> int:
    if args.timeout_ms < 50 or args.timeout_ms > 5000:
        raise ProbeError("--timeout-ms must be in 50..5000")
    if args.allow_address_write and args.confirm_default_address != 4:
        raise ProbeError(
            "address migration requires --confirm-default-address 4"
        )
    if args.confirm_default_address is not None and not args.allow_address_write:
        raise ProbeError(
            "--confirm-default-address is valid only with --allow-address-write"
        )

    timeout_seconds = args.timeout_ms / 1000.0
    with open_serial(args.port, timeout_seconds) as serial_port:
        print(
            f"P5 MODBUS HIL: port={args.port} serial=19200_8E1 "
            f"timeout_ms={args.timeout_ms}"
        )
        passed, failed = run_cases(serial_port, read_only_cases())
        if failed:
            print(f"P5 MODBUS HIL: FAIL (passed={passed}, writes NOT_RUN)")
            return 1
        if not args.allow_address_write:
            print(
                f"P5 MODBUS HIL: PASS_READ_ONLY ({passed}/10, "
                "address writes NOT_RUN, H10/H11 MANUAL_NOT_RUN)"
            )
            return 0

        for group in address_write_cases():
            group_passed, group_failed = run_cases(serial_port, group)
            passed += group_passed
            if group_failed:
                print(
                    "P5 MODBUS HIL: FAIL_ADDRESS_MIGRATION "
                    "(stop; confirm current address before recovery)"
                )
                return 1
        print(
            f"P5 MODBUS HIL: PASS_WITH_ADDRESS_RESTORE (passed={passed}, "
            "final_address=4, H10/H11 MANUAL_NOT_RUN)"
        )
        return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bounded Project Five address-4 Modbus RTU HIL probe"
    )
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--self-test", action="store_true", help="run without serial")
    mode.add_argument("--dry-run", action="store_true", help="print frames without serial")
    mode.add_argument("--port", help="explicit COM/tty device; never auto-scanned")
    parser.add_argument("--timeout-ms", type=int, default=500)
    parser.add_argument("--allow-address-write", action="store_true")
    parser.add_argument("--confirm-default-address", type=int)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        if args.self_test:
            if args.allow_address_write or args.confirm_default_address is not None:
                raise ProbeError("write options are invalid with --self-test")
            return self_test()
        if args.dry_run:
            if args.allow_address_write or args.confirm_default_address is not None:
                raise ProbeError("write options are invalid with --dry-run")
            return dry_run()
        return run_port(args)
    except ProbeError as error:
        print(f"P5 MODBUS HIL: ERROR: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
