"""Regression tests: resolve MinGW runtime DLLs via target GCC, not host PATH."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('xengine_runtime_build', ROOT / 'tools' / 'build.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class RuntimeDllTests(unittest.TestCase):
    def test_release_finds_versioned_gcc_runtime_and_transitive_imports(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            exe = root / 'Game.exe'
            exe.write_bytes(b'PE executable')
            stage = root / 'dist'
            stage.mkdir()
            sdk_bin = root / 'sdk' / 'bin'
            sdk_bin.mkdir(parents=True)
            gcc_bin = root / 'mingw' / 'bin'
            gcc_bin.mkdir(parents=True)
            # GCC's runtime isn't located beside the compiler or in dll_dirs.
            gcc_runtime = root / 'mingw' / 'lib' / 'gcc' / '12-posix'
            gcc_runtime.mkdir(parents=True)
            (sdk_bin / 'SDL2.dll').write_bytes(b'sdl win64')
            (gcc_runtime / 'libgcc_s_seh-1.dll').write_bytes(b'mingw gcc')
            (gcc_runtime / 'libstdc++-6.dll').write_bytes(b'mingw stdc++')
            compiler = str(gcc_bin / 'x86_64-w64-mingw32-g++')
            tc = b.Toolchain(compiler, str(gcc_bin / 'ar'), 'pkg-config', [], [], 'version',
                             objdump='cross-objdump', dll_dirs=(sdk_bin,), cross=True)
            imports = {
                'Game.exe': 'DLL Name: SDL2.dll\nDLL Name: libstdc++-6.dll\nDLL Name: KERNEL32.dll',
                'SDL2.dll': 'DLL Name: libgcc_s_seh-1.dll\n',
                'libgcc_s_seh-1.dll': 'DLL Name: KERNEL32.dll\n',
                'libstdc++-6.dll': '',
            }

            def query(argv, **kwargs):
                if len(argv) == 2 and argv[1].startswith('-print-file-name='):
                    dll = argv[1].split('=', 1)[1]
                    return mock.Mock(returncode=0, stdout=str(gcc_runtime / dll) + '\n')
                raise AssertionError(f'Unexpected process invocation: {argv}')

            with mock.patch.object(b, 'command', side_effect=lambda argv, **_: imports[Path(argv[-1]).name]), \
                 mock.patch.object(b.subprocess, 'run', side_effect=query):
                copied = b.package_windows_libs(stage, exe, tooling=tc)

            self.assertCountEqual(copied, ['SDL2.dll', 'libgcc_s_seh-1.dll', 'libstdc++-6.dll'])
            self.assertEqual((stage / 'libgcc_s_seh-1.dll').read_bytes(), b'mingw gcc')
            self.assertEqual((stage / 'libstdc++-6.dll').read_bytes(), b'mingw stdc++')
            self.assertFalse((stage / 'KERNEL32.dll').exists())

    def test_release_skips_windows_unicode_dlls_imported_by_sdl_ttf(self):
        """Windows supplies USP10 and MSCTF; Linux cross-builds must not copy them."""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game = root / 'Game.exe'
            game.write_bytes(b'Windows PE')
            stage = root / 'dist'
            stage.mkdir()
            sdk = root / 'windows-sdk' / 'bin'
            sdk.mkdir(parents=True)
            (sdk / 'SDL2.dll').write_bytes(b'sdl')
            (sdk / 'SDL2_ttf.dll').write_bytes(b'ttf')
            # Even if an unrelated copy is visible, never bundle OS DLLs.
            (sdk / 'USP10.dll').write_bytes(b'untrusted')
            tc = b.Toolchain('cross-g++', 'cross-ar', 'pkg-config', [], [], 'version',
                             objdump='cross-objdump', dll_dirs=(sdk,), cross=True)
            imports = {
                'Game.exe': 'DLL Name: SDL2.dll\nDLL Name: SDL2_ttf.dll\n',
                'SDL2.dll': 'DLL Name: KERNEL32.dll\n',
                'SDL2_ttf.dll': 'DLL Name: USP10.dll\nDLL Name: MsCtF.DlL\n'
                            'DLL Name: NORMALIZ.dll\n',
            }
            with mock.patch.object(b, 'command', side_effect=lambda argv, **_: imports[Path(argv[-1]).name]):
                copied = b.package_windows_libs(stage, game, tooling=tc)
            self.assertCountEqual(copied, ['SDL2.dll', 'SDL2_ttf.dll'])
            self.assertFalse((stage / 'USP10.dll').exists())
            self.assertFalse((stage / 'MsCtF.DlL').exists())
            self.assertFalse((stage / 'NORMALIZ.dll').exists())

    def test_release_fails_on_unknown_missing_third_party_dll(self):
        """Skipping Windows DLLs must not mask a genuine missing SDK dependency."""
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            game = root / 'Game.exe'
            game.write_bytes(b'Windows PE')
            stage = root / 'dist'
            stage.mkdir()
            sdk = root / 'windows-sdk' / 'bin'
            sdk.mkdir(parents=True)
            (sdk / 'SDL2.dll').write_bytes(b'sdl')
            tc = b.Toolchain('cross-g++', 'cross-ar', 'pkg-config', [], [], 'version',
                             objdump='cross-objdump', dll_dirs=(sdk,), cross=True)
            imports = {
                'Game.exe': 'DLL Name: SDL2.dll\n',
                'SDL2.dll': 'DLL Name: MissingExternalLibrary.dll\n',
            }
            with mock.patch.object(b, 'command', side_effect=lambda argv, **_: imports[Path(argv[-1]).name]), \
                 mock.patch.object(b, '_gcc_target_dll', return_value=None):
                with self.assertRaisesRegex(b.BuildError, 'MissingExternalLibrary.dll'):
                    b.package_windows_libs(stage, game, tooling=tc)

    def test_fallback_to_gcc_versioned_libgcc_directory(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            runtime = root / 'gcc' / 'version-posix'
            runtime.mkdir(parents=True)
            (runtime / 'libgcc.a').write_bytes(b'archive')
            dll = runtime / 'libgcc_s_seh-1.dll'
            dll.write_bytes(b'runtime')

            def query(argv, **kwargs):
                result = ('libgcc_s_seh-1.dll' if '-print-file-name=' in argv[-1]
                          else str(runtime / 'libgcc.a'))
                return mock.Mock(returncode=0, stdout=result + '\n')

            with mock.patch.object(b.subprocess, 'run', side_effect=query):
                self.assertEqual(b._gcc_target_dll('cross-g++', dll.name), dll)

    def test_does_not_trust_unresolved_or_host_dll(self):
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            unrelated = root / 'old_mingw'
            unrelated.mkdir()
            (unrelated / 'libgcc_s_seh-1.dll').write_bytes(b'unrelated runtime')

            with mock.patch.object(b.subprocess, 'run', return_value=mock.Mock(returncode=0,
                 stdout='libgcc_s_seh-1.dll\n')):
                self.assertIsNone(b._gcc_target_dll('cross-g++', 'libgcc_s_seh-1.dll'))


if __name__ == '__main__':
    unittest.main()
