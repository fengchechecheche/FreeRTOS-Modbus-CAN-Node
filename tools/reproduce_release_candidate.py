#!/usr/bin/env python3
"""Reproduce the P5 software candidate from a clean local Git archive."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import os
import pathlib
import shutil
import subprocess
import sys
import tarfile
import tempfile
import time
from typing import Any


SCHEMA = "P5_RELEASE_REPLAY_V1"
LOG_TAIL_LIMIT = 8192
COMMAND_TIMEOUT_SECONDS = 900
TEMP_PREFIX = "p5-s7-t02-"


class ReplayError(RuntimeError):
    """Raised when a bounded replay step fails."""


def sha256_file(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def bounded_tail(text: str, limit: int = LOG_TAIL_LIMIT) -> str:
    if len(text) <= limit:
        return text
    return "[truncated]\n" + text[-limit:]


def run_process(
    name: str,
    argv: list[str],
    cwd: pathlib.Path,
    *,
    timeout: int = COMMAND_TIMEOUT_SECONDS,
) -> dict[str, Any]:
    started = time.monotonic()
    try:
        completed = subprocess.run(
            argv,
            cwd=cwd,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
    except subprocess.TimeoutExpired as exc:
        stdout = exc.stdout.decode("utf-8", "replace") if isinstance(exc.stdout, bytes) else (exc.stdout or "")
        stderr = exc.stderr.decode("utf-8", "replace") if isinstance(exc.stderr, bytes) else (exc.stderr or "")
        result = {
            "name": name,
            "argv": argv,
            "exit_code": None,
            "timed_out": True,
            "duration_seconds": round(time.monotonic() - started, 3),
            "stdout_tail": bounded_tail(stdout),
            "stderr_tail": bounded_tail(stderr),
        }
        raise ReplayError(json.dumps(result, ensure_ascii=False)) from exc

    result = {
        "name": name,
        "argv": argv,
        "exit_code": completed.returncode,
        "timed_out": False,
        "duration_seconds": round(time.monotonic() - started, 3),
        "stdout_tail": bounded_tail(completed.stdout),
        "stderr_tail": bounded_tail(completed.stderr),
    }
    if completed.returncode != 0:
        raise ReplayError(json.dumps(result, ensure_ascii=False))
    return result


def run_text(argv: list[str], cwd: pathlib.Path) -> str:
    completed = subprocess.run(
        argv,
        cwd=cwd,
        text=True,
        encoding="utf-8",
        errors="strict",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=60,
        check=False,
    )
    if completed.returncode != 0:
        raise ReplayError(
            f"command failed ({completed.returncode}): {' '.join(argv)}: "
            f"{bounded_tail(completed.stderr.strip())}"
        )
    return completed.stdout.strip()


def validate_source_repo(source_repo: pathlib.Path, expected_commit: str) -> tuple[str, str]:
    source_repo = source_repo.resolve()
    if not (source_repo / ".git").exists():
        raise ReplayError("source repo has no .git directory")
    head = run_text(["git", "rev-parse", "HEAD"], source_repo)
    if head != expected_commit:
        raise ReplayError(f"HEAD mismatch: {head} != {expected_commit}")
    status = run_text(
        ["git", "status", "--porcelain=v1", "--untracked-files=all"], source_repo
    )
    if status:
        raise ReplayError("source repo is dirty; clean replay refused")
    subject = run_text(["git", "log", "-1", "--pretty=%s"], source_repo)
    return head, subject


def validate_member_name(name: str) -> None:
    path = pathlib.PurePosixPath(name)
    if path.is_absolute() or ".." in path.parts:
        raise ReplayError(f"unsafe archive member: {name}")


def safe_extract(archive_path: pathlib.Path, destination: pathlib.Path) -> None:
    destination.mkdir(parents=True, exist_ok=False)
    with tarfile.open(archive_path, "r:") as archive:
        members = archive.getmembers()
        for member in members:
            validate_member_name(member.name)
            if member.issym() or member.islnk():
                raise ReplayError(f"archive links are not allowed: {member.name}")
        archive.extractall(destination, members=members, filter="data")


def clean_temp_root(path: pathlib.Path) -> None:
    resolved = path.resolve()
    temp_parent = pathlib.Path(tempfile.gettempdir()).resolve()
    if resolved.parent != temp_parent or not resolved.name.startswith(TEMP_PREFIX):
        raise ReplayError(f"refusing unsafe temporary cleanup: {resolved}")
    shutil.rmtree(resolved)


def first_line(argv: list[str], cwd: pathlib.Path) -> str:
    try:
        output = run_text(argv, cwd)
    except (FileNotFoundError, ReplayError):
        return "NOT_FOUND"
    return output.splitlines()[0] if output else "UNKNOWN"


def tool_versions(source_repo: pathlib.Path) -> dict[str, str]:
    return {
        "git": first_line(["git", "--version"], source_repo),
        "gcc": first_line(["gcc", "--version"], source_repo),
        "cmake": first_line(["cmake", "--version"], source_repo),
        "ctest": first_line(["ctest", "--version"], source_repo),
        "ninja": first_line(["ninja", "--version"], source_repo),
        "arm_gcc": first_line(["arm-none-eabi-gcc", "--version"], source_repo),
        "python": first_line(["python3", "--version"], source_repo),
        "clang_format": first_line(["clang-format", "--version"], source_repo),
        "clang_tidy": first_line(["clang-tidy", "--version"], source_repo),
        "cppcheck": first_line(["cppcheck", "--version"], source_repo),
    }


def archive_source(source_repo: pathlib.Path, commit: str, archive_path: pathlib.Path) -> None:
    completed = subprocess.run(
        ["git", "archive", "--format=tar", f"--output={archive_path}", commit],
        cwd=source_repo,
        text=True,
        encoding="utf-8",
        errors="replace",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=120,
        check=False,
    )
    if completed.returncode != 0:
        raise ReplayError(f"git archive failed: {bounded_tail(completed.stderr)}")


def artifact_record(path: pathlib.Path) -> dict[str, Any]:
    if not path.is_file() or path.stat().st_size == 0:
        raise ReplayError(f"artifact missing or empty: {path.name}")
    return {"sha256": sha256_file(path), "bytes": path.stat().st_size}


def parse_size(path: pathlib.Path, cwd: pathlib.Path) -> dict[str, int]:
    output = run_text(["arm-none-eabi-size", str(path)], cwd)
    lines = [line.split() for line in output.splitlines() if line.strip()]
    if len(lines) < 2 or len(lines[-1]) < 3:
        raise ReplayError(f"cannot parse size output for {path.name}")
    text, data, bss = (int(value) for value in lines[-1][:3])
    return {
        "text": text,
        "data": data,
        "bss": bss,
        "flash": text + data,
        "linked_ram": data + bss,
    }


def command_matrix(source: pathlib.Path) -> list[tuple[str, list[str]]]:
    return [
        ("release_readiness", ["python3", "tools/check_release_readiness.py"]),
        ("release_readiness_self_test", ["python3", "tools/check_release_readiness.py", "--self-test"]),
        ("host_debug_release", ["./tools/verify_host.sh"]),
        ("modbus_contract_self_test", ["python3", "tools/verify_modbus_contract.py", "--self-test"]),
        ("can_contract", ["python3", "tools/verify_can_contract.py"]),
        ("can_contract_self_test", ["python3", "tools/verify_can_contract.py", "--self-test"]),
        ("bsp_contract", ["python3", "tools/verify_bsp_contract.py"]),
        ("bsp_contract_self_test", ["python3", "tools/verify_bsp_contract.py", "--self-test"]),
        ("firmware_debug_configure", ["cmake", "--preset", "firmware-debug"]),
        ("firmware_debug_build", ["cmake", "--build", "--preset", "firmware-debug"]),
        ("firmware_release_configure", ["cmake", "--preset", "firmware-release"]),
        ("firmware_release_build", ["cmake", "--build", "--preset", "firmware-release"]),
        (
            "resource_budget",
            [
                "python3",
                "tools/check_resource_budget.py",
                "--source-root",
                ".",
                "--debug-elf",
                "out/firmware-debug/freertos_modbus_can_node.elf",
                "--debug-map",
                "out/firmware-debug/freertos_modbus_can_node.map",
                "--release-elf",
                "out/firmware-release/freertos_modbus_can_node.elf",
                "--release-map",
                "out/firmware-release/freertos_modbus_can_node.map",
            ],
        ),
    ]


def public_command(result: dict[str, Any]) -> dict[str, Any]:
    return {
        "name": result["name"],
        "exit_code": result["exit_code"],
        "timed_out": result["timed_out"],
        "duration_seconds": result["duration_seconds"],
    }


def write_json(path: pathlib.Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def write_manifest(path: pathlib.Path, records: dict[str, dict[str, Any]]) -> None:
    lines = [f"{records[label]['sha256']}  {label}" for label in sorted(records)]
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def perform_replay(args: argparse.Namespace) -> int:
    source_repo = args.source_repo.resolve()
    private_output = args.private_output.resolve()
    head, subject = validate_source_repo(source_repo, args.expected_commit)
    versions = tool_versions(source_repo)

    temp_root = pathlib.Path(tempfile.mkdtemp(prefix=TEMP_PREFIX))
    archive_path = temp_root / "source.tar"
    source = temp_root / "source"
    repeat_source = temp_root / "source-repeat"
    raw_results: list[dict[str, Any]] = []
    try:
        archive_source(source_repo, head, archive_path)
        archive_hash = sha256_file(archive_path)
        safe_extract(archive_path, source)

        for name, argv in command_matrix(source):
            raw_results.append(run_process(name, argv, source))

        artifact_paths = {
            "source/freertos_modbus_can_node.ioc": source / "freertos_modbus_can_node.ioc",
            "source/CMakePresets.json": source / "CMakePresets.json",
            "build/debug/freertos_modbus_can_node.elf": source / "out/firmware-debug/freertos_modbus_can_node.elf",
            "build/debug/freertos_modbus_can_node.hex": source / "out/firmware-debug/freertos_modbus_can_node.hex",
            "build/debug/freertos_modbus_can_node.bin": source / "out/firmware-debug/freertos_modbus_can_node.bin",
            "build/release/freertos_modbus_can_node.elf": source / "out/firmware-release/freertos_modbus_can_node.elf",
            "build/release/freertos_modbus_can_node.hex": source / "out/firmware-release/freertos_modbus_can_node.hex",
            "build/release/freertos_modbus_can_node.bin": source / "out/firmware-release/freertos_modbus_can_node.bin",
        }
        artifacts = {label: artifact_record(path) for label, path in artifact_paths.items()}
        artifacts["source/git_archive.tar"] = {
            "sha256": archive_hash,
            "bytes": archive_path.stat().st_size,
        }

        repeat_result: dict[str, Any] = {"executed": False, "bit_for_bit_claim": "NOT_CLAIMED"}
        if args.repeat_release:
            safe_extract(archive_path, repeat_source)
            for name, argv in (
                ("repeat_release_configure", ["cmake", "--preset", "firmware-release"]),
                ("repeat_release_build", ["cmake", "--build", "--preset", "firmware-release"]),
            ):
                raw_results.append(run_process(name, argv, repeat_source))
            repeat_bin = repeat_source / "out/firmware-release/freertos_modbus_can_node.bin"
            repeat_hex = repeat_source / "out/firmware-release/freertos_modbus_can_node.hex"
            bin_equal = sha256_file(repeat_bin) == artifacts["build/release/freertos_modbus_can_node.bin"]["sha256"]
            hex_equal = sha256_file(repeat_hex) == artifacts["build/release/freertos_modbus_can_node.hex"]["sha256"]
            repeat_result = {
                "executed": True,
                "release_bin_equal": bin_equal,
                "release_hex_equal": hex_equal,
                "comparison": "EQUAL" if bin_equal and hex_equal else "PATH_DEPENDENT_DIFFERENCE",
                "bit_for_bit_claim": "PASS_BIN_HEX" if bin_equal and hex_equal else "NOT_CLAIMED",
            }

        limitations = [
            "Clean software reproduction does not prove hardware operation.",
            "No firmware binary or full build log is stored in the repository.",
            "Debug ELF hashes may include build-path-dependent debug information.",
        ]
        if repeat_result.get("comparison") == "PATH_DEPENDENT_DIFFERENCE":
            limitations.append(
                "Release BIN/HEX are not claimed bit-for-bit reproducible across "
                "differently named clean directories because linked FreeRTOS assert "
                "strings retain absolute source paths."
            )

        replay = {
            "schema": SCHEMA,
            "source": {
                "commit": head,
                "subject": subject,
                "archive_sha256": archive_hash,
                "snapshot": "git archive from clean tracked files",
            },
            "environment": {
                "distribution": "Ubuntu-24.04-STM32",
                "network_used": False,
                "prior_build_cache_used": False,
                "tools": versions,
            },
            "software": {
                "clean_reproduction": "PASS",
                "commands": [public_command(item) for item in raw_results],
                "resources": {
                    "debug": parse_size(artifact_paths["build/debug/freertos_modbus_can_node.elf"], source),
                    "release": parse_size(artifact_paths["build/release/freertos_modbus_can_node.elf"], source),
                },
                "repeat_release": repeat_result,
            },
            "hardware": {
                "flash": "WAITING_FOR_HARDWARE",
                "representative_replay": "NOT_RUN",
                "release_gate": "BLOCKED_WAITING_FOR_HARDWARE",
            },
            "artifacts": artifacts,
            "limitations": limitations,
        }

        private_output.mkdir(parents=True, exist_ok=True)
        raw = {
            "schema": SCHEMA,
            "source_commit": head,
            "commands": raw_results,
        }
        write_json(private_output / "replay_raw.json", raw)
        public_path = private_output / "p5_s7_t02_replay.json"
        write_json(public_path, replay)
        manifest_records = dict(artifacts)
        manifest_records["evidence/p5_s7_t02_replay.json"] = artifact_record(public_path)
        write_manifest(private_output / "p5_s7_t02_candidate_manifest.sha256", manifest_records)

        print(
            f"P5 CLEAN REPLAY: PASS ({len(raw_results)} commands, "
            f"repeat_release={repeat_result['bit_for_bit_claim']})"
        )
        print(f"SOURCE_COMMIT={head}")
        print(f"SOURCE_ARCHIVE_SHA256={archive_hash}")
        print(f"PRIVATE_OUTPUT={private_output}")
        return 0
    except ReplayError as exc:
        private_output.mkdir(parents=True, exist_ok=True)
        write_json(
            private_output / "replay_failure.json",
            {"schema": SCHEMA, "source_commit": head, "error": str(exc), "commands": raw_results},
        )
        print(f"P5 CLEAN REPLAY: FAIL: {exc}", file=sys.stderr)
        return 1
    finally:
        if args.keep_workdir:
            print(f"KEPT_WORKDIR={temp_root}")
        else:
            clean_temp_root(temp_root)


def run_self_test() -> int:
    checks = 0
    assert bounded_tail("abc", 3) == "abc"
    checks += 1
    assert bounded_tail("abcdef", 3) == "[truncated]\ndef"
    checks += 1
    validate_member_name("safe/path.txt")
    checks += 1
    for unsafe in ("/absolute", "../escape", "safe/../../escape"):
        try:
            validate_member_name(unsafe)
        except ReplayError:
            checks += 1
        else:
            raise AssertionError(f"unsafe member accepted: {unsafe}")

    with tempfile.TemporaryDirectory(prefix="p5-s7-t02-self-test-") as directory:
        root = pathlib.Path(directory)
        sample = root / "sample.txt"
        sample.write_bytes(b"p5\n")
        first = artifact_record(sample)
        second = artifact_record(sample)
        assert first == second and first["bytes"] == 3
        checks += 1

        archive_path = root / "unsafe.tar"
        with tarfile.open(archive_path, "w") as archive:
            info = tarfile.TarInfo("../escape.txt")
            payload = b"no"
            info.size = len(payload)
            archive.addfile(info, io.BytesIO(payload))
        try:
            safe_extract(archive_path, root / "extract")
        except ReplayError:
            checks += 1
        else:
            raise AssertionError("unsafe archive extracted")

        manifest = root / "manifest.sha256"
        write_manifest(manifest, {"z": first, "a": first})
        assert manifest.read_text(encoding="utf-8").splitlines()[0].endswith("  a")
        checks += 1

    assert public_command(
        {"name": "x", "exit_code": 0, "timed_out": False, "duration_seconds": 0.1}
    ) == {"name": "x", "exit_code": 0, "timed_out": False, "duration_seconds": 0.1}
    checks += 1

    print(f"P5 CLEAN REPLAY SELF-TEST: PASS ({checks} bounded checks)")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    parser.add_argument("--source-repo", type=pathlib.Path)
    parser.add_argument("--expected-commit")
    parser.add_argument("--private-output", type=pathlib.Path)
    parser.add_argument("--repeat-release", action="store_true")
    parser.add_argument("--keep-workdir", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        return run_self_test()
    if not args.source_repo or not args.expected_commit or not args.private_output:
        parser.error("--source-repo, --expected-commit and --private-output are required")
    return perform_replay(args)


if __name__ == "__main__":
    raise SystemExit(main())
