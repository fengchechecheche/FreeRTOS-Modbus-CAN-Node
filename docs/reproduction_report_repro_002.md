# REPRO-002 current-candidate clean reproduction report

> Software replay: `PASS_CURRENT_CLEAN_REPRODUCTION`
> Evidence schema: `P5_RELEASE_REPLAY_V2`
> Source commit: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Network used: `false`
> Prior build cache used: `false`
> Hardware in this replay: `NOT_RUN_IN_THIS_SOFTWARE_REPLAY`
> Bit-for-bit claim: `NOT_CLAIMED / PATH_DEPENDENT_DIFFERENCE`

## Scope

REPRO-002 replays the committed post-IWDG candidate from a tracked-file-only
Git archive. It validates the current Host, contract, ARM Debug/Release and
static-resource gates without reading an existing build directory, fetching
from a remote or downloading dependencies.

The earlier `[036]` replay remains an immutable historical record in
`docs/reproduction_report.md` and the `p5_s7_t02_*` artifacts. REPRO-002 uses
separate file names and does not replace that evidence.

## Clean input and environment

```text
source commit       = 26411d2b627fd67654479f5a97a2066e47deafb5
source archive SHA  = b18e669a2079705509e12dd7a507490ea27201fe9344307de3101e49818560fe
source archive size = 40099840 bytes
network used        = false
prior cache used    = false
```

| Tool | Version |
|---|---|
| Git | 2.43.0 |
| GCC | 13.3.0 |
| CMake / CTest | 3.28.3 |
| Ninja | 1.11.1 |
| Arm GNU Toolchain | 13.2.1 20231009 |
| Python | 3.12.3 |
| clang-format / clang-tidy | 18.1.3 |
| Cppcheck | 2.13.0 |

## Replay result

All 15 bounded commands completed with exit code zero and without timeout:

- Release readiness and its negative self-test;
- Host Debug/Release, including the 22-test suites and PTY path;
- Modbus, CAN and BSP contracts plus their negative self-tests;
- ARM Debug/Release configure and build;
- static resource budget;
- a second independent Release configure and build.

| Build | text | data | bss | Flash | linked RAM |
|---|---:|---:|---:|---:|---:|
| Debug | 53956 B | 160 B | 13280 B | 54116 B | 13440 B |
| Release | 44992 B | 156 B | 13272 B | 45148 B | 13428 B |

The new candidate identities are:

```text
Debug BIN   f2458f4040b749934874f2ad30881c28c35f81b80865d58a60339e5fe58cf4c1
Debug ELF   6fde4a471ccf7c7d1b6655ed637b56e2d3de341d7fab1c088b4d7af7fd9c5f77
Release BIN 8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c
Release ELF b9f2a94f4df5bb7f718e8b41cd4472eba1d88b281ca31cdedf5d6dc1a382586f
```

The complete bounded identity list is retained in
`artifacts/release/p5_repro_002_candidate_manifest.sha256`.

## Repeat-build boundary

The second clean Release build passed configure, compile and link, but its
BIN/HEX bytes differed from the first clean path. Linked FreeRTOS assertion
strings retain absolute `__FILE__` paths, so REPRO-002 preserves the existing
boundary:

```text
clean procedural reproduction = PASS
cross-path bit-for-bit claim   = NOT_CLAIMED / PATH_DEPENDENT_DIFFERENCE
```

No compiler flags, assertion policy or firmware source were changed to force
byte equality.

## Hardware and evidence boundary

This replay did not flash, reset or sample the NUCLEO board. The separately
reviewed `BSP-02` and `WDG-01` hardware results remain valid at their recorded
layers, but are not rerun or inferred from this software replay. Sensors,
physical RS485, physical CAN and hardware soak remain open.

Only this report, the compact replay JSON and the SHA-256 manifest are public
evidence. The source archive, firmware images, build trees and full command
logs are not committed.

## Gate result

REPRO-002 may be closed for the current software binary candidate because the
committed source completed the no-network/no-cache clean procedure and the new
bundle passes independent consistency checks. The project remains
`UNRELEASED`; hardware blockers and tag/remote Release authorization are
unchanged.
