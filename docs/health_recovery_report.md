# P5-S3-T05 health, watchdog and recovery report

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`  
> Content status: `READY_FOR_CONTENT_REVIEW`  
> S3 bounded hardware gate: `PASS_WDG_01`
> IWDG runtime: `PASS_HARDWARE_BOUNDED`
> Reset-record persistence: `PASS_RESET_ONLY / NOT_CLAIMED_POWER_LOSS`
> Hardware status: `PARTIAL_PASS`
> Input baseline: `662695e7ce361f32f6f2d73c7fdae8ac76ecf6eb` (`[ 017 ]`)
> Hardware supplement base: `433225e7d8694fac7e22a5425350336b4ed483f5` (`[ 045 ]`)

## Health and feed contract

`health_task` evaluates one bounded sample every 1000 ms. It monitors progress
of `protocol_task`, `acquisition_task`, `can_task` and `diagnostic_task` from
their per-task release counters. It does not try to prove its own next release;
instead, it is the only owner of the watchdog feed decision. If the health task
stops, no later feed decision can be produced.

| State | Meaning | Feed decision |
|---|---|---|
| `BOOTSTRAP` | establish the first delta baseline | withheld; armed IWDG has about 8 s nominal margin |
| `SERVICEABLE` | all monitored tasks progressed | allowed |
| `DEGRADED` | bounded warning while tasks still progress | allowed |
| `RECOVERY_REQUIRED` | owner-local recovery remains inside its budget | allowed |
| `RESET_REQUIRED` | same task stalled for two epochs or recovery exhausted | withheld |
| `RESET_LOOP_LATCHED` | reset-loop policy reached its limit | withheld |

One no-progress epoch is degraded and still feedable; two consecutive epochs
for the same monitored task require reset and withhold the decision. A new
deadline miss, budget overrun, queue drop/full condition, snapshot contention
or RS485 error is diagnostic input but does not independently force a reset.
Saturated progress counters remain serviceable with a saturation warning rather
than becoming a false task stall.

The firmware health snapshot now includes each task's counters, the decision,
warning and stalled-task masks, recovery attempts and the current RS485 error
aggregate. Queue diagnostics include current pending, maximum pending, depth and
the T04 transport counters. No periodic text or raw health trace is emitted.

## Recovery ownership

Recovery remains with the resource owner. The health policy can request and
classify recovery, but it does not call SPI, I²C, RS485 or CAN HAL reset APIs.
An episode has a maximum of three failed attempts; exhaustion changes the
decision to `RESET_REQUIRED`.

`vTaskDelete` stays disabled. The five static tasks are not deleted and rebuilt
because they own notification, queue, mutex and peripheral state. A running
owner may later perform a cooperative state/peripheral reset. If the owner task
itself no longer progresses, the only safe candidate escalation is the system
watchdog/reset path.

Health events use a 12 B T04 by-value record and are published only when an
established health state changes. Bootstrap-to-serviceable is intentionally
silent. Event publication failure follows T04 drop-new accounting and never
changes the health decision.

## Reset reason and loop policy

At RTOS initialization, the target reads the RCC reset flags, preserves the raw
`RCC->CSR` value, normalizes power-on, brown-out, pin, software, IWDG, WWDG and
low-power reasons, then clears the hardware flags. Multiple known reasons keep
all bits and use a documented primary-reason precedence for the compact summary.
The target run observed and decoded a real IWDG reset reason.

The HAL-free reset-record codec defines a fixed magic/version/size, saturating
boot and consecutive-recovery-reset counts, last reason/raw flags/fault and a
checksum. Corrupt records return to a safe initialized record. Three consecutive
software/watchdog recovery resets latch the policy; a stable mark clears the
streak.

The fixed 32 B record is placed in `.noinit.app_reset_record`, outside the
startup-cleared BSS. It survived the deliberate IWDG reset with a valid
checksum and carried the smoke fault marker into the next boot. This establishes
retention across the tested reset path only. Backup registers/SRAM are not used,
and persistence across USB removal or other power loss is not claimed.

## Software verification

- Host Debug/Release: 22/22 PASS each;
- default Firmware Debug: PASS, `text/data/bss = 53992/160/13280` B;
- reset-smoke Firmware Debug: PASS, `text/data/bss = 54216/216/13280` B;
- health injection covers bootstrap, normal progress, one/two-epoch task stall,
  warning storm, bounded recovery, counter saturation and reset-loop latch;
- reset tests cover single/multiple/unknown reasons, checksum corruption,
  three recovery resets and stable-clear;
- `.noinit` is 32 B at `0x2000305c` in the default ELF and is outside `.bss`;
- ELF contains `app_health_policy_evaluate`, `app_reset_reason_decode`, T03
  task-notification and T04 queue send/receive symbols;
- extended static self-test rejects one-epoch reset, invalid recovery budget,
  degraded-feed regression, task delete, default-on reset smoke and a second
  feed owner;
- allocator calls and allocator ELF symbols remain absent;
- existing newlib nosys warnings remain unchanged and do not fail linking.

These checks prove the software state machine, source boundaries and ARM
linkage. The bounded target supplement below additionally proves normal feed,
one IWDG reset and reset-only record retention. It does not prove strict LSI
timing, power-loss retention, queue/mutex stress or real peripheral recovery.

## S3 evidence matrix

| Task | Software result | Hardware result | Remaining boundary |
|---|---|---|---|
| T01 scheduler/task model | PASS Host + ARM | `PASS_LIMITED` | scheduler progress observed; jitter/WCET not measured |
| T02 static memory/stack | PASS resource gate | `PASS_BARE_BOARD_LIMITED` | five stack watermarks sampled; sensor/bus workloads must remeasure |
| T03 IRQ/DMA notification | PASS Host + ARM + static contract | `LINKED_NOT_EXECUTED` | real IRQ, switch and latency not measured |
| T04 queue/mutex/ownership | PASS Host + ARM + static contract | `NOT_RUN` | watermark, contention and slow consumer not exercised |
| T05 health/reset policy | PASS Host + ARM + static contract | `PASS_WDG_01` | default feed and one reset passed; power-loss retention not claimed |

Therefore the allowed stage conclusion is:

```text
S3_software_gate = PASS_HOST + PASS_CROSS_BUILD
S3_hardware_gate = PASS_BOUNDED_WATCHDOG_AND_RESOURCE_SUPPLEMENT
```

This does not promote IRQ latency, WCET, queue stress, sensors, physical buses
or long-soak work that remains deferred.

## Hardware supplement

On 2026-08-15, STM32CubeProgrammer 2.17.0 and ST-LINK V2J48M35 were used with a
bare NUCLEO-F446RE; the full probe serial was not retained. The default Debug
ELF SHA-256 was
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca`.
Program/verify passed, and a 25 s VCP window contained one boot, five heartbeat
lines and no IWDG marker or reset. A Hot Plug snapshot then showed stable mark
1 and 46 successful feeds.

The temporary default-OFF smoke ELF SHA-256 was
`243ea87fe6d601bc336717d9386e4a657d118dad40ded35a279bef4ae85c6955`.
After three normal feeds, `P5 S3 T05 IWDG WITHHOLD` appeared at about 4.10 s.
The next boot appeared at 11.69 s, a 7.59 s interval within the deliberately
loose 4--20 s gate, followed once by `P5 S3 T05 IWDG RESET OK`. No third boot
occurred during the remaining 28 s observation.

The post-reset state was `withholding=0`, `completed=1`, `stable=1` with 42
feeds. The retained record reported magic `0x50355252`, version 1, size 32,
boot count 2, zero consecutive recovery resets, an IWDG-containing reason,
fault 0 and checksum `0x984e27d0`; independent recomputation matched. The
default ELF was then reflashed and a final 20 s window again showed one boot,
five heartbeats and no smoke marker.

The bare-board resource snapshots measured minimum-free stack words of
`215 / 168 / 115 / 53 / 215` for protocol/acquisition/CAN/health/diagnostic.
All exceed the 32-word gate. Queue maximum pending, drops and snapshot
contention were zero. One historical acquisition miss/deadline/budget sample
did not grow across a second read and is not promoted into a timing guarantee.
No raw per-epoch or programmer log is retained.

## S4-T05 sensor-local degradation

Health input now includes sensor unavailable, stale-source and recovery masks.
Each nonzero mask sets a distinct warning bit and makes an otherwise
serviceable epoch `DEGRADED`; feed remains allowed. These warning branches do
not increment the global recovery attempt, set `reset_required`, call a bus
recovery API or enter fail-stop. Existing task-stall, exhausted-recovery and
reset-loop rules remain the only reset escalation paths.

The combined Host oracle covers persistent single-device failure and recovery
while the other sources continue. This is a pure-policy/software result. The
IWDG result is now bounded PASS. Real sensor disconnect and recovery remain
`NOT_RUN`; sensor warnings still allow feed by contract.

## S6-T05 long-soak observation boundary

The bounded soak runner evaluates versioned samples for task/sensor progress,
stack watermarks, queue/CAN capacity, counter rollback, sustained error growth,
fault/reset indicators and missing-duration coverage. Isolated events may
require review; persistent stalls, capacity violations, reset/fault, rollback
or a measured stack watermark below 32 free words fail the session.

The watchdog path, reset-only record retention and bare-board task watermarks
now have bounded target evidence. Bus recovery, workload watermarks and the
10-minute/60-minute/8-hour runs remain `NOT_RUN` pending the relevant hardware
and a separately reviewed collector.
