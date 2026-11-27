# Redmi K80 Pro 适配状态

SunUEFI-miro 面向 Redmi K80 Pro（`miro`，SM8750）独立维护。当前实机使用 Linux 7.3.3、Debian sid 与 GNOME Wayland；下面只列出这台手机已确认的能力。

| 功能 | 状态 |
| --- | --- |
| 双系统启动 | 合体 BOOT、Android 引导助手、UEFI 菜单、systemd-boot 与内部存储 Linux 启动可用，保留原厂 Recovery 和 Android 数据 |
| 显示与触控 | 1440×3200、120 Hz、背光调节与 FT3683 多点触控可用 |
| 图形加速 | Adreno 830、Mesa 27 与 GNOME Wayland 可用 |
| 网络 | WCN7861 Wi-Fi、蓝牙及 USB NCM 网络可用；蓝牙 UART 为 3.2 Mbps，A2DP 播放可用 |
| 内置扬声器 | TFA9865 双扬声器左右声道清晰播放，音量调节可用 |
| 内置麦克风 | WCD9395 双麦录音可用，GNOME 提供一个内置麦入口，语音试听清楚 |
| USB-C 耳机 | WCD9395 耳机播放可用 |
| 电池与充电 | bq27z561 电量读取与安全 5 V 充电控制可用；支持输入限流、恒压和温度保护 |

扬声器与麦克风已完成完整内核构建、真机重启、左右声道播放和双通道录音检查。音频通过内核 ALSA、UCM2 与 PipeWire 接入桌面，构建方法见[音频拓扑](devel/building.md#miro-音频拓扑)，来源和许可见[第三方清单](../THIRD_PARTY.md#miro-音频)。

相机、指纹及其他尚未在 miro 上完成的功能不列为可用。仓库保留的 Piano 技术文档用于追溯通用实现和历史来源，其中的平板参数不适用于 miro。
