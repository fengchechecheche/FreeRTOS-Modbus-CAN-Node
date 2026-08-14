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

该 map 当前为 `CANDIDATE_VALIDATED`。P5-S5-T02 已增加 HAL/RTOS-free CRC16、完整 ADU
envelope encode/decode 和 8E1 tchar/t1.5/t3.5 纯整数计算；Host 与 ARM 编译结果只证明完整
caller buffer 上的纯逻辑。

流式 parser、分片/粘连、DMA/IDLE、DE/RE、功能码 handler 和 UART/RS485 runtime 仍为
`NOT_IMPLEMENTED`。当前 BSP 上限仍为 64 B，不能承载完整 249 B input-image response；CAN
filter/message/encoder 也仍为 `NOT_IMPLEMENTED`。详见
[`../docs/modbus_codec.md`](../docs/modbus_codec.md)。

兼容 T01 状态标记 `CRC/parser/handler` 时必须按层解释：CRC complete-buffer core 已实现，
parser/handler/runtime 仍未实现。
