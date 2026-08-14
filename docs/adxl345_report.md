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
