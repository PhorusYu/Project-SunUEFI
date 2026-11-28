#!/usr/bin/env python3
"""Build Debian Resources with platform CPU information and thermal sensors."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from build_piano_runtime_helpers import source_patches
from desktop import debian_package, package_build

SERIES = ROOT / 'patches/resources/51.0/series.json'
RUNTIME = {'resources': 'arm64'}
REVISION = 3


def selection(rootfs):
    target = debian_package.target_identity(rootfs, 'resources')
    version = target['binary']['source_version'].split('-', 1)[0]
    if version not in json.loads(SERIES.read_text())['versions']:
        raise ValueError('Port the platform sensor patches to Resources source ' + version)
    return target


def validate_prepared(source):
    devices = source / 'src/devices'
    if (not (devices / 'platform.rs').is_file()
            or 'devfreq_core_frequency' not in (devices / 'gpu/other.rs').read_text()
            or 'platform::cpu_model_name' not in (devices / 'cpu.rs').read_text()):
        raise ValueError('The prepared Resources source lacks platform information support')


def reusable(output, rootfs):
    target = selection(rootfs)
    patches = [{'path': path.relative_to(ROOT).as_posix(), 'sha256': debian_package.sha(path)}
               for path in source_patches(SERIES)]
    inputs = debian_package.input_records([Path(__file__), SERIES, Path(debian_package.__file__),
                                          Path(package_build.__file__)])
    return debian_package.reusable(Path(output), target, patches, RUNTIME, inputs, REVISION)


def build(output, sysroot, jobs, builder_root=None, install_dependencies=True):
    selection(sysroot or builder_root)
    return debian_package.build(
        output, sysroot, jobs, 'resources', source_patches(SERIES), RUNTIME,
        [Path(__file__), SERIES], revision=REVISION, builder_root=builder_root,
        install_dependencies=install_dependencies, validate=validate_prepared)


def main():
    parser = argparse.ArgumentParser(prog='./build.sh resources', description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--sysroot', type=Path, help='Debian sid ARM64 input; cloned before use')
    mode.add_argument('--builder-root', type=Path, help='Existing isolated sid ARM64 builder')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/resources')
    parser.add_argument('--jobs', type=int, default=package_build.default_jobs())
    parser.add_argument('--skip-install-dependencies', action='store_true')
    parser.add_argument('--worker', action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()
    if not args.worker:
        return subprocess.run(['unshare', '--mount', '--propagation', 'private',
                               sys.executable, __file__, *sys.argv[1:], '--worker']).returncode
    try:
        print(json.dumps(build(args.output, args.sysroot, args.jobs, args.builder_root,
                               not args.skip_install_dependencies), indent=2))
    except (ValueError, OSError, subprocess.SubprocessError, StopIteration) as error:
        parser.exit(2, str(error) + '\n')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
