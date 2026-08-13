# P5-S2-T03｜UART DMA 与 RS485 方向控制

> 内容状态：`FROZEN`（P5-S2-T03 内容审核通过）

## 1. 目标与结论边界

本任务为 STM32F446RE 的 USART1 建立 DMA 收发和 RS485 半双工方向控制候选。软件已经通过 host 测试与
ARM Debug/Release 交叉构建，但板卡、Shield 和 USB-RS485 尚未到货，因此不能写成“RS485 已通信”。

本任务只识别固定 ASCII 请求 `P5T03` 并返回 `P5T03OK`。它不是 Modbus 帧，不包含站址、功能码、CRC
或 3.5-character silent interval。

## 2. 19200、8E1 为什么配置成 9-bit + even parity

USART1 的线上格式是 8 data bits、even parity、1 stop bit。STM32F4 UART 开启 parity 后，word length
包含 parity 位置。因此 HAL 配置使用：

```c
huart1.Init.WordLength = UART_WORDLENGTH_9B;
huart1.Init.Parity = UART_PARITY_EVEN;
huart1.Init.StopBits = UART_STOPBITS_1;
```

其中 9-bit word length 与 parity 组合后，实际有效 payload 仍为 8 bits。这不是 9-bit 业务数据。

## 3. DMA complete 不等于串口发送完毕

UART TX 至少经历两段：

```text
memory --DMA--> USART data register --shift register--> TX pin
```

DMA complete 只说明内存搬运结束。最后一个 byte 可能仍在 USART shift register 中。如果这时立即把
RS485 DE 拉低，末尾 parity/stop bit 甚至最后一个 byte 可能被截断。

当前 STM32CubeF4 HAL 的 normal TX DMA 流程为：

1. DMA complete 后关闭 UART DMAT；
2. 开启 USART `TCIE`；
3. shift register 真正清空后产生 USART `TC`；
4. `USART1_IRQHandler()` 调用 `HAL_UART_IRQHandler()`；
5. HAL 最终调用 `HAL_UART_TxCpltCallback()`。

所以本项目只在第 5 步把 PA8/DE 拉低。

## 4. 三层实现边界

### 4.1 纯状态机

`bsp_rs485_state.c` 不包含 HAL、CMSIS 或寄存器头文件，只通过四个可注入操作访问平台：

- 设置 DE；
- 启动 TX DMA；
- abort TX；
- 挂接 RX DMA。

它维护 `IDLE_RX`、`TX_ACTIVE`、`FAULT_RECOVERY` 三个状态以及少量计数。这样 host 测试可以精确验证
调用顺序，而不伪造 STM32 寄存器。

### 4.2 HAL adapter

`bsp_rs485.c` 把操作映射到 `HAL_UART_Transmit_DMA()`、`HAL_UARTEx_ReceiveToIdle_DMA()`、
`HAL_UART_AbortTransmit()` 和 PA8 GPIO。它还提供 64-byte 静态 RX/TX 缓冲、短 callback 和诊断快照。

### 4.3 app smoke

`app_rs485_smoke.c` 只消费完整 RX event：匹配 `P5T03` 后提交 `P5T03OK`。状态机先复制 response 到内部
TX 缓冲，因此调用者 buffer 在异步发送期间不会失效。

## 5. 发送顺序

正常发送顺序是：

```text
IDLE_RX
  -> 检查 length 与 busy
  -> 复制到 64-byte static TX buffer
  -> DE=HIGH
  -> HAL_UART_Transmit_DMA
  -> TX_ACTIVE
  -> internal DMA complete（DE 仍为 HIGH）
  -> USART TC / HAL_UART_TxCpltCallback
  -> DE=LOW
  -> 确保 RX DMA 已挂接
  -> IDLE_RX
```

如果 DMA start 失败，立即恢复 DE 低。若最终 callback 长时间不出现，主循环使用 T02 的 32-bit 毫秒
tick 触发超时；无符号减法使其跨 `UINT32_MAX` 回绕仍成立。

timeout 按 19200 bit/s、每 character 11 bits 和实际 payload length 计算，再加 20 ms 宽松裕量。它只用于
防止 DE 永久卡高，不是 Modbus 协议 timing。

## 6. normal Receive-to-Idle DMA

RX 使用 DMA2 Stream2 / Channel4 normal mode，buffer 为 64 bytes。USART IDLE 或 buffer full 时，HAL 给出
实际接收长度；callback 复制一次完整候选 frame 后重新挂接 DMA。half-transfer notification 没有消费
价值，因此启动 RX 后关闭 HT interrupt，避免制造半缓冲事件。

发送期间 RX DMA 不强制停止：

- 没有 Shield 的 PA9/PA10 回环需要同时 RX；
- Shield 若在发送态禁用 receiver，RX 只会保持等待；
- 若硬件产生本机回显，最多形成一个有界 RX event，不改变 DE 完成条件。

## 7. ISR 与主循环的职责

ISR callback 只做短操作：复制最多 64 bytes、更新状态/计数、切 DE 或重新挂 RX。固定请求匹配和 response
提交由主循环执行。读取 RX frame 和诊断快照时短暂屏蔽 interrupt，避免 ISR 覆盖同一份静态数据。

本阶段没有 RTOS，因此不引入 queue、semaphore、task notification 或 heap。S3 启用 FreeRTOS 后再重新
评审 ISR-to-task 交接，而不是提前制造两套并发模型。

## 8. host mock 证明什么

`p5.host.rs485` 验证：

1. initialize 后 DE 低且 RX armed；
2. DE high 一定先于 DMA start；
3. busy 时拒绝第二次发送；
4. 0 byte 和超过 64 bytes 被拒绝；
5. start failure 恢复 DE 低；
6. 只有最终 TX complete 才恢复接收；
7. timeout、UART error、RX rearm failure 有界退出；
8. `P5T03` 匹配而其他输入不响应。

mock 不证明 DMA interrupt 真在 MCU 上触发，也不证明波特率、parity、A/B、终端或差分电压正确。

## 9. 被动 smoke 与本地回环模式

默认 `P5_RS485_LOOPBACK_SMOKE=OFF`，节点不会主动占用总线，只响应指定 request。到货后的裸板 UART
自检可临时设为 `ON`，此时只发送三次有限探针，完成后停止。回环线必须直接连接 PA9/PA10，且不得在
安装 Shield、接入外部 RS485 bus 时启用主动回环模式。

## 10. 硬件安全与宽松验收

到货后先做无 Shield 的 PA9/PA10 回环，再接 Shield：

1. 不通电核对 Shield 版本、3.3 V、DE/RE jumpers、terminal resistor；
2. 非隔离 USB-RS485 只用于 short wire common-ground bench；
3. A/B 不通时先断电，再依据 manual 和现象修订映射；
4. 短台架默认不额外启用 termination；只在 physical endpoints 且确有需要时启用；
5. 三次固定双向交换成功且 DE 不 stuck 即满足 T03 最小验收。

没有 logic analyzer 不构成拒收理由。出现末字节截断、DE 卡高或偶发错误时才抓 waveform/raw bytes。

## 11. 本轮问题与修复

- CubeMX CMake generator 产生大量与 T03 无关文件：审核后移到临时备份，仓库只保留 DMA 必需生成物；
- WSL 直接执行 `verify_host.sh` 时发现 CRLF shebang：只规范化为 LF，随后 Debug/Release 入口均通过。

本轮没有需要跨会话排查的故障，因此没有生成诊断 JSON。

## 12. 当前限制与下一步

当前状态为 `PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`。UART 回环、RS485 physical layer、DE
actual timing 和 PC adapter interoperability 都是 `WAITING_FOR_HARDWARE`。

内容审核已通过，T03 软件候选已冻结；硬件到货后补做第二步。完成本次提交与双端同步后，再单独制定
P5-S2-T04 实施计划。
