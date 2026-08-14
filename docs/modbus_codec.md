# P5-S5-T02 Modbus RTU CRC, complete ADU and timing contract

> Software status: `CANDIDATE_VALIDATED`  
> Streaming parser and function handlers: `NOT_IMPLEMENTED`  
> Full-size BSP transport: `NOT_READY` (`64 B` current limit)  
> Hardware: `WAITING_FOR_HARDWARE`

## Layer boundary

This task implements three HAL/RTOS-free primitives:

1. CRC16-Modbus numeric calculation;
2. bounded encoding and validation of one already-delimited RTU ADU;
3. integer microsecond timing calculation for the frozen 8E1 format.

The decoder is not a byte-stream parser. It does not split concatenated frames,
reassemble fragments, classify t1.5/t3.5 gaps or own DMA buffers. It also does
not decide whether an address, function, register range or value is legal.
Stream ownership belongs to P5-S5-T03; PDU semantics and exceptions belong to
P5-S5-T04.

## CRC contract

| Parameter | Value |
|---|---:|
| Width | 16 |
| Initial value | `0xFFFF` |
| Reflected polynomial | `0xA001` |
| Final XOR | `0x0000` |
| Wire order | low byte, then high byte |

`p5_modbus_crc16_calculate()` accepts a null input only for an empty range and
requires a non-null output. It returns the numeric CRC; wire byte order is
applied only by the ADU encoder.

The implementation is bitwise and uses no lookup table, heap, global state,
HAL, RTOS or floating point.

## Complete-ADU contract

| Boundary | Value |
|---|---:|
| Minimum structural ADU | 4 B: address, function, CRC-low, CRC-high |
| Maximum RTU ADU | 256 B |
| Maximum function data | 252 B |

`p5_modbus_rtu_adu_encode()` writes into a caller-owned buffer. On failure it
sets the supplied output length to zero. Input data and output must not overlap.

`p5_modbus_rtu_adu_decode()` first clears the view, checks the `4..256 B`
boundary and verifies CRC. Only a successful call exposes address, function and
a read-only data pointer into the caller's frame. The pointer is valid only as
long as that frame remains valid.

The generic envelope intentionally accepts structurally valid broadcast,
reserved-address, unsupported-function and semantically invalid PDU values.
P5-S5-T04 will reject or respond to those cases after CRC validation.

## 8E1 timing contract

One 8E1 character is 11 bits. Every fractional microsecond is rounded upward so
the software threshold is never shorter than the mathematical interval.

| Baud | Character | t1.5 | t3.5 |
|---|---:|---:|---:|
| 9600 | 1146 us | 1719 us | 4011 us |
| 19200 | 573 us | 860 us | 2006 us |
| 38400 | 287 us | 750 us | 1750 us |
| 115200 | 96 us | 750 us | 1750 us |

At or below 19200 bit/s the intervals are calculated independently from the
exact rational value. Above 19200 bit/s the recommended fixed 750/1750 us
intervals are used. The helper does not configure a UART or timer and does not
claim runtime baud-rate switching.

## Golden vectors

`test/host/modbus_codec_vectors.h` directly feeds the CTest executable. It
contains the official CRC checks (empty, zero, `123456789`, 254-byte pattern),
16 complete Project Three frames and three Project Five address-4 frames.

The Project Three values were read at clean HEAD
`8e0e909a8b7576ab80b6f2ade186631910226b47`; the source CSV SHA-256 is
`995747dace392e334c9448b40071ffcee16d405d25ad731c84ece5569b6265f4`.
Its truncated, noise-prefix and concatenated vectors are not counted as T02
passes because they require the T03 stream state machine.

Project Five address-4 request/echo vectors are:

```text
04 04 00 00 00 7A 71 BC
04 03 00 00 00 04 44 5C
04 06 00 00 00 05 49 9C
```

The last frame is only an envelope/CRC oracle. T02 does not claim the write is
authorized or committed; that behavior remains in T04.

## BSP capacity handoff completed by T03

P5-S5-T03 keeps the Normal-DMA RX chunk at 64 B while separating the complete
RTU/TX capacity at 256 B. The default firmware links one 256 B stream buffer,
one 256 B controller TX copy and two 64 B RX chunk buffers. The legacy smoke
path is compile-time selected and is not called by the default transport.
See [`modbus_transport.md`](modbus_transport.md).

## Verification boundary

`p5.host.modbus_codec` runs in both Host presets. The three production source
files are also compiled by both ARM presets but remain unreferenced by runtime,
so linker garbage collection should add no linked static RAM. Host and ARM
success do not prove physical character time, frame gaps or RS485 transfer.
T03 adds a runtime candidate and Host stream tests, but the same physical
validation boundary remains in force.
