# P5-S3-T05 health, watchdog and recovery report

> Software status: `PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`  
> Content status: `READY_FOR_CONTENT_REVIEW`  
> S3 software gate: `PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`  
> IWDG runtime: `NOT_CONFIGURED / NOT_RUN`  
> Reset-record persistence: `NOT_IMPLEMENTED / NOT_RUN`  
> Hardware status: `WAITING_FOR_HARDWARE`  
> Input baseline: `662695e7ce361f32f6f2d73c7fdae8ac76ecf6eb` (`[ 017 ]`)

## Health and feed contract

`health_task` evaluates one bounded sample every 1000 ms. It monitors progress
of `protocol_task`, `acquisition_task`, `can_task` and `diagnostic_task` from
their per-task release counters. It does not try to prove its own next release;
instead, it is the only owner of the watchdog feed decision. If the health task
stops, no later feed decision can be produced.

| State | Meaning | Feed decision |
|---|---|---|
| `BOOTSTRAP` | establish the first delta baseline | withheld; target IWDG is not armed |
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
This path compiles and links on ARM; no real reset reason has been observed yet.

The HAL-free reset-record codec defines a fixed magic/version/size, saturating
boot and consecutive-recovery-reset counts, last reason/raw flags/fault and a
checksum. Corrupt records return to a safe initialized record. Three consecutive
software/watchdog recovery resets latch the policy; a stable mark clears the
streak.

No target retention backend is selected. The record is not placed in `.noinit`,
backup registers or backup SRAM, so persistence is honestly
`NOT_IMPLEMENTED / NOT_RUN`. The Host codec test is not evidence of data
surviving an STM32 reset or power loss.

## Software verification

- Host Debug: 9/9 PASS;
- Host Release: 9/9 PASS;
- health injection covers bootstrap, normal progress, one/two-epoch task stall,
  warning storm, bounded recovery, counter saturation and reset-loop latch;
- reset tests cover single/multiple/unknown reasons, checksum corruption,
  three recovery resets and stable-clear;
- Firmware Debug: PASS, `text/data/bss = 25192/160/9288` B;
- Firmware Release: PASS, `text/data/bss = 21908/156/9284` B;
- static resource contract: PASS; Debug RAM 9448 B, Release RAM 9440 B;
- ELF contains `app_health_policy_evaluate`, `app_reset_reason_decode`, T03
  task-notification and T04 queue send/receive symbols;
- extended static self-test rejects one-epoch reset, invalid recovery budget,
  degraded-feed regression, task delete, default IWDG and a second/direct feed;
- allocator calls and allocator ELF symbols remain absent;
- existing newlib nosys warnings remain unchanged and do not fail linking.

These checks prove the software state machine, source boundaries and ARM
linkage. They do not prove scheduler execution, IWDG timing/reset, reset-record
retention, queue/mutex runtime pressure or real peripheral recovery.

## S3 evidence matrix

| Task | Software result | Hardware result | Remaining boundary |
|---|---|---|---|
| T01 scheduler/task model | PASS Host + ARM | `WAITING_FOR_HARDWARE` | periods, jitter and deadlines not measured |
| T02 static memory/stack | PASS resource gate | `NOT_MEASURED` | task stack watermarks not measured |
| T03 IRQ/DMA notification | PASS Host + ARM + static contract | `LINKED_NOT_EXECUTED` | real IRQ, switch and latency not measured |
| T04 queue/mutex/ownership | PASS Host + ARM + static contract | `NOT_RUN` | watermark, contention and slow consumer not exercised |
| T05 health/reset policy | PASS Host + ARM + static contract | `NOT_RUN` | IWDG, retention and recovery not exercised |

Therefore the allowed stage conclusion is:

```text
S3_software_gate = PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE
S3_hardware_gate = WAITING_FOR_HARDWARE
```

S4 HAL-free and cross-build work may proceed. S3 must not be labelled complete
or `PASS_HARDWARE` until the deferred hardware gate is run.

## Hardware supplement

After board admission, use a separately reviewed, default-OFF fault option and
a deliberately loose IWDG timeout. Confirm normal feed ownership first, then
withhold one feed, observe reset, read the IWDG reset reason and verify that the
device does not enter an unbounded reset loop. In the same bounded run, sample
task stack watermarks, diagnostic queue maximum/drop and snapshot contention.

Only compact before/after summaries are required. Raw per-epoch logs, queue
traces, mutex timelines and reset evidence bundles are created only for a
failure that needs reproduction.

## S4-T05 sensor-local degradation

Health input now includes sensor unavailable, stale-source and recovery masks.
Each nonzero mask sets a distinct warning bit and makes an otherwise
serviceable epoch `DEGRADED`; feed remains allowed. These warning branches do
not increment the global recovery attempt, set `reset_required`, call a bus
recovery API or enter fail-stop. Existing task-stall, exhausted-recovery and
reset-loop rules remain the only reset escalation paths.

The combined Host oracle covers persistent single-device failure and recovery
while the other sources continue. This is a pure-policy/software result. The
IWDG is still not configured, and real sensor disconnect, feed timing and
reset behavior remain `NOT_RUN`.
