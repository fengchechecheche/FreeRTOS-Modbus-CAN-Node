# FreeRTOS Modbus CAN Node

> REPRO-002 update: committed `[047]
> 26411d2b627fd67654479f5a97a2066e47deafb5` completed a tracked-file-only
> clean replay with 15 bounded commands, no network and no prior build cache.
> Host/contract, ARM Debug/Release and resource gates passed. The current
> Release BIN SHA-256 is
> `8eae8b92b0d9cdf4af3ad938d881fb124f682e9f2e8435b02dbb7a4685ff986c`.
> Cross-path bit-for-bit output remains explicitly `NOT_CLAIMED`; hardware
> blockers and the `UNRELEASED` state are unchanged. See
> [`docs/reproduction_report_repro_002.md`](docs/reproduction_report_repro_002.md).

> 2026-08-21 CAN hardware update: `CAN-03` is `PASS` for the
> admitted candleLight/`gs_usb` adapter, Waveshare Shield and common-GND wiring.
> The default firmware produced 252 valid periodic frames, 42 per required ID;
> separate RX-only and TX-once diagnostics proved physical delivery/ACK in both
> directions with both sides ERROR-ACTIVE and zero errors. Source `[060]` then
> passed the read-only `0x540` Host request / `0x541` STM32 response application
> round trip at 500 kbit/s and Host sample point `0.75`: one request, one matching
> response, zero CAN errors and the matching VCP acceptance marker. A later
> bounded `BUS-02` run also passed simultaneous RS485/CAN traffic, one brief
> peer interruption per route and post-restore checks. See
> [`docs/can_hil_report.md`](docs/can_hil_report.md).

> 2026-08-21 RS485 hardware update: the historical old-adapter return-path
> failure remains recorded, while a new CH340 USB-RS485 passed 10/10
> back-to-back transfers in both directions and the physical H01～H07 matrix at
> address 4, including the 249 B response. A representative H01 also passed
> after NUCLEO reset and after adapter reconnect. `RS485-03` is `PASS` within
> that fixed-address boundary; H08/H09 are `NOT_RUN_BY_POLICY`, and Project
> Three remains `NOT_RUN`. See [`docs/modbus_hil_report.md`](docs/modbus_hil_report.md).

> 2026-08-19 BME280 hardware update: `SNS-01` is `PASS_HARDWARE_LIMITED`.
> The final topology produced five fresh, bounded samples with increasing
> sequence and valid compensated temperature/pressure/humidity output; reset
> reinitialization also passed. No metrology accuracy or independent calibration
> is claimed. See [`docs/bme280_report.md`](docs/bme280_report.md).

> 2026-08-19 VEML7700 hardware update: `SNS-02` is
> `PASS_HARDWARE_LIMITED`. Continuous fresh samples,遮挡下降、恢复照明回升 and
> two bounded automatic range changes passed with zero observed transport
> errors; no lux-meter accuracy or full-range claim is made. See
> [`docs/veml7700_report.md`](docs/veml7700_report.md).

> 2026-08-21 ADXL345 hardware update: `SNS-03` is `PASS_BOUNDED_POLLING`.
> Source `[066] 7cedd353f2e9657702770ed1abc88b1ac612b5fe` defaults to one bounded
> DATA_READY poll per 20 ms acquisition release, with at most one XYZ read when
> ready. In the final three-sensor topology with INT1/INT2 disconnected, sample
> and feature sequences, three posture trends, vibration RMS/peak response,
> restart and 10 consecutive Modbus snapshots passed. The default Ubuntu
> authority Debug ELF SHA-256 is
> `d076ddf743020fe1a043e776ba3196ea1f02153a17c5d98451cc722d6ac0018f`.
> Physical INT1/INT2, exact rate, metrology and standalone SPI robustness remain
> excluded. See [`docs/adxl345_report.md`](docs/adxl345_report.md).

> 2026-08-15 board supplement: NUCLEO-F446RE `BSP-02` is
> `PASS_HARDWARE_LIMITED`. ST-LINK V2J48M35, default Debug ELF
> flash/verify/reset, VCP boot, runtime clock, GPIO register safe-state,
> limited FreeRTOS scheduler smoke and one USB power cycle passed. The tested
> default ELF SHA-256 is
> `4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7`.
> A later S3 supplement passed bare-board stack watermarks, default health feed,
> one controlled IWDG reset and reset-only `.noinit` retention. At that board
> supplement stage, sensors and physical buses were not exercised; current domain
> status is tracked separately. The Hardware Release gate remains open. See [`docs/bsp_validation.md`](docs/bsp_validation.md)
> and [`docs/health_recovery_report.md`](docs/health_recovery_report.md).

> P5-S7-T05 update: intended version `v0.1.0` remains `UNRELEASED`. The current
> result is a `SOFTWARE_CANDIDATE_READY_FOR_HARDWARE`, with a software demo and
> matrix-bound recruitment wording that are not published. `HW-003` is now
> closed by bounded CAN and physical dual-bus evidence, and `HW-001` is closed
> by the bounded sensor/watchdog routes. Project Three and soak blockers remain
> open. No tag, binary attachment or remote Release exists. See
> [`docs/v0_1_0_software_candidate.md`](docs/v0_1_0_software_candidate.md),
> [`docs/demo_guide.md`](docs/demo_guide.md), and
> [`docs/recruitment_claim_ledger.md`](docs/recruitment_claim_ledger.md).

> P5-S7-T04 update: the beginner route now covers all 35 work blocks while
> truthfully exposing all 35 tutorials, with T05 pending content review. Four
> reviewed S1 tutorials are restored, and a bounded ledger keeps 12 real
> engineering problems without converting hardware `NOT_RUN` gaps into fixes.
> See [`docs/learning/README.md`](docs/learning/README.md) and
> [`docs/learning/problem_ledger.md`](docs/learning/problem_ledger.md).

> P5-S7-T03 update: the public evidence matrix contains 24 bounded rows:
> 21 `PASS`, 0 `FAIL`, 2 `NOT_RUN`, and 1 `NOT_CLAIMED`. Every result is qualified by
> software, board, RS485, or CAN evidence layer; no software result is promoted
> to a physical-hardware claim. The hardware Release gate remains
> `BLOCKED_WAITING_FOR_HARDWARE`. See
> [`docs/evidence_matrix.md`](docs/evidence_matrix.md).

> P5-S7-T02 update (historical): clean local-archive reproduction from
> `[036] 15932a2ff7adecdfbe5355559926a95b0df25845` passes the Host, contract,
> Debug/Release ARM and resource gates without prior build cache or network
> access. Candidate hashes are recorded; a second Release build exposed
> absolute FreeRTOS `__FILE__` path dependence, so bit-for-bit reproducibility
> is explicitly `NOT_CLAIMED`. Its then-current hardware state remains a
> historical fact and is not used as the current board status. See
> [`docs/reproduction_report.md`](docs/reproduction_report.md).

> P5-S7-T01 update: the project-owned source scope is MIT-licensed by
> `fengchechecheche`; STM32Cube/CMSIS/HAL/FreeRTOS remain under their original
> terms. The software-source blocker audit is ready for clean reproduction,
> while binary reproduction and every hardware Release gate remain open. See
> [`docs/release_readiness.md`](docs/release_readiness.md) and
> [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).

> P5-S6-T05 update: the bounded soak runner, schema/trend evaluator and
> 20-iteration Host preflight are `PASS_HOST + PASS_CROSS_BUILD +
> READY_FOR_HARDWARE`. The 10-minute, 60-minute and formal 8-hour sessions
> remain `NOT_RUN` until hardware and a reviewed collector are available. See
> [`docs/soak_trend_report.md`](docs/soak_trend_report.md).

> P5-S6-T04 update: the deterministic dual-bus matrix is `PASS_HOST +
> PASS_CROSS_BUILD`; its bounded physical supplement is `PASS`. D01-D08 cover Modbus/RS485 slow,
> CRC, busy and timeout paths together with CAN latest-wins, queue pressure and
> bus-off isolation. The physical supplement covers normal simultaneous traffic,
> one brief peer interruption per route and post-restore checks. See
> [`docs/dual_bus_fault_matrix.md`](docs/dual_bus_fault_matrix.md).

> P5-S6-T03 update: the standard-library SocketCAN probe, dry-run and bounded
> `vcan` matrix are `PASS_HOST`; the common-GND candleLight supplement is
> `PASS` for periodic telemetry, physical ACK in both directions and the
> dedicated read-only application round trip. `can-utils` 2023.03-1 and a
> one-frame `candump`/`cansend` vcan smoke are also verified. See
> [`docs/can_hil_report.md`](docs/can_hil_report.md).

> P5-S6-T02 update: `CAN_CONTRACT_CANDIDATE_VALIDATED +
> CAN_RUNTIME_CANDIDATE_IMPLEMENTED`. Exact filters, bounded IRQ mailboxes,
> task-owned TX scheduling and bus-off recovery are integrated. This T02 claim
> remains software-only; the later bounded physical result is recorded separately
> in [`docs/can_hil_report.md`](docs/can_hil_report.md).

> P5-S5-T05 update: the default firmware is unchanged; a bounded HIL probe now
> provides self-test/dry-run preparation without opening a serial port. A
> reference CH340 USB-RS485 has since passed the bounded fixed-address-4 physical
> matrix, while valid address migration and Project Three integration remain
> deferred. See
> [`docs/modbus_hil_report.md`](docs/modbus_hil_report.md).

基于 STM32F446RE 与 FreeRTOS 的双总线工业状态监测节点。P5-S2-T05 已把 T01～T04 的无硬件结果
汇总为 BSP 软件候选合同；2026-08-15 裸板补验已将 `BSP-02` 提升为
`PASS_HARDWARE_LIMITED`；后续三传感器、RS485、CAN 和一次有界双总线补验也已分别完成。
P5-S3-T01 已集成随包
FreeRTOS V10.3.1 和五任务静态调度骨架，内容已于 2026-08-14 审核冻结。P5-S4-T01 BME280 内容
已经审核冻结；P5-S4-T02 VEML7700 整数照度、有限自动量程与 acquisition task 软件候选也已完成
有界硬件补验。P5-S4-T03 ADXL345 整数工程量和 100 样本振动趋势特征已完成内容审核，
当前模块通过默认有界 DATA_READY 轮询路线完成硬件补验；物理中断路径仍不声明。
P5-S4-T04 已形成统一 sequence、单调时间、
质量和新鲜度的软件候选。P5-S4-T05 已增加固定大小的多传感器监测摘要与确定性
联合故障矩阵，内容已审核冻结。P5-S5-T01 已形成地址 4 的 Modbus register contract 和机器可读
map。P5-S5-T02 已增加 CRC16、完整 ADU envelope 和 8E1 静默间隔纯逻辑候选；stream parser、
RS485 transport 已在 T03 接入；T04 已实现 `0x03/0x04/0x06`、异常响应、122-register image 和
易失地址写入候选。T05 无硬件路径已增加默认只读、显式解锁地址写入的 HIL 探针；串口保持
`NOT_RUN`，项目三地址 4 profile 仍为 `not_created`。P5-S6-T01 已冻结九个 11 位标准 CAN ID、
8-byte payload、little-endian、sequence、状态和 1% 静态负载合同，并增加纯 C codec；P5-S6-T02
已接入 filter、IRQ、固定队列、task notification 与 bus-off 恢复软件候选，硬件保持
`WAITING_FOR_HARDWARE`。P5-S6-T04 已用一个直接链接生产模块的 Host 矩阵验证双总线背压和
软件故障隔离，并完成一次有界实物并发、短暂断线隔离和恢复补验；未修改固件任务或 RTOS 资源。P5-S6-T05 已增加有界
JSONL soak runner、趋势判定和 20 次短时 Host 预检；默认固件保持不变，真实 10 分钟、60 分钟
和 8 小时长稳均为 `NOT_RUN`。

## 当前边界

- 主机侧测试验证构建基础设施、32 位毫秒时间逻辑以及 RS485 DE/DMA 调用顺序，不代表硬件通信完成。
- `freertos_modbus_can_node.ioc`、`Core/`、`Drivers/`、startup 和链接脚本由 STM32CubeMX 6.18.0 /
  STM32CubeF4 1.28.3 建立。
- USART1 为 PA9/PA10、19200、8E1；RX 使用 DMA2 Stream2/Channel4，TX 使用 DMA2 Stream7/Channel4，
  PA8/RS485_DE 上电为低。
- 默认路径只应答地址 4 的合法 Modbus 请求；ASCII `P5T03/P5T03OK` 仅在显式
  `P5_RS485_LOOPBACK_SMOKE=ON` 时启用，它不是 Modbus 帧。
- DE 只在最终 USART `TC` 对应的 `HAL_UART_TxCpltCallback()` 后拉低；start failure、timeout 和 UART
  error 均有有界恢复路径。
- 候选时钟为 HSI 16 MHz、SYSCLK/HCLK 180 MHz、PCLK1 45 MHz、PCLK2 90 MHz；HAL 1 ms tick
  由 TIM6 提供；运行时 profile 和有限 scheduler smoke 已通过实板补验，但未测 HSI 精度或严格抖动。
- USART2 保留 T01 启动标记、T02 clock 摘要和最多五次 1 秒 heartbeat；实板 VCP 已得到
  1 次 BOOT、1 次 CLOCK 和 5 次 heartbeat。
- SPI1 使用 PA5/PA6/PA7、Mode 3、MSB first、software NSS、2.8125 Mbit/s；BME280/PB6 与
  ADXL345/PC7 片选独立，transaction timeout 候选为 20 ms。
- I2C2 使用 PB10/PB3、100 kHz；应用层只保存 VEML7700 7-bit address `0x10`，仅在 HAL boundary
  左移地址。
- 最小探测读取 BME280 `0xD0`/`0x60` 与 ADXL345 `0x00`/`0xE5`；VEML7700 只验证 address/register
  presence，不声称 silicon ID。
- `P5_DEVICE_PROBE_SMOKE` 默认关闭；临时启用时只探测一次并通过 USART2 输出紧凑摘要，设备缺失
  不阻塞正常启动。
- FreeRTOS 任务使用静态分配，`protocol_task` 接管有限 RS485 poll，`diagnostic_task` 接管有限
  heartbeat；`acquisition_task` 保持 20 ms 绝对释放推进 BME280/VEML7700，并处理 ADXL345
  DATA_READY 通知。默认生产后备每次释放最多读取一次 `INT_SOURCE`，仅在 ready 时最多读取
  一组 XYZ；轮询计数不伪装为 EXTI 证据。
- 五个应用任务栈各为 256 words，Host/ARM 资源门已通过；裸板最小剩余为
  `215/168/115/53/215` words，均高于 32-word 门限，传感器和物理总线负载仍需重测。
- 默认固件启用约 8 s 标称 IWDG，只有 `health_task` 根据健康策略刷新；
  `P5_IWDG_RESET_SMOKE` 默认关闭。实板已通过正常喂狗、一次 7.59 s 受控超时复位、IWDG
  原因识别和软件/IWDG 复位间 `.noinit` 保持，不声明断电保持或严格 LSI 超时精度。
- SPI/I²C 候选不使用 DMA、RTOS 或动态内存；timeout/bus error 最多请求一次 recovery，当前 HAL
  adapter 不伪造未实测的 SCL pulse 或重新初始化。
- BME280 使用 1 Hz forced mode、T/P/H x1、filter off 和 5 ms SPI timeout；raw 与整数工程量保存在
  owner-local snapshot；实物 ID、连续采集和复位后重新初始化已通过，计量精度不声明。
- VEML7700 使用 7-bit `0x10`、默认 gain x1/8 与 100 ms integration，以 9 级有限自动量程输出整数
  millilux；实物配置访问、连续采集、遮挡/恢复趋势和有限量程变化已通过，照度计精度不声明。
- ADXL345 候选为 100 Hz、full-resolution、±4 g、FIFO bypass，DATA_READY 映射到 PB4/EXTI4
  priority 6/0；六字节 coherent read 使用 `0xF2` wire command。100 样本窗口只保存
  sum/sum-square/min/max，输出去直流 RMS/peak 和三轴合成 RMS。当前最终拓扑使用默认有界
  轮询后备通过样本/特征、姿态和振动趋势补验；物理 INT1/INT2、精确采样率、单模块 SPI
  鲁棒性、故障诊断和校准结论均不声明。
- 统一 measurement schema 将 BME、VEML、ADXL sample 和 ADXL feature 作为四个独立 source；
  使用 17 个逻辑 field ID、固定整数单位和 `fresh/stale/offline/invalid` 状态。field ID 不是
  Modbus register 或 CAN ID，invalid 不输出伪造工程量，stale/offline last-good 显式 retained。
- `app_sensor_monitor` 只汇总四 source 的样本间隔包络与三设备 fault/recovery 计数，不保存
  原始历史、不控制驱动。单传感器 unavailable/stale/recovery 只使 health 降级，喂狗仍允许，
  不触发全局恢复或复位。Host 虚拟故障矩阵不是实物断线或 60 分钟运行证据。
- PA5 保留 SPI1 SCK，不作为 LD2 heartbeat；两个 SPI CS 初值高。
- NUCLEO-F446RE 已完成 ST-LINK、烧录/校验/复位、VCP、运行时时钟、GPIO 寄存器状态、有限
  scheduler smoke、裸板栈水位、IWDG 单次恢复和断电重连补验；CAN 已完成已准入路径的有限
  实物补验；RS485 固定地址 4 的参考转换器矩阵及一次有界物理双总线并发/故障隔离也已通过。
  三传感器已在各自有界路线内通过，物理 ADXL345 INT 路径继续作为排除项；项目三互操作与
  10 分钟、60 分钟、8 小时长稳仍保持开放。
- 默认 Modbus slave address contract 为 `4`；T01 已冻结 122-register input map、4-register
  holding map 和 0x03/0x04/0x06 应用合同。T02 CRC/ADU/timing、T03 stream/256 B transport 和
  T04 function server/register image 均已达到 Host/ARM 软件候选；runtime 为 `CANDIDATE_IMPLEMENTED`。
  T05 HIL self-test/dry-run 不打开串口；参考 CH340 路径的真实 249 B response 已通过，地址迁移
  与项目三联调仍为 `NOT_RUN`。
- CAN 合同使用节点 4 的七个节点遥测/事件 ID 以及专用诊断 `0x540/0x541`、500 kbit/s、
  standard data frame、DLC 8 和 little-endian。接收侧仅启用一个 16-bit ID-list filter bank，
  四条硬件表项均重复 Host-owned `0x540`；发送侧独立允许七个 STM32 遥测/事件 ID 和 `0x541`。
  固定 IRQ/RX mailbox、latest-wins 周期发送、事件合并、单槽诊断应答及 1 s/3 次 bus-off 恢复均达到
  软件候选；公共 GND 条件下的周期遥测、双向物理 ACK 和 `0x540/0x541` 应用层往返均已通过。
  P5-S6-T03 已用 raw SocketCAN/`vcan` 完成有界软件矩阵；T04 的 D01～D08 Host 矩阵进一步验证
  RS485 CRC/timeout 与 CAN busy、FIFO full、
  bus-off 同时发生时，健康链路、采集和 health task model 仍有进度；虚拟 tick 不构成物理恢复时间。
- 项目自有代码采用 MIT，公开 holder 为 `fengchechecheche`；CubeMX、CMSIS、HAL、FreeRTOS 和其他
  独立通知材料仍受各自条款约束，见根 `LICENSE` 与 `THIRD_PARTY_NOTICES.md`。
- repository remote name 为 `FreeRTOS-Modbus-CAN-Node`。

当前权威 BSP 软件候选见 [`docs/bsp_contract.md`](docs/bsp_contract.md)。改变已冻结 pin、clock、bus、
DMA/IRQ 或 safe-state 时，必须同步合同、配置和相关回归；后续 API 仍允许经审核做增量扩展。

## 主机验证

```bash
./tools/verify_host.sh
```

权威开发环境为 WSL2 `Ubuntu-24.04-STM32`。该入口运行 host Debug/Release 的 21 项 CTest，包括
BME280 calibration/compensation、VEML7700 word/range/state-machine 和 ADXL345
parse/config/feature/recovery、统一 sample schema/quality/freshness、三驱动联合故障矩阵、CAN
known-good payload/边界、Modbus CRC/完整 ADU/8E1 timing，以及双总线 D01～D08 故障隔离回归。
S4 软件验收边界见 [`docs/s4_validation.md`](docs/s4_validation.md)。构建输出位于
`out/`，问题排查证据只在需要时写入被忽略的
`.private/`。

BSP 静态合同检查：

```bash
python3 tools/verify_bsp_contract.py
```

CAN contract 检查：

```bash
python3 tools/verify_can_contract.py
python3 tools/verify_can_contract.py --self-test
```

CAN map/codec 为 `CANDIDATE_VALIDATED`，CAN runtime 为 `CANDIDATE_IMPLEMENTED`；
SocketCAN/`vcan` 为 `PASS_HOST`。实物 CAN 在公共 GND 条件下通过周期遥测和两方向物理
ACK 的有界补验；revision 1 的只读 `0x540/0x541` 实物应用层往返也已通过，`CAN-03`
保持 `PASS`。`BUS-02` 已在参考 CH340、已准入 CAN 路径、一次短暂断线/恢复的有界条件下为
`PASS`；不声明重复断线耐久、任意断线时长、物理 bus-off 恢复或长稳。

CAN HIL 软件预检：

实物 `can0` 的项目默认配置必须显式包含
`ip link set can0 type can bitrate 500000 sample-point 0.75`；不得依赖曾选择
`0.875` 的主机默认值。usbipd 附加和 WSL root 配置命令见
[`docs/can_runtime.md`](docs/can_runtime.md#default-wsl-physical-can-setup)。

```bash
python3 tools/can_hil_probe.py --self-test
python3 tools/can_hil_probe.py --dry-run
python3 tools/can_hil_probe.py --diagnostic-ping --interface can0 \
  --sequence 0x2A --nonce 0x12345678 --response-timeout 2
```

`can-utils` 的 `candump`/`cansend` 单帧 vcan smoke 已通过。`vcan` 的显式发送矩阵和到货后的接线/接口步骤见
[`docs/can_hil_report.md`](docs/can_hil_report.md)。`PASS_HOST` 不代表 candleLight、ACK 或物理总线通过。

双总线背压与故障隔离矩阵：

```bash
./out/host-debug/p5_host_dual_bus_fault_matrix
```

紧凑结果和实物补验边界见 [`docs/dual_bus_fault_matrix.md`](docs/dual_bus_fault_matrix.md)。其中的
虚拟恢复 tick 和 task-model release 不能解释为 MCU 实测时间或 deadline 结果。

Modbus register contract 检查：

```bash
python3 tools/verify_modbus_contract.py
python3 tools/verify_modbus_contract.py --self-test
```

该入口验证 map、类型、地址、metadata、范围和候选 runtime 状态。CRC/ADU/timing、stream
和 function server 分别见 [`docs/modbus_codec.md`](docs/modbus_codec.md)、[`docs/modbus_transport.md`](docs/modbus_transport.md) 与 [`docs/modbus_server.md`](docs/modbus_server.md)。软件通过不代表 UART/RS485 实物已运行。

无硬件 HIL 准备：

```bash
python3 tools/modbus_hil_probe.py --self-test
python3 tools/modbus_hil_probe.py --dry-run
```

两种模式均输出 `serial NOT_OPENED`，不得打开串口。真实端口、写地址安全门和最小矩阵见
[`docs/modbus_hil_report.md`](docs/modbus_hil_report.md)。

## 固件构建

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug
cmake --preset firmware-release
cmake --build --preset firmware-release
```

每个 firmware preset 生成 ELF、HEX、BIN 与 MAP。交叉链接成功只证明构建链闭合，不代表 hardware、
wiring、bus timing 或 protocol 已验收。

到货后的无 Shield PA9/PA10 loopback 可临时在 configure 时设置 `P5_RS485_LOOPBACK_SMOKE=ON`；该模式只
允许用于有限三次本地探针，不得接入外部 RS485 bus。

T04 台架探测可临时配置：

```bash
cmake --preset firmware-debug -DP5_DEVICE_PROBE_SMOKE=ON
cmake --build --preset firmware-debug
```

验证后必须恢复默认 `OFF`；该模式不是周期扫描或完整传感器驱动。
