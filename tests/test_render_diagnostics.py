"""Regression tests: route selection, profiling, optimizations, CPU benchmark."""
import csv
import importlib.util
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RenderDiagnosticsTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_route_selection_is_independent_of_background_calls(self):
        source = r'''
#include "RenderDiagnostics.h"
#include <cassert>
using namespace RenderDiagnostics;
int main() {
    assert(Select(false,true,3,1000)==Path::Disabled);
    assert(Select(true,false,3,1000)==Path::Disabled);
    assert(Select(true,true,0,1000)==Path::NoWorkers);
    assert(Select(true,true,3,383)==Path::BelowThreshold);
    assert(Select(true,true,3,1536)==Path::Parallel);
    assert(Name(Path::BelowThreshold)==std::string("below_threshold"));
}
'''.replace('#include <cassert>', '#include <cassert>\n#include <string>')
        with tempfile.TemporaryDirectory() as temp:
            src = Path(temp)/'test.cpp'; exe = Path(temp)/'test'
            src.write_text(source)
            subprocess.run(['g++','-std=c++20','-I',str(ROOT/'engine/include'),str(src),'-o',str(exe)],
                           check=True,timeout=30,capture_output=True)
            subprocess.run([str(exe)],check=True,timeout=10)

    def test_route_metrics_are_written_for_each_frame(self):
        src = (ROOT/'engine/src/Engine.cpp').read_text()
        for field in ('render_path_reason','render_eligible_previous','render_eligible_current',
                      'render_background_draws','render_world_draws','render_debug_draws',
                      'render_ui_draws','render_background_ms','render_world_ms',
                      'render_debug_ms','render_ui_ms','render_tint_cache_hits',
                      'render_tint_updates'):
            self.assertIn(field,src)
        self.assertIn('RenderDiagnostics::Select(',src)
        self.assertIn('RenderDiagnostics::Name(mLastRenderPath)',src)

    def test_submit_count_is_after_culling_and_debug_does_not_trigger_parallel(self):
        render = (ROOT/'engine/src/RenderManager.cpp').read_text()
        self.assertIn('mDrawPhase != DrawPhase::Debug',render)
        world = render.split('void RenderManager::DrawTextureImmediate_',1)[1].split(
            'void RenderManager::DrawTexturePrepared_',1)[0]
        self.assertLess(world.find('if (!Intersects(aabbRot, viewport)) return;'),
                        world.find('CountSubmission_();'))
        self.assertIn('mDrawPhase = DrawPhase::Debug;',render)
        self.assertIn('SDL_RenderDrawLineF',render)
        self.assertIn('const Vec2I wS{mWinW, mWinH};',render)
        self.assertIn('std::max(s1.x, s2.x)',render)

    def test_modulation_cache_invalidated_on_texture_destruction(self):
        assets=(ROOT/'engine/src/Assets.cpp').read_text()
        render=(ROOT/'engine/src/RenderManager.cpp').read_text()
        self.assertEqual(assets.count('render->InvalidateTextureTint_(mTexture);'),2)
        self.assertIn('mTextureTintCache.clear();', render)
        self.assertIn('mTextureTintCache.erase(texture);', render)
        self.assertIn('mTextureTintCache.find(texture)', render)

    def test_comparator_reports_actual_parallel_usage(self):
        spec=importlib.util.spec_from_file_location('comparator',ROOT/'tools/compare_profiles.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        fields=([name for name,_ in mod.METRICS]+['fixed_steps','narrowphase_tests']
                +list(mod.OPTIONAL_RENDER_COUNTS))
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'single.csv'
            with path.open('w',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=fields)
                writer.writeheader()
                for _ in range(5):
                    row=dict.fromkeys(fields,0)
                    row.update(frame_ms=0.5,render_path_reason=2,render_world_draws=100,
                               render_eligible_current=35)
                    writer.writerow(row)
            rows=mod.read_profile(path,0)
            output=mod.report(rows,rows)
            self.assertIn('below_threshold',output)
            self.assertIn('never activated',output)
            self.assertIn('world',output)

    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_synthetic_benchmark_verifies_checksum_and_emits_both_modes(self):
        with tempfile.TemporaryDirectory() as temp:
            csv_path=Path(temp)/'benchmark.csv'
            command=[sys.executable,str(ROOT/'tools/benchmark_render_cpu.py'),
                     '--sprites','128','--colliders','40','--workers','2',
                     '--repeats','3','--warmup','1','--csv',str(csv_path)]
            output=subprocess.run(command,check=True,timeout=40,capture_output=True,text=True)
            self.assertIn('[CHECKSUM] identical=',output.stdout)
            self.assertIn('does NOT time SDL',output.stdout)
            with csv_path.open(newline='') as f:
                rows=list(csv.DictReader(f))
            self.assertEqual(len(rows),6)
            self.assertEqual({r['mode'] for r in rows},{'single','parallel'})
            self.assertEqual(len({r['checksum'] for r in rows}),1)
            self.assertTrue(all(float(r['total_ms'])>=0 for r in rows))

if __name__ == '__main__':
    unittest.main()
