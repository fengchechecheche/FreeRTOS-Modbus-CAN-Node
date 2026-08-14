# protocol

项目自有 Modbus RTU 与 CAN 协议纯逻辑边界。P5-S4-T04 冻结的 17 个
`app_measurement` logical field ID、source、quality 和整数单位是两种协议后续映射的
唯一上游字典。

这些 ID 不是 Modbus register address、function code、CAN message/arbitration ID 或
wire ABI。S5/S6 必须分别显式选择宽度、endianness、saturation 和状态编码，并通过
cross-interface tests 保证同一样本的单位、sequence 与 freshness 不矛盾。禁止把 C
struct 的 enum、bool、padding 或 pointer 直接复制到总线。

P5-S5-T01 已定义 Modbus RTU slave register map contract：默认地址 4、零基地址、
`0x03/0x04/0x06` 子集、122-register 连续 input image、4-register holding image，
以及显式整数端序/metadata/atomicity。机器权威表位于
[`register_map.json`](register_map.json)，人类合同位于
[`../docs/modbus_contract.md`](../docs/modbus_contract.md)。

该 map 当前为 `CANDIDATE_VALIDATED`。Modbus CRC/parser/handler 和 UART/RS485 runtime
仍为 `NOT_IMPLEMENTED`；CAN filter/message/encoder 也仍为 `NOT_IMPLEMENTED`。合同
validator PASS 不代表已能收发 Modbus 帧。
