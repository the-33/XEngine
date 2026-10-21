"""CPU-only render geometry tests (no SDL development packages required)."""
from __future__ import annotations

import csv
import importlib.util
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class RenderPreparationTests(unittest.TestCase):
    @unittest.skipUnless(shutil.which("g++"), "g++ is unavailable")
    def test_projection_culling_rotation_and_parallel_order(self):
        source = r'''
#include "ParallelExecutor.h"
#include "RenderPreparation.h"
#include <cassert>
#include <cmath>
#include <vector>
using namespace RenderPreparation;
static bool close(float a, float b) { return std::fabs(a-b) < .001f; }
int main() {
    Camera camera{0,0,20,10,1,1280,720};
    auto inside = Prepare(camera, {-1,-1,2,2}, 0, .5f, .5f);
    assert(inside.visible && close(inside.screen.x,576) && close(inside.screen.y,288));
    assert(close(inside.screen.w,128) && close(inside.screen.h,144));
    auto outside = Prepare(camera, {50,50,1,1}, 0, .5f, .5f);
    assert(!outside.visible);
    auto edge = Prepare(camera, {10,0,1,1}, 0, .5f, .5f);
    assert(!edge.visible);  // touching the right boundary only
    auto rotated = Prepare(camera, {10.1f,-1.f,2.f,2.f}, 45.f, .5f, .5f);
    assert(rotated.visible); // rotation expands into the viewport
    assert(!Prepare(Camera{0,0,20,10,0,1280,720}, {0,0,1,1},0,.5f,.5f).visible);
    constexpr std::size_t n=2048;
    std::vector<Prepared> serial(n), parallel(n);
    for(std::size_t i=0;i<n;++i) {
        auto work=[&](std::size_t j){
            return Prepare(camera, {(float)((int)j-1000),-1.f,2.f,2.f},
                           (j%3==0)?45.f:0.f,.5f,.5f);
        };
        serial[i]=work(i);
    }
    ParallelExecutor workers(3);
    workers.Run(n,[&](std::size_t i){
        parallel[i]=Prepare(camera, {(float)((int)i-1000),-1.f,2.f,2.f},
                            (i%3==0)?45.f:0.f,.5f,.5f);
    });
    for(std::size_t i=0;i<n;++i) {
        assert(serial[i].visible==parallel[i].visible);
        assert(close(serial[i].screen.x,parallel[i].screen.x));
        assert(close(serial[i].pivotX,parallel[i].pivotX));
    }
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            source_path = Path(tmp) / "render_prep.cpp"
            executable = Path(tmp) / "render_prep"
            source_path.write_text(source)
            subprocess.run(["g++", "-std=c++20", "-O0", "-pthread", "-I",
                            str(ROOT / "engine/include"), str(source_path), "-o", str(executable)],
                           check=True, text=True, capture_output=True)
            subprocess.run([str(executable)], check=True, timeout=15)

    def test_worker_threads_never_touch_sdl_or_live_scene(self):
        source = (ROOT / "engine/src/RenderManager.cpp").read_text()
        segment = source.split("const auto prepare =", 1)[1].split("    if (workers", 1)[0]
        for forbidden in ("SDL_", "mRenderer", "GetSDL()", "mActiveCam", "SceneManager", "AssetManager"):
            self.assertNotIn(forbidden, segment)
        self.assertIn("workers->Run(chunks", source)
        self.assertIn("mLastPreparedCount", source)
        self.assertIn("DrawTexturePrepared_", source)

    def test_serial_and_parallel_profile_columns(self):
        source = (ROOT / "engine/src/Engine.cpp").read_text()
        self.assertIn("mRenderRecordTime", source)
        self.assertIn("mRenderWorkerWaitTime", source)
        self.assertIn("render_record_ms,render_prepare_ms", source)
        self.assertIn("mCfg.renderer.parallelPreparation", source)
        self.assertIn("const bool record", source)
        self.assertIn("render->mPublicWorldCommandsThisFrame", source)

    def test_comparator_accepts_old_and_new_profiles(self):
        path = ROOT / "tools/compare_profiles.py"
        spec = importlib.util.spec_from_file_location("compare_profiles_test", path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        required = [key for key, _ in module.METRICS] + ["fixed_steps", "narrowphase_tests"]
        optional = [key for key, _ in module.RENDER_METRICS]
        with tempfile.TemporaryDirectory() as tmp:
            old, new = Path(tmp)/"old.csv", Path(tmp)/"new.csv"
            for target, names in ((old, required), (new, required+optional)):
                with target.open("w", newline="") as f:
                    writer = csv.DictWriter(f, fieldnames=names)
                    writer.writeheader()
                    writer.writerow({name: 1 for name in names})
            old_rows = module.read_profile(old, 0)
            new_rows = module.read_profile(new, 0)
            self.assertIn("Frame wall time", module.report(old_rows, new_rows))
            self.assertIn("Render CPU preparation", module.report(new_rows, new_rows))


if __name__ == "__main__":
    unittest.main()
