# P5-S7-T03 software, board, RS485 and CAN evidence matrix

> Matrix status: `FROZEN_SCHEMA / UPDATED_EVIDENCE`
> Matrix schema: `P5_EVIDENCE_MATRIX_V1`
> Matrix baseline: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> PTY evidence update: `[042] d7428e62a2df72325020ed63ab7979f4fb8c12f9`
> Clean replay source: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Board supplement evidence: `[044] d409a8161669aae8ea4c01246df577d36212650a`
> Watchdog supplement evidence: `[046] 96fa46b41c38a0a0bb249876d9a0736165ac1642`
> Row summary: `24 = 18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`
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

`[036]`/`[037]` retain the historical T02 source/evidence pair. REPRO-002
independently replayed committed source `[047]`; the current Release BIN
candidate is:

```text
8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c
```

That retained Release candidate was not used for the board supplement.
`BSP-02` uses the actually flashed default Debug ELF
`4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7`.
The reviewed board supplement and its two minimal fixes are committed in
`[044] d409a8161669aae8ea4c01246df577d36212650a`.
`WDG-01` uses the separately built default Debug ELF
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca`;
the temporary reset-smoke hash remains in the health report and is not the
final installed image.
Evidence precedence is: executed hardware supplement, REPRO-002 clean replay,
domain report, release blocker ledger, then README navigation. A historical
header or tutorial state never upgrades a runtime result.

## Environment and release

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| ENV-01 | WINDOWS_CONFIG | PASS | `docs/bsp_validation.md` | Windows configuration provenance is recorded; builds ran in Ubuntu |
| REL-01 | HOST | PASS | `docs/release_readiness.md` | license, attribution and public-path engineering gate passed |
| REP-01 | CLEAN_BUILD | PASS | `docs/reproduction_report_repro_002.md` | tracked `[047]` completed clean Host/ARM/resource replay |
| REP-02 | CLEAN_BUILD | NOT_CLAIMED | `docs/reproduction_report_repro_002.md` | no bit-for-bit claim across differently named clean paths |

`REP-02` remains separate from `REP-01`: procedural reproduction passed, while
linked FreeRTOS `__FILE__` paths changed Release BIN/HEX bytes.

## Software and firmware

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| SW-01 | HOST | PASS | REPRO-002 replay JSON | Ubuntu Host Debug/Release each passed 22 tests |
| FW-01 | CROSS_BUILD | PASS | REPRO-002 manifest/report | clean ARM Debug linked and produced ELF/HEX/BIN/MAP within budget |
| FW-02 | CROSS_BUILD | PASS | REPRO-002 manifest/report | clean ARM Release linked and produced ELF/HEX/BIN/MAP within budget |

Cross-build PASS does not establish flashing, startup, timing or communication.

## BSP, sensors and watchdog

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| BSP-01 | HOST | PASS | `docs/bsp_validation.md` | pin/clock/DMA/IRQ/safe-state/bus static contract passed |
| BSP-02 | HARDWARE | PASS | `docs/bsp_validation.md` | NUCLEO-F446RE ST-LINK, flash/verify/reset, VCP boot, runtime clock, GPIO register state, limited scheduler smoke and one power cycle passed |
| SNS-01 | HARDWARE | PASS | `docs/bme280_report.md` | `0x60` identity, five fresh bounded samples with increasing sequence, application compensation output and reset reinitialization passed; metrology accuracy remains not claimed |
| SNS-02 | HARDWARE | PASS | `docs/veml7700_report.md` | `0x10` config access, five-sample sequence progress,遮挡/恢复方向 and two bounded range changes passed; metrology accuracy remains not claimed |
| SNS-03 | HARDWARE | NOT_RUN | `docs/adxl345_report.md` | `0xE5` identity passed only in the final topology; standalone SPI robustness is not claimed and DATA_READY/axis/vibration checks remain incomplete |
| WDG-01 | HARDWARE | PASS | `docs/health_recovery_report.md` | default health feed, one IWDG reset, reset-reason decode and reset-only `.noinit` retention passed; no power-loss claim |

The three sensor drivers also have Host/cross-build software evidence inside
`SW-01` and the domain reports. The rows above intentionally describe only the
missing physical claims.

## RS485 and Project Three

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| RS485-01 | HOST | PASS | `docs/modbus_hil_report.md` | UART DMA/RTU/server software contracts passed Host/cross-build gates |
| RS485-02 | HOST | PASS | `docs/modbus_hil_report.md` | committed production-C PTY matrix passed 10/10 with a 249 B maximum response |
| RS485-03 | HARDWARE | FAIL | `docs/modbus_hil_report.md` | H01 timed out; bounded loopback proved the forward path but the return path raised USART1 framing errors, so a second USB-RS485 is required to isolate the adapter from the Shield/PA10 path |
| P3-01 | INTEGRATION | NOT_RUN | `docs/modbus_hil_report.md` | Project Three address-4 profile and interoperability were not run |

Self-test/dry-run alone is not PTY evidence. The committed PTY result is Host
evidence and still is not physical RS485 evidence.

## CAN and dual bus

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| CAN-01 | HOST | PASS | `docs/can_runtime.md` | map/codec/filter/IRQ/queue/recovery software candidate passed |
| CAN-02 | VIRTUAL_BUS | PASS | `docs/can_hil_report.md` | vcan matrix and one can-utils frame passed |
| CAN-03 | HARDWARE | PASS | `docs/can_hil_report.md` | admitted candleLight/Shield/common-GND route passed identity, 252-frame periodic telemetry and bounded physical ACK in both directions; the new read-only `0x540/0x541` application round trip and dual-bus concurrency remain `NOT_RUN` |
| BUS-01 | HOST | PASS | `docs/dual_bus_fault_matrix.md` | D01-D08 Host backpressure/isolation matrix passed |
| BUS-02 | INTEGRATION | NOT_RUN | `docs/dual_bus_fault_matrix.md` | physical RS485+CAN concurrency was not run |

`CAN-02` proves Linux SocketCAN behavior only. `CAN-03` physical ACK is bound to
the reviewed RX-only/TX-once diagnostics and common-GND wiring; a local send
return or TX echo alone does not establish MCU acceptance or physical ACK.

## Soak

| ID | Layer | Result | Primary evidence | Allowed claim |
|---|---|---|---|---|
| SOAK-01 | HOST | PASS | `docs/soak_trend_report.md` | runner/schema/self-test and short Host preflight passed |
| SOAK-02 | SOAK | NOT_RUN | `docs/soak_trend_report.md` | 10-minute, 60-minute and 8-hour hardware sessions were not run |

The 20-iteration Host preflight is process-management evidence, not uptime or
real-time resource-trend evidence.

P5-HW-SNS-00 在基线 `d48f2c75b048dc60320045233312ad6df050e88e` 上以 SHA-256
`d011125dfbf8b96702da03d8c808476687639610d3b59acb7f23b1f42b6f8c4d`
的 one-shot probe ELF 完成最终三模块拓扑 3/3 身份/存在性准入。该补验不包含
连续采样、补偿复算、光照响应、ADXL345 DATA_READY/轴向/振动响应或计量精度，
且 ADXL345 单模块 SPI 鲁棒性保持 `NOT_CLAIMED`，因此 SNS-01..03 仍为
`SNS-03` 保持 `NOT_RUN`；RS485-03 的后续实测保持 `FAIL`。SNS-01/SNS-02 补验通过后，
当前矩阵总计为 `18 PASS + 1 FAIL + 4 NOT_RUN + 1 NOT_CLAIMED`。

## Release blocker projection

| Blocker | Matrix rows | Current state |
|---|---|---|
| HW-001 sensors and watchdog | BSP-02, SNS-01..03, WDG-01 | OPEN; BSP-02 and WDG-01 passed, three sensor functional supplements remain |
| HW-002 RS485 and Project Three | RS485-03, P3-01 | OPEN |
| HW-003 CAN and physical dual bus | CAN-03, BUS-02 | OPEN; CAN-03 passed, BUS-02 remains NOT_RUN |
| HW-004 hardware soak | SOAK-02 | OPEN |

The software source and both clean-reproduction blockers are closed. `BSP-02`
now passes at its narrow board-admission layer and `WDG-01` passes at its
bounded reset-only layer, but `HW-001` remains open for three sensor functional
supplements. No other hardware blocker is closed.

## Public and local evidence

The public matrix points only to repository-relative, public-safe summaries.
It contains no `.private` path, user home, full device serial, credential or
private network identity.

Only the compact REPRO-002 JSON, manifest and report are projected publicly.
Raw command tails, the source archive, firmware images and build directories
remain ignored. The board supplement stays in the existing domain summaries;
no serial number or raw programmer log is retained.

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
