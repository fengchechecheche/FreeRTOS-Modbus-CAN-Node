# bsp

STM32F446RE 板级适配边界。T04 为 SPI1/I2C2 增加有界寄存器访问，统一管理 CS 与 7-bit address；
不实现完整设备驱动、总线 DMA、RTOS mutex、CAN filter 或中断队列。

P5-S2-T05 的权威软件候选合同位于 [`docs/bsp_contract.md`](../docs/bsp_contract.md)。冻结范围是 pin、
clock、bus、DMA/IRQ、safe-state 和当前所有权，不禁止 S3～S6 经审核增加 RTOS 仲裁、完整驱动、协议或
CAN 功能；改变稳定板级事实时必须同步合同和回归。
