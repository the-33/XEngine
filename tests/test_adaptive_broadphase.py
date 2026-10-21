"""Adaptive selector and benchmark grid-size calibration regressions."""
import csv
import shutil
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import sweep_broadphase as sweep


class AdaptiveBroadphaseTests(unittest.TestCase):
    def test_selector_measures_and_adapts_without_thrashing(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('A C++20 compiler is required')
        source = r'''
#include "AdaptiveBroadphase.h"
#include <cassert>
int main() {
    AdaptiveBroadphase a;
    bool grid = false;
    // Grid clearly faster for large spread-out workload.
    for (int i=0;i<120;++i) {
        grid = a.Choose(740, true);
        a.Observe(740, true, grid, grid ? 1.f : 5.f);
    }
    for (int i=0;i<30;++i) {
        grid = a.Choose(740, true);
        assert(grid || i == 23); // scheduled occasional brute probe
        a.Observe(740, true, grid, grid ? 1.f : 5.f);
    }
    // Same collider count, single-thread results may be different.
    for (int i=0;i<90;++i) {
        grid = a.Choose(740, false);
        a.Observe(740, false, grid, grid ? 5.f : 1.f);
    }
    for (int i=0;i<10;++i) {
        grid = a.Choose(740, false);
        assert(!grid);
        a.Observe(740, false, grid, grid ? 5.f : 1.f);
    }
    // Spatial distribution changes; periodic probes can update the model.
    for (int i=0;i<350;++i) {
        grid = a.Choose(740, true);
        a.Observe(740, true, grid, grid ? 12.f : 0.1f);
    }
    int bruteSelections=0;
    for (int i=0;i<30;++i) {
        grid = a.Choose(740, true);
        if (!grid) ++bruteSelections;
        a.Observe(740, true, grid, grid ? 12.f : 0.1f);
    }
    assert(bruteSelections>=25);
    a.Reset();
    assert(!a.Choose(740,true));
}
'''
        with tempfile.TemporaryDirectory() as d:
            src = Path(d)/'auto.cpp'; exe = Path(d)/'auto'
            src.write_text(source)
            proc = subprocess.run([compiler, '-std=c++20', '-O2', '-Wall','-Wextra','-Werror',
                                   '-I'+str(ROOT/'engine/include'), str(src), '-o', str(exe)],
                                  capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)
            proc = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(proc.returncode, 0, proc.stderr)

    def test_crossover_interpolation_and_no_extrapolation(self):
        self.assertEqual(sweep.crossovers([(100,-1),(300,1)]), [200.])
        self.assertEqual(sweep.crossovers([(100,2),(200,3)]), [])
        self.assertEqual(sweep.crossovers([(100,0),(200,2)]), [100.])

    def test_aggregate_preserves_each_cell_size_and_median(self):
        rows = []
        for n in [100, 300]:
            for rep in [1,2,3]:
                rows.append({'colliders': n, 'mode': 'single', 'broadphase': 'bruteforce',
                             'cell_size': 2., 'frame_ms_p50': 5.})
                for cell in [1., 4.]:
                    value = (6 if n == 100 else 4) if cell == 1. else 3.
                    rows.append({'colliders': n, 'mode': 'single', 'broadphase': 'grid',
                                 'cell_size': cell, 'frame_ms_p50': float(value)})
        comp, crosses = sweep.aggregate(rows, [100,300], ['single'], [1.,4.])
        self.assertEqual(len(comp), 4)
        self.assertEqual(len(crosses), 1)
        self.assertEqual(crosses[0]['interpolated_colliders'], 200)

    def test_config_and_profile_integration_declared(self):
        header = (ROOT/'engine/include/Engine.h').read_text()
        cpp = (ROOT/'engine/src/Engine.cpp').read_text()
        bench = (ROOT/'tools/run_benchmark.py').read_text()
        self.assertIn('collisionGridPasses', header)
        self.assertIn('collision_grid_passes,collision_bruteforce_passes', cpp)
        self.assertIn('mode != "auto"', cpp)
        self.assertIn("'auto': 2.0", bench)
        self.assertIn("'--colliders'", bench)

if __name__ == '__main__':
    unittest.main()
