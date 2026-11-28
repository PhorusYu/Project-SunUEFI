# Redmi K80 Pro 适配状态

SunUEFI-miro 面向 Redmi K80 Pro（`miro`，SM8750）独立维护。当前实机使用 Linux 7.3.3、Debian sid 与 GNOME Wayland；下面区分这台手机已确认的能力与桌面软件的构建进度。

| 功能 | 状态 |
| --- | --- |
| 双系统启动 | 合体 BOOT、Android 引导助手、UEFI 菜单、systemd-boot 与内部存储 Linux 启动可用，保留原厂 Recovery 和 Android 数据 |
| 显示与触控 | 1440×3200、120 Hz、背光调节与 FT3683 10 点触控已实机确认；熄屏唤醒黑屏已在 #98 修复并实机确认（详见下方说明） |
| 图形加速 | Adreno 830、Mesa 27 与 GNOME Wayland 可用 |
| 内核身份与 RTC | #98 的 `uname` 为 `7.3.3-miro-sunuefi+`；RTC `hctosys=1`，系统日期已校正至 2026-11-28 |
| CPU boost | 内核 `/sys/devices/system/cpu/cpufreq/boost=1`；仅确认接口开启，未据此宣称性能提升 |
| 桌面型号与温度 | GTK 修复包已构建并通过包与库接口检查；Resources 安装包已构建，补丁测试和整应用编译通过；设置应用修复包也已构建并通过包检查；GTK/Resources/设置包已安装；renderer helper 返回 Adreno 830v1，但设置界面尚未目视确认，Resources 的 GPU 名称仍可能为 N/A |
| 网络 | WCN7861 Wi-Fi、蓝牙及 USB NCM 网络可用；蓝牙 UART 为 3.2 Mbps，A2DP 播放可用 |
| 内置扬声器 | TFA9865 双扬声器左右声道清晰播放，音量调节可用 |
| 内置麦克风 | WCD9395 双麦录音可用，GNOME 提供一个内置麦入口，语音试听清楚 |
| USB-C 耳机 | WCD9395 耳机播放可用 |
| 电池与充电 | bq27z561 电量读取与安全 5 V 充电控制可用；支持输入限流、恒压和温度保护 |

显示唤醒：DPMS 熄屏后黑屏已定位并修复，待观察熄屏功耗后再定稿。机制摘要：唤醒时若 DSI 控制器刚从 runtime-suspend 恢复（`DSI power-on runtime PM ret=0`），首条 DCS 命令的 DMA 永不完成（`STATUS0=0x3`、无 `DMA_DONE`、DSI IRQ 计数不动），面板初始化随之中止，并连带 REGDMA、frame done、kickoff 超时；把 DSI 与 PHY 保持在 runtime-active 后该路径不再出现。另一处问题是每次 unprepare 都复位并全量初始化面板，面板帧存被清空，唤醒后有数秒全屏花屏；改为熄屏只发 `0x28` 并把 DBV 归零、亮屏只发 `0x29` 后消失。背光/DBV 通路本身正常且线性（15 极暗、1149 明显可见、4095 最亮）。#98 实机按电源键熄屏 3 秒再亮屏，直接回到原画面，无花屏、无内核显示报错。补丁与当前面板驱动副本见 `tools/patches/display/`。遗留：link 与面板供电轨在熄屏期间保持供电，需实测待机功耗并确认是否收敛为"仅在 unprepare 与下次 prepare 之间持有"；面板不再进入 sleep-in（`0x10`）。

桌面应用补丁的构建与解析测试不能代替真机界面验证；当前不宣称型号和温度界面已修复。

扬声器与麦克风已完成完整内核构建、真机重启、左右声道播放和双通道录音检查。音频通过内核 ALSA、UCM2 与 PipeWire 接入桌面，构建方法见[音频拓扑](devel/building.md#miro-音频拓扑)，来源和许可见[第三方清单](../THIRD_PARTY.md#miro-音频)。

相机、指纹及其他尚未在 miro 上完成的功能不列为可用。仓库保留的 Piano 技术文档用于追溯通用实现和历史来源，其中的平板参数不适用于 miro。
