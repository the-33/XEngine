"""Regression tests for CPU-only XYZ projection and deferred render command data."""
from __future__ import annotations

import importlib.util
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SpriteProjectionTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which('g++'), 'g++ not installed')
    def test_projection_matches_reference_and_workers(self):
        source = r'''
#include "SpriteProjection.h"
#include "ParallelExecutor.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <vector>
using namespace SpriteProjection;
static float rad(float degrees) { return degrees * 3.14159265358979323846f/180.f; }
static Rect reference(Pose p) {
    float fx = std::max(0.f, std::cos(rad(p.rotationY)));
    float fy = std::max(0.f, std::cos(rad(p.rotationX)));
    float width = p.width * (p.scaleX * ((1.f-1.f) + 1.f*fx));
    float height = p.height * (p.scaleY * ((1.f-1.f) + 1.f*fy));
    float x = p.offsetX*p.scaleX;
    float y = p.offsetY*p.scaleY;
    float offx = x*std::cos(rad(p.rotationZ)) - y*std::sin(rad(p.rotationZ));
    float offy = x*std::sin(rad(p.rotationZ)) + y*std::cos(rad(p.rotationZ));
    return {p.x+offx-width*p.pivotX, p.y+offy-height*p.pivotY,width,height};
}
static bool close(float x, float y) { return std::fabs(x-y) < 0.0003f; }
int main() {
    const int n=4096;
    std::vector<Pose> poses(n);
    std::vector<Rect> serial(n), parallel(n);
    for (int i=0;i<n;++i) {
        auto &p=poses[i];
        p.x=i*.02f; p.y=(i%111)*-.18f;
        p.scaleX=(i%12+1)*.1f; p.scaleY=(i%9+1)*.2f;
        p.rotationX=(i%11==0) ? 0.f : (float)(i%190-95);
        p.rotationY=(i%7==0) ? 0.f : (float)(i%380-190);
        p.rotationZ=(i%5==0) ? 0.f : (float)(i%720-360);
        p.width=.75f; p.height=1.25f;
        p.offsetX=(i%3==0) ? 0.f : .11f;
        p.offsetY=(i%3==0) ? 0.f : -.25f;
        p.pivotX=(i%3)*.5f; p.pivotY=(i%4)*.25f;
        serial[i]=Project(p);
        auto original=reference(p);
        assert(close(serial[i].x,original.x));
        assert(close(serial[i].y,original.y));
        assert(close(serial[i].w,original.w));
        assert(close(serial[i].h,original.h));
    }
    ParallelExecutor pool(3);
    pool.Run(n, [&](std::size_t j){parallel[j]=Project(poses[j]);});
    for (int i=0;i<n;++i) {
        assert(serial[i].x == parallel[i].x);
        assert(serial[i].y == parallel[i].y);
        assert(serial[i].w == parallel[i].w);
        assert(serial[i].h == parallel[i].h);
    }
    Pose defaults{};
    defaults.x=12; defaults.y=20; defaults.width=3; defaults.height=4;
    auto flat=Project(defaults);
    assert(flat.x==10.5f && flat.y==18.f && flat.w==3.f && flat.h==4.f);
}
'''
        with tempfile.TemporaryDirectory() as temp:
            src=Path(temp)/'test.cpp'
            exe=Path(temp)/'test'
            src.write_text(source)
            build=subprocess.run(['g++','-std=c++20','-O1','-pthread',
                                  '-I'+str(ROOT/'engine/include'),str(src),'-o',str(exe)],
                                 text=True,capture_output=True)
            self.assertEqual(build.returncode,0,build.stderr)
            result=subprocess.run([str(exe)],text=True,capture_output=True)
            self.assertEqual(result.returncode,0,result.stderr)

    def test_worker_source_never_touches_live_scene_or_sdl(self):
        src=(ROOT/'engine/src/RenderManager.cpp').read_text()
        prepare=src.split('const auto prepare =',1)[1].split('    if (workers',1)[0]
        self.assertIn('SpriteProjection::Project(snapshot.spritePoses[command.spritePoseIndex])',prepare)
        for forbidden in ('SDL_', 'GetComponent', 'GetSDL', 'mActiveCam', 'GameObject', 'Transform'):
            self.assertNotIn(forbidden,prepare)
        self.assertIn('c.hasDeferredSpritePose = true;',src)
        sprite=(ROOT/'engine/src/SpriteRenderer.cpp').read_text()
        self.assertIn('DrawSprite_',sprite)
        self.assertNotIn('std::cos(',sprite)
        self.assertNotIn('std::sin(',sprite)
        header=(ROOT/'engine/include/RenderManager.h').read_text()
        self.assertIn('mDeferSpriteProjection = true;',header)

    def test_benchmark_has_true_main_vs_worker_comparison(self):
        script=(ROOT/'tools/sweep_sprite_projection.py').read_text()
        main=(ROOT/'games/XEngine Benchmark/src/main.cpp').read_text()
        runner=(ROOT/'tools/run_benchmark.py').read_text()
        self.assertIn("variants = ('main', 'workers') if rep % 2 else ('workers', 'main')",script)
        self.assertIn("'--modes', 'parallel'",script)
        self.assertIn('compare_profiles(m_all, w_all)',script)
        self.assertIn('Renderer->SetDeferredSpriteProjection(spriteProjection == "workers")',main)
        self.assertIn("'--sprite-projection', args.sprite_projection",runner)
        self.assertIn('Game did not confirm selected sprite projection mode',runner)
        self.assertIn("'sprite_projection': args.sprite_projection",runner)

    @unittest.skipUnless(shutil.which('g++'), 'g++ not installed')
    def test_sprite_renderer_source_cpp20_stub_compile(self):
        # Existing project test supplies transparent stand-ins for the SDL SDK.
        spec=importlib.util.spec_from_file_location('stubs',ROOT/'tests/test_camera_benchmark_gcc_compile.py')
        mod=importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        with tempfile.TemporaryDirectory() as temp:
            stub=Path(temp)
            for name,content in mod.STUBS.items():
                dest=stub/name
                dest.parent.mkdir(parents=True,exist_ok=True)
                dest.write_text(content)
            source=ROOT/'engine/src/SpriteRenderer.cpp'
            result=subprocess.run(['g++','-std=c++20','-fsyntax-only','-DXENGINE_DEBUG',
                       '-I'+str(stub),'-I'+str(ROOT/'engine/include'),str(source)],
                       capture_output=True,text=True)
            self.assertEqual(result.returncode,0,result.stderr)


if __name__=='__main__':
    unittest.main()
