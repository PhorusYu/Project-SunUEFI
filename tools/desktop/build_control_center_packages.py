#!/usr/bin/env python3
"""Build Debian GNOME Settings with platform CPU information and renderer fallbacks."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from build_piano_runtime_helpers import source_patches
from desktop import debian_package, package_build

SERIES = ROOT / 'patches/gnome-control-center/51.1/series.json'
RUNTIME = {'gnome-control-center': 'arm64', 'gnome-control-center-data': 'all'}
REVISION = 1


def selection(rootfs):
    target = debian_package.target_identity(rootfs, 'gnome-control-center')
    version = target['binary']['source_version'].split(':', 1)[-1].split('-', 1)[0]
    if version not in json.loads(SERIES.read_text())['versions']:
        raise ValueError('Port the platform information patches to GNOME Settings source ' + version)
    return target


def validate_prepared(source):
    panel = source / 'panels/system/about'
    if ('get_soc_cpu_info' not in (panel / 'cc-about-page.c').read_text()
            or 'gdk_gl_context_realize' not in (panel / 'gnome-control-center-print-renderer.c').read_text()):
        raise ValueError('The prepared Settings source lacks platform information fixes')


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
        output, sysroot, jobs, 'gnome-control-center', source_patches(SERIES), RUNTIME,
        [Path(__file__), SERIES], revision=REVISION, builder_root=builder_root,
        install_dependencies=install_dependencies, validate=validate_prepared)


def main():
    parser = argparse.ArgumentParser(prog='./build.sh control-center', description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument('--sysroot', type=Path, help='Debian sid ARM64 input; cloned before use')
    mode.add_argument('--builder-root', type=Path, help='Existing isolated sid ARM64 builder')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/gnome-control-center')
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

