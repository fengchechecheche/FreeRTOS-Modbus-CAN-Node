# P5-S3-T03 interrupt and task-notification report

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_ISR_CONTRACT`
> Content status: `FROZEN` (approved 2026-08-14)
> Hardware notification path: `LINKED_NOT_EXECUTED`
> ISR-to-task latency: `NOT_MEASURED`
> Hardware status: `WAITING_FOR_HARDWARE`
> Input baseline: `3a1ae8a14977e99809cf5c1003341909dde0c7d5` (`[ 015 ]`)

## IRQ and FreeRTOS boundary

`NVIC_PRIORITYGROUP_4` assigns all four implemented priority bits to preemption.
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` is 5. USART1, DMA2 Stream2 and
DMA2 Stream7 are all priority 6/subpriority 0 in both `.ioc` and generated C,
so their callbacks may use the approved FreeRTOS `FromISR` notification API.
TIM6 remains priority 0 and only supplies the HAL tick.

| Event bit | ISR/callback work | `protocol_task` work |
|---|---|---|
| `RX_FRAME` | preserve length, set pending bit, notify | copy up to 64 B, publish frame, rearm RX DMA |
| `RX_HALF` | set diagnostic bit, notify | count and clear only |
| `TX_COMPLETE` | immediately drive DE low, set bit, notify | finish state transition and restore RX |
| `UART_ERROR` | capture HAL error, drive DE low, set bit, notify | classify, abort/recover and restore RX |

Callbacks do not copy a frame, rearm DMA, advance the RS485 state machine,
retry, parse or block. The checker extracts each callback body and rejects these
operations. Its in-memory negative tests also reject priority 0 and an injected
callback `memcpy()`.

## Fixed mailbox and task timing

The mailbox stores one 32-bit pending mask, RX length, HAL error bits and bounded
counters. Repeated bits coalesce and increment a counter; the first pending RX
length is retained. Invalid RX length becomes an error event. Error or mutually
contradictory RX/TX snapshots take the recovery path and increment a conflict
counter. Events that arrive before notifier registration remain pending and are
drained when `protocol_task` starts.

The notification value is a wake signal, not a frame queue. `protocol_task`
waits only until its existing absolute 5 ms release. After every bounded event
drain it checks that release again, so repeated notifications cannot move the
poll/timeout deadline. T03 itself added no sixth task, queue, semaphore, mutex,
heap or multi-frame ring buffer. The later T04 diagnostic queue and snapshot
mutex do not replace or enter this ISR notification path.

## Software verification

- Host Debug: 7/7 PASS;
- Host Release: 7/7 PASS;
- mailbox injection covers RX frame, RX half, duplicate/coalesced event, invalid
  length, UART/error conflict and early event;
- pure timing tests cover notification storms and tick wrap without moving the
  absolute release;
- latency aggregate tests cover unmeasured state, first/min/max/last/count and
  32-bit cycle wrap;
- Firmware Debug: clean build/link PASS, `text/data/bss = 20136/160/8728` B;
- Firmware Release: clean build/link PASS, `text/data/bss = 17608/156/8724` B;
- static resource contract: PASS; Debug RAM 8888 B, Release RAM 8880 B;
- ELF contains `xTaskGenericNotifyFromISR`, `xTaskNotifyWait`, three IRQ handlers
  and three HAL UART callbacks;
- `P5_IRQ_NOTIFICATION_SMOKE=ON` compiles; final default is restored to `OFF`;
- existing newlib nosys warnings remain unchanged and do not fail linking.

These results prove source consistency, Host behavior and ARM linkage. They do
not prove that a real NVIC/DMA interrupt has fired or that a context switch and
RS485 transfer work on the board.

## Hardware follow-up

After the S2 board and scheduler gates pass, temporarily enable the IRQ smoke
and perform three fixed `P5T03 -> P5T03OK` exchanges. Confirm RX/TX notification
counters and the protocol release continue to advance, with no drop, duplicate
processing, timeout, assert, reset or DE stuck high. Read only one
`count/min/max/last` DWT summary, then restore the option to `OFF`.

There is no strict microsecond pass limit in this stage. A maximum above one
5 ms protocol period is a diagnostic trigger, not an automatic rejection based
on one sample. UART error injection may remain `NOT_RUN` if it is not convenient.
No raw per-event trace or evidence bundle is required for a normal pass.
