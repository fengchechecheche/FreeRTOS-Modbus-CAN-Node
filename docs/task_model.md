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

P5-S3-T02 gives each task a separately named 256-word static stack. These are
explicit candidate allocations rather than a shared provisional constant.
Linked RAM/Flash limits and a read-only watermark snapshot are documented in
`docs/resource_budget.md`. Runtime watermarks remain `NOT_MEASURED` until the
bounded hardware smoke, and later real workloads must remeasure affected tasks.

NVIC uses `NVIC_PRIORITYGROUP_4`, with all four implemented priority bits used
as preemption priority. P5-S3-T03 sets USART1, DMA2 Stream2 and DMA2 Stream7 to
priority 6/subpriority 0 and lets their callbacks wake `protocol_task` with a
direct task notification. The notification only wakes the task; frame copy,
DMA rearm, state transitions and error recovery stay in task context.

`protocol_task` now waits for either a notification or the next absolute 5 ms
release. Every event drain is followed by another absolute-release check, so a
notification storm cannot move the timeout/poll deadline. The other four tasks
retain the original `vTaskDelayUntil()` loop.
