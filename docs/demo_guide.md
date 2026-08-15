# Software and deferred-hardware demo guide

> Guide state: `SOFTWARE_DEMO_READY`
> Hardware demo: `NOT_RUN / BLOCKED_WAITING_FOR_HARDWARE`
> Publication: `NOT_PUBLISHED`

## Boundary

The current demo proves Host behavior, ARM cross-build and bounded documentation
contracts. It does not require a board and does not demonstrate physical
sensor, RS485 or CAN behavior.

Run commands from the repository root in the frozen Ubuntu development
environment. Build output remains under ignored `out/`; do not stage it.

## Current executable software demo

### 1. Host regression

```bash
./tools/verify_host.sh
```

Expected compact result: Host Debug and Host Release each complete 22 tests,
including `p5.host.modbus_pty`.
A failure stops the candidate demo; it must not be hidden by showing an older
report.

### 2. ARM Debug and Release builds

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug
cmake --preset firmware-release
cmake --build --preset firmware-release
```

Expected result: both presets link and generate ELF/HEX/BIN/MAP. Cross-build
success does not imply flashing or target startup.

### 3. Static resource gate

```bash
python3 tools/check_resource_budget.py \
  --source-root . \
  --debug-elf out/firmware-debug/freertos_modbus_can_node.elf \
  --debug-map out/firmware-debug/freertos_modbus_can_node.map \
  --release-elf out/firmware-release/freertos_modbus_can_node.elf \
  --release-map out/firmware-release/freertos_modbus_can_node.map
```

Expected result: `PASS: static-only linker and resource budget contract`.

### 4. Release and documentation gates

```bash
python3 tools/check_release_candidate.py
python3 tools/check_release_candidate.py --self-test
python3 tools/check_release_readiness.py
python3 tools/check_evidence_matrix.py
python3 tools/check_learning_docs.py
```

Expected result: the candidate is ready for hardware while the hardware Release
gate remains blocked.

### 5. Optional virtual-bus routes

The committed Modbus PTY route is:

```bash
python3 tools/modbus_pty_test.py \
  --server out/host-debug/p5_host_modbus_pty_slave
```

Expected result: 10/10 cases PASS with a 249 B maximum response. This runs the
production C RTU/server/register-image modules through a Linux PTY, but it does
not prove UART parity bits, DMA, DE/RE or a physical RS485 link.

The reviewed virtual-bus procedure and can-utils boundary are in
[`can_hil_report.md`](can_hil_report.md). Its vcan result is software evidence
and does not represent a transceiver, physical ACK or MCU frame exchange.

The Modbus self-test, dry-run and PTY boundaries are in
[`modbus_hil_report.md`](modbus_hil_report.md). Self-test/dry-run must not be
described as PTY evidence, and PTY must not be described as USB-RS485 evidence.

## Future hardware demo（NOT_RUN）

Run this sequence only after the corresponding hardware admission gate is
authorized:

1. board/ST-LINK admission and minimal startup — [`bsp_validation.md`](bsp_validation.md);
2. device identity and representative samples — [`bme280_report.md`](bme280_report.md),
   [`veml7700_report.md`](veml7700_report.md), [`adxl345_report.md`](adxl345_report.md);
3. USB-RS485 and Project Three profile — [`modbus_hil_report.md`](modbus_hil_report.md);
4. candleLight and physical CAN — [`can_hil_report.md`](can_hil_report.md);
5. physical dual-bus faults — [`dual_bus_fault_matrix.md`](dual_bus_fault_matrix.md);
6. hardware smoke/pre-run/formal soak — [`soak_trend_report.md`](soak_trend_report.md).

Do not skip directly to a long soak before board, bus and safety admission. A
failed hardware step stays at its own evidence layer and does not invalidate a
qualified Host result.

## Minimal evidence policy

- Keep one compact summary for a useful failure; do not save every successful command.
- Do not commit raw logs, build trees, device serials, private network data or firmware binaries.
- Do not create screenshots or video merely to satisfy this guide.
- Use the evidence matrix, not presentation quality, to decide what can be claimed.
