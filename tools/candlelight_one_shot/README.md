# candleLight one-shot/NART 最小补丁

该目录为项目五使用的 USB-CAN 适配器提供一个有界、可回退的
`one-shot/NART` 补丁载体。它不复制完整第三方源码，也不把自定义适配器固件
纳入 STM32 节点默认构建。

## 适用基线与改动

- 上游来源：`https://github.com/normaldotcom/candleLight_fw`
- 固定提交：`d13b6db511d76885533c6e7e0ef22d74a5e2d817`
- 目标：STM32F072 `candleLight_fw`
- 板型适配：保留当前适配器已验证的 PA1=TX LED、PA0=RX LED 路由
- 功能改动：在固件位时序能力中通告 `GS_CAN_FEATURE_ONE_SHOT`

该基线已经定义 `GS_CAN_MODE_ONE_SHOT`，在 USB 模式请求中解析该标志，并在
bxCAN 初始化时设置 `CAN_MCR_NART`。补丁只使 Linux `gs_usb` 驱动能够发现并
请求这条既有路径。

## 构建

在 Ubuntu-24.04-STM32 中执行：

```bash
cd <freertos_modbus_can_node-repository>

./tools/candlelight_one_shot/build.sh \
  "/path/to/clean/candleLight_fw"
```

脚本会：

1. 核对固定提交；
2. 从该 Git 对象创建被忽略的干净 `out/candlelight-one-shot/source/`；
3. 在复制件中依次应用板型 LED 路由和 one-shot 能力补丁；
4. 构建 STM32F072 `candleLight_fw.bin`；
5. 生成 `out/candlelight-one-shot/SHA256SUMS.txt`。

脚本不会烧录设备，也不会删除或覆盖已有输出目录。

## 烧录后的准入检查

保留已有的厂商全片备份和 SHA-256，确认能够回退后，才允许单独规划 DFU
烧录。重新枚举到 Ubuntu 后，先执行：

```bash
ip -details link show can0
```

只有输出明确列出 `one-shot`，才执行：

```bash
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000 sample-point 0.75 one-shot on
sudo ip link set can0 up
ip -details -statistics link show can0
```

## 验收边界

- `one-shot` 只限制适配器 CAN 控制器对同一帧的硬件自动重发；上层程序反复
  调用发送仍会产生多次发送尝试。
- candleLight 基线在帧写入 CAN 外设时就向 Host 返回 local TX echo，而不是
  等待总线发送成功。因此 local echo 或发送 API 成功不能单独证明收到 ACK。
- 实物验收必须同时观察 STM32 `0x540/0x541` 往返、SocketCAN 错误帧和接口
  计数。若缺少 ACK，单次失败仍可能使 TEC 增加，但不应由同一帧无限重发。
- 本补丁不修改项目五 STM32 默认固件，也不改变其 CAN ID、过滤器或 NART
  策略。
