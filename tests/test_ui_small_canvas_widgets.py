"""Small windows must not silently hide scaled slider/toggle/dropdown controls."""

from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class SmallCanvasWidgetTests(unittest.TestCase):
    def test_scaled_controls_keep_their_minimums_in_reference_pixels(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if not compiler:
            self.skipTest('A C++20 compiler is unavailable')
        source = r'''
#include "XEngine/detail/UIWidgetSize.h"
#include <cassert>
using xengine::detail::ui::ControlHasRoom;
int main() {
    // At reference scale, old guards must still reject undersized widgets.
    assert(!ControlHasRoom(23.5f, 10.5f, 1.f, 24.f, 12.f));
    assert(!ControlHasRoom(23.5f, 10.5f, 1.f, 20.f, 12.f));
    assert(ControlHasRoom(418.f, 44.f, 1.f, 24.f, 12.f));
    assert(ControlHasRoom(94.f, 42.f, 1.f, 20.f, 12.f));

    // On tiny canvases the very same anchored widgets scale below 12px.
    // They are still renderable, just as the volume slider already was.
    assert(ControlHasRoom(418.f * 0.25f, 44.f * 0.25f, 0.25f, 24.f, 12.f));
    assert(ControlHasRoom(94.f * 0.25f, 42.f * 0.25f, 0.25f, 20.f, 12.f));
    assert(ControlHasRoom(359.f * 0.25f, 42.f * 0.25f, 0.25f, 14.f, 8.f));

    // The game's smallest common 16:9 option is 480x270 (scale 0.375).
    assert(ControlHasRoom(418.f * 0.375f, 44.f * 0.375f, 0.375f, 24.f, 12.f));
    assert(ControlHasRoom(94.f * 0.375f, 42.f * 0.375f, 0.375f, 20.f, 12.f));
    assert(!ControlHasRoom(1.f, 1.f, 0.1f, 24.f, 12.f));
}
'''
        with tempfile.TemporaryDirectory(prefix='xengine-widget-scale-') as folder:
            root = Path(folder)
            src = root / 'widgets.cpp'
            output = root / 'widgets'
            src.write_text(source, encoding='utf-8')
            result = subprocess.run(
                [compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                 '-I', str(ROOT / 'engine' / 'include'), str(src), '-o', str(output)],
                text=True, capture_output=True, encoding='utf-8', errors='replace')
            self.assertEqual(result.returncode, 0, result.stderr)
            run = subprocess.run([str(output)], text=True, capture_output=True)
            self.assertEqual(run.returncode, 0, run.stderr)

    def test_widget_draw_paths_use_canvas_aware_guards(self):
        source = (ROOT / 'engine' / 'src' / 'UIManager.cpp').read_text(encoding='utf-8')
        self.assertIn('#include "XEngine/detail/UIWidgetSize.h"', source)
        for name, minimum in [('Slider', '14.f, 8.f'),
                              ('Toggle', '20.f, 12.f'),
                              ('Dropdown', '24.f, 12.f')]:
            with self.subTest(widget=name):
                section = source.split('bool UIManager::' + name + '(const char* id, const Rect& r', 1)[1]
                section = section.split('return false;', 1)[0]
                self.assertIn('ControlHasRoom(r.w, r.h, mTextScale, ' + minimum + ')', section)


if __name__ == '__main__':
    unittest.main()
