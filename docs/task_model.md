# P5-S3-T01 task model

> Status: `TASK_MODEL_CANDIDATE + WAITING_FOR_HARDWARE`
> Content review: `FROZEN` (approved 2026-08-14)

| Task | Priority | Period | Deadline | Design budget | T01 service |
|---|---:|---:|---:|---:|---|
| `protocol_task` | 5 | 5 ms | 5 ms | 1 ms | bounded RS485 poll/smoke; no Modbus parser |
| `acquisition_task` | 4 | 20 ms | 20 ms | 2 ms | advance BME280 service once; at most one SPI transaction |
| `can_task` | 3 | 100 ms | 100 ms | 2 ms | release/health skeleton; CAN remains stopped |
| `health_task` | 2 | 1000 ms | 1000 ms | 5 ms | per-task progress and bounded feed decision; IWDG not armed |
| `diagnostic_task` | 1 | 200 ms | 200 ms | 2 ms | drain at most 2 diagnostic events, then bounded heartbeat/smoke |

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

P5-S3-T04 adds one depth-8 static diagnostic event queue. Task producers use a
zero-tick send and drop-new accounting; `diagnostic_task` consumes at most two
items per release, so event traffic cannot create an unbounded low-priority
loop. Health, resource and IRQ-latency snapshots use a zero-wait static mutex
only while copying complete values. A failed lock returns an explicit
unavailable result and increments a contention counter.

`acquisition_task` is the only runtime owner of SPI1 and I²C2. P5-S4-T01 now
advances the BME280 forced-mode state machine once per 20 ms release, with at
most one 5 ms-timeout SPI transaction. Its owner-local sample is not yet the
cross-sensor freshness schema; protocol, CAN and health tasks must not call
sensor or bus APIs directly. The scheduler-before boot probe remains the only
current exception.

P5-S3-T05 makes `health_task` the sole owner of the health-policy decision. It
checks progress from `protocol_task`, `acquisition_task`, `can_task` and
`diagnostic_task`; it deliberately does not require its own counter to advance.
One no-progress epoch enters `DEGRADED` while feed remains allowed. Two
consecutive no-progress epochs enter `RESET_REQUIRED` and withhold feed.
Warning counters alone do not request reset. Recovery is bounded to three
attempts per episode and never deletes a task. The current firmware only
computes this decision: no IWDG is configured, armed or refreshed.
