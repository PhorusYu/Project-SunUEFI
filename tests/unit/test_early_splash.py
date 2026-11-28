"""Render the actual miro early picker against a guarded native framebuffer."""
import platform
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
INC = ROOT / 'upstream/Mu-Silicium/Mu_Basecore/MdePkg/Include'
LIB = ROOT / 'uefi/components/product-support/Library/ProductBootManagerLib'


class EarlySplashTests(unittest.TestCase):
    def test_miro_geometry_and_actual_picker_pixels(self):
        output = ROOT / 'build/previews/early-splash'
        output.mkdir(parents=True, exist_ok=True)
        binding = 'AArch64' if platform.machine() in ('aarch64', 'arm64') else 'X64'
        with tempfile.TemporaryDirectory(prefix='miro-picker-') as temporary:
            executable = Path(temporary) / 'picker'
            subprocess.run([
                'cc', '-U__aarch64__', '-std=gnu11', '-O2', '-fno-builtin',
                '-fshort-wchar', '-Wall', '-Wextra', '-Werror',
                '-Wno-misleading-indentation', '-DPIANO_EARLY_SPLASH',
                '-I' + str(INC), '-I' + str(INC / binding),
                str(ROOT / 'tests/native/MiroEarlySplashTest.c'),
                str(LIB / 'ProductSplash.c'), '-o', str(executable),
            ], check=True)
            subprocess.run([
                str(executable), str(output / 'android.ppm'), str(output / 'uefi.ppm'),
            ], check=True)


if __name__ == '__main__':
    unittest.main()
