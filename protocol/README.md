# protocol

项目自有 Modbus RTU 与 CAN 协议纯逻辑边界。P5-S4-T04 冻结的 17 个
`app_measurement` logical field ID、source、quality 和整数单位是两种协议后续映射的
唯一上游字典。

这些 ID 不是 Modbus register address、function code、CAN message/arbitration ID 或
wire ABI。S5/S6 必须分别显式选择宽度、endianness、saturation 和状态编码，并通过
cross-interface tests 保证同一样本的单位、sequence 与 freshness 不矛盾。禁止把 C
struct 的 enum、bool、padding 或 pointer 直接复制到总线。

当前 Modbus parser/register table/CRC 和 CAN filter/message/encoder 均为
`NOT_IMPLEMENTED`。
