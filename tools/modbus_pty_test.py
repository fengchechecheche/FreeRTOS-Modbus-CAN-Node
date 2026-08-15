#!/usr/bin/env python3
"""Run the bounded Project Five Modbus matrix through a POSIX PTY."""

from __future__ import annotations

import argparse
import contextlib
import io
import os
from pathlib import Path
import pty
import selectors
import subprocess
import sys
import tty
from typing import Optional, Sequence, TextIO

import modbus_hil_probe as probe

START_TIMEOUT_SECONDS = 2.0
STOP_TIMEOUT_SECONDS = 2.0
SERIAL_TIMEOUT_SECONDS = 0.5
EXPECTED_CASES = 10
EXPECTED_MAX_RESPONSE = 249


class PtyTestError(RuntimeError):
    """A bounded PTY orchestration or result failure."""


class TrackingSerial:
    """Delegate pyserial operations while retaining only the largest read."""

    def __init__(self, serial_port) -> None:
        self._serial_port = serial_port
        self.maximum_read_length = 0

    def __getattr__(self, name: str):
        return getattr(self._serial_port, name)

    def read(self, size: int = 1) -> bytes:
        response = bytes(self._serial_port.read(size))
        self.maximum_read_length = max(self.maximum_read_length, len(response))
        return response


def wait_for_ready(stream: TextIO, process: subprocess.Popen[str]) -> str:
    selector = selectors.DefaultSelector()
    selector.register(stream, selectors.EVENT_READ)
    try:
        if not selector.select(START_TIMEOUT_SECONDS):
            raise PtyTestError("PTY slave READY timeout")
        line = stream.readline().strip()
    finally:
        selector.close()

    if line != "P5 MODBUS PTY SLAVE: READY":
        return_code = process.poll()
        raise PtyTestError(
            f"unexpected PTY slave startup line={line!r} returncode={return_code}"
        )
    return line


def stop_server(process: subprocess.Popen[str]) -> tuple[int, str, str]:
    if process.poll() is None:
        process.terminate()
    try:
        stdout, stderr = process.communicate(timeout=STOP_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        process.kill()
        stdout, stderr = process.communicate(timeout=STOP_TIMEOUT_SECONDS)
        raise PtyTestError(
            "PTY slave did not stop after SIGTERM; forced termination\n"
            f"stdout={stdout.strip()}\nstderr={stderr.strip()}"
        )
    return process.returncode, stdout.strip(), stderr.strip()


def run(server_path: Path) -> int:
    if not server_path.is_file():
        raise PtyTestError(f"PTY slave executable not found: {server_path}")
    if not os.access(server_path, os.X_OK):
        raise PtyTestError(f"PTY slave is not executable: {server_path}")

    master_fd, slave_fd = pty.openpty()
    process: Optional[subprocess.Popen[str]] = None
    probe_output = io.StringIO()
    failure: Optional[str] = None
    maximum_read = 0
    passed = 0
    failed = 0
    server_stdout = ""
    server_stderr = ""
    server_returncode = -1

    try:
        tty.setraw(master_fd)
        tty.setraw(slave_fd)
        slave_name = os.ttyname(slave_fd)
        process = subprocess.Popen(
            [str(server_path), str(master_fd)],
            pass_fds=(master_fd,),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if process.stdout is None:
            raise PtyTestError("PTY slave stdout pipe unavailable")
        wait_for_ready(process.stdout, process)

        with probe.open_serial(slave_name, SERIAL_TIMEOUT_SECONDS) as serial_port:
            tracking_serial = TrackingSerial(serial_port)
            with contextlib.redirect_stdout(probe_output):
                passed, failed = probe.run_cases(
                    tracking_serial, probe.read_only_cases()
                )
            maximum_read = tracking_serial.maximum_read_length

        if failed != 0 or passed != EXPECTED_CASES:
            failure = f"probe result passed={passed} failed={failed}"
        elif maximum_read != EXPECTED_MAX_RESPONSE:
            failure = (
                f"maximum response {maximum_read} != {EXPECTED_MAX_RESPONSE} bytes"
            )
    except (OSError, probe.ProbeError, PtyTestError) as error:
        failure = str(error)
    finally:
        if process is not None:
            try:
                server_returncode, server_stdout, server_stderr = stop_server(process)
            except PtyTestError as error:
                failure = failure or str(error)
        os.close(slave_fd)
        os.close(master_fd)

    expected_server_summary = "P5 MODBUS PTY SLAVE: PASS"
    if server_returncode != 0:
        failure = failure or f"PTY slave return code {server_returncode}"
    elif not server_stdout.startswith(expected_server_summary):
        failure = failure or f"unexpected PTY slave summary: {server_stdout!r}"
    if server_stderr:
        failure = failure or f"PTY slave stderr: {server_stderr}"

    if failure is not None:
        print(f"P5 MODBUS PTY: FAIL reason={failure}", file=sys.stderr)
        details = probe_output.getvalue().strip()
        if details:
            print(details, file=sys.stderr)
        if server_stdout:
            print(server_stdout, file=sys.stderr)
        if server_stderr:
            print(server_stderr, file=sys.stderr)
        return 1

    print(
        "P5 MODBUS PTY: PASS "
        f"({passed}/{EXPECTED_CASES}, max_response={maximum_read} B, "
        "production_c_server=yes, physical_rs485=NOT_RUN)"
    )
    return 0


def parse_args(argv: Optional[Sequence[str]] = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Bounded Project Five production-C Modbus PTY test"
    )
    parser.add_argument("--server", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv: Optional[Sequence[str]] = None) -> int:
    args = parse_args(argv)
    try:
        return run(args.server.resolve())
    except PtyTestError as error:
        print(f"P5 MODBUS PTY: FAIL reason={error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
