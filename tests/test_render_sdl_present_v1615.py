"""Exact SDL-present timing, stale-binary rejection, and build-log separation."""
from __future__ import annotations

from contextlib import redirect_stderr
from io import StringIO
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import render_selector_tails as tails  # noqa: E402
import sweep_render_selector as sweep  # noqa: E402


class ExactSdlPresentTests(unittest.TestCase):
    def test_timing_wraps_present_not_end_cleanup_on_both_backends(self):
        source = (ROOT / 'engine/src/RenderManager.cpp').read_text(encoding='utf-8')
        end = source.split('void RenderManager::End() noexcept', 1)[1].split(
            'void RenderManager::SetVSync(', 1)[0]
        self.assertLess(end.index('mLastSdlPresentMs = 0.f'), end.index('if (!mRenderer) return'))
        self.assertLess(end.index('const auto presentStart'), end.index('mWindow->Present(mRenderer)'))
        self.assertLess(end.index('mWindow->Present(mRenderer)'), end.index('SDL_RenderPresent(mRenderer)'))
        self.assertLess(end.index('SDL_RenderPresent(mRenderer)'), end.index('mLastSdlPresentMs = std::chrono'))
        self.assertLess(end.index('mLastSdlPresentMs = std::chrono'), end.index('SDL_DestroyTexture(t)'))
        self.assertIn('if (mMeasureSdlPresent)', end)

    def test_frame_sample_and_csv_export_use_milliseconds(self):
        engine = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        renderer = (ROOT / 'engine/include/RenderManager.h').read_text(encoding='utf-8')
        profile = (ROOT / 'engine/include/Engine.h').read_text(encoding='utf-8')
        self.assertIn('float mLastSdlPresentMs = 0.f;', renderer)
        self.assertIn('float renderSdlPresentMs = 0.f;', profile)
        self.assertIn('render->mMeasureSdlPresent = mCfg.profileCsv;', engine)
        self.assertIn('sample.renderSdlPresentMs = r->mLastSdlPresentMs;', engine)
        self.assertIn('"render_sdl_present_ms,"', engine)
        self.assertIn("<< ',' << r.renderSdlPresentMs", engine)

    def test_old_capture_can_be_analyzed_but_new_sweep_rejects_it(self):
        row = dict(frame='0', frame_ms='1.0', input_ms='0.01', fixed_ms='0.10',
                   update_ms='0.10', render_present_ms='0.75',
                   render_only_present_ms='0.60', render_world_ms='0.02',
                   render_record_ms='0.01', render_prepare_ms='0.01',
                   render_submit_ms='0.03', render_worker_wait_ms='0.00',
                   render_selector_decision_ms='0.00')
        frames, report = tails.analyze_run([row], build='Release', workload='culling',
                                          repetition=1, mode='fixed', phase_frames=60)
        self.assertEqual(frames[0]['render_sdl_present_ms'], '')
        self.assertEqual(report[0]['exact_sdl_present_available'], 0)
        with self.assertRaisesRegex(ValueError, 'rebuild'):
            sweep.require_exact_present([row])

        row['render_sdl_present_ms'] = '0.55'
        sweep.require_exact_present([row])
        frames, report = tails.analyze_run([row], build='Release', workload='culling',
                                          repetition=1, mode='fixed', phase_frames=60)
        self.assertAlmostEqual(frames[0]['render_cpu_pre_end_ms'], 0.15)
        self.assertAlmostEqual(frames[0]['render_end_cleanup_ms'], 0.05)
        self.assertEqual(frames[0]['render_sdl_present_ms'], 0.55)
        self.assertEqual(report[0]['exact_sdl_present_available'], 1)

    def test_new_capture_refuses_invalid_exact_measurements(self):
        row = {'render_only_present_ms': '0.4', 'render_sdl_present_ms': '0.5'}
        with self.assertRaisesRegex(ValueError, 'nesting'):
            sweep.require_exact_present([row])
        for invalid in ('', 'nan', '-1', 'bogus'):
            with self.subTest(invalid=invalid):
                row['render_sdl_present_ms'] = invalid
                with self.assertRaises(ValueError):
                    sweep.require_exact_present([row])

    def test_raw_build_log_is_not_overwritten_by_console_tee(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'selector'
            with (mock.patch.object(sweep, 'run_build_with_live_log', return_value=7) as build,
                  mock.patch.object(sweep.subprocess, 'call') as run,
                  redirect_stderr(StringIO())):
                status = sweep.main(['--build', '--output', str(output),
                                     '--workloads', 'culling', '--repetitions', '1'])
            self.assertEqual(status, 1)
            build.assert_called_once()
            args = build.call_args.args[0]
            self.assertEqual(args[args.index('--raw-log') + 1], str(output / 'build.log'))
            self.assertEqual(build.call_args.kwargs['log_path'], output / 'build_console.log')
            self.assertNotEqual(Path(args[args.index('--raw-log') + 1]),
                                build.call_args.kwargs['log_path'])
            run.assert_not_called()

    def test_missing_present_column_aborts_before_further_runs(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'stale'
            old_rows = [{'render_only_present_ms': '0.2'}]
            with (mock.patch.object(sweep, 'read_profile', return_value=(old_rows, old_rows)),
                  mock.patch.object(sweep.subprocess, 'call', return_value=0) as run,
                  redirect_stderr(StringIO())):
                status = sweep.main(['--output', str(output), '--workloads', 'culling',
                                     '--repetitions', '7'])
            self.assertEqual(status, 1)
            self.assertEqual(run.call_count, 1)
            self.assertIn('render_sdl_present_ms',
                          (output / 'failures.txt').read_text(encoding='utf-8'))


if __name__ == '__main__':
    unittest.main()
