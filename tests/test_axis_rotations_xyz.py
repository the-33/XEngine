"""Regression tests for reproducible three-axis sprite rendering workloads."""
from __future__ import annotations
from pathlib import Path
import importlib.util
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SCRIPTS = ROOT / 'games/XEngine Benchmark/assets/scripts'
SCENE = ROOT / 'games/XEngine Benchmark/assets/scenes/BenchmarkScene.hpp'


class AxisRotationsXYZTests(unittest.TestCase):
    def test_workload_is_selectable_and_prior_workloads_remain(self):
        main = (ROOT / 'games/XEngine Benchmark/src/main.cpp').read_text()
        runner = (ROOT / 'tools/run_benchmark.py').read_text()
        scene = SCENE.read_text()
        for token in ('baseline', 'transforms', 'tint', 'culling', 'mixed', 'rotations_xyz'):
            self.assertIn(token, scene)
            self.assertIn(token, runner)
        self.assertIn('BenchmarkAxisRotation(i, 0u)', scene)
        self.assertIn('BenchmarkAxisRotation(index_, frame_)', (SCRIPTS / 'BenchmarkRenderExercise.hpp').read_text())
        self.assertIn('rotations_xyz', main)
        self.assertIn('rotations_xyz', (ROOT / 'tools/sweep_render_threshold.py').read_text())

    def test_sampling_has_all_axes_independent_and_combined_and_remains_deterministic(self):
        compiler = next((p for p in ('g++', 'clang++') if shutil.which(p)), None)
        if compiler is None:
            self.skipTest('No standalone C++20 compiler')
        source = r'''
#include <cmath>
#include <cassert>
struct Vec3 { float x, y, z; };
#include "BenchmarkAxisRotations.hpp"
int main() {
    using XEngineBenchmark::BenchmarkAxisRotation;
    auto x = BenchmarkAxisRotation(0u, 0u);
    auto y = BenchmarkAxisRotation(1u, 0u);
    auto z = BenchmarkAxisRotation(2u, 0u);
    auto xyz = BenchmarkAxisRotation(3u, 0u);
    auto xy = BenchmarkAxisRotation(4u, 0u);
    assert(x.x > 88.f && x.y == 0.f && x.z == 0.f);
    assert(y.x == 0.f && y.y > 88.f && y.z == 0.f);
    assert(z.x == 0.f && z.y == 0.f && z.z != 0.f);
    assert(xyz.x != 0.f && xyz.y != 0.f && xyz.z != 0.f);
    assert(xy.x != 0.f && xy.y != 0.f && xy.z == 0.f);
    assert(BenchmarkAxisRotation(3u, 53u).z != xyz.z);
    for(unsigned frame = 0;frame < 480u; ++frame) {
        for(unsigned index = 0;index < 2048u;++index) {
            const auto a = BenchmarkAxisRotation(index, frame);
            const auto b = BenchmarkAxisRotation(index, frame);
            assert(a.x == b.x && a.y == b.y && a.z == b.z);
            assert(std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z));
            assert(std::fabs(a.x) < 90.f && std::fabs(a.y) < 90.f);
        }
    }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            cpp = tmp / 'rotations.cpp'
            output = tmp / 'rotations'
            cpp.write_text(source)
            compile_result = subprocess.run(
                [compiler, '-std=c++20', '-O2', '-I', str(SCRIPTS), str(cpp), '-o', str(output)],
                capture_output=True, text=True, timeout=60,
            )
            self.assertEqual(compile_result.returncode, 0, compile_result.stderr)
            execute_result = subprocess.run([str(output)], capture_output=True, text=True, timeout=60)
            self.assertEqual(execute_result.returncode, 0, execute_result.stderr)

    def test_sweep_no_longer_rejects_xyz_profile(self):
        spec = importlib.util.spec_from_file_location('render_sweep_xyz', ROOT / 'tools/sweep_render_threshold.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        args = module.arguments(['--workloads', 'rotations_xyz', '--counts', '384', '1024'])
        self.assertEqual(args.workloads, ['rotations_xyz'])
        self.assertEqual(args.counts, [384,1024])


if __name__ == '__main__':
    unittest.main()
