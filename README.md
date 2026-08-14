# FreeRTOS Modbus CAN Node

> P5-S7-T02 update: clean local-archive reproduction from
> `[036] 15932a2ff7adecdfbe5355559926a95b0df25845` passes the Host, contract,
> Debug/Release ARM and resource gates without prior build cache or network
> access. Candidate hashes are recorded; a second Release build exposed
> absolute FreeRTOS `__FILE__` path dependence, so bit-for-bit reproducibility
> is explicitly `NOT_CLAIMED`. Flashing and physical replay remain
> `WAITING_FOR_HARDWARE`. See
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
> PASS_CROSS_BUILD + READY_FOR_HARDWARE`. D01-D08 cover Modbus/RS485 slow,
> CRC, busy and timeout paths together with CAN latest-wins, queue pressure and
> bus-off isolation. Physical dual-bus validation remains
> `WAITING_FOR_HARDWARE`. See
> [`docs/dual_bus_fault_matrix.md`](docs/dual_bus_fault_matrix.md).

> P5-S6-T03 update: the standard-library SocketCAN probe, dry-run and bounded
> `vcan` matrix are `PASS_HOST`; candleLight/physical CAN remain
> `WAITING_FOR_HARDWARE`. `can-utils` 2023.03-1 and a one-frame
> `candump`/`cansend` vcan smoke are also verified. See
> [`docs/can_hil_report.md`](docs/can_hil_report.md).

> P5-S6-T02 update: `CAN_CONTRACT_CANDIDATE_VALIDATED +
> CAN_RUNTIME_CANDIDATE_IMPLEMENTED`. Exact filters, bounded IRQ mailboxes,
> task-owned TX scheduling and bus-off recovery are integrated; hardware remains
> `WAITING_FOR_HARDWARE`. See [`docs/can_runtime.md`](docs/can_runtime.md).

> P5-S5-T05 update: the default firmware is unchanged; a bounded HIL probe now
> provides self-test/dry-run preparation without opening a serial port. Physical
> USB-RS485 and Project Three integration remain deferred. See
> [`docs/modbus_hil_report.md`](docs/modbus_hil_report.md).

基于 STM32F446RE 与 FreeRTOS 的双总线工业状态监测节点。P5-S2-T05 已把 T01～T04 的无硬件结果
汇总为 BSP 软件候选合同；当前状态为 `BSP_CONTRACT_CANDIDATE_FROZEN + WAITING_FOR_HARDWARE`。
这允许后续纯软件轨道继续，但不代表板卡、Shield 或外设已经实测。P5-S3-T01 已集成随包
FreeRTOS V10.3.1 和五任务静态调度骨架，内容已于 2026-08-14 审核冻结。P5-S4-T01 BME280 内容
已经审核冻结；P5-S4-T02 VEML7700 整数照度、有限自动量程与 acquisition task 软件候选也已完成
内容审核，等待硬件补验。P5-S4-T03 ADXL345 DATA_READY 中断采样、整数工程量和
100 样本振动趋势特征已经完成内容审核。P5-S4-T04 已形成统一 sequence、单调时间、
质量和新鲜度的软件候选。P5-S4-T05 已增加固定大小的多传感器监测摘要与确定性
联合故障矩阵，内容已审核冻结。P5-S5-T01 已形成地址 4 的 Modbus register contract 和机器可读
map。P5-S5-T02 已增加 CRC16、完整 ADU envelope 和 8E1 静默间隔纯逻辑候选；stream parser、
RS485 transport 已在 T03 接入；T04 已实现 `0x03/0x04/0x06`、异常响应、122-register image 和
易失地址写入候选。T05 无硬件路径已增加默认只读、显式解锁地址写入的 HIL 探针；串口保持
`NOT_RUN`，项目三地址 4 profile 仍为 `not_created`。P5-S6-T01 已冻结七个 11 位标准 CAN ID、
8-byte payload、little-endian、sequence、状态和 1% 静态负载合同，并增加纯 C codec；P5-S6-T02
已接入 filter、IRQ、固定队列、task notification 与 bus-off 恢复软件候选，硬件保持
`WAITING_FOR_HARDWARE`。P5-S6-T04 已用一个直接链接生产模块的 Host 矩阵验证双总线背压和
软件故障隔离；未修改固件任务或 RTOS 资源，实物并发仍待硬件。P5-S6-T05 已增加有界
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
  由 TIM6 提供；SysTick、PendSV 与 SVC 由 FreeRTOS ARM_CM4F port 使用，均尚未板级实测。
- USART2 保留 T01 启动标记、T02 clock 摘要和最多五次 1 秒 heartbeat；当前只通过交叉构建。
- SPI1 使用 PA5/PA6/PA7、Mode 3、MSB first、software NSS、2.8125 Mbit/s；BME280/PB6 与
  ADXL345/PC7 片选独立，transaction timeout 候选为 20 ms。
- I2C2 使用 PB10/PB3、100 kHz；应用层只保存 VEML7700 7-bit address `0x10`，仅在 HAL boundary
  左移地址。
- 最小探测读取 BME280 `0xD0`/`0x60` 与 ADXL345 `0x00`/`0xE5`；VEML7700 只验证 address/register
  presence，不声称 silicon ID。
- `P5_DEVICE_PROBE_SMOKE` 默认关闭；临时启用时只探测一次并通过 USART2 输出紧凑摘要，设备缺失
  不阻塞正常启动。
- FreeRTOS 任务使用静态分配，`protocol_task` 接管有限 RS485 poll，`diagnostic_task` 接管有限
  heartbeat；`acquisition_task` 保持 20 ms 绝对释放推进 BME280/VEML7700，并接收 ADXL345
  DATA_READY 计数通知；每批通知最多读取一帧。
- 五个应用任务栈各为 256 words，Host/ARM 资源门已通过；硬件 watermark 仍为 `NOT_MEASURED`。
- SPI/I²C 候选不使用 DMA、RTOS 或动态内存；timeout/bus error 最多请求一次 recovery，当前 HAL
  adapter 不伪造未实测的 SCL pulse 或重新初始化。
- BME280 使用 1 Hz forced mode、T/P/H x1、filter off 和 5 ms SPI timeout；raw 与整数工程量保存在
  owner-local snapshot；实物 ID、采集与精度仍为 `NOT_RUN`。
- VEML7700 使用 7-bit `0x10`、默认 gain x1/8 与 100 ms integration，以 9 级有限自动量程输出整数
  millilux；高照度修正只标记不伪造，实物 ACK、采集、量程切换与精度仍为 `NOT_RUN`/`NOT_CLAIMED`。
- ADXL345 候选为 100 Hz、full-resolution、±4 g、FIFO bypass，DATA_READY 映射到 PB4/EXTI4
  priority 6/0；六字节 coherent read 使用 `0xF2` wire command。100 样本窗口只保存
  sum/sum-square/min/max，输出去直流 RMS/peak 和三轴合成 RMS，不构成故障诊断或校准结论。
- 统一 measurement schema 将 BME、VEML、ADXL sample 和 ADXL feature 作为四个独立 source；
  使用 17 个逻辑 field ID、固定整数单位和 `fresh/stale/offline/invalid` 状态。field ID 不是
  Modbus register 或 CAN ID，invalid 不输出伪造工程量，stale/offline last-good 显式 retained。
- `app_sensor_monitor` 只汇总四 source 的样本间隔包络与三设备 fault/recovery 计数，不保存
  原始历史、不控制驱动。单传感器 unavailable/stale/recovery 只使 health 降级，喂狗仍允许，
  不触发全局恢复或复位。Host 虚拟故障矩阵不是实物断线或 60 分钟运行证据。
- PA5 保留 SPI1 SCK，不作为 LD2 heartbeat；两个 SPI CS 初值高。
- NUCLEO-F446RE 尚未到货，ST-LINK、VCP、UART loopback、RS485 physical layer 和全部板级接口均保持
  `WAITING_FOR_HARDWARE`。
- 默认 Modbus slave address contract 为 `4`；T01 已冻结 122-register input map、4-register
  holding map 和 0x03/0x04/0x06 应用合同。T02 CRC/ADU/timing、T03 stream/256 B transport 和
  T04 function server/register image 均已达到 Host/ARM 软件候选；runtime 为 `CANDIDATE_IMPLEMENTED`。
  T05 HIL self-test/dry-run 不打开串口；真实 249 B response、地址迁移、UART/RS485 总线和项目三联调
  仍为 `WAITING_FOR_HARDWARE` / `NOT_RUN`。
- CAN 合同使用节点 4 的 `0x140/0x240/0x241/0x340/0x341/0x342/0x440`、500 kbit/s、
  standard data frame、DLC 8 和 little-endian。纯 codec、精确 filter、固定 IRQ/RX mailbox、
  latest-wins 周期发送、事件合并及 1 s/3 次 bus-off 恢复均达到软件候选；candleLight、收发器和
  物理帧仍为 `NOT_RUN`。P5-S6-T03 已用 raw SocketCAN/`vcan` 完成 12 帧有界软件矩阵；
  `can-utils` 2023.03-1 已安装且 `candump`/`cansend` 单帧 vcan smoke 通过；candleLight/实物 HIL
  仍为 `NOT_RUN`。T04 的 D01～D08 Host 矩阵进一步验证 RS485 CRC/timeout 与 CAN busy、FIFO full、
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

CAN map/codec 为 `CANDIDATE_VALIDATED`，CAN runtime 为 `CANDIDATE_IMPLEMENTED`；raw
SocketCAN/`vcan` 为 `PASS_HOST`，candleLight 和物理 CAN 仍为 `NOT_RUN / WAITING_FOR_HARDWARE`。

CAN HIL 软件预检：

```bash
python3 tools/can_hil_probe.py --self-test
python3 tools/can_hil_probe.py --dry-run
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
