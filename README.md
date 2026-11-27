# SunUEFI-miro

专为 **Redmi K80 Pro（代号 `miro`，高通骁龙 8 至尊版 SM8750）** 打造的独立 UEFI 固件与原生 Linux（Debian Sid / GNOME）双系统项目，基于 [slwyts/Project-SunUEFI](https://github.com/slwyts/Project-SunUEFI) 分支独立演化并移除其他机型代码。

[English](README.en.md)

## 当前状态与已验证特性

- **合体 BOOT + Android 引导助手**：通过 `SUNUEFI-SPLITv1` 无损重打包 `boot_a`（保留原厂 Android 内核、`recovery` 与 `vbmeta`），配合 Magisk / KernelSU 模块 WebUI 实现脱离电脑独立双系统启动。
- **UEFI 启动链**：`MiroUEFI`（SimpleInit 菜单 + 持久变量）→ `systemd-boot`（`sunuefi_esp`）→ Linux `7.3.3`（`sunuefi_root`）。
- **120Hz 原生显示**：`xiaomi,miro-o11u`（`o11u_42_02_0a_dsc_cmd`，1440×3200 @ 120Hz）+ DPU DSC 1.2（`DRM_DSC_1_2_444`）+ TE 同步（`gpio86`）+ 屏幕背光亮度调节，桌面与 `kmscube` 无花屏。
- **屏幕触控**：FocalTech FT3683 SPI 10 点触控（`focaltech_ft3683_spi`，`gpi_dma1` + `vreg_l12b_1p8` 供电）。
- **物理按键**：电源键（`pmic_pwrkey`）、音量下键（`pmic_resin`）、音量上键（`gpio-keys` / `pm8550-gpio 6`）。
- **Adreno 830 GPU**：加载 `gen80000_zap.mbn` + GMU `v5.1.25`，搭配 Mesa 27（Freedreno OpenGL 4.6 / Turnip Vulkan 1.4），Wayland GNOME 与 Firefox WebGL 硬件加速正常。
- **Wi-Fi 7、蓝牙与 USB NCM**：WCN7861 Wi-Fi 7（`ath12k` `peach hw2.0` + AOSS PDC 投票，支持固定原厂 MAC）、WCN7861 蓝牙（`qcom,wcn7861-bt` + `miro-bt-setup.service`）与 USB NCM（`eusb2-repeater` + `udhcpd`）可用。
- **电池电量与安全 5V 充电**：`bq27z561-0` 独立电量计 + `miro-adsp.service`（ADSP `charger_pd`）+ `miro_mca` 安全 5V 普通充电控制（支持 SDP/CDP/DCP/PD 限流、`4400 mV` 恒压、温度/电压保护及 `charge_behaviour`）。
- **无损双系统分区**：基于 `piano-resize-f2fs` 与 `piano-storage` 无损收缩 `userdata`，建立 `sunuefi_esp`（512 MiB）与 `sunuefi_root`（`LABEL=PIANOROOT`）。

## 仓库核心目录

```text
uefi/platforms/miroPkg/   Redmi K80 Pro (miro) UEFI 平台包
linux/configs/miro.config Linux 7.3 内核配置片段
tools/patches/            miro 内核补丁（含 120Hz o11u / DSC 1.2 修复）
tools/build_miro_ram_boot.py  miro DTB 覆写与启动镜像打包脚本
tools/firmware/           从已 Root 的 Android 提取 WCN7861 / Adreno 830 固件
android/                  Android 引导助手模块（WebUI + piano-boot-repack）
```

## 常用构建入口

```sh
# 1. 构建 MiroUEFI 产品镜像
./build.sh uefi --target miro

# 2. 构建 miro Android 引导助手模块 ZIP
./build.sh module --product artifacts/product
```

## 致谢与许可证

- 感谢 [slwyts/Project-SunUEFI](https://github.com/slwyts/Project-SunUEFI) 提供 SM8750 UEFI 底层、合体 BOOT（`SUNUEFI-SPLITv1`）、无损存储分区工具与基础基础设施。
- 许可证范围与第三方声明见 [LICENSE.md](LICENSE.md) 与 [THIRD_PARTY.md](THIRD_PARTY.md)。
