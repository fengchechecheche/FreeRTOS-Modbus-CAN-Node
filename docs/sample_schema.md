# P5-S4-T04 unified measurement sample schema

> Software: `PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`
> Content: `READY_FOR_CONTENT_REVIEW`
> Hardware: `WAITING_FOR_HARDWARE`
> Runtime timestamp/jitter: `NOT_MEASURED`

## Scope

`app_measurement` projects the owner-local BME280, VEML7700 and ADXL345
snapshots into one latest-value snapshot for future task consumers. It does not
change the pure sensor drivers and is not a history queue or wire format.

Four independently timed sources are retained:

- BME280 temperature/pressure/humidity;
- VEML7700 illuminance;
- ADXL345 instantaneous XYZ acceleration;
- ADXL345 100-sample mean/RMS/peak/resultant feature.

The ADXL sample and feature use separate sequence/timestamp metadata. A 100 Hz
sample must not refresh a feature that is produced only after a complete
100-sample window.

## Metadata and time semantics

Each source has:

```text
source
state = INVALID | FRESH | STALE | OFFLINE
sequence
sample_monotonic_ms
age_ms
quality_flags
value_present
value_is_retained
```

`sample_monotonic_ms` is the 1 kHz FreeRTOS tick at which acquisition first
accepts a changed, valid source sequence. It is not the sensor conversion
start, IRQ-edge timestamp, UTC or RTC time. Seeing the same sequence again does
not refresh it. `age_ms` uses unsigned `now - sample_time`, including tick wrap.

| Source | Fresh through |
|---|---:|
| BME280 | 2500 ms |
| VEML7700 | 3000 ms |
| ADXL345 sample | 200 ms |
| ADXL345 feature | 2500 ms |

The exact threshold remains fresh; threshold plus one millisecond is stale.
These deliberately loose software limits are not a measured real-time claim.

## State and retained value

| State | Meaning | Value behavior |
|---|---|---|
| `INVALID` | no accepted last-good sample | `value_present=false`; field read is unavailable |
| `FRESH` | last-good age is within the source limit | value available, not retained |
| `STALE` | last-good is older than the limit | value available and retained |
| `OFFLINE` | source driver reached its terminal state | last-good may remain, and is retained when present |

Normal BME forced-mode wait, VEML integration/ranging and ADXL wait-for-IRQ do
not immediately force stale. Freshness is based on last-good age; current
transport/configuration/recovery concerns appear in quality. An invalid field
accessor leaves its output argument unchanged, so zero-initialized payload
memory cannot become a fabricated 0 °C, 0 lux or 0 g reading.

## Normalized quality

The shared 32-bit mask contains only cross-interface categories:

- `UNCALIBRATED`;
- `RANGE_WARNING`;
- `SATURATED`;
- `GAP`;
- `DROPPED`;
- `TRANSPORT_ERROR`;
- `CONFIGURATION_ERROR`;
- `RECOVERY_ACTIVE`.

Driver state, raw registers, range level and transaction/error/recovery
counters remain in the owner-local diagnostic snapshots. State is not encoded
again in quality.

## Logical fields and units

Schema revision is 1. The 17 IDs are logical field IDs, not Modbus register
addresses or CAN message/arbitration IDs.

| ID | Field | Source | Unit |
|---:|---|---|---|
| `0x0101` | temperature | BME280 | centi-°C |
| `0x0102` | pressure | BME280 | Pa |
| `0x0103` | humidity | BME280 | milli-%RH |
| `0x0201` | illuminance | VEML7700 | millilux |
| `0x0301`–`0x0303` | acceleration X/Y/Z | ADXL sample | millig |
| `0x0311`–`0x0313` | mean X/Y/Z | ADXL feature | millig |
| `0x0321`–`0x0323` | RMS X/Y/Z | ADXL feature | millig |
| `0x0331`–`0x0333` | peak X/Y/Z | ADXL feature | millig |
| `0x0341` | resultant RMS | ADXL feature | millig |

The checked accessor returns a neutral `int64_t` value plus metadata. S5 and S6
must explicitly encode this dictionary and choose their own width, byte order
and saturation policy. They must not copy the C struct layout onto either bus.

## Ownership and publication

`acquisition_task` remains the only SPI1/I²C2 runtime owner. After bounded
sensor service, it updates the owner-only measurement model and publishes one
complete base snapshot through the existing zero-wait snapshot mutex. Other
tasks copy the snapshot under the same mutex, release it, and then evaluate age
in the local copy. No HAL, bus operation, encoding, retry or logging runs while
the mutex is held.

Writer contention skips one publication; reader contention returns unavailable.
Both use the existing saturating contention counters. No task, queue, second
mutex, heap or periodic evidence buffer was added.

## Software verification

| Check | Result |
|---|---|
| Host Debug | 13/13 PASS |
| Host Release | 13/13 PASS |
| BSP contract | 422 stable facts PASS |
| negative self-test | duplicate field ID, invalid-value leak and loop mutants rejected |
| Firmware Debug | PASS; text/data/bss `39304/160/10632` B |
| Firmware Release | PASS; text/data/bss `32992/156/10632` B |

Relative to `[022]`, Debug Flash/RAM change is `+1280/+600` B and Release is
`+1032/+600` B. The five 256-word application stacks, queue depth 8 and linker
heap 0 remain unchanged. Existing newlib nosys warnings remain the only linker
warnings.

Host tests cover initial invalid, checked unavailable reads, first-good
admission, unchanged and wrapping sequences, tick wrap, inclusive freshness
boundaries, stale/offline retained values, BME valid-mask rejection, normalized
quality, independent ADXL sample/feature time and all 17 field IDs/units.

## Hardware follow-up

After individual sensor admission, read two compact unified snapshots and
confirm that only sources with new samples change sequence/time. Pause or
disconnect one source and confirm eventual stale/offline plus retained
last-good while the other sources continue. Record one acquisition watermark.

Actual sample instant, jitter, combined WCET and hardware status transitions
remain `NOT_MEASURED`/`NOT_RUN`; the 60-minute matrix belongs to P5-S4-T05.

## S4-T05 diagnostic monitor boundary

`app_sensor_monitor` consumes the four existing source metadata records and
three owner-local driver summaries. It records accepted count, last/min/max
interval, normalized device fault episode/recovery timing, three current masks
and the ADXL IRQ/drop summary. It does not add a field, unit, register, CAN ID
or wire format; schema revision 1 and all 17 logical field IDs are unchanged.

Its Host interval and recovery durations use deterministic virtual ticks and
must not be reported as measured jitter or physical recovery time. No raw
sample history or periodic evidence stream is retained.

## S5-T01 Modbus projection

The candidate Modbus input map projects all 17 logical fields without changing
their S4 types or units. Signed temperature, acceleration and mean values use
two-register int32; pressure, humidity, illuminance, RMS, peak and resultant
values use two-register uint32. Each register sends its high byte first and a
32-bit value sends its high word first. No float or 16-bit rescale is used.

Each of the four sources has an independent 10-register metadata block with
state, value-present/retained flags, quality, sequence, sample monotonic time
and age. Invalid values have no sentinel: value words are meaningful only when
state/value-present allow them. Stale/offline last-good values remain explicitly
retained. The complete candidate input image is 122 contiguous registers and
can fit in one `0x04` read, but CRC/parser/handler runtime remains
`NOT_IMPLEMENTED`.
