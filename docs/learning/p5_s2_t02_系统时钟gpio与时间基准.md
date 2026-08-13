# P5-S2-T02 系统时钟、GPIO 与时间基准

> 内容状态：`FROZEN`
> 软件状态：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> 硬件状态：`WAITING_FOR_HARDWARE`

## 1. 目标与边界

T02 为后续 UART、SPI、I²C、RTOS 和传感器时间戳建立统一基础：明确系统与总线时钟、GPIO 安全
初值、HAL 毫秒 tick 和有限调试输出。本轮板卡尚未到货，因此完成的是可构建、可测试的软件候选，
不是板级 180 MHz 或 1 秒心跳实测。

本任务没有启用 FreeRTOS、RTC、高分辨率计时、MCO、UART DMA 或任何总线事务，也没有重新生成
CubeMX 工程。

## 2. 关键设计与实施

### 2.1 为什么保留 HSI 180 MHz 候选

当前 `.ioc` 使用内部 HSI 16 MHz，经 PLLM=8、PLLN=180、PLLP=2 得到 180 MHz SYSCLK。AHB 不分频，
APB1/2 分别除以 4 和 2，因此 HCLK、PCLK1、PCLK2 为 180、45、90 MHz。

保留 HSI 可以避免在板卡未到时猜测 HSE、ST-LINK MCO 或焊桥状态。代价是不能声称具备外部晶振级
精度；若后续通信或长期计时确实需要更高精度，再基于实物评审 HSE。

### 2.2 GPIO 为什么不使用板载 LED

PA5 同时是 SPI1 SCK 和 Nucleo LD2。将它切回 GPIO 会破坏已冻结的 SPI pinmap，因此调试见证继续
使用 USART2。RS485_DE 保持低，BME280_CS 和 ADXL345_CS 保持高，避免外设在初始化阶段被误选中。

### 2.3 毫秒时间如何跨回绕

`HAL_GetTick()` 返回 32 位毫秒计数，约 49.7 天回绕。时间差使用：

```c
elapsed_ms = now_ms - start_ms;
```

无符号减法能在一次回绕范围内得到正确差值。项目将 HAL 适配与纯时间计算拆开，使 host 测试无需
链接 STM32 HAL。当前 API 只承诺毫秒单调时间，不代表 FreeRTOS tick、微秒计时或绝对时间。

### 2.4 为什么心跳只有五次

T01 启动标记继续保留。T02 先读取 HAL RCC 派生的 SYSCLK/HCLK/PCLK1/PCLK2 与 tick quantum，匹配
候选值后输出一条 clock 摘要，再在 idle 路径最多尝试五次 1 秒 heartbeat。无论 UART 成功或失败，
尝试次数都有上限，之后回到 `__WFI()`，避免调试输出永久占用 CPU 和串口。

## 3. 验证结果

- `.ioc` 保持原 SHA-256 `ca1dd7439fa1b6228916359ce8fb6c31f13dbb5e16ebae970be2ad54c4f17508`；
- host smoke 与 clock 测试 2/2 通过；
- clock 测试覆盖普通差值、到期边界和 32 位回绕；
- firmware Debug/Release 均生成 ELF、HEX、BIN、MAP；
- Debug HEX：`3cadf0d3483122332bb9ed586bc05b9144ac30b8c08b2060fce44ba17d74dfc1`；
- Release HEX：`d87362cc28acb96ffbfca5f6993ce7e523156f6ed64b832eac3597ce0eaa267b`；
- Debug 大小为 text 8240 / data 148 / bss 1940 / total 10328 bytes；
- Release 大小为 text 7208 / data 144 / bss 1936 / total 9288 bytes；
- 两个 ELF 都包含 T01 启动、T02 clock 和 heartbeat 标记；
- `.ioc` 与 CubeMX 生成的 `Core/` 没有变更。

链接器仍有既有 `_close/_lseek/_read/_write` `nosys` 提示。T02 直接调用 HAL UART，没有引入 `printf`
或 stdio 重定向，因此该提示不影响链接通过，也不证明 VCP 已实测。

## 4. 实际问题与修复

首轮有限心跳实现只在 UART 发送成功时增加计数。如果 UART 持续失败，固件会每秒无限重试，与“最多
五次”的边界不一致。修订后计数表示发送尝试次数，无论成功或失败都递增，最多五次后停止。

该问题在提交前代码审查中发现并立即修复，修订后 host 2/2 和两种固件构建重新通过。它没有形成
跨会话阻塞，因此不创建诊断 JSON。

## 5. 限制与下一步

当前状态是 `READY_FOR_HARDWARE + WAITING_FOR_HARDWARE`，不是 `PASS_HARDWARE`。板卡到货后需要先
通过 T01 的 ST-LINK 与最小启动硬件门，再烧录 T02 Debug HEX，观察 clock/heartbeat，并以宽松
0.8～1.2 秒中位间隔排除明显分频错误。

没有示波器或逻辑分析仪不阻塞补验。VCP 时间只能证明粗粒度 tick 可用，不能证明 HSI 精度、温漂、
RTC、FreeRTOS tick 或微秒级实时性。
