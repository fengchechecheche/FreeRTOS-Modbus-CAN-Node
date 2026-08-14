# P5-S5-T01 Modbus RTU slave and register contract

> Contract status: `CANDIDATE_VALIDATED`  
> Runtime: `NOT_IMPLEMENTED`  
> Hardware: `WAITING_FOR_HARDWARE`  
> Register-map revision: 1  
> Machine-readable authority: [`../protocol/register_map.json`](../protocol/register_map.json)

## Scope and role

The STM32 node is a Modbus RTU slave/server. It does not implement a Modbus
master, ASCII, TCP, broadcast, multiple-register write or a custom function.
The candidate subset is `0x03` read holding registers, `0x04` read input
registers and `0x06` write one whitelisted holding register.

This document and the JSON map freeze application semantics only. CRC, RTU
framing, parser, exception response, function handlers and UART/RS485 runtime
remain `NOT_IMPLEMENTED`; PA9/PA10/PA8 hardware remains untested.

## Link and address contract

| Item | Candidate contract |
|---|---|
| Default slave address | `4` |
| Legal unicast range | `1..247` |
| Broadcast address 0 | unsupported by this project subset |
| Default serial profile | `19200 bit/s, 8E1` |
| PDU register address | zero-based `0x0000..0xFFFF` |
| Register byte order | high byte first |
| 32-bit word order | high word first |
| CRC wire order | low byte first; runtime deferred to T02 |

`30001`/`40001` references are display notation only and never change a wire
address. Input and holding registers are separate spaces, so both may contain
numeric address zero.

## Data and validity

All 17 S4 measurement fields retain their native integer units. Signed values
use two-register `int32` two's complement; unsigned values use two-register
`uint32`. No float, automatic endianness detection, 16-bit truncation or
implicit engineering rescale is allowed.

| Source | Values | Unit |
|---|---|---|
| BME280 | temperature / pressure / humidity | centi-°C / Pa / milli-%RH |
| VEML7700 | illuminance | millilux |
| ADXL sample | acceleration X/Y/Z | millig |
| ADXL feature | mean/RMS/peak X/Y/Z and resultant RMS | millig |

There is no invalid-value sentinel. If a source has no last-good value, its
value words are initialized to zero but have no engineering meaning:
consumers must check the source state and `value_present` flag. A retained
stale/offline value is usable only together with state, age, quality and
`value_retained=1`.

The wire state codes are 0 invalid, 1 fresh, 2 stale and 3 offline. Quality
keeps the S4 bits 0..7: uncalibrated, range warning, saturated, gap, dropped,
transport error, configuration error and recovery active. Wire codes are
explicit; C enum values, `bool`, padding and struct layout are not a wire ABI.

## Input-register image

Input registers form one contiguous `0x0000..0x0079` region: 122 registers,
below the `0x04` maximum quantity 125. A master can therefore obtain the full
identity/value/metadata/diagnostic image in one request.

| Range | Count | Contents |
|---|---:|---|
| `0x0000..0x0011` | 18 | signature, revisions, candidate firmware version, generation, evaluated time and masks |
| `0x0012..0x0033` | 34 | 17 S4 values, each int32/uint32 |
| `0x0034..0x005B` | 40 | four 10-register source metadata blocks |
| `0x005C..0x0074` | 25 | ADXL IRQ/drop and three device fault/recovery summaries |
| `0x0075..0x0079` | 5 | health state/warnings and RS485 error aggregate |

Each source metadata block is state (1), flags (1), quality (2), sequence (2),
sample monotonic milliseconds (2) and age milliseconds (2). ADXL sample and
ADXL feature remain independent sources.

The 17 value starting addresses are:

| Address | Field ID | Name | Type |
|---:|---:|---|---|
| `0x0012` | `0x0101` | BME temperature | int32 |
| `0x0014` | `0x0102` | BME pressure | uint32 |
| `0x0016` | `0x0103` | BME humidity | uint32 |
| `0x0018` | `0x0201` | VEML illuminance | uint32 |
| `0x001A/1C/1E` | `0x0301..0303` | ADXL acceleration X/Y/Z | int32 |
| `0x0020/22/24` | `0x0311..0313` | ADXL mean X/Y/Z | int32 |
| `0x0026/28/2A` | `0x0321..0323` | ADXL RMS X/Y/Z | uint32 |
| `0x002C/2E/30` | `0x0331..0333` | ADXL peak X/Y/Z | uint32 |
| `0x0032` | `0x0341` | ADXL resultant RMS | uint32 |

`sample_monotonic_ms` is the software-admission FreeRTOS tick. It is not the
sensor conversion start, IRQ edge, UTC or RTC time.

## Request-local atomicity

Every future `0x04` response must use one request-local fixed image. The
handler first copies the complete required snapshots, releases the existing
zero-tick mutex, and only then performs register lookup, encoding and CRC. It
must not combine objects obtained before and after a failed snapshot read.

If a complete snapshot is unavailable, the future handler returns server
device failure; that behavior is not yet implemented. A successful image
publication increments `register_image_generation`. A client that splits the
region across requests compares generation and retries if it changed.

## Holding-register and write contract

| Address | Name | Access | Semantics |
|---:|---|---|---|
| `0x0000` | active slave address | R/W | 1..247; default 4 |
| `0x0001` | default slave address | R | constant 4 |
| `0x0002` | serial profile | R | code 1 = 19200 8E1 |
| `0x0003` | configuration generation | R | increments after a changed value is committed |

Only `active_slave_address` is in the `0x06` whitelist. A valid changed address
is committed after the normal echo response reaches TX complete, so that
response still uses the old address. The setting is volatile and resets to 4;
there is no Flash/EEPROM persistence. Rewriting the active value succeeds but
does not increment generation. The actual handler is deferred to T04.

## Sensor and system diagnostics

The map exposes only fault class, episode/recovery counts, ADXL IRQ/drop,
health state/warnings and an RS485 error aggregate. It deliberately excludes
raw sample history, per-transaction logs, stack watermarks, queue traces and
raw reset flags.

Sensor fault codes are 0 none, 1 identity/not-present, 2 busy, 3 timeout,
4 I/O, 5 configuration, 6 stalled, 7 recovery active and 8 offline. Health
codes are 0 bootstrap, 1 serviceable, 2 degraded, 3 recovery required,
4 reset required and 5 reset-loop latched.

## Project Three compatibility boundary

At the read-only baseline
`8e0e909a8b7576ab80b6f2ade186631910226b47`, Project Three uses addresses
`1/environment_sensor`, `2/motor_actuator` and `3/fault_injection_device`.
Project Five uses address 4 and does not reuse those field semantics.

Both projects agree on zero-based PDU addresses, `0x03/0x04/0x06`, big-endian
register bytes and high-word-first 32-bit values. No Project Three address-4
profile exists yet, and this task does not authorize modifying that repository.

## Verification and evidence boundary

Run:

```bash
python3 tools/verify_modbus_contract.py
python3 tools/verify_modbus_contract.py --self-test
```

The validator checks JSON structure, duplicate keys, spans, overlap, type
width, 122-register continuity, the exact 17-field oracle, metadata, access,
write whitelist and scope boundaries. A pass proves only that the candidate
contract is internally consistent. It does not prove a request can be parsed,
a response can be encoded or the RS485 hardware can exchange a frame.

Current software result:

| Check | Result |
|---|---|
| Modbus contract validator | 852 facts PASS |
| in-memory negative self-test | 12 mutant classes rejected |
| Host Debug / Release | 14/14 PASS in each preset |
| BSP contract / self-test | 489 facts PASS / PASS |
| Firmware Debug | unchanged `40648/160/11184 B` text/data/bss |
| Firmware Release | unchanged `34064/156/11176 B` text/data/bss |
| static resource contract | PASS; linker heap 0 |

Unchanged firmware size is expected because T01 adds no runtime C source.
