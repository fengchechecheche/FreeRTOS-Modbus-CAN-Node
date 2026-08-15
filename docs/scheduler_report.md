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
- USART1/DMA direct notification wake with an absolute 5 ms timeout;
- finite heartbeat moved to `diagnostic_task`;
- one depth-8 static diagnostic event queue with a two-item drain budget;
- one zero-wait static mutex for short system-snapshot copies;
- one bounded health/feed policy plus reset-reason normalization;
- scheduler smoke defaults to `OFF`.

## Software verification

- BSP stable-fact checker and negative self-test;
- Host Debug/Release: 9/9 tests, including notification/mailbox, bounded
  ownership/backpressure injection, health transitions and reset records;
- Firmware Debug/Release: build and link;
- ELF symbol checks for scheduler start, kernel handlers, five task entries,
  `xTaskGenericNotifyFromISR()` and `xTaskNotifyWait()`;
- source-boundary checks for no heap, CMSIS task API, project-task
  `HAL_Delay()` or enabled-by-default scheduler smoke.

Final cross-build size summaries are:

| Build | text | data | bss |
|---|---:|---:|---:|
| Debug | 25192 B | 160 B | 9288 B |
| Release | 21908 B | 156 B | 9284 B |

These values include five separately named 256-word stacks. The static-only
contract, linker heap removal, limits and exact accounting are maintained in
`docs/resource_budget.md` rather than duplicated here.

## Deferred

No scheduler execution, task jitter, WCET, stack watermark, ISR latency,
queue/mutex stress or health transition has been measured because the board is
unavailable. The watermark API is compiled in, but its snapshot reports values
as unmeasured until called by a running task. IWDG is deliberately not
configured or armed, and reset-record persistence is not implemented. Hardware
acceptance remains a separate limited smoke after the BSP hardware gate; no
trace bundle is required for a normal pass.

## 2026-08-15 limited hardware supplement

The historical deferred statement above remains the record of the original
software-only stage. After board arrival, the existing scheduler smoke was
enabled temporarily on the reviewed working tree based on
`807f85ce43240e94fc4aea3bd07e31c40a81d236`.

The diagnostic ELF SHA-256 was:

```text
ffae6a617a1ce4189df9c6b13e6a46ffaf45f4c7f0c8fc05d20aba6e16063936
```

The ten-second VCP window contained exactly one `P5 S3 T01 SCHEDULER OK`, five
heartbeat lines and no assertion, HardFault, fault marker or reset loop. The
smoke option was then restored to `OFF`; the final default Debug ELF SHA-256 is:

```text
4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7
```

After a separate USB power cycle, Hot Plug inspection showed scheduler state
`running`, a non-null current TCB, heartbeat count 5, clock counter ready and
RTOS fault code 0. The board initially exposed two startup defects that were
fixed minimally: DWT access ordering and an unacknowledged-CAN LEC interrupt
storm. The latter fix retains warning, passive, bus-off and general error
notifications while avoiding one interrupt for every automatic retransmission.

This limited smoke proves scheduler start and bounded diagnostic-task progress
only. Task jitter, WCET, stack watermarks, ISR latency, queue/mutex stress,
IWDG behavior and long-duration recovery remain `NOT_RUN`/`NOT_MEASURED`.
