# P5-S5-T01 Modbus 从站合同与寄存器映射

## 目标与边界

本任务把 S4 的统一样本变成项目五独立的 Modbus register contract。它只冻结地址、类型、
单位、metadata、诊断和写入语义，不实现 CRC、RTU parser、功能码 handler 或 UART/RS485
收发。因此完成状态是 `CANDIDATE_VALIDATED`，不是 Modbus runtime PASS。

## 关键设计与实施

项目五使用默认从站地址 4。项目三已有地址 1/environment_sensor、2/motor_actuator 和
3/fault_injection_device；旧周计划中的地址 2 会产生冲突，因此没有沿用，也没有修改项目三。

input map 使用零基 `0x0000..0x0079`，共 122 个连续寄存器，小于 `0x04` 单次读取上限
125。完整 image 包含：

- 身份、schema revision、generation 和状态 mask；
- 17 个 S4 measurement field；
- 四个 source 各自的 state、flags、quality、sequence、sample time 和 age；
- 三设备 fault/recovery、ADXL IRQ/drop、health 与 RS485 error 摘要。

17 个值保持 centi-°C、Pa、milli-%RH、millilux、millig 原生整数单位。有符号字段使用
int32 二进制补码，无符号字段使用 uint32；register high-byte-first，32 位 high-word-first。
没有 float、自动端序猜测或 16 位截断。

invalid 没有 sentinel。若 `value_present=0`，值寄存器即使为 0 也没有工程意义；retained
last-good 必须与 state、age、quality 和 retained flag 一起使用。

holding map 只有4项。`0x06` 白名单仅允许 active slave address；合法范围 1..247，成功
响应 TX complete 后生效，复位恢复地址 4，不写 Flash。

机器权威 map 使用 JSON，validator 只依赖 Python 标准库。后续 C 代码必须逐字段编码，
不能把 C enum、bool、padding 或 struct 直接复制到线路。

## 验证结果

- Modbus validator：852 项事实 PASS；
- 内存负例自检：地址、重叠、越界、宽度、访问、白名单、float、metadata、word order、
  读上限、外来语义和 runtime-PASS 共12类 mutant 均被拒绝；
- Host Debug/Release：各 14/14 PASS；
- BSP contract/self-test：489 项 PASS / PASS；
- Firmware Debug/Release：`40648/160/11184 B`、`34064/156/11176 B`，与 `[024]` 一致；
- static-only resource contract：PASS，linker heap 0。

固件尺寸不变是预期结果，因为 T01 没有加入 runtime C source。

## 实际问题与修复

validator 初版把整个 JSON 文本中的禁用关键词都当成寄存器语义检查，导致
`scope_exclusions` 和项目三兼容性说明中用于表达“禁止”的 `motor_actuator`、CAN 和生产
fault-injection 字样也被误报。修复后禁用语义检查只扫描 holding/input entry，说明性边界
仍可保留；相应 mutant 改为真正篡改 register entry 名称。

文档 marker 初版还使用了与标题大小写不一致的 `runtime/hardware` 字符串，正常检查及时
暴露。修正为匹配实际文档标记后，normal 和 self-test 均通过。没有为了通过检查而放宽
地址、类型、metadata 或 runtime 边界。

## 限制与下一步

T02 实现 CRC/codec 与黄金向量，T03 实现接收帧边界和半双工时序，T04 才实现
0x03/0x04/0x06 handler、request-local image 和地址切换。USB-RS485 与项目三地址 4 profile
均需硬件和独立授权，当前保持 `WAITING_FOR_HARDWARE` / `NOT_CREATED`。
