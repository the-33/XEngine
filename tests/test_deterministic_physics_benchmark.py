"""Regression coverage for per-frame collision instrumentation and fixed workload.

The C++ source has no SDL2 SDK in some CI environments. This suite tests the
runner behavior with real CSV fixtures and checks critical engine integration.
A compiled SDL2 gameplay run is still required to verify timing on hardware.
"""
from __future__ import annotations

import csv
import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('benchmark_runner_v7', ROOT / 'tools/run_benchmark.py')
runner = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runner)


class DeterministicBenchmarkTests(unittest.TestCase):
    def make_profile(self, root: Path, rows: list[dict]) -> Path:
        profile = root / 'profile.csv'
        columns = sorted(set().union(*(row.keys() for row in rows)))
        with profile.open('w', newline='', encoding='utf-8') as stream:
            writer = csv.DictWriter(stream, fieldnames=columns)
            writer.writeheader()
            writer.writerows(rows)
        return profile

    def sample(self, steps: int = 2, **overrides):
        return {
            'frame_ms': 10, 'physics_ms': 5, 'fixed_ms': 5,
            'collision_ms': 4, 'collision_snapshot_ms': 0.5,
            'collision_candidates_ms': 1.5, 'collision_merge_ms': 0.5,
            'render_prepare_ms': 0.5, 'render_path_reason': 3,
            'broadphase_tests': 400, 'fixed_steps': steps,
            'deterministic_steps_per_frame': steps,
            'collision_passes': 4, 'collision_parallel_passes': 4,
            'physics_substeps': 2, 'contacts_built': 5,
            'render_world_draws': 200, **overrides,
        }

    def test_workload_validator_checks_warmup_and_every_measured_frame(self):
        with tempfile.TemporaryDirectory() as d:
            p = self.make_profile(Path(d), [self.sample() for _ in range(4)])
            runner.assert_fixed_workload(p, warmup=1, frames=3, fixed_steps=2)
            self.assertEqual(runner.expected_timing_marker('deterministic', 2),
                             '[BENCH-TIMING] mode=deterministic fixed_steps_per_frame=2')
            self.assertEqual(runner.expected_timing_marker('realtime', 1),
                             '[BENCH-TIMING] mode=realtime fixed_steps_per_frame=0')

    def test_workload_validator_rejects_incorrect_warmup_or_measured_step(self):
        for failed_index in (0, 3):
            with self.subTest(failed_index=failed_index), tempfile.TemporaryDirectory() as d:
                rows = [self.sample() for _ in range(4)]
                rows[failed_index]['fixed_steps'] = 1
                p = self.make_profile(Path(d), rows)
                with self.assertRaisesRegex(ValueError, f'frame {failed_index}'):
                    runner.assert_fixed_workload(p, warmup=1, frames=3, fixed_steps=2)

    def test_workload_validator_rejects_wrong_scheduler_metadata(self):
        with tempfile.TemporaryDirectory() as d:
            rows = [self.sample() for _ in range(3)]
            rows[0]['deterministic_steps_per_frame'] = 0
            p = self.make_profile(Path(d), rows)
            with self.assertRaisesRegex(ValueError, 'deterministic_steps_per_frame'):
                runner.assert_fixed_workload(p, warmup=1, frames=2, fixed_steps=2)

    def test_workload_validator_rejects_missing_or_short_profiles(self):
        with tempfile.TemporaryDirectory() as d:
            p = self.make_profile(Path(d), [{'fixed_steps': 1}, {'fixed_steps': 1}])
            with self.assertRaisesRegex(ValueError, 'missing fixed-step verification'):
                runner.assert_fixed_workload(p, warmup=0, frames=2, fixed_steps=1)
            p = self.make_profile(Path(d), [self.sample()])
            with self.assertRaisesRegex(ValueError, 'too few profile rows'):
                runner.assert_fixed_workload(p, warmup=1, frames=2, fixed_steps=2)

    def test_physical_workload_totals_and_normalization_use_sum_of_ratios(self):
        with tempfile.TemporaryDirectory() as d:
            rows = [self.sample() for _ in range(3)]
            rows[1]['fixed_steps'] = 1
            rows[1]['collision_passes'] = 2
            rows[1]['collision_ms'] = 2
            rows[1]['collision_candidates_ms'] = 1
            p = self.make_profile(Path(d), rows)
            result = runner.measure(p, warmup=0, frames=3)
            self.assertEqual(result['fixed_steps_total'], 5)
            self.assertEqual(result['collision_passes_total'], 10)
            self.assertAlmostEqual(result['collision_ms_per_fixed_step'], 10 / 5)
            self.assertAlmostEqual(result['collision_candidates_ms_per_collision_pass'], 4 / 10)
            self.assertEqual(result['collision_parallel_passes_total'], 12)

    def test_engine_collects_all_build_contacts_passes_and_resets_each_frame(self):
        engine = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        collision = (ROOT / 'engine/src/CollisionManager.cpp').read_text(encoding='utf-8')
        physics = (ROOT / 'engine/src/PhysicsManager.cpp').read_text(encoding='utf-8')
        self.assertIn('CollisionManager::GetInstancePtr()->ResetFrameProfile_();', engine)
        self.assertIn('AccumulatePassProfile_(out.size());', collision)
        self.assertIn('mFrameBroadphaseTests +=', collision)
        self.assertIn('sample.broadphaseTests = col->mFrameBroadphaseTests;', engine)
        self.assertIn('sample.collisionCandidatesMs = col->mFrameCandidateMs;', engine)
        self.assertIn('mFramePhysicsStepSec += physics->stepTimeSec;', engine)
        self.assertIn('mFramePhysicsSubsteps +=', engine)
        self.assertIn('collision->BuildContacts_(contacts);', physics)
        self.assertIn('collision->DetectAndDispatch();', engine)
        self.assertIn('collision_parallel_passes', engine)

    def test_engine_scheduler_is_opt_in_and_independent_of_thread_flags(self):
        header = (ROOT / 'engine/include/Engine.h').read_text(encoding='utf-8')
        engine = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        game = (ROOT / 'games/XEngine Benchmark/src/main.cpp').read_text(encoding='utf-8')
        self.assertIn('mDeterministicFixedStepsPerFrame = 0', header)
        self.assertIn('ConfigureDeterministicFixedSteps(unsigned steps)', header)
        self.assertIn('if (mDeterministicFixedStepsPerFrame != 0u)', engine)
        self.assertIn('for (unsigned i = 0; i < mDeterministicFixedStepsPerFrame; ++i)', engine)
        self.assertIn('while (mAccumulator >= mFixedDt)', engine)
        self.assertIn('time->delta = mFixedDt *', engine)
        self.assertIn('Engine::ConfigureDeterministicFixedSteps(requestedSteps)', game)
        self.assertIn('[BENCH-TIMING] mode=', game)


if __name__ == '__main__':
    unittest.main()
