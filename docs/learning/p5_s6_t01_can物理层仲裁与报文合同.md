# P5-S6-T01｜CAN 物理层、仲裁与报文合同

> 状态：`READY_FOR_CONTENT_REVIEW`
> 软件结果：`CANDIDATE_VALIDATED + READY_FOR_BXCAN_IMPLEMENTATION`
> CAN runtime：`NOT_IMPLEMENTED`
> 硬件结果：`WAITING_FOR_HARDWARE`

## 目标与边界

本任务先冻结 CAN wire contract，再让后续 bxCAN 驱动按合同实现。它不接入 CAN task、filter、
IRQ、mailbox 或发送队列，也不打开 SocketCAN/CANable。Host 固定向量和 ARM 交叉编译只能证明
纯逻辑候选，不能证明收发器、终端、位时序或实物帧已经通过。

首版使用 Classical CAN 2.0A、11 位标准数据帧、500 kbit/s、DLC 8 和 little-endian。节点号
为 4，与 Modbus 默认地址数值相同，但 CAN ID 与 Modbus register/address 互不复用。本协议是项目
自定义遥测，不宣称 CANopen、J1939 或 UDS。

## 关键设计与实施

### 仲裁和 ID

ID 按 `class_base + (node_id << 4) + subtype` 分组。数值较小者优先：

| ID | 内容 | 周期/触发 |
|---:|---|---|
| `0x140` | 状态事件 | 变化触发并限速 |
| `0x240` | heartbeat | 1 s |
| `0x241` | 健康摘要 | 1 s 或状态变化 |
| `0x340/0x341` | BME 温度/气压/湿度双帧 | 1 s |
| `0x342` | 光照摘要 | 1 s |
| `0x440` | resultant RMS 振动摘要 | 1 s |

事件优先，但 steady-state 不周期发送。首版不发送 100 Hz X/Y/Z，避免演示数据占用无必要带宽。
六个周期帧加最多十个事件帧按 150 bit/frame 保守估算为 0.48%，合同上限为 1%。这只是静态
预算，不是硬件测得的总线负载。

### payload 和状态

每帧 byte 0 是 revision 1，byte 1 是 8 位 sequence。数据帧使用统一 source sequence 低八位；
`255 -> 0` 是正常回绕。`data_flags` 保留两位 state、retained 和 any-quality-warning，保留位
必须为零。完整 quality 仍由 Modbus/诊断路径提供，CAN 不复制所有元数据。

BME 使用两帧，是因为温度、Pa 气压和 milli-percent 湿度无法在保留 revision/sequence/state
后无损放进一个 8-byte payload。消费者只在两帧 revision 和 sequence 相同时配对，避免丢半帧后
把不同采样拼在一起。

invalid 使用显式 sentinel：温度 `INT16_MIN`、压力 `0xFFFFFF`、32 位测量 `0xFFFFFFFF`；age
按 100 ms 编码并饱和，最大码之外保留 unknown。codec 对 ID、DLC、revision、severity/source、
湿度范围、uint24 和保留位返回显式错误，不做 ABI `memcpy` 或静默截断。

### 代码边界

机器权威表是 `protocol/can_message_map.json`。`p5_can_contract.h/.c` 只依赖标准 C，使用固定 8 字节
buffer，无 HAL、RTOS 和动态内存。Host 测试覆盖七类帧、known-good bytes、负温度、uint24、状态
pack、sequence 回绕、age 饱和、BME 配对和错误输入。codec 被加入 ARM source 仅用于交叉编译，
没有被 runtime 调用。

## 验证结果

- CAN contract self-test：5 类 mutant 均被拒绝；
- CAN Host Debug/Release：新增 `p5.host.can_contract` 通过；
- 全量 Host Debug/Release：19/19 通过；
- CAN/Modbus/BSP 合同：通过；
- ARM Debug/Release：交叉构建通过；
- 固件常驻资源与 `[029]` 基线一致，因为未调用的 codec 被链接器回收；
- `git diff --check` 和 12 文件允许清单：通过；
- CAN hardware、SocketCAN、filter/IRQ/queue：`NOT_RUN / NOT_IMPLEMENTED`。

## 实际问题与修复

向 CMake 文件应用小范围上下文补丁时，`patch` 以 fuzz 1 匹配并生成了两个 `.orig` 备份。修改本身
落在预期位置，但备份文件不属于允许清单。处理方式是先检查 diff 和目标上下文，再精确删除这两个
由本轮生成的备份；最终残留检查确认没有 `.orig/.rej`。后续同类修改应优先扩大唯一上下文，减少
模糊匹配。

设计审查还发现，如果 BME 强行只用一个 payload，就必须粗化湿度/压力或丢失 freshness。最终改成
相同 sequence 的双帧，并增加“不一致不得合并”的 Host 向量，避免用隐式精度损失换取表面简洁。

## 限制与下一步

T02 才实现 bxCAN filter、IRQ、task notification、mailbox、有界发送队列、事件合并和错误状态机，
并审核当前 AutoBusOff/AutoRetransmission 策略。T03 在硬件准入后使用已确认的收发器、CANH/CANL/
GND、两端 120 Ω 和 candleLight/SocketCAN 做实物联调。在此之前保持
`WAITING_FOR_HARDWARE`，不把 CTest frame 当作 candump frame。
