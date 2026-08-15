# P5-S7-T03 software, board, RS485 and CAN evidence matrix

> Matrix status: `FROZEN_SCHEMA / UPDATED_EVIDENCE`
> Matrix schema: `P5_EVIDENCE_MATRIX_V1`
> Matrix baseline: `[037] 3f494538d06069d3c78206dd95ca242bb3b27aa5`
> PTY evidence update: `[042] d7428e62a2df72325020ed63ab7979f4fb8c12f9`
> Clean replay source: `[036] 15932a2ff7adecdfbe5355559926a95b0df25845`
> Board supplement base: `807f85ce43240e94fc4aea3bd07e31c40a81d236`
> Watchdog supplement base: `[045] 433225e7d8694fac7e22a5425350336b4ed483f5`
> Row summary: `24 = 15 PASS + 8 NOT_RUN + 1 NOT_CLAIMED`
> Hardware Release: `BLOCKED_WAITING_FOR_HARDWARE`

## Purpose

This is a curated projection of the existing release, Host, cross-build,
board, sensor, RS485, Project Three, CAN, dual-bus and soak evidence. It is not
a new hardware test and does not combine qualified software results into one
project-wide PASS.

The machine-readable source is
`artifacts/release/p5_s7_t03_evidence_matrix.json`. This Markdown view keeps the
same row IDs and claim boundaries but omits repetitive configuration fields.

## Identity and precedence

`[037]` contains the T02 replay tool, report and public evidence. The actual
clean build input was `[036]`; those identities remain separate. The retained
Release BIN candidate is:

```text
7ce43b7263a3941cff8dfa83beef670aa6936ffffd3abcd215a7c5563319643f
```

That retained Release candidate was not used for the board supplement.
`BSP-02` uses the actually flashed default Debug ELF
`4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7`.
The tested working tree used `807f85ce43240e94fc4aea3bd07e31c40a81d236`
as its Git base plus the two reviewed minimal fixes recorded in the BSP report.
`WDG-01` uses the separately built default Debug ELF
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca`;
the temporary reset-smoke hash remains in the health report and is not the
final installed image.
Evidence precedence is: executed hardware supplement, T02 clean replay,
domain report, release blocker ledger, then README navigation. A historical
header or tutorial state never upgrades a runtime result.

## Environment and release

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| ENV-01 | WINDOWS_CONFIG | PASS | `docs/bsp_validation.md` | Windows configuration provenance is recorded; builds ran in Ubuntu |
| REL-01 | HOST | PASS | `docs/release_readiness.md` | license, attribution and public-path engineering gate passed |
| REP-01 | CLEAN_BUILD | PASS | `docs/reproduction_report.md` | tracked `[036]` completed clean Host/ARM/resource replay |
| REP-02 | CLEAN_BUILD | NOT_CLAIMED | `docs/reproduction_report.md` | no bit-for-bit claim across differently named clean paths |

`REP-02` remains separate from `REP-01`: procedural reproduction passed, while
linked FreeRTOS `__FILE__` paths changed Release BIN/HEX bytes.

## Software and firmware

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| SW-01 | HOST | PASS | T02 replay JSON | Ubuntu Host Debug/Release each passed 21 tests |
| FW-01 | CROSS_BUILD | PASS | T02 manifest/report | clean ARM Debug linked and produced ELF/HEX/BIN/MAP within budget |
| FW-02 | CROSS_BUILD | PASS | T02 manifest/report | clean ARM Release linked and produced ELF/HEX/BIN/MAP within budget |

Cross-build PASS does not establish flashing, startup, timing or communication.

## BSP, sensors and watchdog

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| BSP-01 | HOST | PASS | `docs/bsp_validation.md` | pin/clock/DMA/IRQ/safe-state/bus static contract passed |
| BSP-02 | HARDWARE | PASS | `docs/bsp_validation.md` | NUCLEO-F446RE ST-LINK, flash/verify/reset, VCP boot, runtime clock, GPIO register state, limited scheduler smoke and one power cycle passed |
| SNS-01 | HARDWARE | NOT_RUN | `docs/bme280_report.md` | BME280 identity, SPI sampling and accuracy were not run |
| SNS-02 | HARDWARE | NOT_RUN | `docs/veml7700_report.md` | VEML7700 ACK, sampling, range and accuracy were not run |
| SNS-03 | HARDWARE | NOT_RUN | `docs/adxl345_report.md` | ADXL345 identity, DATA_READY, axis and vibration checks were not run |
| WDG-01 | HARDWARE | PASS | `docs/health_recovery_report.md` | default health feed, one IWDG reset, reset-reason decode and reset-only `.noinit` retention passed; no power-loss claim |

The three sensor drivers also have Host/cross-build software evidence inside
`SW-01` and the domain reports. The rows above intentionally describe only the
missing physical claims.

## RS485 and Project Three

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| RS485-01 | HOST | PASS | `docs/modbus_hil_report.md` | UART DMA/RTU/server software contracts passed Host/cross-build gates |
| RS485-02 | HOST | PASS | `docs/modbus_hil_report.md` | committed production-C PTY matrix passed 10/10 with a 249 B maximum response |
| RS485-03 | HARDWARE | NOT_RUN | `docs/modbus_hil_report.md` | USB-RS485 H01-H11 and physical timing were not run |
| P3-01 | INTEGRATION | NOT_RUN | `docs/modbus_hil_report.md` | Project Three address-4 profile and interoperability were not run |

Self-test/dry-run alone is not PTY evidence. The committed PTY result is Host
evidence and still is not physical RS485 evidence.

## CAN and dual bus

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| CAN-01 | HOST | PASS | `docs/can_runtime.md` | map/codec/filter/IRQ/queue/recovery software candidate passed |
| CAN-02 | VIRTUAL_BUS | PASS | `docs/can_hil_report.md` | vcan matrix and one can-utils frame passed |
| CAN-03 | HARDWARE | NOT_RUN | `docs/can_hil_report.md` | candleLight, ACK, physical frames and bus-off were not run |
| BUS-01 | HOST | PASS | `docs/dual_bus_fault_matrix.md` | D01-D08 Host backpressure/isolation matrix passed |
| BUS-02 | INTEGRATION | NOT_RUN | `docs/dual_bus_fault_matrix.md` | physical RS485+CAN concurrency was not run |

`CAN-02` proves Linux SocketCAN behavior only. A local send return or TX echo
does not establish MCU acceptance or a physical ACK.

## Soak

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| SOAK-01 | HOST | PASS | `docs/soak_trend_report.md` | runner/schema/self-test and short Host preflight passed |
| SOAK-02 | SOAK | NOT_RUN | `docs/soak_trend_report.md` | 10-minute, 60-minute and 8-hour hardware sessions were not run |

The 20-iteration Host preflight is process-management evidence, not uptime or
real-time resource-trend evidence.

## Release blocker projection

| Blocker | Matrix rows | Current state |
|---|---|---|
| HW-001 sensors and watchdog | BSP-02, SNS-01..03, WDG-01 | OPEN; BSP-02 and WDG-01 passed, three sensors remain |
| HW-002 RS485 and Project Three | RS485-03, P3-01 | OPEN |
| HW-003 CAN and physical dual bus | CAN-03, BUS-02 | OPEN |
| HW-004 hardware soak | SOAK-02 | OPEN |

The software source and clean-reproduction blockers remain closed. `BSP-02`
now passes at its narrow board-admission layer and `WDG-01` passes at its
bounded reset-only layer, but `HW-001` remains open for the three sensors. No
other hardware blocker is closed.

## Public and local evidence

The public matrix points only to repository-relative, public-safe summaries.
It contains no `.private` path, user home, full device serial, credential or
private network identity.

The ignored local index retains only the existing bounded T02 raw command
summary. The board supplement is recorded in the existing domain summaries;
no serial number, raw programmer log or duplicated success bundle is retained.
Future failures should use one diagnostic record per useful problem.

## Validation

```bash
python3 tools/check_evidence_matrix.py
python3 tools/check_evidence_matrix.py --self-test
```

The checker validates row count, IDs, enums, full commit identities, relative
references, qualified physical PASS requirements, NOT_RUN firmware-null rules,
claim prefixes, summary counts and public privacy patterns. It does not parse
all historical Markdown or auto-promote states.

## Next gate

P5-S7-T04 may use only the `allowed_claim` projection when building the learning
route and problem review. P5-S7-T05 may later select public claims from PASS rows
at their recorded layers. The remaining hardware, integration and soak
`NOT_RUN` rows remain ineligible for Release or recruitment claims until real
supplements exist.
