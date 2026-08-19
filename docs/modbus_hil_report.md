# P5-S5-T05 Modbus HIL 准备与状态报告

> 软件准备：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> Host PTY：`PASS_HOST_PTY / [042] d7428e62a2df72325020ed63ab7979f4fb8c12f9`
> 独立 USB-RS485：`FAIL_HARDWARE_RETURN_PATH / WAITING_FOR_CROSS_CHECK`
> 项目三联调：`NOT_RUN`
> 默认从站地址：4
> 串口：19200 bit/s，8E1

## 1. 当前结论

P5-S5-T05 已完成无硬件准备和 Host PTY 补测；2026-08-19 又执行了真实 USB-RS485 有界排查。因此当前可以说明：

- HIL 请求、响应判定和安全开关已具备纯软件自测入口；
- 项目真实 C 语言 RTU stream、server 和 register image 已通过 Host PTY 端到端测试；
- 项目五 T01～T04 的 Host、合同和 ARM 构建继续通过；
- PTY 中的 249 B 响应已通过；物理 H01 已执行但因回程帧错误失败，H02～H11、物理 249 B 响应、地址迁移和项目三联调仍未执行；
- RS485 当前标记为“硬件回程故障待交叉验证”，不把失败归因于 Modbus 软件、USB-RS485 或 Shield 中的任一单点；
- 项目三地址 4 profile 仍为 `not_created`，项目三仓库没有被本任务修改。

## 2. 轻量探针

探针位于 `tools/modbus_hil_probe.py`。不带 pyserial 也能运行：

```bash
python3 tools/modbus_hil_probe.py --self-test
python3 tools/modbus_hil_probe.py --dry-run
```

结果：

```text
P5 MODBUS HIL SELF-TEST: PASS (14 checks, serial NOT_OPENED)
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
在交叉验证前 `RS485-03` 保持 `FAIL`，项目三联调保持 `NOT_RUN`。

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

这些请求用于排障和可重复执行。H01～H07 已在 PTY 字节流中发送；物理线路当前只执行到失败的
H01，不能表述为 H01～H07 或 USB-RS485 矩阵通过。

## 5. 硬件最小矩阵

| ID | 状态 | 到货后通过条件 |
|---|---|---|
| H01 | `FAIL` | 请求已发送但响应静默；回程诊断触发 USART1 framing error，等待参考转换器交叉验证 |
| H02 | `NOT_RUN` | identity/generation/mask 可解析 |
| H03 | `NOT_RUN` | 收到 CRC 正确的 249 B 响应 |
| H04 | `NOT_RUN` | exception `0x01` |
| H05 | `NOT_RUN` | exception `0x02` |
| H06 | `NOT_RUN` | exception `0x03` |
| H07 | `NOT_RUN` | 坏 CRC、广播和非本机地址静默 |
| H08 | `NOT_RUN` | 4→5 后地址 5 响应 |
| H09 | `NOT_RUN` | 5→4 后地址 4 恢复 |
| H10 | `NOT_RUN` | 复位后易失地址回到 4 |
| H11 | `NOT_RUN` | 断开/重连一次后可再次读取 |

每项一次明确成功即可，H11 再提供一次恢复后的代表性读。不要求长稳、示波器、零误码率或大量重复。

## 6. 到货后执行顺序

1. 回到 S2，确认 NUCLEO、Shield、USB-RS485、电压、A/B/GND、终端和供电边界；
2. 先完成 ST-LINK、最小固件和默认地址 4 的只读请求；
3. 依次执行 H01～H07；
4. 明确允许配置写后执行 H08/H09，并确认最终地址为 4；
5. 人工复位执行 H10，物理断开/重连执行 H11；
6. 独立链路达到 `PASS_HARDWARE` 后，再申请修改项目三仓库。

非隔离 USB-RS485 只用于短线、共地台架。总线上必须只有一个活动主站。

## 7. 项目三边界

项目三当前基线 `8e0e909a8b7576ab80b6f2ade186631910226b47` 只有地址 1～3，且本地分支领先远端 5 个提交。本任务没有处理该 Git 状态，也没有创建地址 4 profile。

后续必须先取得独立 USB-RS485 `PASS_HARDWARE`，再建立项目三独立允许清单和用户授权。首版只建议只读投影设备签名、map revision、image generation、主要传感器值、ADXL resultant RMS、health state 和 warning mask；不复用 `motor_actuator` 语义，也不默认执行 `0x06`。

## 8. 证据规则

Host PTY 正常通过只保存提交 SHA、10/10 摘要和 249 B 最大响应。硬件正常通过才保存固件哈希、
端口、19200 8E1、地址、接线摘要和 H01～H11 状态。只有失败时才增加一条代表性请求/响应、失败类别、
当前可能地址和恢复结果。

不保存持续串口日志、逐帧历史、大型抓包、设备完整序列号或与排障无关的数据。
