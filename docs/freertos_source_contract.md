# FreeRTOS source contract

> Status: `SOURCE_CANDIDATE_FROZEN + WAITING_FOR_HARDWARE`  
> Content review: `FROZEN` (approved 2026-08-14)  
> Task: P5-S3-T01

## Source and version

The repository vendors only the used subset of the FreeRTOS Kernel V10.3.1
shipped with STM32CubeF4 V1.28.3:

- kernel: `tasks.c`, `list.c`, `queue.c`;
- headers: the unmodified `Source/include/` directory;
- port: `portable/GCC/ARM_CM4F/port.c` and `portmacro.h`;
- notices: FreeRTOS `LICENSE` and ST `st_readme.txt`.

The third-party files are copied without formatting or copyright-header changes.
The absolute local package path is intentionally not a project dependency.

## Deliberate exclusions

CMSIS-RTOS wrappers, other compiler/architecture ports, `heap_x.c`, examples,
demos, and the unused co-routine, event-group, stream-buffer and timer sources
are not included. Project code uses the native FreeRTOS task API only.

The kernel MIT license and ST BSD-3-Clause integration notice do not decide the
project-level license, which remains `TBD_USER_REVIEW`.

## Configuration boundary

`config/FreeRTOSConfig.h` enables preemption, a 1 kHz 32-bit tick and static
allocation. Dynamic allocation, software timers, tickless idle and run-time
statistics are disabled. TIM6 owns the HAL 1 ms tick; the ARM_CM4F port owns
SysTick, PendSV and SVC.

An upgrade to another kernel version is a separate reviewed change. It must not
mix a new kernel with this frozen port or configuration without regression.
