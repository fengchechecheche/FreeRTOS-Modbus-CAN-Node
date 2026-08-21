# P5-S6-T01 CAN contract

> Contract status: `CANDIDATE_VALIDATED`
> Codec status: `CANDIDATE_VALIDATED`
> Runtime status: `CANDIDATE_IMPLEMENTED`
> Hardware status: `PASS`

## Scope

This is the project-specific Classical CAN 2.0A telemetry contract. It is not
CANopen, J1939 or UDS. The contract freezes wire meaning and a HAL/RTOS-free
codec. P5-S6-T02 implements bxCAN filters, interrupts, fixed mailboxes, bounded
queues and bus-off recovery as a software candidate. A Host vector or ARM build
is not physical CAN evidence.

The machine-readable authority is
[`../protocol/can_message_map.json`](../protocol/can_message_map.json).

## Physical and arbitration profile

The candidate uses 11-bit standard data frames, 500 kbit/s, DLC 8 and no CAN
FD, extended or remote frames. The current CubeMX candidate has a 45 MHz APB1
clock, prescaler 6 and 15 time quanta (`1 + 12 + 2`), giving 500 kbit/s and an
86.67% sample point. Pins are PB8/CAN1_RX and PB9/CAN1_TX. The bounded physical
check passed with the admitted Shield/`gs_usb` adapter, 500 kbit/s and a common
USB-CAN-to-Shield ground; this does not claim waveform quality or arbitrary
adapter interoperability.

The admitted Host interface must be configured explicitly with
`bitrate 500000 sample-point 0.75`. This Host-side `75%` setting does not change
the MCU's frozen CubeMX timing above. Omitting the Host sample point selected
`0.875` in the tested environment and did not produce a valid physical route.

The node ID is 4. IDs use `class_base + (node_id << 4) + subtype`; this happens
to reuse the numeric Modbus default address but the two address spaces are
independent. Lower identifiers win arbitration:

| ID | Name | Schedule | Meaning |
|---:|---|---|---|
| `0x140` | `status_event` | change only, rate limited | boot, health, sensor and CAN transitions |
| `0x240` | `heartbeat` | 1000 ms | uptime and image generation |
| `0x241` | `health_summary` | 1000 ms or state change | health, four source states and warnings |
| `0x340` | `climate_primary` | 1000 ms | BME temperature, pressure and flags |
| `0x341` | `climate_secondary` | 1000 ms | BME humidity and age |
| `0x342` | `illuminance` | 1000 ms | VEML value, flags and age |
| `0x440` | `vibration_summary` | 1000 ms | ADXL feature resultant RMS, flags and age |
| `0x540` | `diagnostic_ping_request` | explicit Host one-shot | read-only Host-to-node path check |
| `0x541` | `diagnostic_ping_response` | one per accepted request | matching node-to-Host reply |

Events have the highest priority but are not periodic. Repeated code/source
pairs must be coalesced or rate limited by the T02 queue policy.
The original seven revision-1 IDs are produced by the STM32 node. `0x540` is
owned exclusively by the Host and `0x541` exclusively by the STM32 node. The
diagnostic pair is read-only: it cannot change configuration, outputs or device
state. A host must not inject any of the seven producer-owned telemetry/event
IDs while the default firmware is active.


## Common wire rules

All multi-byte integers are little-endian. Byte 0 is schema revision 1 and
byte 1 is an 8-bit sequence. Data frame sequences are the low byte of the
corresponding S4 source sequence; event, heartbeat and health use independent
publication sequences. `255 -> 0` is normal modulo-256 wrap.

`data_flags` uses bits 0..1 for invalid/fresh/stale/offline, bit 2 for retained
last-good and bit 3 for any S4 quality warning. Bits 4..7 transmit as zero.
The summary bit does not replace the full quality mask available through the
diagnostic/Modbus path.

The codec writes fields explicitly. C enum, bool, structure padding and
pointers are never copied to the wire.

## Payloads

### `0x140 status_event`

`revision:u8, sequence:u8, event_code:u16, severity:u8, source:u8, detail:u16`.
Severity is info/warning/error/critical = 0/1/2/3. Source is system/BME/VEML/
ADXL/RS485/CAN = 0..5. Revision 1 reserves boot `0x0001`, health transition
`0x0100`, sensor offline/recovery `0x0200/0x0201` and CAN state change `0x0300`.

### `0x240 heartbeat`

`revision:u8, sequence:u8, uptime_s:u32, image_generation_low16:u16`.

### `0x241 health_summary`

`revision:u8, sequence:u8, health_state:u8, source_state_pack:u8,
warning_mask:u32`. The four two-bit source states are BME, VEML, ADXL sample
and ADXL feature from least to most significant. Health states 0..5 match the
explicit S3/S5 wire codes.

### `0x340/0x341 climate pair`

Primary is `revision:u8, sequence:u8, temperature:i16 centi_deg_c,
pressure:u24 Pa, data_flags:u8`. `INT16_MIN` and `0xFFFFFF` are invalid.

Secondary is `revision:u8, sequence:u8, humidity:u32 milli_percent_rh,
age_100ms:u16`. Humidity `0xFFFFFFFF` and age `0xFFFF` are unknown; valid age
saturates at `0xFFFE`. A consumer combines the two frames only when revision
and sequence match, so a dropped half cannot create a mixed-age sample.

### `0x342 illuminance`

`revision:u8, sequence:u8, illuminance:u32 millilux, data_flags:u8,
age_100ms:u8`. Value `0xFFFFFFFF` and age `0xFF` are unknown; valid age
saturates at `0xFE`.

### `0x440 vibration_summary`

`revision:u8, sequence:u8, resultant_rms:u32 millig, data_flags:u8,
age_100ms:u8`, with the same unknown and age rules as illuminance. Revision 1
does not stream 100 Hz X/Y/Z samples or claim diagnosis/prognosis.

### `0x540/0x541 diagnostic ping pair`

Request `0x540` is `revision:u8, sequence:u8, opcode:u8, reserved:u8,
nonce:u32`; revision 1 accepts only opcode `0x01` and reserved `0x00`.
Response `0x541` is `revision:u8, sequence:u8, status:u8, opcode_echo:u8,
nonce:u32`; status `0x00` means OK. Sequence, opcode and nonce must match the
request. Example: `540#012A010078563412` returns
`541#012A000178563412`.

The responder has one pending response slot, suppresses an identical
sequence/nonce retry, and admits different tokens no faster than once per
100 ms. Processing and response admission occur in `can_task`, never in the
ISR. A valid request emits one bounded VCP marker such as
`P5CANDIAG1 rx=1 seq=42 nonce=12345678 reply=QUEUED`.

## Load budget

Six periodic frames per second, at most ten event frames per second and the
100 ms diagnostic limiter's ten-response-per-second upper bound are budgeted.
Using a conservative 150 bits per 8-byte standard frame gives
`26 * 150 / 500000 = 0.78%`, below the 1% contract limit. This is a static
envelope, not a measurement of arbitration, retransmission or error frames.

## Validation and boundaries

`tools/verify_can_contract.py` checks the JSON/document relationship, ID/DLC,
field coverage, reserved bits, schedule and scope markers. The pure C Host test
checks known frames, signed and uint24 encoding, sentinels, age saturation,
sequence wrap, BME pair rejection and invalid inputs. Debug/Release Host and
ARM builds establish `CANDIDATE_VALIDATED + CANDIDATE_IMPLEMENTED` only for the
software path. Runtime architecture and troubleshooting are recorded in
[`can_runtime.md`](can_runtime.md).

Hardware sessions require the S2 admission gate, known transceiver/jumpers,
CANH/CANL/common GND, reviewed termination, matching bitrate,
candleLight/SocketCAN and representative decoded frames. The bounded admitted
route is recorded in [`can_hil_report.md`](can_hil_report.md). Physical dual-bus
concurrency remains outside T01/T02 and passed separately under the bounded
`BUS-02` scope in [`dual_bus_fault_matrix.md`](dual_bus_fault_matrix.md).
