# P5-S2-T02 候选时钟与 GPIO 报告

> 生成日期：2026-08-14
> 配置层级：`PASS_CONFIG + PASS_CROSS_BUILD`
> 硬件层级：`WAITING_FOR_HARDWARE`

## 1. 结论

当前 `freertos_modbus_can_node.ioc`、CubeMX 生成的 `SystemClock_Config()` 与项目 T02 时钟合同一致：
STM32F446RE 使用 HSI 16 MHz，经 PLL 生成 180 MHz SYSCLK；HCLK、PCLK1、PCLK2 分别为
180 MHz、45 MHz、90 MHz。HAL 使用 SysTick 提供 1 ms tick。

本轮没有修改或重新生成 `.ioc` 和 `Core/`。这些结论属于配置推导和交叉构建，不证明实际板卡频率、
HSI 精度或 VCP 心跳周期已经实测。

## 2. 主时钟推导

| 项目 | 值 | 推导 |
|---|---:|---|
| HSI | 16 MHz | 当前 PLL 输入源 |
| PLLM | 8 | PLL 输入 `16 / 8 = 2 MHz` |
| PLLN | 180 | VCO `2 × 180 = 360 MHz` |
| PLLP | 2 | SYSCLK `360 / 2 = 180 MHz` |
| AHB divider | 1 | HCLK 180 MHz |
| APB1 divider | 4 | PCLK1 45 MHz |
| APB2 divider | 2 | PCLK2 90 MHz |
| APB1 timer clock | 90 MHz | APB 分频不为 1，timer clock 加倍 |
| APB2 timer clock | 180 MHz | APB 分频不为 1，timer clock 加倍 |
| Flash latency | 5 wait states | 当前生成配置 |
| 电压/性能 | Scale 1 + OverDrive | 当前生成配置 |

`.ioc` SHA-256：

```text
ca1dd7439fa1b6228916359ce8fb6c31f13dbb5e16ebae970be2ad54c4f17508  freertos_modbus_can_node.ioc
```

该文件沿用 S1 创建的便携配置，作为 S2 `.ioc` v1；T02 未因文档任务制造无意义再生成差异。

## 3. 外设时钟消费者

| 外设 | 输入时钟 | 当前配置 | T02 边界 |
|---|---:|---:|---|
| USART1 | PCLK2 90 MHz | 19200，8E1 | 不执行 RS485 收发 |
| USART2 | PCLK1 45 MHz | 115200，8N1 | 仅候选调试输出 |
| SPI1 | PCLK2 90 MHz | prescaler 32，2.8125 MHz | 不访问器件 |
| I2C2 | PCLK1 45 MHz | 100 kHz | 不访问器件 |
| CAN1 | PCLK1 45 MHz | prescaler 6，15 TQ，500 kbit/s | 不发帧 |

当前配置未启用 USB、SDIO 或其他 CK48 消费者。PLLQ/48 MHz 域没有在本工作块验收，不得从
180 MHz 主时钟配置推导出 USB 可用结论。

## 4. 时间基准合同

- `bsp_clock_tick_ms()` 返回 `HAL_GetTick()`；
- HAL tick quantum 候选为 1 ms；
- `bsp_clock_elapsed_ms(now, start)` 使用无符号减法跨越一次 32 位回绕；
- `bsp_clock_interval_elapsed()` 只用于短周期调度，当前心跳周期为 1000 ms；
- 32 位毫秒 tick 约 49.7 天回绕；
- 当前没有 FreeRTOS tick、微秒时钟、RTC 或绝对时间；
- tick 不跨复位连续，不承诺 HSI 温漂精度。

host 测试已覆盖普通差值、未到期、恰好到期及 `0xfffffff0 -> 0x00000020` 的 48 ms 回绕差值。

## 5. GPIO 安全初值

| 信号 | 引脚 | 初值/模式 | 边界 |
|---|---|---|---|
| RS485_DE | PA8 | low | 保持接收态 |
| BME280_CS | PB6 | high | 保持未选中 |
| ADXL345_CS | PC7 | high | 保持未选中 |
| ADXL345_INT1 | PB4 | rising-edge input | NVIC/回调仍未启用 |
| SWD | PA13/PA14 | SWD | 保留调试 |
| USART2 VCP | PA2/PA3 | AF7 | 待实物焊桥/VCP 验证 |

PA5 是 SPI1 SCK，同时连接板载 LD2，因此没有改作 GPIO heartbeat。PA8 已属于 RS485_DE，T02 也没有
启用 MCO1；PC9 未因测量方便而新增 MCO2 占用。

## 6. 调试输出候选

T01 的五次启动标记保持不变。T02 在运行时 RCC profile 匹配后输出一条固定时钟摘要，并在
`app_boot_idle()` 中按回绕安全的 1 秒间隔最多尝试发送五次 heartbeat。成功或失败均只尝试五次，
之后继续 `__WFI()`，避免永久刷屏。

```text
P5 S2 T02 CLOCK SYS=180000000 HCLK=180000000 PCLK1=45000000 PCLK2=90000000 TICK=1MS
P5 S2 T02 HEARTBEAT OK
```

Debug 和 Release ELF 均静态包含上述标记。板卡未到货，因此没有 COM、VCP 或周期实测结果。

## 7. 官方参考与未关闭范围

- [RM0390：STM32F446xx reference manual](https://www.st.com/resource/en/reference_manual/dm00135183.pdf)
- [STM32F446 官方文档页（含 DS10693）](https://www.st.com/en/microcontrollers-microprocessors/stm32f446/documentation.html)
- [UM1724：STM32 Nucleo-64 boards](https://www.st.com/resource/en/user_manual/dm00105823.pdf)

板卡到货后仍需补做 T01 硬件门、运行时 profile、VCP 心跳和粗粒度周期检查；本报告不能升级为
`PASS_HARDWARE`。

## 8. 2026-08-15 实板补验

上文保留无硬件阶段的原始判断；本节记录后续 NUCLEO-F446RE 增量补验。最终默认 Debug ELF 为：

```text
4b4fa7f110e74244d0b3850d3a313b8fc17b795eca0fee67039e503f34d772c7
```

VCP `115200 8N1` 在软件复位后得到 1 次启动、1 次时钟摘要和 5 次有限 heartbeat：

```text
P5 S2 T01 BOOT OK
P5 S2 T02 CLOCK SYS=180000000 HCLK=180000000 PCLK1=45000000 PCLK2=90000000 HALTICK=TIM6/1MS
P5 S2 T02 HEARTBEAT OK  # 共 5 次
```

首次实板启动暴露出 DWT 初始化顺序问题：在 `DEMCR.TRCENA` 尚未开启时读取 `DWT->CTRL` 会得到
不可靠值并误判 `NOCYCCNT`。将 trace enable 提前后，实板报告
`bsp_clock_cycle_counter_cycles_per_us=180`、`ready=1`，启动和 heartbeat 恢复。

暂停目标读取的 GPIO 状态为：

| 寄存器 | 值 | 判定 |
|---|---:|---|
| GPIOA_MODER | `0xA829A8A0` | PA8 `[17:16]=01` |
| GPIOA_ODR | `0x00000000` | PA8 bit 8 = 0 |
| GPIOB_MODER | `0x002A1080` | PB6 `[13:12]=01` |
| GPIOB_ODR | `0x00000040` | PB6 bit 6 = 1 |
| GPIOC_MODER | `0x00004000` | PC7 `[15:14]=01` |
| GPIOC_ODR | `0x00000080` | PC7 bit 7 = 1 |

一次 USB 断电重连后，ST-LINK/VCP 重新枚举，heartbeat count 为 5，DWT ready 和 FreeRTOS
scheduler running 均为 1，fault 为 0。该结果不等于排针电压测量，也不建立严格的 heartbeat
抖动或 HSI 精度结论；因此本报告仅把运行时 profile、有限 heartbeat 和 GPIO 寄存器状态提升为
`PASS_HARDWARE_LIMITED`。
