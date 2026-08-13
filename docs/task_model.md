# P5-S3-T01 task model

> Status: `TASK_MODEL_CANDIDATE + WAITING_FOR_HARDWARE`
> Content review: `FROZEN` (approved 2026-08-14)

| Task | Priority | Period | Deadline | Design budget | T01 service |
|---|---:|---:|---:|---:|---|
| `protocol_task` | 5 | 5 ms | 5 ms | 1 ms | bounded RS485 poll/smoke; no Modbus parser |
| `acquisition_task` | 4 | 20 ms | 20 ms | 2 ms | release/health skeleton; no sensor access |
| `can_task` | 3 | 100 ms | 100 ms | 2 ms | release/health skeleton; CAN remains stopped |
| `health_task` | 2 | 1000 ms | 1000 ms | 5 ms | aggregate task counters; no IWDG |
| `diagnostic_task` | 1 | 200 ms | 200 ms | 2 ms | bounded heartbeat and optional one-shot smoke |

Idle priority is 0. The periods, deadlines and budgets are scheduler design
contracts, not measured WCET, final sampling rates or protocol response limits.

Each task keeps an absolute release phase. A late cycle increments bounded
miss/deadline/budget counters and advances to the next future release without
busy-waiting or replaying every historical period. The HAL/FreeRTOS-free host
model tests normal releases, wrap-around, late wake, single/multi-period
overload and independent task state.

All five tasks currently use provisional 256-word static stacks. Linking proves
only that the buffers fit; P5-S3-T02 must establish actual stack and RAM budgets
from hardware watermark evidence.

NVIC uses `NVIC_PRIORITYGROUP_4`, with all four implemented priority bits used
as preemption priority. Existing USART1/DMA interrupt priority values remain
unchanged and those ISRs do not call FreeRTOS APIs in T01.
