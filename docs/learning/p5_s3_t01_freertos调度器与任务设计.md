# P5-S3-T01 FreeRTOS 调度器与任务设计

> 内容状态：`FROZEN`（P5-S3-T01 内容审核于 2026-08-14 通过）

## 目标与边界

本任务把 S2 的裸机主循环迁移到 FreeRTOS 调度骨架，但不提前实现传感器、Modbus 或 CAN 业务。
硬件未到货，因此只闭合来源、任务模型、Host 测试和 ARM 交叉构建，不声称调度器已经在板上运行。

## 关键设计与实施

内核固定为 STM32CubeF4 V1.28.3 随包 FreeRTOS V10.3.1，使用原生 API 和 GCC ARM_CM4F port。
HAL 的 1 ms tick 由 TIM6 提供，SysTick、PendSV 和 SVC 交给 FreeRTOS。任务全部通过
`xTaskCreateStatic()` 创建，动态分配、heap、software timer、tickless idle 和 runtime stats 均关闭。
NVIC 从不适合 RTOS BASEPRI 模型的 `PRIORITYGROUP_0` 修订为 `PRIORITYGROUP_4`，4 个实现位全部
作为抢占优先级；现有外设 IRQ 数值不变。

任务优先级从 protocol 5、acquisition 4、CAN 3、health 2 到 diagnostic 1。周期任务采用绝对
release；迟到时记录 miss 并跳到下一个未来 release，不忙等、不连续补跑历史周期。RS485 poll
迁入 protocol task，有限 heartbeat 迁入 diagnostic task。

## 验证结果

HAL/FreeRTOS-free task model 覆盖合同合法性、正常 release、跨 tick wrap、迟唤醒、单/多周期过载
和任务状态独立性。Host Debug/Release 与 Firmware Debug/Release 的最终结果见
`docs/scheduler_report.md`。

## 实际问题与修复

CubeMX 在 A1 除 TIM6 timebase 外生成了大量 CMSIS/DSP/NN/RTOS 与额外 CMake 文件；这些文件已移到
仓库外可恢复备份，只保留必需差异。A2 编辑时 Windows 无法直接写入 WSL UNC 路径，因此使用可写
临时镜像生成补丁，再机械同步到 Ubuntu 权威仓库并通过构建验证，没有转而修改 Windows 项目副本。
最终审查还发现 CubeMX 原配置为 `NVIC_PRIORITYGROUP_0`；静态构建无法发现该运行时风险，因此同步
修订 `.ioc`、MSP 与 BSP 检查器，避免上板后以错误优先级分组运行 FreeRTOS。

## 限制与下一步

五个任务的 256-word stack 只是 provisional buffer；真实 watermark、总 RAM、jitter、WCET 和 ISR
latency 均未测。P5-S3-T02 才负责静态内存、栈与资源预算；硬件到货后补做有限 scheduler smoke。
