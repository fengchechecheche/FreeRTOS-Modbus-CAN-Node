# P5-S2-T04 SPI/I²C 设备探测报告

> 状态：`CONTENT_FROZEN + READY_FOR_HARDWARE`
> 内容审核：2026-08-14 已通过
> 软件：`PASS_HOST + PASS_CROSS_BUILD`
> 硬件：`WAITING_FOR_HARDWARE`
> 实施基线：`2b8ace847c14a41594b10f6ee15b59a399a08eb4`（提交 `[ 009 ]`）

## 完成内容

- SPI1 为 Mode 3、MSB first、software NSS、2.8125 Mbit/s；
- SPI BSP 统一管理 BME280/PB6 与 ADXL345/PC7 片选，任一返回路径恢复两个 CS high；
- I²C BSP 对外只接受 7-bit address，在 HAL boundary 将 `0x10` 左移；
- SPI/I²C transaction timeout 候选为 20 ms，不使用 DMA、RTOS 或动态内存；
- 纯 probe core 识别 BME280 `0x60`、ADXL345 `0xE5`，并把 VEML7700 记为 address/register
  presence，而不是 silicon ID；
- timeout/bus error 最多请求一次 recovery；当前 HAL adapter 不伪造未实测的自动恢复；
- one-shot option `P5_DEVICE_PROBE_SMOKE` 默认关闭，设备缺失不阻塞正常启动。

当前 `.ioc` SHA-256：

```text
9b94965ddc89e94ea52b553dc15fd9d4c355f7119c2274d4a1cea6ab43c46f4c
```

## 主机测试

`./tools/verify_host.sh` 的 Debug 与 Release 均为 4/4 CTest 通过：

- `p5.host.smoke`；
- `p5.host.clock`；
- `p5.host.rs485`；
- `p5.host.device_probe`。

新增测试覆盖正常 ID/presence、错误 ID、`0x00/0xff` 缺失、VEML7700 NACK、timeout/bus error、
一次 recovery 后成功、recovery 不可用、第二次失败停止，以及单个设备失败不覆盖其他设备结果。

## 交叉构建

| 配置 | text | data | bss | 结果 |
|---|---:|---:|---:|---|
| Debug，probe OFF | 13412 | 148 | 2548 | PASS |
| Release，probe OFF | 11696 | 144 | 2544 | PASS |
| Debug，probe ON | 21292 | 228 | 2884 | PASS_LINK_ONLY |

probe-on 只证明条件编译、HAL adapter、紧凑 USART2 摘要和链接闭环；随后 Debug/Release cache 均已
恢复为 `P5_DEVICE_PROBE_SMOKE=OFF`。既有 nosys syscall warning 不影响链接结果。

## 结论边界

host test 只证明纯 probe policy、retry bound 和设备独立记账；cross build 只证明 ARM build chain
closed。当前不能声称：

- BME280 或 ADXL345 已返回正确 ID；
- VEML7700 已在 `0x10` ACK；
- 两个 SPI CS 已在实物上无交叉选择；
- I²C 外部上拉、idle high 或断线恢复已通过；
- 任一温湿度、气压、光照或加速度测量有效。

## 本轮问题与处理

CubeMX 在 Mode 3 生成时附带 1193 个无关 CMSIS/CMake/syscalls 文件。它们未进入候选，已移动到
仓库外的临时备份目录。具体用户路径不进入公开报告；备份可恢复，仓库只保留 `.ioc` 与
`Core/Src/spi.c` 的 Mode 3 差异。该问题已闭环，不生成 diagnostic JSON。

## 未执行硬件项

```text
bme280_hardware = NOT_RUN
adxl345_hardware = NOT_RUN
veml7700_hardware = NOT_RUN
shared_spi_hardware = NOT_RUN
i2c_disconnect_recovery = NOT_RUN
hardware = WAITING_FOR_HARDWARE
```

无硬件路径内容已审核通过并冻结；本次不自动开始 P5-S2-T05。

## P5-S4-T01 交接

S2 的 one-shot probe 仍只负责 scheduler 前的 ID 准入，并保持默认关闭。
P5-S4-T01 在 `acquisition_task` 中新增独立的 BME280 forced-mode 驱动；既有
single-register probe 回归继续通过。Host mock 和 ARM 链接不改变本报告的
硬件结论，BME280 identity/measurement 仍为 `NOT_RUN`。

## P5-S4-T02 交接

S2 的 VEML7700 探针仍只证明兼容 address/register presence policy，并保持
默认关闭。P5-S4-T02 新增独立 ALS configuration/read、整数 millilux 与有限
auto-range 驱动；既有 one-shot probe 回归继续通过。Host mock 与 ARM 链接不把
`0x10` ACK、silicon identity、光照响应或总线恢复提升为硬件结论，这些仍为
`NOT_RUN`/`NOT_CLAIMED`。

## P5-S4-T03 交接

S2 的 ADXL345 one-shot probe 仍只负责 scheduler 前的 `0xE5` 身份准入，
并保持默认关闭。P5-S4-T03 新增独立的 100 Hz DATA_READY/EXTI4 驱动和六字节
XYZ coherent read；既有 single-register probe 路径不设置 multibyte bit，只有
ADXL345 长度大于 1 的 block read 才增加 D6。Host mock 与 ARM 链接不把实物
identity、INT1、轴方向、量程或振动响应提升为硬件结论。

## P5-HW-SNS-00 实板补验（2026-08-19）

本次以提交 `d48f2c75b048dc60320045233312ad6df050e88e` 为源代码基线，使用
`P5_DEVICE_PROBE_SMOKE=ON` 的原始 one-shot probe ELF。该 ELF 的 SHA-256 为：

```text
d011125dfbf8b96702da03d8c808476687639610d3b59acb7f23b1f42b6f8c4d
```

宽松准入结果如下：

- BME280 单模块连续 3 次复位均为 `BME=60/OK`；
- VEML7700 单模块连续 3 次复位均为 `VEML=10/PRESENT`；
- BME280 与 VEML7700 组合连续 3 次复位均通过；
- 最终三模块拓扑连续 3 次复位均为
  `BME=60/OK ADXL=E5/OK VEML=10/PRESENT`，每次均伴随 5 次 heartbeat。

ADXL345 的通过结论只适用于最终三模块拓扑。去除 BME280 或隔离其 SCK
分支时，ADXL345 返回了不稳定的非 `0xE5` 值；更换短直连 SCK、降低 SPI
时钟到约 703 kHz/352 kHz、调整探测顺序均未消除该现象。隔离 BME280 的
MISO 或 MOSI 时 ADXL345 仍可返回 `0xE5`。商家无法提供与实物 PCB 一致的
原理图，因此当前把该现象限定为模块/SCK 电气依赖或未知 PCB 限制，不归因于
已经证实的 SPI 协议或固件时序缺陷。

```text
bme280_identity_probe = PASS
veml7700_address_register_probe = PASS
adxl345_identity_final_topology = PASS
shared_spi_final_topology = PASS
adxl345_standalone_spi_robustness = NOT_CLAIMED
sensor_functional_sampling = NOT_RUN
hardware = PARTIAL_HARDWARE_EVIDENCE
```

临时诊断改动已恢复，原始 probe ELF 哈希已复核一致。本报告不保存设备序列号、
完整串口日志或逐次原始数据；上述结果不替代连续采样、INT1/DATA_READY、量程、
补偿一致性、响应趋势或计量精度验收。
