# 构建与验证入口

保留源目录相对结构。本仓库不携带 SDK、内核 BSP、模型权重或历史编译结果。
以下为构建入口，不是本次完成跨平台重构或重新全量构建的声明。

## M4

Windows：STM32 ARM GCC 工具链，工程历史使用 13.3.1。
在仓库根运行（把工具链路径替换为实际路径）：

```powershell
./m4/m4_fw/CM4/build_windows.ps1 -ToolchainBin 'C:/path/to/toolchain/bin' -Target m4_fw_CM4_oneshot_v13 -ServoManual -ServoRadar -ServoOneShot
```

构建脚本解析 Makefile 的源文件清单；旧 Makefile 的 SHELL 含原作者 Windows 路径。
不要将默认构建选项等同于已验收 one-shot 配置，也不要直接烧录未核查产物。

## Linux 驱动与 A7

- 驱动：`linux/driver/Makefile`，需与板端运行内核匹配的配置/头文件/构建树；显式传入 KDIR 与 CROSS_COMPILE。
- ncnn：`linux/ncnn/build_ncnn.sh`，历史 tag 20210507。脚本包含 checkout -f 与清理操作，只对专用依赖副本使用，不指向有未提交改动的 ncnn 工作树。
- A7：`linux/app/build_app.sh`。
- GUI：`linux/qt_gui/build_gui.sh`；需要 Qt 5.12（Core/Gui/Widgets/Sql）、OpenCV、ncnn。
- 存储：`telemetry/outbox/build_outbox.sh`。
- 合成事件发送：`telemetry/event_sender/build_sender.sh`。

现有脚本绑定 OpenSTLinux 3.1-snapshot 的 SDK 布局，若使用不同环境须先审查路径；不要混用桌面 Qt 库和 ARM Qt 库。
模型按源码配置路径另行提供，并确认权重授权；不在此仓库分发。

## 不接触开发板的接收端测试

Python 3 与 cryptography。建议在独立虚拟环境安装依赖：

```text
python -m pip install -r requirements-dev.txt
python -B -m unittest discover -s telemetry/receiver -p "test_*.py" -v
python -B tools/check_repository.py
```

TLS 测试只用临时凭据、临时数据库和回环监听，不应指向正式凭据目录。
Qt/C++ 测试入口在 `linux/qt_gui/tests`、`telemetry/outbox`、`telemetry/event_sender`。

## 部署不是构建的自动后续

`linux/systemd` 保留旧版本部署逻辑，含既有文件名、摘要、SD UUID、服务覆盖项；
须逐项核对目标板。不要依次执行全部历史安装脚本。v19 候选不能替换稳定 v18 后就宣布验收通过。
