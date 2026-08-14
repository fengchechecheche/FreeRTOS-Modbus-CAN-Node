# P5-S2-T05 BSP 软件候选合同

> 合同状态：`BSP_CONTRACT_CANDIDATE_FROZEN`
> 内容审核：2026-08-14 已通过
> 软件状态：`PASS_HOST + PASS_CROSS_BUILD`
> 硬件状态：`WAITING_FOR_HARDWARE`
> 实施基线：`526a962a4a8fbbc5a119603e44539ed83b533b49`（提交 `[ 010 ]`）
> 目标：NUCLEO-F446RE / STM32F446RE
> CubeMX：6.18.0，STM32CubeF4 1.28.3

## 1. 合同含义

本合同汇总 P5-S2-T01～T04 已审核的软件候选。`BSP_CONTRACT_CANDIDATE_FROZEN` 表示 `.ioc`、
生成初始化代码、BSP 公开边界、host tests 和 ARM 构建相互一致，允许后续 S3～S6 纯软件轨道继续。

它不表示板卡、Shield、跳线、供电、电平、波形或外设已经实测。只有到货后完成最小板级一致性检查，
才允许另行升级为 `BSP_CONTRACT_HARDWARE_FROZEN`。

## 2. Pinmap 与安全初值

| 功能 | MCU 引脚 | 候选配置 | 上电/空闲边界 |
|---|---|---|---|
| SWDIO | PA13 | SYS_JTMS-SWDIO | 调试保留 |
| SWCLK | PA14 | SYS_JTCK-SWCLK | 调试保留 |
| USART2 TX/RX | PA2/PA3 | AF7，115200 8N1 | VCP 调试候选 |
| USART1 TX/RX | PA9/PA10 | AF7，19200 8E1 | RS485 UART 候选 |
| RS485 DE | PA8 | GPIO output | reset/receive low |
| SPI1 SCK/MISO/MOSI | PA5/PA6/PA7 | AF5，Mode 3，MSB first | software NSS |
| BME280 CS | PB6 | GPIO output | high/deselected |
| ADXL345 CS | PC7 | GPIO output | high/deselected |
| ADXL345 INT1 | PB4 | rising-edge EXTI pin candidate | NVIC/callback 尚未启用 |
| I2C2 SCL/SDA | PB10/PB3 | AF4 open-drain，100 kHz | 外部上拉待实测 |
| CAN1 RX/TX | PB8/PB9 | AF9，500 kbit/s init candidate | 收发器与总线待实测 |

PA5 不再用于 NUCLEO LD2 heartbeat。`RS485_DE` 上电 low，两个 SPI CS 上电 high；任何后续再生成都必须
保留这些 safe-state。

## 3. Clock 与初始化顺序

候选时钟：

```text
source = HSI 16 MHz
PLL M/N/P = 8/180/2
SYSCLK = 180 MHz
HCLK = 180 MHz
PCLK1 = 45 MHz
PCLK2 = 90 MHz
HAL tick source = TIM6
HAL tick quantum = 1 ms
```

自 P5-S3-T01 A1 起，HAL 的 1 ms timebase 由 TIM6 独占；`SysTick` 不再调用
`HAL_IncTick()`，保留给后续原生 FreeRTOS kernel tick。当前仅为配置、生成代码与交叉构建候选，
不表示 TIM6/SysTick 已在板上测量。

P5-S3-T01 A2 已由原生 FreeRTOS ARM_CM4F port 接管 SysTick、PendSV 与 SVC；TIM6 继续只负责 HAL
tick。P5-S3-T03 将 USART1、DMA2 Stream2 和 DMA2 Stream7 固定为 priority 6/subpriority 0，并使用
`xTaskNotifyFromISR(eSetBits)` 唤醒 `protocol_task`。NVIC 使用 `NVIC_PRIORITYGROUP_4`，FreeRTOS
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`；数值 6 满足 Cortex-M4 port 的 BASEPRI 边界。

生成初始化顺序：

```text
HAL_Init
  -> SystemClock_Config
  -> MX_GPIO_Init
  -> MX_DMA_Init
  -> MX_CAN1_Init
  -> MX_I2C2_Init
  -> MX_SPI1_Init
  -> MX_USART1_UART_Init
  -> MX_USART2_UART_Init
  -> app_boot_initialize
```

频率当前来自配置和运行时 profile 候选，尚未通过板级测量确认。

## 4. 总线与 DMA/IRQ 合同

### 4.1 USART1 / RS485

- PA9/PA10，19200，8E1，无硬件流控；
- RX：DMA2 Stream2 / Channel4，peripheral-to-memory，normal mode；
- TX：DMA2 Stream7 / Channel4，memory-to-peripheral，normal mode；
- DMA2 Stream2、DMA2 Stream7 和 USART1 IRQ 已启用；
- 三个 IRQ 均为 priority 6/subpriority 0，可调用批准的 FreeRTOS `FromISR` API；
- 固定最大 frame 为 64 bytes；
- DE 只在最终 USART transmission complete 后回到 low；
- callback 只捕获事件、必要时常数时间拉低 DE 并通知；frame copy、DMA rearm、状态推进和错误恢复在
  `protocol_task`；
- RX half transfer 默认关闭，若意外出现只计入诊断事件；
- `P5_RS485_LOOPBACK_SMOKE` 默认 `OFF`。

### 4.2 SPI1

- master、2-line、8-bit、Mode 3、MSB first、software NSS；
- prescaler 32，在 PCLK2 90 MHz 下候选速率 2.8125 Mbit/s；
- BME280 和 ADXL345 共用 SPI1，片选独立；
- transaction 前后恢复两个 CS high；
- 默认 timeout 20 ms，不使用 SPI DMA；
- `P5_DEVICE_PROBE_SMOKE` 默认 `OFF`。

### 4.3 I2C2

- PB10/PB3，100 kHz，7-bit addressing；
- 应用和合同只保存 VEML7700 7-bit address `0x10`；
- 仅在 HAL boundary 左移地址；
- 默认 timeout 20 ms，不使用 I²C DMA；
- VEML7700 当前只验证 address/register presence，不声称 silicon ID。

### 4.4 CAN1

- PB8/PB9，prescaler 6，BS1 12TQ，BS2 2TQ，SJW 1TQ；
- 500 kbit/s 只是初始化候选；
- 当前不启动 CAN、不设置 filter、不定义 message ID；
- S6 另行冻结物理层、filter、队列、错误恢复和消息合同。

## 5. 当前 BSP 公开边界

### Clock

- `bsp_clock_tick_ms()`；
- `bsp_clock_get_profile()` / `bsp_clock_profile_is_expected()`；
- `bsp_clock_elapsed_ms()` / `bsp_clock_interval_elapsed()`。

### RS485

- `bsp_rs485_uart_handle()` / `bsp_rs485_set_transmit()`；
- `bsp_rs485_initialize()` / `bsp_rs485_send()` / `bsp_rs485_poll()`；
- `bsp_rs485_register_irq_notifier()` / `bsp_rs485_service_irq_events()`；
- `bsp_rs485_is_busy()` / `bsp_rs485_take_received()`；
- `bsp_rs485_get_diagnostics()`。

### SPI/I²C/CAN

- `bsp_spi_bus_handle()` / `bsp_spi_bus_read_register()`；
- `bsp_i2c_bus_handle()` / `bsp_i2c_bus_is_device_ready()` / `bsp_i2c_bus_read_register()`；
- `bsp_can_handle()`。

CubeMX 生成模块拥有句柄和 `MX_*_Init()`。BSP 只借用句柄，不释放、不重复初始化。当前 SPI/I²C
bare-metal busy flag 不是 FreeRTOS mutex。

## 6. 已验证与未验证

| 范围 | 当前状态 | 允许结论 |
|---|---|---|
| Host Debug/Release | PASS | clock、RS485 状态机、device-probe 纯逻辑按测试合同通过 |
| ARM Debug/Release | PASS | STM32F446RE firmware build/link chain closed |
| BSP 静态一致性 | PASS candidate | 稳定配置在当前仓库来源中一致 |
| ST-LINK/VCP/启动 | WAITING_FOR_HARDWARE | 不声称已枚举、烧录或启动 |
| Clock/GPIO 实测 | WAITING_FOR_HARDWARE | 不声称 180 MHz、1 ms 或 safe-state 已测量 |
| UART/RS485 | WAITING_FOR_HARDWARE | 不声称 TTL/差分固定字节已收发 |
| SPI/I²C | WAITING_FOR_HARDWARE | 不声称模块 ID、ACK、CS 或 pull-up 已验证 |
| CAN | WAITING_FOR_HARDWARE | 不声称收发器或 CAN 对端联调 |

Host 和 cross-build 结果不能升级为 `PASS_HARDWARE`。

## 7. 允许扩展与变更规则

本合同冻结稳定板级事实，不锁死后续设计。S3～S6 可以经审核增加：

- FreeRTOS task、queue、notification、mutex 和 ISR ownership；
- 完整传感器驱动、采样、补偿、FIFO 和 interrupt；
- Modbus register/function/CRC/timing；
- CAN filter、message ID、queue、bus-off 和联调逻辑。

若改变 pin、clock、bus instance/parameter、DMA/IRQ、safe-state 或初始化所有权，必须同步：

1. `.ioc` 和必要生成代码；
2. 本合同；
3. 静态合同检查；
4. 受影响 host/ARM 回归；
5. 对应任务验证记录。

普通 API 增量若不改变稳定板级事实，只需更新 API 清单和相关测试，不要求重新执行无关硬件项目。

## 8. 到货后的硬件冻结门

硬件到货后按“裸板、Shield、单设备、组合”顺序补验：

1. ST-LINK/VCP、烧录、reset 和最小启动；
2. runtime clock profile、有限 heartbeat 和 GPIO safe-state；
3. USART1 loopback、RS485 固定字节和 DE 有界；
4. BME280/ADXL345 单设备 ID、共享 SPI 和独立 CS；
5. VEML7700 `0x10` ACK、register presence、SDA/SCL idle high；
6. CAN 只在 S6 合同明确后做物理与对端联调。

部分硬件到货时按接口记录局部结果，其余保持 `WAITING_FOR_HARDWARE`。只有要求范围的实物 pinmap、
跳线和最小访问均一致后，才升级为 `BSP_CONTRACT_HARDWARE_FROZEN`。

正常通过只保留摘要；只有故障需要复现时才增加日志、照片、波形或 `.private/diagnostics/` 记录。
