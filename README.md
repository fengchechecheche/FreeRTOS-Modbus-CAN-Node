# FreeRTOS Modbus CAN Node

基于 STM32F446RE 与 FreeRTOS 的双总线工业状态监测节点。本仓库当前已建立 P5-S1-T04 bare-metal HAL 最小固件候选；FreeRTOS、协议与传感器业务仍未实现。

## 当前边界

- 主机侧 smoke 只验证构建与测试基础设施，不代表 Modbus、CAN、传感器或 RTOS 功能完成。
- `freertos_modbus_can_node.ioc`、`Core/`、`Drivers/`、startup 和链接脚本由 STM32CubeMX 6.18.0 / STM32CubeF4 1.28.3 建立。
- 固件初始化 RCC、GPIO、SPI1、I2C2、USART1、USART2 与 CAN1；不启动通信、不访问传感器、不包含 FreeRTOS。
- 默认 Modbus 从站地址合同为 `4`；T04 不实现寄存器表。
- 许可证、版权行和公开范围为 `TBD_USER_REVIEW`，当前没有 `LICENSE`。
- 仓库远程名为 `FreeRTOS-Modbus-CAN-Node`。

## 主机验证

```bash
./tools/verify_host.sh
```

权威开发环境为 WSL2 `Ubuntu-24.04-STM32`。构建输出位于 `out/`，原始证据位于被忽略的 `.private/`。

## 固件构建

```bash
cmake --preset firmware-debug
cmake --build --preset firmware-debug
cmake --preset firmware-release
cmake --build --preset firmware-release
```

每个固件预设生成 ELF、HEX、BIN 与 MAP。交叉链接成功只证明构建链闭合，不代表硬件、接线、总线时序或协议功能已经验收。
