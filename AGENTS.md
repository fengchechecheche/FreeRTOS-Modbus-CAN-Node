# 仓库协作约定

## 边界

- `app/`、`protocol/`、`sensors/`、`config/`：项目自有代码与合同。
- `bsp/`：板级适配；从 P5-S1-T04 开始落地。
- `Core/`：CubeMX 生成入口；`Drivers/`：HAL/CMSIS；`Middlewares/Third_Party/`：FreeRTOS 等第三方代码。这三类目录默认不做机械格式化或批量静态改写。
- `test/host/`：不依赖 HAL、CMSIS、FreeRTOS 或寄存器头文件的主机测试。
- `artifacts/`：未来公开安全摘要；`.private/`：不得提交的原始证据。

## 构建与验证

使用 C11、CMake Presets、Ninja 和 CTest。主机验证运行 `./tools/verify_host.sh`。固件入口不等于真实固件通过；只有 T04 提供真实启动文件、链接脚本和依赖并完成 Debug/Release 构建后，才能记录交叉构建通过。

## Git 与公开边界

未经用户单独授权，不执行 `git add`、`git commit`、`git tag`、`git push`，不添加 remote，不创建 PR 或 Release。许可证保持 `TBD_USER_REVIEW`，不得猜测。
