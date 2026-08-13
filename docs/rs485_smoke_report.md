# P5-S2-T03 UART DMA 与 RS485 最小收发报告

> 内容状态：`FROZEN`（P5-S2-T03 内容审核通过）  
> 状态：`PASS_HOST + PASS_CROSS_BUILD + READY_FOR_HARDWARE`  
> 硬件状态：`WAITING_FOR_HARDWARE`  
> 实施基线：`80b064dde7db1548c5ecaa50db3e12322f1affab`（提交 `[ 007 ]`）  
> 验证环境：Ubuntu-24.04-STM32，用户 `stm32`

## 1. 本轮完成范围

- USART1 保持 PA9/PA10、19200、8E1；PA8 继续作为 RS485_DE，上电为低；
- RX 使用 DMA2 Stream2 / Channel4，TX 使用 DMA2 Stream7 / Channel4，均为 normal mode；
- 建立无 HAL 的方向、超时和错误恢复状态机；
- 建立 HAL DMA adapter、64-byte 静态 RX/TX 缓冲与 receive-to-idle 接收；
- 默认只在收到 ASCII `P5T03` 后异步返回 `P5T03OK`；
- 提供默认关闭的三次 PA9/PA10 本地回环编译选项；
- 未实现 Modbus、FreeRTOS、动态内存、ring buffer 或主动周期发包。

当前 `.ioc` SHA-256：

```text
fe41b370a02e61695f61647319529a6715b79ed42219666195ffdb9c5fb2e2ee
```

## 2. 关键时序结论

normal-mode `HAL_UART_Transmit_DMA()` 的 DMA complete 只表示最后一个 byte 已搬入 USART。当前 HAL 会在
内部 DMA complete 后开启 USART `TC` interrupt；只有 `USART1_IRQHandler()` 处理最终 `TC` 后，才调用
`HAL_UART_TxCpltCallback()`。

本实现只在该最终 callback 中把 PA8 拉低并回到接收态。启动失败、回绕安全超时和 UART error 也会把
PA8 恢复为低，并通过有界 recovery 重新挂接 RX DMA。

## 3. 主机测试

权威入口：

```bash
./tools/verify_host.sh
```

Debug 与 Release 均为 3/3 CTest 通过：

- `p5.host.smoke`；
- `p5.host.clock`；
- `p5.host.rs485`。

RS485 主机测试内部覆盖 8 类场景：初始化方向、DE/DMA 调用顺序、busy/长度拒绝、DMA 启动失败、最终
TX complete、跨 `UINT32_MAX` 超时、UART error recovery、RX rearm failure，以及固定请求匹配/不匹配。

host 结果只证明纯逻辑和调用顺序，不证明 DMA interrupt、UART 波形或 RS485 差分物理层。

## 4. 交叉构建

默认回环关闭配置：

| 配置 | text | data | bss | 结果 |
|---|---:|---:|---:|---|
| Debug | 13384 | 148 | 2524 | PASS |
| Release | 11672 | 144 | 2528 | PASS |

临时启用 `P5_RS485_LOOPBACK_SMOKE=ON` 的 Debug 也完成编译链接，随后已恢复为默认 `OFF`。最终 Debug ELF
包含 `USART1_IRQHandler`、两个 DMA2 stream handler、三个 HAL UART callback、RS485 状态机和 smoke
入口。链接器仍输出既有 `--specs=nosys.specs` 的 `_read/_write/_close/_lseek` warning；本任务不消费这些
syscall，warning 不影响链接成功。

## 5. 实施中发现的问题

1. CubeMX CMake generator 同时输出了 1188 个无关 CMSIS/CMake/syscalls 文件。它们未纳入仓库候选，
   已在 Windows 侧移到可恢复临时备份，只保留 DMA 必需生成物；
2. `tools/verify_host.sh` 的 CRLF 导致 WSL 把 shebang 识别为 `sh\r`。本轮只把该脚本机械转换为 LF，
   正文命令没有改动，修复后权威入口可直接运行。

这两项均已通过必要回归，不创建诊断 JSON。

## 6. 硬件补验门

NUCLEO-F446RE 尚未到货，以下均未运行：

- PA9/PA10 本地回环与三次固定探针；
- Shield 版本、跳线、DE/RE、终端和 A/B 核对；
- USB-RS485 短线共地的三次 `P5T03 -> P5T03OK`；
- 末字节、DE 电平和实际 UART 波形。

因此本轮状态为：

```text
software = PASS_HOST + PASS_CROSS_BUILD
uart_loopback_hardware = NOT_RUN
rs485_hardware = NOT_RUN
hardware = WAITING_FOR_HARDWARE
```

没有示波器或逻辑分析仪不阻塞后续补验。只有出现末字节截断、DE 卡高或偶发误码时，才增加波形、
原始 byte 或错误计数作为诊断证据。
