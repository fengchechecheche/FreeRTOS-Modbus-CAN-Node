# app

项目自有 FreeRTOS 任务、状态机和应用编排边界。P5-S3-T01 使用原生 FreeRTOS API 创建五个静态
task skeleton，并把 RS485 poll 与有限 heartbeat 从裸机 idle loop 迁入对应任务。T04 的 one-shot
probe 与全部 smoke 仍默认关闭；本阶段不实现完整传感器、Modbus 或 CAN 业务。
