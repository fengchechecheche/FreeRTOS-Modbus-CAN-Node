# 项目五学习路线

> 路线状态：`FROZEN`
> 路线基线：`[038] a8950b5d506d4b02b65c72aa1ec4d7fc6b85da9b`
> 路线总数：35
> 当前可用教程：34
> 未来计划：P5-S7-T05（不创建空壳文件）

本目录面向第一次系统学习 STM32、FreeRTOS、传感器和工业总线的读者。教程用于解释设计与复盘，
不是硬件验收证据；当前可声明能力必须以 [`../evidence_matrix.md`](../evidence_matrix.md) 为准。

## 如何使用

建议先按下面唯一主路线顺序阅读。`FROZEN` 只表示正文已完成用户内容审核，不表示任务在所有执行层
都通过；“证据上限”说明该教程当前最多能支持到哪一层。`NOT_RUN` 硬件缺口不会因为阅读完成而关闭。

| ID | Topic | Prerequisite | File | Status | Evidence ceiling |
|---|---|---|---|---|---|
| P5-S1-T01 | 项目定位与能力边界 | 无 | `p5_s1_t01_项目定位与能力边界.md` | FROZEN | CONTRACT |
| P5-S1-T02 | 硬件组成与安全准入 | P5-S1-T01 | `p5_s1_t02_硬件组成与安全准入.md` | FROZEN | CONTRACT |
| P5-S1-T03 | 仓库结构与嵌入式构建测试 | P5-S1-T01 | `p5_s1_t03_仓库结构与嵌入式构建测试.md` | FROZEN | HOST |
| P5-S1-T04 | 引脚复用、时钟与接口合同 | P5-S1-T02,P5-S1-T03 | `p5_s1_t04_引脚复用时钟与接口合同.md` | FROZEN | CROSS_BUILD |
| P5-S1-T05 | 轻量验收协议与问题排查 | P5-S1-T01 | `p5_s1_t05_验收协议与问题排查.md` | FROZEN | CONTRACT |
| P5-S2-T01 | STM32 与 ST-LINK 最小启动 | P5-S1-T03,P5-S1-T04 | `p5_s2_t01_stm32与stlink最小启动.md` | FROZEN | CROSS_BUILD |
| P5-S2-T02 | 系统时钟、GPIO 与时间基准 | P5-S2-T01 | `p5_s2_t02_系统时钟gpio与时间基准.md` | FROZEN | CROSS_BUILD |
| P5-S2-T03 | UART DMA 与 RS485 方向控制 | P5-S2-T02 | `p5_s2_t03_uart_dma与rs485方向控制.md` | FROZEN | CROSS_BUILD |
| P5-S2-T04 | SPI、I²C 总线与设备探测 | P5-S2-T02 | `p5_s2_t04_spi_i2c总线与设备探测.md` | FROZEN | CROSS_BUILD |
| P5-S2-T05 | BSP 冻结与板级故障排查 | P5-S2-T01,P5-S2-T04 | `p5_s2_t05_bsp冻结与板级故障排查.md` | FROZEN | HOST |
| P5-S3-T01 | FreeRTOS 调度器与任务设计 | P5-S2-T02 | `p5_s3_t01_freertos调度器与任务设计.md` | FROZEN | CROSS_BUILD |
| P5-S3-T02 | 静态内存、栈与资源预算 | P5-S3-T01 | `p5_s3_t02_静态内存栈与资源预算.md` | FROZEN | CROSS_BUILD |
| P5-S3-T03 | 中断、DMA 与任务通知 | P5-S2-T03,P5-S3-T01 | `p5_s3_t03_中断dma与任务通知.md` | FROZEN | CROSS_BUILD |
| P5-S3-T04 | 队列、互斥与数据所有权 | P5-S3-T01,P5-S3-T02 | `p5_s3_t04_队列互斥与数据所有权.md` | FROZEN | HOST |
| P5-S3-T05 | 看门狗、健康监测与故障恢复 | P5-S3-T02,P5-S3-T04 | `p5_s3_t05_看门狗健康监测与故障恢复.md` | FROZEN | CROSS_BUILD |
| P5-S4-T01 | BME280 SPI 采集与补偿算法 | P5-S2-T04,P5-S3-T04 | `p5_s4_t01_bme280_spi采集与补偿算法.md` | FROZEN | CROSS_BUILD |
| P5-S4-T02 | VEML7700 I²C 光照采集 | P5-S4-T01 | `p5_s4_t02_veml7700_i2c光照采集.md` | FROZEN | CROSS_BUILD |
| P5-S4-T03 | ADXL345 中断采样与振动特征 | P5-S3-T03,P5-S4-T01 | `p5_s4_t03_adxl345中断采样与振动特征.md` | FROZEN | CROSS_BUILD |
| P5-S4-T04 | 统一采样、质量、新鲜度与时间戳 | P5-S4-T01,P5-S4-T03 | `p5_s4_t04_统一采样质量新鲜度与时间戳.md` | FROZEN | HOST |
| P5-S4-T05 | 多传感器调度与故障注入 | P5-S4-T04 | `p5_s4_t05_多传感器调度与故障注入.md` | FROZEN | HOST |
| P5-S5-T01 | Modbus 从站合同与寄存器映射 | P5-S4-T04 | `p5_s5_t01_modbus从站合同与寄存器映射.md` | FROZEN | HOST |
| P5-S5-T02 | Modbus CRC、组帧与静默间隔 | P5-S5-T01 | `p5_s5_t02_modbus_crc组帧与静默间隔.md` | FROZEN | HOST |
| P5-S5-T03 | RS485 接收状态机与半双工时序 | P5-S2-T03,P5-S5-T02 | `p5_s5_t03_rs485接收状态机与半双工时序.md` | FROZEN | CROSS_BUILD |
| P5-S5-T04 | 功能码、异常响应与配置写入 | P5-S5-T03 | `p5_s5_t04_功能码异常响应与配置写入.md` | FROZEN | HOST |
| P5-S5-T05 | 项目三联调与主从站证据 | P5-S5-T04 | `p5_s5_t05_项目三联调与主从站证据.md` | FROZEN | HOST |
| P5-S6-T01 | CAN 物理层、仲裁与报文合同 | P5-S4-T04 | `p5_s6_t01_can物理层仲裁与报文合同.md` | FROZEN | HOST |
| P5-S6-T02 | bxCAN 过滤器、中断与发送队列 | P5-S3-T03,P5-S6-T01 | `p5_s6_t02_bxcan过滤器中断与发送队列.md` | FROZEN | CROSS_BUILD |
| P5-S6-T03 | SocketCAN 与 candleLight 联调 | P5-S6-T01,P5-S6-T02 | `p5_s6_t03_socketcan与candlelight联调.md` | FROZEN | VIRTUAL_BUS |
| P5-S6-T04 | 双总线并发、背压与故障隔离 | P5-S5-T04,P5-S6-T02 | `p5_s6_t04_双总线并发背压与故障隔离.md` | FROZEN | HOST |
| P5-S6-T05 | 长稳测试与实时资源趋势 | P5-S3-T05,P5-S6-T04 | `p5_s6_t05_长稳测试与实时资源趋势.md` | FROZEN | HOST |
| P5-S7-T01 | Release 阻塞审查与许可证 | P5-S6-T05 | `p5_s7_t01_release阻塞审查与许可证.md` | FROZEN | HOST |
| P5-S7-T02 | 清洁构建、烧录与复现演练 | P5-S7-T01 | `p5_s7_t02_清洁构建烧录与复现演练.md` | FROZEN | CLEAN_BUILD |
| P5-S7-T03 | 硬件证据矩阵与结论边界 | P5-S7-T02 | `p5_s7_t03_硬件证据矩阵与结论边界.md` | FROZEN | EVIDENCE_MATRIX |
| P5-S7-T04 | 初学者学习路线与问题复盘 | P5-S7-T03 | `p5_s7_t04_初学者学习路线与问题复盘.md` | FROZEN | DOCUMENTATION |
| P5-S7-T05 | v0.1.0 发布与求职材料 | P5-S7-T04 | `p5_s7_t05_v0_1_0发布与求职材料.md` | PLANNED | NOT_RUN |

## 三条快速路线

- STM32/RTOS：P5-S1-T03 → P5-S1-T04 → P5-S2-T01～T05 → P5-S3-T01～T05。
- 传感器与数据链：P5-S2-T04 → P5-S4-T01～T05 → P5-S5-T01/T04 → P5-S6-T01。
- 工业通信与排障：P5-S1-T05 → P5-S2-T03 → P5-S5 → P5-S6 → P5-S7-T02～T04。

快速路线只提供导航，状态仍以上面的 35 项主表和 [`index.md`](index.md) 为准。

## `[038]` 当前锚点

| Anchor | Current value | Repository source |
|---|---|---|
| CubeMX | 6.18.0 | `freertos_modbus_can_node.ioc` |
| STM32CubeF4 | 1.28.3 | `LICENSES/STM32CubeF4-1.28.3-Package_license.md` |
| FreeRTOS | V10.3.1 | `Middlewares/Third_Party/FreeRTOS/Source/include/task.h` |
| Modbus | zero-based；default slave 4；19200 8E1 | `protocol/register_map.json`、`.ioc` |
| CAN | 500000 bit/s；0x140/240/241/340/341/342/440 | `protocol/can_message_map.json` |
| Host regression | Debug/Release 21 tests | `artifacts/release/p5_s7_t02_replay.json` |
| Evidence matrix | 12 PASS + 11 NOT_RUN + 1 NOT_CLAIMED | `artifacts/release/p5_s7_t03_evidence_matrix.json` |

历史教程中的较小测试计数是当时结果，不表示当前回归规模。上述锚点用于发现教程漂移，不把软件
结果提升为硬件通过。

## 真实问题复盘

集中入口为 [`problem_ledger.md`](problem_ledger.md)。它最多保留 12 个实际发生且有回归价值的问题；
硬件未到、PTY/项目三/物理 CAN/正式长稳未执行等缺口继续留在 evidence matrix，不进入“已解决”。

## 一手资料（访问核验：2026-08-15）

| Source ID | Publisher | Topic/version | Official URL | Access status |
|---|---|---|---|---|
| SRC-01 | STMicroelectronics | STM32F446 documentation：RM0390/DS10693/errata | https://www.st.com/en/microcontrollers-microprocessors/stm32f446/documentation.html | VERIFIED_2026-08-15 |
| SRC-02 | STMicroelectronics | NUCLEO-F446RE / Nucleo-64 hardware | https://www.st.com/en/evaluation-tools/nucleo-f446re.html | VERIFIED_2026-08-15 |
| SRC-03 | FreeRTOS | Kernel tasks、notifications、static allocation、ISR rules | https://www.freertos.org/Documentation/02-Kernel/02-Kernel-features/00-Developer-docs | VERIFIED_2026-08-15 |
| SRC-04 | Modbus Organization | Application Protocol V1.1b3 / Serial Line V1.02 | https://www.modbus.org/modbus-specifications | VERIFIED_2026-08-15 |
| SRC-05 | Bosch Sensortec | BME280 data sheet | https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme280-ds002.pdf | VERIFIED_2026-08-15 |
| SRC-06 | Vishay | VEML7700 product page and data sheet | https://www.vishay.com/en/product/84286/ | VERIFIED_2026-08-15 |
| SRC-07 | Analog Devices | ADXL345 product page / data sheet Rev. G | https://www.analog.com/en/products/adxl345.html | VERIFIED_2026-08-15 |
| SRC-08 | Linux kernel | SocketCAN documentation | https://docs.kernel.org/networking/can.html | VERIFIED_2026-08-15 |

工具实现可继续参考 [linux-can/can-utils](https://github.com/linux-can/can-utils) 和
[candle-usb/candleLight_fw](https://github.com/candle-usb/candleLight_fw)，但上游工具仓库不是协议标准。

这些资料解释芯片、RTOS 和协议原理，不证明本项目板卡已经烧录、通信或长稳通过。
