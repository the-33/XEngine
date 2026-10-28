"""Safety checks for per-user Windows/Linux measured SDL backend priorities."""
from __future__ import annotations

import csv
from contextlib import redirect_stdout
import io
import json
import sys
import tempfile
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import install_render_backend_ranking as install_tool
import benchmark_render_backends as benchmark


class LocalRendererRankingTests(unittest.TestCase):
    def test_status_is_readable_and_json_remains_available(self):
        record = {
            'platform': 'windows', 'ranking_valid': True,
            'display': {'video_driver': 'windows', 'sdl_version': '2.32.10',
                        'window_width': 1280, 'window_height': 720,
                        'display_index': 0, 'display_refresh_hz': 144,
                        'window_fullscreen': False},
            'ranked_backends': ['direct3d11', 'direct3d', 'opengles2'],
            'benchmark': {'build_config': 'Release', 'repetitions': 7,
                          'frames': 480, 'workloads': ['baseline', 'culling'],
                          'excluded_backends': ['software'],
                          'selection_policy': 'fewest long SDL_RenderPresent calls; p99.9 tie; '
                                              'p95/p99 consistency across repetitions'},
        }
        with tempfile.TemporaryDirectory() as temp:
            ranking_path = Path(temp) / 'render_backend_ranking.json'
            ranking_path.write_text(json.dumps(record), encoding='utf-8')
            with patch.object(install_tool, 'destination', return_value=ranking_path):
                human_output = io.StringIO()
                with redirect_stdout(human_output):
                    self.assertEqual(install_tool.main(['--status']), 0)
                human = human_output.getvalue()
                self.assertIn('RENDER BACKEND AUTO', human)
                self.assertIn('Direct3D 11', human)
                self.assertIn('<-- preferred', human)
                self.assertLess(human.index('Direct3D 11'), human.index('OpenGL ES 2'))
                self.assertIn('144 Hz', human)
                self.assertIn('Window mode : windowed', human)
                self.assertIn('7 repetitions, 480 frames per run', human)
                self.assertIn('software', human)
                self.assertNotIn('"ranked_backends"', human)

                json_output = io.StringIO()
                with redirect_stdout(json_output):
                    self.assertEqual(install_tool.main(['--status', '--json']), 0)
                self.assertEqual(json.loads(json_output.getvalue()), record)

    def test_new_presenter_fullscreen_is_saved_without_breaking_old_results(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.make_fixture(Path(temp))
            path = result / 'backends/opengl/baseline/fixed_run01/metadata.json'
            data = json.loads(path.read_text(encoding='utf-8'))
            data['presenter_observations'][0]['presenter']['window_fullscreen'] = True
            path.write_text(json.dumps(data), encoding='utf-8')
            recorded = install_tool.validate_results(result, platform='linux')
            self.assertIs(recorded['display']['window_fullscreen'], True)
            data['presenter_observations'][0]['presenter']['window_fullscreen'] = 'false'
            path.write_text(json.dumps(data), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'fullscreen'):
                install_tool.validate_results(result, platform='linux')
            data['presenter_observations'][0]['presenter'].pop('window_fullscreen')
            path.write_text(json.dumps(data), encoding='utf-8')
            self.assertNotIn('window_fullscreen',
                             install_tool.validate_results(result, platform='linux')['display'])

    def make_fixture(self, root: Path) -> Path:
        result = root / 'render_backends_test'
        result.mkdir()
        names = ('opengl', 'opengles2')
        metadata = {
            'platform': 'linux', 'ranking_valid': True, 'build_config': 'Release',
            'workloads': list(benchmark.WORKLOADS), 'repetitions': 3,
            'frames': 480, 'candidate_backends': list(names),
            'selection_policy': 'fewest frame stalls; SDL_RenderPresent >60Hz',
        }
        (result / 'metadata.json').write_text(json.dumps(metadata), encoding='utf-8')
        rank = []
        for name, stutters in zip(names, (0, 1)):
            rank.append({
                'backend': name, 'measured_frames': 4 * 2 * 3 * 480,
                'frame_over_60hz_count': stutters,
                'sdl_present_over_60hz_count': stutters,
                'frame_over_display_count': stutters,
                'sdl_present_over_display_count': stutters,
                'frame_p999_ms': 4 + stutters,
                'frame_p95_score_ms': 1.7 - stutters * .1,
                'frame_p99_score_ms': 2.1, 'frame_p50_score_ms': 1.0,
            })
            run = result / 'backends' / name / 'baseline' / 'fixed_run01'
            run.mkdir(parents=True)
            presenter = {
                'renderer_name': name, 'video_driver': 'x11',
                'sdl_version': '2.32.10', 'window_width': 1280,
                'window_height': 720, 'display_index': 0,
                'display_refresh_hz': 144,
            }
            (run / 'metadata.json').write_text(json.dumps({
                'presenter_observations': [{'presenter': presenter}]
            }), encoding='utf-8')
        with (result / 'backend_ranking.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rank[0]))
            writer.writeheader()
            writer.writerows(rank)
        (result / 'backend_failures.csv').write_text('backend,reason\n', encoding='utf-8')
        (result / 'backend_equivalence.csv').write_text(
            'backend_a,backend_b,equivalent\nopengl,opengles2,1\n', encoding='utf-8')
        return result

    def add_vsync_exclusion(self, result: Path) -> None:
        metadata_path = result / 'metadata.json'
        metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
        metadata['candidate_backends'].append('software')
        metadata['excluded_backends'] = ['software']
        metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
        with (result / 'backend_exclusions.csv').open('w', newline='', encoding='utf-8') as stream:
            writer = csv.DictWriter(stream, fieldnames=['backend', 'reason_code', 'reason'])
            writer.writeheader()
            writer.writerow({'backend': 'software',
                             'reason_code': benchmark.VSYNC_EXCLUSION_CODE,
                             'reason': benchmark.VSYNC_EXCLUSION_MESSAGE})
        sweep = result / 'backends' / 'software'
        first = sweep / 'baseline' / 'fixed_run01'
        first.mkdir(parents=True)
        (first / 'failures.txt').write_text(
            'render_parallel_run01: ' + benchmark.VSYNC_EXCLUSION_MESSAGE + '\n',
            encoding='utf-8')
        (sweep / 'failures.txt').write_text(
            'baseline/1/fixed: benchmark exited 1\n', encoding='utf-8')
        with (sweep / 'run_order.csv').open('w', newline='', encoding='utf-8') as stream:
            writer = csv.DictWriter(stream,
                                    fieldnames=['workload', 'repetition', 'mode',
                                                'runner_exit_code'])
            writer.writeheader()
            writer.writerow({'workload': 'baseline', 'repetition': 1,
                             'mode': 'fixed', 'runner_exit_code': 1})

    def make_v2_tie_fixture(self, result: Path) -> None:
        """A real v2 order must be recomputable from every run, not blindly trusted."""
        metadata_file = result / 'metadata.json'
        metadata = json.loads(metadata_file.read_text(encoding='utf-8'))
        metadata['selection_policy_version'] = 2
        metadata_file.write_text(json.dumps(metadata), encoding='utf-8')
        runs = []
        for backend, p95, p99 in (('opengl', 1.20, 1.40),
                                  ('opengles2', 1.13, 1.30)):
            for workload in benchmark.WORKLOADS:
                for mode in benchmark.MODES:
                    for rep in (1, 2, 3):
                        runs.append({'backend': backend, 'workload': workload,
                                     'mode': mode, 'repetition': rep,
                                     'measured_frames': 480,
                                     'frame_p95_ms': p95, 'frame_p99_ms': p99})
        with (result / 'backend_tail_runs.csv').open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(runs[0]))
            writer.writeheader()
            writer.writerows(runs)
        path = result / 'backend_ranking.csv'
        rows = install_tool._read_csv(path)
        for row in rows:
            row['frame_over_60hz_count'] = 0
            row['sdl_present_over_60hz_count'] = 0
            row['frame_over_display_count'] = 0
            row['sdl_present_over_display_count'] = 0
            row['frame_p999_ms'] = '1.7001' if row['backend'] == 'opengl' else '1.6999'
            row['frame_p95_score_ms'] = '1.2' if row['backend'] == 'opengl' else '1.13'
            row['frame_p99_score_ms'] = '1.4' if row['backend'] == 'opengl' else '1.3'
        ranked = benchmark.rank_backends(rows, runs, platform='linux')
        with path.open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(ranked)

    def test_v2_ranking_requires_matching_repetition_evidence_and_order(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.make_fixture(Path(temp))
            self.make_v2_tie_fixture(result)
            record = install_tool.validate_results(result, platform='linux')
            self.assertEqual(record['ranked_backends'], ['opengles2', 'opengl'])
            path = result / 'backend_tail_runs.csv'
            runs = install_tool._read_csv(path)
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(runs[0]))
                writer.writeheader()
                writer.writerows(runs[:-1])
            with self.assertRaisesRegex(ValueError, 'repetition evidence'):
                install_tool.validate_results(result, platform='linux')
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(runs[0]))
                writer.writeheader()
                writer.writerows(runs)
            ranking_path = result / 'backend_ranking.csv'
            rows = install_tool._read_csv(ranking_path)
            with ranking_path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(reversed(rows))
            with self.assertRaisesRegex(ValueError, 'does not agree'):
                install_tool.validate_results(result, platform='linux')

    def test_user_local_path_is_os_specific(self):
        self.assertEqual(install_tool.destination('linux', {'HOME': '/user'}),
                         Path('/user/.config/xengine/render_backend_ranking.json'))
        self.assertEqual(install_tool.destination('linux', {'XDG_CONFIG_HOME': '/custom'}),
                         Path('/custom/xengine/render_backend_ranking.json'))
        self.assertEqual(install_tool.destination('win32', {'LOCALAPPDATA': 'C:/settings'}),
                         Path('C:/settings/XEngine/render_backend_ranking.json'))
        with self.assertRaisesRegex(ValueError, 'Unsupported'):
            install_tool.destination('darwin', {})

    def test_validated_results_install_in_stability_order(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.make_fixture(root)
            path, record = install_tool.install(
                result, target=root / 'user-local/ranking.json', platform='linux')
            self.assertEqual(record['ranked_backends'], ['opengl', 'opengles2'])
            self.assertEqual(record['display']['display_refresh_hz'], 144)
            self.assertEqual(json.loads(path.read_text()), record)

    def test_verified_vsync_incompatibility_does_not_discard_valid_gpu_rankings(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.make_fixture(root)
            self.add_vsync_exclusion(result)
            self.assertTrue(benchmark.has_vsync_exclusion_evidence(
                result / 'backends/software', 'baseline'))
            path, record = install_tool.install(
                result, target=root / 'settings/ranking.json', platform='linux')
            self.assertEqual(record['ranked_backends'], ['opengl', 'opengles2'])
            self.assertEqual(record['benchmark']['excluded_backends'], ['software'])
            self.assertEqual(json.loads(path.read_text()), record)

    def test_rejects_missing_or_falsified_vsync_exclusion_evidence(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.make_fixture(Path(temp))
            self.add_vsync_exclusion(result)
            evidence = (result / 'backends/software/baseline/fixed_run01/failures.txt')
            evidence.write_text('render_parallel_run01: renderer crashed\n', encoding='utf-8')
            self.assertFalse(benchmark.has_vsync_exclusion_evidence(
                result / 'backends/software', 'baseline'))
            with self.assertRaisesRegex(ValueError, 'unverified backend exclusion'):
                install_tool.validate_results(result, platform='linux')
            evidence.write_text(
                'render_parallel_run01: ' + benchmark.VSYNC_EXCLUSION_MESSAGE + '\n',
                encoding='utf-8')
            (result / 'backends/software/run_order.csv').write_text(
                'workload,repetition,mode,runner_exit_code\n'
                'baseline,1,fixed,1\nculling,1,adaptive,1\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'unverified backend exclusion'):
                install_tool.validate_results(result, platform='linux')

    def test_rejects_missing_candidates_and_actual_failures_despite_exclusions(self):
        with tempfile.TemporaryDirectory() as temp:
            result = self.make_fixture(Path(temp))
            self.add_vsync_exclusion(result)
            metadata_path = result / 'metadata.json'
            metadata = json.loads(metadata_path.read_text(encoding='utf-8'))
            metadata['candidate_backends'].append('direct3d11')
            metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'exact candidates'):
                install_tool.validate_results(result, platform='linux')
            metadata['candidate_backends'].remove('direct3d11')
            metadata_path.write_text(json.dumps(metadata), encoding='utf-8')
            (result / 'backend_failures.csv').write_text(
                'backend,reason\nopengl,profile incomplete\n', encoding='utf-8')
            with self.assertRaisesRegex(ValueError, 'At least one backend failed'):
                install_tool.validate_results(result, platform='linux')

    def test_refuses_cross_os_and_incomplete_results_without_mutating_settings(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.make_fixture(root)
            target = root / 'user-local/ranking.json'
            with self.assertRaisesRegex(ValueError, 'differs from host'):
                install_tool.install(result, target=target, platform='win32')
            self.assertFalse(target.exists())
            metadata = json.loads((result / 'metadata.json').read_text())
            metadata.pop('selection_policy')  # historical p95-only CSV
            (result / 'metadata.json').write_text(json.dumps(metadata))
            with self.assertRaisesRegex(ValueError, 'Legacy p95-only'):
                install_tool.install(result, target=target, platform='linux')
            self.assertFalse(target.exists())

    def test_refuses_non_equivalence_and_unsorted_measurements(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            result = self.make_fixture(root)
            (result / 'backend_equivalence.csv').write_text(
                'backend_a,backend_b,equivalent\nopengl,opengles2,0\n')
            with self.assertRaisesRegex(ValueError, 'equivalence'):
                install_tool.validate_results(result, platform='linux')
            (result / 'backend_equivalence.csv').write_text(
                'backend_a,backend_b,equivalent\nopengl,opengles2,1\n')
            path = result / 'backend_ranking.csv'
            with path.open(newline='') as stream:
                rows = list(csv.DictReader(stream))
            with path.open('w', newline='') as stream:
                writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                writer.writeheader()
                writer.writerows(reversed(rows))
            with self.assertRaisesRegex(ValueError, 'does not agree'):
                install_tool.validate_results(result, platform='linux')


if __name__ == '__main__':
    unittest.main()
