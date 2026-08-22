# P5-S3-T02 resource budget

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_RESOURCE_BUDGET`  
> Content status: `FROZEN` (approved 2026-08-14)
> Hardware watermark: `PASS_BARE_BOARD_LIMITED`
> Hardware status: `PARTIAL_PASS`
> Baseline: `2e25df3c65c62dfd7b212a144381f42f33d5f7ab` (`[ 013 ]`)

## Static allocation contract

The STM32F446RE contract is 512 KiB Flash and 128 KiB RAM. FreeRTOS dynamic
allocation is disabled, no `heap_x.c` is linked, and the linker heap reserve is
zero. Stack words are 4 bytes on the Cortex-M4F port.

| Object | Configured words | Bytes | Runtime watermark |
|---|---:|---:|---|
| `protocol_task` | 256 | 1024 | `NOT_MEASURED` |
| `acquisition_task` | 256 | 1024 | `NOT_MEASURED` |
| `can_task` | 256 | 1024 | `NOT_MEASURED` |
| `health_task` | 256 | 1024 | `NOT_MEASURED` |
| `diagnostic_task` | 256 | 1024 | `NOT_MEASURED` |
| idle task | 128 | 512 | not sampled by the T02 task snapshot |
| MSP/exception reserve | — | 1024 | linker contract only |
| linker heap reserve | — | 0 | static-only contract |

The five application task stacks total 5120 B. Application tasks, idle and MSP
explicit stack reserves total 6656 B. TCBs, HAL state and other global objects
remain part of the linked RAM result instead of being estimated twice.

## Debug and Release results

Toolchain: GNU Arm Embedded GCC 13.2.1, CMake 3.28.3, Ninja 1.11.1 and
Python 3.12.3.

| Build | text | data | bss | Flash `text + data` | Linked RAM `data + bss` |
|---|---:|---:|---:|---:|---:|
| Debug | 18688 B | 160 B | 8616 B | 18848 B / 3.59% | 8776 B / 6.70% |
| Release | 16372 B | 156 B | 8612 B | 16528 B / 3.15% | 8768 B / 6.69% |
| Debug − Release | +2316 B | +4 B | +4 B | +2320 B | +8 B |

GNU `size` includes the NOLOAD `._user_heap_stack` section in `bss`; therefore
the 1024 B MSP reserve is already included in linked RAM and is not added a
second time. Relative to `[ 013 ]`, Debug gains 328 B Flash and Release gains
296 B Flash for the resource snapshot path. Both builds reduce linked RAM by
432 B after removing the 512 B default heap reserve and adding task handles,
the in-memory snapshot and alignment.

The first-stage limits are intentionally loose: 384 KiB Flash and 96 KiB linked
RAM. Both builds pass. A Debug/Release difference is reported for diagnosis but
is not itself a failure.

### T03 notification regression

After enabling task notifications and adding the fixed RS485 IRQ mailbox, the
T03 default-OFF build remains inside the same limits:

| Build | text | data | bss | Flash | Linked RAM | Change from T02 |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 20136 B | 160 B | 8728 B | 20296 B | 8888 B | Flash +1448 B, RAM +112 B |
| Release | 17608 B | 156 B | 8724 B | 17764 B | 8880 B | Flash +1236 B, RAM +112 B |

The RAM increment covers notification fields in the five static TCBs plus the
fixed mailbox, counters and latency summary. No queue, semaphore, dynamic heap
or event trace buffer was added. Runtime task watermarks and ISR latency remain
`NOT_MEASURED` until hardware execution.

### T04 queue and mutex regression

T04 adds one 96 B by-value event storage area, one `StaticQueue_t`, one
`StaticSemaphore_t`, transport counters and the mutex-related kernel state. The
five task stacks are unchanged.

| Build | text | data | bss | Flash | Linked RAM | Change from T03 |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 23272 B | 160 B | 9056 B | 23432 B | 9216 B | Flash +3136 B, RAM +328 B |
| Release | 20360 B | 156 B | 9052 B | 20516 B | 9208 B | Flash +2752 B, RAM +328 B |

Both builds remain far below the existing 384 KiB Flash and 96 KiB linked-RAM
limits. The increase is accepted without reducing any provisional task stack.
The diagnostic queue item area is fixed at 8 × 12 B. Bare-board queue maximum
pending and mutex-contention measurements are now zero; workload stress remains
deferred.

### T05 health and recovery regression

T05 adds the HAL-free health policy, normalized reset-reason codec, fixed reset
record schema, per-task progress snapshot and one transition-only diagnostic
event. It does not add an IWDG instance, persistent storage or a periodic trace.

| Build | text | data | bss | Flash | Linked RAM | Change from T04 |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 25192 B | 160 B | 9288 B | 25352 B | 9448 B | Flash +1920 B, RAM +232 B |
| Release | 21908 B | 156 B | 9284 B | 22064 B | 9440 B | Flash +1548 B, RAM +232 B |

Both builds remain inside the unchanged 384 KiB Flash and 96 KiB linked-RAM
limits. The RAM increase holds health progress, policy and reset metadata; the
five task stacks and queue depth are unchanged. Hardware feed timing, reset
retention and recovery behavior remain `NOT_RUN`.

## Static-only checks

- `configSUPPORT_STATIC_ALLOCATION = 1`;
- `configSUPPORT_DYNAMIC_ALLOCATION = 0`;
- task and idle task creation use static buffers;
- linker `_Min_Heap_Size = 0` and `_Min_Stack_Size = 1024`;
- project-owned and generated `app/`, `bsp/`, `config/` and `Core/` contain no
  allocator calls;
- final Debug/Release ELF files contain no `malloc`, `free`, `pvPortMalloc` or
  `vPortFree` definitions;
- stack overflow checking remains level 2;
- `health_task` samples `uxTaskGetStackHighWaterMark()` once per second into a
  read-only snapshot with an explicit `measured` flag; it does not print or
  persist a periodic log.

`configUSE_MALLOC_FAILED_HOOK` remains disabled because this static-only build
has no valid FreeRTOS allocation path. If dynamic allocation is introduced
later, heap policy and the failure hook require a separate review.

## Reproduction

After the normal Host and firmware Debug/Release builds, run:

```bash
python3 tools/check_resource_budget.py \
  --source-root . \
  --debug-elf out/firmware-debug/freertos_modbus_can_node.elf \
  --debug-map out/firmware-debug/freertos_modbus_can_node.map \
  --release-elf out/firmware-release/freertos_modbus_can_node.elf \
  --release-map out/firmware-release/freertos_modbus_can_node.map
```

Normal acceptance keeps only this compact summary. ELF, MAP and raw symbol
dumps remain ignored build products. If a limit fails, inspect the MAP and
largest symbols only until the unexpected object or library is found.

## Hardware follow-up

The initial bare-board snapshot is now complete. A task below 32 free words
would still be enlarged by 64 words and retested; none crossed that threshold.
Do not shrink a stack from this light-load result. Sensor, Modbus, CAN, recovery
and soak workloads must remeasure their affected tasks in later stages.

## S4-T01 BME280 regression

T01 adds the pure BME280 calibration/compensation/state machine, one fixed
instance and owner-local snapshot, plus bounded SPI block read/write support.
Host oracle data is excluded from firmware; no task, stack, queue or mutex was
added.

| Build | text | data | bss | Flash | Linked RAM | Change from T05 |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 30516 B | 160 B | 9504 B | 30676 B | 9664 B | Flash +5324 B, RAM +216 B |
| Release | 26196 B | 156 B | 9500 B | 26352 B | 9656 B | Flash +4288 B, RAM +216 B |

Both builds pass the unchanged 384 KiB Flash and 96 KiB linked-RAM gates.
The BME280 workload watermark and SPI WCET remain `NOT_MEASURED` until the
sensor is available.

## S4-T02 VEML7700 regression

T02 adds the pure VEML7700 word/config/range state machine, one fixed instance
and owner-local snapshot, plus bounded I²C register write support. It adds no
task, stack, queue, mutex, dynamic heap or periodic evidence buffer.

| Build | text | data | bss | Flash | Linked RAM | Change from `[019]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 34684 B | 160 B | 9632 B | 34844 B | 9792 B | Flash +4168 B, RAM +128 B |
| Release | 29408 B | 156 B | 9628 B | 29564 B | 9784 B | Flash +3212 B, RAM +128 B |

Both builds pass the unchanged 384 KiB Flash and 96 KiB linked-RAM gates. The
five application stacks and queue depth remain unchanged. The combined sensor
SPI/I²C workload watermark and WCET remain `NOT_MEASURED`.

## S4-T03 ADXL345 regression

T03 adds one pure ADXL345 state machine, fixed app/IRQ state, a 100-sample
streaming accumulator and the latest feature. It does not store 100 raw samples
and adds no task, stack, queue, mutex, semaphore, DMA, dynamic heap or trace.

| Build | text | data | bss | Flash | Linked RAM | Change from `[020]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 38024 B | 160 B | 10032 B | 38184 B | 10192 B | Flash +3340 B, RAM +400 B |
| Release | 31960 B | 156 B | 10032 B | 32116 B | 10188 B | Flash +2552 B, RAM +404 B |

The RAM increment holds the driver, owner-local snapshot, window sums/squares,
latest feature and compact IRQ counters. Both builds remain far below the
unchanged 384 KiB Flash and 96 KiB linked-RAM gates. All five application
stacks remain 256 words and linker heap remains zero. Sensor-workload stack
watermark, IRQ latency and combined bus WCET remain `NOT_MEASURED`.

## S4-T04 unified measurement regression

T04 adds one owner model, one shared base snapshot, fixed owner inputs and a
17-field descriptor table. It reuses the existing zero-wait snapshot mutex and
adds no task, stack, queue, mutex, semaphore, DMA, heap or history buffer.

| Build | text | data | bss | Flash | Linked RAM | Change from `[022]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 39304 B | 160 B | 10632 B | 39464 B | 10792 B | Flash +1280 B, RAM +600 B |
| Release | 32992 B | 156 B | 10632 B | 33148 B | 10788 B | Flash +1032 B, RAM +600 B |

The 600 B RAM increment remains below the T04 1 KiB review threshold. Owner
inputs are static so the three driver snapshots are not simultaneously placed
on the 256-word acquisition stack. The five application stacks, queue depth 8,
linker heap 0 and 384 KiB/96 KiB stage gates remain unchanged. Sensor-workload
stack watermark, snapshot contention and real timestamp jitter remain
`NOT_MEASURED`.

## S4-T05 sensor monitor regression

T05 adds one pure fixed-size monitor model and one published snapshot plus
three health masks. It adds no task, stack, queue, mutex, heap or raw history.

| Build | text | data | bss | Flash | Linked RAM | Change from `[023]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 40648 B | 160 B | 11184 B | 40808 B | 11344 B | Flash +1344 B, RAM +552 B |
| Release | 34064 B | 156 B | 11176 B | 34220 B | 11332 B | Flash +1072 B, RAM +544 B |

Both builds pass the unchanged 384 KiB Flash and 96 KiB linked-RAM gates. The
RAM changes stay below the 1 KiB T05 review threshold. The five 256-word task
stacks, queue depth 8, one snapshot mutex and linker heap 0 are unchanged.
Sensor-workload stack watermark, combined bus WCET and real sample interval
envelope remain `NOT_MEASURED`.

## S5-T03 Modbus stream and RS485 transport regression

T03 adds no task, stack, queue, mutex, heap or raw-frame history. The default
path links a 256 B controller TX copy, 256 B RTU stream buffer and two 64 B RX
chunk buffers. Legacy smoke storage is not retained by the default call graph.

| Build | text | data | bss | Flash | Linked RAM | Change from A0 `[026]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 42252 B | 160 B | 11648 B | 42412 B | 11808 B | Flash +1604 B, RAM +464 B |
| Release | 35400 B | 156 B | 11648 B | 35556 B | 11804 B | Flash +1336 B, RAM +472 B |

Both RAM deltas are below the T03 768 B review threshold and the unchanged
96 KiB linked-RAM gate. Runtime stack watermark and physical timing remain
`NOT_MEASURED`.

## S5-T04 function server and register image regression

T04 adds one HAL/RTOS-free function server, one 122-register input image, four
holding registers and a 256 B response buffer. The protocol task remains the
only server/configuration owner. No task, stack, queue, mutex, heap or frame
history is added; the existing snapshot mutex only protects one aggregate copy.

| Build | text | data | bss | Flash | Linked RAM | Change from A0 `[027]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 45400 B | 160 B | 12560 B | 45560 B | 12720 B | Flash +3148 B, RAM +912 B |
| Release | 37684 B | 156 B | 12560 B | 37840 B | 12716 B | Flash +2284 B, RAM +912 B |

Both builds pass the unchanged 384 KiB Flash and 96 KiB linked-RAM stage
gates. The 912 B RAM delta is bounded by the response/image/server storage and
remains below the T04 1 KiB review threshold. The five 256-word task stacks,
queue depth 8, one snapshot mutex and linker heap 0 are unchanged. The legacy
RS485 loopback smoke also links with the function-server path excluded from its
call graph, then the build option is restored to default `OFF`. Runtime stack
watermark, 249 B response timing, address migration on a real master and
Project Three integration remain `NOT_MEASURED`.

## S6-T02 bxCAN runtime regression

T02 reuses the existing 256-word `can_task` stack and adds no task, RTOS queue,
mutex, semaphore, DMA or dynamic heap. Fixed CAN storage is 660 B: 160 B for the
IRQ mailbox/notifier and 500 B for the TX scheduler, controller, read-only
snapshot and control words. The scheduler holds eight coalescing event slots and
five latest-wins telemetry groups; the RX ring holds four frames.

| Build | text | data | bss | Flash | Linked RAM | Change from A1 `[031]` |
|---|---:|---:|---:|---:|---:|---:|
| Debug | 53272 B | 160 B | 13224 B | 53432 B | 13384 B | Flash +7152 B, RAM +664 B |
| Release | 44404 B | 156 B | 13216 B | 44560 B | 13372 B | Flash +6112 B, RAM +656 B |

The unchanged gates remain 384 KiB Flash and 96 KiB linked RAM. Physical CAN
load, ISR-to-task latency, task stack watermark and recovery timing remain
`NOT_MEASURED` until hardware is available.

## Dedicated CAN diagnostic extension

The always-on, read-only `0x540/0x541` diagnostic adds no task, RTOS object,
dynamic allocation or stack allocation. Exact fixed CAN storage is now 836 B:
160 B for the IRQ mailbox/notifier and 676 B for the TX scheduler, controller,
runtime snapshot/control, 28 B responder state and 112 B bounded VCP report
buffer. This is a 176 B increase over the historical S6-T02 660 B baseline.

The TX scheduler is 324 B, the runtime snapshot is 140 B, and the response path
has exactly one pending frame. Host static assertions bind these values; the
384 KiB Flash, 96 KiB linked-RAM, five 256-word task stacks and linker-heap-zero
gates remain unchanged. Physical stack watermark under a diagnostic request is
still `NOT_MEASURED` and must not be inferred from the static size check.

| Build | text | data | bss | Flash | Linked RAM |
|---|---:|---:|---:|---:|---:|
| Debug | 55272 B | 160 B | 13480 B | 55432 B | 13640 B |
| Release | 46072 B | 156 B | 13472 B | 46228 B | 13628 B |

## S6-T05 soak-runner regression

T05 adds only a Host-side standard-library Python runner and documentation. It
does not change the default firmware, task set, stacks, queues, mutexes, static
objects or linker scripts. The `[034]` resource values therefore remain:

| Build | text | data | bss | Flash | Linked RAM |
|---|---:|---:|---:|---:|---:|
| Debug | 53272 B | 160 B | 13224 B | 53432 B | 13384 B |
| Release | 44404 B | 156 B | 13216 B | 44560 B | 13372 B |

FreeRTOS dynamic allocation is disabled, no `heap_x.c` is linked and linker
heap reserve remains zero. The soak schema records this static heap policy
instead of inventing a free-heap trend. Workload queue pressure and runtime
timing remain `NOT_MEASURED` until the relevant hardware is available.

## P5-HW-OBS-01 default-OFF diagnostic budget

`P5_SOAK_DIAGNOSTIC` adds no task, queue, mutex, heap or CubeMX resource. The
existing diagnostic task keeps its 256-word static stack and 200 ms deadline;
its execution budget is 100 ms only in the diagnostic build, versus 2 ms when
the option is OFF. Snapshot scratch storage is diagnostic-only static BSS. A
bounded integer writer replaces the original large-varargs `snprintf` call, so
the task stack is not enlarged merely to accommodate formatting. The larger
execution budget bounds one formatter call and one blocking USART2 write;
actual runtime watermarks and timing remain pending a new 10-minute hardware
admission.

| Build | Option | text | data | bss | Flash | Linked RAM |
|---|---|---:|---:|---:|---:|---:|
| Debug | OFF | 55484 B | 160 B | 13480 B | 55644 B | 13640 B |
| Debug | ON | 58204 B | 160 B | 15752 B | 58364 B | 15912 B |
| Release | OFF | 46152 B | 156 B | 13472 B | 46308 B | 13628 B |
| Release | ON | 48516 B | 156 B | 15752 B | 48672 B | 15908 B |

The diagnostic delta is 2720/0/2272 B in Debug and 2364/0/2280 B in Release.
The BSS increase is the deliberate bounded replacement for task-stack scratch;
both variants remain far below 384 KiB Flash and 96 KiB linked RAM. Debug ARM
disassembly shows 128 B of explicit stack in the diagnostic service and 72 B
in the formatter entry, versus the rejected 1252 B and 540 B frames. The OFF
build remains byte-for-byte at the recorded resource totals and has no
`P5DIAG1` runtime path.
Source-line changes may still alter a Debug ELF hash, so identity is always
bound to the exact post-submit ELF rather than inferred from a historical hash.

## 2026-08-15 bare-board resource and IWDG supplement

The final default Debug build reports `text/data/bss = 53992/160/13280` B and
keeps all five 256-word stacks, queue depth 8, one snapshot mutex and zero
linker heap. A separate 32 B `.noinit` reset record is retained across the
tested software/IWDG reset path; it is not a dynamic allocation or a power-loss
retention claim.

Two Hot Plug snapshots taken ten seconds apart reported:

| Task | Configured words | Minimum free words | Gate |
|---|---:|---:|---|
| protocol | 256 | 215 | PASS |
| acquisition | 256 | 168 | PASS |
| CAN | 256 | 115 | PASS |
| health | 256 | 53 | PASS |
| diagnostic | 256 | 215 | PASS |

All values stayed unchanged and exceed the 32-word threshold. Queue maximum
pending and transport drop/contention counters remained zero. The acquisition
task had one historical miss/deadline/budget event, but those counters did not
increase in the second read; this is recorded for troubleshooting and is not a
WCET or deadline guarantee. No stack was resized and no periodic trace was
introduced.

## P5-HW-OBS-01 sustained hardware watermark supplement

The Raspberry Pi-hosted formal eight-hour diagnostic run measured the five task
minimum-free watermarks as `190/167/118/52/153` words for
protocol/acquisition/CAN/health/diagnostic. The protocol watermark changed once
from 214 to 190 words during startup and then stayed at 190 in every later
window. With a configured 256-word protocol stack, the measured steady-state
historical maximum usage is therefore 66 words (264 B), leaving 190 words
(760 B, 74.2%) free and 158 words above the 32-word acceptance gate.

This metric is the FreeRTOS minimum-ever-free watermark, not instantaneous stack
occupancy. The one-time decrease records a deeper path first exercised after
the initial observation; it is not evidence that 96 B remained allocated. The
best-supported trigger is the first complete RS485/Modbus receive and response
path, which includes a 64 B task-local receive chunk and nested stream/server
calls. The watermark alone cannot uniquely identify one function as the source
of all 96 B.

No stack resize or buffer-ownership refactor is required by this result. Keep
190 words as the measured steady-state planning value and retain 214 words only
as the cold-start observation. A decrease confined to the first evaluation
window is accepted only when every later window remains at or above the settled
value; a later decrease remains `REVIEW_REQUIRED`. Source remediation is
triggered only by continued steady-state decline, approach to the 32-word gate,
or a stack-overflow hook, HardFault, reset or task stall. Exact call-site work,
if later required, uses default-OFF watermark checkpoints or compiler
`-fstack-usage` analysis rather than increasing the stack pre-emptively. An
optional no-source-change GDB route is documented in `docs/soak_trend_report.md`:
first compare no-request startup against one complete 122-register Modbus read,
then, only if exact attribution is still useful, place a DWT write watchpoint at
the measured `0xA5` fill boundary. This debug-only experiment is not a release
or soak acceptance gate and must not be used for timing claims.
