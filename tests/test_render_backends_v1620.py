"""Offline regression checks for SDL backend discovery/config and ranking."""
from __future__ import annotations

import csv
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import benchmark_render_backends as backends
import install_render_backend_ranking as ranking_installer
import sweep_render_selector as selector_sweep
import run_benchmark as benchmark_runner
import sync_engine_configs as engine_sync
from sweep_render_threshold import FIELDS, VERIFY


def sample_pair(workload='culling', rep=1, renderer='opengl', value=1.0):
    result = {
        'workload': workload, 'repetition': str(rep),
        'sdl_renderer': renderer, 'equivalent_frames': '540',
    }
    for mode in backends.MODES:
        for quantile, factor in [('p50', 1.0), ('p95', 1.5), ('p99', 2.0)]:
            result[f'{mode}_frame_{quantile}_ms'] = str(value * factor)
    return result


class RenderBackendTests(unittest.TestCase):
    def test_window_options_are_explicit_and_forwardable(self):
        flags = ['--width', '1920', '--height', '1080', '--fullscreen']
        top = backends.arguments(flags)
        sweep = selector_sweep.arguments(flags)
        self.assertEqual((top.width, top.height, top.fullscreen), (1920, 1080, True))
        self.assertEqual((sweep.width, sweep.height, sweep.fullscreen), (1920, 1080, True))
        with patch.object(sys, 'argv', ['run_benchmark.py', *flags]):
            runner = benchmark_runner.arguments()
        self.assertEqual((runner.width, runner.height, runner.fullscreen), (1920, 1080, True))
        windowed = backends.arguments([])
        self.assertEqual((windowed.width, windowed.height, windowed.fullscreen),
                         (1280, 720, False))

    def test_selector_sweep_aborts_after_first_unusable_vsync_presenter(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'sweep'
            commands = []

            def run_once(command, **_kwargs):
                commands.append(command)
                run_dir = Path(command[command.index('--output') + 1])
                run_dir.mkdir(parents=True)
                (run_dir / 'failures.txt').write_text(
                    'render_parallel_run01: ' + backends.VSYNC_EXCLUSION_MESSAGE + '\n',
                    encoding='utf-8')
                return 1

            with patch.object(selector_sweep.subprocess, 'call', side_effect=run_once):
                result = selector_sweep.main([
                    '--workloads', 'baseline', '--repetitions', '3',
                    '--frames', '240', '--phase-frames', '60',
                    '--output', str(output)])
            self.assertEqual(result, 1)
            self.assertEqual(len(commands), 1)
            self.assertTrue(backends.has_vsync_exclusion_evidence(output, 'baseline'))

    def test_probe_requires_single_valid_unique_sdl_banner(self):
        name = backends.PROBE_MARKER
        self.assertEqual(
            backends.parse_available_backends(name +
                '[{"name":"opengl","flags":10},{"name":"software","flags":1}]\n'),
            [{'name': 'opengl', 'flags': 10}, {'name': 'software', 'flags': 1}])
        with self.assertRaisesRegex(ValueError, 'one SDL backend probe'):
            backends.parse_available_backends('no SDL marker')
        with self.assertRaisesRegex(ValueError, 'Duplicate'):
            backends.parse_available_backends(name +
                '[{"name":"OpenGL","flags":10},{"name":"opengl","flags":10}]')
        with self.assertRaisesRegex(ValueError, 'Invalid/empty'):
            backends.parse_available_backends(name +
                '[{"name":"../../bad","flags":10}]')

    def test_deterministic_ranking_weights_workloads_equally(self):
        quick = [sample_pair('culling', 1, value=0.4),
                 sample_pair('culling', 2, value=0.6),
                 sample_pair('mixed', 1, value=2.0),
                 sample_pair('mixed', 2, value=2.0)]
        detail, aggregate = backends.summarize_backend(
            'opengl', quick, ['culling', 'mixed'])
        self.assertEqual(len(detail), 4)
        self.assertAlmostEqual(aggregate['frame_p50_score_ms'], 1.0)
        self.assertAlmostEqual(aggregate['frame_p95_score_ms'], 1.5)
        self.assertAlmostEqual(aggregate['frame_p99_score_ms'], 2.0)
        self.assertEqual(detail[0]['frame_p50_ms'], 0.5)

    def test_pairs_reject_wrong_renderer_missing_runs_and_stale_frames(self):
        with tempfile.TemporaryDirectory() as temp:
            folder = Path(temp)
            def save(items):
                with (folder / 'paired_comparisons.csv').open('w', newline='') as out:
                    writer = csv.DictWriter(out, fieldnames=list(items[0]))
                    writer.writeheader()
                    writer.writerows(items)
            rows = [sample_pair(rep=1), sample_pair(rep=2)]
            save(rows)
            self.assertEqual(len(backends.read_pairs(
                folder, backend='opengl', workloads=['culling'],
                repetitions=2, warmup=60, frames=480)), 2)
            with self.assertRaisesRegex(ValueError, 'unexpected'):
                backends.read_pairs(
                    folder, backend='opengl', workloads=['culling'],
                    repetitions=2, warmup=30, frames=480)
            with self.assertRaisesRegex(ValueError, 'actual SDL'):
                backends.read_pairs(
                    folder, backend='direct3d11', workloads=['culling'],
                    repetitions=2, warmup=60, frames=480)
            save(rows[:1])
            with self.assertRaisesRegex(ValueError, 'missing or duplicate'):
                backends.read_pairs(
                    folder, backend='opengl', workloads=['culling'],
                    repetitions=2, warmup=60, frames=480)

    def test_only_supported_platform_defaults_are_auto(self):
        template = json.loads((ROOT / 'tools/templates/BlankGame/assets/config/engine.json')
                              .read_text(encoding='utf-8'))
        self.assertEqual(template['Renderer']['backend'], {
            'windows': 'auto', 'linux': 'auto'})
        self.assertIs(template['Renderer']['backendFallback'], True)
        self.assertEqual(template['Renderer']['parallelSelection'], 'fixed')
        self.assertEqual(template['Renderer']['parallelMinCommands'], 1536)
        for game in ['Blob\'s Quest', 'My Princess Run', 'XEngine Benchmark']:
            config = json.loads((ROOT / 'games' / game / 'assets/config/engine.json')
                                .read_text(encoding='utf-8'))
            self.assertEqual(config['Renderer']['backend'],
                             template['Renderer']['backend'])
            self.assertIs(config['Renderer']['backendFallback'], True)
        for mode in ['single', 'collision', 'parallel']:
            path = ROOT / 'games/XEngine Benchmark/assets/config' / f'{mode}.json'
            self.assertEqual(json.loads(path.read_text())['Renderer']['backend'],
                             template['Renderer']['backend'])
            self.assertIs(json.loads(path.read_text())['Renderer']['backendFallback'], True)

    def test_source_selects_backend_with_logged_fallback_and_strict_process_hint(self):
        source = (ROOT / 'engine/src/RenderManager.cpp').read_text(encoding='utf-8')
        engine = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        self.assertIn('SDL_GetNumRenderDrivers()', source)
        self.assertIn('SDL_GetRenderDriverInfo(i, &info)', source)
        self.assertIn('SDL_GetHint(SDL_HINT_RENDER_DRIVER)', source)
        self.assertIn('mPreferredRendererDriverIndex', source)
        self.assertIn('RendererBackendPolicy::MakePlan(', source)
        self.assertIn('mWindow->SDL(), candidate.index, attemptFlags', source)
        self.assertIn('cfg.backendFallback, hasHint', source)
        self.assertIn('fallback from=%s to=%s', source)
        self.assertIn('mRendererCandidates', source)
        self.assertNotIn('SDL_SetHint(SDL_HINT_RENDER_DRIVER', source)
        self.assertIn('if (!ResolveRendererBackend_', source)
        self.assertIn('itBackend.key() == "linux"', engine)
        self.assertIn('itBackend.key() == "windows"', engine)
        self.assertIn('out.renderer.backendFallback', engine)
        self.assertNotIn('itBackend.key() == "macos"', engine)
        self.assertNotIn('defined(__APPLE__)', engine)

    def test_backend_fallback_policy_cpp(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if compiler is None:
            self.skipTest('C++ compiler unavailable')
        source = r'''
#include "RendererBackendPolicy.h"
#include <cassert>
int main() {
    using namespace RendererBackendPolicy;
    auto win = MakePlan("auto", true, true, false);
    assert(win.names.size() == 6 && win.names[0] == "direct3d11");
    assert(win.names[1] == "direct3d" && win.names[2] == "direct3d12");
    assert(win.names[3] == "opengles2" && win.allowSdlDefault);
    auto forced = MakePlan("OpenGL", true, true, false);
    assert(forced.names[0] == "opengl" && forced.names[1] == "direct3d11");
    assert(forced.names.size() == 6 && forced.allowSdlDefault);
    auto noFallback = MakePlan("Direct3D11", true, false, false);
    assert(noFallback.names.size() == 1 && !noFallback.allowSdlDefault);
    auto strict = MakePlan("Direct3D11", true, true, true);
    assert(strict.names.size() == 1 && !strict.allowSdlDefault);
    auto linuxAuto = MakePlan("auto", false, true, false);
    assert(linuxAuto.names.empty() && linuxAuto.allowSdlDefault);
    auto linuxRanked = MakePlan("auto", false, true, false, {"OpenGLES2", "OpenGL"});
    assert(linuxRanked.names.size() == 2 && linuxRanked.names[0] == "opengles2");
    assert(linuxRanked.names[1] == "opengl" && linuxRanked.allowSdlDefault);
    auto winRanked = MakePlan("auto", true, true, false, {"opengl", "direct3d11"});
    assert(winRanked.names[0] == "opengl" && winRanked.names[1] == "direct3d11");
    assert(winRanked.names[2] == "direct3d");
    auto linuxMeasuredStrict = MakePlan("opengl", false, false, false, {"opengles2"});
    assert(linuxMeasuredStrict.names.size() == 1 && !linuxMeasuredStrict.allowSdlDefault);
    auto benchmarkHint = MakePlan("opengles2", false, true, true, {"opengl"});
    assert(benchmarkHint.names.size() == 1 && benchmarkHint.names[0] == "opengles2");
    auto linuxForced = MakePlan("OpenGL", false, true, false);
    assert(linuxForced.names.size() == 1 && linuxForced.allowSdlDefault);
    auto linuxStrict = MakePlan("OpenGL", false, true, true);
    assert(linuxStrict.names.size() == 1 && !linuxStrict.allowSdlDefault);
    return 0;
}'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            unit = path / 'policy.cpp'
            executable = path / ('policy.exe' if os.name == 'nt' else 'policy')
            unit.write_text(source, encoding='utf-8')
            subprocess.run([compiler, '-std=c++17', '-I',
                            str(ROOT / 'engine/include'), str(unit), '-o',
                            str(executable)], check=True, capture_output=True, text=True)
            subprocess.run([str(executable)], check=True, capture_output=True, text=True)

    def test_template_config_migration_preserves_explicit_backend(self):
        template = engine_sync.read_json(
            ROOT / 'tools/templates/BlankGame/assets/config/engine.json')
        state, _ = engine_sync.read_state(
            ROOT / 'tools/engine_config_schema_state.json', engine_sync.leaf_paths(template))
        previous = {'Renderer': {'backend': {
            'windows': 'opengl', 'linux': 'opengles2'}, 'vsync': False}}
        merged, added, _, _, _ = engine_sync.merge_config(previous, template,
                                                         'My Game', state)
        self.assertEqual(merged['Renderer']['backend'], previous['Renderer']['backend'])
        self.assertFalse(merged['Renderer']['vsync'])
        self.assertTrue(merged['Renderer']['backendFallback'])
        self.assertIn(('Renderer', 'backendFallback'), added)

    def test_removed_macos_setting_is_pruned_without_overwriting_windows_linux(self):
        template = engine_sync.read_json(
            ROOT / 'tools/templates/BlankGame/assets/config/engine.json')
        known, _ = engine_sync.read_state(
            ROOT / 'tools/engine_config_schema_state.json',
            engine_sync.leaf_paths(template))
        # Old v16.20 settings remain in the schema history only so existing
        # games can be migrated safely; there is no macOS runtime option now.
        self.assertIn(('Renderer', 'backend', 'macos'), known)
        previous = {'Renderer': {'backend': {
            'windows': 'direct3d11', 'linux': 'opengl', 'macos': 'metal'}}}
        merged, _, removed, _, _ = engine_sync.merge_config(
            previous, template, 'ExampleGame', known)
        self.assertIn(('Renderer', 'backend', 'macos'), removed)
        self.assertEqual(merged['Renderer']['backend'],
                         {'windows': 'direct3d11', 'linux': 'opengl'})

    def test_cross_backend_invariants_reject_one_mismatching_frame(self):
        sample = {key: '0' for key in set(VERIFY) | set(FIELDS)}
        changed = dict(sample)
        changed['collision_contact_signature'] = 'different'
        with patch.object(backends, 'read_profile', side_effect=[
                ([sample] * 540, []), ([changed] + [sample] * 539, []),
                ([sample] * 540, []), ([sample] * 540, [])]):
            with tempfile.TemporaryDirectory() as temp:
                paths = {'opengl': Path(temp) / 'opengl',
                         'opengles2': Path(temp) / 'opengles2'}
                result = backends.compare_backend_invariants(
                    paths, workloads=['culling'], repetitions=1,
                    warmup=60, frames=480)
        self.assertEqual(result[0]['equivalent'], 0)
        self.assertIn('collision_contact_signature', result[0]['details'])

    def test_report_does_not_recommend_failed_or_non_equivalent_backend(self):
        first = {
            'backend': 'opengl', 'frame_p50_score_ms': 1.,
            'frame_p95_score_ms': 1.5, 'frame_p99_score_ms': 2.,
            **backends.frame_stability([1.0] * 999 + [20.], [0.5] * 1000, 144.),
        }
        report = backends.render_report(
            [first], discovered=['opengl'], failed=[],
            comparisons=[], reliable=False)
        self.assertIn('Sin recomendación válida', report)
        self.assertIn('no los píxeles', report)
        self.assertIn('nunca cambia', report)

    def test_tail_metrics_count_display_60hz_present_and_p999(self):
        frames = [1.] * 998 + [8., 20.]
        presents = [0.1] * 999 + [19.]
        stats = backends.frame_stability(frames, presents, 144.)
        self.assertEqual(stats['measured_frames'], 1000)
        self.assertEqual(stats['frame_over_display_count'], 2)
        self.assertEqual(stats['frame_over_60hz_count'], 1)
        self.assertEqual(stats['sdl_present_over_60hz_count'], 1)
        self.assertEqual(stats['sdl_present_over_display_count'], 1)
        self.assertGreater(stats['frame_p999_ms'], 8.)
        self.assertEqual(stats['frame_max_ms'], 20.)
        with self.assertRaisesRegex(ValueError, 'Invalid'):
            backends.frame_stability([float('nan')], [0.], 144.)

    def test_stability_wins_even_if_another_backend_has_lower_p95(self):
        good = {'frame_p50_score_ms': 1., 'frame_p95_score_ms': 1.2,
                'frame_p99_score_ms': 1.5,
                **backends.frame_stability([1.] * 1000, [0.2] * 1000, 144.)}
        spiky = {'frame_p50_score_ms': 0.8, 'frame_p95_score_ms': 1.0,
                 'frame_p99_score_ms': 1.1,
                 **backends.frame_stability([0.7] * 999 + [40.],
                                           [0.2] * 999 + [35.], 144.)}
        ordered = sorted([spiky, good], key=backends.stability_sort_key)
        self.assertIs(ordered[0], good)

    def test_technical_p999_tie_uses_repeated_p95_p99_consistency(self):
        def candidate(backend, p95, p99, p999):
            return {'backend': backend, 'frame_p50_score_ms': 0.8,
                    'frame_p95_score_ms': p95, 'frame_p99_score_ms': p99,
                    **{**backends.frame_stability([1.] * 1000, [.1] * 1000, 144.),
                       'frame_p999_ms': p999}}

        gles = candidate('opengles2', 1.2044, 1.3421, 1.6905)
        d3d = candidate('direct3d11', 1.2616, 1.3937, 1.6912)
        runs = []
        for backend in ('direct3d11', 'opengles2'):
            for workload in ('baseline', 'culling', 'mixed', 'rotations_xyz'):
                for mode in backends.MODES:
                    for repetition in (1, 2, 3):
                        runs.append({'backend': backend, 'workload': workload,
                                     'mode': mode, 'repetition': repetition,
                                     'frame_p95_ms': 1.0 if backend == 'opengles2' else 1.07,
                                     'frame_p99_ms': 1.2 if backend == 'opengles2' else 1.27})
        result = backends.rank_backends([d3d, gles], runs, platform='win32')
        reverse = backends.rank_backends([gles, d3d], list(reversed(runs)),
                                         platform='win32')
        self.assertEqual([row['backend'] for row in result],
                         ['opengles2', 'direct3d11'])
        self.assertEqual([row['backend'] for row in result],
                         [row['backend'] for row in reverse])
        # Even if the global p95 difference itself is too small to decide,
        # repeated scenario-level wins still count as evidence.
        d3d['frame_p95_score_ms'] = 1.202
        gles['frame_p95_score_ms'] = 1.201
        d3d['frame_p99_score_ms'] = 1.402
        gles['frame_p99_score_ms'] = 1.401
        self.assertEqual(backends.rank_backends([d3d, gles], runs,
                                                platform='win32')[0]['backend'],
                         'opengles2')
        report = backends.render_report(
            result, discovered=['direct3d11', 'opengles2'], failed=[],
            comparisons=[{'equivalent': 1}], reliable=True, run_details=runs)
        self.assertIn('Empate técnico en p99,9', report)
        self.assertIn('Consistencia de p95/p99', report)

        # A real >2% tail difference must still outrank ordinary speed.
        d3d['frame_p999_ms'] = 1.6
        self.assertEqual(backends.rank_backends([gles, d3d], runs,
                                                platform='win32')[0]['backend'],
                         'direct3d11')

    def test_microscopic_changes_do_not_flip_windows_fallback(self):
        def candidate(backend, p95, p99, p999):
            return {'backend': backend, 'frame_p50_score_ms': .8,
                    'frame_p95_score_ms': p95, 'frame_p99_score_ms': p99,
                    **{**backends.frame_stability([1.] * 100, [.1] * 100, 144.),
                       'frame_p999_ms': p999}}
        a = candidate('direct3d11', 1.251, 1.401, 1.7001)
        b = candidate('opengles2', 1.250, 1.400, 1.6999)
        runs = [{'backend': backend, 'workload': 'baseline', 'mode': mode,
                 'repetition': rep, 'frame_p95_ms': value,
                 'frame_p99_ms': value + .2}
                for backend, value in (('direct3d11', 1.25),
                                       ('opengles2', 1.2501))
                for mode in backends.MODES for rep in (1, 2, 3)]
        self.assertEqual(backends.rank_backends([a, b], runs,
                                                platform='win32')[0]['backend'],
                         'direct3d11')
        a['frame_p999_ms'], b['frame_p999_ms'] = (1.6999, 1.7001)
        a['frame_p95_score_ms'], b['frame_p95_score_ms'] = (1.250, 1.251)
        self.assertEqual(backends.rank_backends([b, a], runs,
                                                platform='win32')[0]['backend'],
                         'direct3d11')
        b['frame_over_display_count'] = 1
        self.assertEqual(backends.rank_backends([b, a], runs,
                                                platform='win32')[0]['backend'],
                         'direct3d11')
        with self.assertRaisesRegex(ValueError, 'missing or inconsistent'):
            backends.rank_backends([a, b], runs[:-1], platform='win32')

    def test_reads_measured_window_all_modes_and_runs(self):
        measured = [{'frame_ms': '1', 'render_sdl_present_ms': '0.2'}] * 9 + [
            {'frame_ms': '21', 'render_sdl_present_ms': '20'}]
        presenter = {'renderer_name': 'opengl', 'renderer_vsync': False,
                     'display_refresh_hz': 144,
                     **{field: 'same' for field in backends.DISPLAY_FIELDS
                        if field != 'display_refresh_hz'}}
        with (patch.object(backends, 'read_presenter', return_value=presenter),
              patch.object(backends, 'read_profile',
                           return_value=([{}] * 12, measured))):
            runs, modes, total = backends.read_backend_tails(
                Path('unused'), 'opengl', workloads=['baseline'],
                repetitions=2, warmup=2, frames=10,
                reference_presenter=presenter)
        self.assertEqual(len(runs), 4)
        self.assertEqual(len(modes), 2)
        self.assertEqual(total['measured_frames'], 40)
        self.assertEqual(total['frame_over_60hz_count'], 4)
        self.assertEqual(total['frame_over_display_count'], 4)
        self.assertEqual(total['sdl_present_over_60hz_count'], 4)
        self.assertEqual(modes[0]['frame_over_60hz_count'], 2)
        with (patch.object(backends, 'read_presenter',
                           return_value={**presenter, 'renderer_vsync': True}),
              patch.object(backends, 'read_profile',
                           return_value=([{}] * 12, measured))):
            with self.assertRaisesRegex(ValueError, 'VSync must be disabled'):
                backends.read_backend_tails(
                    Path('unused'), 'opengl', workloads=['baseline'],
                    repetitions=2, warmup=2, frames=10,
                    reference_presenter=presenter)
        incomplete = [dict(row) for row in measured]
        incomplete[-1].pop('render_sdl_present_ms')
        with (patch.object(backends, 'read_presenter', return_value=presenter),
              patch.object(backends, 'read_profile',
                           return_value=([{}] * 12, incomplete))):
            with self.assertRaisesRegex(ValueError, 'missing/invalid SDL present'):
                backends.read_backend_tails(
                    Path('unused'), 'opengl', workloads=['baseline'],
                    repetitions=2, warmup=2, frames=10,
                    reference_presenter=presenter)

    def test_orchestrator_builds_report_from_two_complete_mock_backends(self):
        with tempfile.TemporaryDirectory() as temp:
            workspace = Path(temp)
            executable = workspace / 'XEngine Benchmark'
            executable.touch()
            observed_environments = []
            observed_commands = []

            def fake_sweep(command, *, cwd, env):
                driver = env['SDL_RENDER_DRIVER']
                observed_environments.append(driver)
                observed_commands.append(command)
                folder = Path(command[command.index('--output') + 1])
                folder.mkdir(parents=True, exist_ok=True)
                rows = [sample_pair(rep=rep, renderer=driver,
                                    value=0.4 if driver == 'opengles2' else 1.0)
                        for rep in (1, 2)]
                with (folder / 'paired_comparisons.csv').open('w', newline='') as stream:
                    writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
                    writer.writeheader()
                    writer.writerows(rows)
                return 0

            def fake_presenter(path):
                name = path.parts[path.parts.index('backends') + 1]
                return {'renderer_name': name,
                        'renderer_vsync': False,
                        **{field: 144 if field == 'display_refresh_hz' else 'stable'
                           for field in backends.DISPLAY_FIELDS}}

            def fake_tail(root, backend, **kwargs):
                samples = [0.8] * 999 + ([21.] if backend == 'opengles2' else [0.8])
                present = [0.2] * 999 + ([20.] if backend == 'opengles2' else [0.2])
                result = backends.frame_stability(samples, present, 144.)
                summary = {'backend': backend, **result}
                mode_rows = [{'backend': backend, 'workload': 'culling', 'mode': mode,
                              **result} for mode in backends.MODES]
                run_rows = [{'backend': backend, 'workload': 'culling',
                             'mode': mode, 'repetition': rep, **result}
                            for mode in backends.MODES for rep in (1, 2)]
                return run_rows, mode_rows, summary

            complete = [{'backend_a': 'opengl', 'backend_b': 'opengles2',
                         'workload': 'culling', 'repetition': 1, 'mode': 'fixed',
                         'positions_compared': 540, 'equivalent': 1,
                         'details': '540 equivalent frames'}]
            probe = subprocess.CompletedProcess(
                args=['probe'], returncode=0, stderr='',
                stdout=backends.PROBE_MARKER +
                '[{"name":"opengl","flags":10},{"name":"opengles2","flags":10}]\n')
            output = workspace / 'results'
            with (patch.object(backends, 'executable_default', return_value=executable),
                  patch.object(backends.subprocess, 'run', return_value=probe),
                  patch.object(backends.subprocess, 'call', side_effect=fake_sweep),
                  patch.object(backends, 'read_presenter', side_effect=fake_presenter),
                  patch.object(backends, 'read_backend_tails', side_effect=fake_tail),
                  patch.object(backends, 'compare_backend_invariants',
                               return_value=complete)):
                rc = backends.main(['--skip-build', '--workloads', 'culling',
                                    '--width', '1920', '--height', '1080', '--fullscreen',
                                    '--repetitions', '2', '--output', str(output)])
            self.assertEqual(rc, 0)
            self.assertEqual(observed_environments, ['opengl', 'opengles2'])
            for command in observed_commands:
                self.assertEqual(command[command.index('--width') + 1], '1920')
                self.assertEqual(command[command.index('--height') + 1], '1080')
                self.assertIn('--fullscreen', command)
            metadata = json.loads((output / 'metadata.json').read_text())
            self.assertEqual(metadata['requested_window_width'], 1920)
            self.assertTrue(metadata['requested_fullscreen'])
            report = (output / 'report.md').read_text()
            self.assertIn('Mejor p95 medido', report)
            self.assertIn('`opengles2`', report)
            self.assertIn('Backend recomendado por estabilidad: `opengl`', report)
            self.assertIn('fixed frente a adaptive', report)
            with (output / 'backend_ranking.csv').open(newline='') as stream:
                self.assertEqual(list(csv.DictReader(stream))[0]['backend'], 'opengl')
            self.assertTrue((output / 'backend_tail_modes.csv').exists())
            self.assertFalse((output / 'failures.txt').exists())

            def fake_sweep_with_software(command, *, cwd, env):
                if env['SDL_RENDER_DRIVER'] != 'software':
                    return fake_sweep(command, cwd=cwd, env=env)
                folder = Path(command[command.index('--output') + 1])
                first = folder / 'culling' / 'fixed_run01'
                first.mkdir(parents=True)
                (first / 'failures.txt').write_text(
                    'render_parallel_run01: ' + backends.VSYNC_EXCLUSION_MESSAGE + '\n',
                    encoding='utf-8')
                (folder / 'failures.txt').write_text(
                    'culling/1/fixed: benchmark exited 1\n', encoding='utf-8')
                (folder / 'run_order.csv').write_text(
                    'workload,repetition,mode,runner_exit_code\nculling,1,fixed,1\n',
                    encoding='utf-8')
                return 1

            software_probe = subprocess.CompletedProcess(
                args=['probe'], returncode=0, stderr='',
                stdout=backends.PROBE_MARKER +
                '[{"name":"opengl","flags":10},{"name":"opengles2","flags":10},'
                '{"name":"software","flags":1}]\n')
            with (patch.object(backends, 'executable_default', return_value=executable),
                  patch.object(backends.subprocess, 'run', return_value=software_probe),
                  patch.object(backends.subprocess, 'call',
                               side_effect=fake_sweep_with_software),
                  patch.object(backends, 'read_presenter', side_effect=fake_presenter),
                  patch.object(backends, 'read_backend_tails', side_effect=fake_tail),
                  patch.object(backends, 'compare_backend_invariants',
                               return_value=complete)):
                rc = backends.main(['--skip-build', '--workloads', 'culling',
                                    '--repetitions', '2',
                                    '--output', str(workspace / 'with_software')])
            self.assertEqual(rc, 0)
            metadata = json.loads((workspace / 'with_software/metadata.json').read_text())
            self.assertTrue(metadata['ranking_valid'])
            self.assertEqual(metadata['excluded_backends'], ['software'])
            with (workspace / 'with_software/backend_exclusions.csv').open(newline='') as stream:
                self.assertEqual(list(csv.DictReader(stream))[0]['reason_code'],
                                 backends.VSYNC_EXCLUSION_CODE)
            with (workspace / 'with_software/backend_failures.csv').open(newline='') as stream:
                self.assertFalse(list(csv.DictReader(stream)))
            self.assertIn('Backends no comparables excluidos: 1',
                          (workspace / 'with_software/report.md').read_text())


if __name__ == '__main__':
    unittest.main()
