# P5-S6-T02 bxCAN runtime

> Software status: `CAN_RUNTIME_CANDIDATE_IMPLEMENTED`
> Hardware status: `WAITING_FOR_HARDWARE`
> Review gate: `READY_FOR_CONTENT_REVIEW`

## Runtime boundary

`can_task` owns filter/start/stop, encoding projection, TX admission and
bus-off recovery. It waits for an IRQ task notification or the existing 100 ms
release; no sixth task, dynamic allocation, RTOS queue, mutex or semaphore is
added. Sensor values are copied from the unified snapshot before HAL calls, so
CAN never reads SPI/I2C directly and a CAN failure cannot stop acquisition or
Modbus.

The two exact 16-bit list filter banks admit only `0x140`, `0x240`, `0x241`,
`0x340`, `0x341`, `0x342` and `0x440`. Revision 1 accepts standard data frames
with DLC 8 only. The IRQ path performs one HAL RX copy into a four-frame ring,
merges TX/error bits in a fixed mailbox, and wakes `can_task`; parsing, sending
and recovery stay in task context.

## Bounded scheduling and recovery

- Every 1 s, six telemetry frames are projected from one copied measurement and
  health view. Latest periodic data replaces older unsent data.
- The BME primary/secondary pair keeps one sequence and cannot be replaced after
  only its primary half has been accepted by HAL.
- Eight event slots coalesce equal code/source pairs. Two consecutive events at
  most may precede a waiting periodic frame.
- Each service drains at most two RX frames and admits at most three TX frames.
  `HAL_BUSY` leaves the selected frame at the queue head.
- A bus-off or start failure waits at least 1000 ms before task-context restart.
  Three failed recovery attempts enter `RECOVERY_LATCHED`; other tasks continue.

`app_rtos_get_can_snapshot()` exposes controller state, start/recovery/error
counters, queue counters, last HAL error and IRQ RX/TX/drop counters. It is a
read-only troubleshooting view, not physical acceptance evidence.

## Troubleshooting order

1. If state is `RECOVERY_LATCHED`, inspect `last_hal_error`, `bus_off_transitions`,
   `start_failures` and `recovery_attempts`; do not repeatedly reset the node.
2. If RX is absent, confirm both CAN1 IRQ priorities are 6/0, then compare
   `rx_accepted`, `rx_invalid` and `rx_dropped`. Invalid ID/IDE/RTR/DLC is rejected
   before task processing.
3. If TX stalls, compare `pending_frames`, `hal_busy`, `tx_completed` and
   `tx_aborted`. A growing pending count with no completion points to controller
   state, transceiver, termination or missing ACK rather than sensor ownership.
4. If only climate data appears inconsistent, match revision and sequence across
   `0x340/0x341`; never combine mismatched halves.

Until NUCLEO-F446RE, transceiver, CANH/CANL/GND, two end terminators and a known
500 kbit/s peer are available, waveform, ACK, arbitration, real bus-off recovery
and SocketCAN/candump remain `NOT_RUN`.
