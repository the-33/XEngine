"""Guard against swallowed engine/game build logs in benchmark sweeps."""
from __future__ import annotations

from contextlib import redirect_stdout, redirect_stderr
from io import StringIO
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import sweep_render_threshold as threshold
import sweep_sprite_projection as projection


class BenchmarkBuildLoggingTests(unittest.TestCase):
    def test_live_output_matches_saved_log_including_stderr(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'build.log'
            command = [sys.executable, '-u', '-c',
                       'import sys; '
                       'print("[BUILD] game XEngine Benchmark", flush=True); '
                       'print("[ENGINE] compiling Engine.cpp", flush=True); '
                       'print("[GAME] compiling main.cpp", file=sys.stderr, flush=True); '
                       'print("[OK] build succeeded", flush=True)']
            stdout = StringIO()
            with redirect_stdout(stdout):
                return_code = threshold.run_build_with_live_log(
                    command, cwd=Path(temp), log_path=path)
            self.assertEqual(return_code, 0)
            visible = stdout.getvalue()
            saved = path.read_text(encoding='utf-8')
            for line in ('[BUILD] game XEngine Benchmark',
                         '[ENGINE] compiling Engine.cpp',
                         '[GAME] compiling main.cpp', '[OK] build succeeded'):
                self.assertIn(line, visible)
                self.assertIn(line, saved)
            self.assertTrue(visible.endswith(saved))

    def test_failure_is_visible_and_saved(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'build.log'
            command = [sys.executable, '-u', '-c',
                       'import sys; print("fatal: missing SDL2", file=sys.stderr, flush=True); '
                       'sys.exit(7)']
            stdout = StringIO()
            with redirect_stdout(stdout):
                rc = threshold.run_build_with_live_log(command, cwd=Path(temp), log_path=path)
            self.assertEqual(rc, 7)
            self.assertIn('fatal: missing SDL2', stdout.getvalue())
            self.assertIn('fatal: missing SDL2', path.read_text(encoding='utf-8'))

    def test_projection_sweep_uses_live_build_and_stops_on_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp)/'output'
            with (mock.patch.object(projection, 'run_build_with_live_log', return_value=7) as build,
                  mock.patch.object(projection.subprocess, 'call') as run):
                stderr=StringIO()
                with redirect_stderr(stderr):
                    rc=projection.main(['--build', '--output', str(output),
                                        '--workloads', 'rotations_xyz',
                                        '--counts', '256', '--repetitions', '1'])
            self.assertEqual(rc, 1)
            build.assert_called_once()
            self.assertEqual(build.call_args.kwargs['log_path'], output/'build.log')
            run.assert_not_called()
            self.assertIn('Build failed (7)',stderr.getvalue())

    def test_threshold_sweep_uses_same_build_helper(self):
        with tempfile.TemporaryDirectory() as temp:
            output=Path(temp)/'output'
            with (mock.patch.object(threshold,'run_build_with_live_log',return_value=7) as build,
                  mock.patch.object(threshold.subprocess,'call') as run):
                stderr=StringIO()
                with redirect_stderr(stderr):
                    rc=threshold.main(['--build','--output',str(output),
                                       '--counts','256','--repetitions','1'])
            self.assertEqual(rc,1)
            build.assert_called_once()
            self.assertEqual(build.call_args.kwargs['log_path'], output/'build.log')
            run.assert_not_called()
            self.assertIn('Build failed (7)',stderr.getvalue())


if __name__ == '__main__':
    unittest.main()
