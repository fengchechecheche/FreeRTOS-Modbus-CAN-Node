# protocol

> P5-S5-T04 adds the HAL/RTOS-free function server above the 256 B RTU stream.
> CRC-valid ADUs now receive bounded `0x03/0x04/0x06` or exception responses;
> physical UART/RS485 validation remains pending. See
> [`../docs/modbus_server.md`](../docs/modbus_server.md).

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

该 map 合同为 `CANDIDATE_VALIDATED`，runtime 为 `CANDIDATE_IMPLEMENTED`。P5-S5-T02 提供
HAL/RTOS-free CRC16、ADU 和 timing；T03 提供 stream/RS485 transport；T04 提供纯 C function
server 与 register image。Host 与 ARM 编译结果只证明软件候选。

默认路径支持地址 4、广播/外站静默、`0x03/0x04/0x06`、exception `01/02/03/04`、249 B
最大应答和 TX-complete 后地址提交。CRC/parser/handler 均已接入候选 runtime；DE 波形、DMA
丢失、真实主站请求和项目三联调保持 `WAITING_FOR_HARDWARE / NOT_RUN`。CAN
filter/message/encoder 仍为 `NOT_IMPLEMENTED`。

`CRC/parser/handler` 的 `CANDIDATE_IMPLEMENTED` 不等于物理总线通过；硬件状态必须单独解释。
