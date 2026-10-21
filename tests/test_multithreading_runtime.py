"""Multithreading contract and the standalone CPU executor, without SDL dev packages."""
from __future__ import annotations

import csv
import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class ThreadingTests(unittest.TestCase):
    def test_render_and_fixed_update_are_main_thread_only(self):
        code = (ROOT / 'engine/src/Engine.cpp').read_text()
        self.assertNotIn('SDL_CreateThread', code)
        self.assertNotIn('SDL_CondWait', code)
        self.assertIn('return RunSingleThreadingImpl();', code)
        self.assertIn('collision->EnableParallelCollision(true);', code)
        self.assertIn('render->End();', code)

    def test_both_modes_use_same_monotonic_clock(self):
        code = (ROOT / 'engine/src/Engine.cpp').read_text()
        self.assertIn('std::chrono::steady_clock', code)
        for phase in ('mFixedUpdateTime', 'mPollInputTime', 'mUpdateTime', 'mRenderTime'):
            self.assertIn(f'{phase} = ProfileElapsed_', code)
        self.assertNotIn('(float) time->timeSinceStart -', code)

    def test_collision_workers_do_not_touch_scene_or_dispatch_callbacks(self):
        code = (ROOT / 'engine/src/CollisionManager.cpp').read_text()
        start = code.index('const auto processCollider =')
        end = code.index('// Avoid the synchronization cost', start)
        worker = code[start:end]
        for forbidden in ('OnCollisionEnter(', 'OnCollisionStay(', 'OnTriggerEnter(',
                          'WorldAABB(', 'GetAttachedBody(', 'mCurrPairs.insert('):
            self.assertNotIn(forbidden, worker)
        self.assertIn('n >= 192', code)
        self.assertIn('EnableParallelCollision(false)', code)

    @unittest.skipUnless(shutil.which('g++'), 'g++ is unavailable')
    def test_worker_pool_dispatch_and_reuse(self):
        cpp = r'''#include "ParallelExecutor.h"
#include <atomic>
#include <vector>
#include <functional>
#include <cassert>
int main() {
    ParallelExecutor executor(3);
    for (int round = 0; round < 40; ++round) {
        std::vector<std::atomic<int>> hits(1024);
        for (auto& h : hits) h.store(0);
        const std::function<void(std::size_t)> f = [&](std::size_t i) { hits[i].fetch_add(1); };
        executor.Run(hits.size(), f);
        for (const auto& h : hits) assert(h.load() == 1);
    }
    return 0;
}'''
        with tempfile.TemporaryDirectory() as tmp:
            src = Path(tmp) / 'pool.cpp'
            exe = Path(tmp) / 'pool'
            src.write_text(cpp)
            subprocess.run(['g++', '-std=c++20', '-O2', '-pthread',
                            '-I', str(ROOT / 'engine/include'), str(src), '-o', str(exe)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, timeout=15)

    def test_csv_reader_reports_percentiles(self):
        spec = importlib.util.spec_from_file_location('compare_profiles', ROOT / 'tools/compare_profiles.py')
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertAlmostEqual(module.percentile([10, 20, 30, 40], .5), 25.)
        self.assertAlmostEqual(module.percentile([10, 20, 30, 40], .95), 38.5)
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'test.csv'
            names = [key for key, label in module.METRICS] + ['fixed_steps', 'narrowphase_tests']
            with p.open('w', newline='') as f:
                writer = csv.DictWriter(f, names)
                writer.writeheader()
                for i in range(150):
                    writer.writerow({name: i for name in names})
            rows = module.read_profile(p, 120)
            self.assertEqual(len(rows), 30)
            self.assertEqual(rows[0]['frame_ms'], 120)
            self.assertIn('Frame wall time', module.report(rows, rows))


if __name__ == '__main__':
    unittest.main()
