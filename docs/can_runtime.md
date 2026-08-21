# P5-S6-T02 bxCAN runtime

> Software status: `CAN_RUNTIME_CANDIDATE_IMPLEMENTED`
> Hardware status: `PASS`
> Review gate: `CONTENT_REVIEW_PASSED`

## Runtime boundary

`can_task` owns filter/start/stop, encoding projection, TX admission and
bus-off recovery. It waits for an IRQ task notification or the existing 100 ms
release; no sixth task, dynamic allocation, RTOS queue, mutex or semaphore is
added. Sensor values are copied from the unified snapshot before HAL calls, so
CAN never reads SPI/I2C directly and a CAN failure cannot stop acquisition or
Modbus.

One active exact 16-bit list filter bank repeats the Host-owned diagnostic
request `0x540` in all four hardware entries, so the receive path admits only
that ID. The seven STM32-owned producer IDs and the outgoing `0x541` response
are rejected by the RX direction contract; `0x541` is admitted by the separate
TX contract and consumes no receive-filter entry. Revision 1 accepts standard
data frames with DLC 8 only.
The IRQ path performs one HAL RX copy into a four-frame ring,
merges TX/error bits in a fixed mailbox, and wakes `can_task`; parsing, sending
and recovery stay in task context.

## Bounded scheduling and recovery

- Every 1 s, six telemetry frames are projected from one copied measurement and
  health view. Latest periodic data replaces older unsent data.
- The BME primary/secondary pair keeps one sequence and cannot be replaced after
  only its primary half has been accepted by HAL.
- Eight event slots coalesce equal code/source pairs. Two consecutive events at
  most may precede a waiting periodic frame.
- One high-priority diagnostic response slot handles a valid `0x540` request.
  Exact token duplicates are suppressed and different tokens are limited to one
  per 100 ms; malformed requests receive no response.
- Each service drains at most two RX frames and admits at most three TX frames.
  `HAL_BUSY` leaves the selected frame at the queue head.
- A bus-off or start failure waits at least 1000 ms before task-context restart.
  Three failed recovery attempts enter `RECOVERY_LATCHED`; other tasks continue.

`app_rtos_get_can_snapshot()` exposes controller state, start/recovery/error
counters, queue counters, last HAL error and IRQ RX/TX/drop counters. It is a
read-only troubleshooting view, not physical acceptance evidence.

## SocketCAN preflight

The host-only `tools/can_hil_probe.py` checks the frozen map, decodes bounded
SocketCAN frames,
counts duplicates without failing normal periodic repetition, and pairs
`0x340/0x341` only when their sequence matches. Its self-test, dry-run and
12-frame `vcan` matrix are `PASS_HOST`; see
[`can_hil_report.md`](can_hil_report.md).

The current environment has `vcan` and `gs_usb` kernel modules. `can-utils`
2023.03-1 is installed, and a one-frame `candump`/`cansend` smoke on temporary
`vcan0` is `PASS_HOST`. The admitted candleLight physical route is `PASS` for
periodic telemetry, bounded physical ACK in both
directions and the dedicated `0x540/0x541` application round trip. A local
SocketCAN loopback or adapter TX echo alone is still not an ACK or MCU
application-acceptance result.

## Default WSL physical CAN setup

Every physical CAN session must explicitly use 500 kbit/s and sample point
`0.75`. Omitting `sample-point 0.75` selected `0.875` in the tested host
environment and produced a non-working route; a run that reports `0.875` is
invalid for this project and must not be promoted to hardware evidence.

In Windows PowerShell, first refresh the USB identity and attach the already
shared candleLight device with the current usbipd syntax:

```powershell
usbipd.exe list
usbipd.exe attach --wsl --busid 5-2
```

`5-2` is the BUSID observed in the 2026-08-21 pass, not a permanent hardware
identity. Run `usbipd.exe list` again after changing USB port or rebooting and
replace it when necessary. Do not use the obsolete
`usbipd.exe attach --wsl Ubuntu-24.04-STM32 ...` form; current usbipd selects
the active WSL distribution automatically.

The `stm32` account may require an interactive sudo password. For a bounded
host-side test launched from Windows, use WSL's root startup parameter instead
of first failing with `sudo -n`:

```powershell
wsl.exe -d Ubuntu-24.04-STM32 -u root -- ip link set can0 down
wsl.exe -d Ubuntu-24.04-STM32 -u root -- ip link set can0 type can bitrate 500000 sample-point 0.75
wsl.exe -d Ubuntu-24.04-STM32 -u root -- ip link set can0 up
wsl.exe -d Ubuntu-24.04-STM32 -- ip -details -statistics link show can0
```

The gate is `bitrate 500000`, `sample-point 0.750`, `ERROR-ACTIVE` and zero
initial error counters. The admitted `gs_usb` adapter does not support
`restart-ms`; do not add that option to the default command. Bring `can0` down
after a bounded transmit test.

The physical transmit modes are deliberately limited to the fixed diagnostic
pair. `--diagnostic-ping` sends one `0x540` request and waits at most five
seconds for one matching `0x541`. `--diagnostic-series` opens one SocketCAN
socket, sends a bounded sequence of the same request at fixed intervals, drains
traffic for the whole planned duration and closes the socket only after the
final interval. Neither mode exposes arbitrary CAN ID or payload arguments, and
neither replaces interface/error counters or the VCP receive marker.

Use that repository probe rather than an ad-hoc create/send/close script. A
2026-08-21 lifecycle A/B test showed zero error frames with one socket retained
for 60 s, while an otherwise equivalent separate TX socket closed immediately
after one send produced 5678 error frames and repeated
warning/passive/bus-off state changes. This establishes a sufficient trigger in
this admitted candleLight/`gs_usb`/USB-IP bench, not a component-level root
cause or a general SocketCAN guarantee. Public implementations make the
lifecycle interaction plausible, but no public source located during the
review directly reproduces this immediate-close storm.

A later repeated test also showed that reopening `--diagnostic-ping` for every
request is not an accepted long-phase substitute: three calls were clean, but
the fourth obtained its application reply and was followed by five
error-warning and one error-passive observations. Therefore use
`--diagnostic-ping` only for a single bounded check. For a repeated five-minute
phase use one invocation and one persistent socket:

```bash
python3 tools/can_hil_probe.py \
  --diagnostic-series --interface can0 \
  --sequence 0x51 --nonce 0x51020304 \
  --diagnostic-count 5 --diagnostic-interval 60 \
  --diagnostic-hold-after 120 \
  --response-timeout 5
```

The active measurement duration is `count * interval` (300 s above); sequence
and nonce advance once per request. `--diagnostic-hold-after` keeps the same
socket open for a bounded shutdown guard after measurement. Power off the
NUCLEO during that guard, then allow the probe to close the socket. The series
must produce exactly one matching response
per request, no unexpected/duplicate response, no error frame on the dedicated
monitor and no interface-state/counter growth. The implementation has Host
self-test coverage, but remains a hardware candidate until the strict replay
passes. Application success alone is insufficient; always inspect before/after
interface counters.

The admitted adapter does not expose `one-shot`, `presume-ack` or
`berr-reporting` through this `gs_usb` driver. Do not compensate by changing the
STM32 default NART policy. After the probe completes, first power off the node,
then bring `can0` down and detach the adapter; do not remove the ACK peer while
the default node is still sending periodic telemetry.

For a dedicated error-frame monitor, use the CAN error-mask syntax:

```bash
candump -e 'can0,0~0,#FFFFFFFF'
```

`0~0` excludes ordinary data frames and `#FFFFFFFF` enables all CAN error
classes through `CAN_RAW_ERR_FILTER`. Do not combine `candump -L` with `-e`:
the log-file formatter rejects the human-readable error decoder. If ordinary
timestamped traffic is also required, run a separate `candump -L can0`
process. A normal CAN-ID filter such as `20000000:20000000` is not a substitute
for the `#<error_mask>` syntax and must not be used as zero-error evidence.

`--observe-seconds` is an upper time bound, not a duration guarantee: the probe
also stops after its bounded 128-frame capture limit. Formal elapsed-time gates
must therefore use an independent monotonic timer and treat probe observations
as bounded samples inside that interval.

## Dual-bus backpressure preflight

P5-S6-T04 directly links the CAN scheduler/controller with the production
Modbus stream, RS485 state, task model and health/diagnostic policies. The
bounded D01-D08 Host matrix confirms:

- ordinary telemetry replaces an unsent older value latest-wins;
- `HAL_BUSY` preserves the selected frame and token;
- a full depth-8 event FIFO records the rejected event instead of silently
  growing storage;
- the two-event burst limit allows waiting health/periodic traffic to proceed;
- `0x340/0x341` retains one sequence under event pressure;
- bus-off waits 1000 virtual ms, attempts recovery three times and latches while
  Modbus, acquisition and health task models continue.

The maximum observed pending count in the deliberate event-pressure scenario
is 11 fixed frames. This is not a new queue-size contract or a physical
throughput result. See [`dual_bus_fault_matrix.md`](dual_bus_fault_matrix.md).

## Troubleshooting order

1. If state is `RECOVERY_LATCHED`, inspect `last_hal_error`, `bus_off_transitions`,
   `start_failures` and `recovery_attempts`; do not repeatedly reset the node.
2. If RX is absent, confirm both CAN1 IRQ priorities are 6/0, then compare
   `rx_accepted`, `rx_invalid` and `rx_dropped`. Invalid ID/IDE/RTR/DLC is rejected
   before task processing.
   For the dedicated path, absence of both `P5CANDIAG1` and `0x541` points to
   Host transmission, filtering or the physical receive path; a VCP marker
   without `0x541` narrows the fault to response admission/transmission.
3. If TX stalls, compare `pending_frames`, `hal_busy`, `tx_completed` and
   `tx_aborted`. A growing pending count with no completion points to controller
   state, transceiver, termination or missing ACK rather than sensor ownership.
4. If only climate data appears inconsistent, match revision and sequence across
   `0x340/0x341`; never combine mismatched halves.

The admitted NUCLEO-F446RE, Shield, common-GND candleLight route has passed
bounded 500 kbit/s telemetry, physical ACK and the read-only diagnostic round
trip at sample point `0.75`. Waveform measurement, arbitrary-adapter
interoperability, natural arbitration/event observation, physical bus-off
recovery and simultaneous physical RS485+CAN operation remain unclaimed or
`NOT_RUN`.
