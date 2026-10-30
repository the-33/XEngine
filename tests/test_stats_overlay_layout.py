"""Tests the SDL-independent F3 overlay layout without requiring SDL development headers."""
from __future__ import annotations

import pathlib
import shutil
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]

class StatsOverlayLayoutTests(unittest.TestCase):
    def test_pagination_and_bounds(self):
        compiler = shutil.which('g++') or shutil.which('clang++')
        if compiler is None:
            self.skipTest('C++ compiler not available')
        snippet = r'''
#include "StatsOverlayLayout.h"
#include <cassert>
#include <cstddef>
#include <set>
#include <vector>
int main() {
    std::vector<std::vector<std::size_t>> blocks;
    std::size_t k = 0;
    for (auto count : {9, 20, 8, 5, 6, 6, 15}) {
        std::vector<std::size_t> block;
        for (int i = 0; i < count; ++i) block.push_back(k++);
        blocks.push_back(std::move(block));
    }
    for (auto wh : {std::pair{1536.f, 864.f}, std::pair{800.f, 600.f},
                    std::pair{320.f, 240.f}}) {
        auto layout = XEngineStatsOverlay::Pack(blocks, wh.first, wh.second, 650.f, 27.f);
        assert(!layout.pages.empty());
        std::set<std::size_t> indices;
        for (const auto& page : layout.pages) {
            assert(page.usedColumns <= layout.columnsPerPage);
            for (const auto& line : page.lines) {
                assert(line.x >= 0.f);
                assert(line.x + layout.columnWidth <= wh.first + 0.01f);
                assert(line.y >= 0.f && line.y + 27.f <= wh.second + 0.01f);
                assert(indices.insert(line.line).second);
            }
        }
        assert(indices.size() == k);
    }
    auto shortLayout = XEngineStatsOverlay::Pack({}, 800.f, 600.f, 500.f, 25.f);
    assert(shortLayout.pages.size() == 1);
}
'''
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            source = root / 'overlay.cpp'
            executable = root / 'overlay'
            source.write_text(snippet, encoding='utf-8')
            build = subprocess.run([compiler, '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-I', str(ROOT / 'engine' / 'include'), str(source), '-o', str(executable)],
                    capture_output=True, text=True)
            self.assertEqual(build.returncode, 0, build.stderr)
            run = subprocess.run([str(executable)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stderr)

    def test_overlay_uses_measured_fonts_and_pagination(self):
        engine = (ROOT / 'engine' / 'src' / 'Engine.cpp').read_text(encoding='utf-8')
        self.assertIn('TTF_SizeUTF8(font, text.c_str()', engine)
        self.assertIn('Key::F4', engine)
        self.assertIn('XEngineStatsOverlay::Pack(', engine)
        self.assertNotIn('biggestLineSizeInLastRow', engine)
        self.assertNotIn('blockSizes.push_back', engine)

    def test_overlay_shows_effective_runtime_configuration(self):
        engine = (ROOT / 'engine' / 'src' / 'Engine.cpp').read_text(encoding='utf-8')
        self.assertIn('=== Active Configuration ===', engine)
        self.assertIn('"Multithreading: %s (%u workers; configured %s)"', engine)
        self.assertIn('"Parallel work (last frame): render %s, collision %u passes"', engine)
        self.assertIn('SDL_GetRendererInfo(sdlRenderer, &rendererInfo)', engine)
        self.assertIn('"Render backend: %s (requested %s)"', engine)
        self.assertIn('"Parallel preparation: %s (last path %s)"', engine)
        self.assertIn('"Min commands: %zu"', engine)
        self.assertIn('"Broadphase: %s (last frame %s)"', engine)
        self.assertIn('"Cell size: auto (effective %.3f; base %.3f)"', engine)
        self.assertIn('"Cell size: unused (configured %.3f)"', engine)
        self.assertIn('collision->mFrameLastGridCellSize > 0.f', engine)

if __name__ == '__main__':
    unittest.main()
