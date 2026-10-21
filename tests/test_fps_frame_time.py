"""Regression checks for the FPS counter after high-resolution profiling."""

import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class FPSFrameTimingTests(unittest.TestCase):
    def test_fps_uses_measured_frame_duration(self):
        code = (ROOT / "engine/src/Engine.cpp").read_text(encoding="utf-8")
        match = re.search(r"inline float Engine::GetFPS\(\) noexcept\s*\{(.*?)\n\}", code, re.S)
        self.assertIsNotNone(match)
        fps_code = match.group(1)
        self.assertIn("mFrameTime", fps_code)
        self.assertIn("std::isfinite(frameSeconds)", fps_code)
        self.assertIn("frameSeconds > 0.f", fps_code)
        self.assertNotIn("render->renderTime", fps_code)

    def test_both_modes_update_frame_time_and_share_the_loop(self):
        code = (ROOT / "engine/src/Engine.cpp").read_text(encoding="utf-8")
        self.assertIn("mFrameTime = ProfileElapsed_(frameStart)", code)
        self.assertIn("collision->EnableParallelCollision(true);", code)
        self.assertIn("return RunSingleThreadingImpl();", code)


if __name__ == "__main__":
    unittest.main()
