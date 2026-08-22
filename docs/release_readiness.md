# P5-S7-T01 release readiness and blocker ledger

> Ledger schema: `P5_RELEASE_LEDGER_V1`
> Baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Current clean replay source: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Software source candidate gate: `PASS`
> Binary reproduction gate: `PASS_CURRENT_CLEAN_REPRODUCTION`
> Evidence matrix gate: `PASS_SCHEMA_REFERENCE_CHECK`
> Learning documentation gate: `PASS_35_FROZEN`
> Candidate collateral gate: `PASS_BOUNDED_RELEASE_REVIEW_READY`
> Hardware Release gate: `PASS_BOUNDED_HARDWARE_EVIDENCE`
> Tag / remote Release: `NOT_AUTHORIZED / NOT_RUN`

## Gate interpretation

`PASS` here means the corresponding bounded evidence gate has no open blocker.
It does not authorize a Git tag, remote Release, publication, production deployment,
metrology, MTBF or functional-safety claim.

| ID | Category | Status | Blocks | Summary | Evidence or next action |
|---|---|---|---|---|---|
| LIC-001 | LIC | CLOSED | SOURCE,BINARY,HARDWARE | Project-owned material uses MIT with public holder `fengchechecheche`; vendor paths are excluded | `LICENSE`; user decision recorded in the approved P5-S7-T01 plan |
| LIC-002 | LIC | CLOSED | SOURCE,BINARY,HARDWARE | CMSIS, HAL, FreeRTOS, generated material, original notices, and package terms are mapped | `THIRD_PARTY_NOTICES.md`; component license files; exact STM32CubeF4 1.28.3 package license copy |
| LIC-003 | LIC | CLOSED | SOURCE,HARDWARE | Sensor code is project-authored from cited datasheet registers/formulas; no vendor reference source is vendored | `sensors/`; BME280, VEML7700, and ADXL345 reports |
| PRIV-001 | PRIV | CLOSED | SOURCE,BINARY,HARDWARE | Tracked CubeMX problem report no longer exposes a Windows user-profile path | `docs/device_probe_report.md`; release-readiness path scan |
| SW-001 | SW | CLOSED | SOURCE,BINARY,HARDWARE | Current Host, contracts, ARM builds, and static resource gate pass without firmware changes | P5-S7-T01 final validation summary |
| REPRO-001 | REPRO | CLOSED | BINARY,HARDWARE | Clean local-archive Host/contract/ARM/resource replay passed and candidate hashes are recorded | `docs/reproduction_report.md`; `artifacts/release/p5_s7_t02_replay.json`; SHA-256 manifest |
| REPRO-002 | REPRO | CLOSED | BINARY,HARDWARE | `[047]` completed the independent no-network/no-cache Host, contract, ARM and resource replay | `docs/reproduction_report_repro_002.md`; `artifacts/release/p5_repro_002_replay.json`; REPRO-002 SHA-256 manifest |
| HW-001 | HW | CLOSED | HARDWARE | BME280 and VEML7700 bounded physical supplements passed; ADXL345 passed final-topology sampling, axis/vibration trend, restart and Modbus visibility through the default bounded polling route | Preserve the physical INT1/INT2, exact-rate, metrology and standalone-SPI exclusions in the domain reports |
| HW-002 | HW | CLOSED | HARDWARE | RS485-03 passed on the reference CH340 route at fixed address 4; Project Three `[039]` WSL and `[040]` physical Raspberry Pi 4B/ARM64 routes completed bounded read-only JSONL, one NUCLEO RESET recovery and local MQTT interoperability; the Pi also passed short active-RS485/passive-CAN concurrency | Keep H08/H09 address writes gated; preserve production deployment/MQTT, Host-active-CAN, repeated-fault and soak exclusions |
| HW-003 | HW | CLOSED | HARDWARE | CAN-03 and BUS-02 passed within the admitted common-GND CAN and reference-CH340 RS485 bench route | Keep claims bounded to one short-line concurrency run and one brief peer interruption per route; repeated fault endurance and physical bus-off recovery remain outside this closure |
| HW-004 | HW | CLOSED | HARDWARE | A clean `[074]` diagnostic build passed the Raspberry Pi-hosted formal 8-hour run with 480 samples and no CAN errors/events; the restored default firmware passed a separate approximately 10-minute Modbus/VCP/CAN regression | `docs/soak_trend_report.md`; preserve the historical 60-minute REVIEW_REQUIRED result and the diagnostic-8-hour/default-10-minute distinction |

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

The frozen `[036]` JSON and manifest are now validated as an immutable
historical pair: their exact file hashes, schema, source commit, required
labels, replay JSON digest and artifact mappings remain checked. Historical
source hashes are no longer incorrectly compared with the current worktree.

The independent REPRO-002 harness writes new `p5_repro_002_*` evidence names
and does not overwrite the T02 bundle. Committed source `[047]` completed 15
bounded commands from a tracked-file-only archive without network access or
prior build cache. Host Debug/Release, Release/Modbus/CAN/BSP checks, ARM
Debug/Release and the resource gate passed, so `REPRO-002` is closed for the
current software binary candidate.

The current Release BIN SHA-256 is
`8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c`.
The compact result and complete identity list are retained in the REPRO-002
JSON and manifest; source tar, binaries and full logs remain untracked.

A second independent Release build produced different BIN/HEX bytes because
linked FreeRTOS assert strings retain absolute source paths. The clean build is
repeatable as a procedure, but bit-for-bit output across differently named
directories is `NOT_CLAIMED`; no CMake or firmware flags were changed in T02.
REPRO-002 repeated the same bounded comparison and retained that limitation.

All four bounded hardware blockers are closed. NUCLEO-F446RE supplements passed
the `BSP-02` board-admission row, three bounded sensor routes and `WDG-01`.
Physical CAN, fixed-address-4 RS485, Project Three interoperability and one
bounded simultaneous-bus fault-isolation run have separate PASS rows. A clean
diagnostic build subsequently passed the formal eight-hour soak, and the
restored default firmware passed an independent approximately ten-minute
Modbus/VCP/CAN regression. These results remain bounded by their domain reports
and do not authorize a tag, remote Release, publication, metrology, MTBF or
production deployment.

## Evidence matrix result

The updated matrix projects the reviewed software, hardware and soak evidence into 24 bounded
evidence rows: 23 `PASS`, 0 `FAIL`, 0 `NOT_RUN`, and 1 `NOT_CLAIMED`. The machine-readable source is
`artifacts/release/p5_s7_t03_evidence_matrix.json`; the curated public view and
claim boundaries are in `docs/evidence_matrix.md`.

The matrix schema, result counts, public references, full Git identities, and
hardware-claim restrictions pass `tools/check_evidence_matrix.py`. `BSP-02`
passes only at the bounded board-admission layer. Physical RS485 and CAN each
pass only on their documented admitted routes; ADXL345 passes only through the
documented default bounded-polling route. Project Three interoperability passes
only on the documented Ubuntu-24.04-Gateway x86_64 and physical Raspberry Pi
4B/ARM64 read-only/local-MQTT routes; Host-active CAN and production deployment
remain excluded. `SOAK-02` is closed only by the diagnostic-firmware formal
eight-hour run and the separate restored-default approximately ten-minute
regression; the historical sixty-minute pre-run remains `REVIEW_REQUIRED` and
is not promoted to an independent PASS.

## Learning documentation result

P5-S7-T04 established one 35-entry S1-to-S7 route and restored four reviewed S1
tutorials that were absent from the independent repository. P5-S7-T05 was then
implemented and reviewed; all 35 tutorials now exist and are `FROZEN`.

`docs/learning/problem_ledger.md` retains 12 actual, evidence-linked engineering
problems. Evidence exclusions remain in the matrix and claim ledger rather than
being represented as fixed problems. This documentation gate does not authorize
a tag or remote Release.

## Software candidate and recruitment result

P5-S7-T05 keeps intended version `v0.1.0` in `UNRELEASED` state and advances
the collateral to `BOUNDED_HARDWARE_CANDIDATE_READY_FOR_RELEASE_REVIEW`. The
candidate note preserves `[039]` as the original documentation baseline and
`[047]` as the clean software replay source; later hardware evidence remains
separately bound to its recorded commits and firmware hashes rather than being
inferred from that clean build.

`docs/demo_guide.md` provides separate software and reviewed bounded-hardware
paths. The eight role-specific candidate sentences in
`docs/recruitment_claim_ledger.md` cite only matrix `PASS` rows and remain
`NOT_PUBLISHED`; all hardware wording retains its sensor, integration, soak and
production exclusions.

All 35 tutorials now exist and are `FROZEN` after user content review. This
collateral gate does not create a tag, binary attachment, final v0.1.0 release
note or remote Release.
