# P5-S4-T05 S4 software validation

> Software status: `READY_FOR_CONTENT_REVIEW + READY_FOR_HARDWARE`  
> S4 software handoff: `READY_FOR_S5_SOFTWARE_PLAN`  
> Hardware status: `WAITING_FOR_HARDWARE`  
> Physical sensor fault injection: `NOT_RUN`  
> 60-minute physical run: `NOT_RUN`  
> Runtime timing/watermark: `NOT_MEASURED`

## Acceptance boundary

This report closes only the deterministic S4 software combination gate. Three
real pure drivers run on one virtual clock with independent fake transports;
the result is not evidence of board wiring, real bus recovery, deadline/WCET,
jitter, watchdog feed or a 60-minute physical run. Modbus registers and CAN
messages remain `NOT_IMPLEMENTED`.

The production firmware contains no fault-injection switch. One
`acquisition_task` remains the runtime SPI1/I2C2 owner. A single sensor fault
may make health `DEGRADED`, but feed remains allowed and no global reset or
fail-stop is requested.

## Compact deterministic matrix

| Scenario | Injection/edge | Isolation and final result |
|---|---|---|
| healthy baseline | three normal transport scripts | four sources progress; monitor/health serviceable |
| BME timeout | persistent transport timeout after admission | BME fault episode; VEML/ADXL continue; no global reset |
| BME identity | BMP280/wrong identity | identity/offline classification; humidity not fabricated |
| VEML missing | not-present transport result | unavailable classification; BME/ADXL continue; recovery not fabricated |
| VEML configuration | configuration readback mismatch | configuration class; range warning is not disconnect |
| ADXL repeated IRQ | aggregated count greater than one | at most one frame read; extras enter dropped lower bound |
| ADXL timeout | data read timeout | one bounded fault episode; BME/VEML continue |
| wrap/recovery | sample/fault near `UINT32_MAX` | unsigned interval and recovery duration remain correct |
| persistent local fault | sensor masks nonzero | health `DEGRADED`, feed `ALLOWED`, no reset/fail-stop |
| queue bound | repeated policy transitions | drain budget 2; maximum pending does not exceed depth 8 |

Delayed-ready and exact stall boundaries remain covered by the individual
driver tests executed in the same 14-test suite; no wall-clock sleep or
60-minute virtual-success claim is used.

## Software results

| Check | Result |
|---|---|
| Host Debug | 14/14 PASS |
| Host Release | 14/14 PASS |
| BSP contract | 489 stable facts PASS |
| negative self-test | production injection, monitor loop and sensor-reset mutants rejected |
| Firmware Debug | PASS; text/data/bss `40648/160/11184` B |
| Firmware Release | PASS; text/data/bss `34064/156/11176` B |
| Resource contract | PASS; static-only, heap 0 |
| RAM delta from `[023]` | Debug +552 B; Release +544 B |

The existing newlib `_close/_lseek/_read/_write` nosys warnings remain and do
not fail linkage. There are no linked allocator definitions. No task, queue,
mutex, sensor driver, BSP, CubeMX file, protocol mapping or raw evidence buffer
was added by T05.

## Hardware follow-up

After S2 and each individual sensor admission, safely disconnect one powered-
off module at a time and record one before/fault/after summary. Then run all
three devices for 60 real minutes, retaining only start/end summaries and the
first diagnostic if a fault occurs. Until those checks are performed, all
physical results remain `NOT_RUN` and full S4 hardware closure is blocked.
