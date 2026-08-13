# P5-S2-T05｜BSP 冻结与板级故障排查

> 内容状态：`FROZEN`（P5-S2-T05 内容审核通过）

## 1. 目标与结论边界

T05 的目标是让后续 FreeRTOS、传感器、Modbus 和 CAN 软件知道可以稳定依赖哪些板级事实，而不是在
没有板卡时宣称 BSP 已完成实测。

因此本任务把冻结拆成两层：

- `BSP_CONTRACT_CANDIDATE_FROZEN`：仓库内配置、代码、接口和软件回归一致；
- `BSP_CONTRACT_HARDWARE_FROZEN`：在前者基础上，实物 pinmap、跳线和最小访问也一致。

当前只达到第一层，硬件仍为 `WAITING_FOR_HARDWARE`。

## 2. 为什么现在可以冻结软件候选

T01～T04 已经分别建立启动、时钟/GPIO、UART DMA/RS485 和 SPI/I²C 探测候选，并通过 Host 与 ARM
构建。继续等待所有硬件才整理合同，会让 S3～S6 无法区分“稳定板级事实”和“尚未设计的业务”。

候选冻结允许软件轨道前进，同时通过明确状态防止把编译成功写成实物通过。

## 3. 四源一致性

T05 比较四类来源：

```text
freertos_modbus_can_node.ioc
  <-> Core 生成初始化代码
  <-> bsp 公开接口与 app 消费位置
  <-> T01～T04 已审核验证记录
```

检查重点是 pin、label、安全初值、clock、DMA/IRQ、总线参数、初始化顺序和默认 smoke。CubeMX 字段
排序、注释或无语义差异不作为冻结失败条件。

## 4. 冻结什么，不冻结什么

冻结的稳定事实包括：

- SWD、USART1/2、SPI1、I2C2、CAN1 和 GPIO 的 pinmap；
- HSI/PLL 与 180/180/45/90 MHz 候选；
- RS485 DMA stream/channel、USART1 IRQ 和 DE low；
- SPI Mode 3、两个 CS high；
- I²C 100 kHz 与应用层 7-bit address；
- CubeMX 拥有初始化，BSP 借用句柄。

不冻结完整传感器驱动、RTOS mutex、Modbus 映射、CAN filter/message ID 或未来经审核的 API 增量。冻结
是变更控制，不是禁止项目继续演进。

## 5. 窄静态检查

`tools/verify_bsp_contract.py` 使用 Python 标准库，对当前仓库检查 107 项稳定事实。正常成功只输出一行
摘要；失败时指出来源文件、事实名称和缺失的期望值。

它还提供内存负例：把 device-probe 默认值临时视为 `ON`，确认检查器能够拒绝危险默认值。负例不修改
工作区，也不保存 fixture。

这个脚本证明的是仓库内一致性，不证明电压、波形、模块 ID、I²C 上拉或实际接线。

## 6. Safe-state 为什么重要

上电时 PA8/RS485_DE 必须 low，避免收发器意外占用总线；PB6/BME280_CS 和 PC7/ADXL345_CS 必须
high，避免两个 SPI 从设备同时驱动 MISO。

这些初值同时存在于 `.ioc`、生成 GPIO 初始化和候选合同中。后续 CubeMX 再生成后，静态检查可以尽早
发现它们是否被意外改变。

## 7. 软件验证结果

- 静态合同：107 项 PASS；代表性负例正确失败；
- Host Debug：4/4 PASS；
- Host Release：4/4 PASS；
- ARM Debug：PASS，`text/data/bss = 13412/148/2548`；
- ARM Release：PASS，`11696/144/2544`；
- RS485 loopback 与 device probe smoke：Debug/Release 均为 `OFF`；
- `.ioc` SHA-256 未变化。

这些结果只支持 `PASS_HOST + PASS_CROSS_BUILD`。

## 8. 到货后的宽松硬件冻结

按“裸板、Shield、单设备、组合”的顺序补验：

1. ST-LINK/VCP、烧录、reset 和启动标记；
2. runtime clock profile、有限 heartbeat 和 GPIO safe-state；
3. USART1 loopback、RS485 固定字节和 DE；
4. BME280/ADXL345 单设备 ID、共享 SPI 和独立 CS；
5. VEML7700 `0x10` ACK、register presence 和 SDA/SCL idle high；
6. CAN 留到 S6 合同明确后做收发器与对端联调。

部分硬件到货时只更新对应接口，其他项继续等待。正常通过不要求大量照片、全程串口日志或逻辑分析仪
波形。

## 9. 板级故障排查顺序

遇到问题时一次只改变一个变量：

```text
供电、USB 线、跳线和固件版本
  -> 裸板启动与 VCP
  -> 单一 UART/SPI/I²C 接口
  -> Shield 或单设备
  -> 多设备组合
```

- ST-LINK 失败先查数据型 USB 线、供电/跳线、枚举、目标电压和 reset；
- VCP/时钟异常先查 COM 参数、固件版本、启动错误和 runtime profile；
- RS485 先做 TTL loopback，再查 DE/RE、A/B、GND、termination 和 19200 8E1；
- SPI 先单设备核对 Mode 3、供电、pin 和目标 CS，再组合；
- I²C 先看 SDA/SCL idle high、7-bit `0x10`、NACK/timeout/stuck-low 分类。

只有问题需要跨会话复现或修复回归时，才保存错误摘要、接线照片、字节日志、波形或 diagnostic JSON。

## 10. 本轮问题与处理

四源核对没有发现 pin、参数或初始化冲突。S1 早期接口合同未列出 T03/T04 后续新增 API；T05 新建当前
权威合同，并在历史文件中增加指向说明，没有把旧文档重写成事后状态。

既有 newlib `nosys` warning 未变化且不影响链接，因此不创建诊断记录。

## 11. 当前状态

```text
P5-S2-T05 = CONTENT_FROZEN
bsp_contract = BSP_CONTRACT_CANDIDATE_FROZEN
software = PASS_HOST + PASS_CROSS_BUILD
bsp_contract_hardware_frozen = NOT_GRANTED
hardware = WAITING_FOR_HARDWARE
```

T05 无硬件第一步内容已冻结；本次不自动开始 P5-S3-T01。
