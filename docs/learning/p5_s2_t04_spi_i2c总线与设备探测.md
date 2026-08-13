# P5-S2-T04｜SPI、I²C 总线与设备探测

> 内容状态：`FROZEN`（P5-S2-T04 内容审核通过）

## 1. 目标与边界

T04 建立 SPI1/I2C2 的有限寄存器访问和设备探测候选。软件已通过 host test 与 ARM Debug/Release
cross build；板卡和传感器硬件尚未验证，所以不能写成“三个传感器已通信”。

本任务不采集环境量或加速度，不写量程、采样率、补偿系数、interrupt、FIFO 或 power mode。完整
驱动留给 S4。

## 2. 为什么共享 SPI 改为 Mode 3

S1 最初把 SPI1 冻结为 Mode 0。T04 对照官方资料后发现：BME280 同时接受 Mode 0 和 Mode 3，而
ADXL345 的 4-wire SPI 要求 Mode 3。因此共同配置选择：

```text
CPOL = 1
CPHA = 1
MSB first
software NSS
2.8125 Mbit/s
```

这比每次访问前反复重配 SPI1 更简单，也避免 ADXL345 在错误时序下返回假 ID。`.ioc`、CubeMX 生成的
`spi.c` 和当前接口合同均已同步为 Mode 3。

## 3. 三种设备的最小探测

| 设备 | 总线 | 最小操作 | 成功含义 |
|---|---|---|---|
| BME280 | SPI1，PB6 CS | 读 `0xD0`，期望 `0x60` | silicon ID 候选正确 |
| ADXL345 | SPI1，PC7 CS | 读 `0x00`，期望 `0xE5` | silicon ID 候选正确 |
| VEML7700 | I2C2，7-bit `0x10` | ACK 后读 16-bit command `0x00` | 地址和寄存器访问存在 |

VEML7700 官方寄存器表没有独立 ID register。因此它的成功状态是 `PRESENT`，不能伪装成
`SILICON_ID_CONFIRMED`。

## 4. BSP 与纯逻辑分层

`bsp_spi_bus.c` 和 `bsp_i2c_bus.c` 是 HAL adapter：

- SPI adapter 生成 read command、控制 CS、调用 `HAL_SPI_TransmitReceive()`；
- I²C adapter 接受 7-bit address，在 HAL 调用处左移，并调用 ready/memory read；
- HAL status 映射为明确 result enum；
- timeout 候选统一为 20 ms。

`app_device_probe_logic.c` 不包含 HAL。它只通过注入的 transport port 读取 ID/寄存器、分类结果并执行
最多一次 recovery request，因此可在 host 上验证边界而不伪造 MCU register。

## 5. SPI 片选安全

BME280 与 ADXL345 共用 PA5/PA6/PA7，但片选独立。每次 transaction：

```text
两个 CS 拉高
  -> 只拉低目标 CS
  -> 发送 read command + dummy byte
  -> 读取返回 byte
  -> 无论 OK/BUSY/TIMEOUT/ERROR，两个 CS 都恢复 high
```

当前只有 bare-metal one-shot probe 调用总线，静态 busy flag 只防止重入；它不是 FreeRTOS mutex。S3/S4
引入多任务后必须重新冻结总线所有权。

## 6. I²C 地址为什么只保存 0x10

VEML7700 的合同使用 7-bit `0x10`。STM32 HAL 需要把地址放在包含 R/W 位的位置，所以只有 BSP HAL
boundary 执行 `address_7bit << 1`。应用、测试和报告不混用 `0x10`、write byte `0x20`、read byte
`0x21`，从而避免常见的双重左移错误。

## 7. 有界失败与 recovery

probe core 区分：

- `NOT_PRESENT`：NACK，或 SPI 返回常见开路值 `0x00/0xff`；
- `WRONG_ID`：总线有响应但 ID 不匹配；
- `TIMEOUT` / `BUS_ERROR`：transport 明确失败；
- `RECOVERY_REQUIRED`：需要恢复，但当前 adapter 没有经硬件验证的恢复动作。

首次 timeout/bus error 最多请求一次 recovery。只有 recovery hook 返回成功才允许第二次访问；第二次
失败立即停止。当前无硬件 adapter 明确返回“不支持自动恢复”，没有虚构 SCL pulse 或 HAL reinit。

## 8. 默认关闭的一次性 smoke

`P5_DEVICE_PROBE_SMOKE=OFF` 是最终默认值。默认启动不会访问缺失的传感器，也不会周期扫描总线。

台架构建临时设为 `ON` 时，启动只探测一次并通过 USART2 输出一行最终摘要，例如：

```text
P5 S2 T04 BME=60/OK ADXL=E5/OK VEML=10/PRESENT
```

它不保存逐笔 transaction，也不会无限重试。

## 9. Host test 证明什么

Debug/Release 均为 4/4 CTest 通过。`p5.host.device_probe` 覆盖：

1. 两个正确 ID 和 VEML7700 presence；
2. 错误 ID；
3. `0x00/0xff` 与 I²C NACK；
4. timeout/bus error；
5. 一次 recovery 后成功；
6. recovery 不可用；
7. 第二次失败有界停止；
8. 单设备失败不覆盖其他设备状态。

mock 不能证明真实 CS 电平、SPI edge、I²C pull-up 或模块供电正确。

## 10. ARM 构建

默认 probe OFF：

| 配置 | text | data | bss |
|---|---:|---:|---:|
| Debug | 13412 | 148 | 2548 |
| Release | 11696 | 144 | 2544 |

临时 probe-on Debug 也完成链接，随后恢复 OFF。交叉构建只证明代码能进入 STM32F446RE firmware，不
证明任何设备已经应答。

## 11. 到货后的宽松验收

所有接线都在断电状态完成，先逐个模块再组合：

1. BME280 读到一次 `0x60`；
2. ADXL345 读到一次 `0xE5`；
3. VEML7700 的 SDA/SCL idle high，`0x10` ACK 且 register read 成功；
4. 两个 SPI 模块同时接入后各读一次 ID，未出现交叉选择；
5. 断电移除 VEML7700后，probe 有界退出；断电恢复接线后可再次探测。

不要求几十次重复、逻辑分析仪波形、精确 pull-up 阻值或完整原始日志。只有失败时才增加诊断证据。

## 12. 本轮问题与处理

CubeMX 在 Mode 3 生成时附带 1193 个无关 CMSIS/CMake/syscalls 文件。生成前工作区已确认干净，所有
无关项均移到仓库外可恢复备份，候选只保留 `.ioc` 和 `spi.c` 的必要差异。

本轮没有需要跨会话排查的未解决故障，因此不创建 diagnostic JSON。

## 13. 当前状态

```text
software = PASS_HOST + PASS_CROSS_BUILD
device_probe_smoke_default = OFF
bme280_hardware = WAITING_FOR_HARDWARE
adxl345_hardware = WAITING_FOR_HARDWARE
veml7700_hardware = WAITING_FOR_HARDWARE
shared_spi_hardware = WAITING_FOR_HARDWARE
```

T04 当前为 `CONTENT_FROZEN + READY_FOR_HARDWARE`。软件候选已冻结；硬件到货后再按设备独立补验，
不自动进入 P5-S2-T05。
