#!/usr/bin/env python3
"""Build an interactive RAM-boot Android v2 image for Redmi K80 Pro (miro)."""

import argparse
import re
import shutil
import struct
import subprocess
from pathlib import Path


INIT_SCRIPT = """#!/bin/sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin
/bin/busybox --install -s /bin

mkdir -p /proc /sys /dev /dev/pts /tmp /mnt
mount -t proc proc /proc
mount -t sysfs sysfs /sys
mount -t devtmpfs devtmpfs /dev
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mount -t debugfs debugfs /sys/kernel/debug 2>/dev/null || true

# Bring up USB NCM gadget on a600000.usb (192.168.77.1/24) + telnetd
UDC=""
for i in $(seq 1 20); do
    UDC=$(ls /sys/class/udc 2>/dev/null | head -n 1)
    [ -n "$UDC" ] && break
    usleep 100000
done

if [ -n "$UDC" ]; then
    mkdir -p /sys/kernel/config
    mount -t configfs configfs /sys/kernel/config 2>/dev/null || true
    G=/sys/kernel/config/usb_gadget/miro
    mkdir -p "$G"
    echo 0x18d1 > "$G/idVendor"
    echo 0xd001 > "$G/idProduct"
    echo 0x0200 > "$G/bcdUSB"
    mkdir -p "$G/strings/0x409"
    echo "Xiaomi" > "$G/strings/0x409/manufacturer"
    echo "Redmi K80 Pro (miro) Linux" > "$G/strings/0x409/product"
    echo "miro-linux" > "$G/strings/0x409/serialnumber"
    mkdir -p "$G/functions/ncm.usb0"
    mkdir -p "$G/configs/b.1"
    ln -sf "$G/functions/ncm.usb0" "$G/configs/b.1/ncm.usb0"
    echo "$UDC" > "$G/UDC"
    for i in $(seq 1 20); do
        [ -d /sys/class/net/usb0 ] && break
        usleep 100000
    done
    ip addr add 192.168.77.1/24 dev usb0 2>/dev/null || ifconfig usb0 192.168.77.1 netmask 255.255.255.0 up
    ip link set usb0 up 2>/dev/null || true
    mkdir -p /etc /var/lib/misc
    printf 'start 192.168.77.2
end 192.168.77.10
interface usb0
opt subnet 255.255.255.0
opt lease 86400
' >/etc/udhcpd.conf
    touch /var/lib/misc/udhcpd.leases
    udhcpd /etc/udhcpd.conf 2>/dev/null || true
    telnetd -l /bin/sh -p 23 2>/dev/null || true
fi

# Stop kernel messages from scrolling the status screen away
dmesg -n 1 2>/dev/null || true

while true; do
{
    printf '\033[2J\033[H'
    echo "=========================================="
    echo " Project-SunUEFI: Redmi K80 Pro (miro)"
    echo "=========================================="
    echo "Uptime      : $(cut -d' ' -f1 /proc/uptime)s"
    echo "CPUs online : $(cat /sys/devices/system/cpu/online 2>/dev/null)"
    echo "USB Gadget  : ${UDC:-NONE} state=$(cat /sys/class/udc/*/state 2>/dev/null) speed=$(cat /sys/class/udc/*/current_speed 2>/dev/null)"
    echo "UFS disks   : $(ls /dev/sd? 2>/dev/null | tr '\n' ' ')"
    echo "Net ifaces  : $(ls /sys/class/net 2>/dev/null | tr '\n' ' ')"
    echo "DRM devices : $(ls /dev/dri 2>/dev/null | tr '\n' ' ')"
    echo "------------- USB (last 8) ---------------"
    dmesg | grep -i -E 'dwc3|eusb2|repeater|a600000|88e3000|gadget|udc' | tail -n 8 | cut -c1-90
    echo "------------- Wi-Fi (last 6) -------------"
    dmesg | grep -E 'ath12k|pwrseq|wcn|mhi|qmp|aoss' | tail -n 6 | cut -c1-90
    echo "=========================================="
} >/dev/tty0 2>&1
sleep 5
done
"""


def find_phandle(dts_text: str, node_pattern: str) -> str:
    m = re.search(node_pattern + r".*?phandle\s*=\s*<(0x[0-9a-fA-F]+)>", dts_text, re.S)
    if not m:
        raise RuntimeError(f"phandle not found for {node_pattern}")
    return m.group(1)


def build_miro_dtb(src_dtb: Path, out_dtb: Path, work_dir: Path) -> None:
    dts_file = work_dir / "miro-ram.dts"
    subprocess.run(["dtc", "-I", "dtb", "-O", "dts", "-o", str(dts_file), str(src_dtb)], check=True)
    dts_text = dts_file.read_text()
    tlmm_ph = find_phandle(dts_text, r"pinctrl@f100000\s*\{")
    l2g_ph = find_phandle(dts_text, r'regulator-name\s*=\s*"vreg_l2g_1p8";')
    l12b_ph = find_phandle(dts_text, r'regulator-name\s*=\s*"vreg_l12b_1p8";')
    pm8550_gpio_ph = find_phandle(dts_text, r'compatible\s*=\s*"qcom,pm8550-gpio"')
    aoss_qmp_ph = find_phandle(dts_text, r"power-management@c300000\s*\{")

    # Retain the installed UEFI memory-fixup compatible while identifying miro first.
    overlay = f"""
/ {{
	model = "Xiaomi Redmi K80 Pro";
	compatible = "xiaomi,miro", "xiaomi,piano", "qcom,sm8750";
	display_panel_vddd: regulator-vddd {{
		compatible = "regulator-fixed";
		regulator-name = "display_panel_vddd";
		regulator-min-microvolt = <1060000>;
		regulator-max-microvolt = <1060000>;
		gpio = <{tlmm_ph} 120 0>;
		enable-active-high;
		regulator-boot-on;
		regulator-always-on;
		phandle = <0x7fff>;
	}};

	chosen {{
		#address-cells = <0x02>;
		#size-cells = <0x02>;
		ranges;

		framebuffer@fc800000 {{
			compatible = "simple-framebuffer";
			reg = <0x00 0xfc800000 0x00 0x02b00000>;
			width = <1440>;
			height = <3200>;
			stride = <5760>;
			format = "a8r8g8b8";
			status = "okay";
		}};
	}};

	regulator-vsp {{ status = "disabled"; }};
	regulator-vsn {{ status = "disabled"; }};

	gpio-keys {{
		compatible = "gpio-keys";

		vol-up {{
			label = "Volume Up";
			linux,code = <115>;
			gpios = <{pm8550_gpio_ph} 6 1>;
			debounce-interval = <15>;
		}};
	}};

	wcn7850-pmu {{
		compatible = "qcom,wcn7861-pmu";
		qcom,qmp = <{aoss_qmp_ph}>;
		qcom,pdc-init-table =
			"{{class: wlan_pdc, ss: rf, res: s5f.m, enable: 1}}",
			"{{class: wlan_pdc, ss: rf, res: s5f.v, enable: 1}}",
			"{{class: wlan_pdc, ss: rf, res: s5f.v, upval: 876}}",
			"{{class: wlan_pdc, ss: rf, res: s5f.v, dwnval: 876}}";
		qcom,vreg-pdc-map =
			"s5f:rf", "s4d:bb", "s3g:rf", "s7i:rf";
		qcom,pmu-vreg-map =
			"VDDD_AON_0P9:s5f", "VDDA_RFA_1P9:s3g",
			"VDDA_RFA_1P3:s7i", "VDDA_RFA_0P9:s5f",
			"VDDD_WLMX_0P9:s4d", "VDDD_WLCX_0P9:s5f",
			"VDDD_BTCX_0P9:s5f", "VDDD_BTCMX_0P9:s5f",
			"VDDA_PCIE_1P2:s7i", "VDDA_PCIE_0P9:s7i";
	}};

	reserved-memory {{
		#address-cells = <0x02>;
		#size-cells = <0x02>;
		ranges;

		gunyah-hyp-extra@80e40000 {{ reg = <0x00 0x80e40000 0x00 0x003c0000>; no-map; }};
		adspslpi@9ee00000 {{ reg = <0x00 0x9ee00000 0x00 0x04580000>; no-map; }};
		display_demura@a3500000 {{ reg = <0x00 0xa3500000 0x00 0x04100000>; no-map; }};
		shm@fc000000 {{ reg = <0x00 0xfc000000 0x00 0x00400000>; no-map; }};
		uefi_fb@fc800000 {{ reg = <0x00 0xfc800000 0x00 0x02000000>; no-map; }};
		uefi_fb_extra@fe800000 {{ reg = <0x00 0xfe800000 0x00 0x00b00000>; no-map; }};
	}};

	soc@0 {{
		pinctrl@f100000 {{
			gpio-reserved-ranges = <36 4 48 4 74 1>;

			sde_te_active: sde-te-active-state {{
				pins = "gpio86";
				function = "mdp_vsync";
				drive-strength = <2>;
				bias-pull-down;
				phandle = <0x7ffe>;
			}};

			ts_irq_active: ts-irq-active-state {{
				pins = "gpio162";
				function = "gpio";
				drive-strength = <2>;
				bias-pull-up;
				input-enable;
				phandle = <0x7ffd>;
			}};
		}};

		dma-controller@a00000 {{
			status = "okay";
		}};

		geniqup@8c0000 {{
			status = "okay";
			serial@898000 {{
				bluetooth {{
					compatible = "qcom,wcn7861-bt";
					max-speed = <3200000>;
				}};
			}};
		}};

		geniqup@ac0000 {{
			i2c@a94000 {{
				status = "okay";
				clock-frequency = <100000>;

				battery@55 {{
					compatible = "ti,bq27z561";
					reg = <0x55>;
				}};
			}};

			spi@a88000 {{
				status = "okay";

				touchscreen@0 {{
					compatible = "focaltech,ft3683-spi";
					reg = <0>;
					spi-max-frequency = <10000000>;
					interrupt-parent = <{tlmm_ph}>;
					interrupts = <162 2>;
					pinctrl-names = "default";
					pinctrl-0 = <0x7ffd>;
					iovdd-supply = <{l12b_ph}>;
					avdd-gpios = <{tlmm_ph} 114 0>;
					reset-gpios = <{tlmm_ph} 161 1>;
					/delete-property/ firmware-name;
					/delete-property/ panel;
					/delete-property/ novatek,irq-gpios;
					/delete-property/ novatek,lcd-id-gpios;
					/delete-property/ pinctrl-1;
				}};
			}};
		}};

		usb@a600000 {{
			/delete-property/ usb-role-switch;
			phys = <0xab>;
			phy-names = "usb2-phy";
			dr_mode = "peripheral";
			maximum-speed = "high-speed";
			qcom,select-utmi-as-pipe-clk;
			status = "okay";
		}};

		ufs@1d84000 {{
			/delete-property/ qcom,ice;
		}};

		display-subsystem@ae00000 {{
			status = "okay";
			display-controller@ae01000 {{
				pinctrl-names = "default";
				pinctrl-0 = <0x7ffe>;
			}};
			dsi@ae94000 {{
				panel@0 {{
					compatible = "xiaomi,miro-o11u";
					reset-gpios = <{tlmm_ph} 98 1>;
					vddio-supply = <{l2g_ph}>;
					vdd-supply = <0x7fff>;
					/delete-property/ vci-supply;
				}};
			}};
		}};
		gpu@3d00000 {{
			status = "okay";
			zap-shader {{
				firmware-name = "qcom/gen80000_zap.mbn";
			}};
		}};
		soundwire@6ab0000 {{ status = "disabled"; }};
		soundwire@6ad0000 {{ status = "disabled"; }};
		soundwire@6b10000 {{ status = "disabled"; }};
		soundwire@7630000 {{ status = "disabled"; }};
		spmi@c400000 {{
			pmic@0 {{
				pon@1300 {{
					pwrkey {{
						status = "okay";
					}};
					resin {{
						linux,code = <114>;
						status = "okay";
					}};
				}};
			}};
		}};
		isp@ac04000 {{ status = "disabled"; }};
		video-codec@aa00000 {{ status = "disabled"; }};
		pinctrl@7760000 {{ status = "disabled"; }};
		mmc@8804000 {{ status = "disabled"; }};
	}};
}};
"""
    dts_file.write_text(dts_text + overlay)
    subprocess.run(["dtc", "-I", "dts", "-O", "dtb", "-o", str(out_dtb), str(dts_file)], check=True)


def build_initramfs(busybox: Path, out_cpio: Path, work_dir: Path, fw_dir: Path | None = None) -> None:
    root = work_dir / "initramfs"
    if root.exists():
        shutil.rmtree(root)
    for d in ("bin", "sbin", "dev", "proc", "sys", "tmp", "mnt", "etc", "lib/firmware"):
        (root / d).mkdir(parents=True, exist_ok=True)
    if fw_dir and fw_dir.exists():
        shutil.copytree(fw_dir, root / "lib" / "firmware", dirs_exist_ok=True)
    shutil.copy2(busybox, root / "bin" / "busybox")
    (root / "bin" / "busybox").chmod(0o755)
    (root / "bin" / "sh").symlink_to("busybox")
    init = root / "init"
    init.write_text(INIT_SCRIPT)
    init.chmod(0o755)
    subprocess.run(
        "find . -print0 | cpio --null -o --format=newc | gzip -9 > " + str(out_cpio),
        cwd=root,
        shell=True,
        check=True,
    )


def pack_boot_v2(kernel: Path, ramdisk: Path, dtb: Path, out_img: Path) -> None:
    page = 4096
    def pad(b: bytes) -> bytes:
        r = len(b) % page
        return b + (b"\x00" * (page - r) if r else b"")

    k = kernel.read_bytes()
    r = ramdisk.read_bytes()
    d = dtb.read_bytes()
    hdr = bytearray(page)
    struct.pack_into(
        "<8s10I16s512s8I1024sIQIIQ",
        hdr,
        0,
        b"ANDROID!",
        len(k), 0xA8000000,
        len(r), 0xB0200000,
        0, 0,
        0xA8000000,
        page,
        2,
        0,
        b"miro-linux-ram",
        b"console=tty0 consoleblank=0 loglevel=7 ignore_loglevel clk_ignore_unused pd_ignore_unused regulator_ignore_unused iommu.passthrough=1 panic=20",
        0, 0, 0, 0, 0, 0, 0, 0,
        b"",
        0, 0, 1660,
        len(d), 0xB0000000,
    )
    out_img.write_bytes(bytes(hdr) + pad(k) + pad(r) + pad(d))
    print(f"Wrote {out_img} ({out_img.stat().st_size} bytes)")


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    parser = argparse.ArgumentParser(description="Build interactive RAM-boot image for miro")
    parser.add_argument("--kernel", type=Path, default=Path("/root/miro-linux-test/out/arch/arm64/boot/Image"))
    parser.add_argument("--src-dtb", type=Path, default=Path("/root/miro-linux-test/out/arch/arm64/boot/dts/qcom/sm8750-xiaomi-piano.dtb"))
    parser.add_argument("--busybox", type=Path, default=repo / "build" / "miro-linux" / "busybox")
    parser.add_argument("--firmware-dir", type=Path, default=repo / "build" / "miro-linux" / "firmware" / "lib" / "firmware")
    parser.add_argument("--out-dir", type=Path, default=repo / "build" / "miro-linux")
    args = parser.parse_args()

    args.out_dir.mkdir(parents=True, exist_ok=True)
    out_dtb = args.out_dir / "sm8750-xiaomi-miro.dtb"
    out_cpio = args.out_dir / "initramfs.cpio"
    out_img = args.out_dir / "MiroLinux-ram-bootv2.img"

    build_miro_dtb(args.src_dtb, out_dtb, args.out_dir)
    build_initramfs(args.busybox, out_cpio, args.out_dir, args.firmware_dir)
    pack_boot_v2(args.kernel, out_cpio, out_dtb, out_img)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
