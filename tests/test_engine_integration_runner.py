"""Regression tests for the native SDL2 integration runner's test fixtures."""
from __future__ import annotations

import importlib.util
import io
from pathlib import Path
import subprocess
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock
import wave


ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    'xengine_integration_runner', ROOT / 'tools/test_engine_integration.py')
assert SPEC and SPEC.loader
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class EngineIntegrationRunnerTests(unittest.TestCase):
    def test_captured_log_handles_cp1252_console_without_losing_result(self):
        # Windows UCRT64 Python can default to cp1252 for stdout even when
        # the C++ executable writes UTF-8 log symbols and replacement glyphs.
        sink = io.BytesIO()
        legacy_console = io.TextIOWrapper(sink, encoding='cp1252', errors='strict')
        runner.emit_captured_output('● [LOG] Ready\n⚠ [WARNING]\n✖ [ERROR] \ufffd',
                                    stream=legacy_console)
        legacy_console.flush()
        rendered = sink.getvalue().decode('cp1252')
        self.assertIn(r'\u25cf [LOG] Ready', rendered)
        self.assertIn(r'\u26a0 [WARNING]', rendered)
        self.assertIn(r'\u2716 [ERROR] \ufffd', rendered)
        self.assertTrue(rendered.endswith('\n'))

        utf8_sink = io.BytesIO()
        utf8_console = io.TextIOWrapper(utf8_sink, encoding='utf-8', errors='strict')
        runner.emit_captured_output('● [LOG] Ready\n', stream=utf8_console)
        utf8_console.flush()
        self.assertEqual(utf8_sink.getvalue(), '● [LOG] Ready\n'.encode('utf-8'))

    def test_native_runner_decodes_subprocess_output_as_utf8(self):
        source = (ROOT / 'tools/test_engine_integration.py').read_text(encoding='utf-8')
        self.assertIn("text=True, encoding='utf-8', errors='replace'", source)
        self.assertIn('emit_captured_output(completed.stdout)', source)

    def test_debug_and_release_succeed_with_cp1252_console(self):
        tooling = SimpleNamespace(compiler='g++', cxxflags=[], pkg_cflags=[],
                                  ldflags=[], pkg_libs=[], version='test compiler')
        for configuration in ('Debug', 'Release'):
            with self.subTest(configuration=configuration), \
                 tempfile.TemporaryDirectory(prefix='xengine-native-utf8-') as directory:
                root = Path(directory)
                profile = configuration.lower()
                folder = root / 'build' / 'windows-x86_64' / profile / 'tests' / 'integration'
                folder.mkdir(parents=True)
                sink = io.BytesIO()
                cp1252_stdout = io.TextIOWrapper(sink, encoding='cp1252', errors='strict')
                completed = subprocess.CompletedProcess(
                    [], 0, stdout='● [LOG] Ready\nXENGINE_INTEGRATION_OK: 120 frames\n')
                with mock.patch.object(runner, 'ROOT', root), \
                     mock.patch.object(runner.sys, 'argv', ['test_engine_integration.py',
                                                            '--config', configuration,
                                                            '--frames', '120']), \
                     mock.patch.object(runner.sys, 'stdout', cp1252_stdout), \
                     mock.patch.object(runner.native_build, 'host_platform', return_value='windows'), \
                     mock.patch.object(runner.native_build, 'prepare_tools', return_value=tooling), \
                     mock.patch.object(runner.native_build, 'create_engine', return_value=root / 'lib.a'), \
                     mock.patch.object(runner.native_build, 'common_flags', return_value=[]), \
                     mock.patch.object(runner.native_build, 'compile_sources', return_value=([root / 'x.o'], False)), \
                     mock.patch.object(runner.native_build, 'link_signature', return_value='test'), \
                     mock.patch.object(runner.native_build, 'command'), \
                     mock.patch.object(runner.native_build, 'package_windows_libs'), \
                     mock.patch.object(runner, 'prepare_fixture_assets'), \
                     mock.patch.object(runner.subprocess, 'run', return_value=completed) as run:
                    self.assertEqual(runner.main(), 0)
                    self.assertEqual(run.call_args.kwargs['encoding'], 'utf-8')
                    self.assertEqual(run.call_args.kwargs['errors'], 'replace')
                cp1252_stdout.flush()
                log = sink.getvalue().decode('cp1252')
                self.assertIn(r'\u25cf [LOG] Ready', log)
                self.assertIn('[INTEGRATION] PASS:', log)

    def test_wav_fixtures_are_valid_and_do_not_modify_template(self):
        for existing_audio in (False, True):
            with self.subTest(existing_audio=existing_audio), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                source = root / 'original'
                source.mkdir()
                (source / 'preserved.txt').write_text('unchanged', encoding='utf-8')
                if existing_audio:
                    (source / 'audio').mkdir()
                    (source / 'audio' / '.gitkeep').write_text('', encoding='utf-8')

                destination = root / 'copy'
                runner.prepare_fixture_assets(source, destination)

                self.assertEqual((destination / 'preserved.txt').read_text(), 'unchanged')
                self.assertEqual((source / 'preserved.txt').read_text(), 'unchanged')
                self.assertEqual((source / 'audio').exists(), existing_audio)
                self.assertEqual((destination / 'audio' / '.gitkeep').exists(), existing_audio)

                for filename, expected_samples in (('tone.wav', 2205), ('other.wav', 4410)):
                    with self.subTest(filename=filename), wave.open(
                        str(destination / 'audio' / filename), 'rb'
                    ) as audio:
                        self.assertEqual(audio.getnchannels(), 1)
                        self.assertEqual(audio.getsampwidth(), 2)
                        self.assertEqual(audio.getframerate(), 22050)
                        self.assertEqual(audio.getnframes(), expected_samples)

    def test_sanitizer_ci_is_independent_of_regular_matrix(self):
        workflow = (ROOT / '.github/workflows/test-engine.yml').read_text(
            encoding='utf-8')
        self.assertIn('linux_sanitizers:', workflow)
        self.assertIn('--config Debug --sanitizers --frames 120', workflow)
        self.assertIn('ASAN_OPTIONS: detect_leaks=1:halt_on_error=1', workflow)
        self.assertIn('matrix:\n        configuration: [Debug, Release]', workflow)


if __name__ == '__main__':
    unittest.main()
