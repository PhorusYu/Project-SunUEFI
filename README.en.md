# SunUEFI-miro

Standalone UEFI firmware and native Linux (Debian Sid / GNOME) dual-boot project for the **Redmi K80 Pro (`miro`, Qualcomm Snapdragon 8 Elite / SM8750)**. Source code and issue tracking are maintained independently at [PhorusYu/SunUEFI-miro](https://github.com/PhorusYu/SunUEFI-miro). See the acknowledgements below for its origins.

[简体中文](README.md)

## Verified Capabilities

- **Combined BOOT + Android Boot Helper**: Losslessly repacks active `boot_a` using `SUNUEFI-SPLITv1` (preserving stock Android GKI, `recovery`, and `vbmeta`) with a Magisk / KernelSU WebUI module for PC-free dual-booting.
- **UEFI Boot Chain**: `MiroUEFI` (SimpleInit menu + persistent variables) → `systemd-boot` (`sunuefi_esp`) → Linux `7.3.3` (`sunuefi_root`).
- **120Hz Native Display**: `xiaomi,miro-o11u` (`o11u_42_02_0a_dsc_cmd`, 1440×3200 @ 120Hz) with DPU DSC 1.2 (`DRM_DSC_1_2_444`), TE (`gpio86`), and backlight brightness control.
- **Touchscreen**: Device-tested FocalTech FT3683 SPI 10-finger multitouch (`focaltech_ft3683_spi` with `gpi_dma1` and `vreg_l12b_1p8`).
- **Kernel identity, clock and CPU**: `uname` identifies `miro-sunuefi`; RTC boot-time synchronization works (`hctosys=1`), and the CPU boost interface is enabled (`boost=1`).
- **Hardware Keys**: Power (`pmic_pwrkey`), Volume Down (`pmic_resin`), and Volume Up (`gpio-keys` / `pm8550-gpio 6`).
- **Adreno 830 GPU**: `gen80000_zap.mbn` + GMU `v5.1.25` with Mesa 27 (Freedreno OpenGL 4.6 / Turnip Vulkan 1.4) driving GNOME Wayland and Firefox WebGL.
- **Wi-Fi 7, Bluetooth & USB NCM**: WCN7861 Wi-Fi 7 (`ath12k` `peach hw2.0` via AOSS PDC votes), WCN7861 Bluetooth (`qcom,wcn7861-bt` + `miro-bt-setup.service`), and USB NCM (`eusb2-repeater` + `udhcpd`).
- **Speakers and microphones**: TFA9865 stereo speaker playback, WCD9395 internal dual-microphone recording, and USB-C headset playback work through ALSA UCM2 / PipeWire in GNOME. Bluetooth retains A2DP playback with a 3.2 Mbps UART.
- **Battery & Safe 5V Charging**: `bq27z561-0` fuel gauge + `miro-adsp.service` + `miro_mca` safe 5V charging control (SDP/CDP/DCP/PD current limits, `4400 mV` float voltage, thermal/voltage guards, and `charge_behaviour`).
- **Lossless UFS Partitioning**: Online `userdata` shrink via `piano-resize-f2fs` and `piano-storage` creating `sunuefi_esp` and `sunuefi_root` (`LABEL=PIANOROOT`).

Known display issue: waking after Linux DPMS screen-off can leave the panel black. A full poweroff followed by a fresh boot restored the display on the device; screen-off/wake is not considered fixed.

Desktop hardware information: GTK `4.24.1+ds-1+sunuefi1` packages have been built and passed package and library interface checks. Resources `51.0+ds-3+sunuefi1` packages have also been built; its CPU-name and CPU/GPU-temperature patches passed parser tests and a complete application build. Settings `1:51.1-2+sunuefi1` packages have also been built and checked. The GTK, Resources and Settings packages are now installed on the phone. GNOME Control Center’s GTK renderer helper returned `Adreno (TM) 830v1`; the Settings UI has not been visually checked, and Resources may still show N/A for the GPU name.

## Build Commands

```sh
# 1. Build MiroUEFI product image
./build.sh uefi --target miro

# 2. Build the miro audio topology using linux-msm/audioreach-topology templates
./build.sh miro-audio --topology-source /path/to/audioreach-topology

# 3. Build Android boot helper module ZIP for miro
./build.sh module --product artifacts/product
```

## Credits & License

- Built upon the SM8750 UEFI core, combined BOOT (`SUNUEFI-SPLITv1`), and lossless storage tooling from [slwyts/Project-SunUEFI](https://github.com/slwyts/Project-SunUEFI).
- See [LICENSE.md](LICENSE.md) and [THIRD_PARTY.md](THIRD_PARTY.md) for license scope and third-party notices.
