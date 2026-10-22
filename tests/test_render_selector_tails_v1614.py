"""Regression tests for transparent per-stage p95/p99 selector reports."""
from __future__ import annotations

import csv
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
import zipfile


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import render_selector_tails as tails  # noqa: E402


def samples() -> list[dict[str, str]]:
    data = []
    for frame in range(100):
        duration = float(frame + 1)
        render = 86. if frame == 99 else 2.
        input_time = 10. if frame == 99 else max(0., duration - 4.)
        data.append({'frame': str(frame), 'frame_ms': str(duration),
                     'input_ms': str(input_time), 'fixed_ms': '1',
                     'update_ms': '1', 'render_present_ms': str(render),
                     'render_world_ms': '0.25', 'render_record_ms': '0.1',
                     'render_prepare_ms': '0.01', 'render_submit_ms': '0.3',
                     'render_only_present_ms': '80' if frame == 99 else '1',
                     'render_worker_wait_ms': '0.001',
                     'render_selector_decision_ms': '0.0001',
                     'fixed_steps': '1', 'collision_contact_signature': '17',
                     'contacts_built': '0', 'narrowphase_tests': '0',
                     'render_world_draws': '10', 'render_eligible_current': '10',
                     'render_background_draws': '0', 'render_ui_draws': '0',
                     'render_tint_updates': '0'})
    return data


class RenderSelectorTailTests(unittest.TestCase):
    def test_tail_severity_and_disjoint_stages(self):
        frames, summaries = tails.analyze_run(samples(), build='Release',
            workload='baseline', repetition=2, mode='adaptive', phase_frames=10)
        self.assertEqual(len(frames), 5)  # percentile interpolation => frames 96..100
        self.assertEqual(sum(row['is_p99_tail'] for row in frames), 1)
        self.assertEqual(frames[-1]['phase'], 1)
        self.assertEqual(frames[-1]['largest_top_level_stage'], 'render_present_ms')
        self.assertAlmostEqual(frames[-1]['unaccounted_ms'], 2.)
        self.assertEqual(summaries[0]['tail_frames'], 5)
        self.assertEqual(summaries[1]['tail_frames'], 1)
        self.assertEqual(summaries[0]['input_ms_largest_count'], 4)
        self.assertEqual(summaries[1]['render_present_ms_largest_count'], 1)
        self.assertAlmostEqual(summaries[1]['render_only_present_ms_tail_median'], 80.)

    def test_missing_columns_fail_loudly(self):
        data = samples()
        del data[0]['render_only_present_ms']
        with self.assertRaisesRegex(ValueError, 'render_only_present_ms'):
            tails.analyze_run(data, build='Debug', workload='culling',
                              repetition=1, mode='fixed', phase_frames=10)

    def test_nonfinite_timings_fail_loudly(self):
        data = samples()
        data[50]['frame_ms'] = 'nan'
        with self.assertRaisesRegex(ValueError, 'nonfinite'):
            tails.analyze_run(data, build='Debug', workload='baseline',
                              repetition=1, mode='fixed', phase_frames=10)

    def test_zip_and_directory_reanalysis_match(self):
        data = samples()
        metadata = {'selector_modes': ['fixed','adaptive'], 'workloads': ['baseline'],
                    'repetitions': 1, 'warmup': 0, 'frames': len(data),
                    'build_config': 'Debug', 'phase_frames': 10}
        contents = io.StringIO()
        writer = csv.DictWriter(contents, fieldnames=list(data[0]))
        writer.writeheader()
        writer.writerows(data)
        files = {
            'metadata.json': json.dumps(metadata),
            'baseline/fixed_run01/render_parallel_run01.csv': contents.getvalue(),
            'baseline/adaptive_run01/render_parallel_run01.csv': contents.getvalue(),
        }
        with tempfile.TemporaryDirectory() as temp:
            base = Path(temp)
            folder = base / 'render_selector_20261022_999999'
            for name, value in files.items():
                path = folder / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(value, encoding='utf-8')
            zipped = base / 'bench.zip'
            with zipfile.ZipFile(zipped, 'w') as archive:
                for name, value in files.items():
                    archive.writestr(f'{folder.name}/{name}', value)
            frames_folder, summaries_folder, pairs_folder = tails.analyze_existing(folder)
            frames_zip, summaries_zip, pairs_zip = tails.analyze_existing(zipped)
            self.assertEqual((pairs_folder, pairs_zip), (1,1))
            self.assertEqual(frames_folder, frames_zip)
            self.assertEqual(summaries_folder, summaries_zip)
            self.assertEqual(len(summaries_zip), 4)  # fixed/adaptive x p95/p99

    def test_real_sweep_writes_both_reports(self):
        source = (ROOT / 'tools/sweep_render_selector.py').read_text(encoding='utf-8')
        self.assertIn("write_csv(output/'tail_frames.csv',tail_frames)", source)
        self.assertIn("write_csv(output/'tail_summary.csv',tail_summaries)", source)


if __name__ == '__main__':
    unittest.main()
