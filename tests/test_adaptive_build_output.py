"""The paired runner must tee --build diagnostics without spamming benchmark logs."""
from __future__ import annotations

from contextlib import redirect_stdout
import io
from pathlib import Path
import sys
import tempfile
from unittest import mock
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import compare_adaptive_broadphase as paired


class AdaptiveBuildOutputTests(unittest.TestCase):
    def test_streaming_tees_stdout_and_stderr_to_console_and_build_log(self):
        with tempfile.TemporaryDirectory() as directory:
            logfile = Path(directory) / 'build.log'
            output = io.StringIO()
            command = [sys.executable, '-u', '-c',
                       'import sys; print("[BUILD] updated", flush=True); '
                       'print("[CACHE] unchanged", file=sys.stderr, flush=True)']
            with redirect_stdout(output):
                paired.run_command(command, logfile, stream=True)
            self.assertIn('[BUILD] updated', output.getvalue())
            self.assertIn('[CACHE] unchanged', output.getvalue())
            content = logfile.read_text(encoding='utf-8')
            self.assertTrue(content.startswith('$ '))
            self.assertIn('[BUILD] updated', content)
            self.assertIn('[CACHE] unchanged', content)

    def test_default_benchmark_capture_remains_silent(self):
        with tempfile.TemporaryDirectory() as directory:
            logfile = Path(directory) / 'benchmark.log'
            output = io.StringIO()
            with redirect_stdout(output):
                paired.run_command([sys.executable, '-u', '-c',
                                    'print("benchmark diagnostics")'], logfile)
            self.assertEqual(output.getvalue(), '')
            self.assertIn('benchmark diagnostics', logfile.read_text(encoding='utf-8'))

    def test_streaming_failure_reports_file_and_exit_code(self):
        with tempfile.TemporaryDirectory() as directory:
            logfile = Path(directory) / 'build.log'
            output = io.StringIO()
            with redirect_stdout(output):
                with self.assertRaisesRegex(RuntimeError, r'Command failed \(7\)') as raised:
                    paired.run_command([sys.executable, '-u', '-c',
                                        'print("compilation failed", flush=True); '
                                        'raise SystemExit(7)'], logfile, stream=True)
            self.assertIn('compilation failed', output.getvalue())
            self.assertIn('compilation failed', logfile.read_text(encoding='utf-8'))
            self.assertIn(str(logfile), str(raised.exception))

    def test_build_flag_uses_unbuffered_python_and_streaming(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / 'results'
            executable = Path(directory) / 'fake.exe'
            executable.write_bytes(b'not an actual SDL executable')
            calls = []

            def fake_run(command, logfile, *, stream=False):
                calls.append((command, logfile, stream))
                if len(calls) > 1:
                    raise RuntimeError('stop after verifying build invocation')

            with mock.patch.object(paired, 'run_command', side_effect=fake_run):
                with redirect_stdout(io.StringIO()):
                    with mock.patch('sys.stderr', io.StringIO()):
                        result = paired.main(['--build', '--exe', str(executable),
                                              '--output', str(output), '--scenarios', 'light',
                                              '--modes', 'single', '--repetitions', '1',
                                              '--frames', '1', '--warmup', '0'])
            self.assertEqual(result, 1)  # Stub stops before running the engine.
            self.assertEqual(len(calls), 2)
            self.assertEqual(calls[0][0][1], '-u')
            self.assertEqual(calls[0][1], output / 'build.log')
            self.assertTrue(calls[0][2])
            self.assertFalse(calls[1][2])


if __name__ == '__main__':
    unittest.main()
