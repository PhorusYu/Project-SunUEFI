# Redmi K80 Pro 适配状态

SunUEFI-miro 面向 Redmi K80 Pro（`miro`，SM8750）独立维护。当前实机使用 Linux 7.3.3、Debian sid 与 GNOME Wayland；下面区分这台手机已确认的能力与桌面软件的构建进度。

| 功能 | 状态 |
| --- | --- |
| 双系统启动 | 合体 BOOT、Android 引导助手、UEFI 菜单、systemd-boot 与内部存储 Linux 启动可用，保留原厂 Recovery 和 Android 数据；优化的 RELEASE 镜像实机启动会报 `Synchronous Exception`，同源码 NOOPT 镜像可正常进 Linux，见下方说明 |
| 显示与触控 | 1440×3200、120 Hz、背光调节与 FT3683 10 点触控已实机确认；熄屏唤醒黑屏已在 #98 修复并实机确认（详见下方说明） |
| 图形加速 | Adreno 830、Mesa 27 与 GNOME Wayland 可用 |
| 内核身份与 RTC | #98 的 `uname` 为 `7.3.3-miro-sunuefi+`；RTC `hctosys=1`，系统日期已校正至 2026-11-28 |
| CPU boost | 内核 `/sys/devices/system/cpu/cpufreq/boost=1`；仅确认接口开启，未据此宣称性能提升 |
| 桌面型号与温度 | GTK 修复包已构建并通过包与库接口检查；Resources 安装包已构建，补丁测试和整应用编译通过；设置应用修复包也已构建并通过包检查；GTK/Resources/设置包已安装；设置 → 关于已实机目视确认正常（机型与 CPU 名称正确），Resources 的 GPU 项仍为 N/A（原因见下方说明） |
| 网络 | WCN7861 Wi-Fi、蓝牙及 USB NCM 网络可用；蓝牙 UART 为 3.2 Mbps，A2DP 播放可用 |
| 内置扬声器 | TFA9865 双扬声器左右声道清晰播放，音量调节可用 |
| 内置麦克风 | WCD9395 双麦录音可用，GNOME 提供一个内置麦入口，语音试听清楚 |
| USB-C 耳机 | WCD9395 耳机播放可用 |
| 电池与充电 | bq27z561 电量读取与安全 5 V 充电控制可用；支持输入限流、恒压和温度保护 |

显示唤醒：DPMS 熄屏后黑屏已定位并修复，待观察熄屏功耗后再定稿。机制摘要：唤醒时若 DSI 控制器刚从 runtime-suspend 恢复（`DSI power-on runtime PM ret=0`），首条 DCS 命令的 DMA 永不完成（`STATUS0=0x3`、无 `DMA_DONE`、DSI IRQ 计数不动），面板初始化随之中止，并连带 REGDMA、frame done、kickoff 超时；把 DSI 与 PHY 保持在 runtime-active 后该路径不再出现。另一处问题是每次 unprepare 都复位并全量初始化面板，面板帧存被清空，唤醒后有数秒全屏花屏；改为熄屏只发 `0x28` 并把 DBV 归零、亮屏只发 `0x29` 后消失。背光/DBV 通路本身正常且线性（15 极暗、1149 明显可见、4095 最亮）。#98 实机按电源键熄屏 3 秒再亮屏，直接回到原画面，无花屏、无内核显示报错。补丁与当前面板驱动副本见 `tools/patches/display/`。遗留：link 与面板供电轨在熄屏期间保持供电，需实测待机功耗并确认是否收敛为"仅在 unprepare 与下次 prepare 之间持有"；面板不再进入 sleep-in（`0x10`）。

熄屏功耗未能量化：拔掉充电器、仅走 Wi-Fi 的实测里，亮屏空闲 ≈−358 mA、熄屏 ≈−310 mA，而关掉 Wi-Fi 后仍 ≈−311 mA、恢复后 ≈−305 mA —— 三者几乎相同，说明 `bq27z561` 的 `current_now` 是平滑/滞后值，分钟级采样分辨不出亮屏、熄屏、Wi-Fi 开关之间几十 mA 的差别。这 ≈300 mA 是"Linux 桌面运行、SoC 未深度休眠"的基线，不是待机值。要判定 #98 保持 DSI/PHY 与面板供电的增量成本，只能用小时级 A/B 电量曲线（两配置各 ≥30 分钟）或外接电流表；短时实验不足以支撑结论。

桌面应用补丁的构建与解析测试不能代替真机界面验证。

Resources 的 GPU 项为 N/A 的原因（2026-11-28 实机核对）：`/sys/class/drm/card1/device` 在 msm 上是**显示控制器**（`msm_dpu`，compatible `qcom,sm8750-dpu`），不是 PCI GPU，因此 `OtherGpu::name()` 走的 `Device::name()` 取不到名字；该节点下也没有 hwmon，温度、功耗、频率、VRAM 相应为 N/A。DT 中 GPU 节点为 `soc@0/gpu@3d00000`（`qcom,adreno-44050001`、`#cooling-cells = <2>`、phandle 347），但本平台没有任何 thermal zone 的 cooling-device 引用该 phandle，现有 `platform::node_thermal_paths()` 因此返回空，回落到不存在的 hwmon。真实可用的数据是 `/sys/class/devfreq/3d00000.gpu`（频率）与 `gpuss0`–`gpuss7` 温区。

上述缺口已由 0002/0003 两个补丁补齐（实机确认）：GPU 名称取标准 GTK helper 报出的 renderer 字符串（`Adreno (TM) 830v1`，与设置页同源，不硬编码型号），厂商取设备树根 compatible 的 vendor 前缀（`qcom` → `Qualcomm`），时钟取 `/sys/class/devfreq/3d00000.gpu/cur_freq`（实测 160 MHz 起随负载变化），温度由 `gpuss*` 温区匹配得到（实测 46–48 °C）。仍然为 N/A 的是硬件与内核未暴露的项：使用率/编解码占用（无 `gpu_busy_percent` 或引擎统计）、VRAM（统一内存）、功耗（无 hwmon）、链路（非 PCIe）。

注意：GPU 名称依赖打过补丁的 GTK（helper 的 EGL 初始化由 GTK MR 10780 回移修复）。若 GTK 回退为官方包，helper 会报 `No EGL configuration available`，名称退回 N/A，其余字段不受影响。

迭代方式：Resources 可以在手机上原生编译（约 50 秒，对比 WSL+qemu 的 25–30 分钟），使用 `tools/desktop/build_resources_on_device.sh`；该脚本补齐 meson 才会注入的编译期常量（`LIBEXECDIR` 必须指向 `/usr/libexec/resources`，`RESOURCES_FILE` 指向已安装的 gresource，否则启动会 panic 成空白窗口）。正式发布包仍由 WSL 的可复现流水线产出。

UEFI 构建对照（2026-11-28 实测）：优化 RELEASE 镜像用 `fastboot boot` 临时启动时报 `Synchronous Exception`，未进桌面；同源码仅把 SimpleInit 改为 NOOPT 的镜像（`outputs/miro-uefi-noopt/MiroUEFI-product.img`，28,901,376 字节，SHA256 `f8eb8fc82920705541d9acf9687957d3bee14230ac1ab78152284fa5b4ecd82f`）发送 0.9 秒、12 秒进入 Linux，本次启动显示报错为 0。因此该崩溃属于优化暴露的问题，不是源码逻辑或硬件故障；NOOPT 镜像可作为兜底，但未写入 `boot_a`（安装的稳定镜像未改动）。同一 NOOPT 镜像下 GUI 详情的帧率读数仍为 0–1 fps，说明帧率问题是 GUI pump/渲染实现本身而非优化等级；实机评估为"勉强可接受"，暂不作为缺陷跟踪。

扬声器与麦克风已完成完整内核构建、真机重启、左右声道播放和双通道录音检查。音频通过内核 ALSA、UCM2 与 PipeWire 接入桌面，构建方法见[音频拓扑](devel/building.md#miro-音频拓扑)，来源和许可见[第三方清单](../THIRD_PARTY.md#miro-音频)。

相机、指纹及其他尚未在 miro 上完成的功能不列为可用。仓库保留的 Piano 技术文档用于追溯通用实现和历史来源，其中的平板参数不适用于 miro。
