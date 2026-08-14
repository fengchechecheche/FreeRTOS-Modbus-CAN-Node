# P5-S6-T05 soak runner and resource trend report

> Software status: `PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> Hardware status: `WAITING_FOR_HARDWARE`
> Formal 8-hour soak: `NOT_RUN`
> Baseline: `[034] d5048f029f8ef48a0bb19681782a776dfad152b6`
> Evidence policy: minute-level bounded samples and compact summaries only

## Purpose and boundary

`tools/soak_runner.py` validates a versioned, source-agnostic JSONL stream and
manages an explicitly supplied collector with bounded output, timeout and
process-group cleanup. It does not open a serial port, CAN interface or
debugger by default.

The no-hardware result proves runner/schema/trend behavior and repeated Host
process management. It does not prove STM32 uptime, real task jitter, stack
watermarks, bus throughput, sensor timing, physical recovery or an 8-hour run.

## Current preflight result

| Check | Result |
|---|---|
| runner self-test | PASS_HOST, 20 bounded checks |
| formal dry-run | collector `NOT_STARTED`, hardware/formal `NOT_RUN` |
| dual-bus Host preflight | 20/20 PASS, short process-management check |
| Host Debug | 21/21 PASS |
| Host Release | 21/21 PASS |
| Modbus contract | 878 facts PASS plus negative self-test |
| CAN contract | 114 facts PASS plus five-mutant self-test |
| BSP contract | 532 facts PASS plus negative self-test |
| ARM Debug/Release | PASS_CROSS_BUILD |
| firmware resources | unchanged from `[034]` |

The 20-iteration preflight completes quickly and is not a 10-minute soak.

## Runner modes

```bash
python3 tools/soak_runner.py --self-test
python3 tools/soak_runner.py --dry-run --phase formal
python3 tools/soak_runner.py --host-preflight --iterations 20
python3 tools/soak_runner.py --evaluate .private/soak/<session>/samples.jsonl
```

`--run` additionally requires explicit phase, duration, sample interval,
expected commit, expected firmware SHA-256, firmware path, build text/data/bss,
collector/board identity, output directory and collector argv. It uses
`shell=False`; no command string is evaluated by a shell. Formal mode rejects a
dirty worktree or malformed identity.

## Bounded input and cleanup

- schema revision is 1;
- each JSON line is at most 16 KiB;
- each session retains at most 600 samples;
- collector stderr is retained only up to 64 KiB;
- collector runs in its own process group;
- timeout or interruption sends terminate, waits five seconds, then kills if
  required;
- early clean exit still fails when samples do not cover the planned duration;
- malformed JSON, unknown schema, missing fields, nonzero child exit and
  cleanup/timeout failures return nonzero.

The runner output directory contains at most `samples.jsonl`, `summary.json`
and `summary.md`. A formal 8-hour session at 60-second sampling is about 481
samples, below the fixed limit.

## Trend decision

The runner checks every sample, adjacent deltas and up to four time windows.
It reports one of:

```text
PASS
REVIEW_REQUIRED
FAIL
```

Hard failures include identity drift, reset/fault, time or counter rollback,
measured task stack below 32 free words, declared-capacity violation, two
consecutive task/sensor no-progress samples, three consecutive error-growth
windows, two missing samples, incomplete planned duration and invalid schema.

An isolated counted error, one missing sample, one no-progress sample or a
watermark decrease that remains above 32 words is `REVIEW_REQUIRED`. This keeps
the gate useful without treating every recoverable event as automatic failure.

## Memory interpretation

FreeRTOS dynamic allocation is disabled, no `heap_x.c` is linked and linker
heap reserve is zero. The runner therefore records `heap_policy=disabled`
instead of inventing a changing free-heap number. ELF text/data/bss are
recorded once as session identity facts; runtime resource trends use measured
task watermarks, fixed-capacity queues and counters.

Current unchanged build values are:

| Build | text | data | bss | Flash | linked RAM |
|---|---:|---:|---:|---:|---:|
| Debug | 53272 | 160 | 13224 | 53432 | 13384 |
| Release | 44404 | 156 | 13216 | 44560 | 13372 |

## Hardware follow-up

The firmware already holds task, stack, queue, health, reset, RS485, CAN and
sensor snapshots internally, but it does not yet export one complete periodic
soak sample. A real collector must be reviewed after the board and both bus
paths pass their hardware gates. If a default-OFF USART2 diagnostic producer is
needed, that is a separate allowlist amendment with Host/ARM/resource review.

After collector admission, execute in order:

1. 10-minute smoke;
2. 60-minute pre-run;
3. user-authorized formal target of 8 hours.

The formal run must bind a clean commit and firmware SHA-256. A shorter run is
reported with its actual duration and cannot be called an 8-hour PASS.
