# P5-S2-T05 BSP 候选冻结验证记录

> 状态：`CONTENT_FROZEN + BSP_CONTRACT_CANDIDATE_FROZEN`
> 内容审核：2026-08-14 已通过
> 软件：`PASS_HOST + PASS_CROSS_BUILD`
> 硬件：`WAITING_FOR_HARDWARE`
> 实施基线：`526a962a4a8fbbc5a119603e44539ed83b533b49`（提交 `[ 010 ]`）
> 验证环境：WSL2 `Ubuntu-24.04-STM32`

## 完成范围

T05 没有增加板级运行时功能，也没有重新生成 CubeMX 工程。本轮完成：

- 将 T01～T04 已审核结果汇总为 `docs/bsp_contract.md`；
- 核对 `.ioc`、生成代码、BSP 公开接口和已审核记录四类来源；
- 建立只检查稳定板级事实的 `tools/verify_bsp_contract.py`；
- 完成静态正负例、Host Debug/Release 和 ARM Debug/Release 回归；
- 明确 candidate freeze 与 hardware freeze 的不同结论边界。

当前只冻结软件候选，不声称 NUCLEO-F446RE、Shield、传感器或总线已经实测。

## 四源一致性

| 来源 | 核对内容 | 结果 |
|---|---|---|
| `.ioc` | MCU、pin、label、safe-state、clock、DMA、IRQ、bus parameter | PASS |
| 生成代码 | 初始化顺序、pin macro、GPIO 初值、UART/SPI/I²C/CAN 与 DMA/IRQ | PASS |
| BSP/API | clock、RS485、SPI、I²C、CAN 公开边界与所有权 | PASS |
| T01～T04 记录 | 软件通过、硬件等待、Mode 3 和默认 smoke 边界 | PASS |

当前 `.ioc` SHA-256 保持：

```text
9b94965ddc89e94ea52b553dc15fd9d4c355f7119c2274d4a1cea6ab43c46f4c
```

## 静态合同检查

正常路径：

```text
P5 BSP CONTRACT: PASS (107 stable facts, candidate-only, hardware waiting)
```

脚本检查稳定 pin/peripheral/clock/DMA/IRQ/safe-state、BSP 公开符号、两个 smoke 默认值和合同结论
边界。它不检查 CubeMX 文件排序、注释、全目录 hash，也不推断实物状态。

`--self-test` 在内存中把 `P5_DEVICE_PROBE_SMOKE` 默认值改为 `ON`，检查器正确拒绝该负例：

```text
P5 BSP CONTRACT SELF-TEST: PASS (in-memory device-probe-default-ON mutant rejected)
```

负例没有写入项目文件或生成 fixture。

## Host 与 ARM 回归

| 配置 | 测试/尺寸 | 结果 |
|---|---|---|
| Host Debug | 4/4 CTest | PASS |
| Host Release | 4/4 CTest | PASS |
| Firmware Debug | text 13412 / data 148 / bss 2548 | PASS |
| Firmware Release | text 11696 / data 144 / bss 2544 | PASS |

Debug/Release 缓存均确认：

```text
P5_RS485_LOOPBACK_SMOKE=OFF
P5_DEVICE_PROBE_SMOKE=OFF
```

既有 newlib `nosys` syscall warning 未改变且不影响链接；T05 没有引入 stdio 后端或新的 warning。

## 冻结与非冻结边界

候选冻结包含：pinmap、clock、bus instance/parameter、DMA/IRQ、GPIO safe-state、初始化所有权和当前
BSP 公开边界。

以下仍由后续阶段设计：

- FreeRTOS task、queue、mutex 和 ISR ownership；
- 完整传感器驱动、采样、补偿、FIFO 和 interrupt；
- Modbus register/function/CRC/timing；
- CAN filter、message ID、queue 和 bus-off；
- 经过审核的 BSP API 增量扩展。

如果未来改变稳定板级事实，必须同步 `.ioc`、合同、静态检查和相关回归。

## 未执行硬件项

```text
stlink_vcp_startup = NOT_RUN
clock_gpio_hardware = NOT_RUN
uart_rs485_hardware = NOT_RUN
spi_i2c_hardware = NOT_RUN
can_hardware = NOT_RUN
bsp_contract_hardware_frozen = NOT_GRANTED
hardware = WAITING_FOR_HARDWARE
```

部分硬件到货后可按接口局部补验；单个模块缺失不撤销软件候选冻结。

## 问题与证据边界

本轮没有发现阻塞冻结的配置冲突。S1 历史合同未列出后续新增 API，通过建立 T05 当前权威合同承接，
不需要重写历史文档或创建 diagnostic JSON。

正常成功只保留本摘要，没有生成 manifest、逐事件日志、配置快照或原始构建日志。`out/` 继续作为被
忽略的本地构建缓存，不属于提交候选或冻结证据。

## 当前结论

```text
P5-S2-T05 = CONTENT_FROZEN
bsp_contract = BSP_CONTRACT_CANDIDATE_FROZEN
software = PASS_HOST + PASS_CROSS_BUILD
hardware = WAITING_FOR_HARDWARE
```

无硬件第一步内容已审核通过并冻结；本次不自动开始 P5-S3-T01。

## 2026-08-15 NUCLEO-F446RE 裸板补验

> 补验结论：`PASS_HARDWARE_LIMITED`
> Git 基线：`807f85ce43240e94fc4aea3bd07e31c40a81d236`
> 默认 Debug ELF SHA-256：`4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7`
> 工具：STM32CubeProgrammer `v2.17.0`，ST-LINK `V2J48M35`
> 硬件：NUCLEO-F446RE / STM32F446RE；完整序列号不记录

本节是到货后的增量事实，不改写上文在无硬件阶段形成的历史结论。被测工作区以所列 Git
提交为基线，并包含本次实板暴露的两处最小修复：先开启 `DEMCR.TRCENA` 再检查 DWT 周期计数器，
以及不启用会在缺少 CAN ACK 时形成逐次重发中断风暴的 `CAN_IT_LAST_ERROR_CODE`。两处修改均不改变
CubeMX、引脚、时钟、BSP API、协议或数据结构。

| 检查 | 结果 | 限定 |
|---|---|---|
| Windows 枚举 | PASS | ST-Link Debug 与 VCP `COM22` 均为 OK |
| SWD 连接 | PASS | 识别 `STM32F446xx`，512 KiB Flash，3.26 V 仅为 ST-LINK 读值 |
| 烧录/校验/复位 | PASS | 默认 Debug ELF 下载与 verify 成功 |
| VCP 启动 | PASS | `BOOT` 1 次、`CLOCK` 1 次、heartbeat 5 次 |
| 运行时时钟 | PASS | SYS/HCLK 180 MHz、PCLK1 45 MHz、PCLK2 90 MHz、TIM6 1 ms HAL tick |
| GPIO 寄存器安全状态 | PASS | PA8 ODR=0，PB6/PC7 ODR=1；三者 MODER 均为 output |
| FreeRTOS 有限 smoke | PASS | `SCHEDULER OK` 1 次、heartbeat 5 次、故障输出 0 |
| 断电重连 | PASS | 重新枚举后 heartbeat count=5、DWT ready、scheduler running、fault=0 |

断电补验中还确认：无 CAN 收发器/ACK 时，控制器最终进入限定恢复并停止，CPU 保持在线程态，
不再困于 CAN1 SCE ISR。最终默认固件已恢复全部 smoke 为 `OFF` 并重新烧入。

本次只证明板卡准入、ST-LINK、烧录/校验/复位、VCP 启动、运行时时钟、GPIO 寄存器状态和有限
调度器启动。没有测量排针电压、HSI 精度、严格毫秒抖动、栈水位、ISR latency、IWDG、传感器、
UART loopback、RS485、CAN 物理层或长稳趋势。

```text
BSP-02 = PASS
stlink_vcp_startup = PASS
clock_gpio_hardware = PASS_LIMITED
uart_rs485_hardware = NOT_RUN
spi_i2c_hardware = NOT_RUN
can_hardware = NOT_RUN
bsp_contract_hardware_frozen = NOT_GRANTED
hardware = PARTIAL_PASS
```
