# STM32MP157 异构多核智能门禁实验系统

基于 Cortex-A7/Linux 与 Cortex-M4/FreeRTOS，使用 OpenAMP/RPMsg 连接视觉识别与实时执行侧。
本目录是用于 GitHub 管理的**当前开发源码快照**，不是已验收固件发布包，也不包含完整历史 Git 提交。

## 已实现的技术链路

- M4：FreeRTOS、LD2412 OUT 接近检测、OpenAMP、SG90 空载舵机显式单次动作。
- Linux：remoteproc、RPMsg 字符设备驱动及用户态接口。
- 视觉：V4L2 采集、OpenCV 预处理、ncnn 人脸检测/特征提取与匹配。
- Qt：预览、人员管理、PIN、授权规则、事件查询；显示使用 linuxfb，未证明 GPU 加速。
- 遥测实验：SQLite 事务 outbox、稳定事件 ID、mTLS、ACK 后出队、接收端去重和离线补传。

实际接近传感器不是超声波；舵机空载动作不等于真实门锁开闭确认。

## 当前状态与边界

板端稳定组合：M4 内部 v13 / 驱动 2.0 / GUI 内部 v18。当前源码还包含 v19 事务日志候选功能，
其独立存储测试通过，但 GUI 板端黑屏问题未解决，**不可把本快照直接称作稳定 v18**。
自动补传限流修正仍待板端连续复验；上传只针对合成事件。无远程开门接口。
EC20 仅确认 USB 枚举，4G 数据通信未验收。未完成活体检测、准确率评测及最终整机验收。

## 目录

| 路径 | 用途 |
| --- | --- |
| `m4/m4_fw` | M4 源码、CubeMX 配置及随工程提供的第三方依赖 |
| `linux/driver` | RPMsg 字符设备驱动和测试 |
| `linux/app`、`linux/qt_gui` | 视觉应用及 Qt GUI |
| `linux/dts`、`linux/systemd` | 设备树与分阶段部署脚本；不是一键通用配置 |
| `linux/diagnostics` | 历史诊断工具源码/脚本，不能不加判断地执行 |
| `telemetry` | 测试接收端、发送端、outbox 和自动调度 |
| `docs` | 当前架构、构建入口、版本演进、发布检查与状态 |

阅读顺序：[架构](docs/architecture.md) → [构建与测试](docs/build.md) → [版本记录](CHANGELOG.md) → [发布检查](docs/publishing.md)。

模型权重、工具链、Linux BSP、运行数据库、证书、人脸模板、历史二进制未包含。
保留了第三方许可文件，但本项目原创部分尚未指定开源许可证；公开发布前需确认授权范围。
本次只建立提交候选目录，没有创建远端仓库或上传。
