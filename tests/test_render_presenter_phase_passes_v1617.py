"""SDL presenter handshake and separate contiguous load-phase visits (v16.17)."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import run_benchmark as runner  # noqa: E402
import render_selector_tails as tails  # noqa: E402
import sweep_render_selector as sweep  # noqa: E402


def presenter() -> dict[str, object]:
    return {
        'renderer_info_available': True, 'renderer_name': 'direct3d11',
        'video_driver': 'windows', 'sdl_version': '2.30.10',
        'renderer_flags': 10, 'renderer_vsync': False,
        'renderer_output_width': 1280, 'renderer_output_height': 720,
        'window_width': 1280, 'window_height': 720,
        'display_index': 0, 'display_refresh_hz': 144,
        'window_fullscreen': False,
    }


def measured_rows(*, exact: bool = True, warmup: int = 60,
                  count: int = 480) -> list[dict[str, str]]:
    result = []
    for frame in range(warmup, warmup + count):
        segment = frame // 60
        first_visit = segment < 5
        since_boundary = frame % 60
        latency = 2.0 if first_visit and since_boundary < 5 else 1.0
        row = {'frame': str(frame), 'frame_ms': str(latency),
               'render_present_ms': str(latency - .1),
               'render_only_present_ms': str(latency - .3),
               'render_record_ms': '0.02', 'render_prepare_ms': '0.03',
               'render_submit_ms': '0.04', 'render_worker_wait_ms': '0.001',
               'render_path_reason': '3' if segment % 4 == 2 else '1',
               'render_parallel_executed': '1' if segment % 4 == 2 else '0',
               'render_parallel_fallback': '0'}
        if exact:
            row['render_sdl_present_ms'] = str(latency - .35)
        result.append(row)
    return result


class PresenterDiagnosticsTests(unittest.TestCase):
    def test_marker_accepts_exactly_one_complete_actual_backend(self):
        info = presenter()
        self.assertEqual(runner.parse_presenter_marker('[BENCH-PRESENTER] ' + json.dumps(info)), info)
        for bad in ('', '[BENCH-PRESENTER] {}', '[BENCH-PRESENTER] broken'):
            with self.subTest(bad=bad), self.assertRaisesRegex(RuntimeError, 'presenter'):
                runner.parse_presenter_marker(bad)

        info['renderer_vsync'] = True
        with self.assertRaisesRegex(RuntimeError, 'VSync'):
            runner.parse_presenter_marker('[BENCH-PRESENTER] ' + json.dumps(info))
        info['renderer_vsync'] = False
        with self.assertRaisesRegex(RuntimeError, 'exactly one'):
            runner.parse_presenter_marker('\n'.join(['[BENCH-PRESENTER] ' + json.dumps(info)] * 2))

    def test_metadata_required_and_pair_conditions_matched(self):
        a = presenter()
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            (path / 'metadata.json').write_text(json.dumps({
                'presenter_observations': [{'scenario': 'render', 'mode': 'parallel',
                                            'repetition': 1, 'presenter': a}]}), encoding='utf-8')
            self.assertEqual(sweep.read_presenter(path), a)
        sweep.require_matching_presenters(a, dict(a))
        other = dict(a)
        other['renderer_name'] = 'opengl'
        with self.assertRaisesRegex(ValueError, 'conditions differ'):
            sweep.require_matching_presenters(a, other)
        # Window-focus flags may change across processes and are not paired.
        other = dict(a, window_flags=123456)
        sweep.require_matching_presenters(a, other)

    def test_cpp_snapshot_has_no_frame_timing_side_effect(self):
        renderer = (ROOT / 'engine/src/RenderManager.cpp').read_text(encoding='utf-8')
        game = (ROOT / 'games/XEngine Benchmark/src/main.cpp').read_text(encoding='utf-8')
        self.assertIn('SDL_GetRendererInfo(mRenderer, &info)', renderer)
        self.assertIn('SDL_GetCurrentVideoDriver()', renderer)
        self.assertLess(game.index('Engine::StartFromJson(runtimeConfig)'),
                        game.index('QueryBackendDiagnostics()'))
        self.assertLess(game.index('QueryBackendDiagnostics()'), game.index('Engine::Run()'))
        self.assertIn('SDL_RENDERER_PRESENTVSYNC', renderer)
        self.assertIn('[BENCH-PRESENTER]', game)


class PhasePassDiagnosticsTests(unittest.TestCase):
    def test_distinct_visits_first10_and_exact_sdl(self):
        rows = measured_rows()
        passes = tails.analyze_phase_passes(rows, build='Release', workload='culling',
                                           repetition=1, mode='fixed', phase_frames=60,
                                           warmup=60)
        self.assertEqual(len(passes), 8)
        self.assertEqual([(p['phase'], p['pass_index'], p['phase_start_frame'])
                          for p in passes], [(1, 1, 60), (2, 1, 120), (3, 1, 180),
                                            (0, 1, 240), (1, 2, 300), (2, 2, 360),
                                            (3, 2, 420), (0, 2, 480)])
        self.assertTrue(all(p['complete_phase'] and p['frame_count'] == 60 for p in passes))
        self.assertGreater(passes[0]['frame_p95_ms'], passes[4]['frame_p95_ms'])
        self.assertGreater(passes[0]['first10_frame_p95_ms'],
                           passes[0]['remaining_frame_p95_ms'])
        self.assertGreater(passes[0]['sdl_present_p95_ms'], passes[4]['sdl_present_p95_ms'])
        self.assertEqual(passes[0]['p95_tail_in_first10'], 5)
        self.assertEqual(passes[0]['max_consecutive_p95_frames'], 5)
        self.assertEqual(passes[1]['parallel_executed_frames'], 60)
        self.assertAlmostEqual(passes[0]['end_cleanup_p95_ms'], .05)

    def test_compatibility_with_older_profiles_without_sdl_column(self):
        passes = tails.analyze_phase_passes(measured_rows(exact=False),
                                            build='Release', workload='culling',
                                            repetition=1, mode='fixed', phase_frames=60,
                                            warmup=60)
        self.assertEqual(passes[0]['sdl_exact_available'], 0)
        self.assertEqual(passes[0]['sdl_present_p95_ms'], '')

    def test_two_comparison_axes_are_independent(self):
        fixed = tails.analyze_phase_passes(measured_rows(), build='Release',
                                           workload='culling', repetition=1, mode='fixed',
                                           phase_frames=60, warmup=60)
        adaptive = tails.analyze_phase_passes(measured_rows(), build='Release',
                                              workload='culling', repetition=1, mode='adaptive',
                                              phase_frames=60, warmup=60)
        pair_modes, pair_passes = tails.compare_phase_passes(fixed + adaptive)
        self.assertEqual(len(pair_modes), 8)
        self.assertEqual(len(pair_passes), 8)
        self.assertTrue(all(p['adaptive_frame_p95_gain_pct'] == 0 for p in pair_modes))
        self.assertTrue(any(p['second_frame_p95_gain_pct'] > 0 for p in pair_passes))
        with self.assertRaisesRegex(ValueError, 'duplicate phase'):
            tails.compare_phase_passes(fixed + fixed)

    def test_partial_visits_are_not_silently_paired(self):
        passes = tails.analyze_phase_passes(measured_rows(warmup=30, count=480),
                                            build='Release', workload='culling',
                                            repetition=1, mode='fixed', phase_frames=60,
                                            warmup=30)
        self.assertEqual(passes[0]['complete_phase'], 0)
        self.assertEqual(passes[-1]['complete_phase'], 0)
        self.assertFalse(any(p['phase'] == passes[0]['phase'] and
                             p['pass_index'] == 1 and p['complete_phase']
                             for p in passes))

    def test_mock_sweep_exports_pass_reports_and_counterbalanced_launches(self):
        def profile(path: Path, _warmup: int, _frames: int):
            rows = measured_rows()
            adaptive = 'adaptive_' in str(path)
            for row in rows:
                row.update(render_selection_mode=str(int(adaptive)),
                           render_visibility_skipped_sprites='0',
                           render_eligible_current='1000',
                           render_weighted_units_current='256000',
                           render_executed_switched='0',
                           render_estimated_offscreen_sprites='0',
                           render_effective_units_current='256000',
                           render_path_switched='0')
            return rows, rows

        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'selector'
            with (mock.patch.object(sweep.subprocess, 'call', return_value=0) as run,
                  mock.patch.object(sweep, 'read_profile', side_effect=profile),
                  mock.patch.object(sweep, 'read_presenter', return_value=presenter()),
                  mock.patch.object(sweep, 'compare_profiles', return_value=(0, 'equivalent')),
                  mock.patch.object(sweep, 'analyze_run', return_value=([], [])),
                  mock.patch.object(sweep, 'analyze_phase_run', return_value=([], []))):
                rc = sweep.main(['--output', str(output), '--workloads', 'culling',
                                 '--repetitions', '2'])
            self.assertEqual(rc, 0)
            self.assertEqual(run.call_count, 4)
            with (output / 'run_order.csv').open(encoding='utf-8') as handle:
                import csv
                ordered = list(csv.DictReader(handle))
            self.assertEqual([row['mode'] for row in ordered],
                             ['fixed', 'adaptive', 'adaptive', 'fixed'])
            for name, expected in [('phase_pass_results.csv', 32),
                                   ('phase_pass_comparisons.csv', 16),
                                   ('phase_pass_transitions.csv', 16),
                                   ('paired_comparisons.csv', 2)]:
                with (output / name).open(encoding='utf-8') as handle:
                    import csv
                    self.assertEqual(len(list(csv.DictReader(handle))), expected, name)
            self.assertFalse((output / 'failures.txt').exists())


if __name__ == '__main__':
    unittest.main()
