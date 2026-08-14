# P5-S5-T05 Modbus HIL 准备与状态报告

> 软件准备：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> 独立 USB-RS485：`WAITING_FOR_HARDWARE`
> 项目三联调：`NOT_RUN`
> 默认从站地址：4
> 串口：19200 bit/s，8E1

## 1. 当前结论

P5-S5-T05 已完成无硬件准备，但没有打开串口、烧录板卡或运行真实 USB-RS485 请求。因此当前可以说明：

- HIL 请求、响应判定和安全开关已具备纯软件自测入口；
- 项目五 T01～T04 的 Host、合同和 ARM 构建继续通过；
- 真实 249 B 响应、地址迁移、复位恢复和项目三联调仍未执行；
- 项目三地址 4 profile 仍为 `not_created`，项目三仓库没有被本任务修改。

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

这些请求用于排障和可重复执行，不代表已经在线路上发送。

## 5. 硬件最小矩阵

| ID | 状态 | 到货后通过条件 |
|---|---|---|
| H01 | `NOT_RUN` | holding 0..3 可解释 |
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

正常通过只保存提交/固件哈希、端口、19200 8E1、地址、接线摘要和 H01～H11 状态。只有失败时才增加一条代表性请求/响应、失败类别、当前可能地址和恢复结果。

不保存持续串口日志、逐帧历史、大型抓包、设备完整序列号或与排障无关的数据。

