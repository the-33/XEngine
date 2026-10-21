"""Regression checks for Windows Debug DLL deployment and benchmark fail-fast."""
from __future__ import annotations

from argparse import Namespace
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]


def module_from_file(name: str, file: Path):
    spec = importlib.util.spec_from_file_location(name, file)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


build = module_from_file('xengine_debug_loader_build', ROOT / 'tools/build.py')
runner = module_from_file('xengine_debug_loader_runner', ROOT / 'tools/run_benchmark.py')


class WindowsRuntimeTests(unittest.TestCase):
    def test_windows_debug_syncs_dlls_even_without_run_and_from_cache(self):
        """Before the fix only --run copied DLLs; the benchmark uses --build."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / 'games/Example'
            game.mkdir(parents=True)
            (game / 'config.json').write_text(json.dumps({'project': {'name': 'Example'}}))
            out = root / 'build/windows-x86_64/debug/games/Example'
            out.mkdir(parents=True)
            executable = out / 'Example.exe'
            executable.write_bytes(b'cached game binary')
            (out / 'GameLink.sha256').write_text('known_signature')
            library = root / 'dummy.a'
            library.write_bytes(b'library')
            generated = out / 'generated/GameBuild.generated.cpp'
            toolchain = build.Toolchain('g++', 'ar', 'pkg-config', [], [], 'gcc-16')
            with mock.patch.object(build, 'ROOT', root), \
                 mock.patch.object(build, 'create_engine', return_value=library), \
                 mock.patch.object(build, 'generate_game_config', return_value=generated), \
                 mock.patch.object(build, 'game_sources', return_value=[]), \
                 mock.patch.object(build, 'compile_sources', return_value=([], False)), \
                 mock.patch.object(build, 'compile_windows_icon', return_value=(None, False)), \
                 mock.patch.object(build, 'link_signature', return_value='known_signature'), \
                 mock.patch.object(build, 'package_windows_libs', return_value=['SDL2.dll']) as copy_dll, \
                 mock.patch.object(build, 'command') as command:
                output = build.game_build('Example', target='windows', profile='debug',
                                          arch='x86_64', tooling=toolchain, run=False)
            self.assertEqual(output, executable)
            copy_dll.assert_called_once_with(out, executable, tooling=toolchain)
            # The pre-build synchronizers now run even when compilation is cached.
            # No compiler/linker invocation should occur for this cached binary.
            self.assertEqual(command.call_count, 2)
            self.assertTrue(command.call_args_list[0].args[0][1].endswith('sync_engine_configs.py'))
            self.assertTrue(command.call_args_list[1].args[0][1].endswith('sync_game_icons.py'))

    def test_windows_debug_replaces_old_dll_and_closes_transitive_imports(self):
        """A stale DLL next to an .exe must not shadow the selected UCRT toolchain."""
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stage = root / 'build'
            stage.mkdir()
            exe = stage / 'Example.exe'
            exe.write_bytes(b'executable')
            (stage / 'SDL2.dll').write_bytes(b'old mingw SDL')
            compiler_bin = root / 'ucrt64/bin'
            compiler_bin.mkdir(parents=True)
            (compiler_bin / 'SDL2.dll').write_bytes(b'correct UCRT SDL')
            (compiler_bin / 'libstdc++-6.dll').write_bytes(b'correct UCRT stdc++')
            tc = build.Toolchain(str(compiler_bin / 'g++'), 'ar', 'pkg-config', [], [],
                                 'GCC 16.2', objdump='objdump')
            imports = {
                'Example.exe': 'DLL Name: SDL2.dll\nDLL Name: KERNEL32.dll',
                'SDL2.dll': 'DLL Name: libstdc++-6.dll\n',
                'libstdc++-6.dll': 'DLL Name: KERNEL32.dll\n',
            }
            with mock.patch.object(build, 'command',
                                   side_effect=lambda args, **kw: imports[Path(args[-1]).name]):
                dlls = build.package_windows_libs(stage, exe, tooling=tc)
            self.assertCountEqual(dlls, ['SDL2.dll', 'libstdc++-6.dll'])
            self.assertEqual((stage / 'SDL2.dll').read_bytes(), b'correct UCRT SDL')
            self.assertEqual((stage / 'libstdc++-6.dll').read_bytes(), b'correct UCRT stdc++')
            self.assertFalse((stage / 'KERNEL32.dll').exists())

    def test_windows_loader_status_signed_or_unsigned(self):
        with mock.patch.object(runner.sys, 'platform', 'win32'):
            for value in (3221225785, -1073741511):
                self.assertIn('0xC0000139', runner.windows_loader_error(value))
                self.assertIn('DLL', runner.windows_loader_error(value))
            self.assertIn('0xC0000135', runner.windows_loader_error(0xC0000135))
            self.assertIsNone(runner.windows_loader_error(0))
            self.assertIsNone(runner.windows_loader_error(123))

    def test_runner_aborts_identical_loader_failure_after_first_process(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            game = root / 'games/XEngine Benchmark'
            config_dir = game / 'assets/config'
            config_dir.mkdir(parents=True)
            original = ROOT / 'games/XEngine Benchmark/assets/config'
            for mode in runner.MODES:
                shutil.copy2(original / f'{mode}.json', config_dir)
            executable = root / 'benchmark.exe'
            executable.write_bytes(b'placeholder')
            results = root / 'results'
            args = Namespace(build=False, exe=executable, scenarios=['light', 'render'],
                             modes=list(runner.MODES), repetitions=3, frames=4,
                             warmup=1, seed=12345, timeout=3, output=results,
                             timing='realtime', fixed_steps=1)
            proc = mock.Mock(returncode=3221225785, stdout='', stderr='DLL entry point missing')
            with mock.patch.object(runner, 'GAME', game), \
                 mock.patch.object(runner, 'arguments', return_value=args), \
                 mock.patch.object(runner, 'windows_loader_error',
                                   return_value='0xC0000139: incompatible DLL'), \
                 mock.patch.object(runner.platform, 'platform', return_value='Windows-test'), \
                 mock.patch.object(runner.subprocess, 'run', return_value=proc) as run:
                rc = runner.main()
            self.assertEqual(rc, 1)
            run.assert_called_once()
            failures = (results / 'failures.txt').read_text()
            self.assertIn('0xC0000139', failures)
            self.assertTrue((results / 'light_single_run01.log').is_file())
            self.assertFalse((results / 'summary.csv').exists())


if __name__ == '__main__':
    unittest.main()
