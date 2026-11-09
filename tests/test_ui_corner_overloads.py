"""Independent corner-radius overloads and renderer wiring regression checks."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def read(path):
    """Read a current XEngine source file for a pipeline contract check."""
    return (ROOT / path).read_text(encoding="utf-8")


class CornerOverloadsTests(unittest.TestCase):
    def test_real_uistyle_overloads_and_geometry_run_as_cpp20(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("C++20 compiler unavailable")
        header = read("engine/include/UIManager.h")
        style = header.split("    struct UIStyle {", 1)[1].split("\n    };", 1)[0]
        code = r'''
#include "XEngine/detail/UICornerRadii.h"
#include "XEngine/detail/UIVisualGeometry.h"
#include <cassert>
#include <cmath>
#include <string>
enum class UICornerRadiusUnit { Pixels, Percent };
struct UIStyle {
'''+style+r'''
};
using namespace xengine::detail::ui;
static bool eq(float a, float b) { return std::fabs(a-b) < 0.001f; }
int main() {
    // The nine-field aggregate initialization used by Engine::Start must
    // remain source-compatible after adding the new overload methods.
    UIStyle legacy{Color{1,2,3,4}, Color{1,2,3,4}, Color{1,2,3,4},
                   Color{1,2,3,4}, Color{1,2,3,4}, 6.f,
                   std::string{"ui_default"}, 10.f, 2.f};
    assert(eq(legacy.cornerRadius, 10.f) && eq(legacy.borderWidth, 2.f));
    UIStyle style{};
    assert(!style.useIndividualCornerRadii && eq(style.cornerRadius, 0.f));
    style.cornerRadius = 10.f; // Legacy public field remains available.
    assert(eq(style.cornerRadius, 10.f));
    assert(&style.SetCornerRadius(12.f, 0.f) == &style);
    assert(style.useIndividualCornerRadii);
    assert(eq(style.individualCornerRadii.topLeft, 12.f));
    assert(eq(style.individualCornerRadii.topRight, 12.f));
    assert(eq(style.individualCornerRadii.bottomRight, 0.f));
    assert(eq(style.individualCornerRadii.bottomLeft, 0.f));
    style.SetCornerRadius(1.f, 2.f, 3.f, 4.f);
    assert(eq(style.individualCornerRadii.topLeft, 1.f));
    assert(eq(style.individualCornerRadii.topRight, 2.f));
    assert(eq(style.individualCornerRadii.bottomRight, 3.f));
    assert(eq(style.individualCornerRadii.bottomLeft, 4.f));
    style.SetCornerRadius(7.f);
    assert(!style.useIndividualCornerRadii && eq(style.cornerRadius, 7.f));

    auto radii = FitCornerRadii({80.f, 40.f, 10.f, 0.f}, 100.f, 60.f);
    assert(radii.topLeft+radii.topRight <= 100.001f);
    assert(eq(radii.topLeft, 60.f));
    assert(radii.topLeft+radii.bottomLeft <= 60.001f);
    assert(radii.topRight+radii.bottomRight <= 60.001f);
    auto square = FitCornerRadii({0.f, 20.f, 0.f, 0.f}, 50.f, 40.f);
    assert(eq(square.topLeft, 0.f) && eq(square.bottomLeft, 0.f));
    assert(eq(square.topRight, 20.f));
    const auto reduced = OffsetCornerRadii(square, -3.f);
    assert(eq(reduced.topRight, 17.f) && eq(reduced.topLeft, 0.f));
    const Rect popup{10.f, 20.f, 120.f, 96.f};
    const auto first = DropdownRowClip(popup, 32.f, 0, 3,
                                       CornerRadii{12.f, 8.f, 5.f, 0.f}, 2.f);
    const auto middle = DropdownRowClip(popup, 32.f, 1, 3,
                                        CornerRadii{12.f, 8.f, 5.f, 0.f}, 2.f);
    const auto last = DropdownRowClip(popup, 32.f, 2, 3,
                                      CornerRadii{12.f, 8.f, 5.f, 0.f}, 2.f);
    assert(first.valid && middle.valid && last.valid);
    assert(eq(first.radii.topLeft, 10.f));
    assert(eq(first.radii.topRight, 6.f));
    assert(eq(first.radii.bottomRight, 3.f));
    assert(eq(first.radii.bottomLeft, 0.f));
    assert(eq(first.clip.y + first.clip.h, middle.clip.y));
    assert(eq(middle.clip.y + middle.clip.h, last.clip.y));
    assert(eq(last.clip.y + last.clip.h, last.shape.y + last.shape.h));
}
'''
        with tempfile.TemporaryDirectory(prefix="xengine_corners_") as directory:
            base = Path(directory)
            cpp = base / "corners.cpp"
            executable = base / "corners"
            cpp.write_text(code, encoding="utf-8")
            result = subprocess.run([compiler, "-std=c++20", "-Wall", "-Wextra",
                                     "-pedantic", "-I", str(ROOT / "engine/include"),
                                     str(cpp), "-o", str(executable)],
                                    capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(executable)], capture_output=True,
                                    text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)

    def test_immediate_recorded_and_dropdown_paths(self):
        ui = read("engine/src/UIManager.cpp")
        render = read("engine/src/RenderManager.cpp")
        engine = read("engine/include/Engine.h")
        for token in ("RadiiForRect_(popup)", "geometry.radii", "command.hasCornerRadii = individualCorners",
                      "DrawRoundedRectCornersScreen_(c.r", "DrawClippedRoundedFillScreen_(c.r"):
            self.assertIn(token, ui)
        for token in ("RoundedPathCorners_", "FilledPolygonCorners_", "RoundedOutlineCorners_",
                      "DrawRoundedRectCornersScreenImmediate_", "command.hasScreenCornerRadii = true;",
                      "if (c.hasScreenCornerRadii)", "SDL_RenderSetClipRect(mRenderer, hadClip ? &oldClip : nullptr)"):
            self.assertIn(token, render)
        self.assertIn("xengine::detail::ui::CornerRadii screenCornerRadii{};", engine)
        self.assertIn("bool hasScreenCornerRadii = false;", engine)

    def test_json_overloads_and_documentation(self):
        cpp = read("engine/src/Engine.cpp")
        game_doc = read("docs/reference/game/RENDERING_AND_UI.md")
        engine_doc = read("docs/reference/engine/ENGINE_DEVELOPERS.md")
        self.assertIn("values.size() == 2 || values.size() == 4", cpp)
        self.assertIn("out.uiStyle.useIndividualCornerRadii = true", cpp)
        self.assertIn("style.SetCornerRadius(corners.topLeft", cpp)
        for example in ("SetCornerRadius(10.f)", "SetCornerRadius(12.f, 0.f)",
                        "SetCornerRadius(12.f, 8.f, 4.f, 0.f)"):
            self.assertIn(example, game_doc)
        self.assertIn("DrawRoundedRectCornersScreen_", engine_doc)
        header = read("engine/include/UIManager.h")
        for parameter in ("radius", "top", "bottom", "topLeft", "topRight",
                          "bottomRight", "bottomLeft"):
            self.assertIn(f"@param {parameter} ", header)


if __name__ == "__main__":
    unittest.main()
