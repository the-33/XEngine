"""Regression checks for ordered UI lines with pixel and anchored thickness."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class UiLineApiTests(unittest.TestCase):
    def test_public_pixel_and_anchored_signatures(self):
        header = (ROOT / "engine/include/UIManager.h").read_text(encoding="utf-8")
        self.assertIn(
            "void Line(Vec2 start, Vec2 end, Color color, float thickness = 1.f) noexcept;",
            header,
        )
        self.assertIn(
            "void Line(const UIRectTransform& layout, Color color, float thickness = 1.f,",
            header,
        )
        self.assertIn("const Rect* parent = nullptr) noexcept;", header)
        self.assertIn("@param thickness Positive line width in drawable pixels.", header)
        self.assertIn(
            "@param thickness Positive line width in reference-canvas pixels.", header
        )

    def test_lines_are_ordered_with_other_ui_commands(self):
        source = (ROOT / "engine/src/UIManager.cpp").read_text(encoding="utf-8")
        start = source.index("void UIManager::Line(Vec2 start, Vec2 end,")
        body = source[start:source.index("\n}\n", start) + 2]
        self.assertIn("!std::isfinite(thickness) || thickness <= 0.f", body)
        self.assertIn("cmd.type = CmdType::Line;", body)
        self.assertIn("cmd.from = start;", body)
        self.assertIn("cmd.to = end;", body)
        self.assertIn("cmd.lineWidth = thickness;", body)
        self.assertIn("QueueCmd_(std::move(cmd));", body)
        self.assertIn("render->DrawLineScreen(c.from.x, c.from.y, c.to.x, c.to.y,", source)
        self.assertIn("c.color, c.lineWidth);", source)

    def test_anchored_lines_scale_both_orientation_and_thickness(self):
        source = (ROOT / "engine/src/UIManager.cpp").read_text(encoding="utf-8")
        start = source.index("void UIManager::Line(const UIRectTransform& layout,")
        body = source[start:source.index("\n}\n", start) + 2]
        self.assertIn("const Rect r = Resolve(layout, parent);", body)
        self.assertIn("const float scaledThickness = thickness * CanvasScale();", body)
        self.assertIn("if (r.w >= r.h)", body)
        self.assertIn("Line({r.x, y}, {r.x + r.w, y}, color, scaledThickness);", body)
        self.assertIn("Line({x, r.y}, {x, r.y + r.h}, color, scaledThickness);", body)
        renderer = (ROOT / "engine/src/RenderManager.cpp").read_text(encoding="utf-8")
        self.assertIn("if (thickness <= 1.f)", renderer)
        self.assertIn("SDL_RenderDrawLineF(mRenderer, x1, y1, x2, y2);", renderer)

    def test_native_layout_scales_divider_to_fractional_physical_width(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A C++20 compiler is unavailable")
        code = r'''
#include "UIRectTransform.h"
#include "UILayoutValidation.h"
#include <cassert>
#include <cmath>
int main() {
    UICanvas canvas;
    canvas.scaleMode = UICanvasScaleMode::ScaleWithScreenSize;
    canvas.referenceResolution = {1280.f, 720.f};
    const Rect viewport{0.f, 0.f, 480.f, 270.f};
    const float scale = canvas.ScaleFor(viewport.Size());
    const Rect panel = UIRectTransform::Center({690.f, 518.f})
                           .Resolve(viewport, scale);
    const Rect divider = UIRectTransform::TopLeft({598.f, 2.f}, {46.f, 87.f})
                             .Resolve(panel, scale);
    assert(std::fabs(scale - 0.375f) < 0.001f);
    assert(std::fabs(divider.h - 0.75f) < 0.001f);
    UILayoutValidation::RequireVisible("options.divider", divider, viewport);
    const float thickness = 2.f * scale;
    assert(std::fabs(thickness - 0.75f) < 0.001f);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)
            cpp = path / "line_geometry.cpp"
            exe = path / "line_geometry"
            cpp.write_text(code, encoding="utf-8")
            compiled = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "engine/include"), str(cpp), "-o", str(exe)],
                capture_output=True, text=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            tested = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(tested.returncode, 0, tested.stderr)


if __name__ == "__main__":
    unittest.main()
