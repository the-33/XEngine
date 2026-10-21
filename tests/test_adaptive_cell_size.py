"""Adaptive grid-size strategy regressions; requires only a native C++20 compiler."""
from pathlib import Path
import json
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class AdaptiveCellSizeTests(unittest.TestCase):
    def test_adaptive_cost_selection_and_context_isolation(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if compiler is None:
            self.skipTest('C++20 compiler not installed')
        code = r'''
#include "AdaptiveBroadphase.h"
#include <cassert>
#include <cmath>
int main() {
    AdaptiveCellBroadphase adaptive;
    // One algorithm always wins, independent of calibrated collider-count thresholds.
    auto cost = [](unsigned choice) { return choice == 2 ? 0.4f : 3.0f; };
    for (int i=0; i<300; ++i) {
        auto selected = adaptive.Choose(740,true,0);
        adaptive.Observe(740,true,0,selected.strategy,cost(selected.strategy));
    }
    unsigned winning=0;
    for (int i=0;i<60;++i) {
        auto selected = adaptive.Choose(740,true,0);
        if (selected.strategy==2) ++winning;
        adaptive.Observe(740,true,0,selected.strategy,cost(selected.strategy));
    }
    assert(winning >= 55);
    // Different occupancy distribution must start its own calibration.
    auto fresh=adaptive.Choose(740,true,2);
    assert(fresh.strategy==0 && fresh.probe);
    // Different worker availability is also independent.
    fresh=adaptive.Choose(740,false,0);
    assert(fresh.strategy==0 && fresh.probe);
    // Alter the winning strategy; the selector must probe and eventually adapt.
    for (int i=0;i<800;++i) {
        auto choice=adaptive.Choose(740,true,0);
        adaptive.Observe(740,true,0,choice.strategy,
                         choice.strategy==3 ? 0.1f : 30.0f);
    }
    unsigned changed=0;
    for(int i=0;i<60;++i) {
        auto choice=adaptive.Choose(740,true,0);
        if(choice.strategy==3) ++changed;
        adaptive.Observe(740,true,0,choice.strategy,
                         choice.strategy==3 ? 0.1f : 30.0f);
    }
    assert(changed >= 50);
    assert(std::fabs(AdaptiveCellBroadphase::CellSize(1,2.f)-1.f)<1.e-4f);
    assert(std::fabs(AdaptiveCellBroadphase::CellSize(2,2.f)-2.f)<1.e-4f);
    assert(std::fabs(AdaptiveCellBroadphase::CellSize(3,2.f)-4.f)<1.e-4f);
    adaptive.Reset();
    assert(adaptive.Choose(740,true,0).strategy==0);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            src=Path(directory)/'test.cpp'
            binary=Path(directory)/'test'
            src.write_text(code, encoding='utf-8')
            command=[compiler,'-std=c++20','-O2','-Wall','-Wextra','-Werror',
                     f'-I{ROOT / "engine/include"}',str(src),'-o',str(binary)]
            completed=subprocess.run(command,capture_output=True,text=True,timeout=30)
            self.assertEqual(completed.returncode,0,completed.stderr)
            completed=subprocess.run([str(binary)],capture_output=True,text=True,timeout=30)
            self.assertEqual(completed.returncode,0,completed.stderr)

    def test_config_template_and_runtime_support_auto(self):
        template=json.loads((ROOT/'tools/templates/BlankGame/assets/config/engine.json').read_text())
        self.assertEqual(template['Collision'], {'broadphase':'auto','cellSize':'auto'})
        src=(ROOT/'engine/src/Engine.cpp').read_text()
        self.assertIn('out.collision.autoCellSize = true;',src)
        self.assertIn('out.collision.broadphase != "auto"',src)
        src=(ROOT/'engine/src/CollisionManager.cpp').read_text()
        self.assertIn('mAdaptiveCellBroadphase.Choose(',src)
        self.assertIn('mAdaptiveCellBroadphase.Observe(',src)
        self.assertIn('mLastGridBuildMs + mLastCandidateMs',src)
        from sys import path
        path.insert(0,str(ROOT/'tools'))
        import run_benchmark
        self.assertEqual(run_benchmark.auto_cell_arg('auto'),'auto')
        self.assertEqual(run_benchmark.auto_cell_arg('2.0'),'2.0')

    def test_profile_has_selected_cell_telemetry(self):
        src=(ROOT/'engine/src/Engine.cpp').read_text()
        self.assertIn('collision_grid_cell_size_last,collision_auto_calibration_passes',src)
        self.assertIn('collision_auto_cell_half_passes,collision_auto_cell_base_passes',src)
        self.assertIn('r.collisionGridCellSizeLast',src)

if __name__ == '__main__': unittest.main()
