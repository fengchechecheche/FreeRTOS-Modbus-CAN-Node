# P5-S6-T02｜bxCAN 过滤器、中断与发送队列

> 状态：`READY_FOR_CONTENT_REVIEW`
> 软件结果：`CAN_RUNTIME_CANDIDATE_IMPLEMENTED`
> 硬件结果：`WAITING_FOR_HARDWARE`

## 目标与边界

本任务把 T01 的七类报文接入 bxCAN 和既有 `can_task`，但不把无硬件测试外推为物理 CAN 通过。
实现必须固定内存、有界执行，并让 CAN 启动失败、拥塞或 bus-off 只影响 CAN 自身，不能阻塞
传感器采集和 Modbus。

## 关键设计与实施

- 两个 16-bit list filter bank 只接收七个标准数据帧 ID；DLC 必须为 8。
- TX/RX0/SCE 中断优先级统一为 6/0，回调只复制/合并固定元数据并调用
  `xTaskNotifyFromISR`，没有解析、延时、重启或总线访问。
- RX 使用四帧 ring，事件使用八槽 FIFO；周期遥测使用 latest-wins，BME 双帧保持 sequence 原子性。
- `can_task` 每 100 ms 或收到通知后服务一次，每次最多取两帧 RX、提交三帧 TX；`HAL_BUSY`
  不弹出队头，事件连续两帧后让出一次周期帧。
- bus-off/启动失败至少等待 1 s，在任务上下文 stop/start，最多尝试三次后锁存。只读 snapshot
  保存定位故障需要的计数，不生成原始帧历史。

## 验证结果

纯逻辑测试覆盖 exact filter、RX ring、事件合并、latest-wins、BME 配对、busy 保序、公平性、
时间回绕和三次恢复锁存。Host Debug/Release、ARM Debug/Release、CAN/Modbus/BSP 合同和资源门
均以 A5 最终记录为准。物理 ACK、波形、candump 和真实 bus-off 仍为 `NOT_RUN`。

## 实际问题与修复

CubeMX 界面曾把 CAN1 TX/RX0/SCE 显示为不可编辑的 0 级优先级。直接保留会让后续
`FromISR` 调用越过 FreeRTOS 的最大系统调用中断优先级。处理方式是先恢复本次广泛生成，再只修订
`.ioc`、CAN 初始化和 IRQ 文件，把三个 IRQ 固定为 6/0，并用合同和编译结果复核。

接入遥测投影测试后，CAN Host 目标首次构建缺少 `sensors/include`，因为统一 measurement 头会
传递包含三个传感器驱动头。只给该目标补充已有 include 目录后定向测试通过，没有复制或伪造
传感器类型。

## 限制与下一步

当前完成的是软件候选，不是板级验收。T03 需在硬件准入后检查收发器供电、共地、两端 120 Ω、
500 kbit/s 对端和代表性 candump；T04 再验证 CAN/RS485 并发背压与故障隔离。在此之前保持
`WAITING_FOR_HARDWARE`。
