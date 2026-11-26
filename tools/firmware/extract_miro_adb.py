#!/usr/bin/env python3
"""Extract Redmi K80 Pro (miro) Wi-Fi 7 and Adreno 830 firmware via rooted ADB.

Stages the proprietary blobs under build/miro-linux/firmware/lib/firmware/
for consumption by tools/build_miro_ram_boot.py without committing vendor
binaries to git.
"""

import argparse
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_OUT = ROOT / "build" / "miro-linux" / "firmware" / "lib" / "firmware"

# (device_src_path, relative_dest_path)
FIRMWARE_MAP = (
    # WCN7861 (Peach hw2.0) Wi-Fi 7
    ("/vendor/firmware_mnt/image/peach/amss20.bin", "ath12k/PEACH/hw2.0/amss.bin"),
    ("/vendor/firmware_mnt/image/peach/aux_ucode20.elf", "ath12k/PEACH/hw2.0/aux_ucode.bin"),
    ("/vendor/firmware_mnt/image/peach/bd_o11u.elf", "ath12k/PEACH/hw2.0/board.bin"),
    ("/vendor/firmware_mnt/image/peach/phy_ucode20.elf", "ath12k/PEACH/hw2.0/m3.bin"),
    ("/vendor/firmware_mnt/image/peach/qdss_trace_config_v2.cfg", "ath12k/PEACH/hw2.0/qdss_trace_config.bin"),
    ("/vendor/firmware_mnt/image/peach/regdb_xiaomi.bin", "ath12k/PEACH/hw2.0/regdb.bin"),
    ("/vendor/firmware_mnt/image/tmel_peach_20.elf", "ath12k/PEACH/hw2.0/tmel.bin"),
    # Adreno 830 (gen80000) + miro-signed zap shader
    ("/vendor/firmware/gen80000_aqe.fw", "qcom/gen80000_aqe.fw"),
    ("/vendor/firmware/gen80000_sqe.fw", "qcom/gen80000_sqe.fw"),
    ("/vendor/firmware/gen80000_gmu.bin", "qcom/gen80000_gmu.bin"),
    ("/odm/firmware/gen80000_zap.mbn", "qcom/gen80000_zap.mbn"),
)


def adb_pull_root(adb: str, serial: str | None, src: str, dst: Path) -> None:
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp_name = f"/data/local/tmp/_miro_fw_{dst.name}"
    base = [adb] + (["-s", serial] if serial else [])
    subprocess.run(
        base + ["shell", f"su -c 'cp {src} {tmp_name} && chmod 644 {tmp_name}'"],
        check=True,
    )
    try:
        subprocess.run(base + ["pull", tmp_name, str(dst)], check=True)
    finally:
        subprocess.run(base + ["shell", f"rm -f {tmp_name}"], check=False)


def main() -> int:
    parser = argparse.ArgumentParser(description="Extract miro firmware blobs over rooted ADB")
    parser.add_argument("--adb", default="adb", help="adb executable path")
    parser.add_argument("--serial", default=None, help="optional adb device serial")
    parser.add_argument("--out-dir", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()

    with tempfile.TemporaryDirectory() as td:
        stage = Path(td)
        for src, rel in FIRMWARE_MAP:
            tmp_dst = stage / rel
            print(f"Pulling {src} -> {rel}")
            adb_pull_root(args.adb, args.serial, src, tmp_dst)
            final_dst = args.out_dir / rel
            final_dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(tmp_dst, final_dst)

    print(f"Staged {len(FIRMWARE_MAP)} firmware files under {args.out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
