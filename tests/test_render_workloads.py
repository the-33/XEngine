"""Regression checks for separately calibrated render stress workloads."""
from __future__ import annotations
import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('render_sweep_extended', ROOT / 'tools' / 'sweep_render_threshold.py')
sweep = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sweep)


class RenderWorkloadTests(unittest.TestCase):
    def test_renderer_modes_are_explicit_and_baseline_is_default(self):
        opts = sweep.arguments(['--counts', '128', '384'])
        self.assertEqual(opts.workloads, ['baseline'])
        self.assertEqual(sweep.arguments(['--workloads', 'transforms','mixed']).workloads,
                         ['transforms','mixed'])
        src = (ROOT / 'games/XEngine Benchmark/assets/scenes/BenchmarkScene.hpp').read_text()
        for variant in ('baseline', 'transforms', 'tint', 'culling', 'mixed'):
            self.assertIn(f'workload == "{variant}"', src)
        self.assertIn('std::string renderWorkload = "baseline"', src)

    def test_transform_color_culling_and_screen_draw_paths_exist(self):
        src = (ROOT / 'games/XEngine Benchmark/assets/scenes/BenchmarkScene.hpp').read_text()
        animation = (ROOT / 'games/XEngine Benchmark/assets/scripts/BenchmarkRenderExercise.hpp').read_text()
        for token in ('localRotation', 'pivot01', 'flipX', 'flipY', 'source', 'tint'):
            self.assertIn(token, src)
        for token in ('DrawRectScreen', 'DrawImageScreen', 'DrawTextScreen',
                      'DrawLine', 'DrawRect', 'camera->zoom', 'camera->center'):
            self.assertIn(token, animation)
        self.assertIn('render_workload=%s', src)

    def test_all_profiles_are_passed_through_runner(self):
        runner = (ROOT / 'tools/run_benchmark.py').read_text()
        main = (ROOT / 'games/XEngine Benchmark/src/main.cpp').read_text()
        self.assertIn("'--render-workload', args.render_workload", runner)
        self.assertIn('else if (key == "--render-workload")', main)
        self.assertIn('!IsKnownRenderWorkload(opts.renderWorkload)', main)

    def test_aggregated_results_never_mix_workload_types(self):
        rows = []
        for workload, delta in (('baseline',-0.1), ('transforms',0.1)):
            for n in (256,512):
                for repeat in range(3):
                    rows.append(dict(workload=workload,sprites=n,repetition=repeat+1,
                                     immediate_p50_ms=2.,parallel_p50_ms=2.-delta,
                                     immediate_p95_ms=3.,parallel_p95_ms=2.9,
                                     gain_pct=100*delta/2,render_eligible_median=n,
                                     render_worker_frames=360))
        summary = sweep.aggregate(rows)
        self.assertEqual(len(summary),4)
        self.assertEqual(len(sweep.crossing(summary)),0)
        candidates=sweep.recommendation(summary)
        self.assertEqual(len(candidates),2)
        self.assertEqual({r['workload'] for r in candidates},{'baseline','transforms'})
        self.assertEqual({r['workload']:r['status'] for r in candidates}['baseline'],'not_confirmed')
        self.assertEqual({r['workload']:r['status'] for r in candidates}['transforms'],'candidate_only')

    def test_public_world_and_screen_drawing_api_remains_accessible(self):
        hdr = (ROOT / 'engine/include/RenderManager.h').read_text()
        start = hdr.index('    // PUBLIC API (normal)')
        end = hdr.index('    // PRIVATE / INTERNAL (immediate)')
        self.assertIn('public:', hdr[:start][-200:])
        self.assertIn('private:', hdr[:end][-200:])

    def test_screen_draws_are_classified_as_ui_in_both_render_paths(self):
        # Regression for the mixed workload: immediate drawing previously
        # attributed four screen commands to render_world_draws, while
        # snapshot replay classified them as render_ui_draws.
        renderer = (ROOT / 'engine/src/RenderManager.cpp').read_text()
        for public_name, internal_name in (
            ('DrawRectScreen', 'DrawRectScreenImmediate_'),
            ('DrawImageScreen', 'DrawImageScreenImmediate_'),
            ('DrawTextScreen', 'DrawTextScreenImmediate_'),
        ):
            start = renderer.index('void RenderManager::' + public_name + '(')
            end = renderer.index('\n}', start)
            body = renderer[start:end]
            scoped = ('const DrawPhase previousPhase = mDrawPhase;\n'
                      '    mDrawPhase = DrawPhase::UI;\n'
                      '    ' + internal_name + '(')
            self.assertIn(scoped, body, public_name)
            self.assertIn('mDrawPhase = previousPhase;', body, public_name)
        replay_start = renderer.index('void RenderManager::RenderSnapshot(')
        replay_end = renderer.index('void RenderManager::BeginRecord(', replay_start)
        replay = renderer[replay_start:replay_end]
        self.assertIn('DrawPhase::UI : DrawPhase::World', replay)
        self.assertIn('case DrawPhase::World: ++mWorldDraws;', renderer)
        self.assertIn('case DrawPhase::UI: ++mUiDraws;', renderer)

    def test_mixed_equivalence_requires_world_and_ui_counts(self):
        self.assertIn('render_world_draws', sweep.VERIFY)
        self.assertIn('render_ui_draws', sweep.VERIFY)
        sample = dict.fromkeys(sweep.VERIFY, '0')
        altered = sample.copy()
        altered['render_world_draws'] = '4'
        status, message = sweep.compare_profiles([sample], [altered])
        self.assertEqual(status, 1)
        self.assertIn('render_world_draws', message)
        altered = sample.copy()
        altered['render_ui_draws'] = '4'
        status, message = sweep.compare_profiles([sample], [altered])
        self.assertEqual(status, 1)
        self.assertIn('render_ui_draws', message)

    def test_equivalence_checks_additional_draw_counters(self):
        row = dict.fromkeys(sweep.VERIFY, '0')
        other = row.copy()
        other['render_tint_updates']='1'
        status, detail = sweep.compare_profiles([row], [other])
        self.assertEqual(status,1)
        self.assertIn('render_tint_updates',detail)


if __name__ == '__main__':
    unittest.main()
