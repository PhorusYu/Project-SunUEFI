#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""Build the Redmi K80 Pro AudioReach topology using local templates."""

import argparse
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[2]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--topology-source", type=Path, required=True,
        help="AudioReach template directory containing audioreach/audioreach.m4",
    )
    parser.add_argument(
        "--output", type=Path,
        default=ROOT / "build/miro-audio/Xiaomi Redmi K80 Pro-tplg.bin",
        help="Output topology binary (default: build/miro-audio/Xiaomi Redmi K80 Pro-tplg.bin)",
    )
    args = parser.parse_args()
    source = args.topology_source.resolve()
    if not (source / "audioreach/audioreach.m4").is_file():
        parser.error(f"AudioReach templates not found in {source}: missing audioreach/audioreach.m4")
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    config = output.with_suffix(".conf")
    topology = ROOT / "linux/bsp/topology/miro.m4"
    try:
        with config.open("w") as stream:
            subprocess.run(["m4", "-I", str(source), str(topology)],
                           cwd=source, stdout=stream, check=True)
        # The miro ADSP firmware requires the legacy audio-interface module ID.
        config.write_text(re.sub(r"0x0700117c", "0x0700100e", config.read_text(), flags=re.IGNORECASE))
        subprocess.run(["alsatplg", "-c", str(config), "-o", str(output)], check=True)
    except (OSError, subprocess.CalledProcessError) as error:
        print(f"miro-audio: {error}", file=sys.stderr)
        return 1
    print(f"Built {output} ({output.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
