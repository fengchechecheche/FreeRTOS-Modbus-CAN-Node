# P5-S6-T04 dual-bus fault matrix

> Software status: `PASS_HOST + PASS_CROSS_BUILD`
> Hardware status: `PASS`
> Software content review: `FROZEN` (approved 2026-08-15)
> Hardware supplement content review: `PENDING`
> Baseline: `[033] d6b929b85a1e3918edc90c5b276fe9b81d2cf4e3`
> Hardware supplement source: `[063] f774d4c6d8d1a406a1a8807159383ff91447fdc6`
> Evidence policy: bounded final rows only; no per-tick trace

## Purpose and boundary

This matrix places the existing Modbus RTU stream, RS485 half-duplex state,
CAN latest-wins/event scheduler, CAN recovery controller, task model,
diagnostic admission and health policy on one deterministic Host timeline. It
does not introduce a production dual-bus simulator or change firmware runtime.

The Host fixture uses virtual ticks and fake UART/CAN operations. It proves
bounded software state transitions and cross-link progress, not physical
throughput, MCU scheduling latency, real recovery time, arbitration, ACK,
termination or deadline measurements.

## Fault matrix result

| ID | Injected condition | Compact result | Status |
|---|---|---|---|
| D01 | valid Modbus traffic with CAN periodic telemetry | 2 Modbus frames, 2 CAN commits, all five task models advanced twice | PASS_HOST |
| D02 | slow master plus one `>t1.5` internal gap | normal request idle accepted; 1 inter-character error; next valid frame accepted; CAN/acquisition advanced | PASS_HOST |
| D03 | bad CRC followed by a valid request | 1 CRC mismatch, no bad-frame delivery, next frame accepted, health task advanced | PASS_HOST |
| D04 | second RS485 send while active and TX timeout | second send `BUSY`; 25 ms virtual timeout; RX restored; CAN busy head retained and committed | PASS_HOST |
| D05 | CAN slow consumer/HAL busy | heartbeat replaced latest-wins; selected head unchanged across busy; pending returned to zero; Modbus progressed | PASS_HOST |
| D06 | eight CAN events, ninth overflow, diagnostic queue full | 11 bounded CAN commits; 1 counted CAN event drop; 1 counted diagnostic drop; health and BME pair delivered | PASS_HOST |
| D07 | CAN ACK failure/bus-off and failed recovery | 1000 ms virtual delays, 3 attempts, latched state; 3 Modbus frames and acquisition/health releases progressed | PASS_HOST |
| D08 | CRC/TX timeout overlapping CAN bus-off | both error classes retained; CAN recovered once; RS485 later completed; acquisition/CAN tasks advanced twice | PASS_HOST |

## Backpressure contract exercised

| Data class | Observed bounded behavior |
|---|---|
| ordinary CAN telemetry | one fixed slot per group; a newer value replaces an unsent older value and increments `telemetry_replaced` |
| BME CAN pair | primary and secondary retain one sequence; the test rejects a mixed pair |
| CAN state event | depth-8 FIFO; equal source/code may coalesce; overflow increments `event_dropped` |
| diagnostic event | zero-wait admission; full queue increments `event_dropped_full_count`; drain budget remains 2 |
| RS485 response | caller bytes copied into one fixed 256-byte buffer; an active transaction rejects a second send as `BUSY` |
| CAN HAL busy | selected frame/token remains pending and is retried without reordering the head |

Counted overflow is an accepted degraded result for deliberate saturation. It
is not silent loss: drop/high-water state remains observable and the event
burst limit still gives waiting health/periodic telemetry a send opportunity.

## Isolation invariants

Every scenario is finite and starts from a clean fixture. The matrix asserts:

- fixed-size pending storage and bounded loops;
- healthy-link progress when the other link is slow or failed;
- acquisition and health task-model progress during single and overlapping faults;
- no global reset or task deletion caused by a link fault;
- no unclassified CRC, timeout, busy, queue-full or bus-off result;
- no mixed `0x340/0x341` sequence;
- recovery of one link does not clear the other link's counters.

The task-model counters are contract observations. They do not show real
FreeRTOS execution, preemption or WCET.

## Reproduction

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure
./out/host-debug/p5_host_dual_bus_fault_matrix

cmake --preset host-release
cmake --build --preset host-release
ctest --preset host-release --output-on-failure
```

The full Host suites contain 21 tests after this task. CAN, Modbus and BSP
contract validators and both ARM presets remain separate regression gates.

## 2026-08-21 physical supplement

The physical supplement used the default firmware on NUCLEO-F446RE with the
Waveshare RS485 CAN Shield, the reference CH340 USB-RS485 adapter and a
candleLight/`gs_usb` CAN adapter. The default Debug ELF SHA-256 was
`bd72b55c84350d433aebb7c0eaee14705b3ae3422604f19c732d54fd2808f73f`.
RS485 remained at address 4 and `19200 8E1`; CAN used 500 kbit/s, Host sample
point `0.75`, common GND and the admitted termination. Device serials were not
recorded.

| Physical step | Bounded observation | Result |
|---|---|---|
| Single-route entry gates | RS485 H01～H07 passed 10/10; CAN observed all six periodic IDs while ERROR-ACTIVE | PASS |
| Normal dual-bus concurrency | RS485 H01～H07 passed 30/30 while six CAN windows captured 733 accepted frames across all six periodic IDs, with 0 rejected, 0 duplicate and no BME pair mismatch | PASS |
| RS485 peer disconnect/reconnect | During one approximately 10 s USB-RS485 disconnect, CAN accepted 128 frames across all six periodic IDs; heartbeat continued and the MCU did not reset. After reconnect, RS485 H01～H07 passed 10/10 | PASS |
| CAN peer down/recovery | During one 2 s `can0` software-down interval, RS485 H01～H07 passed 8/8; heartbeat continued and the MCU did not reset. After restoring 500 kbit/s/sample point `0.75`, CAN accepted 120 frames with no rejection/duplicate and the `0x540/0x541` diagnostic round trip passed | PASS |
| Final state | `can0` was ERROR-ACTIVE with Host warning/passive/bus-off/error counters at zero | PASS |

This closes `BUS-02` only for one bounded short-line bench run. It proves that
the healthy bus and heartbeat continued through one brief peer interruption and
that both routes recovered under the existing policy. It does not prove repeated
disconnect endurance, arbitrary outage duration, physical bus-off recovery,
measured recovery latency, MTBF, long-run stability or Project Three
interoperability. The offline D01～D08 matrix remains the evidence for deliberate
queue saturation and backpressure; the physical run did not inject queue
overflow.

## 2026-08-21 Gateway concurrent recheck

A later bounded recheck attached the reference CH340 USB-RS485 adapter and the
candleLight USB-CAN adapter to `Ubuntu-24.04-Gateway` at the same time. It used
Project Three profile `[039] 17d67873f83488b08ea0eee0fa28c8722b0913d6`,
Project Five source `[067] a173717deb0814019a51a73f59834b9c3c5fd309` and the
default Debug ELF SHA-256
`d076ddf743020fe1a043e776ba3196ea1f02153a17c5d98451cc722d6ac0018f`.
RS485 remained read-only at address 4 and `19200 8E1`; CAN remained at
500 kbit/s with Host sample point `0.75`, common GND and the USB-CAN `120R`
setting.

The first 125 s main run and a 25 s focused continuation completed 2060/2060
Project Three Modbus requests successfully across all 17 configured
input-register addresses. All six periodic CAN IDs continued to arrive. Four
ad-hoc Host `0x540` sends each obtained one matching `0x541`; one
error-warning transition was observed and did not grow during the focused
continuation. Those ad-hoc sends closed their raw CAN socket immediately after
receiving the response, so they are not accepted as the final Host diagnostic
procedure.

A later 10 minute replay kept Project Three Modbus polling active and completed
8291/8291 requests with zero failure. The first five minutes were CAN
receive-only and retained zero warning, passive and bus-off transitions. One
ad-hoc `0x540` sender then received the correct `0x541` and immediately closed
its socket; Host counters subsequently reached 8 error-warning, 33
error-passive and 210 bus-off transitions. The counters stopped increasing when
no further Host frame was sent, while periodic CAN RX and Modbus continued.

An early cold CAN-only A/B observation made short-lived Host sender closure a
plausible candidate trigger: one retained socket produced a clean response,
whereas one immediately closed TX socket was followed by 5678 error frames.
That observation was useful for forming a hypothesis, but it did not establish
causality.

The later authoritative T1-T4 lifecycle matrix tested four ten-minute modes:
one persistent socket, a persistent monitor plus a second persistent sender,
per-request immediate sender close, and per-request sender close delayed by
five seconds. All four modes could produce error frames or CAN state-counter
growth. Socket close is therefore not a necessary trigger, and the precise
lower-layer cause cannot be uniquely assigned to socket lifecycle,
candleLight, `gs_usb`, USB-IP, the STM32 firmware or any single component. The
full bounded evidence is recorded in `docs/can_socket_lifecycle_matrix.md`.

T5 then exercised the product-scope route for 600.011 s with active Modbus/RS485
polling and passive CAN telemetry only. It completed 8768 successful Modbus
requests with zero failure, received 610 frames for each of the six periodic CAN
IDs, observed zero dedicated CAN error frames and no warning/passive/bus-off
counter growth, and sent zero Host CAN frames. This closes the product-scope
concurrency check without claiming reliable Host-to-STM32 CAN application
traffic.

## 2026-08-22 Raspberry Pi 4B/ARM64 supplement

A later physical Raspberry Pi 4B run used Ubuntu 24.04.4 AArch64, Project Three
`[040] b28191e0e4a179bb9bcdb245a73d272a70a6c73b`, Project Five `[069]
2789d740e37da6cce4c641a7838fc28ad2b80b84`, the same default Debug ELF, the
reference CH340 adapter and candleLight at 500 kbit/s with Host sample point
`0.75`.

During the 208.691 s simultaneous run, the Raspberry Pi gateway completed
2860/2860 Modbus requests with zero failure while each of the six required CAN
IDs contributed 213 frames. The maximum observed periodic-frame gap was
0.982764 s. Dedicated error-frame count, CAN warning/passive/bus-off counter
deltas and RX-drop delta were all zero; Host CAN TX remained zero. The gateway
and both capture processes then stopped cleanly and `can0` was brought down.

The VCP startup and heartbeat were confirmed before the runs but were not logged
continuously. Success JSONL was summarized and deleted instead of being retained
as a large raw evidence package. This recheck does not add a Project Three CAN
consumer, validate production systemd/TLS deployment, repeat the prior
disconnect injections or close `SOAK-02`.

## Historical hardware follow-up contract

The original minimum physical-validation contract required the NUCLEO-F446RE path, USB-RS485 link, CAN
transceiver and candleLight path to pass their individual entry gates first.
The minimum follow-up is intentionally small:

1. run Modbus queries while observing CAN periodic frames for 2-3 minutes;
2. stop/disconnect the RS485 master briefly and confirm CAN/acquisition/health continue;
3. stop/disconnect the CAN peer briefly and confirm Modbus/acquisition continue;
4. restore both links and record whether the existing recovery or latched policy applies.

The 2026-08-21 supplement above executed this minimum contract. It intentionally
does not convert the brief injected outage durations into measured recovery-time,
throughput, deadline, stack-watermark or long-duration claims. Those trends
remain in P5-S6-T05.
