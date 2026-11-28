# 下一位 AI 处理屏幕唤醒问题的移交提示词

请接手 Redmi K80 Pro（`miro` / SM8750）在 Linux 7.3.3、Debian sid、GNOME Wayland 下的内屏熄屏后无法恢复问题。用户本轮已明确要求：先暂停面板唤醒试验，完成其余修复与 Antigravity 同步，并把屏幕问题交给下一位 AI。现在可以从本提示继续屏幕工作，但不要重复已完成的盲目复位/延时试验。先读完状态和证据，再提出由日志支持的下一步。

## 1. 项目、仓库与硬件

- 设备：Redmi K80 Pro，设备代号 `miro`，SoC `SM8750`，内屏为 1440×3200、120 Hz、DSC 1.2、DPU command mode，Novatek NT37801 面板；系统桌面是 GNOME Wayland。
- 独立项目仓库（Antigravity）：`C:\Users\Phorus\.gemini\antigravity\scratch\Project-SunUEFI`，WSL 路径 `/mnt/c/Users/Phorus/.gemini/antigravity/scratch/Project-SunUEFI`。分支 `miro`，`origin` 指向 `PhorusYu/SunUEFI-miro`。项目已经独立；旧 fork 不存在，不要重新添加 fork remote。
- 本次协作工作区：`C:\Users\Phorus\Documents\Codex\2026-11-27\redmi-k80-pro-miro-sm8750-mainline-2`，WSL 路径 `/mnt/c/Users/Phorus/Documents/Codex/2026-11-27/redmi-k80-pro-miro-sm8750-mainline-2`。测试证据主要在 `work/hardware/`，临时 UEFI 镜像在 `outputs/`。
- Linux 内核 worktree：`/root/Project-SunUEFI/build/kernel-worktrees/release-e08cdb7d7004-d4d9744ba3f9`。
- 内核输出：`/root/miro-linux-test/out`。
- DT/config/启动镜像脚本：`/mnt/c/Users/Phorus/.gemini/antigravity/brain/7ad240a4-2954-4c93-acc5-e989462ea86a/scratch/setup_miro_linux.py` 与同目录 `rebuild_disk_nopt.sh`；仓库中的镜像生成器是 `tools/build_miro_ram_boot.py`。
- 手机 SSH 可通过现有 `work/remote.py`：USB `root@192.168.77.1`，Wi-Fi `root@192.168.2.220`。脚本负责认证。优先用脚本，不要把口令写进仓库或交接文档。

## 2. 硬性安全与 Git 约束

- 严禁修改 `recovery_a` 与 Android `userdata`。不要对 recovery/userdata 分区执行任何写入、格式化、resize 或解锁操作。
- 现有稳定 UEFI 已安装在 `boot_a`。新的 UEFI 试验只能用 `fastboot boot` 临时启动；除非用户明确重新授权，绝不刷写 boot 分区。
- 工作树里有本项目的多项未提交修改，也有与本任务无关的既有改动和被修改的 submodule。不要 `git reset --hard`、`git clean`、批量暂存或覆盖它们。不要提交或推送，除非用户明确要求。
- 若后续获得提交授权，作者只能是 `PhorusYu <PhorusYu@users.noreply.github.com>`；不允许出现 Codex、OpenAI、AI 或 `Co-authored-by` 署名。
- Windows 下的复杂 WSL 命令先保存为 `.py`/`.sh`，避免在 PowerShell 命令行中直接拼接 `&&`、`||` 或裸管道。

## 3. 手机当前状态

- 最新运行内核为 `7.3.3-miro-sunuefi+ #94 SMP PREEMPT_DYNAMIC Sat Nov 28 11:54:40 CST 2026`。
- 当前显示问题复测后，屏幕仍黑；SSH 曾持续在线。Mutter 的 `PowerSaveMode` 读回 0（开启），`/sys/class/graphics/fb0/blank` 曾读回 4；通过标准接口写 0 后画面没有恢复。若重新启动或唤醒，先观察物理屏幕，不要把软件命令返回 0 当成屏幕已亮。
- 最近一次 runtime-PM 对照结束后，MDSS、DPU、DSI、DSI PHY 四个 `power/control` 已全部恢复为 `auto`。当前没有常开电源域设置。
- 完整关机后等待约 10 秒再开机曾恢复正常桌面；但之后再次 DPMS off/on 又变黑。普通软件重启有时仍黑，用户手动重启或完整断电曾恢复。不要把完整关机当成修复。
- 现有显示配置：屏幕 GNOME/DRM connector `card1-DSI-1`，debugfs 设备 `ae01000.display-controller`，DSI host `ae94000.dsi`，DSI PHY `ae95000.phy`。

## 4. 用户已确认、不要重复做的工作

- UEFI 早期启动选择器改成竖屏布局，字体已由用户确认清晰；可正常通过实体按键选择 Android/Linux。
- FT3683 十点触控修复已应用，用户在浏览器确认 10 点可同时使用。新导出的驱动补丁已放进 Antigravity 仓库 `tools/patches/miro-touch-ft3683-spi.patch`，对当前内核的三个文件反向应用检查通过。
- 内核名称已改为 `7.3.3-miro-sunuefi+`，#94 实机确认不含 Piano 字样。
- RTC/NVMEM 开机校时已工作：`/sys/class/rtc/rtc0/hctosys=1`。#94 实测日期为 `2026-11-28`，本地时区 `Asia/Shanghai`，网络时间同步此前为 active。
- CPU boost 已持久开启：`/sys/devices/system/cpu/cpufreq/boost=1`，#94 实机确认。仅确认内核 boost 接口启用，不代表已做性能基准。
- CPU/GPU 的 thermal zones 已由内核暴露，包含 `cpu-*`、`gpuss*` 等；温度在 sysfs 可读。GNOME Settings、Resources 的型号和温度改进已构建并安装；renderer helper 返回 Adreno 830v1，但界面尚未目视复核，见第 8 节。
- 音频已真机验证：TFA9865 左右扬声器清楚无杂音、WCD9395 内置双麦录音语音清晰、USB-C 耳机播放正常、蓝牙 A2DP 3.2 Mbps 正常。不要回退或覆盖相关 UCM2、拓扑、BT 配置。
- 电量读取与安全 5 V 充电控制已完成并验证：bq27z561 独立 gauge、ADSP `charger_pd`、`miro_mca` 输入限流/恒压/温度保护。不要移除充电保护。

## 5. 屏幕问题的逐次实测与代码状态

不要只凭“DSI reset/多等几秒”猜测。现有结果如下。

1. #88、#89、#91 等版多次测试发现：GNOME 发起 DPMS off→on 后，D-Bus 调用成功，但实体屏幕仍黑；有时 SSH 保持在线。故障不应被表述为用户态调用成功或背光有亮度就修复。
2. #92 将 miro 面板供电改为 Android 顺序 `vddio → vddd(GPIO120) → vci(3.0 V L13B)`，VDDD 仅 `boot-on` 不再 `always-on`。该版无花屏冷启动正常，但 DPMS 仍会黑屏。它不能作为已解决证据。
3. #93/#94 只增加 DSI 状态采样，没有改变 DSI 操作时序。补丁差异在 `work/power/dsi-extra-snapshot.diff`、`work/power/dsi-90-snapshot.diff`；对象编译通过。手机 #94 的诊断日志在 `work/hardware/dsi-events.txt`、`display-cycle.txt`、`dsi-after92.txt`、`drm-state.txt`、`display-boot-current.txt`、`pm-controls.txt`。日志时间以 2026-11-28 为准。
4. #94 冷启动时 DSI 0x2A、0x2B、0x90 命令全部完成，status=0；设备屏幕冷启动能正常显示。
5. 普通 runtime-PM `auto` 下，DPMS 恢复有一轮首个 0x2A DMA timeout：`STATUS0=0x3`（command engine/DMA busy），无 DMA_DONE raw IRQ，`INTR=0xaa20aa02`，`LANE_STATUS=0x1f0f`，`LANE_CTRL=0x10000000`，`CMD_DMA_CTRL=0x14000000`，`CLK_STATUS=0x008037c3`。按 dsi.xml，CLK_STATUS bit16（PLL_UNLOCKED）为 0，lane 的 STOPSTATE/ULPS_NOT 位与冷启动一致；之后看到 DPU color REGDMA timeout。另一次 #93 唤醒中 0x2A/0x2B 完成，接着 0x90 先超时，说明故障时机可变。
6. 诊断期间把 MDSS、DPU、DSI、PHY 的 sysfs `power/control` 临时设为 `on`，测试后立即全部恢复 `auto`。屏幕仍黑，但本次 DSI 2A/2B/90 命令全完成且没有 DMA/REGDMA 错误。由此不能只归因于 runtime-PM 掉电；还需要看 DPU 实际 kickoff/frame_done、panel 显示状态和 framebuffer 内容。
7. 一次黑屏后的 debugfs DRM 快照显示 CRTC state active、1440×3200 plane/fb 仍挂在 GNOME Shell，DSI connector connected/enabled；当时 fbdev `blank=4`。Mutter `PowerSaveMode=0`。写 `/sys/class/graphics/fb0/blank=0` 未使实体屏幕恢复。该快照并非最新正常冷启动对照，重新抓取时应保存时间与内核版本。
8. DPU/DSI 代码已有前序试验修改：`dpu_crtc.c` 将 color update 移到 encoder prepare 后，错误 fence 与 REGDMA 故障安全保留；`dsi_host.c` 有 SM8750 host SW reset、DSC/寄存器诊断等修改。不要清空 fault latch 或移除 GEM/VM/clocks 安全保留逻辑。#92/#94 尚未解决亮屏恢复。
9. 完整断电可使面板回到正常冷启动；这表明目前黑屏可由电源循环恢复，但并没有证据说明硬件已永久损坏。用户要求当前暂停屏幕测试，移交后再经用户确认安排新一轮实测。

下一步建议先只读检查 DPU `encoder-0/status`、CRTC debugfs state、frame_done/vblank、Mutter `PowerSaveMode`、DRM fb/plane 和 panel/backlight 状态；比较“正常冷启动显示”和“DPMS 后黑屏”。如果要做新的用户可见测试，先告知用户要观察什么，限制为一次有明确假设的实验，测试后恢复所有临时设置。不要再重复未产生结果的显示模式 off/on 或随意增加 sleep/reset。

## 6. UEFI GUI 与启动安全旁支

- 已安装的稳定 `boot_a` UEFI 菜单字体清晰。安装的 `boot_a` 没有被本轮 RELEASE 崩溃版本覆盖。
- 新 UEFI optimized RELEASE 曾经用 `fastboot boot` 临时启动，屏幕报告 `Synchronous Exception at 0x000000000...CD63FABC`（用户辨读，前导零长度不确定）。它位于动态加载区域，缺少运行时 ImageBase/完整寄存器，不能仅凭绝对 PC 符号化。RELEASE 的 FD `.map/.pdb` 和 SimpleInit `.debug` 在 `work/audio/uefi-release-symbols/`。
- NOOPT 对照镜像已独立构建，保留相同源码、GUI pump、RamOnly、完整身份校验，只将 SimpleInit 改为 NOOPT：`outputs/miro-uefi-noopt/MiroUEFI-product.img`，28,901,376 字节，SHA256 `f8eb8fc82920705541d9acf9687957d3bee14230ac1ab78152284fa5b4ecd82f`。镜像封装检查通过，尚未实机测试。
- RELEASE GUI 的 0–1 fps 问题尚未有合格的设备端帧率验收。下一位 AI 处理显示问题时不要误把 Linux panel wake 与 UEFI GUI 优化混作一个故障，也不要刷写该 NOOPT/RELEASE 镜像。
- 稳定回退镜像历史摘要：`d11edd87518c0cbfa9d780e682c2cd0ff0d07efb3c4ac4fec45cc9a9733a2ff1`。设备 `boot_a` 约 96 MiB；此前记录要求先验证分区身份/长度再做任何写入。当前禁止写入。

## 7. 项目配置与可用功能背景

- Wi-Fi/BT 是 WCN7861，`ath12k` + AOSS PDC；Wi-Fi SSH 常用地址 `192.168.2.220`。USB NCM 地址 `192.168.77.1`。
- GPU 是 Adreno 830；Mesa/Freedreno OpenGL 4.6、Turnip Vulkan 工作。实际 GL renderer 的可靠来源是 Mesa/GTK EGL helper，不要硬编码 GPU 名称。
- 两颗 TFA9865 走 `SECONDARY_TDM_RX_0`，当前左右声道已验证；WCD9395 双麦、UCM2、PipeWire 已验证。
- 电池用 bq27z561 独立 gauge；ADSP 固件/PDR `charger_pd` 已工作，安全 5 V 充电控制有温度、电压、限流保护。
- 设备树来自 SM8750 Piano 上游底层，但项目硬件目标只有 Redmi K80 Pro。不要把 Piano 平板的摄像头、触控笔、四扬声器或传感器参数当作 miro 能力。
- 开机时间修复为只读 RTC/NVMEM 路径：UEFI 提供 NVMEM 数据，Linux `NVMEM_RMEM=y` 与 PM8xxx RTC 内置，`hctosys=1`；systemd-timesyncd 配置在 `/usr/lib/systemd/timesyncd.conf.d/50-miro.conf`。

## 8. 正在收尾的非屏幕改动（已同步到 Antigravity，未提交）

- GTK 4 EGL 修复：Debian GTK 4.24.1 补丁来自已合入的 GNOME MR 10780（上游 commit `49b300d8`）并回移到 Debian `4.24.1+ds-1`，解决 helper 无 EGL configuration。ARM64 包 `libgtk-4-1`、`libgtk-4-common`、`gir1.2-gtk-4.0`，版本 `4.24.1+ds-1+sunuefi1`，位于 `work/hardware/packages/gtk/`。SONAME `libgtk-4.so.1` 不变，5,521 个原动态导出保留。已构建/ABI 检查并安装到手机。
- GNOME 49 起 `org.gnome.SessionManager.Renderer` 为空是官方弃用 API 的预期行为（官方文档 `https://gnome.pages.gitlab.gnome.org/gnome-session/re04.html`），不要试图让它重新非空。GNOME Control Center 应在 renderer 空值时调用标准 GTK helper；helper 的 EGL 初始化由 GTK MR 修复。该链路已安装；手机 helper 实测返回 `Adreno (TM) 830v1`，但尚未目视验证设置界面。
- Resources 51.0：CPU 名称改为优先读取实际 `/proc/cpuinfo`，缺省时用 SoC 信息；CPU/GPU 温度按 ARM thermal zones 识别，不猜造硬件数据。ARM64 包 `resources_51.0+ds-3+sunuefi1_arm64.deb` 已构建并验证，位于 `work/hardware/packages/resources/`，SHA256 `96b4e62a599877e39f07b9e1872358b946b1d845f9c326a246fe5c54391122b2`。补丁与包来源/哈希验证通过且已安装到手机。运行 GNOME renderer helper 实测返回 `Adreno (TM) 830v1`；Resources 的 GPU 名称仍可能显示 N/A。
- GNOME Control Center 51.1：CPU SoC fallback、空 renderer helper fallback 及错误处理补丁已构建；两包通过来源/架构/版本/SHA256 检查并已安装；设置界面尚未目视复核。
- GSD 51.0 power-button `blank` 动作补丁、`90-miro-power.gschema.override` 已同步；真机电源键熄屏行为尚未单独验证，DPMS 唤醒问题仍在。
- `miro.config`、`setup_miro_linux.py`、`tools/build_miro_ram_boot.py` 已同步 RTC/NVMEM、`LOCALVERSION=-miro-sunuefi`、面板 VCI L13B 与 VDDD boot-on 设置。
- `tools/patches/miro-touch-ft3683-spi.patch` 为已验证十点版本。
- `/usr/lib/tmpfiles.d/miro-cpufreq.conf` 持久开启 boost；timesyncd 配置与 systemd 包清单已同步。
- `README.md`、`README.en.md`、`docs/status.md` 已如实更新唤醒限制与平台状态；`THIRD_PARTY.md` 已登记 GTK/GSD/Control Center/Resources 的上游、版本和许可核对。项目 `LICENSE.md` 没有改动（本次没有改变项目整体许可范围）。
- GTK 三包、Resources 单包、GNOME Control Center 两包现已全部构建、通过来源/架构/版本/SHA256 检查并安装到手机：
  - GTK：`work/hardware/packages/gtk/`，`4.24.1+ds-1+sunuefi1`；`libgtk-4.so.1` ABI 保持，5,521 个导出全部保留。
  - Resources：`work/hardware/packages/resources/resources_51.0+ds-3+sunuefi1_arm64.deb`，SHA256 `96b4e62a599877e39f07b9e1872358b946b1d845f9c326a246fe5c54391122b2`。
  - GNOME Control Center：`work/hardware/packages/control-center/`，`1:51.1-2+sunuefi1`；主包 SHA256 `c09946ca389fa7c805512c296a2859daf164df4de96c647cb1c27749301f6838`，data 包 SHA256 `df1f8ae038b1a3cedb6a6e40c685c2439140fdf1db406cc6b3bfa251bf326175`。
- 这些属于当前用户请求的未提交改动；此分支没有要求现在提交或推送。手机端 renderer helper 返回 `Adreno (TM) 830v1`；实体设置界面尚未目视验证，Resources GPU 名称仍可能 N/A。软件包安装后未重启或重启桌面会话。

## 9. 当前 Antigravity 工作树注意事项

修改在 `C:\Users\Phorus\.gemini\antigravity\scratch\Project-SunUEFI`，当前批次尚未提交。除此以外还存在与本任务无关的未提交改动（Novatek Piano pen ini、音频 service symlink、`Xiaomi Pad 8 Pro.conf`、`upstream/Mu-Silicium` 与 `upstream/simple-init` submodule 状态等）；先检查 `git status --short --untracked-files=all`，只处理本任务明确涉及文件，不要清理或覆盖其他改动。

## 10. 要完成的屏幕任务

只处理 Linux 内屏从 DPMS/idle off 恢复黑屏的根因，按“只读对照→提出一条由证据支持的假设→一次改动→完整构建→校验 Image/DTB→用户观察实机→保存证据”的顺序推进。保留 Android、当前稳定 UEFI、用户数据和其他已验证功能。先恢复当前设备可用状态并记录冷启动基线，再针对 DPU command-mode kickoff/frame_done 与 panel `prepare/enable/unprepare` 顺序做对照。不要宣称修复成功，直到用户确认实体屏幕从熄屏自动亮回桌面，且触控、Wi-Fi 和 SSH 均正常。
