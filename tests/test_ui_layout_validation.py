"""Compile the explicit UI-fit guard without needing SDL runtime libraries."""
from __future__ import annotations

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class UILayoutValidationTests(unittest.TestCase):
    def test_public_api_and_windowed_size_for_recovery(self):
        umbrella = (ROOT / "engine/include/XEngine.h").read_text(encoding="utf-8")
        window = (ROOT / "engine/include/WindowManager.h").read_text(encoding="utf-8")
        self.assertIn('#include "UILayoutValidation.h"', umbrella)
        self.assertIn("Vec2I GetWindowedSize() const noexcept", window)
        self.assertLess(window.index("public:"),
                        window.index("Vec2I GetWindowedSize() const noexcept"))

    def test_native_bounds_exceptions_and_scaled_control_sizes(self):
        compiler = shutil.which("g++") or shutil.which("clang++")
        if not compiler:
            self.skipTest("A C++20 compiler is unavailable")
        source = r'''
#include "UILayoutValidation.h"
#include "UIRectTransform.h"
#include <cassert>
#include <cmath>
#include <limits>
#include <string>

template<class F>
void expect(F&& f, UILayoutException::Reason reason, const char* element) {
    bool thrown = false;
    try { f(); }
    catch (const UILayoutException& error) {
        thrown = true;
        assert(error.FailureReason() == reason);
        assert(error.ElementId() == element);
        assert(error.RequestedDrawableSize().x > 0);
    }
    assert(thrown);
}

int main() {
    UICanvas canvas{};
    canvas.scaleMode = UICanvasScaleMode::ScaleWithScreenSize;
    canvas.referenceResolution = {1280.f, 720.f};
    const Rect hd{0.f, 0.f, 1280.f, 720.f};
    const Rect small{0.f, 0.f, 640.f, 360.f};
    const Rect tiny{0.f, 0.f, 480.f, 270.f};
    auto divider = UIRectTransform::TopLeft({598.f, 2.f}, {46.f, 87.f});
    auto frame = UIRectTransform::Center({690.f, 518.f});

    for (Rect view : {hd, small, tiny}) {
        const float scale = canvas.ScaleFor(view.Size());
        const Rect panel = frame.Resolve(view, scale);
        const Rect line = divider.Resolve(panel, scale);
        UILayoutValidation::RequireVisible("frame", panel, view);
        UILayoutValidation::RequireVisible("divider", line, view);
        UILayoutValidation::RequireControlVisible(
            "resolution", {50.f, 70.f, 418.f * scale, 44.f * scale},
            view, scale, 24.f, 12.f);
    }

    // A fractional rectangle is rasterized as one physical pixel, not lost.
    UILayoutValidation::RequireVisible(
        "fractional", {5.25f, 7.25f, 0.75f, 0.75f}, tiny);
    expect([&] { UILayoutValidation::RequireVisible(
        "needs-two-pixels", {5.25f, 7.25f, 0.75f, 0.75f}, tiny, 2.f); },
        UILayoutException::Reason::TooSmall, "needs-two-pixels");
    expect([&] { UILayoutValidation::RequireVisible(
        "zero", {5.f, 5.f, 10.f, 0.f}, tiny); },
        UILayoutException::Reason::TooSmall, "zero");
    expect([&] { UILayoutValidation::RequireVisible(
        "snapped-offscreen", {640.f, 10.f, 0.75f, 4.f}, small); },
        UILayoutException::Reason::Clipped, "snapped-offscreen");
    expect([&] { UILayoutValidation::RequireVisible(
        "offscreen", {600.f, 0.f, 60.f, 30.f}, small); },
        UILayoutException::Reason::Clipped, "offscreen");
    expect([&] { UILayoutValidation::RequireControlVisible(
        "dropdown", {20.f, 10.f, 1.5f, 40.f}, small,
        0.5f, 24.f, 12.f); },
        UILayoutException::Reason::TooSmall, "dropdown");
    bool invalid = false;
    try { UILayoutValidation::RequireVisible("bad", {0.f, 0.f, 5.f, 5.f},
        {0.f, 0.f, 0.f, 720.f}); }
    catch (const UILayoutException& error) {
        invalid = error.FailureReason() == UILayoutException::Reason::InvalidCanvas;
    }
    assert(invalid);
    expect([&] { UILayoutValidation::RequireVisible("nan",
        {0.f, 0.f, std::numeric_limits<float>::quiet_NaN(), 5.f}, hd); },
        UILayoutException::Reason::TooSmall, "nan");
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            path = Path(temporary)
            cpp = path / "layout_test.cpp"
            executable = path / "layout_test"
            cpp.write_text(source, encoding="utf-8")
            compiled = subprocess.run(
                [compiler, "-std=c++20", "-Wall", "-Wextra", "-Werror",
                 "-I", str(ROOT / "engine/include"), str(cpp), "-o", str(executable)],
                text=True, capture_output=True,
            )
            self.assertEqual(compiled.returncode, 0, compiled.stderr)
            tested = subprocess.run([str(executable)], text=True, capture_output=True)
            self.assertEqual(tested.returncode, 0, tested.stderr)


if __name__ == "__main__":
    unittest.main()
