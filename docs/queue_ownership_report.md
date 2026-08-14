# P5-S3-T04 queue, mutex and ownership report

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`  
> Content status: `READY_FOR_CONTENT_REVIEW`  
> Runtime queue/mutex stress: `NOT_RUN`  
> Hardware status: `WAITING_FOR_HARDWARE`  
> Input baseline: `d8bf2eb0fe95f0e76fed13b275bea99635e691ea` (`[ 016 ]`)

## Primitive selection

| Data path | Selected primitive | Current boundary |
|---|---|---|
| USART1/DMA ISR event | T03 notification plus fixed mailbox | unchanged; ISR does not use the new queue |
| task diagnostic event | one static FreeRTOS queue | 8 items, 12 B by-value records |
| system/measurement state | latest snapshot plus short mutex copy | system snapshots implemented; measurement schema deferred to S4 |
| future Modbus command | separate static command queue | reject-new policy frozen; schema and instance deferred to S5 |
| SPI1 and I²C2 | single runtime owner | `acquisition_task`; no bus mutex |

The primitives are intentionally separate. A wake-up bit, an ordered event, a
latest-value snapshot and a command have different loss and ownership rules and
are not routed through a universal message channel.

## Diagnostic event queue

The event item contains only `timestamp_ms`, `detail`, `source` and `code`.
Its size is fixed at 12 B and it contains no pointer, DMA-buffer view or HAL
handle. The queue uses `StaticQueue_t`, 96 B item storage and
`xQueueCreateStatic()`; dynamic allocation remains disabled.

- depth: 8;
- producer wait: 0 ticks;
- full policy: drop the new event and saturate `event_dropped_full_count`;
- consumer: `diagnostic_task`;
- drain limit: at most 2 events per 200 ms release;
- normal action: discard after bounded accounting; no serial log or persistence;
- ISR use: forbidden; T03 task notification remains the ISR path.

No T04 business event code is fabricated. Until T05 publishes real health or
recovery events, the firmware queue is correctly described as
`LINKED_NOT_WORKLOAD_EXECUTED`.

## Snapshot mutex and data ownership

One `StaticSemaphore_t` mutex protects only task-context copy-in/copy-out of the
health, resource and IRQ-latency snapshots. Take uses 0 ticks. Contention makes
a getter return `false`, or makes a writer skip that publication, and increments
a saturating read/write counter. The caller keeps its previous local copy or
records the value as unavailable; it never receives a partially updated object.

The mutex never covers HAL calls, SPI/I²C transactions, RS485/CAN encoding,
queue operations, logging, retry or delay. Scheduler-before initialization may
write the initial snapshot directly. SPI1 and I²C2 have no RTOS mutex:
`acquisition_task` is their only runtime owner, while the existing boot probe is
the scheduler-before exception. Other tasks will consume copied measurement
snapshots after S4 instead of calling sensor or BSP bus APIs.

## Software verification

- Host Debug: 8/8 PASS;
- Host Release: 8/8 PASS;
- burst model: 12 publications into depth 8 gives 8 accepted and 4 drop-new;
- FIFO/by-value copy, drain budget 2, slow consumer, invalid input, timestamp
  wrap and saturating counters: PASS;
- future command admission: reject-new on full, no overwrite: PASS policy model;
- snapshot read/write contention counters: PASS policy model;
- BSP/static negative self-test rejects zero queue depth, blocking wait, dynamic
  queue creation and an ordinary queue API injected into a UART callback;
- Firmware Debug: PASS, `text/data/bss = 23272/160/9056` B;
- Firmware Release: PASS, `text/data/bss = 20360/156/9052` B;
- linked symbols include static queue creation, static mutex creation, queue send
  and queue receive;
- static resource contract: PASS; Debug RAM 9216 B, Release RAM 9208 B;
- allocator calls and allocator ELF symbols remain absent;
- existing newlib nosys warnings remain unchanged and do not fail linking.

These checks prove the policy model, source boundary and ARM linkage. They do
not prove board scheduling, priority inheritance, queue watermark, contention
rate, latency or slow-consumer behavior under the real task workload.

## Deferred runtime gate

P5-S3-T05 will combine a bounded board workload with health/recovery checks and
sample only compact queue watermark, drop and mutex-contention summaries. S4
defines measurement fields, units, quality and freshness before instantiating
the measurement snapshot. S5 defines command IDs and Modbus write semantics
before instantiating the command queue.

No raw queue trace, per-item CSV/JSON, mutex timeline or evidence bundle is
required for a normal pass.
