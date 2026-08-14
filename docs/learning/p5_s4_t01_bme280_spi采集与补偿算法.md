# P5-S4-T01 BME280 SPI 采集与补偿算法

## 目标与边界

本任务把 BME280 从“可探测的 SPI 设备”推进为可测试的采集驱动：读取校准、
启动 forced measurement、读取原始值并输出整数温度、气压和湿度。开发板和
传感器尚未到货，所以教程中的 PASS 都是 Host/ARM 软件结论。

## 关键设计与实施

驱动拆成三层：`sensors/bme280` 只处理寄存器、算法和状态；`app_bme280`
把通用 read/write 操作映射到 BSP；`acquisition_task` 每 20 ms 调用一次
service。这样纯驱动可在 PC 上注入 mock bus，固件仍保持 SPI1 单 owner。

BME280 在 `0x88..0xA1` 与 `0xE1..0xE7` 保存 production trim。普通 16-bit
字段按 little-endian 解析；H4/H5 共用一个字节的两个 nibble，因此先拼成
12-bit 再显式符号扩展。全零、全 `0xFF`、T1=0 或 P1=0 才拒绝，不用经验
阈值误伤少见但合法的器件系数。

补偿顺序必须是 temperature -> `t_fine` -> pressure/humidity。固件使用
32/64-bit 整数，并输出 centi-°C、Pa 和 milli-%RH；三项 raw 同时保留以便
出问题时复算。三组固定向量覆盖代表值和两端钳位，但不冒充实物测量。

forced mode 配置为 T/P/H x1、filter off、1 Hz。`ctrl_hum` 要先写，之后写
`ctrl_meas` 才让湿度配置生效。一次 20 ms release 最多做一笔 SPI 交易，
reset 和 conversion 都靠 wrap-safe deadline 分多次推进，任务中没有 sleep
或 busy polling。

瞬态 BUSY/timeout/I/O error 每个 episode 只尝试一次 soft reset；成功后记录
recovery，第二次失败转 offline。wrong ID、BMP280 ID 和 invalid calibration
直接分类，不用无休止 reset 掩盖硬件或型号问题。

## 验证结果

- Host Debug/Release：均为 10/10；
- 三组补偿向量、H4/H5 符号、ID/校准错误、时钟回绕和恢复边界：PASS；
- ARM Debug/Release：构建链接 PASS；
- Flash：30676 B / 26352 B；Static RAM：9664 B / 9656 B；
- 静态合同：无 HAL/delay/heap/无限循环/第二 SPI owner，负向自检 PASS；
- BME280 实物身份与采集：`NOT_RUN`。

## 实际问题与修复

第一次冻结湿度期望值时，手工换算结果与 Bosch 整数公式不一致。逐项核对
`t_fine`、H4/H5 和 Q22.10 单位后，把期望值修正为官方公式对应的
75270 milli-%RH，没有通过放宽容差掩盖差异。

Host Release 首次构建又暴露标准 `assert` 在 `NDEBUG` 下会被移除，测试文件
随即出现未使用变量并失去断言。测试入口在包含 `assert.h` 前撤销 `NDEBUG`，
保持 Release 优化配置的同时确保测试断言仍执行；重新配置后 10/10 通过。

## 限制与下一步

当前 snapshot 仍是 BME280 owner-local 结构，不定义跨传感器 age/fresh/stale
语义；这些统一字段留给 P5-S4-T04。板卡到货后按紧凑步骤补验 ID、5 个样本、
一次 Host 复算、一次恢复和一次 stack watermark。P5-S4-T02 不在本任务自动开始。
