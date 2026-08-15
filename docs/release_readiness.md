# P5-S7-T01 release readiness and blocker ledger

> Ledger schema: `P5_RELEASE_LEDGER_V1`
> Baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Software source candidate gate: `PASS`
> Binary reproduction gate: `HISTORICAL_PASS_CURRENT_REPLAY_REQUIRED`
> Evidence matrix gate: `PASS_SCHEMA_REFERENCE_CHECK`
> Learning documentation gate: `PASS_35_FROZEN`
> Software candidate collateral gate: `PASS_READY_FOR_HARDWARE`
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
| REPRO-001 | REPRO | CLOSED | BINARY,HARDWARE | Clean local-archive Host/contract/ARM/resource replay passed and candidate hashes are recorded | `docs/reproduction_report.md`; `artifacts/release/p5_s7_t02_replay.json`; SHA-256 manifest |
| REPRO-002 | REPRO | OPEN | BINARY,HARDWARE | Current IWDG source differs from the frozen `[036]` replay manifest | After the user commits the current candidate, run a new tracked-file-only clean replay and refresh the candidate manifest in a separately authorized release task |
| HW-001 | HW | OPEN | HARDWARE | NUCLEO board admission and `WDG-01` passed; physical BME280, VEML7700 and ADXL345 checks are not complete | Run the frozen S4 sensor supplements when the sensors arrive |
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

## Clean reproduction result and next gate

P5-S7-T02 reproduced `[036]` from a tracked-file-only local archive in a new
temporary directory. Host Debug/Release, the release/Modbus/CAN/BSP checks,
ARM Debug/Release and the resource gate passed without network access or prior
build cache, so `REPRO-001` remains closed for that historical software binary
candidate.

The current post-`[045]` IWDG source is not covered by the frozen `[036]`
manifest. The unchanged historical manifest therefore rejects the current
`.ioc` hash as intended; `REPRO-002` keeps the current binary/hardware release
candidate open until a separately authorized clean replay is completed. This
supplement does not weaken or rewrite the earlier replay evidence.

A second independent Release build produced different BIN/HEX bytes because
linked FreeRTOS assert strings retain absolute source paths. The clean build is
repeatable as a procedure, but bit-for-bit output across differently named
directories is `NOT_CLAIMED`; no CMake or firmware flags were changed in T02.

The four hardware blockers remain open. NUCLEO-F446RE supplements passed the
narrow `BSP-02` board-admission row and `WDG-01`, including normal health feed,
one controlled IWDG reset and reset-only `.noinit` retention. `HW-001` remains
open for the three sensors; physical buses and the 8-hour soak are not inferred
from those bounded board results.

## Evidence matrix result

P5-S7-T03 projects the current baseline into 24 bounded evidence rows: 15
`PASS`, 8 `NOT_RUN`, and 1 `NOT_CLAIMED`. The machine-readable source is
`artifacts/release/p5_s7_t03_evidence_matrix.json`; the curated public view and
claim boundaries are in `docs/evidence_matrix.md`.

The matrix schema, result counts, public references, full Git identities, and
hardware-claim restrictions pass `tools/check_evidence_matrix.py`. `BSP-02`
passes only at the bounded board-admission layer; sensors, physical RS485,
physical CAN, and the 10-minute/60-minute/8-hour sessions remain open exactly
as listed above.

## Learning documentation result

P5-S7-T04 establishes one 35-entry S1-to-S7 route, restores four reviewed S1
tutorials that were absent from the independent repository, and leaves only
P5-S7-T05 as `PLANNED` without creating an empty file. At the T04 content-review
gate, all 34 available tutorials through T04 are `FROZEN`; T05 remains the only
`PLANNED` route entry and has no tutorial file.

`docs/learning/problem_ledger.md` retains 12 actual, evidence-linked engineering
problems. Hardware and integration `NOT_RUN` items remain in the evidence
matrix rather than being represented as fixed problems. This documentation
gate closes no HW blocker and does not authorize T05, a tag, or a Release.

## Software candidate and recruitment result

P5-S7-T05 keeps intended version `v0.1.0` in `UNRELEASED` state and preserves
the historical `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE` collateral. The
candidate note distinguishes the `[039]` documentation baseline from the
`[036]` clean replay source. The later IWDG source and hardware supplement are
not relabeled as clean-replayed; `REPRO-002` is the current replay gate.

`docs/demo_guide.md` provides one software-only demonstration path and a
separate hardware sequence marked `NOT_RUN`. The six role-specific candidate
sentences in `docs/recruitment_claim_ledger.md` cite only matrix `PASS` rows and
remain `NOT_PUBLISHED`; physical sensors, Project Three, physical CAN,
simultaneous physical buses and hardware soak stay ineligible.

All 35 tutorials now exist and are `FROZEN` after user content review. This
collateral gate closes no hardware blocker and does not create a
tag, binary attachment, final v0.1.0 release note or remote Release.
