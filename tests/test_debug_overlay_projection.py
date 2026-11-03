"""World debug draws must project onto native output pixels, not the world target."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "engine"


class DebugOverlayProjectionTests(unittest.TestCase):
    def test_virtual_to_native_projection_and_letterbox(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if compiler is None:
            self.skipTest("C++ compiler not available")
        snippet = r'''
#include "XEngine/detail/DebugOverlayProjection.h"
#include <cassert>
#include <cmath>
#include <initializer_list>
int main() {
    using namespace XEngineDebugOverlay;
    const auto close = [](float a, float b) {
        return std::fabs(a - b) < 0.001f;
    };
    // A single world point stays centered when the low-res world is upscaled.
    for (auto output : {Point{1920.f, 1080.f}, Point{1536.f, 864.f},
                        Point{960.f, 540.f}}) {
        const auto center = ProjectToOutput({640.f, 360.f}, 1280, 720,
                                            0, 0, (int)output.x, (int)output.y);
        assert(close(center.x, output.x * 0.5f));
        assert(close(center.y, output.y * 0.5f));
    }
    // 1920x1200 output letterboxes a 16:9 world at y=60, not y=0.
    const auto top = ProjectToOutput({0.f, 0.f}, 1280, 720,
                                     0, 60, 1920, 1080);
    const auto bottom = ProjectToOutput({1280.f, 720.f}, 1280, 720,
                                        0, 60, 1920, 1080);
    assert(close(top.x, 0.f) && close(top.y, 60.f));
    assert(close(bottom.x, 1920.f) && close(bottom.y, 1140.f));
    const auto invalid = ProjectToOutput({5.f, 7.f}, 0, 0, 0, 0, 0, 0);
    assert(invalid.x == 5.f && invalid.y == 7.f);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "projection.cpp"
            executable = root / "projection"
            source.write_text(snippet, encoding="utf-8")
            compiled = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ENGINE / "include"), str(source), "-o", str(executable)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)

    def test_debug_gizmos_are_deferred_until_native_screen_pass(self):
        source = (ENGINE / "src" / "RenderManager.cpp").read_text(encoding="utf-8")
        header = (ENGINE / "include" / "RenderManager.h").read_text(encoding="utf-8")
        self.assertIn("std::vector<DebugCmd> mDeferredDebugCmds;", header)
        self.assertIn("DrawDeferredDebugOnScreen_();", source)
        self.assertIn("XEngineDebugOverlay::ProjectToOutput(", source)
        self.assertIn("output.x, output.y, output.w, output.h", source)
        self.assertIn("DrawLineScreenImmediate_(a.x, a.y, b.x, b.y, color, 2.f)", source)
        self.assertIn("if (!snapRecord) {", source)
        self.assertIn("mDeferredDebugCmds.insert(", source)
        self.assertIn("if (mWorldTargetActive && mRenderTarget) {", source)
        self.assertIn("if (c.debugDraw && c.type == DrawCmd::Type::WorldLine)", source)
        self.assertIn("if (!mDeferredDebugCmds.empty()) BeginScreenPass_();", source)
        self.assertIn("mDrawPhase == DrawPhase::Debug) ++nDrawCallsThisFrame;", source)
        self.assertIn("const float cap = mDrawPhase == DrawPhase::Debug ? 1.f : 0.f;", source)


if __name__ == "__main__":
    unittest.main()
