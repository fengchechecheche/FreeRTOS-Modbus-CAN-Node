# P5-S6-T03 SocketCAN / candleLight HIL report

> Software status: `PASS_HOST`
> Hardware status: `PASS_HARDWARE_LIMITED`
> Integration status: `NOT_RUN`
> Baseline: `4a581aea31dc97c6deec51726ed0ccf7325579bd` (`[032]`)
> Historical hardware supplement source: `285a894d52ff817bf9683a5a7034c01c27675e89`
> Current hardware supplement source: `[057] db84dccd1eaaeaf5403b57482497c39a99d4ca3e`

## Scope and boundary

This report records the hardware-free SocketCAN preflight and the later bounded
physical supplement. The Python probe uses the standard-library `AF_CAN/CAN_RAW`
API and the frozen `protocol/can_message_map.json`; it does not add a CAN command
or treat local loopback as physical evidence.

The current CAN revision adds one read-only diagnostic pair: Host-owned request
`0x540` and STM32-owned response `0x541`. It is not a configuration-write or
general echo command. A local send return or candleLight TX echo alone still
cannot prove remote ACK or MCU application acceptance; a pass requires the
matching response and the bounded VCP receive marker.

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

A physical hardware observation is receive-only:

```bash
python3 tools/can_hil_probe.py \
  --observe --interface <can-interface> --observe-seconds 10
```

`--allow-send` is reserved for `--vcan-self-test`. Physical `--observe` rejects
that flag with `SEND_OWNERSHIP`. The reviewed physical transmit mode is fixed
to the dedicated diagnostic pair:

```bash
python3 tools/can_hil_probe.py \
  --diagnostic-ping --interface can0 \
  --sequence 0x2A --nonce 0x12345678 \
  --response-timeout 2
```

It sends exactly `540#012A010078563412`, expects exactly
`541#012A000178563412`, and classifies timeout, mismatch or duplicate response
separately. Use a new sequence or nonce for another test; do not send a
producer-owned telemetry ID.
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
| H01 | candleLight identity and `gs_usb` netdev in WSL | PASS |
| H02 | 500 kbit/s, UP, initial ERROR-ACTIVE | PASS_LIMITED: host sample point `75%` |
| H03 | six periodic IDs in a bounded default-firmware window | PASS: 252/252 accepted, 42 per ID |
| H04 | revision/DLC/fields decode | PASS |
| H05 | one matching `0x340/0x341` pair | PASS: 42 pairs observed |
| H06 | one bounded host-to-device frame with physical ACK | PASS: `RX_ONLY rxacc=1`, both sides ERROR-ACTIVE and zero errors with common GND |
| H07 | state event if naturally observed | NOT_OBSERVED_ALLOWED |
| H08 | interface-down, USB detach and default-firmware restore | PASS |

## 2026-08-19 bounded hardware supplement

The admitted route used NUCLEO-F446RE, the Waveshare RS485 CAN Shield and a
candleLight/CANable-class `gs_usb` adapter at 500 kbit/s. Device serial numbers
are intentionally omitted. The tested default Debug ELF SHA-256 was
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca`.

With the host sample point set to `75%`, a clean 10-second receive window
captured and accepted 60 periodic frames: 10 each for `0x240`, `0x241`,
`0x340`, `0x341`, `0x342` and `0x440`. All 10 BME280 frame pairs matched,
the host remained ERROR-ACTIVE, and the STM32 CAN error status remained clear.
A second bounded run with the single 120-ohm termination moved to the USB-CAN
end accepted 54 frames, nine per ID, with nine matching BME pairs and no new
errors. This establishes only the STM32-to-host physical receive direction.

H06 failed under both single-termination placements. Sending one frozen,
non-periodic `0x140` status event from the host caused the candleLight side to
enter ERROR-PASSIVE/BUS-OFF and raised the STM32 transmit/receive error
counters. A local `cansend` completion or adapter echo is therefore not treated
as remote ACK. The failure remains bounded to the current USB-CAN transmitter
versus the Shield transceiver/CAN_RX-to-PB8 receive path; without a known-good
adapter, transceiver or oscilloscope, this report does not select one cause.

A temporary 500-kbit/s STM32 diagnostic using an `80%` sample point did not
improve the route: receive traffic began, then the adapter reached BUS-OFF and
the STM32 receive counter saturated. The diagnostic was rejected, the original
default firmware was reflashed and verified, its BTR returned to `0x001B0005`,
the interface was brought down and detached, and both devices were powered
off. No temporary firmware or raw frame log is retained in the repository.

Consequently `CAN-03` is `FAIL`, not `PASS`: identity and the periodic
device-to-host frame/decode/pairing route passed, while physical bidirectional
ACK, bounded host-to-device transfer, bus-off recovery and dual-bus concurrency
remain unaccepted. `vcan` results continue to be Host-only evidence.

## 2026-08-20 common-ground closure

The 2026-08-19 failure above remains the historical observation. The follow-up
separated receive-only and transmit-once behavior with bounded diagnostics from
`[057] db84dccd1eaaeaf5403b57482497c39a99d4ca3e`. The RX-only ELF SHA-256 was
`aec9f0d3b200467c330c4e6692d8448dd82bce47b0e1bb70927772162b48d003`;
the TX-once NART ELF SHA-256 was
`230468db28157c2b53a387ee92e6b0e623d50098081cb8a673b177fcbf0f1f68`.

Without a public USB-CAN-to-Shield ground, one host userspace send of `0x240`
was accepted 17 times by the STM32 RX-only diagnostic (`rxacc=17`) while the
host accumulated CAN errors. This proves lower-layer retries of one userspace
request and explains why the earlier Echo experiment amplified into repeated
responses. It does not support attributing every repeated frame to STM32
hardware retransmission.

After adding `USB-CAN GND <-> Shield GND`, the same one-send RX-only check gave
`rxacc=1`, `rxdrop=0`, and zero STM32/host error counters; both sides remained
ERROR-ACTIVE. The separate TX-once NART check delivered
`140#A15A54584F4E4345` to the host and reported `txc=1`, TEC/REC/LEC zero. Thus
the physical host-to-STM32 delivery/ACK direction and STM32-to-host/ACK
direction both passed with the common reference connected.

The verified default firmware SHA-256
`d611f7c4ed9935a70f2e43b8fce100d30f0c8d26f9f42e35bc2a9ab2efc0996e`
was then restored. A passive window captured 252 valid periodic frames: 42 each
of `0x240`, `0x241`, `0x340`, `0x341`, `0x342` and `0x440`; the interface
remained ERROR-ACTIVE with zero errors. The interface was subsequently brought
down and both devices were powered off.

A later manual host transmission under the then-current default firmware reused
`0x140`, which the seven-ID revision 1 contract assigns to STM32-produced status events, while normal producer
traffic was active. Its resulting errors are not a valid H06 result: revision 1
defined no host command/echo ID, and injecting any of the seven node-owned IDs
can create a same-ID data-phase conflict. The physical probe now enforces this
ownership boundary by rejecting `--observe --allow-send`.

Consequently `CAN-03` is `PASS_HARDWARE_LIMITED` for the admitted adapter,
Shield, wiring, 500 kbit/s Classical CAN, periodic telemetry, physical ACK in
both directions and default-firmware restore. This does not claim a host command
protocol, application-level response, naturally observed `0x140` event,
arbitrary CAN adapter interoperability, physical bus-off recovery, or physical
RS485+CAN concurrency. `BUS-02` therefore remains `NOT_RUN` and `HW-003` remains open.

## Dedicated diagnostic extension awaiting physical rerun

The default firmware now implements the read-only `0x540/0x541` diagnostic
pair with a one-slot response queue, duplicate suppression and a 100 ms minimum
interval for different tokens. Host and ARM verification establish the
software path only. Until the command above is run on the admitted common-GND
hardware route, the new application-level round trip remains
`NOT_RUN_HARDWARE`; the earlier physical ACK evidence is not rewritten as a
pass for this new protocol.

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
