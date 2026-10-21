"""Toolchain ~/ paths expand for the actual account running the build."""
from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('homepath_build', ROOT / 'tools/build.py')
b = importlib.util.module_from_spec(spec)
spec.loader.exec_module(b)


class ToolchainHomePathTests(unittest.TestCase):
    def test_home_is_selected_at_runtime_not_hardcoded(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            for username in ('developer-one', 'developer-two'):
                home = base / username
                home.mkdir()
                environment_key = 'USERPROFILE' if os.name == 'nt' else 'HOME'
                with mock.patch.dict(os.environ, {environment_key: str(home)}):
                    self.assertEqual(b._expand_home_path('~/sdk/bin'), str(home / 'sdk/bin'))
                    self.assertEqual(
                        b._expand_home_paths(['~/sdk/pc', '~/sdk/extra-pc']),
                        os.pathsep.join((str(home / 'sdk/pc'), str(home / 'sdk/extra-pc'))),
                    )
                    self.assertEqual(
                        b._expand_home_flags(('-I~/sdk/include', '-L', '~/sdk/lib', '--sysroot=~/sysroot')),
                        (f'-I{home}/sdk/include', '-L', str(home / 'sdk/lib'), f'--sysroot={home}/sysroot'),
                    )

    @unittest.skipUnless(os.name == 'posix', 'Linux toolchain cross-environment test')
    def test_prepare_tools_expands_each_target_path(self):
        with tempfile.TemporaryDirectory() as directory:
            home = Path(directory) / 'some-username'
            home.mkdir()
            (home / 'sysroot').mkdir()
            settings = {
                'cxx': 'cross-g++',
                'ar': 'cross-ar',
                'pkg_config': 'cross-pkg-config',
                'pkg_config_libdir': ['~/sdk/lib/pkgconfig', '~/extra/pkgconfig'],
                'pkg_config_sysroot_dir': '~/sysroot',
                'sysroot': '~/sysroot',
                'dll_dirs': ['~/sdk/bin'],
                'lib_dirs': ['~/sdk/lib'],
                'cxxflags': ['-I~/sdk/include'],
                'ldflags': ['-L~/sdk/lib'],
            }

            def fake_pc(argv, *, env, **kwargs):
                self.assertEqual(env['PKG_CONFIG_LIBDIR'], os.pathsep.join([
                    str(home / 'sdk/lib/pkgconfig'), str(home / 'extra/pkgconfig')
                ]))
                self.assertEqual(env['PKG_CONFIG_SYSROOT_DIR'], str(home / 'sysroot'))
                return mock.Mock(returncode=0, stdout='-I/target/include\n' if '--cflags' in argv else '-lSDL2\n', stderr='')

            with mock.patch.dict(os.environ, {'HOME': str(home)}), \
                 mock.patch.object(b, '_local_toolchain_config', return_value=settings), \
                 mock.patch.object(b, '_tool', side_effect=lambda name, required=True: name), \
                 mock.patch.object(b, 'command', side_effect=lambda argv, **_: 'x86_64-w64-mingw32' if '-dumpmachine' in argv else 'compiler v1'), \
                 mock.patch.object(b.subprocess, 'run', side_effect=fake_pc):
                tc = b.prepare_tools('windows')

            self.assertEqual(tc.lib_dirs, (home / 'sdk/lib',))
            self.assertEqual(tc.dll_dirs, (home / 'sdk/bin',))
            self.assertEqual(tc.sysroot, home / 'sysroot')
            self.assertEqual(tc.cxxflags, (f'-I{home}/sdk/include',))
            self.assertEqual(tc.ldflags, (f'-L{home}/sdk/lib',))


if __name__ == '__main__':
    unittest.main()
