# 项目五真实问题台账

> Ledger status：`READY_FOR_CONTENT_REVIEW`
> Baseline：`[038] a8950b5d506d4b02b65c72aa1ec4d7fc6b85da9b`
> Selection rule：只保留实际发生、对学习或复现有价值且具有公开证据的问题；最多 12 条。

本台账不是硬件缺口表。板卡、传感器、PTY、项目三、物理 RS485/CAN 和正式长稳未执行的事实见
[`../evidence_matrix.md`](../evidence_matrix.md)，不得在这里标为 `FIXED`。

| ID | Task / phase | Symptom | Wrong assumption | Root cause | Fix or disposition | Regression evidence | Prevention | Status |
|---|---|---|---|---|---|---|---|---|
| PRB-01 | P5-S1-T05 | 初版要求每次运行生成多类文件和完整环境信息 | 排障、全过程审计和项目展示必须使用同一证据系统 | 没有按实际消费场景分层 | 改为成功摘要 + 按需诊断记录，展示材料延后提取 | `docs/learning/p5_s1_t05_验收协议与问题排查.md` | 先回答证据由谁、何时消费，再新增字段 | FIXED |
| PRB-02 | P5-S2-T03 / S7-T02 | Ubuntu 把脚本 shebang 解释为 `sh\r` | Windows 文本可直接作为 Linux 可执行脚本 | CRLF 进入需要 LF 的 shebang；早期只修复必需入口，版本采集脚本仍保留该限制 | 两个 Shell 入口统一为 LF；`.gitattributes` 固定 `*.sh eol=lf`；Host 入口先执行 LF-only 检查 | `docs/rs485_smoke_report.md`<br>`docs/reproduction_report.md` | 新增或修改 Shell 脚本必须同时通过 Git 属性、LF-only 检查和语法检查 | FIXED |
| PRB-03 | P5-S2/S3/S4 CubeMX tasks | 小范围配置生成附带大量 CMSIS/CMake/syscalls 等超范围文件 | CubeMX 只会改动所选外设相关文件 | 生成器按项目模板刷新广泛输出 | 生成前要求 clean；恢复广泛生成，只按批准 allowlist 保留必要差异 | `docs/device_probe_report.md`<br>`docs/learning/p5_s3_t03_中断dma与任务通知.md` | 每次生成先冻结允许清单并保留可恢复边界 | MITIGATED |
| PRB-04 | P5-S3-T01/T03、S6-T02 | priority group 或 CAN IRQ 显示为 0，后续 `FromISR` 会越过 FreeRTOS 门 | CubeMX 默认 NVIC 设置可直接用于 FreeRTOS API | priority 0 是最高逻辑优先级，且初始 priority group 不满足项目合同 | 固定 `NVIC_PRIORITYGROUP_4` 和批准 IRQ 6/0，并用负向 mutant 检查 | `docs/interrupt_notification_report.md`<br>`docs/learning/p5_s6_t02_bxcan过滤器中断与发送队列.md` | 所有调用 `FromISR` 的 IRQ 同时审查 group、数值和 API | FIXED |
| PRB-05 | P5-S2-T02 | UART 持续失败时有限 heartbeat 会每秒无限重试 | 只在发送成功后计数等价于限制尝试次数 | 成功计数与尝试预算混为一个状态 | 无论成功失败都增加尝试计数，最多五次后停止 | `docs/learning/p5_s2_t02_系统时钟gpio与时间基准.md` | 对失败路径单独验证循环上界 | FIXED |
| PRB-06 | P5-S3-T02 | RAM 预算首次把 MSP/heap-stack reservation 重复相加 | GNU `size` 的 bss 不含 NOLOAD reservation | `._user_heap_stack` 已计入 bss，但又在总量外追加 | 总量使用 data+bss；MSP 仅在明细表单列 | `docs/learning/p5_s3_t02_静态内存栈与资源预算.md` | 资源脚本先冻结工具输出语义再写公式 | FIXED |
| PRB-07 | P5-S4-T01/T02 | Host Release 中标准 `assert` 被移除，测试变量未使用且断言失效 | Debug 测试写法在 Release 会保持相同行为 | `NDEBUG` 改变了测试程序语义 | 测试入口在包含 `assert.h` 前撤销 `NDEBUG`，随后 Debug/Release 回归通过 | `docs/learning/p5_s4_t01_bme280_spi采集与补偿算法.md`<br>`docs/learning/p5_s4_t02_veml7700_i2c光照采集.md` | 测试断言机制不得依赖生产优化宏 | FIXED |
| PRB-08 | P5-S4-T04/T05 | transient wait 导致 freshness 抖动；终态 OFFLINE 丢失原始错误原因 | 当前驱动状态可以同时表示年龄和根因 | freshness、terminal state 与 last error 是不同维度 | freshness 使用 last-good age；quality 保留 error/recovery；monitor 同时读取 last_error_status | `docs/learning/p5_s4_t04_统一采样质量新鲜度与时间戳.md`<br>`docs/learning/p5_s4_t05_多传感器调度与故障注入.md` | 数据模型分开时间、质量、终态和诊断根因 | FIXED |
| PRB-09 | P5-S6-T02 | CAN Host 目标接入遥测投影后首次构建缺传感器 include | 统一 measurement 头不会向目标传播额外依赖 | 头文件传递包含三个传感器驱动类型 | 只给该目标补充已有 include 目录并定向回归 | `docs/learning/p5_s6_t02_bxcan过滤器中断与发送队列.md` | 新目标审查传递头依赖而非复制类型 | FIXED |
| PRB-10 | P5-S6-T05 | collector 返回 0 但可能提前退出，仍会被误判为完成计划时长 | 进程成功退出等于时长门通过 | 退出码只说明进程状态，不证明时间覆盖 | 增加 `last elapsed >= duration - sample period` 硬门和负向自测 | `docs/soak_trend_report.md`<br>`docs/learning/p5_s6_t05_长稳测试与实时资源趋势.md` | 运行时验收同时验证退出码、样本跨度和终止原因 | FIXED |
| PRB-11 | P5-S7-T02 | 两个不同绝对目录的 Release BIN/HEX 不同 | clean procedure PASS 应产生跨目录逐字节一致产物 | FreeRTOS assert 字符串保留绝对 `__FILE__` 路径 | 保留可复现构建流程和候选哈希，但明确不声明跨目录 bit-for-bit | `docs/reproduction_report.md`<br>`docs/evidence_matrix.md` | 发布表述分开 procedure reproducibility 与 byte reproducibility | ACCEPTED_LIMITATION |
| PRB-12 | P5-S7-T01 | 公开报告含本机用户路径；组件许可证不足以独立覆盖包级条款 | 被忽略路径和组件 LICENSE 已足以保证公开安全/归属完整 | 公开扫描与 package-level notice 范围不完整 | 删除非必要本机身份，纳入 STM32CubeF4 官方包级许可证并增加 Release 检查 | `docs/learning/p5_s7_t01_release阻塞审查与许可证.md`<br>`docs/release_readiness.md` | 发布前同时检查公开路径和包级许可来源 | FIXED |

## 使用规则

- 问题已经修复也不删除原始现象和错误假设；
- 同类事件合并为一条，例如多次 CubeMX 超范围生成；
- 新问题只有在影响跨会话排障、回归或公开结论时才进入台账；
- 普通命令拼写错误、一次性临时文件和没有工程影响的工具交互不扩展为长期记录；
- 未来硬件失败应先使用轻量 diagnostic record，确认有长期价值后再提炼到此处。
