# P5-S4-T02 VEML7700 I²C 光照采集报告

> 状态：`CONTENT_FROZEN + READY_FOR_HARDWARE`
> 内容审核：2026-08-14 已通过
> 软件：`PASS_HOST + PASS_CROSS_BUILD + PASS_STATIC_CONTRACT`
> 硬件：`WAITING_FOR_HARDWARE`

## 参考与结论边界

寄存器、字节序、增益、积分时间和分辨率依据 Vishay VEML7700 数据手册
Revision 1.8（Document 84286）与 2025-03-06 应用说明（Document 84323）。
当前 Host 向量只验证纯驱动逻辑；没有实测 `0x10` ACK、I²C 波形、照度响应、
噪声、量程切换、功耗或原厂精度。

## 配置、量程与数据合同

I²C2 保持 100 kHz，驱动使用 7-bit address `0x10`、configuration register
`0x00` 和 ALS register `0x04`。16-bit word 按 low byte first 读写。默认配置为
gain x1/8、integration time 100 ms；PSM、threshold、interrupt 与 WHITE channel
不启用。

9 级量程从 x1/8、25 ms 的 `2.1504 lx/count`，逐级过渡到 x2、800 ms 的
`0.0042 lx/count`。默认 level 2。raw `<=100` 时每个样本最多提升一级灵敏度，
raw `>=10000` 时最多降低一级；每个 ranging episode 最多调整 8 次。到达端点后
发布 `RANGE_LIMITED` 或 `SATURATED`，不形成无限重试。

工程量以整数 millilux 保存，同时保留 raw、gain、integration time、range level、
sequence 和 quality flags。超过约 1000 lx 时标记 `HIGH_LUX_UNCORRECTED`；本任务
不强行应用与具体光学结构有关的高照度多项式修正。

## 调度、错误与恢复

`acquisition_task` 每 20 ms 依次推进 BME280 和 VEML7700，一次 VEML service
最多发起一笔 5 ms-timeout I²C transaction。配置写入、回读、积分等待、ALS 读取
分散到多个 release；没有 delay、busy poll、动态分配或周期日志。稳定样本周期为
1 Hz。BME 与 VEML 单个 release 的候选上界是最多一笔 SPI 加一笔 I²C；真实 WCET
和总线仲裁仍待 P5-S4-T05 硬件审查。

状态保留 NACK/not-present、BUSY、timeout、I/O error、configuration mismatch、
saturation、range limit、recovery unavailable 和 offline。瞬态错误每个 episode
最多请求一次恢复；当前 BSP adapter 返回 unavailable，不伪造 SCL pulse、SDA
release 或 peripheral re-init 已经成功。snapshot 仍为 acquisition owner-local；
跨传感器 freshness/last-good 由 P5-S4-T04 统一定义。

## 软件验证

| 检查 | Debug | Release |
|---|---:|---:|
| Host CTest | 11/11 PASS | 11/11 PASS |
| ARM configure/build/link | PASS | PASS |
| Flash | 34844 B / 6.65% | 29564 B / 5.64% |
| Static RAM | 9792 B / 7.47% | 9784 B / 7.46% |

Host 覆盖 little-endian word、9 级配置、5 个冻结换算向量、100/10000 边界、
两端量程 progression、integration wait、tick wrap、NACK/BUSY/timeout/I/O、
config mismatch 与恢复分支；mock 断言每次 service 不超过一笔 transaction。
契约自检会拒绝 sensor HAL/delay、无限循环、第二 I²C runtime owner 和 adapter
mutex。ARM ELF 包含 VEML7700 driver/adapter/service，未发现 `__aeabi_d*` 或
`__aeabi_f*` helper。

相对 `[019]`，Debug 增加 4168 B Flash/128 B RAM，Release 增加 3212 B
Flash/128 B RAM。五个 task stack、queue depth 和 linker heap=0 不变；既有
newlib nosys warning 不变。

## 到货后的宽松补验

S2 最小启动通过后，只需确认 `0x10` ACK 且 config 可读写，取得至少 5 个 sequence
递增样本，遮挡与恢复照明时 raw/millilux 方向合理，并读取一次 range/error/recovery
计数与 acquisition stack watermark。保存至多一条 config/raw/value 摘要供问题复算。
不要求遍历全部 9 级量程、制造饱和、计量精度比对、拔线恢复或大证据包。

```text
veml7700_address_and_config = NOT_RUN
veml7700_basic_acquisition = NOT_RUN
veml7700_light_response_direction = NOT_RUN
veml7700_auto_range_hardware = NOT_RUN
veml7700_accuracy = NOT_CLAIMED
hardware = WAITING_FOR_HARDWARE
```

## P5-HW-SNS-00 地址/寄存器探针补验（2026-08-19）

在提交 `d48f2c75b048dc60320045233312ad6df050e88e` 和 probe ELF SHA-256
`d011125dfbf8b96702da03d8c808476687639610d3b59acb7f23b1f42b6f8c4d`
下，VEML7700 单模块连续 3 次复位均返回 `VEML=10/PRESENT`；BME280 与
VEML7700 组合以及最终三模块拓扑也各连续 3 次通过。该结果只证明 `0x10`
地址 ACK 和探针所需的配置寄存器访问，不把它表述为独立 silicon ID。

```text
veml7700_address_and_config_probe = PASS
veml7700_basic_acquisition = NOT_RUN
veml7700_light_response_direction = NOT_RUN
veml7700_auto_range_hardware = NOT_RUN
veml7700_accuracy = NOT_CLAIMED
hardware = PARTIAL_HARDWARE_EVIDENCE
```

本次没有取得连续样本，也没有执行遮挡/照明响应或量程检查，因此 `SNS-02`
仍为 `NOT_RUN`，`HW-001` 仍保持 `OPEN`。
