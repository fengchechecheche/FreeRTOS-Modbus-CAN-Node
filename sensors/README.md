# Sensors layer

This directory contains platform-independent sensor logic. A sensor driver may
parse registers, compensate raw values and advance a bounded state machine, but
must not call HAL, FreeRTOS, delay, allocator, mutex or logging APIs.

`bme280.c` implements the P5-S4-T01 BME280 candidate. The application adapter
injects SPI read/write operations, and `acquisition_task` remains the only
runtime owner. One service call performs at most one SPI transaction. The
current owner-local snapshot is not the cross-sensor freshness contract; that
schema remains deferred to P5-S4-T04.

`veml7700.c` implements the P5-S4-T02 VEML7700 candidate. It uses injected
I2C word read/write operations, integer millilux conversion and a bounded
nine-level auto-range state machine. `acquisition_task` remains the only I2C2
runtime owner, and one VEML service call performs at most one transaction.
High-lux correction is deliberately flagged but not fabricated without an
application-specific optical validation.

`adxl345.c` implements the P5-S4-T03 ADXL345 candidate. It uses injected SPI
operations, one-transaction-per-service initialization, six-byte coherent XYZ
reads, signed integer scaling and a fixed 100-sample accumulator window. The
window stores no raw history and publishes only mean, DC-removed RMS, peak and
resultant RMS. DATA_READY event coalescing is reported as a lower-bound drop
count; FIFO, FFT, alarm thresholds and fault classification remain out of scope.

Host vectors and ARM linking validate software behavior only. Until hardware is
available, chip identity, bus timing, measurement acquisition and accuracy all
remain `NOT_RUN`/`NOT_CLAIMED`.
