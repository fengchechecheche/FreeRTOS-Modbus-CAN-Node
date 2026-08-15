# Recruitment claim ledger

> Ledger schema: `P5_RECRUITMENT_CLAIMS_V1`
> Source matrix: `artifacts/release/p5_s7_t03_evidence_matrix.json`
> Candidate baseline: `[039] e878e379ed499b51961eff12443869f1bb7f32f4`
> Publication: `NOT_PUBLISHED`
> Hardware claims: `INELIGIBLE_WHILE_NOT_RUN`

## Usage

These are bounded candidate sentences for later CV or portfolio review. They
are not automatically published. Evidence row IDs refer to the frozen
[`evidence_matrix.md`](evidence_matrix.md); every positive row below uses only
matrix results currently marked `PASS`.

## Positive candidate claims

| Claim ID | Target role | Candidate wording | Evidence rows | Qualification | Publication |
|---|---|---|---|---|---|
| CLM-EMB-01 | EMBEDDED | 面向 STM32F446RE 与 FreeRTOS 固件，完成 Host Debug/Release 各 21 项回归及 ARM Debug/Release 交叉构建与资源门。 | SW-01,FW-01,FW-02 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-EMB-02 | EMBEDDED | 实现 BSP 静态合同与 UART DMA、RS485 状态机、Modbus RTU/server 软件候选，并显式保留硬件验证边界。 | BSP-01,RS485-01 | SOFTWARE_CANDIDATE | NOT_PUBLISHED |
| CLM-IOT-01 | IOT | 实现 Modbus RTU 从站与经典 CAN 软件候选，分别通过 Host 和交叉构建合同验证。 | RS485-01,CAN-01 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-IOT-02 | IOT | 使用 SocketCAN/vcan 与 Host 双总线故障矩阵验证软件报文路径、有界背压和故障隔离，不声明实物总线联调。 | CAN-02,BUS-01 | HOST+VIRTUAL_BUS | NOT_PUBLISHED |
| CLM-ROB-01 | ROBOTICS_LOW_LEVEL | 完成 STM32 固件 Host 回归、ARM Debug/Release 交叉构建和静态资源门，并保留硬件时序验证边界。 | SW-01,FW-01,FW-02 | HOST+CROSS_BUILD | NOT_PUBLISHED |
| CLM-ROB-02 | ROBOTICS_LOW_LEVEL | 通过 CAN 固定队列与恢复软件合同及 Host 双总线故障矩阵验证有界背压和软件故障隔离。 | CAN-01,BUS-01 | SOFTWARE_CANDIDATE | NOT_PUBLISHED |

These sentences may be shortened for a specific job, but their evidence IDs,
qualification and exclusion boundaries must remain intact.

## Ineligible claims

| Restriction ID | Matrix rows | Boundary | Eligibility |
|---|---|---|---|
| LIM-HW-01 | BSP-02,SNS-01,SNS-02,SNS-03,WDG-01 | Board startup, physical sensors and watchdog behavior were not run. | NOT_ELIGIBLE |
| LIM-RS485-01 | RS485-02,RS485-03,P3-01 | PTY, USB-RS485 and Project Three interoperability were not run. | NOT_ELIGIBLE |
| LIM-CAN-01 | CAN-03,BUS-02 | Physical CAN and simultaneous physical RS485/CAN were not run. | NOT_ELIGIBLE |
| LIM-SOAK-01 | SOAK-02 | Hardware smoke, pre-run and formal soak were not run. | NOT_ELIGIBLE |
| LIM-REPRO-01 | REP-02 | Bit-for-bit BIN/HEX equality across differently named clean paths is not claimed. | NOT_ELIGIBLE |

## Wording rules

- Keep “Host”, “cross-build”, “software candidate” and “vcan” qualifiers.
- Do not replace “software fault matrix” with an unqualified physical dual-bus claim.
- Do not state Project Three gateway interoperability until `P3-01` passes.
- Do not publish sampling periods, recovery times or soak durations as measured results while their hardware rows are `NOT_RUN`.
- Do not use “industrial grade”, “production ready”, “functional safety” or equivalent language.
- Any actual CV, portfolio or recruitment-platform update requires a separate review and authorization.
