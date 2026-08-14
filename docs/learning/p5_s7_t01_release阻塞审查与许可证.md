# P5-S7-T01 Release 阻塞审查与许可证

> 教程状态：`FROZEN`（内容审核通过：2026-08-15）
> 执行路径：`SOFTWARE_CANDIDATE_RELEASE_AUDIT_PATH`
> 软件源码候选：`PASS_SOFTWARE_CANDIDATE`
> 硬件 Release：`BLOCKED_WAITING_FOR_HARDWARE`

## 1. 目标与边界

本任务不创建 Release，而是先回答“现在能公开什么、还缺什么、第三方代码按什么条款保留”。软件测试和许可证通过，只能让源码候选进入清洁复现；没有板卡、总线和长稳结果时，仍不能生成硬件 v0.1.0 结论。

## 2. 为什么需要两层发布门

源码候选主要检查软件正确性、许可证、第三方归属和公开隐私。硬件 Release 还要求烧录、三传感器、RS485、CAN、项目三互操作和正式长稳。把两层混成一个 PASS，会让硬件缺失阻断全部软件收尾，或反过来把交叉编译夸大为实物通过。

因此 blocker ledger 将 `LIC/PRIV/SW` 视为源码候选硬门，将 `REPRO` 交给 T02，将 `HW` 保持为硬件 Release blocker。

## 3. 项目许可证与第三方排除

用户明确选择项目自有代码采用 MIT，对外 copyright holder 为 `fengchechecheche`。根 `LICENSE` 只覆盖项目编写的应用、BSP 适配、协议、传感器驱动、配置、测试、工具、CMake 和文档。

它不覆盖：

- CubeMX 生成的 `Core/`；
- `Drivers/` 中 CMSIS 与 STM32 HAL；
- `Middlewares/Third_Party/` 中 FreeRTOS；
- startup、linker 或其他自带通知的文件。

第三方组件继续使用各自许可证，`THIRD_PARTY_NOTICES.md` 只做来源、版本、路径和条款索引，不重新授权这些代码。

## 4. 实际第三方映射

- CMSIS Core 5.9.0：Apache-2.0；
- STM32F4 CMSIS Device 2.6.11：Apache-2.0，并保留包级条款；
- STM32F4 HAL 1.8.5：BSD-3-Clause，并保留包级条款；
- FreeRTOS Kernel 10.3.1：MIT；
- ST FreeRTOS integration notes、CubeMX 生成文件和 linker script：保留文件头与适用包级条款。

仓库保留 STM32CubeF4 1.28.3 的完整 `Package_license.md` 副本，并校验固定 SHA-256。在线最新版本不能替代本项目实际使用的 1.28.3 原文。

官方 Markdown 自带行尾空格。这里优先保持原始字节与固定哈希，不为满足项目格式检查而改写第三方许可证；项目自有新增文件仍单独通过空白检查。

## 5. 传感器算法的来源边界

BME280、VEML7700 和 ADXL345 驱动依据厂商数据手册中的寄存器、比例和补偿公式编写。源码审查未发现厂商参考软件被直接放入 `sensors/`。因此当前实现属于项目自有代码，同时文档继续保留 Bosch、Vishay 和 Analog Devices 数据手册来源。

如果以后引入厂商参考源码，仅写“参考数据手册”不够，必须同时保存该源码许可证和归属。

## 6. Release-readiness 检查器

`tools/check_release_readiness.py` 只依赖 Python 标准库，检查：

- 根 LICENSE 的 holder、MIT 文本和第三方排除；
- NOTICE 的 CMSIS/HAL/FreeRTOS/生成代码覆盖；
- 包级许可证 SHA-256 与关键组件标记；
- blocker ID、类别、状态、阻塞层级和证据；
- 项目公开文件中的高置信 Windows、Linux、drive 和 WSL 绝对路径。

扫描不把 vendor 代码中的 `token`、`secret` 等普通 API 词汇直接当成泄密，也不扫描检查器自身的规则文本。`--self-test` 用有界 fixture 验证缺许可证、占位符、绝对路径、开放软件 blocker、重复 ID 和未知状态。

## 7. 实际问题与修复

审查发现 `docs/device_probe_report.md` 保存了 CubeMX 备份目录的完整 Windows 用户路径。这对问题复盘没有必要，还暴露本机用户名。修订后只记录“备份位于仓库外临时目录且可恢复”，不再公开具体路径。

许可证审查还发现 HAL/CMSIS 的组件文件要求结合包级 `Package_license`，而仓库原先只保留组件级许可证。T01 增加 STM32CubeF4 1.28.3 的完整官方包级许可证副本，避免依赖本机安装目录。

## 8. 当前结果和下一步

软件源码候选的 LIC、PRIV 和已知 SW blocker 已关闭。以下仍保持 OPEN：

- T02 清洁目录复现与哈希；
- 板卡和三传感器实测；
- USB-RS485 与项目三互操作；
- candleLight/CAN 和双总线实测；
- 10 分钟、60 分钟及正式 8 小时长稳。

因此下一步只能是 P5-S7-T02 清洁复现，不能直接 tag、发布或声明 v0.1.0 硬件 Release。
