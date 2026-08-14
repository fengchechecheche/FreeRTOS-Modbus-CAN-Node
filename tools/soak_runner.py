#!/usr/bin/env python3
"""Bounded soak orchestration and trend evaluation for Project Five."""

from __future__ import annotations

import argparse
import copy
import dataclasses
import datetime
import hashlib
import json
import os
import pathlib
import re
import signal
import subprocess
import sys
import threading
import time
import uuid
from typing import Any, Iterable, Sequence

SCHEMA_REVISION = 1
TASK_NAMES = ("protocol", "acquisition", "can", "health", "diagnostic")
SENSOR_NAMES = ("bme280", "veml7700", "adxl345_sample", "adxl345_feature")
MAX_SAMPLES = 600
MAX_LINE_BYTES = 16 * 1024
MAX_STDERR_BYTES = 64 * 1024
STACK_MINIMUM_FREE_WORDS = 32
PROCESS_GRACE_SECONDS = 5.0
IDENTITY_40 = re.compile(r"^[0-9a-f]{40}$")
IDENTITY_64 = re.compile(r"^[0-9a-f]{64}$")


class InputError(ValueError):
    """Raised for invalid or unbounded runner input."""


@dataclasses.dataclass
class Evaluation:
    status: str
    failures: list[str]
    reviews: list[str]
    metrics: dict[str, Any]


@dataclasses.dataclass
class ProcessResult:
    returncode: int
    timed_out: bool
    forced_kill: bool
    lines: list[str]
    stderr: str
    failure: str | None


def _is_int(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _require_mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise InputError(f"{label} must be an object")
    return value


def _require_int(mapping: dict[str, Any], key: str, label: str) -> int:
    value = mapping.get(key)
    if not _is_int(value):
        raise InputError(f"{label}.{key} must be an integer")
    return value


def _require_bool(mapping: dict[str, Any], key: str, label: str) -> bool:
    value = mapping.get(key)
    if not isinstance(value, bool):
        raise InputError(f"{label}.{key} must be a boolean")
    return value


def _require_string(mapping: dict[str, Any], key: str, label: str) -> str:
    value = mapping.get(key)
    if not isinstance(value, str) or not value:
        raise InputError(f"{label}.{key} must be a non-empty string")
    return value


def _deduplicate(items: Iterable[str]) -> list[str]:
    return list(dict.fromkeys(items))


def validate_metadata(metadata: dict[str, Any]) -> None:
    if metadata.get("kind") != "session":
        raise InputError("first record must have kind=session")
    if _require_int(metadata, "schema_revision", "session") != SCHEMA_REVISION:
        raise InputError("unsupported session schema_revision")
    phase = _require_string(metadata, "phase", "session")
    if phase not in {"smoke", "prerun", "formal", "self-test"}:
        raise InputError(f"unsupported phase: {phase}")
    _require_string(metadata, "session_id", "session")
    duration = _require_int(metadata, "planned_duration_s", "session")
    period = _require_int(metadata, "sample_period_s", "session")
    if duration <= 0 or duration > 24 * 60 * 60:
        raise InputError("planned_duration_s must be in 1..86400")
    if period <= 0 or period > duration:
        raise InputError("sample_period_s must be positive and no longer than duration")
    commit = _require_string(metadata, "git_commit", "session")
    firmware = _require_string(metadata, "firmware_sha256", "session")
    clean = _require_bool(metadata, "git_clean", "session")
    if phase == "formal":
        if not clean:
            raise InputError("formal session requires a clean Git worktree")
        if not IDENTITY_40.fullmatch(commit):
            raise InputError("formal session requires a 40-hex git_commit")
        if not IDENTITY_64.fullmatch(firmware):
            raise InputError("formal session requires a 64-hex firmware_sha256")
    if metadata.get("heap_policy") != "disabled":
        raise InputError("heap_policy must be disabled")
    if _require_int(metadata, "heap_reserve_bytes", "session") != 0:
        raise InputError("heap_reserve_bytes must be zero")
    for key in ("build_text_bytes", "build_data_bytes", "build_bss_bytes"):
        if _require_int(metadata, key, "session") < 0:
            raise InputError(f"session.{key} must be nonnegative")
    _require_string(metadata, "collector_kind", "session")
    _require_string(metadata, "board_identity", "session")
    _require_string(metadata, "started_at_utc", "session")


def validate_sample(sample: dict[str, Any], metadata: dict[str, Any]) -> None:
    if sample.get("kind") != "sample":
        raise InputError("sample record must have kind=sample")
    if _require_int(sample, "schema_revision", "sample") != SCHEMA_REVISION:
        raise InputError("unsupported sample schema_revision")
    if _require_string(sample, "session_id", "sample") != metadata["session_id"]:
        raise InputError("sample session_id does not match metadata")
    for key in ("sample_index", "elapsed_ms", "monotonic_ms"):
        if _require_int(sample, key, "sample") < 0:
            raise InputError(f"sample.{key} must be nonnegative")
    _require_string(sample, "boot_id", "sample")

    tasks = _require_mapping(sample.get("tasks"), "sample.tasks")
    if set(tasks) != set(TASK_NAMES):
        raise InputError("sample.tasks must contain exactly the five task names")
    for name in TASK_NAMES:
        task = _require_mapping(tasks[name], f"sample.tasks.{name}")
        for key in (
            "release",
            "missed",
            "deadline_miss",
            "budget_overrun",
            "configured_words",
            "minimum_free_words",
        ):
            if _require_int(task, key, f"sample.tasks.{name}") < 0:
                raise InputError(f"sample.tasks.{name}.{key} must be nonnegative")
        _require_bool(task, "measured", f"sample.tasks.{name}")

    queue = _require_mapping(sample.get("queue"), "sample.queue")
    for key in ("depth", "current", "maximum", "dropped", "drained"):
        if _require_int(queue, key, "sample.queue") < 0:
            raise InputError(f"sample.queue.{key} must be nonnegative")

    health = _require_mapping(sample.get("health"), "sample.health")
    _require_string(health, "state", "sample.health")
    _require_string(health, "feed", "sample.health")
    for key in ("warning_mask", "stalled_mask", "fault_code"):
        if _require_int(health, key, "sample.health") < 0:
            raise InputError(f"sample.health.{key} must be nonnegative")

    reset = _require_mapping(sample.get("reset"), "sample.reset")
    _require_string(reset, "primary", "sample.reset")
    _require_int(reset, "raw_flags", "sample.reset")
    _require_bool(reset, "loop", "sample.reset")

    rs485 = _require_mapping(sample.get("rs485"), "sample.rs485")
    for key in ("accepted", "error_count"):
        if _require_int(rs485, key, "sample.rs485") < 0:
            raise InputError(f"sample.rs485.{key} must be nonnegative")

    can = _require_mapping(sample.get("can"), "sample.can")
    _require_string(can, "state", "sample.can")
    for key in (
        "pending",
        "capacity",
        "maximum_pending",
        "event_dropped",
        "hal_busy",
        "bus_off",
        "recovery_attempts",
    ):
        if _require_int(can, key, "sample.can") < 0:
            raise InputError(f"sample.can.{key} must be nonnegative")

    sensors = _require_mapping(sample.get("sensors"), "sample.sensors")
    if set(sensors) != set(SENSOR_NAMES):
        raise InputError("sample.sensors must contain exactly four sources")
    for name in SENSOR_NAMES:
        sensor = _require_mapping(sensors[name], f"sample.sensors.{name}")
        _require_string(sensor, "state", f"sample.sensors.{name}")
        for key in ("sequence", "fault_count", "recovery_count"):
            if _require_int(sensor, key, f"sample.sensors.{name}") < 0:
                raise InputError(f"sample.sensors.{name}.{key} must be nonnegative")


def parse_json_lines(lines: Iterable[str]) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    records: list[dict[str, Any]] = []
    for line_number, raw in enumerate(lines, 1):
        if len(raw.encode("utf-8")) > MAX_LINE_BYTES:
            raise InputError(f"line {line_number} exceeds {MAX_LINE_BYTES} bytes")
        if not raw.strip():
            continue
        try:
            record = json.loads(raw)
        except json.JSONDecodeError as error:
            raise InputError(f"line {line_number} is not valid JSON: {error.msg}") from error
        if not isinstance(record, dict):
            raise InputError(f"line {line_number} must be a JSON object")
        records.append(record)
        if len(records) > MAX_SAMPLES + 1:
            raise InputError(f"input exceeds {MAX_SAMPLES} samples")
    if len(records) < 2:
        raise InputError("input requires one session record and at least one sample")
    metadata = records[0]
    samples = records[1:]
    validate_metadata(metadata)
    for sample in samples:
        validate_sample(sample, metadata)
    return metadata, samples


def _counter_paths(sample: dict[str, Any]) -> dict[str, int]:
    counters: dict[str, int] = {}
    for name in TASK_NAMES:
        task = sample["tasks"][name]
        for key in ("release", "missed", "deadline_miss", "budget_overrun"):
            counters[f"task.{name}.{key}"] = task[key]
    for key in ("maximum", "dropped", "drained"):
        counters[f"queue.{key}"] = sample["queue"][key]
    counters["rs485.accepted"] = sample["rs485"]["accepted"]
    counters["rs485.error_count"] = sample["rs485"]["error_count"]
    for key in ("maximum_pending", "event_dropped", "hal_busy", "bus_off", "recovery_attempts"):
        counters[f"can.{key}"] = sample["can"][key]
    for name in SENSOR_NAMES:
        sensor = sample["sensors"][name]
        for key in ("sequence", "fault_count", "recovery_count"):
            counters[f"sensor.{name}.{key}"] = sensor[key]
    return counters


def evaluate(metadata: dict[str, Any], samples: list[dict[str, Any]]) -> Evaluation:
    validate_metadata(metadata)
    if not samples:
        raise InputError("at least one sample is required")
    for sample in samples:
        validate_sample(sample, metadata)

    failures: list[str] = []
    reviews: list[str] = []
    task_stagnant = {name: 0 for name in TASK_NAMES}
    sensor_stagnant = {name: 0 for name in SENSOR_NAMES}
    repeated_errors: dict[str, int] = {}
    error_keys = {
        *(f"task.{name}.deadline_miss" for name in TASK_NAMES),
        *(f"task.{name}.budget_overrun" for name in TASK_NAMES),
        "queue.dropped",
        "rs485.error_count",
        "can.event_dropped",
        "can.bus_off",
    }
    first = samples[0]
    first_boot = first["boot_id"]
    previous: dict[str, Any] | None = None
    expected_index = first["sample_index"]
    period_ms = metadata["sample_period_s"] * 1000

    min_stack = {name: first["tasks"][name]["minimum_free_words"] for name in TASK_NAMES}
    max_queue = 0
    max_can_pending = 0

    for sample in samples:
        if sample["sample_index"] != expected_index:
            failures.append(
                f"sample_index expected {expected_index}, got {sample['sample_index']}"
            )
            expected_index = sample["sample_index"]
        expected_index += 1
        if sample["boot_id"] != first_boot:
            failures.append("boot identity changed during session")
        if sample["health"]["fault_code"] != 0:
            failures.append(f"fault_code became {sample['health']['fault_code']}")
        if sample["reset"]["loop"]:
            failures.append("reset loop was reported")

        queue = sample["queue"]
        max_queue = max(max_queue, queue["current"], queue["maximum"])
        if queue["current"] > queue["depth"] or queue["maximum"] > queue["depth"]:
            failures.append("diagnostic queue exceeded declared depth")
        can = sample["can"]
        max_can_pending = max(max_can_pending, can["pending"], can["maximum_pending"])
        if can["pending"] > can["capacity"] or can["maximum_pending"] > can["capacity"]:
            failures.append("CAN pending exceeded declared capacity")

        for name in TASK_NAMES:
            task = sample["tasks"][name]
            if task["measured"]:
                min_stack[name] = min(min_stack[name], task["minimum_free_words"])
                if task["minimum_free_words"] < STACK_MINIMUM_FREE_WORDS:
                    failures.append(
                        f"task {name} minimum_free_words={task['minimum_free_words']} < {STACK_MINIMUM_FREE_WORDS}"
                    )

        if previous is not None:
            if sample["elapsed_ms"] <= previous["elapsed_ms"]:
                failures.append("elapsed_ms did not increase")
            if sample["monotonic_ms"] <= previous["monotonic_ms"]:
                failures.append("monotonic_ms did not increase")
            gap = sample["elapsed_ms"] - previous["elapsed_ms"]
            if gap > 3 * period_ms:
                failures.append(f"two or more consecutive samples missing: gap={gap}ms")
            elif gap > 2 * period_ms:
                reviews.append(f"one isolated sample gap: {gap}ms")

            old_counters = _counter_paths(previous)
            new_counters = _counter_paths(sample)
            for path, current in new_counters.items():
                prior = old_counters[path]
                if current < prior:
                    failures.append(f"counter rollback: {path} {prior}->{current}")
                delta = current - prior
                if path in error_keys:
                    repeated_errors[path] = repeated_errors.get(path, 0) + 1 if delta > 0 else 0
                    if delta > 0:
                        reviews.append(f"counted error increment: {path} +{delta}")
                    if repeated_errors[path] >= 3:
                        failures.append(f"error increased in three consecutive windows: {path}")

            for name in task_stagnant:
                if sample["tasks"][name]["release"] == previous["tasks"][name]["release"]:
                    task_stagnant[name] += 1
                    reviews.append(f"task {name} had one no-progress sample")
                else:
                    task_stagnant[name] = 0
                if task_stagnant[name] >= 2:
                    failures.append(f"task {name} stalled for two consecutive samples")

            for name in SENSOR_NAMES:
                if sample["sensors"][name]["sequence"] == previous["sensors"][name]["sequence"]:
                    sensor_stagnant[name] += 1
                    reviews.append(f"sensor {name} had one no-progress sample")
                else:
                    sensor_stagnant[name] = 0
                if sensor_stagnant[name] >= 2:
                    failures.append(f"sensor {name} stalled for two consecutive samples")

            for name in TASK_NAMES:
                old_free = previous["tasks"][name]["minimum_free_words"]
                new_free = sample["tasks"][name]["minimum_free_words"]
                if sample["tasks"][name]["measured"] and new_free < old_free:
                    reviews.append(f"task {name} stack watermark decreased {old_free}->{new_free}")
        previous = sample

    required_last_ms = max(
        0, metadata["planned_duration_s"] * 1000 - period_ms
    )
    if samples[-1]["elapsed_ms"] < required_last_ms:
        failures.append(
            "session ended before planned duration: "
            f"last={samples[-1]['elapsed_ms']}ms required>={required_last_ms}ms"
        )

    window_count = min(4, len(samples))
    windows: list[dict[str, Any]] = []
    for window in range(window_count):
        start = (window * len(samples)) // window_count
        end = ((window + 1) * len(samples)) // window_count
        subset = samples[start:end]
        windows.append(
            {
                "index": window + 1,
                "sample_count": len(subset),
                "elapsed_first_ms": subset[0]["elapsed_ms"],
                "elapsed_last_ms": subset[-1]["elapsed_ms"],
                "queue_max": max(item["queue"]["current"] for item in subset),
                "minimum_free_words": {
                    name: min(item["tasks"][name]["minimum_free_words"] for item in subset)
                    for name in TASK_NAMES
                },
                "error_totals": {
                    "rs485": subset[-1]["rs485"]["error_count"] - subset[0]["rs485"]["error_count"],
                    "queue_drop": subset[-1]["queue"]["dropped"] - subset[0]["queue"]["dropped"],
                    "can_event_drop": subset[-1]["can"]["event_dropped"] - subset[0]["can"]["event_dropped"],
                },
            }
        )

    failures = _deduplicate(failures)
    reviews = _deduplicate(reviews)
    status = "FAIL" if failures else ("REVIEW_REQUIRED" if reviews else "PASS")
    metrics = {
        "sample_count": len(samples),
        "elapsed_first_ms": samples[0]["elapsed_ms"],
        "elapsed_last_ms": samples[-1]["elapsed_ms"],
        "minimum_free_words": min_stack,
        "queue_maximum_observed": max_queue,
        "can_pending_maximum_observed": max_can_pending,
        "windows": windows,
    }
    return Evaluation(status, failures, reviews, metrics)


def _terminate_process(process: subprocess.Popen[str]) -> bool:
    if process.poll() is not None:
        return False
    if os.name == "posix":
        os.killpg(process.pid, signal.SIGTERM)
    else:
        process.terminate()
    try:
        process.wait(PROCESS_GRACE_SECONDS)
        return False
    except subprocess.TimeoutExpired:
        if os.name == "posix":
            os.killpg(process.pid, signal.SIGKILL)
        else:
            process.kill()
        process.wait(PROCESS_GRACE_SECONDS)
        return True


def run_process(argv: Sequence[str], timeout_seconds: float) -> ProcessResult:
    if not argv or timeout_seconds <= 0:
        raise InputError("process argv and timeout must be explicit")
    process = subprocess.Popen(
        list(argv),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        encoding="utf-8",
        errors="replace",
        shell=False,
        start_new_session=os.name == "posix",
    )
    lines: list[str] = []
    stderr_chunks: list[str] = []
    failure: list[str] = []
    stop = threading.Event()

    def read_stdout() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            if len(line.encode("utf-8")) > MAX_LINE_BYTES:
                failure.append(f"collector line exceeds {MAX_LINE_BYTES} bytes")
                stop.set()
                continue
            if len(lines) >= MAX_SAMPLES:
                failure.append(f"collector exceeds {MAX_SAMPLES} samples")
                stop.set()
                continue
            lines.append(line.rstrip("\r\n"))

    def read_stderr() -> None:
        assert process.stderr is not None
        retained = 0
        for chunk in iter(lambda: process.stderr.read(4096), ""):
            encoded = chunk.encode("utf-8")
            if retained < MAX_STDERR_BYTES:
                remaining = MAX_STDERR_BYTES - retained
                kept = encoded[:remaining].decode("utf-8", errors="replace")
                stderr_chunks.append(kept)
                retained += len(kept.encode("utf-8"))

    stdout_thread = threading.Thread(target=read_stdout, daemon=True)
    stderr_thread = threading.Thread(target=read_stderr, daemon=True)
    stdout_thread.start()
    stderr_thread.start()
    deadline = time.monotonic() + timeout_seconds
    timed_out = False
    forced_kill = False
    try:
        while process.poll() is None:
            if stop.is_set():
                forced_kill = _terminate_process(process)
                break
            if time.monotonic() >= deadline:
                timed_out = True
                forced_kill = _terminate_process(process)
                break
            time.sleep(0.01)
    except BaseException:
        forced_kill = _terminate_process(process)
        raise
    finally:
        stdout_thread.join(PROCESS_GRACE_SECONDS)
        stderr_thread.join(PROCESS_GRACE_SECONDS)
    return ProcessResult(
        process.returncode if process.returncode is not None else -1,
        timed_out,
        forced_kill,
        lines,
        "".join(stderr_chunks),
        failure[0] if failure else None,
    )


def stable_metadata(phase: str = "self-test") -> dict[str, Any]:
    return {
        "kind": "session",
        "schema_revision": SCHEMA_REVISION,
        "session_id": "self-test-session",
        "phase": phase,
        "planned_duration_s": 480,
        "sample_period_s": 60,
        "git_commit": "a" * 40,
        "git_clean": True,
        "firmware_sha256": "b" * 64,
        "heap_policy": "disabled",
        "heap_reserve_bytes": 0,
        "build_text_bytes": 53272,
        "build_data_bytes": 160,
        "build_bss_bytes": 13224,
        "collector_kind": "self-test",
        "board_identity": "self-test-board",
        "started_at_utc": "2026-08-15T00:00:00Z",
    }


def stable_sample(index: int) -> dict[str, Any]:
    tasks = {
        name: {
            "release": (index + 1) * (position + 10),
            "missed": 0,
            "deadline_miss": 0,
            "budget_overrun": 0,
            "configured_words": 256,
            "minimum_free_words": 128 - position,
            "measured": True,
        }
        for position, name in enumerate(TASK_NAMES)
    }
    sensors = {
        name: {
            "sequence": (index + 1) * (position + 1),
            "state": "FRESH",
            "fault_count": 0,
            "recovery_count": 0,
        }
        for position, name in enumerate(SENSOR_NAMES)
    }
    return {
        "kind": "sample",
        "schema_revision": SCHEMA_REVISION,
        "session_id": "self-test-session",
        "sample_index": index,
        "elapsed_ms": index * 60_000,
        "monotonic_ms": 1_000 + index * 60_000,
        "boot_id": "boot-1",
        "tasks": tasks,
        "queue": {"depth": 8, "current": index % 2, "maximum": 2, "dropped": 0, "drained": index},
        "health": {"state": "SERVICEABLE", "warning_mask": 0, "stalled_mask": 0, "feed": "ALLOWED", "fault_code": 0},
        "reset": {"primary": "POWER_ON", "raw_flags": 1, "loop": False},
        "rs485": {"accepted": index * 10, "error_count": 0},
        "can": {"state": "ACTIVE", "pending": 0, "capacity": 14, "maximum_pending": 2, "event_dropped": 0, "hal_busy": 0, "bus_off": 0, "recovery_attempts": 0},
        "sensors": sensors,
    }


def run_self_test() -> int:
    metadata = stable_metadata()
    stable = [stable_sample(index) for index in range(9)]
    checks = 0

    def expect_status(name: str, samples: list[dict[str, Any]], status: str) -> None:
        nonlocal checks
        result = evaluate(metadata, samples)
        if result.status != status:
            raise AssertionError(f"{name}: expected {status}, got {result.status}: {result.failures} {result.reviews}")
        checks += 1

    expect_status("stable", copy.deepcopy(stable), "PASS")

    mutants: list[tuple[str, list[dict[str, Any]]]] = []
    sample = copy.deepcopy(stable)
    sample[3]["boot_id"] = "boot-2"
    mutants.append(("identity", sample))
    sample = copy.deepcopy(stable)
    sample[3]["monotonic_ms"] = sample[2]["monotonic_ms"]
    mutants.append(("timestamp", sample))
    sample = copy.deepcopy(stable)
    sample[3]["rs485"]["accepted"] = 0
    mutants.append(("counter", sample))
    sample = copy.deepcopy(stable)
    for index in (3, 4):
        sample[index]["tasks"]["protocol"]["release"] = sample[2]["tasks"]["protocol"]["release"]
    mutants.append(("task-stall", sample))
    sample = copy.deepcopy(stable)
    for index in (3, 4):
        sample[index]["tasks"]["health"]["release"] = sample[2]["tasks"]["health"]["release"]
    mutants.append(("health-task-stall", sample))
    sample = copy.deepcopy(stable)
    sample[4]["tasks"]["can"]["minimum_free_words"] = 31
    mutants.append(("stack", sample))
    sample = copy.deepcopy(stable)
    sample[4]["queue"]["current"] = 9
    mutants.append(("queue", sample))
    sample = copy.deepcopy(stable)
    sample[4]["health"]["fault_code"] = 1
    mutants.append(("fault", sample))
    sample = copy.deepcopy(stable)
    for index in (3, 4, 5):
        sample[index]["rs485"]["error_count"] = index - 2
        for later in range(index + 1, len(sample)):
            sample[later]["rs485"]["error_count"] = max(
                sample[later]["rs485"]["error_count"], index - 2
            )
    mutants.append(("error-growth", sample))
    for name, samples in mutants:
        expect_status(name, samples, "FAIL")

    isolated = copy.deepcopy(stable)
    for index in range(4, len(isolated)):
        isolated[index]["queue"]["dropped"] = 1
    expect_status("isolated-review", isolated, "REVIEW_REQUIRED")

    try:
        parse_json_lines([json.dumps(metadata), "not-json"])
        raise AssertionError("malformed input accepted")
    except InputError:
        checks += 1
    try:
        parse_json_lines([json.dumps(metadata), "{" + ("x" * MAX_LINE_BYTES) + "}"])
        raise AssertionError("oversized input accepted")
    except InputError:
        checks += 1
    try:
        unknown = copy.deepcopy(metadata)
        unknown["schema_revision"] = 99
        parse_json_lines([json.dumps(unknown), json.dumps(stable[0])])
        raise AssertionError("unknown schema accepted")
    except InputError:
        checks += 1
    try:
        missing = copy.deepcopy(stable)
        del missing[0]["tasks"]["protocol"]["release"]
        evaluate(metadata, missing)
        raise AssertionError("missing required field accepted")
    except InputError:
        checks += 1
    try:
        missing_metadata = copy.deepcopy(metadata)
        del missing_metadata["build_text_bytes"]
        parse_json_lines([json.dumps(missing_metadata), json.dumps(stable[0])])
        raise AssertionError("missing session metadata accepted")
    except InputError:
        checks += 1
    try:
        excessive = [json.dumps(metadata)] + [
            json.dumps(stable_sample(index)) for index in range(MAX_SAMPLES + 1)
        ]
        parse_json_lines(excessive)
        raise AssertionError("sample limit accepted")
    except InputError:
        checks += 1

    script = pathlib.Path(__file__).resolve()
    passed = run_process([sys.executable, str(script), "--fixture-mode", "pass"], 2.0)
    if passed.returncode != 0 or passed.timed_out or len(passed.lines) != 3:
        raise AssertionError(f"passing fixture failed: {passed}")
    checks += 1
    failed = run_process([sys.executable, str(script), "--fixture-mode", "nonzero"], 2.0)
    if failed.returncode == 0 or failed.timed_out:
        raise AssertionError(f"nonzero fixture misclassified: {failed}")
    checks += 1
    timed = run_process([sys.executable, str(script), "--fixture-mode", "sleep"], 0.15)
    if not timed.timed_out or timed.returncode == 0:
        raise AssertionError(f"timeout fixture misclassified: {timed}")
    checks += 1

    print(f"P5 SOAK RUNNER SELF-TEST: PASS ({checks} bounded checks)")
    return 0


def fixture_mode(mode: str) -> int:
    if mode == "pass":
        for index in range(3):
            print(json.dumps(stable_sample(index), separators=(",", ":")), flush=True)
        return 0
    if mode == "nonzero":
        print("fixture failure", file=sys.stderr)
        return 3
    time.sleep(5.0)
    return 0


def git_identity(repo: pathlib.Path) -> tuple[str, bool]:
    commit = subprocess.run(
        ["git", "-C", str(repo), "rev-parse", "HEAD"],
        check=True,
        text=True,
        capture_output=True,
    ).stdout.strip()
    status = subprocess.run(
        ["git", "-C", str(repo), "status", "--porcelain"],
        check=True,
        text=True,
        capture_output=True,
    ).stdout
    return commit, not bool(status.strip())


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_summary(output_dir: pathlib.Path, metadata: dict[str, Any], result: Evaluation) -> None:
    payload = {
        "schema_revision": SCHEMA_REVISION,
        "status": result.status,
        "session": metadata,
        "failures": result.failures,
        "reviews": result.reviews,
        "metrics": result.metrics,
    }
    (output_dir / "summary.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    lines = [
        "# P5 soak summary",
        "",
        f"- status: `{result.status}`",
        f"- phase: `{metadata['phase']}`",
        f"- samples: `{result.metrics['sample_count']}`",
        f"- git commit: `{metadata['git_commit']}`",
        f"- firmware SHA-256: `{metadata['firmware_sha256']}`",
        "",
        "## Failures",
        "",
    ]
    lines.extend(f"- {item}" for item in result.failures)
    if not result.failures:
        lines.append("- none")
    lines.extend(["", "## Review items", ""])
    lines.extend(f"- {item}" for item in result.reviews)
    if not result.reviews:
        lines.append("- none")
    (output_dir / "summary.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


def host_preflight(binary: pathlib.Path, iterations: int, timeout: float) -> int:
    if iterations < 1 or iterations > 100:
        raise InputError("host-preflight iterations must be in 1..100")
    started = time.monotonic()
    for index in range(iterations):
        result = run_process([str(binary)], timeout)
        if result.timed_out or result.failure or result.returncode != 0:
            print(
                f"HOST PREFLIGHT: FAIL iteration={index + 1} returncode={result.returncode} "
                f"timeout={result.timed_out} reason={result.failure or 'child failure'}",
                file=sys.stderr,
            )
            return 1
    elapsed = time.monotonic() - started
    print(f"P5 SOAK HOST PREFLIGHT: PASS ({iterations}/{iterations}, elapsed={elapsed:.3f}s)")
    return 0


def run_mode(args: argparse.Namespace) -> int:
    if not args.collector_command:
        raise InputError("--run requires --collector-command and explicit argv")
    command = list(args.collector_command)
    if command and command[0] == "--":
        command = command[1:]
    if not command:
        raise InputError("collector argv is empty")
    output_dir = args.output_dir.resolve()
    if output_dir.exists() and any(output_dir.iterdir()):
        raise InputError("output-dir already exists and is not empty")
    output_dir.mkdir(parents=True, exist_ok=True)
    commit, clean = git_identity(pathlib.Path.cwd())
    if commit != args.expected_commit:
        raise InputError(f"current commit {commit} does not match expected commit")
    firmware_hash = sha256_file(args.firmware_path)
    if firmware_hash != args.expected_firmware_sha256:
        raise InputError("firmware SHA-256 does not match expected value")
    metadata = {
        "kind": "session",
        "schema_revision": SCHEMA_REVISION,
        "session_id": str(uuid.uuid4()),
        "phase": args.phase,
        "planned_duration_s": args.duration_seconds,
        "sample_period_s": args.sample_seconds,
        "git_commit": commit,
        "git_clean": clean,
        "firmware_sha256": firmware_hash,
        "heap_policy": "disabled",
        "heap_reserve_bytes": 0,
        "build_text_bytes": args.build_text_bytes,
        "build_data_bytes": args.build_data_bytes,
        "build_bss_bytes": args.build_bss_bytes,
        "collector_kind": args.collector_kind,
        "board_identity": args.board_identity,
        "started_at_utc": datetime.datetime.now(datetime.timezone.utc)
        .isoformat()
        .replace("+00:00", "Z"),
    }
    validate_metadata(metadata)
    process = run_process(command, args.duration_seconds + PROCESS_GRACE_SECONDS)
    failures: list[str] = []
    if process.timed_out:
        failures.append("collector exceeded total timeout")
    if process.failure:
        failures.append(process.failure)
    if process.returncode != 0:
        failures.append(f"collector exited with {process.returncode}")
    records = [json.dumps(metadata, separators=(",", ":")), *process.lines]
    (output_dir / "samples.jsonl").write_text("\n".join(records) + "\n", encoding="utf-8")
    try:
        parsed_metadata, samples = parse_json_lines(records)
        result = evaluate(parsed_metadata, samples)
    except InputError as error:
        result = Evaluation("FAIL", [str(error)], [], {"sample_count": len(process.lines)})
    if failures:
        result.failures = _deduplicate([*failures, *result.failures])
        result.status = "FAIL"
    write_summary(output_dir, metadata, result)
    print(json.dumps({"status": result.status, "samples": len(process.lines)}))
    return 0 if result.status == "PASS" else 1


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--self-test", action="store_true")
    mode.add_argument("--dry-run", action="store_true")
    mode.add_argument("--host-preflight", action="store_true")
    mode.add_argument("--evaluate", type=pathlib.Path)
    mode.add_argument("--run", action="store_true")
    mode.add_argument("--fixture-mode", choices=("pass", "nonzero", "sleep"), help=argparse.SUPPRESS)
    parser.add_argument("--phase", choices=("smoke", "prerun", "formal"), default="smoke")
    parser.add_argument("--iterations", type=int, default=20)
    parser.add_argument("--binary", type=pathlib.Path, default=pathlib.Path("out/host-debug/p5_host_dual_bus_fault_matrix"))
    parser.add_argument("--child-timeout-seconds", type=float, default=2.0)
    parser.add_argument("--duration-seconds", type=int, default=600)
    parser.add_argument("--sample-seconds", type=int, default=60)
    parser.add_argument("--expected-commit", default="")
    parser.add_argument("--expected-firmware-sha256", default="")
    parser.add_argument("--firmware-path", type=pathlib.Path)
    parser.add_argument("--build-text-bytes", type=int)
    parser.add_argument("--build-data-bytes", type=int)
    parser.add_argument("--build-bss-bytes", type=int)
    parser.add_argument("--collector-kind", default="")
    parser.add_argument("--board-identity", default="")
    parser.add_argument("--output-dir", type=pathlib.Path)
    parser.add_argument("--collector-command", nargs=argparse.REMAINDER)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        if args.fixture_mode:
            return fixture_mode(args.fixture_mode)
        if args.self_test:
            return run_self_test()
        if args.dry_run:
            print(
                json.dumps(
                    {
                        "status": "DRY_RUN",
                        "phase": args.phase,
                        "collector": "NOT_STARTED",
                        "hardware": "NOT_RUN",
                        "formal_8h": "NOT_RUN",
                    },
                    sort_keys=True,
                )
            )
            return 0
        if args.host_preflight:
            return host_preflight(args.binary, args.iterations, args.child_timeout_seconds)
        if args.evaluate:
            with args.evaluate.open("r", encoding="utf-8") as stream:
                metadata, samples = parse_json_lines(stream)
            result = evaluate(metadata, samples)
            print(
                json.dumps(
                    dataclasses.asdict(result), indent=2, sort_keys=True
                )
            )
            return 0 if result.status == "PASS" else 1
        if args.run:
            if args.output_dir is None or args.firmware_path is None:
                raise InputError("--run requires --output-dir and --firmware-path")
            if any(
                value is None
                for value in (
                    args.build_text_bytes,
                    args.build_data_bytes,
                    args.build_bss_bytes,
                )
            ):
                raise InputError("--run requires explicit build text/data/bss sizes")
            if not args.collector_kind or not args.board_identity:
                raise InputError("--run requires --collector-kind and --board-identity")
            return run_mode(args)
    except (InputError, OSError, subprocess.SubprocessError) as error:
        print(f"P5 SOAK RUNNER: FAIL: {error}", file=sys.stderr)
        return 2
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
