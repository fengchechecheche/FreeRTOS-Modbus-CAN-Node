#!/usr/bin/env python3
"""Reject CRLF/CR line endings in project-owned shell scripts."""

from pathlib import Path


def main() -> int:
    repo_root = Path(__file__).resolve().parent.parent
    scripts = sorted((repo_root / "tools").rglob("*.sh"))
    failures: list[str] = []

    for script in scripts:
        data = script.read_bytes()
        if b"\r" in data:
            failures.append(script.relative_to(repo_root).as_posix())

    if failures:
        print("P5 SHELL LINE ENDINGS: FAIL")
        for path in failures:
            print(f"CR byte found: {path}")
        return 1

    print(f"P5 SHELL LINE ENDINGS: PASS ({len(scripts)} scripts, LF only)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
