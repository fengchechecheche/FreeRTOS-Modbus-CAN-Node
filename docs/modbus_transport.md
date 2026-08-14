# P5-S5-T03 Modbus RTU transport candidate

> Software status: `PASS_HOST + PASS_CROSS_BUILD + READY_FOR_CONTENT_REVIEW`  
> Hardware status: `WAITING_FOR_HARDWARE`  
> Implemented scope: stream assembly, RX chunk metadata, bounded half-duplex TX  
> Deferred scope: function handlers, responses, register writes and physical timing validation

## Runtime path

The default firmware path is:

```text
USART1/DMA IRQ
  -> one-slot metadata mailbox (kind, length, DWT capture)
  -> protocol_task notification
  -> copy 1..64 B Normal-DMA chunk and re-arm RX
  -> reconstruct candidate byte-end timestamps
  -> HAL/RTOS-free RTU stream state machine
  -> CRC-valid synchronous consumer
  -> unhandled_valid_frames++ (T04 handoff)
```

`P5_RS485_LOOPBACK_SMOKE=ON` selects the legacy bounded ASCII loopback path.
It is not enabled in the default firmware and must not be used on an external
RS485 bus.

## Stream states and boundaries

- `IDLE`: no partial frame.
- `RECEIVING`: 1..256 bytes retained in the single fixed frame buffer.
- `DISCARD_UNTIL_GAP`: an inter-character violation or byte 257 discards the
  candidate until a `t3.5` gap.
- A silence exactly equal to `t1.5` is accepted. A silence `>t1.5` and `<t3.5`
  invalidates the frame. A silence `>=t3.5` closes the previous frame.
- `poll()` closes a partial frame once the silence since the last byte end is
  `>=t3.5`.
- Unsigned 32-bit subtraction makes all comparisons safe across DWT wrap.
- Frames shorter than 4 B, CRC mismatches and overlong frames update saturating
  counters; no raw-frame history or heap allocation is retained.

The consumer is synchronous in `protocol_task` context. At T03 a CRC-valid ADU
is deliberately not answered: it increments `unhandled_valid_frames` and is
released. Function-code admission and response construction belong to T04.

## Normal-DMA timestamp adapter

RX remains a 64 B `DMA_NORMAL` Receive-to-IDLE chunk. ISR work is limited to
event kind, length, DWT capture and task notification. The task copies the
chunk and re-arms the DMA.

- DMA transfer-complete capture approximates the end of the last byte.
- IDLE capture occurs after approximately one character of idle time, so one
  calculated character duration is subtracted.
- Earlier bytes in the same chunk are projected backwards at one calculated
  8E1 character duration per byte.

This adapter is sufficient for the software candidate and deterministic Host
tests. It cannot observe an internal physical pause between roughly 1.0 and
1.5 characters when the HAL reports the bytes as one chunk. Therefore no
claim is made that real t1.5/t3.5 timing is validated.

## Half-duplex TX order

`bsp_rs485_send()` accepts 1..256 B and copies the caller data before changing
the link:

```text
validate/copy -> stop RX DMA -> DE=TX -> start TX DMA
TX final USART TC ISR -> DE=RX -> notify protocol_task
protocol_task -> account completion -> re-arm RX DMA
```

RX-stop, TX-start, timeout, UART-error and RX-rearm failures are fail-closed
and counted. A second send while TX/recovery is active returns `BUSY`. No task,
queue, mutex or heap object was added.

## Verification boundary

Host tests cover valid frames, CRC/short/overlong rejection, exact t1.5,
inter-character invalidation, t3.5 splitting, timestamp wrap, 256 B TX and
RX-stop failure. Debug/Release ARM builds prove only compile/link closure.
Oscilloscope or logic-analyzer evidence for DE, first/last bit timing and real
bus gaps remains `NOT_RUN` until hardware arrives.
