# P5-S6-T03 SocketCAN / candleLight HIL report

> Software status: `PASS_HOST`
> Hardware status: `WAITING_FOR_HARDWARE`
> Integration status: `NOT_RUN`
> Baseline: `4a581aea31dc97c6deec51726ed0ccf7325579bd` (`[032]`)

## Scope and boundary

This report closes the hardware-free SocketCAN preflight only. The Python probe
uses the standard-library `AF_CAN/CAN_RAW` API and the frozen
`protocol/can_message_map.json`; it does not add a CAN command, modify firmware,
or treat local loopback as physical evidence.

The current CAN revision has no request, configuration-write or echo command.
A future host transmission can prove a bounded bus-direction exercise, but a
local send return or candleLight TX echo alone cannot prove remote ACK or MCU
application acceptance.

## Environment

| Item | Observed value | Result |
|---|---|---|
| Distro | `Ubuntu-24.04-STM32` | PASS |
| Kernel | `6.18.33.2-microsoft-standard-WSL2` | PASS |
| Python | 3.12.3 | PASS |
| iproute2 | 6.1.0 | PASS |
| `vcan` | kernel module present | PASS_READY |
| `gs_usb` | kernel module present | PASS_READY |
| `can-utils` | 2023.03-1, `/usr/bin/candump`, `/usr/bin/cansend` | PASS |
| physical CAN netdev | none | EXPECTED_NO_HARDWARE |

After user-managed installation, a bounded CLI smoke created a temporary
`vcan0`, captured one `0x240` heartbeat with `candump`, sent it with `cansend`,
and removed the interface. This is `PASS_HOST`, not physical CAN evidence.

## Probe usage

The modes that do not open an interface are:

```bash
python3 tools/can_hil_probe.py --self-test
python3 tools/can_hil_probe.py --dry-run
```

The explicit virtual-interface test is:

```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan
sudo ip link set vcan0 up
python3 tools/can_hil_probe.py \
  --vcan-self-test --interface vcan0 --allow-send
sudo ip link del vcan0
```

A future hardware observation defaults to receive-only:

```bash
python3 tools/can_hil_probe.py \
  --observe --interface <can-interface> --observe-seconds 10
```

Adding `--allow-send` sends exactly one frozen standard/DLC-8 heartbeat test
frame after the passive observation. It does not enable a control operation.
Output files are created only when `--output-dir` is explicitly supplied, and
the capture is capped at 128 frames.

## Hardware-free results

| ID | Check | Result |
|---|---|---|
| V01 | self-test | PASS: 7 known-good vectors |
| V02 | dry-run | PASS: interface `NOT_OPENED` |
| V03 | `vcan0` seven-ID exchange | PASS: 12 sent / 12 captured |
| V04 | payload decode | PASS: all 7 message types interpreted |
| V05 | BME pair | PASS: 1 matching pair, 0 mismatch |
| V06 | invalid input | PASS: ID/DLC/schema/reserved each rejected once |
| V07 | duplicate | PASS: 1 duplicate counted, not failed |
| V08 | bounded exit | PASS: fixed 2 s window, 128-frame cap, no output by default |
| V09 | `can-utils` CLI | PASS: one `0x240` frame sent/captured on temporary `vcan0` |

The virtual test accepted 8 frames and rejected 4. It observed all six periodic
IDs plus the event ID. The separate CLI smoke captured
`240#01027B0000000100`. The temporary `vcan0` was removed after each run.

Existing regression at `[032]` also passed:

- Host Debug 20/20 and Host Release 20/20;
- CAN contract 114 facts and five-mutant self-test;
- Modbus contract 878 facts and BSP contract 532 facts;
- ARM Debug and Release cross-build.

No firmware file changed, so the firmware resource baseline remains T02's
Debug `text/data/bss = 53272/160/13224` and Release
`44404/156/13216`.

## Physical HIL matrix

| ID | Required observation | Status |
|---|---|---|
| H01 | candleLight identity and `gs_usb` netdev in WSL | NOT_RUN |
| H02 | 500 kbit/s, UP, initial ERROR-ACTIVE | NOT_RUN |
| H03 | six periodic IDs in an approximately 10 s window | NOT_RUN |
| H04 | revision/DLC/fields decode | NOT_RUN |
| H05 | one matching `0x340/0x341` pair | NOT_RUN |
| H06 | one bounded host TX without new interface errors | NOT_RUN |
| H07 | state event if naturally observed | NOT_OBSERVED_ALLOWED |
| H08 | interface-down and USB detach cleanup | NOT_RUN |

Physical HIL must wait for the NUCLEO-F446RE, transceiver, candleLight, wiring,
two-end termination and S2 admission gates. `vcan` does not establish ACK,
arbitration, electrical integrity, filter IRQ behavior or bus-off recovery.

## Lightweight evidence rule

A normal physical pass will add only the adapter/driver and interface summary,
firmware hash, wiring/termination summary, H01-H08 table, one representative
decode for each required periodic ID, and before/after interface counters.
A short bounded `candump` is retained under `.private/can-hil/` only if it helps
diagnose a failure. Continuous capture and per-frame archives are out of scope.

## References

- [Linux Kernel SocketCAN documentation](https://docs.kernel.org/networking/can.html)
- [linux-can/can-utils](https://github.com/linux-can/can-utils)
- [candleLight firmware](https://github.com/candle-usb/candleLight_fw)
- [Microsoft WSL USB guide](https://learn.microsoft.com/windows/wsl/connect-usb)
