# P5-S5-T05 Modbus HIL 准备与状态报告

> 软件准备：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> Host PTY：`PASS_HOST_PTY / [042] d7428e62a2df72325020ed63ab7979f4fb8c12f9`
> 独立 USB-RS485：`PASS_HARDWARE_FIXED_ADDRESS_4`
> 地址迁移 H08/H09：`NOT_RUN_BY_POLICY`
> 项目三联调：`PASS_BOUNDED_INTEROP`
> 默认从站地址：4
> 串口：19200 bit/s，8E1

## 1. 当前结论

P5-S5-T05 已完成无硬件准备和 Host PTY 补测；2026-08-19 执行了旧 USB-RS485 的有界故障排查，2026-08-21 又以新 USB-RS485 完成交叉验证。因此当前可以说明：

- HIL 请求、响应判定和安全开关已具备纯软件自测入口；
- 项目真实 C 语言 RTU stream、server 和 register image 已通过 Host PTY 端到端测试；
- 项目五 T01～T04 的 Host、合同和 ARM 构建继续通过；
- PTY 与新转换器物理线路中的 249 B 响应均已通过；物理 H01～H07、复位后的代表性 H01 和转换器断开/重连后的代表性 H01 均通过；
- `RS485-03` 在“新 CH340 转换器、短线共地、地址 4、19200 8E1”边界内标记为 `PASS`；旧转换器仍保留为与 Shield 组合时的兼容性故障历史，不能据此宣称旧转换器普遍损坏；
- H08/H09 涉及有效地址写入，因未获得本轮显式写授权而保持 `NOT_RUN_BY_POLICY`；本轮没有执行地址 4→5→4 迁移；
- 项目三 `[039]` 已建立地址 4 只读 profile；在 Ubuntu-24.04-Gateway x86_64、新 CH340、
  短线共地边界内，真实 JSONL、一次 NUCLEO RESET 恢复和约 60 秒本地 MQTT 投影通过，
  `P3-01` 升级为 `PASS`。

## 2. 轻量探针

探针位于 `tools/modbus_hil_probe.py`。不带 pyserial 也能运行：

```bash
python3 tools/modbus_hil_probe.py --self-test
python3 tools/modbus_hil_probe.py --dry-run
```

结果：

```text
P5 MODBUS HIL SELF-TEST: PASS (15 checks, serial NOT_OPENED)
P5 MODBUS HIL DRY-RUN: serial NOT_OPENED
```

`--self-test` 检查 CRC、正常响应、异常响应、写 echo、错误 CRC、错误地址、错误功能、长度和静默判定。
`--dry-run` 只打印 H01～H07 的固定请求和预期结果，不枚举或打开 COM/tty。

真实串口模式必须显式指定端口：

```bash
python3 tools/modbus_hil_probe.py --port <COM_OR_TTY> --timeout-ms 500
```

串口模式才按需导入项目虚拟环境中的 pyserial。工具固定为 19200 8E1、一次一个请求、有界超时；不会自动扫描端口。

### 2.1 Host PTY 补测

提交 `[042] d7428e62a2df72325020ed63ab7979f4fb8c12f9` 在 Ubuntu 权威项目仓库中重新构建并执行：

```bash
cmake --preset host-debug
cmake --build --preset host-debug --target p5_host_modbus_pty_slave
python3 tools/modbus_pty_test.py \
  --server out/host-debug/p5_host_modbus_pty_slave
```

结果：

```text
P5 MODBUS PTY: PASS (10/10, max_response=249 B,
production_c_server=yes, physical_rs485=NOT_RUN)
```

Python 使用 raw/no-echo PTY 和现有 HIL probe；PTY 对端链接生产 CRC、ADU、RTU stream、server 与
register image 模块。H01～H07 的 10 个具体用例全部通过，其中 H03 返回 CRC 正确的 249 B；H07 的坏
CRC、广播读取和非本机地址均在有界超时内静默。Host Debug/Release 各为 22/22 PASS。

PTY 不承载真实奇偶校验位，也没有 UART DMA、DE/RE、收发器、电缆或终端电阻，因此该结果只关闭
`RS485-02`，不能关闭 `RS485-03`、`P3-01` 或 `HW-002`。

### 2.2 USB-RS485 回程故障补测

2026-08-19 在短线、共地、`A→A`、`B→B`、地址 4、19200 8E1 条件下运行真实 H01。主机发送
`04 03 00 00 00 04 44 5C` 后未收到响应，H02～H07 因 H01 未建立通信而停止，写入项保持
`NOT_RUN`。

随后临时烧录 SHA-256 为
`ae5952c0e8017f9d2cbafbfe9a5f60016998ff0a7cfa64bbfa5224951f086acb` 的有限三轮诊断固件：

- STM32 经 Shield 发出的 `P5T03` 能被 CH340 USB-RS485 完整接收；
- 主机等待 50 ms 后原样回送 5 字节，STM32 仍停在第 1/3 轮；
- STM32 诊断状态为 `waiting=1`、`completed=0`、USART1 `HAL error=4`（framing error），接收完成计数为 0；
- 8E1/8N1/8O1 回送、A/B 交叉、Shield/跳帽重插及 RX/TX 两个跳帽本体互换均未消除故障；
- A/B 交叉时没有有效接收活动，因此最终恢复 `A→A`、`B→B`。

排查完成后已重新烧录并校验默认 Debug ELF，SHA-256 为
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca`；VCP 再次出现启动、时钟摘要和
5 次 heartbeat。当前证据证明 STM32→Shield→USB-RS485 正向链路可用，但不能唯一地区分
USB-RS485 发送/方向控制与 Shield MAX485/`485_RX→RX1→PA10` 回程路径。已选购另一只 USB-RS485，
该轮结束时、交叉验证尚未执行，`RS485-03` 当时保持 `FAIL`；该历史结果不因后续通过而改写。
在该历史故障排查阶段，项目三联调仍为 `NOT_RUN`；后续通过结果见 2.4 节。

新转换器完成参考路线验收后，又将旧转换器接回完全相同的 Shield 接线与默认固件。旧转换器在
初次请求、NUCLEO RESET 后以及转换器断开/重连后三次执行 H01 均为 `response=<silence>`；转换器
TXD 指示有活动而 RXD 无活动。每次均在 H01 失败后有界停止，没有继续执行 H02～H11。LED 只能证明
转换器侧出现发送活动且未显示接收活动，不能代替 A/B 差分波形、Shield RO 或 PA10 的分层观测。

### 2.3 新 USB-RS485 交叉验证与固定地址 4 补验

2026-08-21 先将新旧两只 CH340 USB-RS485 以 `A→A`、`B→B`、`GND→GND` 背对背连接，固定为
19200 8E1。旧→新与新→旧各发送 10 次，均取得 10/10 字节完全一致的结果，无超时、乱码或重复。
该结果说明旧转换器并非完全失效，但不消除其与 Shield 组合时的方向控制、电气或时序兼容性差异。
结合新转换器在相同接线、固件和串口参数下通过，而旧转换器在复位与重连后仍稳定复现静默，当前
最强结论是“旧转换器与 Shield 的特定组合存在兼容性/稳定性限制”；现有证据仍不足以在自动方向
控制、差分驱动电平、接收门限或收发切换时序之间确定唯一根因。

随后使用新转换器、NUCLEO-F446RE 与 Waveshare RS485 CAN Shield，在短线、共地、`A→A`、
`B→B`、默认地址 4、19200 8E1 条件下运行只读/异常矩阵。被测源码等价基线为
`[060] 39c6114eba1b547d29741d142582f3828a902dc7`，默认 Debug ELF SHA-256 为
`bd72b55c84350d433aebb7c0eaee14705b3ae3422604f19c732d54fd2808f73f`；设备完整序列号未记录。

```text
H01 PASS
H02 PASS
H03 PASS
H04 PASS
H05 PASS
H06A PASS
H06B PASS
H07A PASS
H07B PASS
H07C PASS
P5 MODBUS HIL: PASS_READ_ONLY (10/10, address writes NOT_RUN, H10/H11 MANUAL_NOT_RUN)
```

H03 收到 CRC 正确的 249 B 响应。H06B 发送的是预期得到 exception `0x03` 的非法零地址写请求，
不构成有效配置写入。之后人工复位 NUCLEO，地址 4 的代表性 H01 再次 `PASS`；断开并重连新转换器
后，COM 端口重新枚举正常，地址 4 的代表性 H01 再次 `PASS`。因此 H10/H11 仅证明“当前固定地址 4
在复位和转换器重连后仍可读取”，不证明地址 5 会在复位后恢复为 4。

H08/H09 需要 `--allow-address-write --confirm-default-address 4` 双重显式授权，本轮未执行，保持
`NOT_RUN_BY_POLICY`。在该固定地址补验结束时，项目三地址 4 profile 与互操作仍为
`NOT_RUN`；后续通过结果见 2.4 节。

### 2.4 项目三地址 4 真实互操作

2026-08-21 使用项目三 `[039]
17d67873f83488b08ea0eee0fa28c8722b0913d6` 的地址 4 只读 profile，与项目五 `[067]
a173717deb0814019a51a73f59834b9c3c5fd309` 的默认固件联调。项目五默认 Debug ELF
SHA-256 为
`d076ddf743020fe1a043e776ba3196ea1f02153a17c5d98451cc722d6ac0018f`。主站运行在
Ubuntu-24.04-Gateway x86_64，通过新 CH340 `1a86:7523`、短线共地、`A→A`、`B→B`、
19200 8E1 读取地址 4；完整设备序列号未记录。

首轮省略全部 MQTT 和写入参数，以 JSONL sink 连续运行约 141.145 秒：17 个地址全部覆盖，
1946/1946 请求成功、失败 0，1946 条 telemetry 均为 `valid_sample`。运行中人工按一次
NUCLEO RESET，串口未重开、未离线、未超时，最大相邻完成间隔为 856 ms，最终有界停止。

JSONL 通过后，启用项目三现有 MQTT Debug 构建并向本地 Mosquitto `127.0.0.1:1883`
投影约 60 秒。网关 822/822 个 Modbus 请求成功，MQTT publish success 825、failure 0；
订阅器收到 822 条 telemetry、覆盖 17 个 topic，全部为 `fresh/valid_sample`。BME280、
VEML7700、ADXL345 和 health 必需主题各收到 60 条；设备签名为 20533，四源 present mask
为 15。成功路线原始日志在形成摘要后删除。

因此 `P3-01` 在上述指定提交、二进制、转换器和台架边界内为 `PASS`，`HW-002` 可以关闭。
该结论不覆盖 H08/H09 地址迁移、Raspberry Pi/ARM64 实物 RS485、多个真实从站、远程或 TLS
MQTT、重复断线、严格恢复上界或硬件长稳。

## 3. 写地址安全门

默认串口模式只运行 H01～H07，不写配置。地址 4→5→4 迁移必须同时提供两个显式参数：

```bash
python3 tools/modbus_hil_probe.py \
  --port <COM_OR_TTY> \
  --allow-address-write \
  --confirm-default-address 4
```

脚本先验证旧地址静默、新地址响应，再恢复地址 4。任一迁移步骤失败时立即停止，并提示先确认当前地址；不会盲目重复写入。H10 复位和 H11 物理断开/重连保持人工步骤。

## 4. 固定请求

| ID | 请求 | 预期 |
|---|---|---|
| H01 | `04 03 00 00 00 04 44 5C` | 读取 holding 0..3 |
| H02 | `04 04 00 00 00 12 70 52` | 读取 input 0..17 |
| H03 | `04 04 00 00 00 7A 71 BC` | 122 registers、249 B 响应 |
| H04 | `04 05 00 00 FF 00 8C 6F` | exception `0x01` |
| H05 | `04 04 00 79 00 02 A0 47` | exception `0x02` |
| H06A | `04 04 00 00 00 00 F0 5F` | exception `0x03` |
| H06B | `04 06 00 00 00 00 89 9F` | exception `0x03` |
| H07A | H01 的坏 CRC 版本 | 静默 |
| H07B | 地址 0 read | 静默 |
| H07C | 非本机地址 5 read | 静默 |

这些请求用于排障和可重复执行。H01～H07 已在 PTY 和新 USB-RS485 物理线路中发送并通过；其中
H06B 是预期返回 exception `0x03` 的非法值测试，不等同于有效地址写入。

## 5. 硬件最小矩阵

| ID | 状态 | 到货后通过条件 |
|---|---|---|
| H01 | `PASS` | holding 0..3 响应正确 |
| H02 | `PASS` | identity/generation/mask 可解析 |
| H03 | `PASS` | 收到 CRC 正确的 249 B 响应 |
| H04 | `PASS` | exception `0x01` |
| H05 | `PASS` | exception `0x02` |
| H06 | `PASS` | H06A/H06B 均返回 exception `0x03` |
| H07 | `PASS` | 坏 CRC、广播和非本机地址静默 |
| H08 | `NOT_RUN_BY_POLICY` | 有效地址写入未获本轮显式授权 |
| H09 | `NOT_RUN_BY_POLICY` | 因 H08 未执行，不执行 5→4 恢复 |
| H10 | `PASS_BOUNDED` | 未执行地址迁移；复位后当前地址 4 代表性 H01 通过 |
| H11 | `PASS` | 新转换器断开/重连后代表性 H01 通过 |

每项一次明确成功即可，H11 再提供一次恢复后的代表性读。不要求长稳、示波器、零误码率或大量重复。

## 6. 后续执行顺序

1. 保持当前新转换器、短线共地、`A→A`、`B→B`、19200 8E1 作为参考接线；
2. 只有在需要验证易失地址迁移且取得显式授权后，才执行 H08/H09，并最终确认地址为 4；
3. 项目三 `[039]` 地址 4 profile 与有界互操作已通过；后续只在扩展 Raspberry Pi、商用从站、
   写地址或生产 MQTT 时另行制定计划；
4. RS485 与 CAN 各自通过后执行的有界双总线并发和故障隔离矩阵现已完成；边界见 `docs/dual_bus_fault_matrix.md` 的 2026-08-21 实物补验。

非隔离 USB-RS485 只用于短线、共地台架。总线上必须只有一个活动主站。

## 7. 项目三边界

项目三 `[039] 17d67873f83488b08ea0eee0fa28c8722b0913d6` 已增加独立地址 4 只读
profile，并由项目三 production loader 合同测试覆盖。profile 只使用 `0x04`，不复用
`motor_actuator` 语义，也不提供 `write_function`。

在 Ubuntu-24.04-Gateway x86_64 与新 CH340 参考路线内，项目三已经持续读取真实项目五节点并
完成 JSONL、一次 NUCLEO RESET 恢复和约 60 秒本地 MQTT 投影。因此窄范围互操作为
`PASS_BOUNDED_INTEROP`。项目三整体仍保持 `PUBLISHED=false`、`HARDWARE_VALIDATED=false`、
`TAG=null`；本结果不能替代 Raspberry Pi、商用从站、CAN、电气安全或硬件长稳。

## 8. 证据规则

Host PTY 正常通过只保存提交 SHA、10/10 摘要和 249 B 最大响应。硬件正常通过才保存固件哈希、
端口、19200 8E1、地址、接线摘要和 H01～H11 状态。只有失败时才增加一条代表性请求/响应、失败类别、
当前可能地址和恢复结果。

项目三联调成功路线只保留两个项目提交、默认 ELF SHA-256、转换器 VID:PID、串口 profile、
JSONL/MQTT 运行时长、请求/主题计数、代表字段首尾值和 RESET 恢复摘要；原始逐帧日志已删除。

不保存持续串口日志、逐帧历史、大型抓包、设备完整序列号或与排障无关的数据。
