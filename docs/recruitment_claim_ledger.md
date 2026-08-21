# Recruitment claim ledger

> Ledger schema: `P5_RECRUITMENT_CLAIMS_V1`
> Source matrix: `artifacts/release/p5_s7_t03_evidence_matrix.json`
> Candidate software-test baseline: `[042] d7428e62a2df72325020ed63ab7979f4fb8c12f9`
> Publication: `NOT_PUBLISHED`
> Hardware claims: `BOUNDED_CLAIMS_ONLY / NOT_PUBLISHED`

## Usage

These are bounded candidate sentences for later CV or portfolio review. They
are not automatically published. Evidence row IDs refer to the frozen
[`evidence_matrix.md`](evidence_matrix.md); every positive row below uses only
matrix results currently marked `PASS`.

## Positive candidate claims

| Claim ID | Target role | Candidate wording | Evidence rows | Qualification | Publication |
|---|---|---|---|---|---|
| CLM-EMB-01 | EMBEDDED | 面向 STM32F446RE 与 FreeRTOS 固件，完成 Host Debug/Release 各 22 项回归及 ARM Debug/Release 交叉构建与资源门。 | SW-01,RS485-02,FW-01,FW-02 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-EMB-02 | EMBEDDED | 实现 BSP 静态合同与 UART DMA、RS485 状态机、Modbus RTU/server 软件候选，并通过 Host PTY 验证 10 个有界交互用例。 | BSP-01,RS485-01,RS485-02 | SOFTWARE_CANDIDATE | NOT_PUBLISHED |
| CLM-IOT-01 | IOT | 实现 Modbus RTU 从站与经典 CAN 软件候选，Modbus 生产 C 模块通过 Host PTY 交互，CAN 通过 Host 与交叉构建合同验证。 | RS485-01,RS485-02,CAN-01 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-IOT-02 | IOT | 使用 SocketCAN/vcan 与 Host 双总线故障矩阵验证软件报文路径、有界背压和故障隔离，不声明实物总线联调。 | CAN-02,BUS-01 | HOST+VIRTUAL_BUS | NOT_PUBLISHED |
| CLM-IOT-03 | IOT | 在 Ubuntu-24.04-Gateway x86_64 上通过参考 CH340 读取真实 STM32 地址 4 数据，完成只读 JSONL、一次复位恢复及约 60 秒本地 MQTT 投影。 | RS485-03,P3-01 | BOUNDED_HARDWARE_INTEGRATION | NOT_PUBLISHED |
| CLM-ROB-01 | ROBOTICS_LOW_LEVEL | 完成 STM32 固件 Host 回归、ARM Debug/Release 交叉构建和静态资源门，并保留硬件时序验证边界。 | SW-01,FW-01,FW-02 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-ROB-02 | ROBOTICS_LOW_LEVEL | 通过 CAN 固定队列与恢复软件合同及 Host 双总线故障矩阵验证有界背压和软件故障隔离。 | CAN-01,BUS-01 | SOFTWARE_CANDIDATE | NOT_PUBLISHED |

These sentences may be shortened for a specific job, but their evidence IDs,
qualification and exclusion boundaries must remain intact.

## Ineligible claims

| Restriction ID | Matrix rows | Boundary | Eligibility |
|---|---|---|---|
| LIM-HW-01 | BSP-02,SNS-01,SNS-02,SNS-03,WDG-01 | Board startup, bounded watchdog and three sensor routes passed; metrology, physical ADXL345 INT and full hardware release remain outside the claims. | NOT_ELIGIBLE |
| LIM-RS485-01 | RS485-03,P3-01 | Fixed-address-4 physical USB-RS485 and bounded Project Three read-only interoperability passed; valid address migration, Raspberry Pi hardware, production MQTT and repeated-fault endurance were not run. | NOT_ELIGIBLE |
| LIM-SOAK-01 | SOAK-02 | Hardware smoke, pre-run and formal soak were not run. | NOT_ELIGIBLE |
| LIM-REPRO-01 | REP-02 | Bit-for-bit BIN/HEX equality across differently named clean paths is not claimed. | NOT_ELIGIBLE |

## Wording rules

- Keep “Host”, “cross-build”, “software candidate” and “vcan” qualifiers.
- Keep the PTY claim qualified as Host software interaction, not physical RS485.
- Keep any physical RS485 claim bound to the reference CH340 route and fixed address 4. Project Three interoperability may be stated only with the Ubuntu-24.04-Gateway x86_64, read-only JSONL/RESET/local-MQTT and no-address-write qualifiers.
- Do not replace “software fault matrix” with an unqualified physical dual-bus claim; any physical claim must cite the single bounded `BUS-02` route and its exclusions.
- Do not extend `P3-01` to Raspberry Pi hardware, multiple real slaves, production MQTT, address writes or long-run reliability.
- Do not publish sampling periods, recovery times or soak durations as measured results while their hardware rows are `NOT_RUN`.
- Do not use “industrial grade”, “production ready”, “functional safety” or equivalent language.
- Any actual CV, portfolio or recruitment-platform update requires a separate review and authorization.
