# v0.1.0 software candidate

> Intended version: `v0.1.0`
> Release state: `UNRELEASED`
> Candidate state: `BOUNDED_HARDWARE_CANDIDATE_READY_FOR_RELEASE_REVIEW`
> Original T05 documentation baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Current software-test baseline: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Clean replay source: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Evidence matrix: `24 = 23 PASS + 0 FAIL + 0 NOT_RUN + 1 NOT_CLAIMED`
> Hardware Release evidence: `PASS_BOUNDED_HARDWARE_EVIDENCE`
> Tag / remote Release: `ABSENT / NOT_AUTHORIZED`

## Purpose

This document describes the current source candidate plus its separately
bounded board, sensor, RS485, CAN, Project Three and soak evidence. It is not
final v0.1.0 release notes and does not authorize a tag, binary attachment,
remote Release or publication.

The candidate keeps its identities separate. `[036]`/`[037]` remain the
historical T02 source/evidence pair, `[039]` is the original T05 documentation
baseline, `[042]` adds the Host PTY path, `[044]` records bounded board
admission, and `[046]` records the watchdog supplement. Committed `[047]` is
the current tracked-file clean replay source recorded by
[`reproduction_report_repro_002.md`](reproduction_report_repro_002.md).

## Included software scope

- STM32F446RE firmware source and STM32CubeMX/CubeF4 configuration provenance;
- statically allocated FreeRTOS task model and bounded ISR/task handoff;
- BME280, VEML7700 and ADXL345 software drivers and Host logic tests;
- Modbus RTU slave software contract and committed production-C Host PTY matrix at the frozen project address;
- classic CAN codec, filter, queue and recovery software candidate;
- Host dual-bus fault matrix, SocketCAN/vcan checks and bounded soak tooling;
- MIT project scope, third-party notices, learning route and public evidence matrix.

The detailed evidence ceiling for every capability remains
[`evidence_matrix.md`](evidence_matrix.md). This summary never upgrades a row.

## Current software validation

The current Host suite includes the PTY, board-support and IWDG software
supplements:

| Gate | Result | Meaning |
|---|---|---|
| Host Debug | 22/22 PASS | includes the production-C PTY matrix; Host only |
| Host Release | 22/22 PASS | includes the production-C PTY matrix; Host only |
| ARM Debug | PASS | configure, link and ELF/HEX/BIN/MAP generation |
| ARM Release | PASS | configure, link and ELF/HEX/BIN/MAP generation |
| Static resource gate | PASS | linked firmware remains within the frozen budget |
| Clean replay | PASS on `[047]` | tracked archive, no cache and no network; 15 bounded commands passed |
| Bit-for-bit across clean paths | NOT_CLAIMED | linked FreeRTOS `__FILE__` strings remain path-dependent |

Build output under `out/` is ignored and is not part of the public candidate.
The current Release BIN identity is
`8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c`;
the manifest records hashes but does not attach firmware binaries.

## Bounded hardware evidence

- `BSP-02`, `SNS-01..03` and `WDG-01` passed within the documented board,
  three-sensor, bounded-polling and reset-only routes;
- `RS485-03` and `P3-01` passed fixed-address-4 physical Modbus plus bounded
  read-only JSONL/RESET/local-MQTT interoperability;
- `CAN-03` and `BUS-02` passed the admitted common-ground candleLight route and
  one bounded physical dual-bus interruption/recovery run;
- `SOAK-02` passed a hash-bound diagnostic-firmware formal eight-hour run and
  a separate restored-default approximately ten-minute regression.

These rows close the four bounded hardware evidence blockers. They do not
authorize publication or expand the explicit exclusions below.

## Explicitly excluded

The following remain outside the candidate claim despite all matrix execution
rows having completed:

- sensor metrology/calibration accuracy, physical ADXL345 INT1/INT2, exact
  sampling rate and standalone-module SPI robustness;
- valid Modbus address migration H08/H09, multiple production slaves,
  production MQTT and repeated RS485 fault endurance;
- arbitrary CAN-adapter interoperability, Host-active production commands,
  repeated physical bus-off recovery and arbitrary interruption duration;
- MTBF, production-grade reliability, functional safety and unrestricted fault
  recovery;
- bit-for-bit BIN/HEX equality across differently named clean source paths.

The historical 60-minute pre-run remains `REVIEW_REQUIRED`. The accepted soak
claim is specifically the diagnostic-firmware eight-hour run plus the separate
default-firmware approximately ten-minute regression.

Use [`demo_guide.md`](demo_guide.md) for the software route and reviewed bounded
hardware evidence order. Use
[`recruitment_claim_ledger.md`](recruitment_claim_ledger.md) for bounded public
wording.

## Distribution and licensing

Project-owned material is MIT-licensed by `fengchechecheche`; vendor and
third-party paths retain their own terms. A source distribution must retain
[`../LICENSE`](../LICENSE), [`../THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md)
and referenced component license files.

T05 does not create a source archive, firmware bundle, tag, pull request or
remote Release. It also does not publish the candidate wording to a CV,
portfolio or recruitment platform.

## Upgrade gate for final v0.1.0

Final release notes may be created only after all of the following are true:

1. `HW-001` through `HW-004` are closed with reviewed physical evidence;
2. the evidence matrix is updated without promoting unexecuted rows;
3. the exact proposed release commit completes a fresh clean replay;
4. selected binaries and notices receive a release manifest;
5. the user separately authorizes the tag and remote Release.

Until then the only valid state is:

```text
UNRELEASED + BOUNDED_HARDWARE_CANDIDATE_READY_FOR_RELEASE_REVIEW
```
