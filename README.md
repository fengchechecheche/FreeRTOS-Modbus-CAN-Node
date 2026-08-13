# FreeRTOS Modbus CAN Node

基于 STM32F446RE 与 FreeRTOS 的双总线工业状态监测节点。本仓库当前处于 P5-S1-T03：只建立独立仓库、主机 CMake/CTest 骨架和 Cortex-M4F 工具链入口。

## 当前边界

- 主机侧 smoke 只验证构建与测试基础设施，不代表 Modbus、CAN、传感器或 RTOS 功能完成。
- 固件预设只有工具链入口；真实 CubeMX、HAL/CMSIS、FreeRTOS、startup 和链接脚本从 P5-S1-T04 开始。
- 默认 Modbus 从站地址合同为 `4`；T03 不实现寄存器表。
- 许可证、版权行和公开范围为 `TBD_USER_REVIEW`，当前没有 `LICENSE`。
- 未来远程仓库名预留为 `FreeRTOS-Modbus-CAN-Node`；当前没有 Git remote。

## 主机验证

```bash
./tools/verify_host.sh
```

权威开发环境为 WSL2 `Ubuntu-24.04-STM32`。构建输出位于 `out/`，原始证据位于被忽略的 `.private/`。
