# FreeRTOS Modbus CAN Node

基于 STM32F446RE 与 FreeRTOS 的双总线工业状态监测节点。P5-S2-T05 已把 T01～T04 的无硬件结果
汇总为 BSP 软件候选合同；当前状态为 `BSP_CONTRACT_CANDIDATE_FROZEN + WAITING_FOR_HARDWARE`。
这允许后续纯软件轨道继续，但不代表板卡、Shield 或外设已经实测。FreeRTOS、Modbus 协议、CAN
业务与完整传感器驱动仍未实现。

## 当前边界

- 主机侧测试验证构建基础设施、32 位毫秒时间逻辑以及 RS485 DE/DMA 调用顺序，不代表硬件通信完成。
- `freertos_modbus_can_node.ioc`、`Core/`、`Drivers/`、startup 和链接脚本由 STM32CubeMX 6.18.0 /
  STM32CubeF4 1.28.3 建立。
- USART1 为 PA9/PA10、19200、8E1；RX 使用 DMA2 Stream2/Channel4，TX 使用 DMA2 Stream7/Channel4，
  PA8/RS485_DE 上电为低。
- 默认 smoke 不主动发包；只有收到 ASCII `P5T03` 才异步返回 `P5T03OK`。它不是 Modbus 帧。
- DE 只在最终 USART `TC` 对应的 `HAL_UART_TxCpltCallback()` 后拉低；start failure、timeout 和 UART
  error 均有有界恢复路径。
- 候选时钟为 HSI 16 MHz、SYSCLK/HCLK 180 MHz、PCLK1 45 MHz、PCLK2 90 MHz；HAL tick quantum 为
  1 ms，尚未板级实测。
- USART2 保留 T01 启动标记、T02 clock 摘要和最多五次 1 秒 heartbeat；当前只通过交叉构建。
- SPI1 使用 PA5/PA6/PA7、Mode 3、MSB first、software NSS、2.8125 Mbit/s；BME280/PB6 与
  ADXL345/PC7 片选独立，transaction timeout 候选为 20 ms。
- I2C2 使用 PB10/PB3、100 kHz；应用层只保存 VEML7700 7-bit address `0x10`，仅在 HAL boundary
  左移地址。
- 最小探测读取 BME280 `0xD0`/`0x60` 与 ADXL345 `0x00`/`0xE5`；VEML7700 只验证 address/register
  presence，不声称 silicon ID。
- `P5_DEVICE_PROBE_SMOKE` 默认关闭；临时启用时只探测一次并通过 USART2 输出紧凑摘要，设备缺失
  不阻塞正常启动。
- SPI/I²C 候选不使用 DMA、RTOS 或动态内存；timeout/bus error 最多请求一次 recovery，当前 HAL
  adapter 不伪造未实测的 SCL pulse 或重新初始化。
- PA5 保留 SPI1 SCK，不作为 LD2 heartbeat；两个 SPI CS 初值高。
- NUCLEO-F446RE 尚未到货，ST-LINK、VCP、UART loopback、RS485 physical layer 和全部板级接口均保持
  `WAITING_FOR_HARDWARE`。
- 默认 Modbus slave address contract 为 `4`；当前不实现 register table、function code 或 CRC。
- license、copyright line 和 public scope 为 `TBD_USER_REVIEW`，当前没有 `LICENSE`。
- repository remote name 为 `FreeRTOS-Modbus-CAN-Node`。

当前权威 BSP 软件候选见 [`docs/bsp_contract.md`](docs/bsp_contract.md)。改变已冻结 pin、clock、bus、
DMA/IRQ 或 safe-state 时，必须同步合同、配置和相关回归；后续 API 仍允许经审核做增量扩展。

## 主机验证

```bash
./tools/verify_host.sh
```

权威开发环境为 WSL2 `Ubuntu-24.04-STM32`。该入口运行 host Debug/Release 的 smoke、clock、RS485
和 device-probe CTest。构建输出位于 `out/`，问题排查证据只在需要时写入被忽略的 `.private/`。

BSP 静态合同检查：

```bash
python3 tools/verify_bsp_contract.py
```

## 固件构建

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug
cmake --preset firmware-release
cmake --build --preset firmware-release
```

每个 firmware preset 生成 ELF、HEX、BIN 与 MAP。交叉链接成功只证明构建链闭合，不代表 hardware、
wiring、bus timing 或 protocol 已验收。

到货后的无 Shield PA9/PA10 loopback 可临时在 configure 时设置 `P5_RS485_LOOPBACK_SMOKE=ON`；该模式只
允许用于有限三次本地探针，不得接入外部 RS485 bus。

T04 台架探测可临时配置：

```bash
cmake --preset firmware-debug -DP5_DEVICE_PROBE_SMOKE=ON
cmake --build --preset firmware-debug
```

验证后必须恢复默认 `OFF`；该模式不是周期扫描或完整传感器驱动。
