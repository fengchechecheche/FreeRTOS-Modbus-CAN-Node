# P5-S5-T04 Modbus function server candidate

> Software status: `PASS_HOST + PASS_CROSS_BUILD + CANDIDATE_IMPLEMENTED`  
> Hardware status: `WAITING_FOR_HARDWARE`  
> Default slave address: `4`  
> Implemented functions: `0x03`, `0x04`, `0x06`

## Layer boundary

`p5_modbus_server` is HAL/RTOS-free. It consumes a CRC-valid
`p5_modbus_adu_view_t`, builds one bounded response and calls an injected TX
sink. It owns only the active address, configuration generation, one pending
address commit and saturating aggregate diagnostics.

The default runtime path is:

```text
RTU stream -> decoded ADU view -> p5_modbus_server
  -> optional request-local 122-register image
  -> 1..249 B response -> bsp_rs485_send()
  -> final TX complete: commit pending address
  -> UART error/timeout/conflict: cancel pending address
```

No task, queue, mutex, semaphore, heap object or raw-frame history is added.

## Address and function rules

- Address `0` is unsupported broadcast and is silently ignored.
- A foreign unicast address is silently ignored.
- Only the current active address enters function dispatch.
- `0x03` reads holding registers `0..3`.
- `0x04` reads any valid subset of input registers `0..121`; the full response
  is 249 B.
- `0x06` writes only holding register `0`, with values `1..247`.
- Unsupported functions return exception `01`.
- Register span or read-only write errors return exception `02`.
- malformed known-function data, quantity 0 or greater than 125, and an
  out-of-range address value return exception `03`.
- An unavailable request-local snapshot returns exception `04`.

Register bytes are big-endian, 32-bit fields are high-word-first and RTU CRC
bytes remain low-byte-first.

## Delayed address commit

A changed address is not applied when its request is parsed:

```text
validate -> encode old-address echo -> TX accepted -> stage
final USART TX complete -> active address changes -> generation increments
```

TX rejection does not stage. UART error, TX timeout, recovery failure or an
RX/TX direction-conflict event cancels the staged value. Rewriting the current
address returns a normal echo but does not stage or increment generation. The
configuration is volatile and resets to address 4.

The pending slot remains protocol-task local. A command queue would only be
needed if a future writable register were owned by another task.

## Request-local register image

`app_rtos_get_modbus_register_source()` takes the existing zero-wait snapshot
mutex once and copies one compact projection of measurement, sensor-monitor and
health state. It then releases the mutex before age refresh, register mapping,
CRC or TX.

`app_modbus_register_image_build()` is pure C and writes all 122 registers. It
uses explicit switches for measurement, sensor-fault and health wire codes;
no C enum, bool, padding or struct is copied to the wire. Missing values remain
zero, while retained values keep their value and expose state/flags/quality/age.

`register_image_generation` increments after a successful measurement/monitor
publication and after a successful health publication. A read request does not
increment it. The request-time stale mask is recomputed from the refreshed
metadata so the header and metadata blocks agree.

## Diagnostics and evidence

Only saturating aggregates are retained: addressed/ignored requests, normal
responses, four exception classes, snapshot unavailable, TX rejection and
address write staged/committed/cancelled/idempotent counts. Successful raw
frames, register histories and periodic traces are not stored.

Host tests cover address filtering, normal and exceptional functions, the
249 B boundary, image failure, TX rejection, delayed commit, cancellation,
idempotent writes, address migration, all register categories and explicit
wire encoding. These results do not prove physical DE timing, UART/DMA loss or
communication with an external master.
