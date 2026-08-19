# P5-S4-T01 BME280 SPI 采集与补偿报告

> 状态：`CONTENT_FROZEN + READY_FOR_HARDWARE`
> 内容审核：2026-08-14 已通过
> 软件：`PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`  
> 硬件：`WAITING_FOR_HARDWARE`

## 参考与结论边界

寄存器和补偿行为依据 Bosch BME280 数据手册 Revision 1.24 与官方
BME280 SensorAPI v3.5.1。项目采用自有最小实现，没有复制或 vendoring
SensorAPI 源码。固定 Host 向量是算法回归输入，不是实物校准或环境测量。

当前只确认纯逻辑、构建和静态合同。BME280 ID、SPI 波形、采集值、噪声、
功耗和原厂精度均未进行硬件验证。

## 配置与数据合同

| 项目 | 候选值 |
|---|---|
| SPI | SPI1、Mode 3、MSB first、BME280 CS=PB6 |
| 模式 | forced，1 Hz |
| 过采样 | temperature/pressure/humidity 均为 x1 |
| IIR | off |
| transaction timeout | 5 ms |
| reset/NVM deadline | 100 ms |
| conversion deadline | 40 ms |

完整 sample 同时保存 20-bit temperature/pressure raw、16-bit humidity raw
以及 `0.01 °C`、`Pa`、`0.001 %RH` 整数工程量。温度、气压和湿度分别钳位
到 Bosch 整数 API 的有效输出范围；固件未引入 `float`/`double` 运行时。

校准解析覆盖 `dig_T1..T3`、`dig_P1..P9`、`dig_H1..H6`，包括 H4/H5 的
跨 nibble 12-bit 符号扩展。只有全零、全 `0xFF`、T1=0、P1=0 或参数无效
才拒绝校准，不构造无官方依据的 trim 正常区间。

## 状态、错误和恢复

20 ms 的 `acquisition_task` release 每次只推进一个状态，最多发起一笔
SPI transaction；无 delay、busy poll 或完整初始化紧循环。初始化顺序为
ID -> soft reset -> NVM ready -> 两段 calibration -> `ctrl_hum` -> filter，
采样顺序为 forced start -> measurement ready -> 8-byte burst -> compensation。

状态可区分 wrong ID、BMP280 `0x58`、invalid calibration、BUSY、timeout、
I/O error、NVM timeout、measurement timeout、recovery required 和 offline。
瞬态错误每个 episode 最多一次 soft-reset recovery；再次失败进入 offline，
不停止其他任务，也不改变 watchdog feed decision。

## 软件验证

| 检查 | Debug | Release |
|---|---:|---:|
| Host CTest | 10/10 PASS | 10/10 PASS |
| ARM configure/build/link | PASS | PASS |
| Flash | 30676 B / 5.85% | 26352 B / 5.03% |
| Static RAM | 9664 B / 7.37% | 9656 B / 7.37% |

三组冻结向量、H4/H5 正负拼接、校准拒绝、ID 分类、写寄存器顺序、tick
wrap、NVM/measurement timeout、BUSY/timeout、一次恢复成功及重复失败均通过。
每次 service 的 mock bus 调用数不超过 1。静态负向自检可拒绝 sensor HAL/
delay、无限循环、第二 SPI runtime owner 和 BME mutex。

ARM ELF 包含 `bme280_compensate`、`bme280_service` 与
`app_bme280_service`，未发现补偿相关 `__aeabi_d*`/`__aeabi_f*` helper。
相对 `[018]`，Debug 增加 5324 B Flash/216 B RAM，Release 增加
4288 B Flash/216 B RAM；五个 task stack、queue depth 和 linker heap=0 不变。
既有 newlib nosys warning 未变化。

## 硬件补验

到货且 S2 最小启动通过后，只需：确认 ID=`0x60`；以 1 Hz 获取至少 5 个
sequence 递增的 valid sample；选 1 个 raw/calibration 在 Host 侧复算；执行
一次 soft-reset/re-init；读取一次 acquisition stack watermark 和错误计数。
正常通过不要求计量仪器比对或大证据包。

```text
bme280_identity = NOT_RUN
bme280_measurement = NOT_RUN
bme280_compensation_consistency = NOT_RUN
bme280_accuracy_calibration = NOT_CLAIMED
hardware = WAITING_FOR_HARDWARE
```

## P5-HW-SNS-00 身份探针补验（2026-08-19）

在提交 `d48f2c75b048dc60320045233312ad6df050e88e` 和 probe ELF SHA-256
`d011125dfbf8b96702da03d8c808476687639610d3b59acb7f23b1f42b6f8c4d`
下，BME280 单模块连续 3 次复位均返回 `0x60`；最终三模块拓扑连续 3 次复位
也均返回 `BME=60/OK`。因此只把实物身份和最终共享 SPI 拓扑下的单寄存器读取
记为通过。

```text
bme280_identity_probe = PASS
bme280_continuous_sampling = NOT_RUN
bme280_compensation_consistency = NOT_RUN
bme280_accuracy_calibration = NOT_CLAIMED
hardware = PARTIAL_HARDWARE_EVIDENCE
```

本次没有读取 5 组连续样本、复算补偿值或执行复位后重新初始化，因此 `SNS-01`
仍为 `NOT_RUN`，`HW-001` 仍保持 `OPEN`。

## P5-HW-SNS-01 连续采样与复位补验（2026-08-19）

在提交 `285a894d52ff817bf9683a5a7034c01c27675e89` 和默认 Debug ELF SHA-256
`d6940bed9ee6d5d8fd7ceab3299a82ee9f85b6914c710627fd34eef6e582f3ca` 下，通过 ST-LINK
HOTPLUG 只读统一测量快照 `app_rtos_measurement_snapshot`，不依赖当前故障中的 RS485 链路。

一组连续 5 次读取结果为：

| sequence | state | value present | quality | 温度 | 压力 | 湿度 |
|---:|---|---:|---:|---:|---:|---:|
| 126 | FRESH | 1 | `0x00000000` | 25.98 °C | 100446 Pa | 39.821 %RH |
| 128 | FRESH | 1 | `0x00000000` | 25.99 °C | 100449 Pa | 39.951 %RH |
| 129 | FRESH | 1 | `0x00000000` | 25.99 °C | 100460 Pa | 39.975 %RH |
| 130 | FRESH | 1 | `0x00000000` | 25.98 °C | 100448 Pa | 39.962 %RH |
| 131 | FRESH | 1 | `0x00000000` | 25.99 °C | 100449 Pa | 39.916 %RH |

异步读取跨过一次采样周期时允许 sequence 跳过一个值；本次只要求严格递增，不以 ST-LINK 轮询间隔
声明精确采样周期。复位约 3.5 秒后重新读取，得到 sequence 4、FRESH、value present 1、quality 0，
温度 25.94 °C、压力 100455 Pa、湿度 40.100 %RH，证明驱动能在复位后重新初始化并继续采集。

```text
bme280_identity_probe = PASS
bme280_continuous_sampling = PASS
bme280_reset_reinitialize = PASS
bme280_compensation_path = PASS_LIMITED
bme280_accuracy_calibration = NOT_CLAIMED
hardware = PASS_HARDWARE_LIMITED
```

因此 `SNS-01` 提升为 `PASS`。该结论只覆盖当前模块身份、SPI 采集、应用补偿输出、序列推进和复位
恢复；没有使用独立仪表复核温湿压，不声明计量精度、独立校准或环境范围性能。
