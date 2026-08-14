# P5-S5-T02：Modbus CRC、完整组帧与静默间隔

> 状态：`READY_FOR_CONTENT_REVIEW`  
> 范围：纯 C 逻辑、Host 测试和 ARM 编译  
> 硬件：`WAITING_FOR_HARDWARE`

## 1. 为什么先拆出纯逻辑层

CRC、ADU 字节布局和间隔公式都不需要 UART 或 FreeRTOS。先把它们做成 caller-buffer 纯函数，
可以在主机上快速覆盖边界，再由后续状态机组合。这样 CRC 错误不会与 DMA、DE/RE 或真实线路
问题混在一起排查。

本任务中的“组帧”是对一个完整 ADU 的包装和验证，不是从任意字节流中寻找帧。分片、粘连、
噪声前缀和 t3.5 边界必须由 P5-S5-T03 的单一 stream state machine 处理。

## 2. CRC 数值与线路顺序

Modbus CRC 使用初值 `0xFFFF` 和 reflected polynomial `0xA001`。API 返回 16 位数值，encoder 再
显式发送低字节和高字节。例如地址 4 读取全部 input register：

```text
CRC 输入：04 04 00 00 00 7A
CRC 数值：0xBC71
线路尾部：71 BC
完整 ADU：04 04 00 00 00 7A 71 BC
```

把数值显示顺序直接复制到线路会得到 `BC 71`，Host wire-order 负例会拒绝它。

## 3. 完整 ADU 的安全边界

RTU ADU 上限是 256 B：1 B address、最多 253 B PDU、2 B CRC。本项目 API 把 function 单独作为
1 B，因此 function data 上限是 252 B。encoder 不分配内存，只写调用方提供的缓冲区；容量不足
时输出长度归零。

decoder 先检查 4..256 B，再检查 CRC，成功后才返回指向输入 frame 的只读 data view。它不检查
address 4、功能码或寄存器范围，以免和 T04 handler 重复实现语义。

## 4. 8E1 时间计算

8E1 的一字符包含 start、8 data、parity、stop，共 11 bit。19200 bit/s 下：

```text
tchar = ceil(11 / 19200 s)       = 573 us
t1.5  = ceil(1.5 * 11 / 19200 s) = 860 us
t3.5  = ceil(3.5 * 11 / 19200 s) = 2006 us
```

t1.5/t3.5 必须从精确分数独立向上取整，不能先把 tchar 取整后再乘，否则会额外放大误差。高于
19200 bit/s 使用 750/1750 us 固定值。当前 runtime profile 仍只有 19200 8E1。

## 5. 黄金向量的使用方式

测试复用项目三已冻结的 CRC 数值，但不复制项目三 C++ 算法。这样 production C 实现与 oracle
来源保持独立。项目三 19 个向量中只有 16 个是完整 frame；truncated、noise-prefix 和
concatenated 三项明确留给 T03，避免把 complete-buffer 检查误报成 parser 能力。

另外加入地址 4 的 `0x03/0x04/0x06` 代表 frame、4 B 最小结构、256 B 最大结构、257 B 过长、
capacity、bit flip、CRC 字节交换和 timing 临界点。

## 6. 当前未完成项

- 现有 BSP 仍只能收发 64 B，完整 122-register response 需要 249 B；
- 分片、粘连、超长流、早到/晚到字节和 drop counter 尚未实现；
- DMA/IDLE、DE/RE 与实际定时器尚未接线；
- 地址、功能码、register image、exception 和地址切换仍待 T04；
- 真实 RS485 字符时间、静默间隔和 249 B 传输仍待硬件。

因此 Host/ARM 通过只能写 `CANDIDATE_VALIDATED`，不能写 runtime 或 hardware PASS。
