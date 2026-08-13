# P5-S3-T01 scheduler report

> Software status: `PASS_HOST + PASS_CROSS_BUILD`  
> Content status: `CONTENT_FROZEN` (approved 2026-08-14)  
> Hardware status: `WAITING_FOR_HARDWARE`

## Implemented

- native FreeRTOS Kernel V10.3.1 and GCC ARM_CM4F port;
- static allocation only; no heap implementation;
- TIM6 for HAL timebase and SysTick/PendSV/SVC for the kernel;
- five static skeleton tasks with absolute releases;
- RS485 poll moved to `protocol_task`;
- finite heartbeat moved to `diagnostic_task`;
- scheduler smoke defaults to `OFF`.

## Software verification

- BSP stable-fact checker and negative self-test;
- Host Debug/Release: 5/5 tests;
- Firmware Debug/Release: build and link;
- ELF symbol checks for scheduler start, kernel handlers and five task entries;
- source-boundary checks for no heap, CMSIS task API, project-task
  `HAL_Delay()` or enabled-by-default scheduler smoke.

Final cross-build size summaries are:

| Build | text | data | bss |
|---|---:|---:|---:|
| Debug | 18688 B | 160 B | 8616 B |
| Release | 16372 B | 156 B | 8612 B |

These values include five separately named 256-word stacks. The static-only
contract, linker heap removal, limits and exact accounting are maintained in
`docs/resource_budget.md` rather than duplicated here.

## Deferred

No scheduler execution, task jitter, WCET, stack watermark or ISR latency has
been measured because the board is unavailable. The watermark API is compiled
in, but its snapshot reports values as unmeasured until called by a running
task. Hardware acceptance remains a separate limited smoke after the BSP
hardware gate; no trace bundle is required for a normal pass.
