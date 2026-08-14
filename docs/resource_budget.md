# P5-S3-T02 resource budget

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_RESOURCE_BUDGET`  
> Content status: `FROZEN` (approved 2026-08-14)
> Hardware watermark: `NOT_MEASURED`  
> Hardware status: `WAITING_FOR_HARDWARE`  
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

When the board is available, run the bounded scheduler smoke for about 10
seconds and query the five-task resource snapshot once. A task below 32 free
words is enlarged by 64 words and the smoke is repeated. Do not shrink a stack
from the empty-skeleton result. Sensor, Modbus, CAN, recovery and soak workloads
must remeasure their affected tasks in later stages.

No task watermark has been measured in this report.
