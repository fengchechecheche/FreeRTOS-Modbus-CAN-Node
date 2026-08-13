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
