# P5-S5-T03 Modbus RTU transport candidate

> Software status: `PASS_HOST + PASS_CROSS_BUILD + CANDIDATE_IMPLEMENTED`
> Hardware status: `WAITING_FOR_HARDWARE`  
> Implemented scope: stream assembly, RX chunk metadata, bounded half-duplex TX  
> T04 extension: function handlers, responses and volatile address writes implemented; physical validation deferred

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
  -> HAL/RTOS-free function server
  -> request-local register image when required
  -> bsp_rs485_send()
  -> TX-complete commit or link-failure cancel
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
was deliberately not answered. T04 replaces that handoff with address/function
admission and bounded response construction; see [`modbus_server.md`](modbus_server.md).

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
RX-stop failure. T04 additionally covers functions, exceptions, 249 B response,
register image and delayed address commit. Debug/Release ARM builds prove only
compile/link closure.
Oscilloscope or logic-analyzer evidence for DE, first/last bit timing and real
bus gaps remains `NOT_RUN` until hardware arrives.

## S6-T04 concurrent fault boundary

The dual-bus Host matrix reuses this stream and RS485 controller directly. It
distinguishes normal master idle from one `>t1.5` internal gap, rejects one bad
CRC before accepting the next valid frame, observes `BUSY` without overwriting
the active response, and restores RX after the existing 25 ms virtual timeout
for the fixed 8-byte test response.

During each RS485 fault, CAN/task-model progress remains observable. During CAN
bus-off, valid Modbus frames continue to be accepted. These are deterministic
software-isolation results; physical master timing, UART gaps, DE timing and
recovery duration remain `WAITING_FOR_HARDWARE`. See
[`dual_bus_fault_matrix.md`](dual_bus_fault_matrix.md).
