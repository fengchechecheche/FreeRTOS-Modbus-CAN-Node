# 项目五正式冻结记录

> 状态：`PROJECT_FROZEN / MAINTENANCE_ONLY`
> 冻结日期：2026-08-22
> 功能基线：`[078] 705a988af8cc9043756ec9aebcbd8b7885eb69e2`
> 预期版本：`v0.1.0`
> 发布状态：`UNRELEASED / NOT_PUBLISHED`

## 1. 冻结结论

项目五“FreeRTOS Modbus CAN Node”在当前有界目标内完成开发、硬件补验、项目三互操作、正式
8 小时长稳测试、默认固件回归和证据归一化，现正式转入维护状态。后续项目展示、简历和面试材料
只允许从已审核证据中投影，不再通过追加功能扩大项目本体。

冻结不是远程发布授权，也不是量产、计量、功能安全或无限故障恢复声明。当前没有创建 Git tag、
远程 Release 或二进制附件。

## 2. 冻结依据

| 项目 | 冻结状态 |
|---|---|
| 源码与文档基线 | `[078] 705a988af8cc9043756ec9aebcbd8b7885eb69e2` |
| 证据矩阵 | `24 = 23 PASS + 0 FAIL + 0 NOT_RUN + 1 NOT_CLAIMED` |
| Release blocker | `LIC/PRIV/SW/REPRO/HW` 有界 blocker 全部关闭 |
| 硬件范围 | NUCLEO-F446RE、三传感器、参考 CH340 RS485、准入 candleLight CAN、双总线与项目三联调 |
| 长稳范围 | `[074]` 诊断固件正式 8 小时通过；恢复的默认固件独立约 10 分钟回归通过 |
| 默认回归固件 | ELF SHA-256 `45b4a2ed3ee64ade9be820c1bf917632e45e70ac32ff2d7759335bff83d02e1d` |
| 灾难恢复 | Windows 端已保存 WSL 私有证据、完整 Linux 项目归档和 Git bundle；该备份保持未跟踪、私有 |

权威结论分别位于：

- [`evidence_matrix.md`](evidence_matrix.md)：逐项证据状态和声明上限；
- [`release_readiness.md`](release_readiness.md)：Release 门和 blocker；
- [`soak_trend_report.md`](soak_trend_report.md)：8 小时诊断固件与默认固件回归边界；
- [`recruitment_claim_ledger.md`](recruitment_claim_ledger.md)：可用于求职材料的有界措辞；
- [`demo_guide.md`](demo_guide.md)：项目展示和复现入口。

## 3. 冻结后禁止的默认变更

没有单独解冻决策时，不再执行：

- 新功能、任务、协议字段、Modbus 寄存器或 CAN ID 扩展；
- CubeMX 重新生成、引脚/时钟调整、任务数量或静态内存策略改变；
- 为扩大简历表述而补造测试、改写历史证据或提升声明等级；
- 重新开启已经明确排除的 Host-active CAN、ADXL345 物理中断、地址写入或计量精度路线；
- tag、远程 Release、固件附件或公开发布。

## 4. 冻结后允许的维护

以下工作可以在用户明确授权后，以独立变更和完整回归方式进行：

- 严重缺陷、安全问题、许可证问题或证据错误修订；
- 编译器、依赖或开发环境升级导致的兼容性维护；
- 项目三继续使用冻结的地址 4、`19200 8E1` Modbus 只读路线进行联调复现；
- README、项目展示、简历和面试材料的证据约束型投影；
- 备份完整性检查和灾难恢复演练。

任何源码、CubeMX、协议或硬件声明变化都必须先记录解冻原因、允许文件清单和回归范围。完成后重新
冻结，并形成新的基线提交；不得静默修改当前冻结结论。

## 5. 保留限制

冻结后仍保留以下边界：

- 跨不同路径构建的 BIN/HEX 位级一致性为唯一 `NOT_CLAIMED`；
- 不声明传感器独立计量校准、ADXL345 INT1/INT2、精确采样率或独立模块 SPI 鲁棒性；
- 不声明有效地址迁移 H08/H09、生产 MQTT、多从站现场部署或重复 RS485 故障耐久；
- 不声明任意 CAN 适配器兼容、Host-active 生产命令、重复物理 BUS-OFF 恢复或任意断线时长；
- 不声明 MTBF、量产可靠性或功能安全。

历史 60 分钟预跑的 `REVIEW_REQUIRED` 仍作为历史事实保留。正式长稳结论仅由诊断固件 8 小时
测试和恢复默认固件后的独立约 10 分钟回归共同支撑。

## 6. 后续交接

项目本体至此停止主动开发。后续工作转入项目展示、简历材料和面试复盘时，应优先引用
`recruitment_claim_ledger.md`，并保持证据矩阵中的限定语。若展示需要截图、曲线或精简架构图，
只生成公开安全的投影，不把 `.private/` 原始日志、设备身份、网络信息或本地路径直接发布。
