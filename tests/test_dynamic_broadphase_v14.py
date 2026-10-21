"""Regression coverage for spatial layout changes and adaptive profiling v14."""
from __future__ import annotations
import csv
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import compare_adaptive_broadphase as paired


class DynamicBroadphaseTests(unittest.TestCase):
    def test_concentration_contexts_and_low_load_timing_noise(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('C++20 compiler missing')
        program = r'''
#include "AdaptiveBroadphase.h"
#include <cassert>
int main() {
    std::array<unsigned,16> even{}; even.fill(10);
    std::array<unsigned,16> center{}; center[5]=160;
    std::array<unsigned,16> pair{}; pair[0]=80; pair[15]=80;
    assert(AdaptiveCellBroadphase::ConcentrationClass(even)==2);
    assert(AdaptiveCellBroadphase::ConcentrationClass(center)==0);
    assert(AdaptiveCellBroadphase::ConcentrationClass(pair)==0);
    AdaptiveCellBroadphase a;
    // Nine contexts (occupancy 0-2 x concentration 0-2) must calibrate
    // independently. Interleaved inexpensive timing jitter must not keep
    // waking up probes once the selector has become stable.
    for (unsigned d=0;d<9;++d) {
        assert(a.Choose(64,true,d).strategy==0);
    }
    unsigned early=0,late=0;
    for(int i=0;i<1800;++i) {
        auto c=a.Choose(64,true,8);
        if(c.probe && i>60 && i<300) ++early;
        if(c.probe && i>1400) ++late;
        // Simulated sub-ms timing noise: very large percent variation but
        // bounded absolute amplitude; no real workload regression.
        const float cost=0.008f + ((i%5)==0 ? 0.006f : 0.f);
        a.Observe(64,true,8,c.strategy,cost);
    }
    assert(late < early);
    assert(late <= 8);
    assert(a.Choose(64,true,0).probe);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            source=Path(folder)/'test.cpp'
            exe=Path(folder)/'test'
            source.write_text(program,encoding='utf-8')
            proc=subprocess.run([compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror',
                                 '-I'+str(ROOT/'engine/include'),str(source),'-o',str(exe)],
                                capture_output=True,text=True,timeout=30)
            self.assertEqual(proc.returncode,0,proc.stderr)
            proc=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(proc.returncode,0,proc.stderr)

    def test_layout_motion_changes_exactly_at_phase_boundaries(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('C++20 compiler missing')
        mock = r'''#pragma once
#include <utility>
struct Vec3 { float x{},y{},z{}; };
struct Transform { Vec3 position; };
struct GameObject {
    Transform value;
    struct Proxy { GameObject* owner;
      Transform* get() const { return &owner->value; } } transform{this};
};
struct Behaviour { virtual ~Behaviour()=default; virtual void Update(float) {} };
'''
        program = r'''
#include "BenchmarkDistribution.hpp"
#include <cassert>
int main() {
    GameObject a,b,c,d,e;
    std::vector<std::pair<GameObject*,Vec3>> original{
      {&a,{-1.f,0.f,0.f}},{&b,{1.f,0.f,0.f}},
      {&c,{1.f,1.f,0.f}},{&d,{1.f,2.f,0.f}},
      {&e,{4.f,5.f,0.f}}
    };
    XEngineBenchmark::BenchmarkDistribution mover(original,"cycling",3);
    mover.Update(0.5f);
    assert(e.value.position.x==4.f);
    mover.Update(0.01f); mover.Update(3.f);
    assert(e.value.position.x==4.f);
    mover.Update(300.f);
    assert(e.value.position.x!=4.f);
    const float x=e.value.position.x;
    mover.Update(0.f); mover.Update(0.f);
    assert(e.value.position.x==x);
    mover.Update(0.f);
    assert(e.value.position.x!=x); // two separated clusters
    mover.Update(0.f); mover.Update(0.f); mover.Update(0.f);
    assert(e.value.position.x==4.f); // deterministic return to regular
}
'''
        with tempfile.TemporaryDirectory() as folder:
            f=Path(folder)
            (f/'XEngine.h').write_text(mock,encoding='utf-8')
            src=f/'test.cpp';src.write_text(program,encoding='utf-8')
            exe=f/'test'
            proc=subprocess.run([compiler,'-std=c++20','-Wall','-Wextra','-Werror',
                                 '-I'+str(f),'-I'+str(ROOT/'games/XEngine Benchmark/assets/scripts'),
                                 str(src),'-o',str(exe)],capture_output=True,text=True,timeout=30)
            self.assertEqual(proc.returncode,0,proc.stderr)
            proc=subprocess.run([str(exe)],capture_output=True,text=True,timeout=15)
            self.assertEqual(proc.returncode,0,proc.stderr)

    def test_phase_metrics_and_pairing_for_dynamic_scene(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'profile.csv'
            header=('frame_ms','collision_candidates_ms','collision_grid_build_ms',
                    'collision_auto_selector_ms','collision_auto_calibration_passes',
                    'collision_bruteforce_passes','collision_grid_passes')
            with path.open('w',encoding='utf-8',newline='') as handle:
                w=csv.DictWriter(handle,fieldnames=header); w.writeheader()
                for i in range(26):
                    cost = 2. if i < 10 else (0.5 if i < 20 else 2.)
                    w.writerow({'frame_ms':cost,'collision_candidates_ms':cost/2,
                                'collision_grid_build_ms':0,
                                'collision_auto_selector_ms':0,
                                'collision_auto_calibration_passes':0,
                                'collision_bruteforce_passes':2 if i < 10 else 0,
                                'collision_grid_passes':0 if i < 10 else 2})
            rows=paired.phase_metrics(path,'cycling','collision','single',1,'auto_cells',
                                      0,26,10,2)
            self.assertEqual([r['spatial_phase'] for r in rows],
                             ['regular','clustered','spread'])
            self.assertEqual([r['measured_frames'] for r in rows],[8,8,4])
            self.assertEqual([r['frame_p50_ms'] for r in rows],[2.,.5,2.])
            synthetic=[]
            for strategy in paired.STRATEGIES:
                for row in rows:
                    synthetic.append(dict(row,strategy=strategy,
                                         broadphase_mean_ms=(.4 if strategy=='auto_cells' else 1.)))
            comparison=paired.paired_phases(synthetic)
            self.assertEqual(len(comparison),3)
            self.assertTrue(all(r['auto_vs_best_fixed_pct']>0 for r in comparison))

    def test_runner_accepts_and_propagates_layout_arguments(self):
        import run_benchmark
        self.assertIn('cycling',run_benchmark.LAYOUTS)
        self.assertEqual(paired.spatial_phase('cycling',1),'clustered')
        self.assertEqual(paired.spatial_phase('alternating',2),'regular')
        main=(ROOT/'games/XEngine Benchmark/src/main.cpp').read_text()
        self.assertIn('opts.phaseFrames',main)
        self.assertIn('opts.layout',main)
        runner=(ROOT/'tools/run_benchmark.py').read_text()
        self.assertIn("'--phase-frames', str(args.phase_frames)",runner)


if __name__=='__main__': unittest.main()
