"""Compile the SDL-independent anchor geometry and check immediate-mode wiring."""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ResponsiveUiTests(unittest.TestCase):
    def test_cpp_anchor_geometry_for_resolutions_aspects_and_scaling(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('A C++20 compiler is unavailable')

        cpp = r'''
#include "UIRectTransform.h"
#include <cassert>
#include <cmath>

static bool eq(float a, float b) { return std::fabs(a - b) < 0.02f; }
static void check(Rect r, float x, float y, float w, float h) {
    assert(eq(r.x, x) && eq(r.y, y) && eq(r.w, w) && eq(r.h, h));
}

int main() {
    const Rect hd{0.f, 0.f, 1920.f, 1080.f};
    const Rect small{0.f, 0.f, 1280.f, 720.f};
    const Rect aspect43{0.f, 0.f, 1024.f, 768.f};

    // Anchor to edges rather than hardcoding absolute screen coordinates.
    const auto topRight = UIRectTransform::TopRight({200.f, 50.f}, {20.f, 30.f});
    check(topRight.Resolve(hd), 1700.f, 30.f, 200.f, 50.f);
    check(topRight.Resolve(small), 1060.f, 30.f, 200.f, 50.f);
    check(topRight.Resolve(aspect43), 804.f, 30.f, 200.f, 50.f);
    check(UIRectTransform::TopLeft({100.f, 60.f}, {20.f, 10.f}).Resolve(small),
          20.f, 10.f, 100.f, 60.f);
    check(UIRectTransform::BottomRight({80.f, 40.f}, {10.f, 15.f}).Resolve(small),
          1190.f, 665.f, 80.f, 40.f);
    check(UIRectTransform::Center({100.f, 40.f}, {5.f, -5.f}).Resolve(hd),
          915.f, 515.f, 100.f, 40.f);

    // Stretch expands to the parent while retaining fixed margins.
    const auto stretch = UIRectTransform::Stretch(10.f, 20.f, 30.f, 40.f);
    check(stretch.Resolve(hd), 10.f, 20.f, 1880.f, 1020.f);
    check(stretch.Resolve(aspect43), 10.f, 20.f, 984.f, 708.f);

    // Parents are real resolved rectangles; children follow a nested panel.
    const Rect panel{100.f, 200.f, 300.f, 400.f};
    check(UIRectTransform::BottomRight({80.f, 40.f}, {10.f, 15.f}).Resolve(panel),
          310.f, 545.f, 80.f, 40.f);
    UIRectTransform leftHalf{{0.f, 0.f}, {0.5f, 1.f}, {0.5f, 0.5f},
                             {0.f, 0.f}, {-40.f, -20.f}};
    check(leftHalf.Resolve(hd), 20.f, 10.f, 920.f, 1060.f);

    // Screen-size scale changes offsets/sizes, never anchor percentages.
    UICanvas canvas{};
    assert(eq(canvas.ScaleFor({1920.f, 1080.f}), 1.f));
    canvas.scaleMode = UICanvasScaleMode::ScaleWithScreenSize;
    canvas.matchWidthOrHeight = 0.f;
    assert(eq(canvas.ScaleFor({960.f, 1080.f}), 0.5f));
    canvas.matchWidthOrHeight = 1.f;
    assert(eq(canvas.ScaleFor({960.f, 1080.f}), 1.f));
    canvas.matchWidthOrHeight = 0.5f;
    assert(eq(canvas.ScaleFor({960.f, 1080.f}), std::sqrt(0.5f)));
    const float doubleScale = canvas.ScaleFor({3840.f, 2160.f});
    assert(eq(doubleScale, 2.f));
    check(topRight.Resolve({0.f, 0.f, 3840.f, 2160.f}, doubleScale),
          3400.f, 60.f, 400.f, 100.f);

    // Overconstrained stretches cannot produce negative drawing/hitbox sizes.
    const auto tiny = stretch.Resolve({0.f, 0.f, 10.f, 10.f});
    assert(tiny.w == 0.f && tiny.h == 0.f);

    // Invalid scale settings safely fall back to constant pixel size.
    canvas.referenceResolution = {0.f, 1080.f};
    assert(eq(canvas.ScaleFor({1920.f, 1080.f}), 1.f));
}
'''
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / 'ui_anchors.cpp'
            binary = root / 'ui_anchors'
            source.write_text(cpp, encoding='utf-8')
            compile_run = subprocess.run(
                [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                 '-I', str(ROOT / 'engine/include'), str(source), '-o', str(binary)],
                text=True, capture_output=True,
            )
            self.assertEqual(compile_run.returncode, 0, compile_run.stderr)
            execute = subprocess.run([str(binary)], text=True, capture_output=True)
            self.assertEqual(execute.returncode, 0, execute.stderr)

    def test_manager_wires_anchored_widgets_to_existing_pixel_paths(self):
        header = (ROOT / 'engine/include/UIManager.h').read_text(encoding='utf-8')
        source = (ROOT / 'engine/src/UIManager.cpp').read_text(encoding='utf-8')
        umbrella = (ROOT / 'engine/include/XEngine.h').read_text(encoding='utf-8')
        renderer = (ROOT / 'engine/src/RenderManager.cpp').read_text(encoding='utf-8')
        self.assertIn('#include "UIRectTransform.h"', umbrella)
        self.assertIn('Rect Resolve(const UIRectTransform& layout', header)
        self.assertIn('window->GetDrawableSize()', source)
        self.assertIn('return Button(id, Resolve(layout, parent), text);', source)
        self.assertIn('return InvisibleButton(id, Resolve(layout, parent));', source)
        self.assertIn('return Checkbox(id, Resolve(layout, parent), value, label);', source)
        self.assertIn('Image(tex, Resolve(layout, parent), srcPixels, tint, flipX, flipY);', source)
        self.assertIn('ProgressBar(value01, Resolve(layout, parent), back, fill, outline);', source)
        self.assertIn('ScopedCanvasTextScale scale(mTextScale, CanvasScale());', source)
        self.assertIn('cmd.textScale = mTextScale;', source)
        self.assertIn('static_cast<float>(tw) * c.textScale', source)
        self.assertIn('c.color, c.textScale);', source)
        self.assertIn('c.textScale = scale;', renderer)
        self.assertIn('scaledExtent(e.width)', renderer)


if __name__ == '__main__':
    unittest.main()
