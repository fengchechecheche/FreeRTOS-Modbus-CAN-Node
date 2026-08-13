# P5-S2-T01 STM32 与 ST-LINK 最小启动

> 内容状态：`FROZEN`
> 软件状态：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`
> 执行路径：无硬件路径  
> 硬件状态：`WAITING_FOR_HARDWARE`

## 1. 目标与边界

本工作块先建立一个能交叉编译、便于板卡到货后直接烧录观察的最小启动候选。固件初始化既有
USART2 后，通过它输出短时启动标记，然后回到原有低功耗空闲路径。

NUCLEO-F446RE 尚未到货，因此本轮不执行到货验收、USB 枚举、ST-LINK 连接、烧录、VCP 观察或
三次冷启动。交叉编译通过只能证明软件候选可生成，不能证明串口输出或板级启动已经通过。

## 2. 关键设计与实施

启动逻辑只修改项目自有的 `app/src/app_boot.c`，不修改 CubeMX 生成的 `main.c`、`usart.c` 或
`.ioc`。`main.c` 原有顺序已经在 `MX_USART2_UART_Init()` 后调用 `app_boot_initialize()`，因此应用层
可以直接使用 `huart2`。

启动标记固定为：

```text
P5 S2 T01 BOOT OK
```

固件使用阻塞式 `HAL_UART_Transmit()` 发送五次，每次间隔约 1 秒。这样板卡到货后，即使 Windows
枚举虚拟串口和打开终端稍慢，也有机会看到后续标记。任一次发送失败都会返回 `APP_BOOT_ERROR`，由
现有 `main.c` 进入 `Error_Handler()`；全部完成后主循环继续调用 `app_boot_idle()` 中的 `__WFI()`。

USART2 沿用 CubeMX 候选配置：115200 bit/s、8 数据位、无校验、1 停止位、无流控。这里没有引入
`printf`、stdio 重定向、DMA、RTOS 或新的日志系统，也没有借用与 SPI1 SCK 冲突的 PA5/LD2。

## 3. 验证结果

- Ubuntu 权威仓库基线为 `[003] 第三次提交，冻结轻量验收与问题诊断合同`；
- host Debug 配置、构建和 CTest 通过，结果为 1/1；
- firmware Debug 与 Release 交叉编译、链接均通过，均生成 ELF、HEX、BIN 和 MAP；
- Debug HEX SHA-256：`d92ec7dccd921ea437cd7c0d0fe42b7e02e385fcd0dd58c5c3938714c2a6e9dc`；
- Release HEX SHA-256：`874abdabb6fdc8c4cfdfd07a46d07604bcb9987b8ee1abc47571985f53843238`；
- Debug 大小为 text 7908 / data 32 / bss 1928 / total 9868 bytes；
- Release 大小为 text 6900 / data 32 / bss 1928 / total 8860 bytes；
- 两个 ELF 均能静态找到启动标记，Debug ELF 中存在 `app_boot_initialize`、`app_boot_idle`、
  `HAL_UART_Transmit` 和 `huart2` 符号。

链接器仍提示 `_close/_lseek/_read/_write` 未实现。这是既有 `nosys` 合同警告；本实现直接调用 HAL，
没有依赖这些 syscall，因此不影响本轮链接通过，也不能用它推断串口已经实测。

## 4. 实际问题与修复

本轮没有发现源码或构建阻塞问题。实施前确认 Windows 工作副本仍停在 `[002]`，没有与 Ubuntu
`[003]` 对齐，因此遵守双环境边界，只修改 Ubuntu 权威仓库，不在 Windows 副本编辑源码。

通过 Windows UNC 直接修改 WSL 文件被权限边界拒绝，随后改为在 Ubuntu 内使用补丁工具应用同一份
最小补丁；修改结果通过 `git diff --check` 和交叉构建验证。该过程没有形成需要跨会话排查的工程
故障，因此不创建诊断 JSON。

## 5. 限制与下一步

当前状态是 `READY_FOR_HARDWARE + WAITING_FOR_HARDWARE`，不是 `PASS_HARDWARE`。板卡到货后需要补做：

1. 裸板型号、外观和数据型 Mini-B USB 线检查；
2. Windows 枚举 ST-LINK/VCP 并读取目标 MCU；
3. 对本轮 Debug HEX 执行下载、verify 和 reset；
4. 在 USART2 VCP 上观察启动标记；
5. 完成三次断电冷启动并记录一份轻量结论。

在补验前，T01 保持待硬件状态，但不阻塞 T02～T05 以及后续阶段的纯软件候选工作。
