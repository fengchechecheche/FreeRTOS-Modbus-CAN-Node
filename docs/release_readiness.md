# P5-S7-T01 release readiness and blocker ledger

> Ledger schema: `P5_RELEASE_LEDGER_V1`
> Baseline: `[035] 591f60516afa50511c70bd53f985ce0ccbb4a018`
> Software source candidate gate: `PASS`
> Binary reproduction gate: `OPEN_FOR_P5-S7-T02`
> Hardware Release gate: `BLOCKED_WAITING_FOR_HARDWARE`
> Tag / remote Release: `NOT_AUTHORIZED / NOT_RUN`

## Gate interpretation

`PASS` here means the source candidate has no open licensing, public-path, or
known software-regression blocker. It does not mean clean-room reproduction,
firmware flashing, hardware operation, integration, or an 8-hour soak passed.

| ID | Category | Status | Blocks | Summary | Evidence or next action |
|---|---|---|---|---|---|
| LIC-001 | LIC | CLOSED | SOURCE,BINARY,HARDWARE | Project-owned material uses MIT with public holder `fengchechecheche`; vendor paths are excluded | `LICENSE`; user decision recorded in the approved P5-S7-T01 plan |
| LIC-002 | LIC | CLOSED | SOURCE,BINARY,HARDWARE | CMSIS, HAL, FreeRTOS, generated material, original notices, and package terms are mapped | `THIRD_PARTY_NOTICES.md`; component license files; exact STM32CubeF4 1.28.3 package license copy |
| LIC-003 | LIC | CLOSED | SOURCE,HARDWARE | Sensor code is project-authored from cited datasheet registers/formulas; no vendor reference source is vendored | `sensors/`; BME280, VEML7700, and ADXL345 reports |
| PRIV-001 | PRIV | CLOSED | SOURCE,BINARY,HARDWARE | Tracked CubeMX problem report no longer exposes a Windows user-profile path | `docs/device_probe_report.md`; release-readiness path scan |
| SW-001 | SW | CLOSED | SOURCE,BINARY,HARDWARE | Current Host, contracts, ARM builds, and static resource gate pass without firmware changes | P5-S7-T01 final validation summary |
| REPRO-001 | REPRO | OPEN | BINARY,HARDWARE | Clean-directory configure/build/replay and candidate hashes are not part of T01 | Execute P5-S7-T02; do not call the current local build a clean reproduction |
| HW-001 | HW | OPEN | HARDWARE | NUCLEO board admission and physical BME280, VEML7700, and ADXL345 checks are not complete | Run the frozen S2/S4 hardware supplements when hardware is available |
| HW-002 | HW | OPEN | HARDWARE | Physical USB-RS485 and Project Three interoperability are not complete | Run S5 hardware/integration supplement |
| HW-003 | HW | OPEN | HARDWARE | Physical CAN, candleLight, bus-off, and dual-bus concurrency are not complete | Run S6 hardware supplements |
| HW-004 | HW | OPEN | HARDWARE | 10-minute smoke, 60-minute pre-run, and formal 8-hour soak are not run | Admit a reviewed collector, then execute the frozen T05 sequence with separate authorization |

## Current license inventory result

The repository retains the original CMSIS Apache-2.0, HAL BSD-3-Clause,
FreeRTOS MIT, ST integration, generated-file, startup, and linker notices. The
exact STM32CubeF4 1.28.3 package terms are retained once under `LICENSES/`.
The root MIT scope explicitly excludes third-party and independently noticed
material.

The official package-license file is byte-for-byte preserved and retains its
upstream Markdown trailing spaces. Project-authored files pass the whitespace
check; the verbatim license is checked by its frozen SHA-256 instead of being
reformatted to satisfy a project style rule.

This is an engineering traceability result, not a legal opinion or a statement
that every future combination is automatically license-compatible.

## Public-safety result

The high-confidence scan covers project-owned public documentation, code,
tests, tools, and build files. It rejects user-profile home directories, local
drive paths, and WSL UNC paths. Vendor directories and the checker itself are
excluded from that narrow scan to avoid treating ordinary API terms or policy
patterns as leaked credentials.

`.private/` remains ignored, but ignore rules do not replace a release-file
scan. Complete device serial numbers, credentials, private network identifiers,
and raw diagnostics remain outside the public source candidate.

## Next gate

P5-S7-T02 may now perform clean reproduction. It must keep `REPRO-001` open
until a separate clean directory passes the required configure, Host tests,
ARM builds, and hash checks. The four hardware blockers remain open regardless
of T02 software success.
