# bsp

STM32F446RE 板级适配边界。T04 为 SPI1/I2C2 增加有界寄存器访问，统一管理 CS 与 7-bit address；
不实现完整设备驱动、总线 DMA、RTOS mutex、CAN filter 或中断队列。
