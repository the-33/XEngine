"""Selector scoring ablation, honest per-frame route parity and fail-fast benchmark."""
from __future__ import annotations

import csv
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import sweep_render_selector_cost as cost
import run_benchmark as benchmark
from sweep_render_threshold import FIELDS, VERIFY


def profile(path: Path, *, estimate: str, route: str = 'recorded', altered: bool = False) -> None:
    columns = sorted(set(FIELDS) | set(VERIFY) | set(cost.REQUIRED))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('w', newline='', encoding='utf-8') as handle:
        writer = csv.DictWriter(handle, fieldnames=columns)
        writer.writeheader()
        for i in range(9):
            row = dict.fromkeys(columns, '0')
            row.update({
                'frame_ms': f'{1.0 + i * .01 + cost.ESTIMATE_CODES[estimate] * .04:.5f}',
                'render_world_ms': f'{.3 + cost.ESTIMATE_CODES[estimate] * .02:.5f}',
                'render_record_ms': '.30', 'render_prepare_ms': '.15',
                'render_submit_ms': '.35', 'render_selector_decision_ms': '.0002',
                'fixed_steps': '1', 'render_world_draws': '100',
                'render_eligible_current': '2048' if not altered else '2047',
                'render_background_draws': '1', 'render_ui_draws': '0',
                'render_tint_updates': '2', 'collision_contact_signature': '42',
                'render_route_override': str(cost.ROUTE_CODES[route][0]),
                'render_path_reason': str(cost.ROUTE_CODES[route][1]),
                'render_workers': '3' if route == 'recorded' else '0',
                'render_parallel_executed': '1' if route == 'recorded' else '0',
                'render_work_estimate_mode': str(cost.ESTIMATE_CODES[estimate]),
                'render_estimated_offscreen_sprites': '10' if estimate == 'visibility' else '0',
            })
            writer.writerow(row)


class SelectorCostTests(unittest.TestCase):
    def test_selector_probe_marker_rejects_stale_binary(self):
        stdout = '[BENCH-SELECTOR-PROBE] estimate=visibility route=recorded\n'
        benchmark.assert_selector_probe_marker(stdout, estimate='visibility', route='recorded')
        with self.assertRaisesRegex(RuntimeError, 'rebuild'):
            benchmark.assert_selector_probe_marker('', estimate='visibility', route='recorded')
        with self.assertRaisesRegex(RuntimeError, 'handshake'):
            benchmark.assert_selector_probe_marker(stdout, estimate='off', route='recorded')

    def test_profile_checks_and_deltas(self):
        with tempfile.TemporaryDirectory() as temp:
            paths = {mode: Path(temp) / f'{mode}.csv' for mode in cost.ESTIMATES}
            for mode, path in paths.items():
                profile(path, estimate=mode)
            captures = {mode: cost.load_checked(path, 1, 8, mode, 'recorded')
                        for mode, path in paths.items()}
            for mode in ('weighted', 'visibility'):
                cost.assert_same_paths(captures['off'][0], captures[mode][0])
            pair = {'workload': 'baseline', 'route': 'recorded', 'repetition': 1}
            cost.add_deltas(pair, {mode: cost.stats(rows) for mode, (_, rows) in captures.items()})
            self.assertAlmostEqual(pair['weights_minus_off_render_world_ms_p50_ms'], .02)
            self.assertAlmostEqual(pair['visibility_minus_weights_render_world_ms_p95_ms'], .02)
            summary = cost.summarize([pair])
            self.assertEqual(summary[0]['repetitions'], 1)
            self.assertEqual(summary[0]['visibility_minus_weights_render_world_ms_p50_ms_positive_runs'], 1)
            profile(paths['visibility'], estimate='visibility', altered=True)
            changed, _ = cost.load_checked(paths['visibility'], 1, 8, 'visibility', 'recorded')
            with self.assertRaisesRegex(ValueError, 'Non-equivalent'):
                cost.assert_same_paths(captures['off'][0], changed)

    def test_wrong_route_or_work_estimate_is_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / 'fake.csv'
            profile(path, estimate='off', route='immediate')
            with self.assertRaisesRegex(ValueError, 'incorrect selector'):
                cost.load_checked(path, 1, 8, 'weighted', 'immediate')
            with self.assertRaisesRegex(ValueError, 'incorrect selector'):
                cost.load_checked(path, 1, 8, 'off', 'recorded')

    def test_full_mock_sweep_writes_all_exports(self):
        with tempfile.TemporaryDirectory() as temp:
            output = Path(temp) / 'output'
            calls = []

            def fake_call(argv, *, cwd):
                self.assertEqual(cwd, ROOT)
                estimate = argv[argv.index('--render-estimate') + 1]
                route = argv[argv.index('--render-route') + 1]
                dest = Path(argv[argv.index('--output') + 1])
                profile(dest / 'render_parallel_run01.csv', estimate=estimate, route=route)
                calls.append(estimate)
                return 0

            with mock.patch.object(cost.subprocess, 'call', side_effect=fake_call):
                self.assertEqual(cost.run(['--workloads', 'baseline', '--routes', 'recorded',
                                           '--frames', '8', '--warmup', '1', '--phase-frames', '2',
                                           '--repetitions', '1', '--output', str(output)]), 0)
            self.assertEqual(calls, list(cost.ESTIMATES))
            for file in ('paired_cost.csv', 'phase_cost.csv', 'p99_outliers.csv',
                         'summary.csv', 'metadata.json'):
                self.assertTrue((output / file).is_file(), file)
            with (output / 'paired_cost.csv').open(newline='') as stream:
                pairs = list(csv.DictReader(stream))
            self.assertEqual(len(pairs), 1)
            self.assertEqual(pairs[0]['equivalent_frames'], '9')

    def test_release_build_failure_aborts_without_running_stale_binary(self):
        with tempfile.TemporaryDirectory() as temp:
            with mock.patch.object(cost, 'run_build_with_live_log', return_value=17) as build, \
                 mock.patch.object(cost.subprocess, 'call', side_effect=AssertionError('stale executable')):
                result = cost.run(['--build', '--build-config', 'Release', '--output', temp])
            self.assertEqual(result, 1)
            self.assertIn('Release', build.call_args.args[0])
            self.assertIn('Build failed', (Path(temp) / 'failures.txt').read_text())

    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_new_enum_defaults_compile_without_sdl(self):
        source = '''
#include "RenderDiagnostics.h"
#include <cassert>
int main() {
    using namespace RenderDiagnostics;
    static_assert(static_cast<unsigned>(WorkEstimate::Off) == 0);
    static_assert(static_cast<unsigned>(WorkEstimate::Weighted) == 1);
    static_assert(static_cast<unsigned>(WorkEstimate::Visibility) == 2);
    static_assert(static_cast<unsigned>(RouteOverride::Recorded) == 2);
    static_assert(static_cast<unsigned>(Path::ForcedImmediate) == 4);
    assert(Name(Path::ForcedImmediate)[0] == 'f');
    assert(SelectAdaptive(true,true,3,1536*256,1536,false)==Path::Parallel);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            p = Path(temp)
            (p/'test.cpp').write_text(source)
            subprocess.run(['g++', '-std=c++20', '-I', str(ROOT/'engine/include'),
                            str(p/'test.cpp'), '-o', str(p/'test')], check=True,
                           capture_output=True, timeout=30)
            subprocess.run([str(p/'test')], check=True, timeout=10)


if __name__ == '__main__':
    unittest.main()
