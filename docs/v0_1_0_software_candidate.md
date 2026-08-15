# v0.1.0 software candidate

> Intended version: `v0.1.0`
> Release state: `UNRELEASED`
> Candidate state: `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE`
> Current documentation baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Clean replay source: `[036] 15932a2ff7adecdfbe5355559926a95b0df25845`
> Evidence matrix: `24 = 12 PASS + 11 NOT_RUN + 1 NOT_CLAIMED`
> Hardware Release: `BLOCKED_WAITING_FOR_HARDWARE`
> Tag / remote Release: `ABSENT / NOT_RUN`

## Purpose

This document describes the software candidate that can be reviewed before the
NUCLEO board and physical buses are available. It is not final v0.1.0 release
notes and does not assert that firmware was flashed or operated on hardware.

The candidate keeps two identities separate. Commit `[036]` is the tracked-file
clean replay source recorded by [`reproduction_report.md`](reproduction_report.md).
Commit `[039]` is the current documentation and release-collateral baseline.
The product, build and test inputs did not change between them; `[039]` itself
is not described as clean-replayed.

## Included software scope

- STM32F446RE firmware source and STM32CubeMX/CubeF4 configuration provenance;
- statically allocated FreeRTOS task model and bounded ISR/task handoff;
- BME280, VEML7700 and ADXL345 software drivers and Host logic tests;
- Modbus RTU slave software contract at the frozen project address;
- classic CAN codec, filter, queue and recovery software candidate;
- Host dual-bus fault matrix, SocketCAN/vcan checks and bounded soak tooling;
- MIT project scope, third-party notices, learning route and public evidence matrix.

The detailed evidence ceiling for every capability remains
[`evidence_matrix.md`](evidence_matrix.md). This summary never upgrades a row.

## Current software validation

The T05 implementation reran the current `[039]` working tree without changing
product sources:

| Gate | Result | Meaning |
|---|---|---|
| Host Debug | 21/21 PASS | Host logic and contract regression only |
| Host Release | 21/21 PASS | Host logic and contract regression only |
| ARM Debug | PASS | configure, link and ELF/HEX/BIN/MAP generation |
| ARM Release | PASS | configure, link and ELF/HEX/BIN/MAP generation |
| Static resource gate | PASS | linked firmware remains within the frozen budget |
| Clean replay | PASS on `[036]` | no-cache/no-network procedure; identity is not rewritten to `[039]` |
| Bit-for-bit across clean paths | NOT_CLAIMED | linked FreeRTOS `__FILE__` strings remain path-dependent |

Build output under `out/` is ignored and is not part of the public candidate.
The retained public hashes remain the exact `[036]` identities in the T02
manifest; no new binary attachment is created in T05.

## Explicitly excluded

The following remain `NOT_RUN` and are not candidate accomplishments:

- ST-LINK flashing, reset, clock, GPIO and target startup;
- BME280, VEML7700 and ADXL345 identity, sampling and accuracy on real devices;
- physical watchdog reset and persistence;
- USB-RS485 exchange, physical timing and Project Three interoperability;
- candleLight, transceiver, physical CAN ACK/frames and real bus-off recovery;
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
