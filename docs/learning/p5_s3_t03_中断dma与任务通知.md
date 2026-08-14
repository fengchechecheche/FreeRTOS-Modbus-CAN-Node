# P5-S3-T03 中断、DMA 与任务通知

## 目标与边界

本任务只建立 USART1、DMA2 Stream2/Stream7 到既有 `protocol_task` 的第一条通知链。目标是让中断快速
交出工作，同时保留原有 5 ms 绝对周期和静态内存合同。本轮不新增任务、queue、semaphore，也不提前实现
Modbus、CAN 中断或传感器 EXTI/DMA。

板卡尚未到货，因此当前结论是 `PASS_HOST + PASS_CROSS_BUILD`。真实 DMA interrupt、上下文切换、RS485
收发和 ISR-to-task latency 都仍是 `NOT_MEASURED`。

## 关键设计与实施

### 1. 为什么 priority 0 不能直接通知 FreeRTOS

STM32 的优先级数字越小，紧迫度越高。本项目的
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5`，所以调用 `xTaskNotifyFromISR()` 的外设中断必须使用
数值 5 或更大。USART1 和两个 DMA IRQ 已通过 CubeMX 设为 6/0；TIM6 保持 0，因为它只维护 HAL tick，
不调用 FreeRTOS API。

### 2. ISR 只传递有限事件

四个事件位分别代表 RX frame、RX half、TX complete 和 UART error。callback 只记录事件和必要 metadata，
然后使用 `xTaskNotifyFromISR(eSetBits)` 唤醒 `protocol_task`。TX complete/error 允许立即把 DE 拉低，这是
释放半双工总线的常数时间安全动作。

以下工作全部迁到 task context：

- 复制最多 64 B 的 frame；
- 重新挂接 Receive-to-IDLE DMA；
- 推进 TX/RX 状态；
- UART error 分类、abort 和 recovery；
- smoke/未来协议解析。

### 3. notification 不是消息队列

FreeRTOS notification value 只保存唤醒 bit。固定 mailbox 另外保存 RX 长度、HAL error 和少量诊断计数。
同一个 bit 在 task 消费前重复到达时只保留一份 pending event，并增加 coalesced counter。RX DMA 在 task
完成复制前不会重挂，因此 DMA 不会同时覆盖正在复制的 buffer。

notifier 尚未注册时，事件仍留在 mailbox；`protocol_task` 启动后先 drain，避免对空 task handle 调用
FreeRTOS。这个 mailbox 不是多帧 ring buffer；连续 Modbus 帧的数据所有权留给后续任务单独设计。

### 4. 通知不能推迟 5 ms timeout

`protocol_task` 每次只等待到下一个绝对 release：通知先到就处理一份 bounded snapshot，随后重新检查
release；没有通知则在 release 到期时运行原有 RS485 poll/smoke。这样连续通知不会把 timeout 变成“最后
一次事件之后再等 5 ms”。其他四个任务继续使用原来的 `vTaskDelayUntil()`。

### 5. 延迟观测保持默认关闭

`P5_IRQ_NOTIFICATION_SMOKE` 默认 `OFF`。临时打开时，DWT cycle counter 只生成
`sample_count/min/max/last`，不保存逐事件 trace。32-bit wrap 用无符号差值处理。Host 只验证聚合数学，
不会生成伪硬件 latency。

## 验证结果

- Host Debug/Release 各 7/7；
- 覆盖正常、重复、half、invalid length、error conflict、早到事件、notification storm 和 tick/cycle wrap；
- Firmware Debug/Release 编译链接通过；
- Debug clean build `text/data/bss = 20136/160/8728` B；
- Release clean build `text/data/bss = 17608/156/8724` B；
- resource checker 保持通过，dynamic allocation 仍为 0；
- BSP checker 能拒绝 priority 0 和 callback 内 `memcpy()` 负例；
- DWT smoke 的 ON 分支编译通过，最终默认恢复 OFF。

## 实际问题与修复

CubeMX 在只修改 IRQ priority 时额外生成了 CMSIS、工具链和无关源文件。所有超范围输出先移动到仓库外
可恢复备份，再把仓库差异收敛到 `.ioc`、`dma.c` 和 `usart.c`。实施检查器首次运行时漏导入 Python
`re` 模块，补齐导入后正向合同和全部负向 mutant 通过。

## 限制与下一步

硬件到货后只补做三次固定 UART/RS485 交换和一份 latency 汇总；不强制长稳、原始 trace 或真实 error
注入。单个 max 超过 5 ms 时先检查 priority、长临界区和 DMA flag，不直接否决整个任务。完成硬件补验前，
不能声称通知链已经在 NUCLEO-F446RE 上执行。
