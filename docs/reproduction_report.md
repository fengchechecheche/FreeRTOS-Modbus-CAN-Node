# P5-S7-T02 clean reproduction report

> Software replay: `PASS_CLEAN_REPRODUCTION`
> Source baseline: `[036] 15932a2ff7adecdfbe5355559926a95b0df25845`
> Hardware flash/replay: `WAITING_FOR_HARDWARE / NOT_RUN`
> Bit-for-bit claim: `NOT_CLAIMED / PATH_DEPENDENT_DIFFERENCE`
> Network used: `false`
> Prior build cache used: `false`

## Scope

This report covers a tracked-file-only local archive of `[036]`, fresh Host and
ARM build directories, bounded contract checks, the static resource gate and
candidate hashes. It does not claim that the firmware was flashed or that any
sensor, RS485, CAN or long-soak hardware path passed.

## Clean input

The authoritative Ubuntu repository was clean and synchronized with the
Windows mirror before replay. The runner exported the exact local commit with
`git archive`; it did not copy the current worktree, use the existing `out/`,
read `.private/`, fetch from a remote or download dependencies.

```text
commit              = 15932a2ff7adecdfbe5355559926a95b0df25845
source archive size = 39823360 bytes
source archive SHA  = 650c213d778822ab2f6025151260b1e8d5e54cc2281d7c54b90fa0b444b71883
```

## Environment

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

The tracked legacy `tools/capture_tool_versions.sh` contains CRLF line endings
and cannot be executed directly by Ubuntu's `/usr/bin/env`. T02 did not hide
that failure or rewrite the helper outside the approved allowlist; the replay
runner captures the same versions directly without a shell wrapper.

## Clean replay result

| Gate | Result |
|---|---|
| runner self-test | 10 bounded checks PASS |
| release-readiness on `[036]` | PASS_SOFTWARE_CANDIDATE |
| release-readiness self-test on `[036]` | 10 bounded checks PASS |
| Host Debug | 21/21 PASS |
| Host Release | 21/21 PASS |
| Modbus contract | PASS plus negative self-test |
| CAN contract | 114 facts PASS; 5 mutants rejected |
| BSP contract | 532 stable facts PASS; negative self-test PASS |
| ARM Debug | configure/build PASS; ELF/HEX/BIN/MAP generated |
| ARM Release | configure/build PASS; ELF/HEX/BIN/MAP generated |
| static resource gate | PASS; static-only; heap reserve 0 |
| independent Release repeat | builds PASS; BIN/HEX differ by build path |

The clean builds produced:

| Build | text | data | bss | Flash | linked RAM |
|---|---:|---:|---:|---:|---:|
| Debug | 53224 B | 160 B | 13224 B | 53384 B | 13384 B |
| Release | 44360 B | 156 B | 13216 B | 44516 B | 13372 B |

These values differ slightly from an old in-place build because linked
FreeRTOS assert strings retain the absolute source path. They remain well
inside the frozen resource limits and do not indicate source-code drift.

## Candidate integrity

The public manifest records the source archive, `.ioc`, CMake presets, Debug
and Release ELF/HEX/BIN, and the compact replay JSON. Build products themselves
are not committed.

The retained candidate BIN hashes are:

```text
Debug  ab854d5f5189c5387f2da136e5a9977026c52c5d4158c3285117a71ca003e18b
Release 7ce43b7263a3941cff8dfa83beef670aa6936ffffd3abcd215a7c5563319643f
```

Use `artifacts/release/p5_s7_t02_candidate_manifest.sha256` for the complete
bounded list. The manifest is an identity record, not a promise that a build in
an arbitrarily named directory will produce the same bytes.

## Bit-for-bit investigation

A second Release build used the same archive and toolchain in another new
directory. Configure, compile, link and resource checks passed, but BIN and HEX
hashes differed. The first differing bytes and `strings` inspection showed
absolute paths to:

```text
Middlewares/Third_Party/FreeRTOS/Source/tasks.c
Middlewares/Third_Party/FreeRTOS/Source/queue.c
Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c
```

The second directory name was longer, and Release text increased from 44360 B
to 44380 B. T02 therefore reports:

```text
clean procedural reproduction = PASS
bit-for-bit reproducibility    = NOT_CLAIMED / PATH_DEPENDENT
```

T02 does not add `-ffile-prefix-map`, change FreeRTOS assertions or modify
firmware build flags. A deterministic-binary improvement may be reviewed as a
separate change if a future Release requires it.

## Hardware boundary

No new user-confirmed hardware admission was available. Ubuntu also had no
`lsusb` command and no detected `STM32_Programmer_CLI`, `st-flash` or `openocd`
executable. No installation or USB permission change was authorized, so T02 did
not call a programmer or open UART, RS485 or CAN.

```text
flash / verify / reset     = NOT_RUN
representative sensor path = NOT_RUN
Modbus / CAN physical path = NOT_RUN
hardware Release           = BLOCKED_WAITING_FOR_HARDWARE
```

## Evidence policy

The repository retains only the compact replay JSON, SHA-256 manifest, this
report and the tutorial. Full logs, object files, CMake caches, source tar and
firmware binaries are not committed. A bounded raw result exists only under
ignored `.private/` for current troubleshooting.

## Next gate

`REPRO-001` may be closed for the software binary candidate because the clean
procedure passed. `HW-001` through `HW-004` remain open. P5-S7-T03 may assemble
the evidence matrix with those hardware gaps, but it must not convert them to
PASS or call the result a v0.1.0 hardware Release.
