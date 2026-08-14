# P5-S6-T03｜SocketCAN 与 candleLight 联调

## 目标与边界

本任务把 T01 的 CAN 报文合同和 T02 的 bxCAN runtime 接到 Linux SocketCAN 工具链，但将软件预检与实物
联调分开记录。无硬件阶段只允许获得 `PASS_HOST`；真实 ACK、收发器、终端、bit timing 和 bus-off 必须等
NUCLEO、收发器和 candleLight 到货后补验。

当前 revision 1 只有周期遥测和状态事件，没有请求、控制、配置写入或 echo 命令。因此联调工具不能为了
“双向”测试临时增加控制 ID，也不能把 host 成功调用 `send()` 解释为 MCU 已执行某项业务。

## 关键设计与实施

新增 `tools/can_hil_probe.py`，仅使用 Python 标准库：

- 从机器可读 message map 取得 7 个 ID、DLC、字段和范围；
- `--self-test` 和 `--dry-run` 不打开接口；
- 只有显式提供 `--interface` 才使用 SocketCAN；
- `--vcan-self-test` 还必须显式增加 `--allow-send`；
- 真实接口默认先被动观察，发送解锁后也只发一个固定白名单测试帧；
- 接收窗口最长 60 秒，单次最多保存 128 帧；
- 默认不写文件，只有指定 `--output-dir` 才保存有界摘要和帧片段。

解码器拒绝 extended、RTR、错误 DLC、未知 ID、未知 schema、保留位和已冻结范围外字段。周期 payload
重复是正常现象，因此重复帧只计数不判错。BME primary/secondary 必须 schema 和 sequence 一致才能配对。

## 验证结果

纯内存 self-test 通过 7 个正常向量，并拒绝 4 个代表性 mutant。dry-run 明确输出
`interface NOT_OPENED` 和 `hardware NOT_RUN`。

临时 `vcan0` 矩阵发送并捕获 12 帧：

- 8 帧被接受，包括 7 个报文类型和 1 个重复 heartbeat；
- unknown ID、wrong DLC、unknown schema、reserved flags 各拒绝 1 帧；
- 六个周期 ID 全部出现；
- BME 匹配 1 组、错配 0 组；
- 测试后删除本轮创建的 `vcan0`，没有遗留真实 CAN 接口。

既有 Host Debug/Release 均为 20/20，CAN/Modbus/BSP 合同和 ARM Debug/Release 也通过。没有修改固件，
资源与 T02 基线一致。

## 实际问题与处理

A0 时 WSL 已包含 `vcan` 和 `gs_usb`，但 `can-utils` 尚未安装，因此先保留环境跟进门。用户随后自行安装
`can-utils` 2023.03-1；本轮在临时 `vcan0` 上用 `cansend` 发送一帧 `0x240` heartbeat，并由 `candump`
成功捕获，随后删除接口。该结果闭合 CLI 工具链，但仍只是 `PASS_HOST`。

另一个容易误判的边界是 candleLight local TX echo。其上游说明 echo 可能在帧写入 CAN 外设时就返回，
所以 echo、`send()` 成功或 host TX packet 计数都不能单独证明远端 ACK。实物验收需要结合接口错误计数、
STM32 周期帧和明确的两节点接线判断。

## 限制与下一步

当前状态为：

```text
SocketCAN probe/vcan = PASS_HOST
ARM firmware         = PASS_CROSS_BUILD
can-utils CLI        = PASS_HOST (2023.03-1, one-frame vcan smoke)
candleLight HIL      = WAITING_FOR_HARDWARE
physical CAN         = NOT_RUN
```

硬件到货后先完成 S2 准入和 WSL USB 透传，再配置 500 kbit/s 接口。先被动观察约 10 秒，看到六个周期 ID
和一组匹配的 BME 双帧后，才允许一次主机测试发送。状态事件没有自然变化时可以记 `NOT_OBSERVED`，不要求
为凑证据制造故障。双总线并发、bus-off 故障隔离和长稳分别留给 T04/T05。
