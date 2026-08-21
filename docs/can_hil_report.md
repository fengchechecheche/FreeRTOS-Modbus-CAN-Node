# P5-S6-T03 SocketCAN / candleLight HIL report

> Software status: `PASS_HOST`
> Hardware status: `PASS`
> Integration status: `NOT_RUN`
> Baseline: `4a581aea31dc97c6deec51726ed0ccf7325579bd` (`[032]`)
> Historical hardware supplement source: `285a894d52ff817bf9683a5a7034c01c27675e89`
> Current hardware supplement source: `[060] 39c6114eba1b547d29741d142582f3828a902dc7`

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
| physical CAN netdev during original Host-only preflight | none | EXPECTED_NO_HARDWARE |

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

Before opening a physical interface, apply the mandatory Windows/WSL setup in
[`can_runtime.md`](can_runtime.md#default-wsl-physical-can-setup). The project
default is explicitly `500000 bit/s` with `sample-point 0.75`; a session that
omits the sample point or reports `0.875` is not valid hardware evidence.

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
| H02 | 500 kbit/s, UP, initial ERROR-ACTIVE | PASS: mandatory project sample point `75%` |
| H03 | six periodic IDs in a bounded default-firmware window | PASS: 252/252 accepted, 42 per ID |
| H04 | revision/DLC/fields decode | PASS |
| H05 | one matching `0x340/0x341` pair | PASS: 42 pairs observed |
| H06 | one bounded host-to-device frame with physical ACK | PASS: `RX_ONLY rxacc=1`, both sides ERROR-ACTIVE and zero errors with common GND |
| H07 | state event if naturally observed | NOT_OBSERVED_ALLOWED |
| H08 | interface-down, USB detach and default-firmware restore | PASS |

## CAN 故障复盘与最终解决路径

本节把后续三个日期章节中的原始观察重新按因果关系归纳。历史失败记录仍保留在下文；这里的目的
不是把所有异常归结为一个“CAN 初始化错误”，而是区分物理链路、测试协议、应用处理和诊断工具
四个层次。

### 1. 星期三初始故障的时间线重建

以下时间点来自当时会话。早期操作没有记录 USB-CAN 与 NUCLEO 的实际上电先后，因此必须把
“日志事实”和“事后最可能解释”分开：

1. **18:30～18:52：第一次重新上电没有规定顺序。** 18:52 的操作只要求重新给 USB-CAN 和
   NUCLEO 上电，没有要求 USB-CAN 先进入可 ACK 状态。实际顺序未被记录，因而不能确认当时一定是
   NUCLEO 先上电。但如果确实如此，默认固件会在没有其他活动节点驱动 ACK slot 的条件下发送周期
   帧，CAN 控制器会报告 ACK error、自动重试并提高 STM32 发送错误计数；持续失败足以使 STM32
   进入 ERROR-PASSIVE。后续 TX-once 对照中，无对端的一次发送得到 `TEC=8/LEC=3`，证明这条
   机制在本台架上真实存在。USB-CAN 后上电时可能接入一个已经存在重试和错误状态的总线，但仅凭
   当时现象不能证明 USB-CAN 自身的 ERROR-PASSIVE 完全由上电顺序造成。
2. **21:06～21:07：第二次重新上电仍没有规定顺序。** 21:06 的指令同样没有固定顺序；根据事后
   回忆，21:07 轮次可能是 USB-CAN 先上电、NUCLEO 后上电，随后 H01～H05 首次通过。这与
   “先让唯一对端具备 ACK 能力，再启动周期发送节点”的机制一致，因此足以把固定上电顺序纳入
   后续准入流程；但该轮还伴随终端设置和重新上电，且早期实际顺序没有独立记录，所以不能把它写成
   只改变上电顺序的严格 A/B 实验。
3. **21:09：H06 使用 `0x140` 后出现 BUS-OFF。** Host 使用了 STM32-owned ID，这在协议设计上
   是错误测试方法；但 `0x140` 是事件帧而不是周期帧。现有证据没有显示 STM32 与 Host 在同一时刻
   发送不同内容的 `0x140`，所以不能把本次 BUS-OFF 确认为“同 ID 数据冲突”单独造成。该结果只
   能同时支持两点：原 H06 的 ID 所有权不合法；当时总线仍可能受未清洁错误状态、ACK、公共 GND
   或 Host 位时序配置影响。后来用 Host-owned `0x540` 和 STM32-owned `0x541` 完成零错误往返，
   才是 Host→STM32 路线的有效闭环。

据此，后续冷启动顺序固定为：两端断电并等待，先给 USB-CAN 上电并完成 WSL attach、500 kbit/s、
Host sample point `0.75` 和接口 ERROR-ACTIVE 检查，再给 NUCLEO 上电或复位。该顺序用于建立
可复现的干净基线，不表示 CAN 产品在正常运行中只能按此顺序启动；正式系统仍应具备离线节点恢复
能力。

### 2. 实际遇到的故障及证据边界

| 阶段 | 观察结果 | 证据支持的判断 | 不能据此断言 |
|---|---|---|---|
| 初始上电顺序未固定 | 18:52 和 21:06 两轮均只要求两端重新上电；可能在 USB-CAN 尚不能 ACK 时先启动了默认固件。21:07 的成功轮次可能采用了 USB-CAN 先上电 | 无对端 ACK 会使发送端自动重试并提高 TEC；固定“USB-CAN 准备完成后再启动 NUCLEO”可避免以脏错误状态开始验收 | 早期顺序没有被记录，不能认定它是全部故障的已证实唯一根因，也不能仅凭 Host 状态断言是哪一端先进入 ERROR-PASSIVE |
| 初始被动接收 | 在 Host sample point `0.75` 下可连续收到六种周期 ID；相同台架采用 `0.875` 时不能形成有效路线 | STM32 CAN1 初始化、PB8/PB9 路由、Shield 发送器和 Host 接收器并非系统性失效；`0.75` 是该 Host/适配器台架的强制配置 | 不能把 Host 的 `0.75` 当作 MCU sample point；MCU 仍是 CubeMX 冻结的 `86.67%` |
| 初始 Host 主动发送 | 普通测试 ID `0x123` 被硬件过滤；即使收到旧七种 ID，原应用也只取出后执行 `(void)frame` | 原 revision 1 是 STM32 单向遥测合同，没有 Host 命令、解析或应答语义；“应用看不到响应”不等于 CAN1 RX/IRQ 失效 | 不能用 `cansend` 返回成功或本地 TX echo 证明 MCU 已接收 |
| 使用 `0x140/0x240/.../0x440` 做 Host 请求 | 这些 ID 归默认固件所有；Host 注入 `0x140` 后曾出现 ERROR-PASSIVE/BUS-OFF | 七种 ID 的所有权属于 STM32；若 Host 与节点同时发送相同 ID、不同内容，仲裁阶段无法选出唯一发送者，后续不同位会触发 CAN 错误，因此该操作不是合法的 Host→STM32 验收 | `0x140` 不是周期帧，现有记录没有证明两端同时发送它；不能把该轮 BUS-OFF 唯一归因于同 ID 冲突，也不能用该失败否定 Host→STM32 物理接收 |
| 临时 Echo 诊断 | 无公共 GND 时，一次 userspace send 在 RX-only 固件中先后出现 `rxacc=5` 和 `rxacc=17`；Echo 逻辑会对每次呈交放大为应答 | 重复帧在 STM32 不发送 Echo 时已经存在，说明至少包含发送端 CAN 控制器在未稳定取得 ACK 时的底层重试；Echo 只是放大器 | 不能把风暴唯一归因于 USB-CAN 应用重复发送，也不能唯一归因于 STM32 应答的自动重发 |
| RX-only/TX-once 分离 | USB-CAN 断电时 TX-once 得到 ACK error、`TEC=8/LEC=3`，这是无对端的预期对照；对端正常且公共 GND 接通后 TX-once 为 `txc=1`、TEC/REC/LEC 全零 | 正常接线下 STM32→Host 能取得 ACK；无对端时的错误不是项目故障。TX-once 的 NART 只用于把诊断限制为一次硬件发送尝试 | 不能据此说默认固件必须永久关闭自动重发；NART 不是 Echo 风暴的唯一根因或最终修复 |
| 公共 GND A/B 对照 | 无公共 GND 时 RX-only 为 `rxacc=17`；接通 `USB-CAN GND↔Shield GND` 后，同一 one-send 为 `rxacc=1`，双方 ERROR-ACTIVE 且错误计数为零 | 缺少可靠公共参考是重复呈交、ACK 不稳定和前期偶发收发的重要物理因素；无 GND 偶尔通信只能视为通过 USB/ST-LINK 等形成了不可控参考路径 | 不能把“某次无 GND 也收到帧”升级为允许省略公共 GND |
| 最终应用往返 | `0x540` 请求得到唯一匹配 `0x541`，同时出现 `P5CANDIAG1 ... reply=QUEUED`；前后错误计数为零 | Host→STM32 物理接收、硬件过滤、任务解析、应答排队、STM32→Host 发送和双向 ACK 在准入路线内全部闭环 | 不证明任意 ID、任意适配器、波形质量、物理 bus-off 恢复或双总线并发 |

`0x140` 是事件帧而不是周期帧，因此 H07 没有自然观察到它属于
`NOT_OBSERVED_ALLOWED`，不是 CAN 故障，也不需要改用其他 STM32-owned ID 从 Host 注入来补测。

### 3. 最符合完整证据的根因组合

前期现象不是一个单点根因，而是以下因素叠加：

1. **启动基线、物理和 Host 配置不稳定**：早期没有固定两端上电顺序，可能在唯一 ACK 对端尚未
   准备时先启动周期发送节点；缺少明确公共 GND 时 ACK 路径也不稳定；Host 曾采用未验证的
   `0.875` sample point，而通过路线要求显式 `0.75`。将 STM32 临时改为 `80%` 没有修复路线，
   因而“只改 MCU sample point”已被实验否定。上电顺序是假设，公共 GND 和 Host `0.75` 则有
   后续对照或通过路线支持。
2. **测试 ID 所有权错误**：`0x140/0x240/0x241/0x340/0x341/0x342/0x440` 都是
   STM32 producer-owned ID，不是 Host 命令。Host 与节点同时用相同 ID 发送不同数据时可能制造
   总线错误，且即使没有同时发送也会破坏单一生产者语义，因此不能作为接收功能测试。21:09 的
   `0x140` BUS-OFF 证明了测试方法无效，但没有证明同 ID 同时发送是该轮唯一物理根因。
3. **原始应用合同没有请求/应答**：旧代码确实在取出 RX frame 后丢弃，但这是当时单向遥测
   revision 的实现边界，不是 CAN1 初始化或中断配置缺陷。`0x123` 又会在到达应用前被 exact
   filter 拒绝。
4. **临时 Echo 放大底层重试**：无公共 GND 时，RX-only 已证明一次 userspace send 会被硬件层
   重复呈交；Echo 对每次呈交应答使现象看起来像“STM32 风暴”。TX-once NART 固件只负责有界
   定位，不能单独解释或修复该问题。

因此，现有证据不支持“CAN1 初始化错误”“PB8/PB9 未重映射”“USB-CAN 一定损坏”或“只因
STM32 未使用 NART”这些单一结论。周期遥测 252 帧、RX-only、TX-once 和最终应用往返已经分别
反证了 CAN1 TX/RX、IRQ 和 Shield 基本路径的系统性故障。

### 4. NXP 官方资料给出的三层依据

1. **ID 所有权和内容标签**：NXP AN2726 说明，CAN 系统中的每种报文应定义唯一标识符，标识符
   同时用于标记固定的报文内容。因此项目把七种遥测/事件 ID 定义为 STM32 producer-owned，Host
   不应复用这些 ID 发送另一种测试内容。
2. **相同仲裁字段、不同数据的直接冲突案例**：NXP AN94088 明确讨论了不同 Host 同时发送报文
   的情况：如果报文在仲裁字段相同、到数据字段才出现差异，叠加会导致错误；只有内容相同的报文
   叠加才可能不产生这种错误。这直接支持“相同 ID、不同内容、同时发送”是危险条件。
3. **仲裁、ACK、错误帧和自动重发机制**：NXP AN1776 说明仲裁字段包含 ID 和 RTR，所有活动
   节点会监视总线位；正确接收者在 ACK slot 驱动 dominant，发送者未检测到 ACK 会重发；检测到
   协议错误时会发送 error frame 并终止当前帧。这解释了无对端 ACK、同 ID 在仲裁后出现不同位，
   以及重复错误导致错误计数增长的协议链路。

这些资料证明的是通用 CAN 机制，不会自动证明某次历史故障的唯一根因。对 21:09 的 `0x140`
轮次，缺少“STM32 与 Host 同时发送不同 `0x140`”的总线抓包，因此文档只把 ID 所有权错误列为
已证实问题，把同 ID 物理冲突保留为条件性风险。

### 5. 最终采用的解决方案

**准入硬件与 Host 配置**：

- Shield `D14↔CAN_TX`、`D15↔CAN_RX`，MCU 使用 PB9/CAN1_TX、PB8/CAN1_RX；
- `CANH→CANH`、`CANL→CANL`、`USB-CAN GND→Shield GND`；最终验收保持 USB-CAN
  `120R` 设置；
- Classical CAN 2.0A、500 kbit/s、11-bit standard data frame、DLC 8、非 CAN FD；
- 冷启动验收时先给 USB-CAN 上电，完成 WSL attach、接口配置和 ERROR-ACTIVE 检查，再给
  NUCLEO 上电；不再使用“同时重新上电”这种顺序不明确的操作；
- Host 必须显式执行 `ip link set can0 type can bitrate 500000 sample-point 0.75`，随后确认
  `sample-point 0.750`、ERROR-ACTIVE 和零初始错误；不能依赖自动得到的 `0.875`；
- 发生 ERROR-PASSIVE/BUS-OFF 的失败轮次先双端断电冷启动，清除残留状态后再按准入配置复测；
  这只是恢复测试基线，不是通信协议的一部分。

**最终软件与操作约束**：

- 新增 Host-owned `0x540` 只读诊断请求和 STM32-owned `0x541` 应答；Host 不再发送七种
  producer-owned 遥测/事件 ID；
- RX/TX 白名单分离：一个 16-bit ID-list filter bank 的四条硬件表项均填 `0x540`，RX 只接收
  请求；TX 独立允许七种节点帧和 `0x541`，避免自有帧回灌和 ID 所有权混乱；
- 默认固件在 task context 解码请求，使用单应答槽、相同 token 去重和不同 token 100 ms 限速；
  `0x541` 不在 RX filter 中，因此不会形成应用层自激 Echo；
- Host 只使用 `tools/can_hil_probe.py --diagnostic-ping` 发送一个固定格式请求，同时以唯一匹配
  `0x541`、VCP `P5CANDIAG1` 标记及前后错误计数作为通过条件；一次发送完成后将 `can0` 置 down，
  防止失败状态下继续重试并保持下一轮基线清晰。

最终解决方案不是某一个跳线或某一行代码，而是“公共 GND + 明确终端与 500 kbit/s/Host 0.75
配置 + 正确 ID 所有权 + 专用有界请求/应答 + 分离 RX/TX 白名单”的组合。该组合已在下文
2026-08-20 和 2026-08-21 两轮证据中闭环。

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

Without a common USB-CAN-to-Shield ground, one host userspace send of `0x240`
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

## 2026-08-21 dedicated diagnostic physical pass

The read-only `0x540/0x541` application round trip was run on source
`[060] 39c6114eba1b547d29741d142582f3828a902dc7`. The tested default Debug ELF
SHA-256 was
`bd72b55c84350d433aebb7c0eaee14705b3ae3422604f19c732d54fd2808f73f`.
The route kept CANH-to-CANH, CANL-to-CANL, common GND and the USB-CAN `120R`
setting. The Host explicitly configured 500 kbit/s and sample point `0.75`;
`can0` was ERROR-ACTIVE with zero initial error counters.

A passive three-second window first observed three consecutive sets of
`0x240`, `0x241`, `0x340`, `0x341`, `0x342` and `0x440` periodic frames. The
Host then sent exactly one request, sequence `0x2C` and nonce `0x10203040`:

```bash
python3 tools/can_hil_probe.py \
  --diagnostic-ping --interface can0 \
  --sequence 0x2C --nonce 0x10203040 \
  --response-timeout 2
```

The probe reported `PASS_HARDWARE_ROUND_TRIP_CANDIDATE`, one request sent, one
matching `0x541` response, zero mismatches and 13 captured frames. The VCP
marker independently confirmed MCU application acceptance:

```text
P5CANDIAG1 rx=1 seq=44 nonce=10203040 reply=QUEUED
```

The matching Host response proves that the queued response subsequently left
the STM32. After the exchange, SocketCAN reported 236 RX packets, one TX packet,
zero dropped/error packets, zero warning/passive/bus-off transitions and
ERROR-ACTIVE state. `can0` was then brought down and both devices were powered
off.

This closes the dedicated application round trip as `PASS` on
the admitted adapter/Shield/common-GND route. It does not claim arbitrary
adapter interoperability, natural `0x140` observation, waveform quality,
physical bus-off recovery or simultaneous physical RS485+CAN operation.

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
- [NXP AN2726: XGATE Library: CAN Driver](https://www.nxp.com/docs/en/application-note/AN2726.pdf)
- [NXP AN94088: P82C150 Serial Linked I/O Device](https://www.nxp.com/documents/application_note/AN94088.pdf)
- [NXP AN1776: Summary of CAN](https://www.nxp.com/docs/en/application-note/AN1776.pdf)
