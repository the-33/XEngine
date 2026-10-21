"""Regression: conservative culling-aware selector, honest worker telemetry and Release runner."""
from __future__ import annotations
import csv
import importlib.util
import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]

class VisibilitySelectorTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'g++ unavailable')
    def test_conservative_culling_never_rejects_visible_rotated_sprite(self):
        code=r'''
#include "RenderDiagnostics.h"
#include "SpriteProjection.h"
#include <cassert>
#include <cmath>
#include <random>
using namespace RenderDiagnostics;
int main() {
    const View v{0,0,22,12.375f};
    assert(DefinitelyOffscreenSprite(v,100,0,1,1,1,1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(v,0,0,1,1,1,1,0,0,.5,.5));
    assert(!DefinitelyOffscreenSprite(v,12,0,1,1,1,1,0,0,-30,.5));
    assert(EffectiveUnits(576,true)==144);
    assert(EffectiveUnits(576,false)==576);
    std::mt19937 rng(20261021);
    std::uniform_real_distribution<float> xy(-50.f,50.f), scale(-4.f,4.f),
        angle(-180.f,180.f), pivot(-1.f,2.f), offset(-2.f,2.f);
    for (int i=0;i<16000;i++) {
        SpriteProjection::Pose p;
        p.x=xy(rng);p.y=xy(rng);p.width=1.2f;p.height=.9f;
        p.scaleX=scale(rng);p.scaleY=scale(rng);
        p.rotationX=angle(rng);p.rotationY=angle(rng);p.rotationZ=angle(rng);
        p.pivotX=pivot(rng);p.pivotY=pivot(rng);
        p.offsetX=offset(rng);p.offsetY=offset(rng);
        if (!DefinitelyOffscreenSprite(v,p.x,p.y,p.width,p.height,
                p.scaleX,p.scaleY,p.offsetX,p.offsetY,p.pivotX,p.pivotY)) continue;
        const auto r=SpriteProjection::Project(p);
        const float cx=r.x+r.w*p.pivotX, cy=r.y+r.h*p.pivotY;
        const float rad=p.rotationZ*3.14159265358979323846f/180.f;
        const float c=std::cos(rad),s=std::sin(rad);
        float minx=1e30f,maxx=-1e30f,miny=1e30f,maxy=-1e30f;
        for (int k=0;k<4;k++) {
            float dx=r.w*(float(k&1)-p.pivotX);
            float dy=r.h*(float((k>>1)&1)-p.pivotY);
            float x=cx+c*dx-s*dy,y=cy+s*dx+c*dy;
            minx=std::min(minx,x);maxx=std::max(maxx,x);
            miny=std::min(miny,y);maxy=std::max(maxy,y);
        }
        assert(maxx<-11 || minx>11 || maxy<-6.1875f || miny>6.1875f);
    }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(code)
            subprocess.run(['g++','-std=c++20','-O2','-I',str(ROOT/'engine/include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True,timeout=35)
            subprocess.run([str(p/'test')],check=True,timeout=15)

    def test_render_metrics_separate_selected_and_executed(self):
        e=(ROOT/'engine/src/Engine.cpp').read_text()
        for name in ('render_parallel_executed','render_parallel_fallback','render_executed_switched',
                     'render_effective_units_previous','render_effective_units_current',
                     'render_estimated_offscreen_sprites'):
            self.assertIn(name,e)
        self.assertIn('mLastRenderPath == RenderDiagnostics::Path::Parallel',e)
        self.assertIn('mRenderParallelExecuted = render->mLastWorkers > 0',e)
        self.assertIn('mRenderParallelFallback = (record && !mRenderParallelExecuted)',e)
        s=(ROOT/'tools/sweep_render_selector.py').read_text()
        self.assertIn("r['render_parallel_executed']",s)
        self.assertIn("r['render_parallel_fallback']",s)

    def test_runner_counts_real_worker_frames_not_requested_frames(self):
        spec=importlib.util.spec_from_file_location('benchmark',ROOT/'tools/run_benchmark.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp)/'profil.csv'
            with p.open('w',newline='') as f:
                writer=csv.DictWriter(f,fieldnames=['frame_ms','collision_ms','render_prepare_ms',
                 'render_path_reason','render_parallel_executed','render_workers','broadphase_tests','fixed_steps'])
                writer.writeheader()
                writer.writerow(dict(frame_ms=2,collision_ms=0,render_prepare_ms=0,render_path_reason=3,
                                     render_parallel_executed=0,render_workers=0,broadphase_tests=0,fixed_steps=1))
                writer.writerow(dict(frame_ms=1,collision_ms=0,render_prepare_ms=0,render_path_reason=3,
                                     render_parallel_executed=1,render_workers=3,broadphase_tests=0,fixed_steps=1))
            m=mod.measure(p,0,2)
            self.assertEqual(m['render_parallel_requested_frames'],2)
            self.assertEqual(m['render_parallel_frames'],1)

    def test_release_profile_paths_and_cli(self):
        spec=importlib.util.spec_from_file_location('benchmark2',ROOT/'tools/run_benchmark.py')
        mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
        dbg=mod.executable_default('Debug')
        rel=mod.executable_default('Release')
        version=json.loads((ROOT/'games/XEngine Benchmark/config.json').read_text())['project']['version']
        self.assertIn('/debug/',dbg.as_posix())
        self.assertIn('/dist/',rel.as_posix())
        self.assertIn(str(version),rel.as_posix())
        self.assertIn('assets = executable.parent if args.build_config', (ROOT/'tools/run_benchmark.py').read_text())
        self.assertIn("'--build-config',args.build_config",(ROOT/'tools/sweep_render_selector.py').read_text())

    def test_release_failed_build_never_executes_benchmark(self):
        sys.path.insert(0,str(ROOT/'tools'))
        try:
            spec=importlib.util.spec_from_file_location('selector_v1610',ROOT/'tools/sweep_render_selector.py')
            mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
            with tempfile.TemporaryDirectory() as tmp:
                with patch.object(mod,'run_build_with_live_log',return_value=42) as call, \
                     patch.object(mod.subprocess,'call',side_effect=AssertionError('must not run')):
                    self.assertEqual(mod.main(['--build','--build-config','Release','--output',tmp]),1)
                    self.assertIn('Release',call.call_args.args[0])
        finally: sys.path.remove(str(ROOT/'tools'))

if __name__=='__main__': unittest.main()
