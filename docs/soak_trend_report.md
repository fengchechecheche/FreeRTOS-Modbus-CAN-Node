# P5-HW-OBS-01 长稳诊断与主机采集器

> 软件状态：`PASS_HOST + PASS_CROSS_BUILD`
> 硬件 10 分钟准入：`PASS`
> 60 分钟预跑：`REVIEW_REQUIRED`（历史会话保留；异常已完成聚焦排查）
> 正式 8 小时长稳：`PASS`
> 默认固件 10 分钟回归：`PASS`
> 实施基线：`[070] b8daebfbee46d5e3851b34963823b6b48c21779a`
> 正式证据基线：`[074] e4770957ee579cf63f1c67e29937ef54af55d6e4`

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

### 树莓派部署与烧录默认边界

树莓派是采集与 `systemd` 托管主机，不承担 GitHub 同步或 ST-LINK 烧录。后续每次长稳或
树莓派实物联调都遵守以下默认操作：

1. **源码同步**：在已审核的 Ubuntu-24.04-STM32 工作区从干净提交生成 Git bundle，校验 bundle
   后通过 SCP 复制至树莓派；树莓派只对该本地 bundle 执行 `git fetch` 和 `git merge --ff-only`。
   禁止树莓派通过 SSH 或 HTTPS 连接 GitHub、配置 GitHub 凭据，或直接 `git pull`。
2. **固件烧录**：在 Windows 使用 STM32CubeProgrammer `v2.17.0` 和板载 ST-LINK 完成 ELF 下载、
   写入校验与复位。默认 CLI 固定为
   `D:\24.STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe`；后续烧录先直接检查该路径，只有
   文件不存在或版本不符时才重新定位安装目录。树莓派不安装、不调用 ST-LINK 烧录工具，也不把
   USB 设备透传作为烧录替代方案。
3. **运行接线**：烧录验证后，把 ST-LINK USB 接至树莓派，树莓派通过其稳定的 VCP
   `/dev/serial/by-id/` 路径采集诊断；USB-RS485 与 USB-CAN 同样连接树莓派。

该分工使 Windows 保持唯一的烧录环境，Ubuntu 保持唯一的构建与 bundle 来源，树莓派保持可在
SSH 控制通道短暂中断时独立持续运行的现场采集主机。

树莓派使用稳定的 `/dev/serial/by-id/` 路径，并先把 CAN 固定为本项目已验证配置：

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.75
sudo ip link set can0 up
ip -details -statistics link show can0
```

当前 candleLight/`gs_usb` 适配器明确返回 `Device doesn't support restart from Bus Off`，因此
不得反复加入无效的 `restart-ms 100`。本台架固定为 `restart-ms 0`，采集器把 BUS-OFF 错误帧
作为硬失败并停止晋级；恢复时重新冷启动适配器和同一阶段，而不声明自动恢复能力。

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
  --collector-command python3 tools/soak_hil_collector.py \
    --vcp /dev/serial/by-id/<stlink-vcp> \
    --rs485 /dev/serial/by-id/<ch340-rs485> \
    --can-interface can0
```

`soak_runner.py` 自动向该采集器追加 session ID、时长、60 秒周期和私有原始证据目录，调用方
不得重复提供这些参数。

runner 对 HIL 子进程的总时限为“计划时长 + 一个采样周期 + 退出宽限”。额外采样周期只用于
对齐任意相位开始的 `P5DIAG1`，不减少 10 分钟所需的 10 个样本，也不把 9 个样本提升为通过。
CAN 通过门只覆盖冻结合同中的六个周期 ID：`0x240/0x241/0x340/0x341/0x342/0x440`；
变化触发的事件 ID `0x140` 可以记录，但不得要求它在稳定长稳窗口中必然出现。

### 2026-08-22 首轮 10 分钟工具失败

会话 `eef183b2-1449-4fb9-bc22-d222236f7a96` 保留为 `FAIL`，不得改写。该轮形成 9 个完整
RS485/传感器样本和持续 CAN 原始帧，未出现 CAN 错误帧、BUS-OFF、任务 missed/deadline/
budget overrun、队列丢弃、RS485 错误、传感器故障或 MCU 复位。失败由两个工具契约错误触发：

- runner 在 `600 + 5 s` 终止了仍按自身 `600 + 60 s` 对齐边界等待第 10 个样本的采集器；
- CAN 必测集合误含非周期事件 `0x140`，同时遗漏周期振动摘要 `0x440`。

因此该轮既不能作为 10 分钟 PASS，也不能据此声称硬件失败。修订后必须绑定新的干净提交，
从空证据目录完整重跑 600 秒；旧证据仅用于解释工具修订，不与新会话拼接。

## 实板阶段结果

### 10 分钟诊断准入

会话 `p5_hw_obs01_smoke_073_rerun_20260821T201011Z` 在干净提交
`b9a5a97cd8fda7490e720ee38579bba88dd5f9f4` 和诊断 ELF SHA-256
`695679201e058e108c73166122e507e45837518d069bc4fd33e47889d66650a0` 上取得 10 个
60 秒样本，判定为 `PASS`。五任务栈最低水位分别为 protocol/acquisition/CAN/health/diagnostic
`190/167/118/52/153` words，队列最大占用为 0，CAN pending 最大值为 6；各窗口
RS485、CAN event drop 与 queue drop 均为 0。

### 60 分钟预跑与异常边界

会话 `p5_hw_obs01_prerun_073_20260821T2025Z` 形成 60 个完整样本，任务栈、队列、RS485
和传感器推进没有硬失败，但原始判定保留为 `REVIEW_REQUIRED`：SocketCAN 共记录 28 个非
BUS-OFF 错误帧。后续对 candleLight/`gs_usb`、主机 socket 生命周期、终端与公共 GND 的分相
排查表明该轮不能升级为 60 分钟 `PASS`，也不能据此判定 MCU 固件失稳。本报告不改写该历史
结果；最终长稳结论仅使用后续从空目录启动且无错误帧的正式 8 小时会话。

### 正式 8 小时诊断固件长稳

正式会话 `p5_formal_8h_074_20260822T0830Z` 绑定干净提交
`e4770957ee579cf63f1c67e29937ef54af55d6e4`、诊断 ELF SHA-256
`b66f909338ca9d23a266a06861809fb03a32cacd767e40ba26bcce0864231d32` 和
`nucleo-f446re-stlink-v2-1-no-serial`。判定器与独立复评均为 `PASS`：

- 480 个 60 秒样本，首末样本跨度 `28740000 ms`，四个窗口各 120 个样本；
- 五任务最低栈水位为 `190/167/118/52/153` words；protocol `214→190` 与 diagnostic
  `163→153` 只发生在启动收敛区，后续窗口不再下降；
- CAN pending 最大值 6、队列最大占用 0，四窗口 RS485/CAN event drop/queue drop 均为 0；
- 保存 172824 条 CAN 帧、480 条 host metrics、480 条 Modbus snapshot 与 480 条传感器
  timeseries；`can_errors.jsonl` 和 events 均为空；
- 会话结束时 `can0` 为 `ERROR-ACTIVE`，500 kbit/s、sample point 0.75、SJW 4，restart、
  bus error、error-warning、error-passive 与 bus-off 计数均为 0；
- 九个原始证据文件的 `SHA256SUMS.txt` 全部校验通过。

详细原始证据仅保存在 `.private/soak/p5_formal_8h_074_20260822T0830Z/`，公开结论不包含
设备序列号，也不把诊断固件 8 小时扩大为默认固件 8 小时。

### 默认固件恢复与 10 分钟回归

关闭全部 smoke/diagnostic 选项后构建并通过 Windows STM32CubeProgrammer v2.17.0 烧回默认
ELF，SHA-256 为 `45b4a2ed3ee64ade9be820c1bf917632e45e70ac32ff2d7759335bff83d02e1d`；
ELF 中不存在 `P5DIAG1`。会话 `p5_default_10m_074_20260822T1635` 的有效采集窗口通过：

- 10 轮 Modbus 均为 `PASS_READ_ONLY (10/10)`；
- VCP 恰有一次 BOOT、一次时钟摘要和五次 heartbeat，且无 `P5DIAG1`；
- 六类周期 CAN ID 各 609 帧，共 3654 帧，首末帧跨度约 `598.977 s`；
- CAN error 日志为空，结束时 `can0` 为 `ERROR-ACTIVE` 且错误计数为 0；
- 四个原始文件的 SHA-256 全部通过。

manifest 的宿主进程总历时为 1958 秒，不代表固件测试运行了 32 分钟。600 秒采集完成后，后台
`cat` 读取 VCP 时未响应脚本发出的 `SIGINT`，导致 systemd 收尾等待；只终止该遗留采集子进程后，
主脚本正常写入 manifest/SHA 并以 `ExecMainStatus=0` 退出。该清理异常发生在上述约 599 秒 CAN
窗口和 10 轮 Modbus 已完成之后，不改变本轮回归结论，但后续复用脚本时应把 VCP 子进程退出改为
有界 `SIGTERM`/超时回收。

因此 `SOAK-02` 可在“诊断固件正式 8 小时 PASS + 默认固件约 10 分钟回归 PASS”的限定下关闭；
历史 60 分钟预跑仍保持 `REVIEW_REQUIRED`，不单独宣称其通过，也不据此声明 MTBF、计量精度、
任意故障恢复或生产级可靠性。

## 阶段门

1. 10 分钟准入、正式 8 小时和默认固件回归已经完成；后续不得用历史失败/复评数据替换原始证据。
2. 正式 8 小时必须继续绑定干净提交与诊断 ELF SHA-256；默认固件回归使用独立 ELF SHA-256。
3. 证据必须明确区分“诊断固件 8 小时”和“默认固件 10 分钟”，不得互相替代。
4. 历史 60 分钟 `REVIEW_REQUIRED` 只支持异常复盘，不得改写成独立 `PASS`。
5. Git 提交、Tag、远程 Release 和求职材料发布仍需分别审核与授权。
