# P5-HW-OBS-01 长稳诊断与主机采集器

> 软件状态：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_10_MINUTE_ADMISSION`
> 硬件 10 分钟准入：`NOT_RUN`
> 60 分钟预跑：`NOT_RUN`
> 正式 8 小时长稳：`NOT_RUN`
> 实施基线：`[070] b8daebfbee46d5e3851b34963823b6b48c21779a`

## 目的与边界

`P5_SOAK_DIAGNOSTIC` 是默认关闭的固件构建选项。启用后，USART2 在启动阶段输出首条
`P5DIAG1`，随后每 60 秒输出一条固定键值记录。记录覆盖五任务推进/栈水位、诊断队列、健康、
复位、RS485、CAN 与四类传感源计数。它不改变 CubeMX、任务数量、引脚、时钟、Modbus/CAN
映射或动态内存策略；关闭开关时不启用该运行时路径。

`tools/soak_hil_collector.py` 面向 Linux/Raspberry Pi，使用同一长驻进程完成：

- 读取 USART2 的 `P5DIAG1`，忽略普通 BOOT/CLOCK/heartbeat 文本；
- 每分钟以 Modbus `0x04` 只读地址 4 的完整 122 个输入寄存器；
- 被动监听 CAN `0x140/0x240/0x241/0x340/0x341/0x342` 与错误帧，不发送 CAN；
- 把采样投影为既有 `soak_runner.py` Schema 1 JSONL；
- 保存分钟级传感器、完整 Modbus 快照、完整六 ID CAN 流、CAN 错误、主机资源与事件记录。

保留树莓派是因为它是后续真实部署环境，可同时稳定持有两个 USB 串口和 SocketCAN，并减少
Windows/WSL USB 转发层变量；它只是测试执行主机，不是项目三依赖，也不是项目五产品组件。

## 有界实现

- `P5DIAG1` 由独立纯 C formatter 生成；缓冲区不足时整条拒绝，不截断。
- 诊断 task 仍为 200 ms period/deadline；仅在长稳开关开启时执行预算提升为 100 ms，默认仍为 2 ms。
- 主机样本上限 600、单行上限 16 KiB、stderr 上限 64 KiB。
- 各详细证据文件最大 64 MiB；CAN 为被动监听，不逐条回注业务请求。
- 采集器异常、串口/CAN 退出、Modbus CRC/长度/签名错误、样本不足均有界退出并返回非零。
- 正常退出和信号退出均生成 manifest 与 `SHA256SUMS.txt`。

60 分钟和 8 小时阶段保留详细原始证据，包含传感器工程量，但不导出 ADXL345 每 20 ms 原始
样本，避免诊断本身改变实时行为。正式证据放在 `.private/soak/<session>/`，不进入公开报告。

## 判定

原有趋势门继续检查任务/传感源推进、复位、fault、栈水位、队列/CAN 容量、计数回退、连续错误
增长、样本缺失和持续时间。`p5-hil-v1` 采集器另外要求：

- 每个分钟窗口的 Modbus 122-register 快照、CRC 和 `0x5035` 签名有效；
- 六种周期 CAN ID 在会话内均至少出现一次；
- SocketCAN BUS-OFF 错误帧为硬失败；
- 非 BUS-OFF CAN 错误帧进入 `REVIEW_REQUIRED`，不得自动提升为通过；
- 孤立错误只允许聚焦排查并重跑同一时长。

## 离线验证结果

| 项目 | 结果 |
|---|---|
| P5DIAG1 formatter Host 测试 | PASS |
| HIL collector 自测 | PASS，4 项有界检查 |
| soak runner 自测 | PASS，23 项有界检查 |
| Host Debug | PASS，24/24 |
| BSP 合同 | PASS，622 项稳定事实及负向自测 |
| ARM Debug/Release，`P5_SOAK_DIAGNOSTIC=ON` | PASS |
| 诊断 Debug text/data/bss | `58204/160/15752` B |
| 诊断 Release text/data/bss | `48516/156/15752` B |
| 默认 OFF ELF 中 `P5DIAG1` 缺席 | PASS |

以上仅证明代码和构建准入，不能替代实板 10 分钟、60 分钟或 8 小时结果。

### 首次实板启动异常与有界修订

首次烧入的诊断 ELF 只能输出 `BOOT` 和时钟摘要，随后 heartbeat 与
`P5DIAG1` 均未出现，因此该次启动不计入 10 分钟准入。ARM 反汇编确认，原实现将多个
完整快照放在诊断任务栈中：`app_rtos_soak_diagnostic_service()` 的显式栈帧为
1252 B，嵌套的单次 `snprintf()` 格式化器另有 540 B 显式栈帧，已超过诊断任务现有
256 words（1024 B）静态栈。

最小修订没有扩大任务栈，而是把仅诊断构建使用的快照暂存区移至静态 BSS，并用有界的
十进制/十六进制追加器替代大参数 `snprintf()`。修订后 Debug ELF 中服务函数显式栈帧为
128 B（112 B 局部区加 16 B 保存寄存器），格式化入口为 72 B；当前最深的直接格式化
子调用仍使可见嵌套量保持在约 280 B，低于 1024 B。最大 `uint32_t`、缓冲区不足、空
指针和 CRLF 边界均由 Host Debug/Release 测试覆盖。该静态分析只恢复实板准入资格，
实际最低栈水位仍必须由新的 10 分钟准入确认。

## 构建与运行模板

先在 Ubuntu-24.04-STM32 构建诊断固件：

```bash
cmake --preset firmware-debug \
  -DP5_SOAK_DIAGNOSTIC=ON \
  -DP5_DEVICE_PROBE_SMOKE=OFF \
  -DP5_RS485_LOOPBACK_SMOKE=OFF \
  -DP5_RTOS_SCHEDULER_SMOKE=OFF \
  -DP5_IRQ_NOTIFICATION_SMOKE=OFF \
  -DP5_IWDG_RESET_SMOKE=OFF \
  -DP5_ADXL345_HIL_DIAGNOSTIC=OFF \
  -DP5_CAN_ACK_RX_DIAGNOSTIC=OFF \
  -DP5_CAN_ACK_TX_DIAGNOSTIC=OFF \
  -DP5_CAN_BOUNDED_ECHO_DIAGNOSTIC=OFF
cmake --build --preset firmware-debug
sha256sum out/firmware-debug/freertos_modbus_can_node.elf
arm-none-eabi-size out/firmware-debug/freertos_modbus_can_node.elf
```

树莓派使用稳定的 `/dev/serial/by-id/` 路径，并先把 CAN 固定为本项目已验证配置：

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.75 restart-ms 100
sudo ip link set can0 up
ip -details -statistics link show can0
```

审核、提交、双端同步并记录干净提交及诊断 ELF 哈希后，以实际值替换占位符：

```bash
python3 tools/soak_runner.py --run \
  --phase smoke \
  --duration-seconds 600 \
  --sample-seconds 60 \
  --expected-commit <40-hex-commit> \
  --expected-firmware-sha256 <64-hex-sha256> \
  --firmware-path out/firmware-debug/freertos_modbus_can_node.elf \
  --build-text-bytes <text> \
  --build-data-bytes <data> \
  --build-bss-bytes <bss> \
  --collector-kind p5-hil-v1 \
  --board-identity nucleo-f446re-stlink-v2-1-no-serial \
  --output-dir .private/soak/<session> \
  --collector-command -- python3 tools/soak_hil_collector.py \
    --vcp /dev/serial/by-id/<stlink-vcp> \
    --rs485 /dev/serial/by-id/<ch340-rs485> \
    --can-interface can0
```

`soak_runner.py` 自动向该采集器追加 session ID、时长、60 秒周期和私有原始证据目录，调用方
不得重复提供这些参数。

## 阶段门

1. 当前停在内容审核与用户提交/同步之前，不烧录、不形成硬件 PASS。
2. 10 分钟准入必须先达到 `PASS`；`REVIEW_REQUIRED` 只允许排查并重跑 10 分钟。
3. 10 分钟通过后，60 分钟与 8 小时可按已审核方案以“目标”持续推进。
4. 8 小时必须绑定干净提交和诊断 ELF SHA-256。
5. 8 小时通过后关闭 `P5_SOAK_DIAGNOSTIC`，重新构建并烧回默认固件，再执行额外 10 分钟回归。
6. 证据必须明确区分“诊断固件 8 小时”和“默认固件 10 分钟”，不得互相替代。
