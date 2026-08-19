# P5-S4-T03 ADXL345 中断采样与振动特征报告

> Software: `PASS_HOST + PASS_CROSS_BUILD + PASS_EXTI_CONTRACT`  
> Content: `FROZEN` (approved 2026-08-14)
> Hardware: `WAITING_FOR_HARDWARE`  
> Identity / DATA_READY / axis direction / vibration response: `NOT_RUN`

## 配置与边界

候选配置来自 ADXL345 Data Sheet Rev. G 与 AN-1077：

| 项目 | 候选值 |
|---|---|
| identity | `DEVID(0x00)=0xE5` |
| output rate | `BW_RATE=0x0A`，100 Hz normal power |
| format | `DATA_FORMAT=0x09`，full-resolution、±4 g、4-wire SPI |
| measurement | `POWER_CTL=0x08` |
| interrupt | DATA_READY → INT1；PB4/EXTI4 rising，pull-down，priority 6/0 |
| FIFO | bypass；本任务不启用 FIFO/watermark |
| SPI | Mode 3；`DATAX0..DATAZ1` 六字节命令为 `0xF2` |

priority 6 满足
`configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5` 的 FreeRTOS
`FromISR` 边界。ISR 只核对 pin、饱和计数和通知，不访问 SPI。

这些配置、Host mock 和 ARM 链接都不是实物证据。模块 ID、INT 极性、
100 Hz 时序、轴方向、SPI 波形和量程仍需到货后补验。

## 驱动与调度

pure driver 不依赖 HAL、FreeRTOS、delay、allocator、mutex 或 logging。
初始化按“一次 `service` 最多一笔 transaction”推进：

```text
ID -> standby -> interrupt off -> FIFO bypass -> format -> rate -> INT1 map
   -> readback -> measure -> power readback -> DATA_READY enable
```

`vTaskNotifyGiveFromISR()` 把 DATA_READY 作为计数通知交给
`acquisition_task`。一次通知批次最多读取一帧；当 count 大于 1 时只把
`count-1` 记为丢样下界，不循环补读或伪造连续样本。任务随后重新检查原有
20 ms 绝对释放，继续推进 BME280、VEML7700 和 ADXL stall deadline。

transport error、config mismatch 或 100 ms stall 每个 episode 最多触发一次
reconfigure；再次失败进入 offline。wrong ID 直接停止，不写入测量配置。

## 数据与特征

六字节按 little-endian 二补码解析：

```text
X = DATAX1 << 8 | DATAX0
Y = DATAY1 << 8 | DATAY0
Z = DATAZ1 << 8 | DATAZ0
```

每轴先减可注入的 signed `bias_lsb`，再按 full-resolution 名义比例计算：

```text
millig = round(corrected_counts * 1000 / 256)
```

默认 bias 为 0，但设置 `BIAS_UNCALIBRATED`，不冒充零偏、比例或安装方向校准。

100 个有效样本只累计 `sum`、`sum_square`、`min`、`max` 和质量计数。
对每轴：

```text
mean = sum / N
rms = sqrt(N * sum_square - sum * sum) / N
peak = max(abs(min - mean), abs(max - mean))
resultant_rms = sqrt(rms_x^2 + rms_y^2 + rms_z^2)
```

平方根为固定上界整数算法。窗口不保存 raw history，不实现 FFT、频谱、报警阈值、
轴承特征或正常/故障分类；特征只表示趋势。

## 软件验证

| 检查 | 结果 |
|---|---|
| Host Debug | 12/12 PASS |
| Host Release | 12/12 PASS |
| BSP contract | 382 stable facts PASS |
| negative contract self-test | ADXL priority/recovery/loop/MB/callback mutants rejected |
| Firmware Debug | PASS，`text/data/bss = 38024/160/10032` B |
| Firmware Release | PASS，`text/data/bss = 31960/156/10032` B |

Host 覆盖 `0`、`-1`、±256、int16 边界、混合轴、非零 bias、99/100/101
窗口、常量/交替/step/resultant、count 0/1/3、read error、wrong ID、BUSY、
timeout、I/O、config mismatch、99/100 ms stall、tick wrap 和一次恢复。

相对 `[020]`：

| Build | Flash 增量 | Linked RAM 增量 |
|---|---:|---:|
| Debug | +3340 B | +400 B |
| Release | +2552 B | +404 B |

五个 256-word application stack、queue depth 8、linker heap 0 均不变。
ELF/MAP 是忽略的构建产物；正常验收不生成 raw trace 或证据包。

## 到货后的宽松补验

1. 读取一次 `DEVID=0xE5`；
2. 静止放置时确认三轴数据可更新且数值有界；
3. 轻敲或翻转模块，确认对应轴与 RMS/peak 趋势发生变化；
4. 观察 IRQ/sample/drop/error 计数，无持续 storm 或 stall；
5. 记录一次 task watermark 与紧凑摘要。

若方向、比例、频响或 FIFO 不便验证，可保持 `NOT_RUN`，不阻塞软件内容审核。

## P5-HW-SNS-00 身份探针补验（2026-08-19）

在提交 `d48f2c75b048dc60320045233312ad6df050e88e` 和 probe ELF SHA-256
`d011125dfbf8b96702da03d8c808476687639610d3b59acb7f23b1f42b6f8c4d`
下，最终 BME280、ADXL345、VEML7700 三模块拓扑连续 3 次复位均返回
`ADXL=E5/OK`。该结果足以通过最终拓扑下的单寄存器身份准入，但不能扩展为
ADXL345 单模块 SPI 鲁棒性结论。

有界排查显示：完全移除 BME280 或隔离其 SCK 分支后，ADXL345 会返回非
`0xE5` 值；隔离 BME280 MISO 或 MOSI 时仍可返回 `0xE5`。更换短直连 SCK、
把 SPI 时钟从约 2.81 MHz 降到约 703 kHz/352 kHz，以及让 ADXL345 先探测，
均未修复单模块现象。商家提供的原理图与实际 PCB 不一致，且无法提供准确版本，
所以不依据该图推断实物上的电阻、VS 或 INT1 连接，也不把问题归因为已证实的
固件时序错误。

### 有界排查矩阵

| 接线/程序条件 | 重复结果 | 有界结论 |
|---|---|---|
| BME280、ADXL345、VEML7700 完整接入 | `ADXL=E5/OK`，连续 3 次 | 最终三模块拓扑下的单寄存器身份准入通过 |
| 完全移除 BME280 | `8E/C7` 等非 `0xE5` 值 | ADXL345 独立接线的 SPI 身份读取不稳定 |
| 仅断开 BME280 MISO/SDO | `ADXL=E5/OK`，连续 3 次 | 不支持“BME280 MISO 主动驱动导致冲突”的假设 |
| 仅断开 BME280 MOSI/SDI | `ADXL=E5/OK`，连续 3 次 | 不支持“必须保留 BME280 MOSI 支路”的假设 |
| 仅断开 BME280 SCK | `ADXL=97/WRONG_ID`，连续 3 次 | 故障与 BME280 SCK 支路的存在强相关，但尚不能确定电气机理 |
| ADXL345 SCK 改用新的短直连线 | `ADXL=DE/WRONG_ID`，连续 3 次 | 不能把问题简单归因于原 SCK 跳线接触不良 |
| SPI 分频由 `/32` 改为 `/128`、`/256` | 仍为 `DD/DE` 等错误 ID | 不能把问题简单归因于 2.81 MHz 时钟过高 |
| 调整为 ADXL345 优先探测 | 仍为 `DE/WRONG_ID` | 不能把问题简单归因于 BME280 先访问或片选顺序 |

这些结果只缩小排查范围，不证明 BME280 SCK 支路提供了正确的上拉、下拉、
负载、电平整形或回流路径。缺少实际 PCB 原理图、示波器/逻辑分析仪波形和
万用表静态测量时，根因继续保持未知，不据此修改 SPI 驱动或硬件接口。

### 原因假设（未证实）

以下方向按与当前现象的吻合程度排列，仅用于后续聚焦排查：

1. 实际 ADXL345 模块 PCB 可能存在未知的电阻、保护器件、电平转换、焊接桥或
   丝印差异；商家提供的原理图与实物版本不一致，无法据此确认具体网络；
2. BME280 的 SCK 输入支路可能通过输入电容、板载阻抗或电平转换网络增加负载，
   从而改变上升沿/下降沿、过冲、振铃或阈值附近抖动；
3. 面包板节点、接触状态或信号回流路径可能随 BME280 SCK 支路接入而改变；
4. ADXL345 模块的 SCK 输入、焊接或器件状态可能处于边缘工作区，附加负载后才
   能稳定识别时钟。

SPI 分频降低只会延长时钟周期，不会直接降低 STM32 GPIO 的边沿速度，所以降频
失败不能单独排除信号完整性问题。另一方面，当前也没有波形证据证明存在过冲、
振铃或阈值抖动。后续若继续定位，优先级为：更换来源可靠的 ADXL345 模块；断电
执行连续性和静态阻值检查；使用示波器或逻辑分析仪对比 BME280 SCK 支路接入前后
MCU 端与 ADXL345 端波形。不得在模块未供电时仅接入 SCK，以免经输入保护结构
反向供电。

在获得上述证据前，这些方向均保持 `HYPOTHESIS`，不用于宣称根因，不触发
CubeMX、SPI 模式、引脚、驱动接口或协议架构修改。

```text
adxl345_identity_final_topology = PASS
adxl345_standalone_spi_robustness = NOT_CLAIMED
adxl345_data_ready_irq = NOT_RUN
adxl345_axis_response = NOT_RUN
adxl345_vibration_features = NOT_RUN
hardware = PARTIAL_HARDWARE_EVIDENCE
```

因此 `SNS-03` 仍为 `NOT_RUN`，`HW-001` 仍保持 `OPEN`。后续保持当前 SPI
架构，先推进 BME280、VEML7700、RS485 和 CAN；ADXL345 I²C 仅作为以后
单独限时评估的备选，不在本次补验中修改 CubeMX、引脚或驱动接口。
