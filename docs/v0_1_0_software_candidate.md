# v0.1.0 software candidate

> Intended version: `v0.1.0`
> Release state: `UNRELEASED`
> Candidate state: `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE`
> Original T05 documentation baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Current software-test baseline: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Clean replay source: `[047] 26411d2b627fd67654479f5a97a2066e47deafb5`
> Evidence matrix: `24 = 20 PASS + 0 FAIL + 3 NOT_RUN + 1 NOT_CLAIMED`
> Hardware Release: `BLOCKED_WAITING_FOR_HARDWARE`
> Tag / remote Release: `ABSENT / NOT_RUN`

## Purpose

This document describes the current software candidate plus its separately
bounded bare-board evidence. It is not final v0.1.0 release notes and does not
promote the remaining sensor, Project Three interoperability or soak gaps.

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

## Bounded hardware evidence already available

- `BSP-02`: ST-LINK, default Debug flash/verify/reset, VCP boot, runtime clock,
  GPIO register state, limited scheduler smoke and one power cycle passed;
- `WDG-01`: normal health feed, one controlled IWDG reset, reset-reason decode
  and reset-only `.noinit` retention passed.

These results are not a complete hardware Release and do not imply sensor or
physical-bus operation.

## Explicitly excluded

The following remain incomplete and are not candidate accomplishments:

- BME280 bounded sampling/reset and VEML7700 light-response/range checks passed without metrology claims; ADXL345 interrupt/axis checks remain `NOT_RUN`;
- a reference CH340 USB-RS485 passed the fixed-address-4 physical H01～H07 matrix, one post-reset H01 and one post-reconnect H01; valid address migration H08/H09 and Project Three interoperability remain `NOT_RUN`;
- the admitted common-GND candleLight route passed periodic telemetry, physical ACK in both directions and one bounded read-only `0x540/0x541` application round trip at 500 kbit/s with Host sample point `0.75`; arbitrary-adapter interoperability, physical bus-off recovery and simultaneous RS485+CAN remain unclaimed;
- simultaneous physical RS485/CAN behavior;
- the 10-minute, 60-minute and formal 8-hour hardware sessions.

Use [`demo_guide.md`](demo_guide.md) for the software-only demonstration and
the deferred hardware order. Use
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
UNRELEASED + SOFTWARE_CANDIDATE_READY_FOR_HARDWARE
```
