# 许可证范围说明

**SunUEFI-miro** 基于 [slwyts/Project-SunUEFI](https://github.com/slwyts/Project-SunUEFI) 独立维护，专用于 **Redmi K80 Pro（`miro`，SM8750）**。本项目保留上游及第三方代码原有的全部版权声明、SPDX 标识与开源许可条款。

SunUEFI-miro 在 `patches/linux/`、`tools/patches/` 及其子目录中的内核相关贡献默认采用 GNU 通用公共许可证第 2 版或任何后续版本（`GPL-2.0-or-later`）。有明确许可的内容适用各自条款。来自 Linux 内核或其他项目的内容保留原有许可、版权声明及署名。完整条款和贡献者名单见[内核补丁目录许可证](patches/linux/LICENSE)。

仓库其他目录采用已有的文件级许可。

## 已有声明

| 声明 | 已核对的代表文件 | 范围说明 |
| --- | --- | --- |
| BSD-2-Clause-Patent | [PianoProductCore.c](uefi/core/PianoProductCore.c)、[PianoEarlyMemory.c](uefi/handoff/early-memory/PianoEarlyMemory.c) | 适用各文件已有声明，不覆盖全部 UEFI 代码。 |
| GPL-2.0-only | [PianoKeys.c](uefi/core/PianoKeys.c)、[PianoPmicMetadata.c](uefi/core/PianoPmicMetadata.c) | 保留已有标识和参考来源。 |
| GPL-2.0-or-later | [PianoTouchProbe.c](uefi/core/PianoTouchProbe.c) | 保留已有标识；与 GPL-2.0-only 分别记录。 |
| LGPL-3.0-or-later | [simple-init runtime](uefi/components/product-pump/simple-init/src/gui/piano_product_runtime.c) 及其头文件 | 适用这些文件，不覆盖整个固件或所有工具。 |
| BSD-3-Clause | [Linux DT overlays](linux/dts/piano-linux-owned-dma.dtso) 等带相同标识的文件 | 以文件自身声明为准。 |

上表列出代表文件。其他文件按各自 SPDX、版权和来源声明处理。许可正文可按 ID 查阅 [SPDX License List](https://spdx.org/licenses/)。第三方与二进制材料说明见 [THIRD_PARTY.md](THIRD_PARTY.md)。
