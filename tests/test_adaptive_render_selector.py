"""Adaptive render route regression tests: weighting, hysteresis, I/O and fail-closed builds."""
import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]

class AdaptiveRenderSelectorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'),'requires g++')
    def test_hysteresis_weighting_and_thread_gates(self):
        code = r'''
#include "RenderDiagnostics.h"
#include <cassert>
#include <initializer_list>
using namespace RenderDiagnostics;
int main() {
    static_assert(kParallelThreshold == 1536);
    static_assert(SpriteUnits(false,false,false) == 256);
    static_assert(SpriteUnits(true,true,true) == 576);
    static_assert(RectangleUnits(true) == 320);
    const auto t = kParallelThreshold;
    assert(Select(true,true,2,t-1,t) == Path::BelowThreshold);
    assert(Select(true,true,2,t,t) == Path::Parallel);
    // Same raw count: complex sprites enter earlier than plain sprites.
    assert(SelectAdaptive(true,true,2,1024*kUnit,t,false) == Path::BelowThreshold);
    assert(SelectAdaptive(true,true,2,1024*SpriteUnits(true,true,false),t,false) == Path::Parallel);
    assert(SelectAdaptive(true,true,2,t*kUnit-1,t,false) == Path::BelowThreshold);
    assert(SelectAdaptive(true,true,2,t*kUnit,t,false) == Path::Parallel);
    assert(SelectAdaptive(true,true,2,t*kUnit*86/100,t,true) == Path::Parallel);
    assert(SelectAdaptive(true,true,2,t*kUnit*84/100,t,true) == Path::BelowThreshold);
    assert(SelectAdaptive(false,true,2,999999,t,true) == Path::Disabled);
    assert(SelectAdaptive(true,false,2,999999,t,true) == Path::Disabled);
    assert(SelectAdaptive(true,true,0,999999,t,true) == Path::NoWorkers);
    // Cycle 55/75/100/65%; assert few transitions and no alternating each frame.
    bool active=false; int transitions=0;
    for (int round=0;round<4;++round) {
        for (int pct: {55,75,100,65}) {
            for (int f=0;f<50;++f) {
                const auto path=SelectAdaptive(true,true,3,2048ULL*pct*kUnit/100,t,active);
                bool next=(path==Path::Parallel);
                transitions+=(next!=active);active=next;
            }
        }
    }
    assert(transitions<=8);
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp)/'s.cpp';exe=Path(tmp)/'run';source.write_text(code)
            subprocess.run(['g++','-std=c++20','-I',str(ROOT/'engine/include'),str(source),'-o',str(exe)],check=True,capture_output=True,timeout=35)
            subprocess.run([str(exe)],check=True,timeout=10)

    def test_wiring_and_metrics_present(self):
        code=(ROOT/'engine/src/Engine.cpp').read_text()
        render=(ROOT/'engine/src/RenderManager.cpp').read_text()
        for needle in ('RenderDiagnostics::SelectAdaptive(',
                       'mPreviousRenderParallel = record;',
                       'adaptive ? 1u : mCfg.renderer.parallelMinCommands',
                       'render_weighted_units_previous,render_weighted_units_current,',
                       'render_path_switched,render_selection_mode',
                       'mLastWorldWorkUnits = render->mPublicWorldWorkUnitsThisFrame;',
                       'value == "adaptive"'):
            self.assertIn(needle,code)
        self.assertIn('RenderDiagnostics::SpriteUnits(',render)
        self.assertIn('mPublicWorldWorkUnitsThisFrame +=',render)

    def test_template_and_benchmark_modes_are_consistent(self):
        for item in ('tools/templates/BlankGame/assets/config/engine.json',
                     'games/XEngine Benchmark/assets/config/single.json',
                     'games/XEngine Benchmark/assets/config/collision.json',
                     'games/XEngine Benchmark/assets/config/parallel.json'):
            data=json.loads((ROOT/item).read_text())['Renderer']
            self.assertEqual(data['parallelMinCommands'],1536)
            self.assertEqual(data['parallelSelection'],'fixed')
        manifest=json.loads((ROOT/'tools/engine_config_schema_state.json').read_text())
        self.assertIn(['Renderer','parallelSelection'], manifest['managedLeaves'])

    def test_benchmark_scene_cycles_real_gameobjects(self):
        scene=(ROOT/'games/XEngine Benchmark/assets/scenes/BenchmarkScene.hpp').read_text()
        controller=(ROOT/'games/XEngine Benchmark/assets/scripts/BenchmarkRenderCycle.hpp').read_text()
        self.assertIn('cycle->AddComponent<BenchmarkRenderCycle>',scene)
        self.assertIn('objects_[i]->SetActive(i < active);',controller)
        self.assertIn('55u, 75u, 100u, 65u',controller)

    def test_sweep_rejects_bad_arguments_before_build(self):
        spec=importlib.util.spec_from_file_location('selector',ROOT/'tools/sweep_render_selector.py')
        # Import from source by injecting tools in module search path.
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        try:
            mod=importlib.util.module_from_spec(spec); spec.loader.exec_module(mod)
            self.assertEqual(mod.main(['--sprites','0','--build']),2)
            self.assertEqual(mod.main(['--frames','10','--build']),2)
        finally:sys.path.remove(str(ROOT/'tools'))

    def test_build_error_aborts_without_running_old_binary(self):
        import sys
        sys.path.insert(0,str(ROOT/'tools'))
        try:
            spec=importlib.util.spec_from_file_location('selector2',ROOT/'tools/sweep_render_selector.py')
            mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
            with tempfile.TemporaryDirectory() as tmp:
                with patch.object(mod,'run_build_with_live_log', return_value=37), \
                     patch.object(mod.subprocess,'call', side_effect=AssertionError('must not run old binary')):
                    self.assertEqual(mod.main(['--build','--output',tmp]),1)
        finally:sys.path.remove(str(ROOT/'tools'))

if __name__=='__main__': unittest.main()
