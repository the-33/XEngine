"""Regression tests for stable auto-cell selection and the paired benchmark runner."""
from __future__ import annotations

import csv
import shutil
import subprocess
import sys
import tempfile
from unittest import mock
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import compare_adaptive_broadphase as paired


class AdaptiveStabilityTests(unittest.TestCase):
    def test_recent_costs_confirmation_backoff_and_drift(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('A C++20 compiler is required')
        program = r'''
#include "AdaptiveBroadphase.h"
#include <cassert>
#include <cmath>

int main() {
    // Moderately noisy configurations should NOT cause repeated switching.
    AdaptiveCellBroadphase a;
    unsigned probesEarly=0, probesLate=0, switches=0;
    unsigned last=0;
    for (int i=0; i<1250; ++i) {
        auto c = a.Choose(740, true, 1);
        float cost = c.strategy == 0 ? 1.0f : 0.96f;
        // Intermittent OS-style timing spikes and tiny 5% differences.
        if (i % 17 == 0) cost *= 1.24f;
        if (i >= 20 && c.strategy != last && !c.probe) ++switches;
        if (c.probe && i >= 20 && i < 300) ++probesEarly;
        if (c.probe && i >= 950) ++probesLate;
        a.Observe(740,true,1,c.strategy,cost);
        if (!c.probe) last=c.strategy;
    }
    assert(switches == 0);
    assert(probesEarly > probesLate); // stable bucket throttles probes
    assert(probesLate < 10);

    // New work distribution within the SAME occupancy bucket: grid2 becomes
    // significantly faster. Three fresh confirmations prevent one-off switches.
    unsigned winner2=0;
    for (int i=0; i<1700; ++i) {
        auto c=a.Choose(740,true,1);
        const float cost = c.strategy==2 ? 0.08f : 3.0f;
        a.Observe(740,true,1,c.strategy,cost);
        if (i>1500 && c.strategy==2 && !c.probe) ++winner2;
    }
    assert(winner2 > 160);
    // Worker and occupancy contexts must be isolated.
    assert(a.Choose(740,false,1).strategy==0);
    assert(a.Choose(740,true,2).strategy==0);
    a.Reset();
    assert(a.Choose(740,true,1).strategy==0);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            src = Path(folder) / 'adaptive.cpp'
            exe = Path(folder) / 'adaptive'
            src.write_text(program, encoding='utf-8')
            proc = subprocess.run([compiler, '-std=c++20', '-O2', '-Wall', '-Wextra',
                                   '-Werror', '-I' + str(ROOT / 'engine/include'),
                                   str(src), '-o', str(exe)],
                                  capture_output=True, text=True, timeout=30)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            proc = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
            self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_paired_order_rotates_and_contains_all_strategies(self):
        for repetition in range(1, 6):
            for scenario_index in range(3):
                order = paired.rotated_order(repetition, scenario_index, 0)
                self.assertEqual(set(order), set(paired.STRATEGIES))
                self.assertEqual(len(order), 4)
        orders = [paired.rotated_order(i, 0, 0) for i in range(1, 5)]
        self.assertGreaterEqual(len(set(orders)), 3)

    def test_paired_summary_and_comparison_calculations(self):
        samples = []
        for rep in range(1, 4):
            for strategy, time in [('bruteforce', 4.0), ('grid1', 3.0),
                                   ('grid2', 2.0), ('auto_cells', 2.1)]:
                samples.append({'scenario': 'stress', 'mode': 'collision',
                                'repetition': rep, 'strategy': strategy,
                                'frame_ms_p50': time + rep * .01,
                                'frame_ms_p95': time + .4,
                                'broadphase_cost_ms_mean': time * .4,
                                'collision_auto_calibration_passes_total': 20 if strategy=='auto_cells' else 0})
        report = paired.median_summary(samples)
        self.assertEqual(len(report), 4)
        auto = next(x for x in report if x['strategy']=='auto_cells')
        self.assertEqual(auto['best_fixed_strategy'], 'grid2')
        self.assertLess(float(auto['auto_vs_best_fixed_pct']), 0)
        comparisons = paired.paired_comparisons(samples)
        self.assertEqual(len(comparisons), 9)
        self.assertEqual({r['baseline'] for r in comparisons}, {'grid1', 'grid2', 'bruteforce'})

    def test_runner_pipeline_with_stubbed_engine_profiles(self):
        # Exercise the orchestration, CSV readers and equivalence checks without SDL.
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            executable = root / 'fake.exe'
            executable.write_bytes(b'fake')
            out = root / 'results'

            def simulated_run(command, logfile):
                dest = Path(command[command.index('--output') + 1])
                scenario = command[command.index('--scenarios') + 1]
                mode = command[command.index('--modes') + 1]
                path = dest / f'{scenario}_{mode}_run01.csv'
                columns = ['frame_ms', 'collision_ms', 'collision_candidates_ms',
                           'collision_grid_build_ms', 'render_prepare_ms', 'render_path_reason',
                           'broadphase_tests', 'fixed_steps', 'render_world_draws',
                           'collision_contact_signature', 'contacts_built', 'narrowphase_tests',
                           'collision_auto_calibration_passes']
                with path.open('w', newline='') as fd:
                    writer = csv.DictWriter(fd, fieldnames=columns)
                    writer.writeheader()
                    for frame in range(2):
                        writer.writerow({'frame_ms': 2.0, 'collision_ms': 0.3,
                                         'collision_candidates_ms': 0.2,
                                         'collision_grid_build_ms': 0.1,
                                         'render_prepare_ms': 0.1,
                                         'render_path_reason': 0,
                                         'broadphase_tests': 10, 'fixed_steps': 1,
                                         'render_world_draws': 100,
                                         'collision_contact_signature': 12345,
                                         'contacts_built': 1, 'narrowphase_tests': 2,
                                         'collision_auto_calibration_passes': 0})
                (dest / 'summary.csv').write_text('scenario,mode\nlight,single\n')

            with mock.patch.object(paired, 'run_command', side_effect=simulated_run):
                rc = paired.main(['--exe', str(executable), '--output', str(out),
                                  '--scenarios', 'light', '--modes', 'single',
                                  '--repetitions', '1', '--frames', '1', '--warmup', '1'])
            self.assertEqual(rc, 0)
            self.assertTrue((out / 'summary.csv').is_file())
            self.assertTrue((out / 'paired_comparisons.csv').is_file())
            with (out / 'equivalence.csv').open() as fd:
                equiv = list(csv.DictReader(fd))
            self.assertEqual(len(equiv), 3)
            self.assertTrue(all(r['equivalent'] == 'True' for r in equiv))

    def test_profiler_exports_selector_overhead(self):
        cpp = (ROOT / 'engine/src/Engine.cpp').read_text(encoding='utf-8')
        collision = (ROOT / 'engine/src/CollisionManager.cpp').read_text(encoding='utf-8')
        self.assertIn('collision_auto_selector_ms', cpp)
        self.assertIn('sample.collisionAutoSelectorMs', cpp)
        self.assertIn('mFrameAutoSelectorMs +=', collision)


if __name__ == '__main__':
    unittest.main()
